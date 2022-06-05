#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

void main() {
  // Get hit record
  HitState state = getHitState();

  // Hit Light
  if (state.lightId >= 0) {
    hitLight(state.lightId, state.hitPos);
    return;
  }

  // Fetch textures
  if (state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = textureEval(state.mat.diffuseTextureId, state.uv).rgb;
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

  // Configure information for denoiser
  if (payload.depth == 1)
  {
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
      float bsdfPdf =
          cosineHemispherePdf(dot(lightSample.direction, state.ffnormal));
      vec3 bsdfVal = state.mat.diffuse * INV_PI;
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
  bsdfSample.direction = cosineSampleHemisphere(rand2(payload.seed));
  bsdfSample.pdf = cosineHemispherePdf(bsdfSample.direction.z);

  // World space
  bsdfSample.direction = toWorld(state.tangent, state.bitangent, state.ffnormal,
                                 bsdfSample.direction);
  vec3 bsdfVal = state.mat.diffuse * INV_PI;

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