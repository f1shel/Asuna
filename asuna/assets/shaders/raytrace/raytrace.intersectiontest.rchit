#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../../hostdevice/binding.h"
#include "common/structs.glsl"

// ray payloads are used to send information between shaders.
layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

void main() {
	payload.radiance = vec3(1.0,0.0,0.0);
}