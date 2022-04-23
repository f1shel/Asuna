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
    // Direct light
    {
        vec3 Lin = vec3(0);

        float tMin  = MINIMUM;
        float tMax  = INFINITY;
        uint  flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT |
                     gl_RayFlagsSkipClosestHitShaderEXT;

        BsdfSample bsdfSample;

        // Environment Light
        {
            float lightPdf;
            vec3  lightDir   = uniformSampleSphere(payload.seed, lightPdf);
            vec3  emitterVal = vec3(1.0f);

            isShadowed = true;

            // Shadow ray (payload 1 is Shadow.miss)
            traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 1,
                        offsetPositionAlongNormal(payload.hit, geoNormal), tMin, lightDir, tMax,
                        1);

            if (!isShadowed)
            {
                vec3 bsdfVal = vec3(0.0);

                if (dot(geoNormal, -gl_WorldRayDirectionEXT) > 0.0 &&
                    dot(geoNormal, lightDir) > 0.0)
                {
                    bsdfVal += material.diffuse * INV_PI;
                }

                float bsdfPdf = 0.0;
                if (dot(geoNormal, -gl_WorldRayDirectionEXT) > 0.0 &&
                    dot(geoNormal, lightDir) > 0.0)
                {
                    bsdfPdf = dot(geoNormal, lightDir) * INV_PI;
                }

                float cosTheta  = dot(geoNormal, lightDir);
                float misWeight = powerHeuristic(lightPdf, bsdfPdf);

                if (lightPdf > 0.0 && cosTheta > 0.0)
                    Lin += misWeight * emitterVal * bsdfVal * cosTheta * payload.throughput /
                           (lightPdf + EPS);
            }
        }

        payload.radiance += Lin;
    }
    // Indirect light
    {
        vec3 bsdfThroughput = vec3(1.0);

        BsdfSample bsdfSample;
        bsdfSample.bsdfDir = uniformSampleSphere(payload.seed, bsdfSample.pdf);
        vec3  bsdfVal      = vec3(0.0);
        float cosTheta     = dot(geoNormal, bsdfSample.bsdfDir);
        if (dot(geoNormal, -gl_WorldRayDirectionEXT) > 0.0 && cosTheta > 0.0)
        {
            bsdfVal += material.diffuse * INV_PI;
        }
        if (bsdfSample.pdf <= 0.0 || cosTheta <= 0.0)
        {
            payload.stop = true;
        }
        else
        {
            bsdfThroughput *= bsdfVal * cosTheta / (bsdfSample.pdf + EPS);

            payload.throughput *= bsdfThroughput;

            // Update a new ray path bounce direction
            payload.bsdf          = bsdfSample;
            payload.ray.origin    = offsetPositionAlongNormal(payload.hit, geoNormal);
            payload.ray.direction = bsdfSample.bsdfDir;
        }
    }
}
END_TRACE