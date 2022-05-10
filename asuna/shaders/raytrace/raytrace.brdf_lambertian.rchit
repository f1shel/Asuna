#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "common/macro.glsl"

TRACE_BLOCK
{
    vec3 ffnormal = dot(geoNormal, gl_WorldRayDirectionEXT) <= 0.0 ? geoNormal : -geoNormal;
    // Reset absorption when ray is going out of surface
    if (dot(geoNormal, ffnormal) > 0.0)
    {
        payload.absorption = vec3(0.0);
    }
    // Add absoption (transmission / volume)
    payload.throughput *= exp(-payload.absorption * payload.hitDis);
    // Light and environment contribution
    VisibilityContribution contrib;
    contrib.radiance = vec3(0);
    contrib.visible  = false;
    {
        vec3  Li = vec3(0);
        float lightPdf;
        vec3  lightContrib;
        vec3  lightDir;
        float lightDist = 1e32;
        bool  isLight   = false;

        // Emitter
        {
            isLight = true;

            // randomly select one of the lights
            int emitterIndex =
                int(min(rand(payload.seed) * pc.emittersNum, pc.emittersNum - 1));
            GPUEmitter emitter = emitters.e[emitterIndex];

            lightContrib = emitter.emittance;
            lightDir     = normalize(emitter.direction);
            lightPdf     = 1.0;
        }

        // Environment Light
        /*
        {
            lightDir     = uniformSampleSphere(payload.seed, lightPdf);
            lightContrib = vec3(1.0);
        }
        */

        if (dot(lightDir, ffnormal) > 0.0)
        {
            BsdfSample bsdfSample;

            vec3  bsdfSampleVal = material.diffuse * INV_PI;
            float misWeight = isLight ? 1.0 : max(0.0, powerHeuristic(lightPdf, bsdfSample.pdf));

            Li += misWeight * bsdfSampleVal * abs(dot(lightDir, ffnormal)) * lightContrib /
                  lightPdf;

            contrib.visible   = true;
            contrib.lightDir  = lightDir;
            contrib.lightDist = lightDist;
            contrib.radiance  = Li;
        }
    }

    contrib.radiance *= payload.throughput;
    // Sample next ray
    BsdfSample bsdfSample;
    bsdfSample.bsdfDir = uniformSampleSphere(payload.seed, bsdfSample.pdf);
    vec3 bsdfSampleVal = material.diffuse * INV_PI;
    // Set absorption only if the ray is currently inside the object.
    if (dot(ffnormal, bsdfSample.bsdfDir) < 0.0)
    {
        payload.absorption = vec3(1.0);
    }
    if (bsdfSample.pdf > 0.0)
    {
        payload.throughput *=
            bsdfSampleVal * abs(dot(ffnormal, bsdfSample.bsdfDir)) / bsdfSample.pdf;
    }
    else
    {
        payload.stop = true;
        return;
    }
    // Next ray
    payload.ray.direction = bsdfSample.bsdfDir;
    payload.ray.origin    = offsetPositionAlongNormal(
           payload.hit, dot(bsdfSample.bsdfDir, ffnormal) > 0 ? ffnormal : -ffnormal);
    // We are adding the contribution to the radiance only if the ray is not occluded by an
    // object. This is done here to minimize live state across ray-trace calls.
    if (contrib.visible == true)
    {
        // Shoot shadow ray up to the light (1e32 == environement)
        Ray  shadowRay = Ray(payload.ray.origin, contrib.lightDir);
        uint rayFlags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT |
                        gl_RayFlagsCullBackFacingTrianglesEXT;
        float maxDist = contrib.lightDist;
        isShadowed    = true;
        traceRayEXT(topLevelAS,                 // acceleration structure
                    rayFlags,                   // rayFlags
                    0xFF,                       // cullMask
                    0,                          // sbtRecordOffset
                    0,                          // sbtRecordStride
                    1,                          // missIndex
                    shadowRay.origin,           // ray origin
                    0.0,                        // ray min range
                    shadowRay.direction,        // ray direction
                    maxDist,                    // ray max range
                    1                           // payload layout(location = 1)
        );
        if (!isShadowed)
        {
            payload.radiance += contrib.radiance;
        }
    }

    // For Russian-Roulette (minimizing live state)
    float rrPcont =
        min(max(payload.throughput.x, max(payload.throughput.y, payload.throughput.z)) + 0.001,
            0.95);
    // paths with low throughput that won't contribute
    if (rand(payload.seed) >= rrPcont)
    {
        payload.stop = true;
        return;
    }
    payload.throughput /= rrPcont;        // boost the energy of the non-terminated paths
}
END_TRACE