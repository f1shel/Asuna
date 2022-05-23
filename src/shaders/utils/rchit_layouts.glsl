#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL

#include "../../shared/binding.h"
#include "../../shared/light.h"
#include "../../shared/material.h"
#include "../../shared/pushconstant.h"
#include "../../shared/instance.h"
#include "../../shared/sun_and_sky.h"
#include "../../shared/vertex.h"
#include "structs.glsl"
#include "math.glsl"
#include "sample_light.glsl"
#include "structs.glsl"
#include "sun_and_sky.glsl"

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
layout(location = 0) rayPayloadInEXT RayPayload payload;
//
hitAttributeEXT vec2 _bary;
// clang-format on

struct HitState
{
  int         lightId;        // lightId >= 0 if hit point is a emitter
  vec2        uv;             // texture coordinates
  vec3        hitPos;         // hit point position
  vec3        shadingNormal;  // interpolated vertex normal
  vec3        faceNormal;     // face normal
  vec3        ffnormal;       // face forward normal, Z
  vec3        tangent;        // tangent, X
  vec3        bitangent;      // bitangent, Y
  vec3        viewDir;        // normalized view direction
  GpuMaterial mat;            // material
};

HitState getHitState()
{
  HitState    state;
  GpuInstance _inst     = instances.i[gl_InstanceID];
  Indices     _indices  = Indices(_inst.indexAddress);
  Vertices    _vertices = Vertices(_inst.vertexAddress);
  ivec3       id        = _indices.i[gl_PrimitiveID];
  GpuVertex   v0        = _vertices.v[id.x];
  GpuVertex   v1        = _vertices.v[id.y];
  GpuVertex   v2        = _vertices.v[id.z];
  vec3        bary      = vec3(1.0 - _bary.x - _bary.y, _bary.x, _bary.y);
  state.lightId         = _inst.lightId;
  state.uv              = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;
  state.hitPos          = gl_ObjectToWorldEXT * vec4(v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z, 1.f);
  state.shadingNormal   = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
  state.faceNormal      = normalize(cross(v1.pos - v0.pos, v2.pos - v0.pos));
  state.viewDir         = -normalize(gl_WorldRayDirectionEXT);
  if(state.lightId < 0)
    state.mat = Materials(_inst.materialAddress).m[0];
  if(pc.useFaceNormal == 1)
    state.ffnormal = dot(state.faceNormal, state.viewDir) > 0.0 ? state.faceNormal : -state.faceNormal;
  else
    state.ffnormal = dot(state.shadingNormal, state.viewDir) > 0.0 ? state.shadingNormal : -state.shadingNormal;
  basis(state.ffnormal, state.tangent, state.bitangent);

  return state;
}

void hitLight(in int lightId, in vec3 hitPos)
{
  GpuLight light               = lights.l[lightId];
  vec3     lightDirection      = normalize(hitPos - payload.ray.origin);
  vec3     lightNormal         = normalize(cross(light.u, light.v));
  float    lightSideProjection = dot(lightNormal, lightDirection);

  payload.stop = 1;
  // single side light
  if(lightSideProjection > 0)
    return;
  // do not mis
  if(payload.depth == 0)
  {
    payload.radiance = light.emittance;
    return;
  }
  // do mis
  float lightDist  = length(hitPos - payload.ray.origin);
  float distSquare = lightDist * lightDist;
  float lightPdf   = distSquare / (light.area * abs(lightSideProjection) + EPS);
  float misWeight  = powerHeuristic(payload.bsdfPdf, lightPdf);
  payload.radiance += payload.throughput * light.emittance * misWeight;
  return;
}

#endif