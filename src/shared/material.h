#ifndef MATERIAL_H
#define MATERIAL_H

#include "binding.h"

// clang-format off
START_ENUM(MaterialType)
  MaterialTypeBrdfLambertian                = 0,
  MaterialTypeBrdfEmissive                  = 1,
  MaterialTypeBrdfPbrMetalnessRoughness     = 2,
  MaterialTypeBrdfKang18                    = 3,
  MaterialTypeNum                           = 4
END_ENUM();
// clang-format on

struct GpuMaterial
{
  vec3  diffuse;             // diffuse albedo
  vec3  rhoSpec;             // kang18
  vec2  anisoAlpha;          // kang18
  float roughness;           // disney
  float subsurface;          // disney
  float specular;            // disney
  float specularTint;        // disney
  float anisotropic;         // disney
  float sheen;               // disney
  float sheenTint;           // disney
  float clearcoat;           // disney
  float clearcoatGloss;      // disney
  vec3  emittance;           // emittance
  float metalness;           // metalness
  vec3  emittanceFactor;     // emittance factor
  int   diffuseTextureId;    // diffuse albedo texture id
  int   roughnessTextureId;  // roughness texture id
  int   metalnessTextureId;  // metalness texture id
  int   emittanceTextureId;  // emittance texture id
  int   normalTextureId;     // normal texture id
  int   tangentTextureId;    // tangent texture id
  uint  type;                // material type
};

#endif