#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

void main() {
  // Treat emissive as a light
  payload.pRec.stop = true;

  // Get hit state
  HitState state = getHitState();

  // Fetch textures
  if (state.mat.radianceTextureId >= 0)
    state.mat.radiance =
        state.mat.radianceFactor *
        textureEval(state.mat.radianceTextureId, state.uv).rgb;

  if (pc.ignoreEmissive == 0)
    payload.pRec.radiance += state.mat.radiance * payload.pRec.throughput;
}