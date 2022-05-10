#ifndef MACRO_GLSL
#define MACRO_GLSL

#include "../../../hostdevice/binding.h"
#include "../../../hostdevice/emitter.h"
#include "../../../hostdevice/material.h"
#include "../../../hostdevice/scene.h"
#include "../../../hostdevice/vertex.h"
#include "../../../hostdevice/pushconstant.h"
#include "math.glsl"
#include "structs.glsl"

/* Accessble variables:
 * -- RayPayload payload
 * -- bool isShadowed
 * -- ivec3 id
 * -- GPUVertex v0, v1, v2
 * -- vec3 bary
 * -- vec3 uv
 * -- vec3 pos
 * -- vec3 normal
 * -- vec3 hit
 * -- vec3 geoNormal
 */
#define TRACE_BLOCK                                                                         \
    layout(location = 0) rayPayloadInEXT RayPayload payload;                                \
    layout(location = 1) rayPayloadEXT bool         isShadowed;                             \
    layout(buffer_reference, scalar) buffer         Vertices                                \
    {                                                                                       \
        GPUVertex v[];                                                                      \
    };                                                                                      \
    layout(buffer_reference, scalar) buffer Indices                                         \
    {                                                                                       \
        ivec3 i[];                                                                          \
    };                                                                                      \
    layout(buffer_reference, scalar) buffer Materials                                       \
    {                                                                                       \
        GPUMaterial m[];                                                                    \
    };                                                                                      \
    layout(set = eGPUSetRaytraceRaytrace, binding = eGPUBindingRaytraceTlas)                \
        uniform accelerationStructureEXT topLevelAS;                                        \
    layout(set = eGPUSetRaytraceGraphics, binding = eGPUBindingGraphicsTextures)            \
        uniform sampler2D textureSamplers[];                                                \
    layout(set = eGPUSetRaytraceGraphics, binding = eGPUBindingGraphicsSceneDesc, scalar)   \
        buffer _SceneDesc                                                                   \
    {                                                                                       \
        GPUInstanceDesc m[];                                                                \
    }                                                                                       \
    sceneDesc;                                                                              \
    layout(set = eGPUSetRaytraceGraphics, binding = eGPUBindingGraphicsEmitters, scalar)    \
        buffer _Emitters                                                                    \
    {                                                                                       \
        GPUEmitter e[];                                                                     \
    }                                                                                       \
    emitters;                                                                               \
    layout(push_constant) uniform GPUPushConstantRaytrace_                                  \
    {                                                                                       \
        GPUPushConstantRaytrace pc;                                                         \
    };                                                                                      \
    hitAttributeEXT vec2 hitbary;                                                           \
    void                 main()                                                             \
    {                                                                                       \
        GPUInstanceDesc instDesc = sceneDesc.m[gl_InstanceCustomIndexEXT];                  \
        Indices         indices  = Indices(instDesc.indexAddress);                          \
        Vertices        vertices = Vertices(instDesc.vertexAddress);                        \
        GPUMaterial     material = Materials(instDesc.materialAddress).m[0];                \
        const ivec3     id       = indices.i[gl_PrimitiveID];                               \
        const GPUVertex v0       = vertices.v[id.x];                                        \
        const GPUVertex v1       = vertices.v[id.y];                                        \
        const GPUVertex v2       = vertices.v[id.z];                                        \
        const vec3      bary     = vec3(1.0 - hitbary.x - hitbary.y, hitbary.x, hitbary.y); \
        const vec2      uv       = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;        \
        const vec3      pos      = v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z;     \
        const vec3      normal =                                                            \
            normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);        \
        const vec3 hit       = gl_ObjectToWorldEXT * vec4(pos, 1.0);                        \
        const vec3 geoNormal = normalize((normal * gl_WorldToObjectEXT).xyz);               \
        payload.hit          = hit;

#define END_TRACE }

#endif