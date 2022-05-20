#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "utils/layouts.glsl"
#include "utils/math.glsl"
#include "utils/sample_light.glsl"
#include "utils/structs.glsl"
#include "utils/sun_and_sky.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

void main()
{
  UNPACK_VERTEX_INFO(id, v0, v1, v2, bary, uv, hitPos, shadingNormal, faceNormal, lightId, material);

  vec3 viewDir = -normalize(gl_WorldRayDirectionEXT);
  vec3 ffnormal;
  if(pc.useFaceNormal == 1)
    ffnormal = dot(faceNormal, viewDir) > 0.0 ? faceNormal : -faceNormal;
  else
    ffnormal = dot(shadingNormal, viewDir) > 0.0 ? shadingNormal : -shadingNormal;

  vec3 tangent, bitangent;
  basis(ffnormal, tangent, bitangent);
  mat3 local2global = mat3(tangent, bitangent, ffnormal);
  mat3 global2local = transpose(local2global);

  // Hit Light
  if(lightId >= 0)
  {
    GpuLight light               = lights.l[lightId];
    vec3     lightDirection      = normalize(hitPos - payload.ray.origin);
    vec3     lightNormal         = normalize(cross(light.u, light.v));
    float    lightSideProjection = dot(lightNormal, lightDirection);

    payload.stop = 1;
    // single side light
    if(lightSideProjection > 0)
      return;
    // do not mis
    if(payload.depth == 0)
    {
      payload.radiance = light.emittance;
      return;
    }
    // do mis
    float lightDist  = length(hitPos - payload.ray.origin);
    float distSquare = lightDist * lightDist;
    float lightPdf   = distSquare / (light.area * abs(lightSideProjection) + EPS);
    float misWeight  = powerHeuristic(payload.bsdf.pdf, lightPdf);
    payload.radiance += payload.throughput * light.emittance * misWeight;
    return;
  }

  VisibilityContribution contrib;
  contrib.radiance = vec3(0);
  contrib.visible  = 0;
  // Direct light
  {
    // Light and environment contribution
    LightSample lightSample;

    vec3 Li              = vec3(0);
    bool allowDoubleSide = false;

    do
    {
      if(pc.numLights == 0)
        break;
      // randomly select one of the lights
      int      lightIndex = int(min(rand(payload.seed) * pc.numLights, pc.numLights - 1));
      GpuLight light      = lights.l[lightIndex];
      sampleOneLight(payload.seed, light, hitPos, lightSample);
      lightSample.emittance *= pc.numLights;  // selection pdf
      allowDoubleSide = (light.doubleSide == 1);
    } while(false);

    if(dot(lightSample.direction, ffnormal) > 0.0 && (dot(lightSample.normal, lightSample.direction) < 0 || allowDoubleSide))
    {
      BsdfSample bsdfSample;

      vec3 bsdfSampleVal = material.diffuse * INV_PI;
      bsdfSample.pdf     = cosineHemispherePdf(dot(lightSample.direction, ffnormal));
      float misWeight    = powerHeuristic(lightSample.pdf, bsdfSample.pdf);

      Li += misWeight * bsdfSampleVal * dot(lightSample.direction, ffnormal) * lightSample.emittance / (lightSample.pdf + EPS);

      contrib.visible   = 1;
      contrib.lightDir  = lightSample.direction;
      contrib.lightDist = lightSample.dist;
      contrib.radiance  = Li * payload.throughput;
    }
  }

  if(payload.depth >= pc.maxPathDepth)
  {
    payload.stop = 1;
    return;
  }

  // Sample next ray
  BsdfSample bsdfSample;
  bsdfSample.direction = normalize(local2global * cosineSampleHemisphere(vec2(rand(payload.seed), rand(payload.seed))));
  bsdfSample.pdf       = cosineHemispherePdf(dot(bsdfSample.direction, ffnormal));
  vec3 bsdfSampleVal   = material.diffuse * INV_PI;

  if(bsdfSample.pdf < 0.0)
  {
    payload.stop = 1;
    return;
  }
  // Next ray
  payload.bsdf          = bsdfSample;
  payload.ray.direction = bsdfSample.direction;
  payload.throughput *= bsdfSampleVal * abs(dot(ffnormal, bsdfSample.direction)) / (bsdfSample.pdf + EPS);
  payload.ray.origin = offsetPositionAlongNormal(hitPos, ffnormal);

  // We are adding the contribution to the radiance only if the ray is not
  // occluded by an object. This is done here to minimize live state across
  // ray-trace calls.
  // Shoot shadow ray up to the light(INFINITY == environement)
  if(contrib.visible == 1)
  {
    Ray   shadowRay = Ray(payload.ray.origin, contrib.lightDir);
    uint  rayFlags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    float maxDist   = contrib.lightDist - EPS;
    isShadowed      = true;
    traceRayEXT(tlas,                 // acceleration structure
                rayFlags,             // rayFlags
                0xFF,                 // cullMask
                0,                    // sbtRecordOffset
                0,                    // sbtRecordStride
                1,                    // missIndex
                shadowRay.origin,     // ray origin
                0.0,                  // ray min range
                shadowRay.direction,  // ray direction
                maxDist,              // ray max range
                1                     // payload layout(location = 1)
    );
    if(!isShadowed)
      payload.radiance += contrib.radiance;
    // debugPrintfEXT("shadow ray: dir = %v3f, distance = %f\n", shadowRay.direction, maxDist);
  }

  // // For Russian-Roulette (minimizing live state)
  // float rrPcont = min(max(payload.throughput.x, max(payload.throughput.y, payload.throughput.z)) + 0.001, 0.95);
  // // paths with low throughput that won't contribute
  // if(rand(payload.seed) >= rrPcont)
  // {
  //   payload.stop = 1;
  //   return;
  // }
  // payload.throughput /= rrPcont;  // boost the energy of the non-terminated paths
}