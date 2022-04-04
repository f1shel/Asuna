#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "../../hostdevice/pushconstant.h"

layout(push_constant) uniform _PushConstantGraphic
{
    PushConstantGraphic pcGraphic;
};

void main()
{
  
}
