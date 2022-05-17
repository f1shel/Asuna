#ifndef SAMPLE_LIGHT_GLSL
#define SAMPLE_LIGHT_GLSL

#include "../../../hostdevice/light.h"
#include "math.glsl"
#include "structs.glsl"

void sampleRectLight(inout uint seed, in GpuLight light, in vec3 scatterPos, inout LightSample lightSample)
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
  lightSample.pdf       = distSq / (light.area * abs(dot(lightSample.normal, lightSample.direction)));
  lightSample.shouldMis = 1.0;
}

void sampleDistantLight(inout uint seed, in GpuLight light, in vec3 scatterPos, inout LightSample lightSample)
{
  lightSample.normal    = vec3(0.0);
  lightSample.direction = normalize(light.direction);
  lightSample.emittance = light.emittance;
  lightSample.dist      = INFINITY;
  lightSample.pdf       = 1.0;
  lightSample.shouldMis = 0.0;
}

void sampleOneLight(inout uint seed, in GpuLight light, in vec3 scatterPos, inout LightSample lightSample)
{
  int type = int(light.type);
  if(type == LightTypeRect)
    sampleRectLight(seed, light, scatterPos, lightSample);
  else if(type == LightTypeDirectional)
    sampleDistantLight(seed, light, scatterPos, lightSample);
}

#endif