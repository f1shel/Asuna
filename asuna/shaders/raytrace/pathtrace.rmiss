#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_scalar_block_layout : require

#include "../../hostdevice/binding.h"
#include "../../hostdevice/sun_and_sky.h"
#include "utils/math.glsl"
#include "utils/structs.glsl"
#include "utils/sun_and_sky.glsl"

// clang-format off
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(set = RtEnv, binding = EnvSunsky, scalar) uniform _SunAndSky { GpuSunAndSky sunAndSky; };
// clang-format on

void main()
{
  // Stop path tracing loop from rgen shader
  payload.stop = 1;
  // environment light
  vec3 env = vec3(0.f);
  if(sunAndSky.in_use == 1)
    env = sun_and_sky(sunAndSky, gl_WorldRayDirectionEXT);
  // Done sampling return
  payload.radiance += env * payload.throughput;
}