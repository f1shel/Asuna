#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

float dielectricFresnel(float cosThetaI, float eta) {
  float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

  // Total internal reflection
  if (sinThetaTSq > 1.0) return 1.0;

  float cosThetaT = sqrt(max(1.0 - sinThetaTSq, 0.0));

  float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
  float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

  return 0.5f * (rs * rs + rp * rp);
}

vec3 dielectricEval(vec3 L, vec3 V, vec3 N, float eta) {
  vec3 f = vec3(0);
  if (dot(L, N) > 0) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) f += vec3(1);
  } else {
    if (abs(dot(refract(-V, N, eta), L) - 1) < EPS) f += vec3(1) * eta * eta;
  }
  return f;
}

// Do not mis with direct light
float dielectricPdf(vec3 L, vec3 V, vec3 N, float eta) {
  float pdf = 0.0;
  float F = dielectricFresnel(abs(dot(V, N)), eta);
  if (dot(L, N) > 0) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) pdf = F;
  } else {
    if (abs(dot(refract(-V, N, eta), L) - 1) < EPS) pdf = 1 - F;
  }
  return pdf;
}

void dielectricSample(vec3 V, vec3 N, float eta, inout BsdfSample bsdfSample) {
  float F = dielectricFresnel(abs(dot(V, N)), eta);
  bsdfSample.shouldMis = false;
  if (rand(payload.seed) < F) {
    bsdfSample.direction = makeNormal(reflect(-V, N));
    bsdfSample.pdf = F;
    bsdfSample.val = vec3(1);
  } else {
    bsdfSample.direction = makeNormal(refract(-V, N, eta));
    bsdfSample.pdf = 1 - F;
    bsdfSample.val = vec3(1) * eta * eta;
  }
}

void main() {
  // Get hit record
  HitState state = getHitState();

  // Hit Light
  if (state.lightId >= 0) {
    hitLight(state.lightId, state.hitPos);
    return;
  }

  // Fetch textures
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.shadingNormal =
        toWorld(state.tangent, state.bitangent, state.shadingNormal, n);
    state.shadingNormal = makeNormal(state.shadingNormal);

    configureShadingFrame(state);
  }

  float eta = dot(state.viewDir, state.shadingNormal) > 0.0
                  ? (1.0 / state.mat.ior)
                  : state.mat.ior;

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
      // Multi importance sampling
      float bsdfPdf = lightSample.shouldMis ? dielectricPdf(lightSample.direction, state.viewDir,
                                    state.ffnormal, eta) : 0.0;
      vec3 bsdfVal = dielectricEval(lightSample.direction, state.viewDir,
                                    state.ffnormal, eta);
      float misWeight = powerHeuristic(lightSample.pdf, bsdfPdf);

      Li += misWeight * bsdfVal * dot(lightSample.direction, state.ffnormal) *
            lightSample.emittance / (lightSample.pdf + EPS);

      payload.directContribution = Li * payload.throughput;
    }
  }

  // Sample next ray
  BsdfSample bsdfSample;

  // World space
  dielectricSample(state.viewDir, state.ffnormal, eta, bsdfSample);

  // Reject invalid sample
  if (bsdfSample.pdf <= 0.0 || length(bsdfSample.val) == 0.0) {
    payload.stop = true;
    return;
  }

  // Next ray
  payload.bsdfPdf = bsdfSample.pdf;
  payload.ray.direction = bsdfSample.direction;
  payload.bsdfShouldMis = false;
  payload.throughput *= bsdfSample.val *
                        abs(dot(state.ffnormal, bsdfSample.direction)) /
                        (bsdfSample.pdf + EPS);
  payload.ray.origin = offsetPositionAlongNormal(
      state.hitPos, sign(dot(bsdfSample.direction, state.ffnormal)) * state.ffnormal);
}