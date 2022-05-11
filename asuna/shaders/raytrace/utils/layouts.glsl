#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL

#include "../../../hostdevice/binding.h"
#include "../../../hostdevice/emitter.h"
#include "../../../hostdevice/material.h"
#include "../../../hostdevice/pushconstant.h"
#include "../../../hostdevice/scene.h"
#include "../../../hostdevice/sun_and_sky.h"
#include "../../../hostdevice/vertex.h"
#include "math.glsl"
#include "structs.glsl"
#include "sun_and_sky.glsl"

// clang-format off
layout(buffer_reference, scalar) buffer Vertices  { GPUVertex v[];   };
layout(buffer_reference, scalar) buffer Indices   { ivec3 i[];       };
layout(buffer_reference, scalar) buffer Materials { GPUMaterial m[]; };
//
layout(set = S_ACCEL, binding = eTlas)             uniform accelerationStructureEXT topLevelAS;
//
layout(set = S_SCENE, binding = eTextures)         uniform sampler2D  textureSamplers[];
layout(set = S_SCENE, binding = eInstData, scalar) buffer  _SceneDesc { GPUInstanceDesc i[];        } instances;
layout(set = S_SCENE, binding = eLights,   scalar) buffer  _Emitters  { GPUEmitter l[];             } lights;
//                                                                                                  
layout(set = S_ENV,   binding = eSunSky,   scalar) uniform _SSBuffer  { SunAndSky sunAndSky;        };
//
layout(push_constant)                              uniform _RtxState  { GPUPushConstantRaytrace pc; };
//
hitAttributeEXT vec2 bary;
// clang-format on

#endif