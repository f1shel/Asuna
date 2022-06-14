#ifndef MATERIAL_H
#define MATERIAL_H

#include "binding.h"

// clang-format off
START_ENUM(MaterialType)
  MaterialTypeBrdfLambertian            = 0,
  MaterialTypeBrdfKang18                = 1,
  MaterialTypeBrdfEmissive              = 2,
  MaterialTypeBrdfPbrMetalnessRoughness = 3,
  MaterialTypeBrdfPlastic               = 4,
  MaterialTypeBrdfMirror                = 5,
  MaterialTypeBrdfRoughPlastic          = 6,
  MaterialTypeBsdfDielectric            = 7,
  MaterialTypeNum                       = 8
END_ENUM();
// clang-format on

struct GpuMaterial {
  vec3 diffuse;            // diffuse albedo
  vec3 rhoSpec;            // kang18
  vec2 anisoAlpha;         // kang18
  float ior;               // dielectric
  float roughness;         // disney
  float subsurface;        // disney
  float specular;          // disney
  float specularTint;      // disney
  float anisotropic;       // disney
  float sheen;             // disney
  float sheenTint;         // disney
  float clearcoat;         // disney
  float clearcoatGloss;    // disney
  vec3 radiance;          // radiance
  float metalness;         // metalness
  vec3 radianceFactor;    // radiance factor
  int diffuseTextureId;    // diffuse albedo texture id
  int roughnessTextureId;  // roughness texture id
  int metalnessTextureId;  // metalness texture id
  int radianceTextureId;  // radiance texture id
  int normalTextureId;     // normal texture id
  int tangentTextureId;    // tangent texture id
  uint type;               // material type
};

#endif