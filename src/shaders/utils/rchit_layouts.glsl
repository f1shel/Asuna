#ifndef LAYOUTS_GLSL
#define LAYOUTS_GLSL

#include "../../shared/binding.h"
#include "../../shared/light.h"
#include "../../shared/material.h"
#include "../../shared/pushconstant.h"
#include "../../shared/instance.h"
#include "../../shared/sun_and_sky.h"
#include "../../shared/vertex.h"
#include "../../shared/camera.h"
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
layout(set = RtScene, binding = SceneCamera)            uniform _Camera    { GpuCamera cameraInfo; };
layout(set = RtEnv,   binding = EnvSunsky, scalar)      uniform _SunAndSky { GpuSunAndSky sunAndSky; };
layout(set = RtEnv,   binding = EnvAccelMap)            uniform sampler2D  envmapSamplers[3];
//
layout(push_constant)                                   uniform _RtxState  { GpuPushConstantRaytrace pc; };
//
layout(location = 0) rayPayloadInEXT RayPayload payload;
//
hitAttributeEXT vec2 _bary;
// clang-format on

struct HitState {
  // lightId >= 0 if hit point is a emitter
  int lightId;
  // texture coordinates
  vec2 uv;
  // hit point position
  vec3 pos;
  // normalized view direction in world space
  vec3 V;
  // interpolated vertex normal/shading Normal
  vec3 N;
  // geo normal
  vec3 geoN;
  // face forward normal
  vec3 ffN;
  // tangent
  vec3 X;
  // bitangent
  vec3 Y;
  // material
  GpuMaterial mat;
};

// clang-format off
void configureShadingFrame(inout HitState state) {
  if (pc.useFaceNormal == 1) state.N = state.geoN;
  basis(state.N, state.X, state.Y);
  state.ffN = dot(state.N, state.V) > 0 ? state.N : -state.N;
}

HitState getHitState() {
  HitState state;

  GpuInstance _inst     = instances.i[gl_InstanceID];
  Indices     _indices  = Indices(_inst.indexAddress);
  Vertices    _vertices = Vertices(_inst.vertexAddress);

  ivec3     id = _indices.i[gl_PrimitiveID];
  GpuVertex v0 = _vertices.v[id.x];
  GpuVertex v1 = _vertices.v[id.y];
  GpuVertex v2 = _vertices.v[id.z];
  vec3      ba = vec3(1.0 - _bary.x - _bary.y, _bary.x, _bary.y);

  state.lightId = _inst.lightId;
  state.uv      = barymix2(v0.uv, v1.uv, v2.uv, ba);
  state.pos     = gl_ObjectToWorldEXT * vec4(barymix3(v0.pos, v1.pos, v2.pos, ba), 1.f);
  state.N       = barymix3(v0.normal, v1.normal, v2.normal, ba);
  state.N       = makeNormal((state.N * gl_WorldToObjectEXT).xyz);
  state.ffN     = cross(v1.pos - v0.pos, v2.pos - v0.pos);
  state.ffN     = makeNormal((state.ffN * gl_WorldToObjectEXT).xyz);
  state.V       = makeNormal(-gl_WorldRayDirectionEXT);

  // Get material if hit surface is not emitter
  if (state.lightId < 0) state.mat = Materials(_inst.materialAddress).m[0];

  configureShadingFrame(state);

  return state;
}
// clang-format on

vec3 sampleEnvironmentLight(inout LightSamplingRecord lRec) {
  vec3 radiance = vec3(0);
  // Sample environment light
  lRec.flags = EArea;
  lRec.dist = INFINITY;
  // Eusure visible light
  lRec.n = -makeNormal(gl_WorldRayDirectionEXT);
  if (sunAndSky.in_use == 1)
    radiance =
        sampleSunSky(rand2(payload.pRec.seed), sunAndSky, lRec.d, lRec.pdf);
  else if (pc.hasEnvMap == 1)
    radiance = sampleEnvmap(rand2(payload.pRec.seed), envmapSamplers,
                            cameraInfo.envTransform, pc.envMapResolution,
                            pc.envMapIntensity, lRec.d, lRec.pdf);
  else
    radiance = sampleBackGround(rand2(payload.pRec.seed), pc.bgColor, lRec.d,
                                lRec.pdf);
  return radiance;
}

vec3 sampleLights(vec3 scatterPos, vec3 scatterNormal, out bool visible, out LightSamplingRecord lRec) {
    bool allowDoubleSide = false;
    vec3 radiance = vec3(0);

    // 4 cases:
    // (1) has env & light: envSelectPdf = 0.5, analyticSelectPdf = 0.5
    // (2) no env & light: skip direct light
    // (3) has env, no light: envSelectPdf = 1.0, analyticSelectPdf = 0.0
    // (4) no env, has light: envSelectPdf = 0.0, analyticSelectPdf = 1.0
    bool hasEnv = (pc.hasEnvMap == 1 || sunAndSky.in_use == 1);
    bool hasLight = (pc.numLights > 0);
    float envSelectPdf, analyticSelectPdf;
    if (hasEnv && hasLight)
      envSelectPdf = analyticSelectPdf = 0.5f;
    else if (hasEnv)
      envSelectPdf = 1.f, analyticSelectPdf = 0.f;
    else if (hasLight)
      envSelectPdf = 0.f, analyticSelectPdf = 1.f;
    else
      envSelectPdf = analyticSelectPdf = 0.f;

    float envOrAnalyticSelector = rand(payload.pRec.seed);
    if (envOrAnalyticSelector < envSelectPdf) {
      // Sample environment light
      radiance = sampleEnvironmentLight(lRec) / envSelectPdf;
      allowDoubleSide = true;
    } else if (envOrAnalyticSelector < envSelectPdf + analyticSelectPdf) {
      // Sample analytic light, randomly pick one
      int lightIndex =
          min(1 + int(rand(payload.pRec.seed) * pc.numLights), pc.numLights);
      GpuLight light = lights.l[lightIndex];
      radiance = sampleOneLight(rand2(payload.pRec.seed), light, scatterPos,
                                lRec) *
            pc.numLights / analyticSelectPdf;
      allowDoubleSide = (light.doubleSide == 1);
    }

    // Configure direct light setting by light sample
    payload.dRec.ray.o = offsetPositionAlongNormal(scatterPos, scatterNormal);
    payload.dRec.ray.d = lRec.d;
    payload.dRec.dist = lRec.dist;

    // Light at the back of the surface is not visible
    visible = (dot(lRec.d, scatterNormal) > 0.0 && lRec.pdf > 0.0);
    // Surface on the back of the light is also not illuminated
    visible =
        visible && (dot(lRec.n, lRec.d) < 0 || allowDoubleSide);

    return radiance;
}

vec4 textureEval(int texId, vec2 uv) {
  return texture(textureSamplers[nonuniformEXT(texId)], uv).rgba;
}

#endif