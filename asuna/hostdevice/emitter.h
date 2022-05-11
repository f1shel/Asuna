#ifndef EMITTER_H
#define EMITTER_H

#include "binding.h"

// clang-format off
START_ENUM(GPUEmitterType)
  ePointLight       = 0,
  eDirectionalLight = 1,
  eUndefinedLight   = 2
END_ENUM();
// clang-format on

struct GPUEmitter
{
    int  type;
    vec3 direction;
    vec3 emittance;
};

#endif