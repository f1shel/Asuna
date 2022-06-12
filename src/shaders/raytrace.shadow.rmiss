#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_debug_printf : require
#extension GL_GOOGLE_include_directive : require

#include "utils/structs.glsl"
#include "utils/math.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadInEXT bool isShadowed;

void main() { isShadowed = false; }
