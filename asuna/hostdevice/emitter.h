#ifndef EMITTER_H
#define EMITTER_H

#include "binding.h"

// clang-format off
START_ENUM(GPUEmitterType)
  eDirectionalLight = 0,
  eRectLight        = 1,
  eSphereLight      = 2,
  eUndefinedLight   = 3
END_ENUM();
// clang-format on

struct GPUEmitter
{
    int   type;
    vec3  position;
    vec3  direction;
    vec3  emittance;
    vec3  u, v;
    float radius;
    float area;
};

#endif