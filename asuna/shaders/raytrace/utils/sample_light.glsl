#ifndef SAMPLE_LIGHT_GLSL
#define SAMPLE_LIGHT_GLSL

#include "../../hostdevice/emitter.h"
#include "math.glsl"
#include "structs.glsl"

void SampleSphereLight(inout uint seed, in GPUEmitter light, in vec3 scatterPos,
                       inout LightSample lightSample)
{
    vec3  sphereCentertoSurface = scatterPos - light.position;
    float distToSphereCenter    = length(sphereCentertoSurface);
    vec3  sampledDir;
    float sampledPdf;

    // TODO: Fix this. Currently assumes the light will be hit only from the outside
    sphereCentertoSurface /= distToSphereCenter;
    sampledDir = uniformSampleSphere(seed, sampledPdf);
    vec3 T, B;
    basis(sphereCentertoSurface, T, B);
    sampledDir = T * sampledDir.x + B * sampledDir.y + sphereCentertoSurface * sampledDir.z;

    vec3 lightSurfacePos = light.position + sampledDir * light.radius;

    lightSample.direction = lightSurfacePos - scatterPos;
    lightSample.dist      = length(lightSample.direction);
    float distSq          = lightSample.dist * lightSample.dist;

    lightSample.direction /= lightSample.dist;
    lightSample.normal    = normalize(lightSurfacePos - light.position);
    lightSample.emittance = light.emittance;
    lightSample.pdf =
        distSq / (light.area * 0.5 * abs(dot(lightSample.normal, lightSample.direction)));
    lightSample.shouldMIS = 1.0;
}

void SampleRectLight(inout uint seed, in GPUEmitter light, in vec3 scatterPos,
                     inout LightSample lightSample)
{
    float r1 = rand(seed);
    float r2 = rand(seed);

    vec3 lightSurfacePos  = light.position + light.u * r1 + light.v * r2;
    lightSample.direction = lightSurfacePos - scatterPos;
    lightSample.dist      = length(lightSample.direction);
    float distSq          = lightSample.dist * lightSample.dist;
    lightSample.direction /= lightSample.dist;
    lightSample.normal    = normalize(cross(light.u, light.v));
    lightSample.emittance = light.emittance;
    lightSample.pdf =
        distSq / (light.area * abs(dot(lightSample.normal, lightSample.direction)));
    lightSample.shouldMIS = 1.0;
}

void SampleDistantLight(inout uint seed, in GPUEmitter light, in vec3 scatterPos,
                        inout LightSample lightSample)
{
    lightSample.normal    = vec3(0.0);
    lightSample.direction = normalize(light.direction);
    lightSample.emittance = light.emittance;
    lightSample.dist      = INFINITY;
    lightSample.pdf       = 1.0;
    lightSample.shouldMIS = 0.0;
}

void SampleOneLight(inout uint seed, in GPUEmitter light, in vec3 scatterPos,
                    inout LightSample lightSample)
{
    int type = int(light.type);
    if (type == eRectLight)
        SampleRectLight(seed, light, scatterPos, lightSample);
    else if (type == eSphereLight)
        SampleSphereLight(seed, light, scatterPos, lightSample);
    else if (type == eDirectionalLight)
        SampleDistantLight(seed, light, scatterPos, lightSample);
}

#endif