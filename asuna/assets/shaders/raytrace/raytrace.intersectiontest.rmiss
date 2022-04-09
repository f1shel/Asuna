#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "common/structs.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
	payload.radiance = vec3(0.0,1.0,0.0);
}