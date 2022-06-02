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

void main() {
  // Stop ray if it does not hit anything
  payload.stop = true;

  // Evaluate environment light and only do mis when depth > 1.
  vec3 env;
  float envPdf = 0.0;
  if (sunAndSky.in_use == 1) {
    env = sun_and_sky(sunAndSky, gl_WorldRayDirectionEXT);
    envPdf = cosineHemispherePdf(gl_WorldRayDirectionEXT.z);
  } else if (pc.hasEnvMap == 1) {
    env = evalEnvmap(envmapSamplers, gl_WorldRayDirectionEXT, pc.envRotateAngle,
                     pc.envMapIntensity);
    envPdf = pdfEnvmap(envmapSamplers, gl_WorldRayDirectionEXT,
                       pc.envRotateAngle, pc.envMapResolution);
  } else {
    env = pc.bgColor;
    envPdf = cosineHemispherePdf(gl_WorldRayDirectionEXT.z);
  }

  // Multiple importance sampling
  float misWeight =
      payload.depth > 1 ? powerHeuristic(payload.bsdfPdf, envPdf) : 1.f;

  payload.radiance += payload.throughput * env * misWeight;
}