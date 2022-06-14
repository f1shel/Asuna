#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

float fresnelDielectricExt(float cosThetaI_, inout float cosThetaT_,
                           float eta) {
  if (eta == 1) {
    cosThetaT_ = -cosThetaI_;
    return 0.0f;
  }

  // Using Snell's law, calculate the squared sine of the
  // angle between the normal and the transmitted ray
  float scale = (cosThetaI_ > 0) ? 1 / eta : eta,
        cosThetaTSqr = 1 - (1 - cosThetaI_ * cosThetaI_) * (scale * scale);

  // Check for total internal reflection
  if (cosThetaTSqr <= 0.0f) {
    cosThetaT_ = 0.0f;
    return 1.0f;
  }

  // Find the absolute cosines of the incident/transmitted rays
  float cosThetaI = abs(cosThetaI_);
  float cosThetaT = sqrt(cosThetaTSqr);

  float Rs = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
  float Rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

  cosThetaT_ = (cosThetaI_ > 0) ? -cosThetaT : cosThetaT;

  // No polarization -- return the unpolarized reflectance
  return 0.5f * (Rs * Rs + Rp * Rp);
}

float sqr(float x) { return x * x; }

float dAnisoGGX(float HdotN, float HdotX, float HdotY, float ax, float ay) {
  return 1 /
         // ---------------------------------------------------------
         (PI * ax * ay * sqr(sqr(HdotX / ax) + sqr(HdotY / ay) + sqr(HdotN)) +
          EPS);
}

vec3 importanceSampleAnisoGGX(vec2 u, vec3 wo, float ax, float ay) {
  float u1 = u.x;
  float u2 = u.y;
  float factor = safeSqrt(u1 / max(1 - u1, EPS));
  float phi = TWO_PI * u2;

  vec3 wh = vec3(0, 0, 1);
  wh.x = -ax * factor * cos(phi);
  wh.y = -ay * factor * sin(phi);
  wh = makeNormal(wh);

  vec3 wi = reflect(-wo, wh);
  return wi;
}

float importanceAnisoGGXPdf(in vec3 wh, in vec3 wo, in float ax, in float ay) {
  float pdf = 0.0, HdotV = dot(wh, wo);
  vec3 wi = reflect(-wo, wh);
  if (wi.z > 0.0 && wo.z > 0.0 && wh.z > 0.0)
    pdf = dAnisoGGX(wh.z, wh.x, wh.y, ax, ay) * abs(wh.z) / (4 * HdotV + EPS);
  return pdf;
}

float g1SmithAnisoGGX(float NdotV, float VdotX, float VdotY, float ax,
                      float ay) {
  if (NdotV <= 0.0) return 0.0;
  vec3 factor = vec3(ax * VdotX, ay * VdotY, NdotV);
  return 1 / (NdotV + length(factor));
}

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 kd, vec3 ks, float eta,
          float fdrInt, float invEta2, float ax, float ay, uint flags) {
  vec3 weight = vec3(0);

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (((flags & EArea) == 0) || NdotL < 0 || NdotV < 0) return weight;

  // Specular
  {
    vec3 H = makeNormal(L + V);
    float _, Fs = fresnelDielectricExt(dot(H, V), _, eta);
    float Gs = g1SmithAnisoGGX(NdotV, dot(V, X), dot(V, Y), ax, ay);
    Gs *= g1SmithAnisoGGX(NdotL, dot(L, X), dot(L, Y), ax, ay);
    float Ds = dAnisoGGX(dot(H, N), dot(H, X), dot(H, Y), ax, ay);

    weight += ks * Fs * Gs * Ds * NdotL;
  }
  // Diffuse
  {
    float _1, Fo = fresnelDielectricExt(NdotV, _1, eta);
    float _2, Fi = fresnelDielectricExt(NdotL, _2, eta);
    vec3 diff = kd;
    diff /= (1 - diff * fdrInt);

    weight += (1 - Fi) * (1 - Fo) * diff * invEta2 * INV_PI * NdotL;
  }

  return weight;
}

float pdf(vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y, float ax, float ay, float eta,
          float substrateSamplingWeight, uint flags) {
  float pdf = 0.0;

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (NdotL < 0 || NdotV < 0 || ((flags & EArea) == 0)) return pdf;

  float _, Fo = fresnelDielectricExt(NdotV, _, eta);
  float substrateWeight = substrateSamplingWeight * (1.0 - Fo);
  float specularWeight = Fo;
  float probSpecular = specularWeight / (specularWeight + substrateWeight);

  vec3 wi = toLocal(X, Y, N, L);
  vec3 wo = toLocal(X, Y, N, V);
  vec3 wh = makeNormal(wi + wo);
  float glossyPdf = importanceAnisoGGXPdf(wh, wo, ax, ay);
  pdf += probSpecular * glossyPdf;

  float diffusePdf = cosineHemispherePdf(wi.z);
  pdf += (1 - probSpecular) * diffusePdf;

  return pdf;
}

vec3 sampleBsdf(vec2 u, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 kd, vec3 ks,
                float eta, float fdrInt, float invEta2, float ax, float ay,
                float substrateSamplingWeight, inout BsdfSamplingRecord bRec) {
  vec3 weight = vec3(0);

  float NdotV = dot(V, N);
  if (NdotV <= 0) {
    bRec.flags = EBsdfNull;
    bRec.pdf = 0;
    bRec.d = vec3(0);
    return weight;
  }

  float _, Fo = fresnelDielectricExt(NdotV, _, eta);
  float substrateWeight = substrateSamplingWeight * (1.0 - Fo);
  float specularWeight = Fo;
  float probSpecular = specularWeight / (specularWeight + substrateWeight);

  if (u.x < probSpecular) {
    u.x = u.x / probSpecular;

    vec3 wo = toLocal(X, Y, N, V);
    vec3 wi = importanceSampleAnisoGGX(u, wo, ax, ay);
    vec3 L = bRec.d = toWorld(X, Y, N, wi);
    vec3 H = makeNormal(V + L);
    vec3 wh = makeNormal(wi + wo);

    float NdotL = dot(N, L);
    float _, Fs = fresnelDielectricExt(dot(H, V), _, eta);
    float Gs = g1SmithAnisoGGX(NdotV, dot(V, X), dot(V, Y), ax, ay);
    Gs *= g1SmithAnisoGGX(NdotL, dot(L, X), dot(L, Y), ax, ay);
    float Ds = dAnisoGGX(dot(H, N), dot(H, X), dot(H, Y), ax, ay);

    bRec.flags = EGlossyReflection;
    bRec.pdf = importanceAnisoGGXPdf(wh, wo, ax, ay) * probSpecular;

    weight = ks * Fs * Gs * Ds * NdotL;
  } else {
    u.x = (u.x - probSpecular) / (1 + EPS - probSpecular);
    vec3 wi = cosineSampleHemisphere(u);
    float _, Fi = fresnelDielectricExt(wi.z, _, eta);
    vec3 diff = kd;
    diff /= (1 - diff * fdrInt);
    bRec.pdf = (1 - probSpecular) * cosineHemispherePdf(wi.z);
    bRec.d = toWorld(X, Y, N, wi);
    bRec.flags = EDiffuseReflection;

    weight = (1 - Fi) * (1 - Fo) * invEta2 * diff * INV_PI * abs(wi.z);
  }

  return weight;
}

void main() {
  // Get hit record
  HitState state = getHitState();

  // Fetch textures
  if (state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = textureEval(state.mat.diffuseTextureId, state.uv).rgb;
  if (state.mat.roughnessTextureId >= 0)
    state.mat.anisoAlpha =
        textureEval(state.mat.roughnessTextureId, state.uv).rg;
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.N = toWorld(state.X, state.Y, state.N, n);
    state.N = makeNormal(state.N);

    configureShadingFrame(state);
  }

  float eta = state.mat.ior;
  float fdrInt = state.mat.radiance.x;
  float invEta2 = 1 / (eta * eta);
  float ax = max(EPS, state.mat.anisoAlpha.x);
  float ay = max(EPS, state.mat.anisoAlpha.y);
  float substrateSamplingWeight = luminance(state.mat.diffuse);
  const vec3 ks = vec3(1);

  // Configure information for denoiser
  if (payload.pRec.depth == 1) {
    payload.mRec.albedo = state.mat.diffuse;
    payload.mRec.normal = state.ffN;
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
          eval(lRec.d, state.V, state.ffN, state.X, state.Y, state.mat.diffuse,
               ks, eta, fdrInt, ax, ay, invEta2, EArea);

      // Multi importance sampling
      float bsdfPdf = pdf(lRec.d, state.V, state.ffN, state.X, state.Y, eta, ax,
                          ay, substrateSamplingWeight, lRec.flags);
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
  vec3 bsdfWeight =
      sampleBsdf(rand2(payload.pRec.seed), state.V, state.ffN, state.X, state.Y,
                 state.mat.diffuse, ks, eta, fdrInt, invEta2, ax, ay,
                 substrateSamplingWeight, bRec);

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