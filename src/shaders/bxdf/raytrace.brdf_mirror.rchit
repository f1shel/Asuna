#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 diffuse, uint flag) {
  vec3 weight = vec3(0);

  if ((flag & EDelta) == 0) return weight;
  if (abs(dot(reflect(-V, N), L) - 1) < EPS) weight = diffuse;

  return weight;
}

float pdf(vec3 L, vec3 V, vec3 N, uint flag) {
  float pdf = 0;
  if ((flag & EDelta) == 0) return pdf;
  if (abs(dot(reflect(-V, N), L) - 1) < EPS) pdf = 1;
  return pdf;
}

vec3 sampleBsdf(vec3 V, vec3 N, vec3 diffuse, out BsdfSamplingRecord bRec) {
  vec3 weight = vec3(0);

  bRec.pdf = 1;
  bRec.d = reflect(-V, N);
  bRec.flags = ESpecularReflection;

  weight = diffuse;
  return weight;
}

void main() {
  // Get hit record
  HitState state = getHitState();

  // Fetch textures
  if (state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = textureEval(state.mat.diffuseTextureId, state.uv).rgb;
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.N = toWorld(state.X, state.Y, state.N, n);
    state.N = makeNormal(state.N);

    configureShadingFrame(state);
  }

  // Configure information for denoiser
  if (payload.pRec.depth == 1) {
//    payload.mRec.albedo = state.mat.diffuse;
//    payload.mRec.normal = state.ffN;
  }

#if USE_MIS
  // Direct light
  {
    // Light and environment contribution
    vec3 Ld = vec3(0);
    bool visible;
    LightSamplingRecord lRec;
    vec3 radiance = sampleLights(state.pos, state.ffN, visible, lRec);

    if (visible) {
      vec3 bsdfWeight =
          eval(lRec.d, state.V, state.ffN, state.mat.diffuse, EArea);

      // Multi importance sampling
      float bsdfPdf = pdf(lRec.d, state.V, state.ffN, lRec.flags);
      float misWeight = powerHeuristic(lRec.pdf, bsdfPdf);

      Ld += misWeight * bsdfWeight * radiance * payload.pRec.throughput /
            (lRec.pdf + EPS);
    }

    payload.dRec.radiance = Ld;
    payload.dRec.skip = (!visible);
  }
#endif

  // Sample next ray
  BsdfSamplingRecord bRec;
  vec3 bsdfWeight = sampleBsdf(state.V, state.ffN, state.mat.diffuse, bRec);

  // Reject invalid sample
  if (bRec.pdf <= 0.0 || isBlack(bsdfWeight)) {
    payload.pRec.stop = true;
    return;
  }

  // Next ray
  payload.bRec = bRec;
  payload.pRec.ray =
      Ray(offsetPositionAlongNormal(state.pos, state.ffN), payload.bRec.d);
  payload.pRec.throughput *= bsdfWeight / bRec.pdf;
}