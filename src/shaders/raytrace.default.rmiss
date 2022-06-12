#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_scalar_block_layout : require

#include "../shared/binding.h"
#include "utils/structs.glsl"
#include "../shared/sun_and_sky.h"
#include "../shared/camera.h"
#include "../shared/pushconstant.h"
#include "utils/math.glsl"
#include "utils/sun_and_sky.glsl"
#include "utils/sample_light.glsl"

// clang-format off
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(set = RtEnv, binding = EnvSunsky, scalar) uniform _SunAndSky { GpuSunAndSky sunAndSky; };
layout(set = RtEnv, binding = EnvAccelMap)       uniform sampler2D  envmapSamplers[3];
layout(set = RtScene, binding = SceneCamera)     uniform _Camera    { GpuCamera cameraInfo; };
layout(push_constant)                            uniform _RtxState  { GpuPushConstantRaytrace pc; };
// clang-format on

void main() {
  // Stop ray if it does not hit anything
  payload.pRec.stop = true;

  // Evaluate environment light and only do mis when depth > 1.
  vec3 env = vec3(0), d = payload.pRec.ray.d;
  float envPdf = 0.0;
  if (sunAndSky.in_use == 1)
    env = sun_and_sky(sunAndSky, d);
  else if (pc.hasEnvMap == 1)
    env = evalEnvmap(envmapSamplers, cameraInfo.envTransform,
                     pc.envMapIntensity, d);
  else
    env = pc.bgColor;

  if (payload.pRec.depth != 1 && isNonSpecular(payload.bRec.flags)) {
    if (sunAndSky.in_use == 1)
      envPdf = uniformSpherePdf();
    else if (pc.hasEnvMap == 1)
      envPdf = pdfEnvmap(envmapSamplers, cameraInfo.envTransform,
                         pc.envMapResolution, d);
    else
      envPdf = uniformSpherePdf();
  }

  // Multiple importance sampling
  float misWeight = 1.0;
  if (isNonSpecular(payload.bRec.flags) && payload.pRec.depth != 1)
    misWeight = powerHeuristic(payload.bRec.pdf, envPdf);

  payload.pRec.radiance += payload.pRec.throughput * env * misWeight;
  DEBUG_INF_NAN(env, "miss\n");
  DEBUG_INF_NAN1(misWeight, "misWeight\n");
}