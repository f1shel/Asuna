#ifndef SAMPLE_LIGHT_GLSL
#define SAMPLE_LIGHT_GLSL

#include "../../shared/light.h"
#include "math.glsl"
#include "structs.glsl"

void sampleTriangleLight(inout uint seed, in GpuLight light, in vec3 scatterPos,
                     inout LightSample lightSample) {
  float r1 = rand(seed);
  float r2 = (1 - r1) * rand(seed);

  vec3 lightSurfacePos = light.position + light.u * r1 + light.v * r2;
  lightSample.direction = lightSurfacePos - scatterPos;
  lightSample.dist = length(lightSample.direction);
  float distSq = lightSample.dist * lightSample.dist;
  lightSample.direction /= lightSample.dist;
  lightSample.normal = normalize(cross(light.u, light.v));
  lightSample.emittance = light.emittance;
  lightSample.pdf =
      distSq /
      (light.area * abs(dot(lightSample.normal, lightSample.direction)));
  lightSample.shouldMis = 1.0;
}

void sampleRectLight(inout uint seed, in GpuLight light, in vec3 scatterPos,
                     inout LightSample lightSample) {
  float r1 = rand(seed);
  float r2 = rand(seed);

  vec3 lightSurfacePos = light.position + light.u * r1 + light.v * r2;
  lightSample.direction = lightSurfacePos - scatterPos;
  lightSample.dist = length(lightSample.direction);
  float distSq = lightSample.dist * lightSample.dist;
  lightSample.direction /= lightSample.dist;
  lightSample.normal = normalize(cross(light.u, light.v));
  lightSample.emittance = light.emittance;
  lightSample.pdf =
      distSq /
      (light.area * abs(dot(lightSample.normal, lightSample.direction)));
  lightSample.shouldMis = 1.0;
}

void sampleDistantLight(inout uint seed, in GpuLight light, in vec3 scatterPos,
                        inout LightSample lightSample) {
  lightSample.normal = vec3(0.0);
  lightSample.direction = normalize(light.direction);
  lightSample.emittance = light.emittance;
  lightSample.dist = INFINITY;
  lightSample.pdf = 1.0;
  lightSample.shouldMis = 0.0;
}

void sampleOneLight(inout uint seed, in GpuLight light, in vec3 scatterPos,
                    inout LightSample lightSample) {
  int type = int(light.type);
  if (type == LightTypeRect)
    sampleRectLight(seed, light, scatterPos, lightSample);
  else if (type == LightTypeDirectional)
    sampleDistantLight(seed, light, scatterPos, lightSample);
  else if (type == LightTypeTriangle)
    sampleTriangleLight(seed, light, scatterPos, lightSample);
}

float pdfEnvmap(in sampler2D envmapSamplers[3], in vec3 direction,
                in mat4 envTransform, in vec2 hdrResolution) {
  direction = transformVector(transpose(envTransform), direction);
  direction = normalize(direction);

  float theta = acos(clamp(direction.y, -1.0, 1.0));
  vec2 uv =
      vec2((PI + atan(direction.z, direction.x)) * INV_2PI, theta * INV_PI);
  float pdf = texture(envmapSamplers[2], uv).y *
              texture(envmapSamplers[1], vec2(0., uv.y)).y;
  return (pdf * hdrResolution.x * hdrResolution.y) / (TWO_PI * PI * sin(theta));
}

vec3 evalEnvmap(in sampler2D envmapSamplers[3], in vec3 evalDir,
                in mat4 envTransform, in float intensity) {
  evalDir = transformVector(transpose(envTransform), evalDir);
  evalDir = normalize(evalDir);

  float theta = acos(clamp(evalDir.y, -1.0, 1.0));
  vec2 uv = vec2((PI + atan(evalDir.z, evalDir.x)) * INV_2PI, theta * INV_PI);
  return intensity * texture(envmapSamplers[0], uv).rgb;
}

vec3 sampleEnvmap(in uint seed, in sampler2D envmapSamplers[3],
                  in mat4 envTransform, in vec2 hdrResolution, inout float pdf) {
  float r1 = rand(seed);
  float r2 = rand(seed);

  float v = texture(envmapSamplers[1], vec2(0., r1)).x;  // marginal
  float u = texture(envmapSamplers[2], vec2(r2, v)).x;   // conditional

  pdf = texture(envmapSamplers[2], vec2(u, v)).y *
        texture(envmapSamplers[1], vec2(0., v)).y;

  float phi = u * TWO_PI;
  float theta = v * PI;

  if (sin(theta) == 0.0) pdf = 0.0;

  pdf = (pdf * hdrResolution.x * hdrResolution.y) / (TWO_PI * PI * sin(theta));
  vec3 rayDir =
      vec3(-sin(theta) * cos(phi), cos(theta), -sin(theta) * sin(phi));

  rayDir = transformVector(envTransform, rayDir);
  return rayDir;
}

#endif