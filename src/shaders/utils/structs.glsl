#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

#include "../../shared/material.h"

struct Ray {
  vec3 origin;
  vec3 direction;
};

struct BsdfSample {
  vec3 direction;
  float pdf;
};

struct LightSample {
  vec3 normal;
  float dist;
  vec3 direction;
  float pdf;
  vec3 emittance;
  float shouldMis;
};

struct VisibilityContribution {
  vec3 radiance;    // Radiance at the point if light is visible
  float lightDist;  // Distance to the light (1e32 for infinite or sky)
  vec3 lightDir;    // Direction to the light, to shoot shadow ray
  uint visible;     // true if in front of the face and should shoot shadow ray
};

struct RayPayload {
  Ray ray;
  float bsdfPdf;
  vec3 radiance;
  vec3 throughput;
  uint depth;
  uint seed;
  bool stop;
  // for direct light
  bool shouldDirectLight;
  bool directVisible;
  float directDist;
  vec3 directHitPos;
  vec3 directDir;
  vec3 directContribution;
  // for denoiser
  vec3 denoiserAlbedo;
  vec3 denoiserNormal;
};

#endif