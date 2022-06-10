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

float fresnelSchlick(float cosThetaI, float f0) {
  return f0 + (1 - f0) * pow(clamp(1 - cosThetaI, 0, 1), 5);
}

float dAnisoGGX(float HdotN, float HdotX, float HdotY, float ax, float ay) {
  return 1 /
         // ---------------------------------------------------------
         (PI * ax * ay * sqr(sqr(HdotX / ax) + sqr(HdotY / ay) + sqr(HdotN)) +
          EPS);
}

vec3 importanceSampleAnisoGGX(vec3 lwo, float ax, float ay) {
  float u1 = rand(payload.seed);
  float u2 = rand(payload.seed);
  float factor = safeSqrt(u1 / max(1 - u1, EPS));
  float phi = TWO_PI * u2;

  vec3 lwh = vec3(0, 0, 1);
  lwh.x = -ax * factor * cos(phi);
  lwh.y = -ay * factor * sin(phi);
  lwh = makeNormal(lwh);

  float HdotV = dot(lwh, lwo);
  vec3 lwi = reflect(-lwo, lwh);

  return lwi;
}

float importanceAnisoGGXPdf(in vec3 lwh, in vec3 lwo, in float ax,
                            in float ay) {
  float pdf = 0.0, HdotV = dot(lwh, lwo);
  vec3 lwi = reflect(-lwo, lwh);
  if (lwi.z > 0.0) {
    pdf =
        dAnisoGGX(lwh.z, lwh.x, lwh.y, ax, ay) * abs(lwh.z) / (4 * HdotV + EPS);
  }
  return pdf;
}

float g1SmithAnisoGGX(float NdotV, float VdotX, float VdotY, float ax,
                      float ay) {
  if (NdotV <= 0.0) return 0.0;
  vec3 factor = vec3(ax * VdotX, ay * VdotY, NdotV);
  return 1 / (NdotV + length(factor));
}

const float DIFFUSE_LOBE_PROBABILITY = 0.5;

vec3 kangEval(vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 rhoDiff,
              vec3 rhoSpec, float ax, float ay, float metalness, float eta) {
  vec3 f = vec3(0);

  float NdotL = dot(N, L);
  float NdotV = dot(N, V);
  if (NdotL <= 0.0 || NdotV <= 0.0) return vec3(0.0);

  vec3 H = makeNormal(L + V);

  float HdotN = dot(H, N);
  float HdotX = dot(H, X);
  float HdotY = dot(H, Y);
  float HdotL = dot(H, L);
  float HdotV = dot(H, V);

  float F0 = sqr((eta - 1) / (eta + 1));
  float Fs = fresnelSchlick(HdotV, F0);
  float Ds = dAnisoGGX(HdotN, HdotX, HdotY, ax, ay);
  float Gs = g1SmithAnisoGGX(NdotV, dot(V, X), dot(V, Y), ax, ay);
  Gs *= g1SmithAnisoGGX(NdotL, dot(L, X), dot(L, Y), ax, ay);
  vec3 diffuse = rhoDiff * INV_PI;
  vec3 specular = rhoSpec * Fs * Ds * Gs;

  f += diffuse;

  f += specular;

  return f;
}

float kangPdf(vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y, float ax, float ay) {
  float pdf = 0.0;
  float NdotL = dot(N, L);
  float NdotV = dot(N, V);
  if (NdotL <= 0.0 || NdotV <= 0.0) return pdf;

  vec3 H = makeNormal(L + V);
  vec3 lwh = makeNormal(toLocal(X, Y, N, H));
  vec3 lwo = makeNormal(toLocal(X, Y, N, V));
  vec3 lwi = makeNormal(toLocal(X, Y, N, L));

  pdf += DIFFUSE_LOBE_PROBABILITY * cosineHemispherePdf(lwi.z);
  pdf +=
      (1 - DIFFUSE_LOBE_PROBABILITY) * importanceAnisoGGXPdf(lwh, lwo, ax, ay);

  return pdf;
}

void kangSample(vec3 lwo, float ax, float ay, inout BsdfSample bsdfSample) {
  vec3 lwi;
  float r = rand(payload.seed);
  bsdfSample.pdf = 0.0;
  bsdfSample.shouldMis = true;
  if (r < DIFFUSE_LOBE_PROBABILITY) {
    lwi = cosineSampleHemisphere(rand2(payload.seed));
    bsdfSample.pdf = cosineHemispherePdf(lwi.z);
  } else {
    lwi = importanceSampleAnisoGGX(lwo, ax, ay);
    vec3 lwh = makeNormal(lwi + lwo);
    bsdfSample.pdf = importanceAnisoGGXPdf(lwh, lwo, ax, ay);
  }
  bsdfSample.direction = lwi;
}

void main() {
  // Get hit state
  HitState state = getHitState();

  // Fetch textures
  if (state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = textureEval(state.mat.diffuseTextureId, state.uv).rgb;
  if (state.mat.metalnessTextureId >= 0)
    state.mat.rhoSpec = textureEval(state.mat.metalnessTextureId, state.uv).rgb;
  if (state.mat.roughnessTextureId >= 0)
    state.mat.anisoAlpha =
        textureEval(state.mat.roughnessTextureId, state.uv).rg;
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.shadingNormal = makeNormal((n * gl_WorldToObjectEXT).xyz);

    configureShadingFrame(state);
  }
  if (state.mat.tangentTextureId >= 0) {
    vec3 c = textureEval(state.mat.tangentTextureId, state.uv).rgb;
    vec3 t = 2 * c - 1;
    state.tangent = makeNormal((t * gl_WorldToObjectEXT).xyz);
  }

  // Rebuild tangent and bitangent
  state.bitangent = makeNormal(cross(state.shadingNormal, state.tangent));
  state.tangent = makeNormal(cross(state.bitangent, state.shadingNormal));
  state.ffnormal = dot(state.shadingNormal, state.viewDir) > 0
                       ? state.shadingNormal
                       : -state.shadingNormal;

  float ax = max(state.mat.anisoAlpha.x, EPS);
  float ay = max(state.mat.anisoAlpha.y, EPS);
  // eta = ior since there is no refraction
  float eta = state.mat.ior;

  // Configure information for denoiser
  if (payload.depth == 1) {
    payload.denoiserAlbedo = state.mat.diffuse;
    payload.denoiserNormal = state.ffnormal;
  }

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
      float bsdfPdf =
          lightSample.shouldMis
              ? kangPdf(lightSample.direction, state.viewDir, state.ffnormal,
                        state.tangent, state.bitangent, ax, ay)
              : 0.0;
      vec3 bsdfVal =
          kangEval(lightSample.direction, state.viewDir, state.ffnormal,
                   state.tangent, state.bitangent, state.mat.diffuse,
                   state.mat.rhoSpec, ax, ay, state.mat.metalness, eta);

      float misWeight = powerHeuristic(lightSample.pdf, bsdfPdf);

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
  kangSample(lwo, ax, ay, bsdfSample);
  bsdfSample.val =
      kangEval(bsdfSample.direction, state.viewDir, state.ffnormal,
               state.tangent, state.bitangent, state.mat.diffuse,
               state.mat.rhoSpec, ax, ay, state.mat.metalness, eta);

  // World space
  bsdfSample.direction = toWorld(state.tangent, state.bitangent, state.ffnormal,
                                 bsdfSample.direction);
  bsdfSample.direction = makeNormal(bsdfSample.direction);

  // Reject invalid sample
  if (bsdfSample.pdf <= 0.0 || length(bsdfSample.val) == 0.0) {
    payload.stop = true;
    return;
  }

  // Next ray
  payload.bsdfPdf = bsdfSample.pdf;
  payload.bsdfShouldMis = bsdfSample.shouldMis;
  payload.ray.direction = bsdfSample.direction;
  payload.throughput *= bsdfSample.val *
                        abs(dot(state.ffnormal, bsdfSample.direction)) /
                        (bsdfSample.pdf + EPS);
  payload.ray.origin = payload.directHitPos;
}