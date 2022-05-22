#ifndef MATERIAL_H
#define MATERIAL_H

#include "binding.h"

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
};

#endif