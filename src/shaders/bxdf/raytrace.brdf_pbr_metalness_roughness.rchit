#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

float sqr(float x) { return x * x; }

vec3 fresnelSchlick(float cosThetaI, vec3 f0) {
  return f0 + (1 - f0) * pow(clamp(1 - cosThetaI, 0, 1), 5);
}

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

const float DIFFUSE_LOBE_PROBABILITY = 0.2;

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 albedo, float ax,
          float ay, float metalness, float eta, uint flags) {
  vec3 weight = vec3(0);
  if ((flags & EArea) == 0) return weight;

  float NdotL = dot(N, L);
  float NdotV = dot(N, V);
  if (NdotL <= 0.0 || NdotV <= 0.0) return weight;

  vec3 H = makeNormal(L + V);

  float HdotN = dot(H, N);
  float HdotX = dot(H, X);
  float HdotY = dot(H, Y);
  float HdotL = dot(H, L);
  float HdotV = dot(H, V);

  float F0 = sqr((eta - 1) / (eta + 1));
  vec3 Fs = fresnelSchlick(HdotV, mix(vec3(F0), albedo, metalness));
  float Ds = dAnisoGGX(HdotN, HdotX, HdotY, ax, ay);
  float Gs = g1SmithAnisoGGX(NdotV, dot(V, X), dot(V, Y), ax, ay);
  Gs *= g1SmithAnisoGGX(NdotL, dot(L, X), dot(L, Y), ax, ay);
  vec3 diffuse = albedo * (1 - metalness) * INV_PI;
  vec3 specular = Fs * Ds * Gs;

  weight += diffuse;
  weight += specular;
  weight *= NdotL;
  return weight;
}

float pdf(vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y, float ax, float ay,
          uint flags) {
  float pdf = 0.0;
  if ((flags & EArea) == 0) return pdf;

  float NdotL = dot(N, L);
  float NdotV = dot(N, V);
  if (NdotL <= 0.0 || NdotV <= 0.0) return pdf;

  vec3 H = makeNormal(L + V);
  vec3 wh = makeNormal(toLocal(X, Y, N, H));
  vec3 wo = makeNormal(toLocal(X, Y, N, V));
  vec3 wi = makeNormal(toLocal(X, Y, N, L));

  pdf += DIFFUSE_LOBE_PROBABILITY * cosineHemispherePdf(wi.z);
  pdf += (1 - DIFFUSE_LOBE_PROBABILITY) * importanceAnisoGGXPdf(wh, wo, ax, ay);
  return pdf;
}

vec3 sampleBsdf(vec2 u, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 albedo, float ax,
                float ay, float metalness, float eta,
                out BsdfSamplingRecord bRec) {
  vec3 wo = makeNormal(toLocal(X, Y, N, V));

  vec3 wi;
  bRec.pdf = 0.0;
  if (rand(payload.pRec.seed) < DIFFUSE_LOBE_PROBABILITY) {
    wi = cosineSampleHemisphere(u);
    bRec.flags = EDiffuseReflection;
    bRec.pdf = cosineHemispherePdf(wi.z);
  } else {
    wi = importanceSampleAnisoGGX(u, wo, ax, ay);
    vec3 wh = makeNormal(wi + wo);
    bRec.pdf = importanceAnisoGGXPdf(wh, wo, ax, ay);
    bRec.flags = EGlossyReflection;
  }
  bRec.d = toWorld(X, Y, N, wi);
  vec3 weight =
      eval(bRec.d, V, N, X, Y, albedo, ax, ay, metalness, eta, EArea) *
      abs(wi.z);
  return weight;
}

void main() {
  // Get hit state
  HitState state = getHitState();

  // Fetch textures
  if (state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = textureEval(state.mat.diffuseTextureId, state.uv).rgb;
  if (state.mat.metalnessTextureId >= 0)
    state.mat.metalness = textureEval(state.mat.metalnessTextureId, state.uv).r;
  if (state.mat.roughnessTextureId >= 0)
    state.mat.roughness = textureEval(state.mat.roughnessTextureId, state.uv).r;
  if (state.mat.normalTextureId >= 0) {
    vec3 c = textureEval(state.mat.normalTextureId, state.uv).rgb;
    vec3 n = 2 * c - 1;
    state.N = toWorld(state.X, state.Y, state.N, n);
    state.N = makeNormal(state.N);

    configureShadingFrame(state);
  }
  float ax = max(sqr(state.mat.roughness), 0.001);
  // Isotropic
  float ay = ax;
  // eta = ior since there is no refraction
  float eta = state.mat.ior;

  // Configure information for denoiser
  if (payload.pRec.depth == 1) {
    payload.mRec.albedo = state.mat.diffuse;
    payload.mRec.normal = state.ffN;
  }

#if USE_MIS
  // Direct light
  {
    vec3 Ld = vec3(0);
    bool visible;
    LightSamplingRecord lRec;
    vec3 radiance = sampleLights(state.pos, state.ffN, visible, lRec);

    if (visible) {
      vec3 bsdfWeight =
          eval(lRec.d, state.V, state.ffN, state.X, state.Y, state.mat.diffuse,
               ax, ay, state.mat.metalness, eta, lRec.flags);
      // Multi importance sampling
      float bsdfPdf =
          pdf(lRec.d, state.V, state.ffN, state.X, state.Y, ax, ay, lRec.flags);

      float misWeight = powerHeuristic(lRec.pdf, bsdfPdf);

      Ld = misWeight * bsdfWeight * radiance * payload.pRec.throughput /
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
                 state.mat.diffuse, ax, ay, state.mat.metalness, eta, bRec);

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