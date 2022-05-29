#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"
#include "raytrace.brdf_disney.tools.glsl"

void main()
{
  // Get hit state
  HitState state = getHitState();

  // Hit Light
  if(state.lightId > 0)
  {
    hitLight(state.lightId, state.hitPos);
    return;
  }

  if(payload.depth >= pc.maxPathDepth)
  {
    payload.stop = 1;
    return;
  }

  // Build local frame
  mat3 local2global = mat3(state.tangent, state.bitangent, state.ffnormal);
  mat3 global2local = transpose(local2global);

  // Fetch textures
  if(state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = texture(textureSamplers[nonuniformEXT(state.mat.diffuseTextureId)], state.uv).rgb;
  if(state.mat.metalnessTextureId >= 0)
    state.mat.metalness = texture(textureSamplers[nonuniformEXT(state.mat.metalnessTextureId)], state.uv).r;
  if(state.mat.roughnessTextureId >= 0)
    state.mat.roughness = texture(textureSamplers[nonuniformEXT(state.mat.roughnessTextureId)], state.uv).r;
  if(state.mat.normalTextureId >= 0)
  {
    vec3 c         = texture(textureSamplers[nonuniformEXT(state.mat.normalTextureId)], state.uv).rgb;
    vec3 n         = 2 * c - 1;
    state.ffnormal = normalize(local2global * n);
    if(dot(state.ffnormal, state.viewDir) < 0)
      state.ffnormal = -state.ffnormal;
    // Rebuild frame
    basis(state.ffnormal, state.tangent, state.bitangent);
    local2global = mat3(state.tangent, state.bitangent, state.ffnormal);
    global2local = transpose(local2global);
  }

  // Direct light
  {
    // Light and environment contribution
    payload.lightRadiance = vec3(0);
    payload.lightVisible  = 0;
    LightSample lightSample;

    vec3  Li               = vec3(0);
    bool  allowDoubleSide  = false;

    /*
     * 4 cases:
     * (1) has env & light: envSelectPdf = 0.5, lightSelectPdf = 0.5
     * (2) no env & light: skip direct light
     * (3) has env, no light: envSelectPdf = 1.0, lightSelectPdf = 0.0
     * (4) no env, has light: envSelectPdf = 0.0, lightSelectPdf = 1.0
     */
    bool hasEnv = (pc.hasEnvMap == 1 || sunAndSky.in_use == 1);
    bool hasLight = (pc.numLights > 0);
    float envSelectPdf, lightSelectPdf;
    if (hasEnv && hasLight) envSelectPdf = lightSelectPdf = 0.5f;
    else if (hasEnv) envSelectPdf = 1.f, lightSelectPdf = 0.f;
    else if (hasLight) envSelectPdf = 0.f, lightSelectPdf = 1.f;
    else envSelectPdf = lightSelectPdf = 0.f;

    float selectRand = rand(payload.seed);
    if(selectRand < envSelectPdf)
    {
      sampleEnvironmentLight(lightSample);
      lightSample.normal = -state.ffnormal;
      lightSample.emittance /= envSelectPdf;
    }
    else if (selectRand < envSelectPdf + lightSelectPdf)
    {
      // sample analytic light
      int      lightIndex = min(1 + int(rand(payload.seed) * pc.numLights), pc.numLights);
      GpuLight light      = lights.l[lightIndex];
      sampleOneLight(payload.seed, light, state.hitPos, lightSample);
      lightSample.emittance *= pc.numLights;  // selection pdf
      lightSample.emittance /= lightSelectPdf;
//      debugPrintfEXT("light position: %v3f emittance: %v3f\n", light.position, lightSample.emittance);
      allowDoubleSide = (light.doubleSide == 1);
    }

    if (envSelectPdf + lightSelectPdf > 0.f) {

    if(dot(lightSample.direction, state.ffnormal) > 0.0 && (dot(lightSample.normal, lightSample.direction) < 0 || allowDoubleSide))
    {
      BsdfSample bsdfSample;

      vec3 bsdfSampleVal = DisneyEval(state.mat, state.viewDir, state.ffnormal, lightSample.direction, bsdfSample.pdf);

      if (bsdfSample.pdf > 0.0) {

          float misWeight = powerHeuristic(lightSample.pdf, bsdfSample.pdf);

          Li += misWeight * bsdfSampleVal * max(dot(lightSample.direction, state.ffnormal), 0.0) * lightSample.emittance
                / (lightSample.pdf + EPS);

          payload.lightVisible  = 1;
          payload.lightDir      = lightSample.direction;
          payload.lightDist     = lightSample.dist;
          payload.lightRadiance = Li * payload.throughput;
      }
    }
    payload.shouldDirectLight = 1;
    payload.lightHitPos       = offsetPositionAlongNormal(state.hitPos, state.ffnormal);
    }
  }

  // Sample next ray
  BsdfSample bsdfSample;
  vec3       bsdfSampleVal =
      DisneySample(state.mat, payload.seed, state.viewDir, state.ffnormal, bsdfSample.direction, bsdfSample.pdf);
  if (bsdfSample.direction.z <= 0) bsdfSample.pdf = 0.0;
  bsdfSample.direction = normalize(local2global * bsdfSample.direction);
  vec3 throughput      = bsdfSampleVal * abs(dot(state.ffnormal, bsdfSample.direction)) / (bsdfSample.pdf + EPS);

  if(bsdfSample.pdf <= 0.0)
  {
    payload.stop = 1;
    return;
  }
  // Next ray
  payload.bsdfPdf       = bsdfSample.pdf;
  payload.ray.direction = bsdfSample.direction;
  payload.throughput *= throughput;
  payload.ray.origin = payload.lightHitPos;
  /*
  DEBUG_INF_NAN(throughput, "error when updating throughput\n");
  if(checkInfNan(throughput))
  {
    debugPrintfEXT("val: %v3f, pdf: %f\n", bsdfSampleVal, bsdfSample.pdf);
  }
  */
          // Russian roulette
            float q = min(max(payload.throughput.x, max(payload.throughput.y, payload.throughput.z)) + 0.001, 0.95);
            if (rand(payload.seed) > q) {
                payload.stop = 1;
                return;
            }
            payload.throughput /= q;
}