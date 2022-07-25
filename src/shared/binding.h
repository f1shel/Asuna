#ifndef BINDING_H
#define BINDING_H

#ifdef __cplusplus
#include <cstdint>
#include "nvmath/nvmath.h"
using vec2 = nvmath::vec2f;
using vec3 = nvmath::vec3f;
using vec4 = nvmath::vec4f;
using mat4 = nvmath::mat4f;
using mat3 = nvmath::mat3f;
using uint = unsigned int;
#endif

#ifdef __cplusplus  // Descriptor binding helper for C++ and GLSL
#define START_ENUM(a) enum a {
#define END_ENUM() }
#else
#define START_ENUM(a) const uint
#define END_ENUM()
#endif

// clang-format off
// Sets
START_ENUM(RasterBindSet)
  RasterNum = 0
END_ENUM();

START_ENUM(RtBindSet)
  RtAccel = 0,  // Acceleration structure
  RtOut   = 1,  // Offscreen output image
  RtScene = 2,  // Scene data
  RtEnv   = 3,  // Environment / Sun & Sky
  RtNum   = 4
END_ENUM();

START_ENUM(PostBindSet)
  PostInput = 0,
  PostNum   = 1
END_ENUM();

// Acceleration Structure - Set 0
START_ENUM(AccelBindings)
  AccelTlas = 0 
END_ENUM();

// Output image - Set 1
START_ENUM(OutputBindings)
  OutputStore   = 0  // As storage
END_ENUM();

// Scene Data - Set 2
START_ENUM(SceneBindings)
  SceneCamera    = 0, 
  SceneInstances = 1, 
  SceneLights    = 2,            
  SceneTextures  = 3  // must be last elem            
END_ENUM();

// Environment - Set 3
START_ENUM(EnvBindings)
  EnvSunsky = 0,
  EnvAccelMap = 1
END_ENUM();

START_ENUM(InputBindings)
  InputSampler = 0
END_ENUM();

#define NUM_OUTPUT_IMAGES 8
// clang-format on

#endif