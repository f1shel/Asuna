#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../../hostdevice/binding.h"
#include "../../hostdevice/material.h"
#include "../../hostdevice/scene.h"
#include "../../hostdevice/vertex.h"
#include "common/structs.glsl"

// ray payloads are used to send information between shaders.
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;
// gpu buffers
layout(buffer_reference, scalar) buffer Vertices
{
	GPUVertex v[];
};        // Vertices of an object
layout(buffer_reference, scalar) buffer Indices
{
	ivec3 i[];
};        // Triangle indices
layout(buffer_reference, scalar) buffer Materials
{
	GPUMaterial m[];
};        // Array of all materials on an object
// descriptor sets
layout(set     = eGPUSetRaytraceRaytrace,
       binding = eGPUBindingRaytraceTlas) uniform accelerationStructureEXT topLevelAS;
layout(set     = eGPUSetRaytraceGraphics,
       binding = eGPUBindingGraphicsTextures) uniform sampler2D textureSamplers[];
layout(set = eGPUSetRaytraceGraphics, binding = eGPUBindingGraphicsSceneDesc,
       scalar) buffer _SceneDesc
{
	GPUInstanceDesc m[];
}
sceneDesc;
// This will store two of the barycentric coordinates of the intersection
hitAttributeEXT vec2 hit;

vec4 textureEval(in int texId, in vec2 uv)
{
	return texture(textureSamplers[nonuniformEXT(texId)], uv).rgba;
}

void main()
{
	// Object data
	GPUInstanceDesc instDesc = sceneDesc.m[gl_InstanceCustomIndexEXT];
	Indices         indices  = Indices(instDesc.indexAddress);
	Vertices        vertices = Vertices(instDesc.vertexAddress);
	GPUMaterial     material = Materials(instDesc.materialAddress).m[0];
	// Indices of the triangle
	ivec3 id = indices.i[gl_PrimitiveID];
	// Vertex of the triangle
	const GPUVertex v0 = vertices.v[id.x];
	const GPUVertex v1 = vertices.v[id.y];
	const GPUVertex v2 = vertices.v[id.z];
	// Barycentrics coordinate of the hit position
	const vec3 bary = vec3(1.0 - hit.x - hit.y, hit.x, hit.y);

	const vec2 uv     = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;
	const vec3 pos    = v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z;
	const vec3 normal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
	const vec3 worldPos  = gl_ObjectToWorldEXT * vec4(pos, 1.0);
	const vec3 geoNormal = normalize((normal * gl_WorldToObjectEXT).xyz);

	payload.radiance  = textureEval(material.normalTextureId, uv).rgb;
	payload.uv        = uv;
	payload.geoNormal = geoNormal;
	payload.depth     = gl_HitTEXT;
}