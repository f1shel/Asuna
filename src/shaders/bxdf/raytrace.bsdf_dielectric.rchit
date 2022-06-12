#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

float dielectricFresnel(float cosThetaI, float eta) {
  float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

  // Total internal reflection
  if (sinThetaTSq > 1.0) return 1.0;

  float cosThetaT = sqrt(max(1.0 - sinThetaTSq, 0.0));

  float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
  float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

  return 0.5f * (rs * rs + rp * rp);
}

vec3 eval(vec3 L, vec3 V, vec3 N, float eta, uint flags) {
  vec3 weight = vec3(0);
  if ((flags & EDelta) == 0) return weight;

  float F = dielectricFresnel(abs(dot(V, N)), eta);
  if (dot(L, N) > 0) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) weight += F * vec3(1);
  } else {
    if (abs(dot(refract(-V, N, eta), L) - 1) < EPS)
      weight += (1 - F) * vec3(1) * eta * eta;
  }
  return weight;
}

// Do not mis with area light
float pdf(vec3 L, vec3 V, vec3 N, float eta, uint flags) {
  float pdf = 0.0;
  if ((flags & EDelta) == 0) return pdf;

  float F = dielectricFresnel(abs(dot(V, N)), eta);
  if (dot(L, N) > 0) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) pdf = F;
  } else {
    if (abs(dot(refract(-V, N, eta), L) - 1) < EPS) pdf = 1 - F;
  }
  return pdf;
}

vec3 sampleBsdf(float u, vec3 V, vec3 N, float eta,
                inout BsdfSamplingRecord bRec) {
  vec3 weight = vec3(0);
  float F = dielectricFresnel(abs(dot(V, N)), eta);

  if (u < F) {
    bRec.d = makeNormal(reflect(-V, N));
    bRec.pdf = F;
    bRec.flags = ESpecularReflection;
    weight = F * vec3(1);
  } else {
    bRec.d = makeNormal(refract(-V, N, eta));
    bRec.pdf = 1 - F;
    bRec.flags = ESpecularTransmission;
    weight = (1 - F) * vec3(1) * eta * eta;
  }

  return weight;
}

void main() {
  // Get hit record
  HitState state = getHitState();

  // Fetch textures
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.N = toWorld(state.X, state.Y, state.N, n);
    state.N = makeNormal(state.N);

    configureShadingFrame(state);
  }

  float eta =
      dot(state.V, state.N) > 0.0 ? (1.0 / state.mat.ior) : state.mat.ior;

  // Configure information for denoiser
  if (payload.pRec.depth == 1) {
    payload.mRec.albedo = state.mat.diffuse;
    payload.mRec.normal = state.ffN;
  }

  // Direct light
  {
    // Light and environment contribution
    vec3 Ld = vec3(0);
    bool visible;
    LightSamplingRecord lRec;
    vec3 radiance = sampleLights(state.pos, state.ffN, visible, lRec);

    if (visible) {
      vec3 bsdfWeight = eval(lRec.d, state.V, state.ffN, eta, EArea);

      // Multi importance sampling
      float bsdfPdf = pdf(lRec.d, state.V, state.ffN, eta, lRec.flags);
      float misWeight = powerHeuristic(lRec.pdf, bsdfPdf);

      Ld += misWeight * bsdfWeight * radiance * payload.pRec.throughput /
            (lRec.pdf + EPS);
    }

    payload.dRec.radiance = Ld;
    payload.dRec.skip = (!visible);
  }

  // Sample next ray
  BsdfSamplingRecord bRec;
  vec3 bsdfWeight =
      sampleBsdf(rand(payload.pRec.seed), state.V, state.ffN, eta, bRec);

  // Reject invalid sample
  if (bRec.pdf <= 0.0 || length(bsdfWeight) == 0.0) {
    payload.pRec.stop = true;
    return;
  }

  // Next ray
  payload.bRec = bRec;
  payload.pRec.ray.o = offsetPositionAlongNormal(
      state.pos, sign(dot(bRec.d, state.N)) * state.N);
  payload.pRec.ray.d = bRec.d;
  payload.pRec.throughput *= bsdfWeight / (bRec.pdf + EPS);
}