#version 460
#extension GL_EXT_debug_printf         : require
#extension GL_EXT_ray_tracing          : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout  : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2    : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../../hostdevice/binding.h"
#include "../../hostdevice/vertex.h"
#include "../../hostdevice/scene.h"
#include "common/structs.glsl"

// ray payloads are used to send information between shaders.
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;
// gpu buffers
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; }; // Vertices of an object
layout(buffer_reference, scalar) buffer Indices  { ivec3 i[]; };  // Triangle indices
// descriptor sets
layout(set = 0, binding = eBindingRaytraceTlas) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = eBindingGraphicSceneDesc, scalar) buffer _SceneDesc { MeshDesc m[]; } sceneDesc;
// This will store two of the barycentric coordinates of the intersection
hitAttributeEXT vec2 hit;

void main() {
    // Object data
    MeshDesc  meshDesc = sceneDesc.m[gl_InstanceCustomIndexEXT];
    Indices   indices  = Indices(meshDesc.indexAddress);
    Vertices  vertices = Vertices(meshDesc.vertexAddress);
    // Indices of the triangle
    ivec3 id = indices.i[gl_PrimitiveID];
    // Vertex of the triangle
    const Vertex v0 = vertices.v[id.x];
    const Vertex v1 = vertices.v[id.y];
    const Vertex v2 = vertices.v[id.z];
    // Barycentrics coordinate of the hit position
    const vec3 bary      = vec3(1.0 - hit.x - hit.y, hit.x, hit.y);

    const vec2 uv        = v0.uv*bary.x + v1.uv*bary.y + v2.uv*bary.z;
    const vec3 pos       = v0.pos*bary.x + v1.pos*bary.y + v2.pos*bary.z;
    const vec3 normal    = normalize(v0.normal*bary.x + v1.normal*bary.y + v2.normal*bary.z);
    const vec3 worldPos  = gl_ObjectToWorldEXT * vec4(pos, 1.0);
    const vec3 geoNormal = normalize((normal * gl_WorldToObjectEXT).xyz);

    payload.radiance = geoNormal;
}