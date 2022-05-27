#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

void main()
{
  // Get hit state
  HitState state = getHitState();

  // Hit Light
  if(state.lightId >= 0)
  {
    hitLight(state.lightId, state.hitPos);
    return;
  }

  // Fetch textures
  if(state.mat.emittanceTextureId >= 0)
    state.mat.emittance =
        state.mat.emittanceFactor * texture(textureSamplers[nonuniformEXT(state.mat.emittanceTextureId)], state.uv).rgb;
  
  payload.stop = 1;
  if(pc.ignoreEmissive == 0)
    payload.radiance += state.mat.emittance * payload.throughput;
}