#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL

#include "../../../hostdevice/binding.h"
#include "../../../hostdevice/light.h"
#include "../../../hostdevice/material.h"
#include "../../../hostdevice/pushconstant.h"
#include "../../../hostdevice/instance.h"
#include "../../../hostdevice/sun_and_sky.h"
#include "../../../hostdevice/vertex.h"

// clang-format off
layout(buffer_reference, scalar) buffer Vertices  { GpuVertex v[];   };
layout(buffer_reference, scalar) buffer Indices   { ivec3 i[];       };
layout(buffer_reference, scalar) buffer Materials { GpuMaterial m[]; };
//
layout(set = RtAccel, binding = AccelTlas)              uniform accelerationStructureEXT tlas;
layout(set = RtScene, binding = SceneTextures)          uniform sampler2D  textureSamplers[];
layout(set = RtScene, binding = SceneInstances, scalar) buffer  _Instances { GpuInstance i[];        } instances;
layout(set = RtScene, binding = SceneLights, scalar)    buffer  _Lights    { GpuLight l[];           } lights;
layout(set = RtEnv,   binding = EnvSunsky, scalar)      uniform _SunAndSky { GpuSunAndSky sunAndSky; };
//
layout(push_constant)                                   uniform _RtxState  { GpuPushConstantRaytrace pc; };
//
hitAttributeEXT vec2 bary;
// clang-format on

#endif