#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require

#include "common/math.glsl"
#include "common/structs.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    // Stop path tracing loop from rgen shader
    payload.stop    = true;
    //vec3 emitterVal = vec3(1.0);
    //payload.radiance += emitterVal * payload.throughput;
}