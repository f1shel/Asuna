#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_scalar_block_layout : require

#include "../shared/binding.h"
#include "../shared/sun_and_sky.h"
#include "../shared/pushconstant.h"
#include "utils/math.glsl"
#include "utils/structs.glsl"
#include "utils/sun_and_sky.glsl"
#include "utils/sample_light.glsl"

// clang-format off
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(set = RtEnv, binding = EnvSunsky, scalar) uniform _SunAndSky { GpuSunAndSky sunAndSky; };
layout(set = RtEnv, binding = EnvAccelMap)       uniform sampler2D  envmapSamplers[3];
layout(push_constant)                            uniform _RtxState  { GpuPushConstantRaytrace pc; };
// clang-format on

void main()
{
  // Stop path tracing loop from rgen shader
  payload.stop = 1;
  // eval environment light
  vec3  env;
  float pdf;
  if(sunAndSky.in_use == 1)
  {
    env = sun_and_sky(sunAndSky, gl_WorldRayDirectionEXT);
    pdf = uniformSpherePdf();
  }
  else if(pc.hasEnvMap == 1)
  {
    env = evalEnvmap(envmapSamplers, gl_WorldRayDirectionEXT, pc.envMapIntensity);
    pdf = pdfEnvmap(envmapSamplers, gl_WorldRayDirectionEXT, pc.envMapResolution);
  }
  else
  {
    env = pc.bgColor;
    pdf = uniformSpherePdf();
  }
  float misWeight = 1.0;
  if(payload.depth > 0)
    misWeight = powerHeuristic(payload.bsdfPdf, pdf);
  // Done sampling return
  vec3 radiance = misWeight * env * payload.throughput;
  payload.radiance += radiance;
  /*
  DEBUG_INF_NAN(radiance, "error when ray missing\n");
  if (checkInfNan(radiance))
  {
    debugPrintfEXT("mis: %f, env: %v3f, throughput: %v3f\n", misWeight, env, payload.throughput);
  }
  */
}