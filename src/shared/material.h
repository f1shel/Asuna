#ifndef MATERIAL_H
#define MATERIAL_H

#include "binding.h"

// clang-format off
START_ENUM(MaterialType)
  MaterialTypeBrdfLambertian            = 0,
  MaterialTypeBrdfPbrMetalnessRoughness = 1,
  MaterialTypeBrdfEmissive              = 2,
  MaterialTypeNum                       = 3
END_ENUM();
// clang-format on

struct GpuMaterial
{
  vec3  diffuse;             // diffuse albedo
  float roughness;           // roughness
  vec3  emittance;           // emittance
  float metalness;           // metalness
  vec3  emittanceFactor;     // emittance factor
  int   diffuseTextureId;    // diffuse albedo texture id
  int   roughnessTextureId;  // roughness texture id
  int   metalnessTextureId;  // roughness texture id
  int   emittanceTextureId;  // emittance texture
  uint  type;                // material type
};

#endif