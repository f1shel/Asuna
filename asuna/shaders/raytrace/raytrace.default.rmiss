#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_debug_printf : require

#include "common/structs.glsl"
#include "common/math.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
        // Stop path tracing loop from rgen shader
    payload.stop = true;
    {
        float misWeight = 1.0f;
        vec3 lightDir = gl_WorldRayDirectionEXT;
        vec3 emitterVal = vec3(1.0);
        if (payload.recur > 0)
        {
            float lightPdf = 0.25*INV_PI;
            misWeight = powerHeuristic(payload.bsdf.pdf, lightPdf);
        }
        payload.radiance += misWeight * emitterVal * payload.throughput;
    }
}