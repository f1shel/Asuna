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
  float dist;
  vec3  direction;
  float pdf;
  vec3  emittance;
  float shouldMis;
};

struct RayPayload
{
  Ray        ray;
  BsdfSample bsdf;
  vec3       radiance;
  uint       seed;
  vec3       throughput;
  uint       depth;
  uint       stop;
};

struct VisibilityContribution
{
  vec3  radiance;   // Radiance at the point if light is visible
  float lightDist;  // Distance to the light (1e32 for infinite or sky)
  vec3  lightDir;   // Direction to the light, to shoot shadow ray
  uint  visible;    // true if in front of the face and should shoot shadow ray
};

#endif