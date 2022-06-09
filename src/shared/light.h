#ifndef LIGHT_H
#define LIGHT_H

#include "binding.h"

// clang-format off
START_ENUM(LightType)
  LightTypeDirectional = 0,
  LightTypeRect        = 1,
  LightTypeTriangle    = 2,
  LightTypeUndefined   = 3
END_ENUM();
// clang-format on

struct GpuLight {
  int type;
  vec3 position;
  vec3 direction;
  vec3 emittance;
  vec3 u, v;
  float radius;
  float area;
  uint doubleSide;
};

#endif