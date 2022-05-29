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
    float envOrAnalyticPdf = 0.5;

    if(pc.numLights == 0)
      envOrAnalyticPdf = 1.0;
    if(rand(payload.seed) < envOrAnalyticPdf)
    {
      sampleEnvironmentLight(lightSample);
      lightSample.normal = -state.ffnormal;
      lightSample.emittance /= envOrAnalyticPdf;
    }
    else
    {
      // sample analytic light
      int      lightIndex = min(1 + int(rand(payload.seed) * pc.numLights), pc.numLights);
      GpuLight light      = lights.l[lightIndex];
      sampleOneLight(payload.seed, light, state.hitPos, lightSample);
      lightSample.emittance *= pc.numLights;  // selection pdf
      lightSample.emittance /= 1.0 - envOrAnalyticPdf;
      debugPrintfEXT("light position: %v3f emittance: %v3f\n", light.position, lightSample.emittance);
      allowDoubleSide = (light.doubleSide == 1);
    }

    if(dot(lightSample.direction, state.ffnormal) > 0.0 && (dot(lightSample.normal, lightSample.direction) < 0 || allowDoubleSide))
    {
      BsdfSample bsdfSample;

      vec3 bsdfSampleVal = DisneyEval(state.mat, state.viewDir, state.ffnormal, lightSample.direction, bsdfSample.pdf);

      float misWeight = powerHeuristic(lightSample.pdf, bsdfSample.pdf);

      Li += misWeight * bsdfSampleVal * max(dot(lightSample.direction, state.ffnormal), 0.0) * lightSample.emittance
            / (lightSample.pdf + EPS);

      payload.lightVisible  = 1;
      payload.lightDir      = lightSample.direction;
      payload.lightDist     = lightSample.dist;
      payload.lightRadiance = Li * payload.throughput;
    }

    payload.shouldDirectLight = 1;
    payload.lightHitPos       = offsetPositionAlongNormal(state.hitPos, state.ffnormal);
  }

  if(payload.depth >= pc.maxPathDepth)
  {
    payload.stop = 1;
    return;
  }

  // Sample next ray
  BsdfSample bsdfSample;
  vec3       bsdfSampleVal =
      DisneySample(state.mat, payload.seed, state.viewDir, state.ffnormal, bsdfSample.direction, bsdfSample.pdf);
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
}