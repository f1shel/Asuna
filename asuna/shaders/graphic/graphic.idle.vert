#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../../hostdevice/camera.h"
#include "../../hostdevice/pushconstant.h"

layout(binding = 0) uniform _Camera
{
    Camera cam;
};

layout(push_constant) uniform _PushConstantGraphic
{
    PushConstantGraphic pcGraphic;
};

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec2 i_texCoord;
layout(location = 2) in vec3 i_normal;

void main()
{
}
