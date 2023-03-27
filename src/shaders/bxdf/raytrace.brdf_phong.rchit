#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 diffuse, vec3 specular, float shininess, uint flag) {
  vec3 weight = vec3(0);

  if ((flag & EArea) == 0) return weight;
  float NdotL = dot(N, L), NdotV = dot(N, V);
  if (NdotL < 0 || NdotV < 0) return weight;

  vec3 H = normalize(L + V);
  vec3 diffuseLobe = diffuse * INV_PI;
  vec3 specularLobe = specular * pow(max(dot(H, N), 0.0), shininess) * (shininess + 2) * INV_2PI;

  float diffuseBrightness = luminance(diffuse);
  float specularBrightness = luminance(specular);
  float diffuseWeight = diffuseBrightness / (diffuseBrightness + specularBrightness);
  float specularWeight = 1 - diffuseWeight;

  weight = (diffuseWeight * diffuseLobe + specularWeight * specularLobe) * NdotL;
  return weight;
}

float pdfPhong(vec3 L, vec3 V, vec3 N, float shininess) {
  vec3 H = normalize(L + V);
  float NdotH = max(dot(H, N), 0);
  float VdotH = max(dot(H, V), 0);
  return (shininess + 1) * INV_2PI * pow(NdotH, shininess) * 0.25 / (VdotH + EPS);
}

float pdf(vec3 L, vec3 V, vec3 N, vec3 diffuse, vec3 specular, float shininess, uint flag) {
  float pdf = 0;
  if ((flag & EArea) == 0) return pdf;

  float diffuseBrightness = luminance(diffuse);
  float specularBrightness = luminance(specular);
  float diffuseWeight = diffuseBrightness / (diffuseBrightness + specularBrightness);
  float specularWeight = 1 - diffuseWeight;

  float pdfDiffuse = cosineHemispherePdf(dot(N, L));
  float pdfSpecular = pdfPhong(L, V, N, shininess);
  pdf = diffuseWeight * pdfDiffuse + specularWeight * pdfSpecular;
  return pdf;
}

vec3 sampleBsdf(vec2 u, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 diffuse, vec3 specular, float shininess,
                out BsdfSamplingRecord bRec) {
  vec3 weight = vec3(0);

  float diffuseBrightness = luminance(diffuse);
  float specularBrightness = luminance(specular);
  float diffuseWeight = diffuseBrightness / (diffuseBrightness + specularBrightness);
  float specularWeight = 1 - diffuseWeight;

  if (u.x < diffuseWeight) {
    u.x /= diffuseWeight;
    vec3 wi = cosineSampleHemisphere(u);
    bRec.pdf = cosineHemispherePdf(wi.z);
    bRec.d = toWorld(X, Y, N, wi);
    bRec.flags = EDiffuseReflection;
  } else {
    u.x = (u.x - diffuseWeight) / specularWeight;
    float cosTheta = pow(u.x, 1 / (shininess + 1));
    float phi = TWO_PI * u.y;
    float sinTheta = safeSqrt(1 - cosTheta * cosTheta);
    vec3 wh = vec3(sinTheta * sin(phi), sinTheta * cos(phi), cosTheta);
    vec3 H = toWorld(X, Y, N, wh);
    vec3 L = reflect(-V, H);
    bRec.pdf = pdfPhong(L, V, N, shininess);
    bRec.d = L;
    bRec.flags = EGlossyReflection;
  }

  weight = eval(bRec.d, V, N, diffuse, specular, shininess, EArea);
  return weight;
}

void hitLight(int lightId, vec3 hitPos) {
  GpuLight light = lights.l[lightId];
  vec3 lightDirection = makeNormal(payload.pRec.ray.d);
  vec3 lightNormal = makeNormal(cross(light.u, light.v));
  float lightSideProjection = dot(lightNormal, lightDirection);

  // Stop ray if it hits a light
  payload.pRec.stop = true;
  // Single side light
  if (lightSideProjection > 0 && light.doubleSide == 0) return;

  float misWeight = 1.0;
#if USE_MIS
  // Do mis
  if (isNonSpecular(payload.bRec.flags) && payload.pRec.depth != 1) {
    // Do mis with area light
    float lightDist = length(hitPos - payload.pRec.ray.o);
    float distSquare = lightDist * lightDist;
    float lightPdf = distSquare / (light.area * abs(lightSideProjection) + EPS);
    misWeight = powerHeuristic(payload.bRec.pdf, lightPdf);
  }
#endif

  payload.pRec.radiance += payload.pRec.throughput * light.radiance * misWeight;
}

void main() {
  // Get hit record
  HitState state = getHitState();

  // Hit Light
  if (state.lightId >= 0) {
    hitLight(state.lightId, state.pos);
    return;
  }

  // Fetch textures
  if (state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = textureEval(state.mat.diffuseTextureId, state.uv).rgb;
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.N = toWorld(state.X, state.Y, state.N, n);
    state.N = makeNormal(state.N);

    configureShadingFrame(state);
  }

  // Configure information for denoiser
  if (payload.pRec.depth == 1) {
    if (pc.diffuseOutChannel >= 0)
      payload.mRec.channel[pc.diffuseOutChannel] = state.mat.diffuse;
    if (pc.normalOutChannel >= 0)
      payload.mRec.channel[pc.normalOutChannel] = state.ffN;
    if (pc.specularOutChannel >= 0)
      payload.mRec.channel[pc.specularOutChannel] = state.mat.rhoSpec;
    if (pc.tangentOutChannel >= 0)
      payload.mRec.channel[pc.tangentOutChannel] = state.X;
    if (pc.roughnessOutChannel >= 0)
      payload.mRec.channel[pc.roughnessOutChannel] = vec3(1, 1, 0);
    if (pc.positionOutChannel >= 0)
      payload.mRec.channel[pc.positionOutChannel] = state.pos;
    if (pc.uvOutChannel >= 0)
      payload.mRec.channel[pc.uvOutChannel] = vec3(state.uv, 1);
  }

#if USE_MIS
  // Direct light
  {
    // Light and environment contribution
    vec3 Ld = vec3(0);
    bool visible;
    LightSamplingRecord lRec;
    vec3 radiance = sampleLights(state.pos, state.ffN, visible, lRec);

    if (visible) {
      vec3 bsdfWeight =
          eval(lRec.d, state.V, state.ffN, state.mat.diffuse, state.mat.rhoSpec, state.mat.specular, EArea);

      // Multi importance sampling
      float bsdfPdf = pdf(lRec.d, state.V, state.ffN, state.mat.diffuse, state.mat.rhoSpec, state.mat.specular, lRec.flags);
      float misWeight = powerHeuristic(lRec.pdf, bsdfPdf);

      Ld += misWeight * bsdfWeight * radiance * payload.pRec.throughput /
            (lRec.pdf + EPS);
    }

    payload.dRec.radiance = Ld;
    payload.dRec.skip = (!visible);
  }
#endif

  // Sample next ray
  BsdfSamplingRecord bRec;
  vec3 bsdfWeight = sampleBsdf(rand2(payload.pRec.seed), state.V, state.ffN, state.X,
                               state.Y, state.mat.diffuse, state.mat.rhoSpec, state.mat.specular, bRec);

  // Reject invalid sample
  if (bRec.pdf <= 0.0 || isBlack(bsdfWeight)) {
    payload.pRec.stop = true;
    return;
  }

  // Next ray
  payload.bRec = bRec;
  payload.pRec.ray =
      Ray(offsetPositionAlongNormal(state.pos, state.ffN), payload.bRec.d);
  payload.pRec.throughput *= bsdfWeight / bRec.pdf;
}