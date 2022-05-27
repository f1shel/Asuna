#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

void main()
{
  // Get hit state
  HitState state = getHitState();

  // Hit Light
  if(state.lightId >= 0)
  {
    hitLight(state.lightId, state.hitPos);
    return;
  }

  // Fetch textures
  if(state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = texture(textureSamplers[nonuniformEXT(state.mat.diffuseTextureId)], state.uv).rgb;
  // Build local frame
  mat3 local2global = mat3(state.tangent, state.bitangent, state.ffnormal);
  mat3 global2local = transpose(local2global);

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
      allowDoubleSide = (light.doubleSide == 1);
    }

    if(dot(lightSample.direction, state.ffnormal) > 0.0 && (dot(lightSample.normal, lightSample.direction) < 0 || allowDoubleSide))
    {
      BsdfSample bsdfSample;

      vec3 bsdfSampleVal = state.mat.diffuse * INV_PI;
      bsdfSample.pdf     = cosineHemispherePdf(dot(lightSample.direction, state.ffnormal));
      float misWeight    = powerHeuristic(lightSample.pdf, bsdfSample.pdf);

      Li += misWeight * bsdfSampleVal * dot(lightSample.direction, state.ffnormal) * lightSample.emittance
            / (lightSample.pdf + EPS);

      payload.lightVisible  = 1;
      payload.lightDir      = lightSample.direction;
      payload.lightDist     = lightSample.dist;
      payload.lightRadiance = Li * payload.throughput;
    }

    payload.shouldDirectLight = 1;
    payload.lightHitPos = offsetPositionAlongNormal(state.hitPos, state.ffnormal);
  }

  // Sample next ray
  BsdfSample bsdfSample;
  bsdfSample.direction = normalize(local2global * cosineSampleHemisphere(vec2(rand(payload.seed), rand(payload.seed))));
  bsdfSample.pdf       = cosineHemispherePdf(dot(bsdfSample.direction, state.ffnormal));
  vec3 bsdfSampleVal   = state.mat.diffuse * INV_PI;

  if(bsdfSample.pdf < 0.0)
  {
    payload.stop = 1;
    return;
  }
  // Next ray
  payload.bsdfPdf       = bsdfSample.pdf;
  payload.ray.direction = bsdfSample.direction;
  payload.throughput *= bsdfSampleVal * abs(dot(state.ffnormal, bsdfSample.direction)) / (bsdfSample.pdf + EPS);
  payload.ray.origin = payload.lightHitPos;
}