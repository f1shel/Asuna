#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

struct Ray {
  vec3 origin;
  vec3 direction;
};

struct BsdfSample {
  vec3 val;
  vec3 direction;
  float pdf;
  bool shouldMis;
};

struct LightSample {
  vec3 normal;
  float dist;
  vec3 direction;
  float pdf;
  vec3 emittance;
  bool shouldMis;
};

struct VisibilityContribution {
  vec3 radiance;    // Radiance at the point if light is visible
  float lightDist;  // Distance to the light (1e32 for infinite or sky)
  vec3 lightDir;    // Direction to the light, to shoot shadow ray
  bool visible;     // true if in front of the face and should shoot shadow ray
};

struct RayPayload {
  Ray ray;
  vec3 radiance;
  vec3 throughput;
  uint depth;
  uint seed;
  bool stop;
  // for indirect light
  float bsdfPdf;
  bool bsdfShouldMis;
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
  // for debug
  vec3 debugInfo;
};

#endif