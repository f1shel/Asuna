#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

struct RayPayload
{
	vec3  radiance;
	vec2  uv;
	vec3  geoNormal;
	float depth;
};

#endif