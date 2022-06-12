#ifndef SAMPLE_LIGHT_GLSL
#define SAMPLE_LIGHT_GLSL

#include "../../shared/light.h"
#include "../../shared/sun_and_sky.h"
#include "math.glsl"
#include "structs.glsl"
#include "sun_and_sky.glsl"

vec3 sampleTriangleLight(vec2 r, GpuLight light, vec3 scatterPos,
                         inout LightSamplingRecord lRec) {
  float r1 = r.x;
  float r2 = (1 - r1) * r.y;
  vec3 radiance = vec3(0);

  vec3 lightSurfacePos = light.position + light.u * r1 + light.v * r2;
  lRec.d = lightSurfacePos - scatterPos;
  lRec.dist = length(lRec.d);
  float distSq = lRec.dist * lRec.dist;
  lRec.d /= lRec.dist;
  lRec.n = makeNormal(cross(light.u, light.v));
  radiance = light.radiance;
  lRec.pdf = distSq / (light.area * abs(dot(lRec.n, lRec.d)));
  lRec.flags = EArea;

  return radiance;
}

vec3 sampleDistantLight(in GpuLight light, in vec3 scatterPos,
                        inout LightSamplingRecord lRec) {
  lRec.d = makeNormal(light.direction);
  lRec.n = -lRec.d;
  vec3 radiance = light.radiance;
  lRec.dist = INFINITY;
  lRec.pdf = 1.0;
  lRec.flags = EDelta;
  return radiance;
}

vec3 sampleRectLight(vec2 r, GpuLight light, vec3 scatterPos,
                     out LightSamplingRecord lRec) {
  float r1 = r.x;
  float r2 = r.y;
  vec3 radiance = vec3(0);

  vec3 lightSurfacePos = light.position + light.u * r1 + light.v * r2;
  lRec.d = lightSurfacePos - scatterPos;
  lRec.dist = length(lRec.d);
  float distSq = lRec.dist * lRec.dist;
  lRec.d /= lRec.dist;
  lRec.n = makeNormal(cross(light.u, light.v));
  radiance = light.radiance;
  lRec.pdf = distSq / (light.area * abs(dot(lRec.n, lRec.d)));
  lRec.flags = EArea;

  return radiance;
}
vec3 sampleOneLight(vec2 r, GpuLight light, vec3 scatterPos,
                    inout LightSamplingRecord lRec) {
  int type = int(light.type);
  if (type == LightTypeRect)
    return sampleRectLight(r, light, scatterPos, lRec);
  else if (type == LightTypeDirectional)
    sampleDistantLight(light, scatterPos, lRec);
  else if (type == LightTypeTriangle)
    sampleTriangleLight(r, light, scatterPos, lRec);
}

float pdfEnvmap(in sampler2D envmapSamplers[3], in mat4 envTransform,
                in vec2 hdrResolution, in vec3 L) {
  L = transformDirection(transpose(envTransform), L);

  float theta = acos(clamp(L.y, -1.0, 1.0));
  vec2 uv = vec2((PI + atan(L.z, L.x)) * INV_2PI, theta * INV_PI);
  float pdf = texture(envmapSamplers[2], uv).y *
              texture(envmapSamplers[1], vec2(0., uv.y)).y;
  float sinTheta = sin(theta);
  if (sinTheta == 0) return 0;
  return (pdf * hdrResolution.x * hdrResolution.y) / (TWO_PI * PI * sinTheta);
}

vec3 evalEnvmap(in sampler2D envmapSamplers[3], in mat4 envTransform,
                in float intensity, in vec3 L) {
  L = transformDirection(transpose(envTransform), L);

  float theta = acos(clamp(L.y, -1.0, 1.0));
  vec2 uv = vec2((PI + atan(L.z, L.x)) * INV_2PI, theta * INV_PI);
  return intensity * texture(envmapSamplers[0], uv).rgb;
}

vec3 sampleEnvmap(vec2 r, sampler2D envmapSamplers[3], mat4 envTransform,
                  vec2 hdrResolution, float intensity, out vec3 L,
                  out float pdf) {
  float r1 = r.x;
  float r2 = r.y;

  vec2 uv;
  uv.y = texture(envmapSamplers[1], vec2(0., r1)).x;    // marginal
  uv.x = texture(envmapSamplers[2], vec2(r2, uv.y)).x;  // conditional

  pdf = texture(envmapSamplers[2], uv).y *
        texture(envmapSamplers[1], vec2(0., uv.y)).y;

  float phi = uv.x * TWO_PI;
  float theta = uv.y * PI;

  if (sin(theta) == 0.0) pdf = 0.0;
  pdf = (pdf * hdrResolution.x * hdrResolution.y) / (TWO_PI * PI * sin(theta));

  L = vec3(-sin(theta) * cos(phi), cos(theta), -sin(theta) * sin(phi));
  L = transformDirection(envTransform, L);

  return intensity * texture(envmapSamplers[0], uv).rgb;
}

vec3 sampleBackGround(vec2 u, vec3 bgColor, out vec3 L, out float pdf) {
  L = uniformSampleSphere(u);
  pdf = uniformSpherePdf();
  return bgColor;
}

vec3 sampleSunSky(vec2 u, GpuSunAndSky sk, out vec3 L, out float pdf) {
  L = uniformSampleSphere(u);
  pdf = uniformSpherePdf();
  return sun_and_sky(sk, L);
}

#endif