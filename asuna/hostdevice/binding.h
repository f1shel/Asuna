#ifndef BINDING_H
#define BINDING_H

#ifdef __cplusplus
#    include <cstdint>
#    include "nvmath/nvmath.h"
using vec2 = nvmath::vec2f;
using vec3 = nvmath::vec3f;
using vec4 = nvmath::vec4f;
using mat4 = nvmath::mat4f;
using uint = unsigned int;
#endif

// clang-format off
#ifdef __cplusplus        // Descriptor binding helper for C++ and GLSL
#define START_ENUM(a) enum a {
#define END_ENUM()    }
#else
#define START_ENUM(a) const uint
#define END_ENUM()
#endif

// Sets
START_ENUM(GPUSet)
  S_ACCEL = 0,  // Acceleration structure
  S_OUT   = 1,  // Offscreen output image
  S_SCENE = 2,  // Scene data
  S_ENV   = 3,  // Environment / Sun & Sky
  S_CNT   = 4
END_ENUM();

// Acceleration Structure - Set 0
START_ENUM(AccelBindings)
  eTlas = 0 
END_ENUM();

// Output image - Set 1
START_ENUM(OutputBindings)
  eStore   = 0  // As storage
END_ENUM();

// Scene Data - Set 2
START_ENUM(SceneBindings)
  eCamera    = 0, 
  eInstData  = 1, 
  eLights    = 2,            
  eTextures  = 3  // must be last elem            
END_ENUM();

// Environment - Set 3
START_ENUM(EnvBindings)
  eSunSky     = 0
END_ENUM();

#define eNStores 4

// clang-format on

#endif