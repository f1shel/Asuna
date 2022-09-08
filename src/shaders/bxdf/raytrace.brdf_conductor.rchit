#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

// From "PHYSICALLY BASED LIGHTING CALCULATIONS FOR COMPUTER GRAPHICS" by Peter
// Shirley http://www.cs.virginia.edu/~jdl/bib/globillum/shirley_thesis.pdf
float conductorReflectance(float eta, float k, float cosThetaI) {
  float cosThetaISq = cosThetaI * cosThetaI;
  float sinThetaISq = max(1.0f - cosThetaISq, 0.0f);
  float sinThetaIQu = sinThetaISq * sinThetaISq;

  float innerTerm = eta * eta - k * k - sinThetaISq;
  float aSqPlusBSq =
      sqrt(max(innerTerm * innerTerm + 4.0f * eta * eta * k * k, 0.0f));
  float a = sqrt(max((aSqPlusBSq + innerTerm) * 0.5f, 0.0f));

  float Rs = ((aSqPlusBSq + cosThetaISq) - (2.0f * a * cosThetaI)) /
             ((aSqPlusBSq + cosThetaISq) + (2.0f * a * cosThetaI));
  float Rp = ((cosThetaISq * aSqPlusBSq + sinThetaIQu) -
              (2.0f * a * cosThetaI * sinThetaISq)) /
             ((cosThetaISq * aSqPlusBSq + sinThetaIQu) +
              (2.0f * a * cosThetaI * sinThetaISq));

  return 0.5f * (Rs + Rs * Rp);
}

vec3 conductorReflectance3(const vec3 eta, const vec3 k, float cosThetaI) {
  return vec3(conductorReflectance(eta.x, k.x, cosThetaI),
              conductorReflectance(eta.y, k.y, cosThetaI),
              conductorReflectance(eta.z, k.z, cosThetaI));
}

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 kd, vec3 eta, vec3 k, uint flags) {
  vec3 weight = vec3(0);

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (NdotL < 0 || NdotV < 0 || ((flags & EDelta) == 0)) return weight;

  if (abs(dot(reflect(-V, N), L) - 1) < EPS) {
    weight = kd * conductorReflectance3(eta, k, NdotL);
  }

  return weight;
}

float pdf(vec3 L, vec3 V, vec3 N, uint flags) {
  float pdf = 0.0;

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (NdotL < 0 || NdotV < 0 || ((flags & EDelta) == 0)) return pdf;

  if (abs(dot(reflect(-V, N), L) - 1) < EPS) {
    pdf = 1.0;
  }

  return pdf;
}

vec3 sampleBsdf(vec2 u, vec3 V, vec3 N, vec3 kd, vec3 eta, vec3 k,
                inout BsdfSamplingRecord bRec) {
  vec3 weight = vec3(0);

  float NdotV = dot(V, N);
  if (NdotV <= 0) {
    bRec.flags = EBsdfNull;
    bRec.pdf = 0;
    bRec.d = vec3(0);
    return weight;
  }

  bRec.d = reflect(-V, N);
  bRec.pdf = 1.0f;
  bRec.flags = ESpecularReflection;

  weight = kd * conductorReflectance3(eta, k, dot(N, bRec.d));

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

  vec3 eta = state.mat.radiance;
  vec3 k = state.mat.radianceFactor;

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
          eval(lRec.d, state.V, state.ffN, state.mat.diffuse, eta, k, EArea);

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
  vec3 bsdfWeight = sampleBsdf(rand2(payload.pRec.seed), state.V, state.ffN,
                               state.mat.diffuse, eta, k, bRec);

  // Reject invalid sample
  if (bRec.pdf <= 0.0 || length(bsdfWeight) == 0.0) {
    payload.pRec.stop = true;
    return;
  }

  // Next ray
  payload.bRec = bRec;
  payload.pRec.ray =
      Ray(offsetPositionAlongNormal(state.pos, state.ffN), payload.bRec.d);
  payload.pRec.throughput *= bsdfWeight / (bRec.pdf + EPS);
}