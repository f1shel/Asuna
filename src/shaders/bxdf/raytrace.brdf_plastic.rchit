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

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 kd, float eta, float fdrInt,
          float invEta2, uint flags) {
  vec3 weight = vec3(0);

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (NdotL < 0 || NdotV < 0) return weight;

  float _, Fo = fresnelDielectricExt(NdotV, _, eta);
  bool hasSpecular = ((flags & EDelta) != 0);
  bool hasDiffuse = ((flags & EArea) != 0);
  if (hasSpecular) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) {
      weight = Fo * vec3(1);
    }
  } else if (hasDiffuse) {
    float _, Fi = fresnelDielectricExt(NdotL, _, eta);
    vec3 diff = kd * INV_PI * NdotL;
    diff /= (1 - diff * fdrInt);

    weight = (1 - Fi) * (1 - Fo) * diff * invEta2;
  }
  return weight;
}

float pdf(vec3 L, vec3 V, vec3 N, float eta, float specularSamplingWeight,
          uint flags) {
  float pdf = 0.0;

  float NdotL = dot(L, N);
  float NdotV = dot(V, N);
  if (NdotL < 0 || NdotV < 0) return pdf;

  float _, Fo = fresnelDielectricExt(NdotV, _, eta);
  bool hasSpecular = ((flags & EDelta) != 0);
  bool hasDiffuse = ((flags & EArea) != 0);
  float probSpecular =
      (Fo * specularSamplingWeight) /
      (Fo * specularSamplingWeight + (1 - Fo) * (1 - specularSamplingWeight));

  if (hasSpecular) {
    if (abs(dot(reflect(-V, N), L) - 1) < EPS) {
      pdf = probSpecular;
    }
  } else if (hasDiffuse) {
    pdf = NdotL * (1 - probSpecular);
  }

  return pdf;
}

vec3 sampleBsdf(vec2 u, vec3 V, vec3 N, vec3 X, vec3 Y, vec3 kd, float eta,
                float fdrInt, float invEta2, float specularSamplingWeight,
                inout BsdfSamplingRecord bRec) {
  vec3 weight = vec3(0);

  float NdotV = dot(V, N);
  if (NdotV <= 0) {
    bRec.flags = EBsdfNull;
    bRec.pdf = 0;
    bRec.d = vec3(0);
    return weight;
  }

  float _, Fo = fresnelDielectricExt(NdotV, _, eta);
  float probSpecular =
      (Fo * specularSamplingWeight) /
      (Fo * specularSamplingWeight + (1 - Fo) * (1 - specularSamplingWeight));

  if (u.x < probSpecular) {
    bRec.d = makeNormal(reflect(-V, N));
    bRec.flags = ESpecularReflection;
    bRec.pdf = probSpecular;
    weight = vec3(1) * Fo;
  } else {
    u.x = (u.x - probSpecular) / (1 + EPS - probSpecular);
    vec3 wi = cosineSampleHemisphere(u);
    float _, Fi = fresnelDielectricExt(wi.z, _, eta);
    vec3 diff = kd * INV_PI * abs(wi.z);
    diff /= (1 - diff * fdrInt);
    bRec.pdf = (1 - probSpecular) * cosineHemispherePdf(wi.z);
    bRec.d = toWorld(X, Y, N, wi);
    bRec.flags = EDiffuseReflection;

    weight = (1 - Fi) * (1 - Fo) * invEta2 * diff;
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

  float eta = state.mat.ior;
  float fdrInt = state.mat.radiance.x;
  float dAvg = luminance(state.mat.diffuse), sAvg = luminance(vec3(1));
  float specularSamplingWeight = sAvg / (dAvg + sAvg);
  float invEta2 = 1 / (eta * eta);

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
      vec3 bsdfWeight = eval(lRec.d, state.V, state.ffN, state.mat.diffuse, eta,
                             fdrInt, invEta2, EArea);

      // Multi importance sampling
      float bsdfPdf = pdf(lRec.d, state.V, state.ffN, eta,
                          specularSamplingWeight, lRec.flags);
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
                               state.X, state.Y, state.mat.diffuse, eta, fdrInt,
                               invEta2, specularSamplingWeight, bRec);

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