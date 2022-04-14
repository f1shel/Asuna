#version 450

#extension GL_KHR_vulkan_glsl: enable

layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 o_color;

layout(set = 0, binding = 0) uniform sampler2D g_image;

void main()
{
    vec3 R = texture(g_image, i_uv).xyz;
    o_color = vec4(R,1.0);
}
