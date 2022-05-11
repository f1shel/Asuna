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
    GPUInstanceDesc instDesc = instances.i[gl_InstanceCustomIndexEXT];
    Indices         indices  = Indices(instDesc.indexAddress);
    Vertices        vertices = Vertices(instDesc.vertexAddress);
    GPUMaterial     material = Materials(instDesc.materialAddress).m[0];
    const ivec3     id       = indices.i[gl_PrimitiveID];
    const GPUVertex v0       = vertices.v[id.x];
    const GPUVertex v1       = vertices.v[id.y];
    const GPUVertex v2       = vertices.v[id.z];
    const vec3      bary     = vec3(1.0 - bary.x - bary.y, bary.x, bary.y);
    const vec2      uv       = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;
    const vec3      pos      = v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z;
    const vec3 normal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    const vec3 hitPos = gl_ObjectToWorldEXT * vec4(pos, 1.0);
    const vec3 geoNormal = normalize((normal * gl_WorldToObjectEXT).xyz);
    payload.hitPos       = hitPos;

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
        vec3        Li = vec3(0);
        LightSample lightSample;
        // Emitter
        if (pc.lightsNum > 0)
        {
            // randomly select one of the lights
            int lightIndex   = int(min(rand(payload.seed) * pc.lightsNum, pc.lightsNum - 1));
            GPUEmitter light = lights.l[lightIndex];

            SampleOneLight(payload.seed, light, payload.hitPos, lightSample);

            lightSample.emittance *= pc.lightsNum;        // selection pdf
        }

        // Environment Light
        /* {
            lightDir     = uniformSampleSphere(payload.seed, lightPdf);
            lightContrib = vec3(1.0);
        } */

        if (dot(lightSample.direction, ffnormal) > 0.0)
        {
            BsdfSample bsdfSample;

            vec3 bsdfSampleVal = material.diffuse * INV_PI;
            bsdfSample.pdf     = 0.25*INV_PI;
            float misWeight    = lightSample.shouldMIS > 0.0 ?
                                     max(0.0, powerHeuristic(lightSample.pdf, bsdfSample.pdf)) :
                                     1.0;

            Li += misWeight * bsdfSampleVal * abs(dot(lightSample.direction, ffnormal)) *
                  lightSample.emittance / lightSample.pdf;

            contrib.visible   = true;
            contrib.lightDir  = lightSample.direction;
            contrib.lightDist = lightSample.dist;
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
           payload.hitPos, dot(bsdfSample.bsdfDir, ffnormal) > 0 ? ffnormal : -ffnormal);
    // We are adding the contribution to the radiance only if the ray is not occluded by an
    // object. This is done here to minimize live state across ray-trace calls.
    if (contrib.visible == true)
    {
        // Shoot shadow ray up to the light (1e32 == environement)
        Ray  shadowRay = Ray(payload.ray.origin, contrib.lightDir);
        uint rayFlags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT |
                        gl_RayFlagsCullBackFacingTrianglesEXT;
        float maxDist = contrib.lightDist - EPS;
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