#ifndef PUSHCONSTANT_H
#define PUSHCONSTANT_H

#include "binding.h"

struct GpuPushConstantGraphics {
  uint placeholder;
};

struct GpuPushConstantRaytrace {
  int spp;
  int curFrame;
  int maxPathDepth;
  int numLights;

  vec3 bgColor;
  uint useFaceNormal;

  uint ignoreEmissive;
  uint hasEnvMap;
  vec2 envMapResolution;

  float envMapIntensity;
  uint nMultiChannel;
  int diffuseOutChannel;
  int specularOutChannel;

  int roughnessOutChannel;
  int normalOutChannel;
  int positionOutChannel;
  int tangentOutChannel;

  int uvOutChannel;
};

// clang-format off
START_ENUM(ToneMappingType)
  ToneMappingTypeNone     = 0,
  ToneMappingTypeGamma    = 1,
  ToneMappingTypeReinhard = 2,
  ToneMappingTypeAces     = 3,
  ToneMappingTypeFilmic   = 4,
  ToneMappingTypePbrt     = 5,
  ToneMappingTypeCustom   = 6,
  ToneMappingTypeNum      = 7
END_ENUM();
// clang-format on

// Tonemapper used in post.frag
struct GpuPushConstantPost {
  float brightness;
  float contrast;
  float saturation;
  float vignette;
  float avgLum;
  float zoom;
  vec2 renderingRatio;
  int autoExposure;
  float Ywhite;  // Burning white
  float key;     // Log-average luminance
  uint tmType;
};

#endif