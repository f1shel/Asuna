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

float fresnelDiffuseReflectance(float eta) {
  // Fast mode: the following code approximates the
  // diffuse Frensel reflectance for the eta<1 and
  // eta>1 cases. An evalution of the accuracy led
  // to the following scheme, which cherry-picks
  // fits from two papers where they are best.
  if (eta < 1) {
    // Fit by Egan and Hilgeman (1973). Works
    // reasonably well for "normal" IOR values (<2).
    //
    // Max rel. error in 1.0 - 1.5 : 0.1%
    // Max rel. error in 1.5 - 2   : 0.6%
    // Max rel. error in 2.0 - 5   : 9.5%
    return -1.4399f * (eta * eta) + 0.7099f * eta + 0.6681f + 0.0636f / eta;
  } else {
    // Fit by d'Eon and Irving (2011)
    //
    // Maintains a good accuracy even for
    // unrealistic IOR values.
    //
    // Max rel. error in 1.0 - 2.0   : 0.1%
    // Max rel. error in 2.0 - 10.0  : 0.2%
    float invEta = 1.0f / eta, invEta2 = invEta * invEta,
          invEta3 = invEta2 * invEta, invEta4 = invEta3 * invEta,
          invEta5 = invEta4 * invEta;

    return 0.919317f - 3.4793f * invEta + 6.75335f * invEta2 -
           7.80989f * invEta3 + 4.98554f * invEta4 - 1.36881f * invEta5;
  }
}

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 kd, float eta, uint flags) {
  vec3 weight = vec3(0);

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (NdotL < 0 || NdotV < 0) return weight;

  float Fv = dielectricFresnel(abs(dot(V, N)), eta);
  if ((flags & EDelta) != 0) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) weight += Fv * vec3(1);
  } else if ((flags & EArea) != 0) {
    float Fl = dielectricFresnel(abs(NdotL), eta);
    vec3 diff = kd * INV_PI * abs(NdotL);
//    float fdrInt = fresnelDiffuseReflectance(1 / eta);
//    diff /= (1 - diff * fdrInt);

    weight += (1 - Fv) * (1 - Fl) * diff * (eta * eta);
  }
  return weight;
}

float pdf(vec3 L, vec3 V, vec3 N, float eta, uint flags) {
  float pdf = 0.0;

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (NdotL < 0 || NdotV < 0) return pdf;

  float Fv = dielectricFresnel(abs(dot(V, N)), eta);
  if ((flags & EDelta) != 0) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) pdf = Fv;
  } else if ((flags & EArea) != 0) {
    pdf = (1 - Fv) * cosineHemispherePdf(NdotL);
  }
  return pdf;
}

vec3 sampleBsdf(vec2 u, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 kd, float eta,
                inout BsdfSamplingRecord bRec) {
  vec3 weight = vec3(0);
  float Fv = dielectricFresnel(abs(dot(V, N)), eta);

  if (u.x < Fv) {
    bRec.d = makeNormal(reflect(-V, N));
    bRec.pdf = Fv;
    bRec.flags = ESpecularReflection;
    weight = vec3(1) / Fv;
  } else {
    u.x = min((u.x - Fv) / (1 - Fv + EPS), 1);
    vec3 wi = cosineSampleHemisphere(u);
    float Fl = dielectricFresnel(abs(wi.z), eta);
    bRec.d = toWorld(X, Y, N, wi);
    bRec.pdf = (1 - Fv) * cosineHemispherePdf(wi.z);
    bRec.flags = EDiffuseReflection;
    vec3 diff = kd * INV_PI * abs(wi.z);
//    float fdrInt = fresnelDiffuseReflectance(1 / eta);
//    diff /= (1 - diff * fdrInt);

    weight = (1 - Fv) * (1 - Fl) * eta * eta * diff / (1 - Fv);
  }

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

  float eta = 1 / state.mat.ior;

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
      vec3 bsdfWeight =
          eval(lRec.d, state.V, state.ffN, state.mat.diffuse, eta, EArea);

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
  vec3 bsdfWeight = sampleBsdf(rand2(payload.pRec.seed), state.V, state.ffN,
                               state.X, state.Y, state.mat.diffuse, eta, bRec);

  // Reject invalid sample
  if (bRec.pdf <= 0.0 || length(bsdfWeight) == 0.0) {
    payload.pRec.stop = true;
    return;
  }

  // Next ray
  payload.bRec = bRec;
  payload.pRec.ray.o = payload.dRec.ray.o;
  payload.pRec.ray.d = bRec.d;
  payload.pRec.throughput *= bsdfWeight / (bRec.pdf + EPS);
}