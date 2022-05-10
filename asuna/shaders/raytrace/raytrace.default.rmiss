#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require
#extension GL_EXT_scalar_block_layout: require

#include "../../hostdevice/binding.h"
#include "../../hostdevice/sun_and_sky.h"
#include "utils/math.glsl"
#include "utils/structs.glsl"
#include "utils/sun_and_sky.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(set = eGPUSetRaytraceGraphics, binding = eGPUBindingGraphicsSunAndSky,
       scalar) uniform _SSBuffer
{
    SunAndSky sunAndSky;
};

void main()
{
    // Stop path tracing loop from rgen shader
    payload.stop = true;
    // environment light
    vec3 env;
    if (sunAndSky.in_use == 1)
        env = sun_and_sky(sunAndSky, gl_WorldRayDirectionEXT);
    // Done sampling return
    payload.radiance += env * payload.throughput;
}