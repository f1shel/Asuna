#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

struct Ray
{
  vec3 origin;
  vec3 direction;
};

struct BsdfSample
{
  vec3  direction;
  float pdf;
};

struct LightSample
{
  vec3  normal;
  vec3  direction;
  vec3  emittance;
  float dist;
  float pdf;
  float shouldMis;
};

struct RayPayload
{
  uvec4      seed;
  uint       depth;
  uint       stop;
  Ray        ray;
  vec3       hitPos;
  vec3       radiance;
  vec3       throughput;
  vec3       absorption;
  float      hitDis;
  BsdfSample bsdf;
};

struct VisibilityContribution
{
  vec3  radiance;   // Radiance at the point if light is visible
  vec3  lightDir;   // Direction to the light, to shoot shadow ray
  float lightDist;  // Distance to the light (1e32 for infinite or sky)
  bool  visible;    // true if in front of the face and should shoot shadow ray
};

#endif