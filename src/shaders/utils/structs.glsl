#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

#define USE_MIS 0

#include "../../shared/binding.h"

struct Ray {
  // Origin in world space
  vec3 o;
  // Direction in world space
  vec3 d;
};

// clang-format off
const uint EBsdfNull = 0;
const uint EDiffuseReflection = 1 << 0;
const uint EDiffuseTransmission = 1 << 1;
const uint EGlossyReflection = 1 << 2;
const uint EGlossyTransmission = 1 << 3;
const uint ESpecularReflection = 1 << 4;
const uint ESpecularTransmission = 1 << 5;
const uint EReflection = EDiffuseReflection | EGlossyReflection | ESpecularReflection;
const uint ETransmission = EDiffuseTransmission | EGlossyTransmission | ESpecularTransmission;
const uint EDiffuse = EDiffuseTransmission | EDiffuseReflection;
const uint EGlossy = EGlossyTransmission | EGlossyReflection;
const uint ESpecular = ESpecularTransmission | ESpecularReflection;
const uint ESmooth = EDiffuse | EGlossy;
// clang-format on

bool isNonSpecular(uint flags) { return (flags & ESmooth) != 0; }
bool isBsdf(uint flags) { return flags != EBsdfNull; }
bool isBlack(vec3 val) { return length(val) == 0; }

struct BsdfSamplingRecord {
  // Direction in world space
  vec3 d;
  // Importance sampling
  float pdf;
  // Whether sampled lobe is specular, etc.
  uint flags;
};

const uint ELightNull = 0;
const uint EDelta = 1 << 0;
const uint EArea = 1 << 1;

struct LightSamplingRecord {
  // Direction in world space: from [scatterPos] to [lightPos]
  vec3 d;
  // Normal of sampled light
  vec3 n;
  // Distance between [scatterPos] and [lightPos]
  float dist;
  // Importance sampling
  float pdf;
  // Whether sampled emitter is delta, etc.
  uint flags;
};

struct DirectLightRecord {
  // Radiance of direct light contribution
  vec3 radiance;
  // Distance to the light (infinity for far away light)
  float dist;
  // Shadow ray
  Ray ray;
  // Should shoot shadow ray and test
  bool skip;
};

struct PathRecord {
  Ray ray;
  vec3 radiance;
  vec3 throughput;
  uint depth;
  uint seed;
  bool stop;
};

struct MultiChannelRecord {
  vec3 channel[NUM_OUTPUT_IMAGES-1];
};

struct RayPayload {
  PathRecord pRec;
  BsdfSamplingRecord bRec;
  MultiChannelRecord mRec;
  DirectLightRecord dRec;
};

#endif