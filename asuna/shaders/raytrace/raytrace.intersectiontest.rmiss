#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "common/structs.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
	payload.radiance  = vec3(0.0);
	payload.uv        = vec2(0.0);
	payload.geoNormal = vec3(0.0);
	payload.depth     = 0.0;
}