#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

float sqr(float x) { return x * x; }

vec3 fresnelSchlick(float cosThetaI, vec3 f0) {
  return f0 + (1 - f0) * pow(clamp(1 - cosThetaI, 0, 1), 5);
}

float dAnisoGGX(float nDotH, float hDotX, float hDotY, float ax, float ay) {
  return 1 /
         max(PI * ax * ay * sqr(sqr(hDotX / ax) + sqr(hDotY / ay) + sqr(nDotH)),
             EPS);
}

float g1SmithAnisoGGX(float nDotV, float vDotX, float vDotY, float ax,
                      float ay) {
  return 1 / (nDotV + safeSqrt(sqr(nDotV) + sqr(ax * vDotX) + sqr(ay * vDotY)));
}

vec3 sampleAnisoGGX(vec3 lwo, float ax, float ay) {
  vec2 u = rand2(payload.seed);
  float factor = safeSqrt(u.x / (1 - u.x));
  float xh = ax * factor * cos(TWO_PI * u.y);
  float yh = ay * factor * sin(TWO_PI * u.y);
  vec3 lwh = makeNormal(vec3(-xh, -yh, 1));
  vec3 lwi = reflect(-lwo, lwh);
  return lwi;
}

float anisoGGXPdf(vec3 lwh, vec3 lwo, float ax, float ay) {
  vec3 lwi = reflect(-lwo, lwh);
  float pdf = 0.0;
  if (lwi.z > 0.0) {
    pdf = dAnisoGGX(lwh.z, lwh.x, lwh.y, ax, ay) * lwh.z / (4 * dot(lwh, lwo));
  }
  return pdf;
}

vec3 pbrEval(vec3 lwi, vec3 lwo, vec3 albedo, float ax, float ay,
             float metalness, out float pdf) {
  pdf = 0.0;
  if (lwi.z <= 0.0 || lwo.z <= 0.0) return vec3(0.0);
  vec3 lwh = makeNormal(lwi + lwo);
  pdf = anisoGGXPdf(lwh, lwo, ax, ay);

  float hDotV = dot(lwo, lwh);
  float hDotL = dot(lwi, lwh);
  vec3 Fs = fresnelSchlick(hDotV, mix(vec3(0.04), albedo, metalness));
  float Ds = dAnisoGGX(lwh.z, lwh.x, lwh.y, ax, ay);
  float Gs = g1SmithAnisoGGX(max(lwi.z, 0.0), lwi.x, lwi.y, ax, ay);
  Gs *= g1SmithAnisoGGX(max(lwo.z, 0.0), lwo.x, lwo.y, ax, ay);
  vec3 diffuse = albedo * (1 - metalness) * INV_PI;
  vec3 specular = Fs * Ds * Gs;
  return diffuse + specular;
}

vec3 pbrSample(vec3 lwo, float ax, float ay) {
  return sampleAnisoGGX(lwo, ax, ay);
}

void main() {
  // Get hit state
  HitState state = getHitState();

  // Hit Light
  if (state.lightId > 0) {
    hitLight(state.lightId, state.hitPos);
    return;
  }

  // Fetch textures
  if (state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = textureEval(state.mat.diffuseTextureId, state.uv).rgb;
  if (state.mat.metalnessTextureId >= 0)
    state.mat.metalness = textureEval(state.mat.metalnessTextureId, state.uv).r;
  if (state.mat.roughnessTextureId >= 0)
    state.mat.roughness = textureEval(state.mat.roughnessTextureId, state.uv).r;
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.ffnormal = toWorld(state.tangent, state.bitangent, state.ffnormal, n);
    state.ffnormal = makeNormal(state.ffnormal);

    // Ensure face forward shading normal
    if (dot(state.ffnormal, state.viewDir) < 0)
      state.ffnormal = -state.ffnormal;

    // Rebuild frame
    basis(state.ffnormal, state.tangent, state.bitangent);
  }
  float ax = max(sqr(state.mat.roughness), 0.001);
  float ay = ax;

  // Direct light
  {
    // Light and environment contribution
    vec3 Li = vec3(0);
    bool allowDoubleSide = false;
    LightSample lightSample;

    // 4 cases:
    // (1) has env & light: envSelectPdf = 0.5, analyticSelectPdf = 0.5
    // (2) no env & light: skip direct light
    // (3) has env, no light: envSelectPdf = 1.0, analyticSelectPdf = 0.0
    // (4) no env, has light: envSelectPdf = 0.0, analyticSelectPdf = 1.0
    bool hasEnv = (pc.hasEnvMap == 1 || sunAndSky.in_use == 1);
    bool hasLight = (pc.numLights > 0);
    float envSelectPdf, analyticSelectPdf;
    if (hasEnv && hasLight)
      envSelectPdf = analyticSelectPdf = 0.5f;
    else if (hasEnv)
      envSelectPdf = 1.f, analyticSelectPdf = 0.f;
    else if (hasLight)
      envSelectPdf = 0.f, analyticSelectPdf = 1.f;
    else
      envSelectPdf = analyticSelectPdf = 0.f;

    float envOrAnalyticSelector = rand(payload.seed);
    if (envOrAnalyticSelector < envSelectPdf) {
      // Sample environment light
      sampleEnvironmentLight(lightSample);
      lightSample.normal = -state.ffnormal;
      lightSample.emittance /= envSelectPdf;
    } else if (envOrAnalyticSelector < envSelectPdf + analyticSelectPdf) {
      // Sample analytic light, randomly pick one
      int lightIndex =
          min(1 + int(rand(payload.seed) * pc.numLights), pc.numLights);
      GpuLight light = lights.l[lightIndex];
      sampleOneLight(payload.seed, light, state.hitPos, lightSample);
      lightSample.emittance *= pc.numLights;
      lightSample.emittance /= analyticSelectPdf;
      allowDoubleSide = (light.doubleSide == 1);
    } else {
      // Skip direct light
      payload.shouldDirectLight = false;
    }

    // Configure direct light setting by light sample
    payload.directHitPos =
        offsetPositionAlongNormal(state.hitPos, state.ffnormal);
    payload.directDir = lightSample.direction;
    payload.directDist = lightSample.dist;

    // Light at the back of the surface is not visible
    payload.directVisible = (dot(lightSample.direction, state.ffnormal) > 0.0);
    // Surface on the back of the light is also not illuminated
    payload.directVisible =
        payload.directVisible &&
        (dot(lightSample.normal, lightSample.direction) < 0 || allowDoubleSide);

    if (payload.directVisible) {
      BsdfSample bsdfSample;

      // Multi importance sampling
      float bsdfPdf = 0;
      vec3 lwi = makeNormal(toLocal(state.tangent, state.bitangent,
                                    state.ffnormal, lightSample.direction));
      vec3 lwo = makeNormal(toLocal(state.tangent, state.bitangent,
                                    state.ffnormal, state.viewDir));
      vec3 bsdfVal = pbrEval(lwi, lwo, state.mat.diffuse, ax, ay,
                             state.mat.metalness, bsdfPdf);
      float misWeight =
          bsdfPdf > 0.0 ? powerHeuristic(lightSample.pdf, bsdfPdf) : 1.0;

      Li += misWeight * bsdfVal * dot(lightSample.direction, state.ffnormal) *
            lightSample.emittance / (lightSample.pdf + EPS);

      payload.directContribution = Li * payload.throughput;
    }
  }

  // Sample next ray
  BsdfSample bsdfSample;

  // Shading frame, local tangent space
  vec3 lwo = makeNormal(
      toLocal(state.tangent, state.bitangent, state.ffnormal, state.viewDir));
  bsdfSample.direction = pbrSample(lwo, ax, ay);
  vec3 bsdfVal = pbrEval(bsdfSample.direction, lwo, state.mat.diffuse, ax, ay,
                         state.mat.metalness, bsdfSample.pdf);

  // World space
  bsdfSample.direction = toWorld(state.tangent, state.bitangent, state.ffnormal,
                                 bsdfSample.direction);
  bsdfSample.direction = makeNormal(bsdfSample.direction);

  // Reject invalid sample
  if (bsdfSample.pdf <= 0.0 || length(bsdfVal) == 0.0) {
    payload.stop = true;
    return;
  }

  // Next ray
  payload.bsdfPdf = bsdfSample.pdf;
  payload.ray.direction = bsdfSample.direction;
  payload.throughput *= bsdfVal *
                        abs(dot(state.ffnormal, bsdfSample.direction)) /
                        (bsdfSample.pdf + EPS);
  payload.ray.origin = payload.directHitPos;
}