#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL

#include "../../shared/binding.h"
#include "../../shared/light.h"
#include "../../shared/material.h"
#include "../../shared/pushconstant.h"
#include "../../shared/instance.h"
#include "../../shared/sun_and_sky.h"
#include "../../shared/vertex.h"

// clang-format off
layout(buffer_reference, scalar) buffer Vertices  { GpuVertex v[];   };
layout(buffer_reference, scalar) buffer Indices   { ivec3 i[];       };
layout(buffer_reference, scalar) buffer Materials { GpuMaterial m[1]; };
//
layout(set = RtAccel, binding = AccelTlas)              uniform accelerationStructureEXT tlas;
layout(set = RtScene, binding = SceneTextures)          uniform sampler2D  textureSamplers[];
layout(set = RtScene, binding = SceneInstances, scalar) buffer  _Instances { GpuInstance i[];        } instances;
layout(set = RtScene, binding = SceneLights, scalar)    buffer  _Lights    { GpuLight l[];           } lights;
layout(set = RtEnv,   binding = EnvSunsky, scalar)      uniform _SunAndSky { GpuSunAndSky sunAndSky; };
//
layout(push_constant)                                   uniform _RtxState  { GpuPushConstantRaytrace pc; };
//
hitAttributeEXT vec2 _bary;
// clang-format on

#define UNPACK_VERTEX_INFO(id, v0, v1, v2, bary, uv, hitPos, shadingNormal, faceNormal, lightId, material)             \
  ivec3       id;                                                                                                      \
  int         lightId;                                                                                                 \
  vec2        uv;                                                                                                      \
  vec3        bary, hitPos, shadingNormal, faceNormal;                                                                 \
  GpuVertex   v0, v1, v2;                                                                                              \
  GpuMaterial material;                                                                                                \
  {                                                                                                                    \
    GpuInstance _inst     = instances.i[gl_InstanceID];                                                                \
    Indices     _indices  = Indices(_inst.indexAddress);                                                               \
    Vertices    _vertices = Vertices(_inst.vertexAddress);                                                             \
    id                    = _indices.i[gl_PrimitiveID];                                                                \
    lightId               = _inst.lightId;                                                                             \
    v0                    = _vertices.v[id.x];                                                                         \
    v1                    = _vertices.v[id.y];                                                                         \
    v2                    = _vertices.v[id.z];                                                                         \
    bary                  = vec3(1.0 - _bary.x - _bary.y, _bary.x, _bary.y);                                           \
    uv                    = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;                                          \
    hitPos                = gl_ObjectToWorldEXT * vec4(v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z, 1.f);      \
    shadingNormal         = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);                   \
    faceNormal            = normalize(cross(v1.pos - v0.pos, v2.pos - v0.pos));                                        \
    if(lightId < 0)                                                                                                    \
    {                                                                                                                  \
      material = Materials(_inst.materialAddress).m[0];                                                                \
      if(material.diffuseTextureId >= 0)                                                                               \
        material.diffuse = texture(textureSamplers[nonuniformEXT(material.diffuseTextureId)], uv).rgb;                 \
      if(material.roughnessTextureId >= 0)                                                                             \
        material.roughness = texture(textureSamplers[nonuniformEXT(material.roughnessTextureId)], uv).r;               \
      if(material.metalnessTextureId >= 0)                                                                             \
        material.metalness = texture(textureSamplers[nonuniformEXT(material.metalnessTextureId)], uv).r;               \
      if(material.emittanceTextureId >= 0)                                                                             \
        material.emittance =                                                                                           \
            material.emittanceFactor * texture(textureSamplers[nonuniformEXT(material.emittanceTextureId)], uv).rgb;   \
    }                                                                                                                  \
  }

#endif