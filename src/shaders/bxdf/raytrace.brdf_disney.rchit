/* References:
 * [1] [Physically Based Shading at Disney]
 * https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
 * [2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface
 * Scattering]
 * https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
 * [3] [The Disney BRDF Explorer]
 * https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf [4] [Miles
 * Macklin's implementation]
 * https://github.com/mmacklin/tinsel/blob/master/src/disney.h [5] [Simon
 * Kallweit's project report] http://simon-kallweit.me/rendercompo2015/report/
 * [6] [Microfacet Models for Refraction through Rough Surfaces]
 * https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf [7] [Sampling
 * the GGX Distribution of Visible Normals]
 * https://jcgt.org/published/0007/04/01/paper.pdf [8] [Pixar's Foundation for
 * Materials]
 * https://graphics.pixar.com/library/PxrMaterialsCourse2017/paper.pdf
 */

#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

struct DisneyMaterial {
  vec3 baseColor;
  float anisotropic;

  float metallic;
  float roughness;
  float subsurface;
  float specularTint;

  float sheen;
  float sheenTint;
  float clearcoat;
  float clearcoatRoughness;

  float ior;
  float opacity;
  float ax;
  float ay;
};

float Luminance(vec3 c) {
  return 0.212671 * c.x + 0.715160 * c.y + 0.072169 * c.z;
}

float GTR1(float NDotH, float a) {
  if (a >= 1.0) return INV_PI;
  float a2 = a * a;
  float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
  return (a2 - 1.0) / (PI * log(a2) * t);
}

vec3 SampleGTR1(float rgh, float r1, float r2) {
  float a = max(0.001, rgh);
  float a2 = a * a;

  float phi = r1 * TWO_PI;

  float cosTheta = sqrt((1.0 - pow(a2, 1.0 - r1)) / (1.0 - a2));
  float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
  float sinPhi = sin(phi);
  float cosPhi = cos(phi);

  return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

float GTR2(float NDotH, float a) {
  float a2 = a * a;
  float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
  return a2 / (PI * t * t);
}

vec3 SampleGTR2(float rgh, float r1, float r2) {
  float a = max(0.001, rgh);

  float phi = r1 * TWO_PI;

  float cosTheta = sqrt((1.0 - r2) / (1.0 + (a * a - 1.0) * r2));
  float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
  float sinPhi = sin(phi);
  float cosPhi = cos(phi);

  return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

vec3 SampleGGXVNDF(vec3 V, float ax, float ay, float r1, float r2) {
  vec3 Vh = normalize(vec3(ax * V.x, ay * V.y, V.z));

  float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
  vec3 T1 =
      lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
  vec3 T2 = cross(Vh, T1);

  float r = sqrt(r1);
  float phi = 2.0 * PI * r2;
  float t1 = r * cos(phi);
  float t2 = r * sin(phi);
  float s = 0.5 * (1.0 + Vh.z);
  t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

  vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

  return normalize(vec3(ax * Nh.x, ay * Nh.y, max(0.0, Nh.z)));
}

float GTR2Aniso(float NDotH, float HDotX, float HDotY, float ax, float ay) {
  float a = HDotX / ax;
  float b = HDotY / ay;
  float c = a * a + b * b + NDotH * NDotH;
  return 1.0 / (PI * ax * ay * c * c);
}

vec3 SampleGTR2Aniso(float ax, float ay, float r1, float r2) {
  float phi = r1 * TWO_PI;

  float sinPhi = ay * sin(phi);
  float cosPhi = ax * cos(phi);
  float tanTheta = sqrt(r2 / (1 - r2));

  return vec3(tanTheta * cosPhi, tanTheta * sinPhi, 1.0);
}

float SmithG(float NDotV, float alphaG) {
  float a = alphaG * alphaG;
  float b = NDotV * NDotV;
  return (2.0 * NDotV) / (NDotV + sqrt(a + b - a * b));
}

float SmithGAniso(float NDotV, float VDotX, float VDotY, float ax, float ay) {
  float a = VDotX * ax;
  float b = VDotY * ay;
  float c = NDotV;
  return (2.0 * NDotV) / (NDotV + sqrt(a * a + b * b + c * c));
}

float SchlickFresnel(float u) {
  float m = clamp(1.0 - u, 0.0, 1.0);
  float m2 = m * m;
  return m2 * m2 * m;
}

float DielectricFresnel(float cosThetaI, float eta) {
  float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

  // Total internal reflection
  if (sinThetaTSq > 1.0) return 1.0;

  float cosThetaT = sqrt(max(1.0 - sinThetaTSq, 0.0));

  float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
  float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

  return 0.5f * (rs * rs + rp * rp);
}

float DisneyFresnel(float metallic, float eta, float LDotH, float VDotH) {
  float metallicFresnel = SchlickFresnel(LDotH);
  float dielectricFresnel = DielectricFresnel(abs(VDotH), eta);
  return mix(dielectricFresnel, metallicFresnel, metallic);
}

vec3 EvalDiffuse(DisneyMaterial mat, vec3 Csheen, vec3 V, vec3 L, vec3 H,
                 out float pdf) {
  pdf = 0.0;
  if (L.z <= 0.0) return vec3(0.0);

  // Diffuse
  float FL = SchlickFresnel(L.z);
  float FV = SchlickFresnel(V.z);
  float FH = SchlickFresnel(dot(L, H));
  float Fd90 = 0.5 + 2.0 * dot(L, H) * dot(L, H) * mat.roughness;
  float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

  // Fake Subsurface TODO: Replace with volumetric scattering
  float Fss90 = dot(L, H) * dot(L, H) * mat.roughness;
  float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
  float ss = 1.25 * (Fss * (1.0 / (L.z + V.z) - 0.5) + 0.5);

  // Sheen
  vec3 Fsheen = FH * mat.sheen * Csheen;

  pdf = L.z * INV_PI;
  return (1.0 - mat.metallic) *
         (INV_PI * mix(Fd, ss, mat.subsurface) * mat.baseColor + Fsheen);
}

vec3 EvalSpecReflection(DisneyMaterial mat, float eta, vec3 specCol, vec3 V,
                        vec3 L, vec3 H, out float pdf) {
  pdf = 0.0;
  if (L.z <= 0.0) return vec3(0.0);

  float FM = DisneyFresnel(mat.metallic, eta, dot(L, H), dot(V, H));
  vec3 F = mix(specCol, vec3(1.0), FM);
  float D = GTR2Aniso(H.z, H.x, H.y, mat.ax, mat.ay);
  float G1 = SmithGAniso(abs(V.z), V.x, V.y, mat.ax, mat.ay);
  float G2 = G1 * SmithGAniso(abs(L.z), L.x, L.y, mat.ax, mat.ay);

  pdf = G1 * D / (4.0 * V.z);
  return F * D * G2 / (4.0 * L.z * V.z);
}

vec3 EvalClearcoat(DisneyMaterial mat, vec3 V, vec3 L, vec3 H, out float pdf) {
  pdf = 0.0;
  if (L.z <= 0.0) return vec3(0.0);

  float FH = DielectricFresnel(dot(V, H), 1.0 / 1.5);
  float F = mix(0.04, 1.0, FH);
  float D = GTR1(H.z, mat.clearcoatRoughness);
  float G = SmithG(L.z, 0.25) * SmithG(V.z, 0.25);
  float jacobian = 1.0 / (4.0 * dot(V, H));

  pdf = D * H.z * jacobian;
  return vec3(0.25) * mat.clearcoat * F * D * G / (4.0 * L.z * V.z);
}

void GetSpecColor(DisneyMaterial mat, float eta, out vec3 specCol,
                  out vec3 sheenCol) {
  float lum = Luminance(mat.baseColor);
  vec3 ctint = lum > 0.0 ? mat.baseColor / lum : vec3(1.0f);
  float F0 = (1.0 - eta) / (1.0 + eta);
  specCol = mix(F0 * F0 * mix(vec3(1.0), ctint, mat.specularTint),
                mat.baseColor, mat.metallic);
  sheenCol = mix(vec3(1.0), ctint, mat.sheenTint);
}

void GetLobeProbabilities(DisneyMaterial mat, float eta, vec3 specCol,
                          float approxFresnel, out float diffuseWt,
                          out float specReflectWt, out float clearcoatWt) {
  diffuseWt = Luminance(mat.baseColor) * (1.0 - mat.metallic);
  specReflectWt = Luminance(mix(specCol, vec3(1.0), approxFresnel));
  clearcoatWt = 0.25 * mat.clearcoat * (1.0 - mat.metallic);
  float totalWt = diffuseWt + specReflectWt + clearcoatWt;

  diffuseWt /= totalWt;
  specReflectWt /= totalWt;
  clearcoatWt /= totalWt;
}

vec3 eval(vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y, DisneyMaterial mat, float eta,
          uint flags, out float bsdfPdf) {
  vec3 weight = vec3(0);
  bsdfPdf = 0.0;
  if ((flags & EArea) == 0) return weight;

  // NDotL = L.z; NDotV = V.z; NDotH = H.z
  V = toLocal(X, Y, N, V);
  L = toLocal(X, Y, N, L);

  if (L.z <= 0 || V.z <= 0) return weight;

  vec3 H;
  H = normalize(L + V);
  if (H.z < 0.0) H = -H;

  // Specular and sheen color
  vec3 specCol, sheenCol;
  GetSpecColor(mat, eta, specCol, sheenCol);

  // Lobe weights
  float diffuseWt, specReflectWt, clearcoatWt;
  float fresnel = DisneyFresnel(mat.metallic, eta, dot(L, H), dot(V, H));
  GetLobeProbabilities(mat, eta, specCol, fresnel, diffuseWt, specReflectWt,
                       clearcoatWt);

  float pdf = 0.0;

  // Diffuse
  if (diffuseWt > 0.0 && L.z > 0.0) {
    weight += EvalDiffuse(mat, sheenCol, V, L, H, pdf);
    bsdfPdf += pdf * diffuseWt;
  }

  // Specular Reflection
  if (specReflectWt > 0.0 && L.z > 0.0 && V.z > 0.0) {
    weight += EvalSpecReflection(mat, eta, specCol, V, L, H, pdf);
    bsdfPdf += pdf * specReflectWt;
  }

  // Clearcoat
  if (clearcoatWt > 0.0 && L.z > 0.0 && V.z > 0.0) {
    weight += EvalClearcoat(mat, V, L, H, pdf);
    bsdfPdf += pdf * clearcoatWt;
  }

  return weight * L.z;
}

vec3 sampleBsdf(vec2 u, vec3 V, vec3 N, vec3 X, vec3 Y, DisneyMaterial mat,
                float eta, out BsdfSamplingRecord bRec) {
  float pdf = 0.0;
  vec3 f = vec3(0.0);

  float r1 = u.x;
  float r2 = u.y;

  V = toLocal(X, Y, N, V);  // NDotL = L.z; NDotV = V.z; NDotH = H.z
  vec3 L;

  // Specular and sheen color
  vec3 specCol, sheenCol;
  GetSpecColor(mat, eta, specCol, sheenCol);

  // Lobe weights
  float diffuseWt, specReflectWt, clearcoatWt;
  // Note: Fresnel is approx and based on N and not H since H isn't available at
  // this stage.
  float approxFresnel = DisneyFresnel(mat.metallic, eta, V.z, V.z);
  GetLobeProbabilities(mat, eta, specCol, approxFresnel, diffuseWt,
                       specReflectWt, clearcoatWt);

  // CDF for picking a lobe
  float cdf[4];
  cdf[0] = diffuseWt;
  cdf[1] = cdf[0] + clearcoatWt;
  cdf[2] = cdf[1] + specReflectWt;

  // Diffuse Reflection Lobe
  if (r1 < cdf[0]) {
    r1 /= cdf[0];
    L = cosineSampleHemisphere(vec2(r1, r2));

    vec3 H = normalize(L + V);

    f = EvalDiffuse(mat, sheenCol, V, L, H, pdf);
    pdf *= diffuseWt;

    bRec.flags = EDiffuseReflection;
  }
  // Clearcoat Lobe
  else if (r1 < cdf[1]) {
    r1 = (r1 - cdf[0]) / (cdf[1] - cdf[0]);

    vec3 H = SampleGTR1(mat.clearcoatRoughness, r1, r2);

    if (H.z < 0.0) H = -H;

    L = normalize(reflect(-V, H));

    f = EvalClearcoat(mat, V, L, H, pdf);
    pdf *= clearcoatWt;

    bRec.flags = EGlossyReflection;
  }
  // Specular Reflection Lobe
  else {
    r1 = (r1 - cdf[1]) / (1.0 - cdf[1]);
    vec3 H = SampleGGXVNDF(V, mat.ax, mat.ay, r1, r2);

    if (H.z < 0.0) H = -H;

    L = normalize(reflect(-V, H));
    f = EvalSpecReflection(mat, eta, specCol, V, L, H, pdf);

    pdf *= specReflectWt;

    bRec.flags = EGlossyReflection;
  }

  bRec.d = toWorld(X, Y, N, L);
  bRec.pdf = pdf;

  return f * abs(dot(N, L));
}

void main() {
  // Get hit record
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

  // Fetch opacity
  float opacity = state.mat.rhoSpec.x;
  if (state.mat.opacityTextureId >= 0)
    opacity = textureEval(state.mat.opacityTextureId, state.uv).r;

  if (rand(payload.pRec.seed) < opacity) {
    payload.pRec.ray.o = offsetPositionAlongNormal(state.pos, -state.ffN);
    payload.pRec.depth--;
    return;
  }

  DisneyMaterial mat;
  {
    float aspect = sqrt(1.0 - state.mat.anisotropic * 0.9);
    mat.ax = max(0.001, state.mat.roughness * state.mat.roughness / aspect);
    mat.ay = max(0.001, state.mat.roughness * state.mat.roughness * aspect);

    mat.baseColor = state.mat.diffuse;
    mat.anisotropic = state.mat.anisotropic;

    mat.metallic = state.mat.metalness;
    mat.roughness = max(state.mat.roughness * state.mat.roughness, 0.001);
    mat.subsurface = state.mat.subsurface;
    mat.specularTint = state.mat.specularTint;

    mat.sheen = state.mat.sheen;
    mat.sheenTint = state.mat.sheenTint;
    mat.clearcoat = state.mat.clearcoat;
    mat.clearcoatRoughness = mix(0.1, 0.001, state.mat.clearcoatGloss);

    mat.ior = state.mat.ior;
    mat.opacity = opacity;
  }

  // Configure information for denoiser
  if (payload.pRec.depth == 1) {
//    payload.mRec.albedo = state.mat.diffuse;
//    payload.mRec.normal = state.ffN;
  }

  float eta = dot(state.V, state.N) > 0.0 ? (1.0 / mat.ior) : mat.ior;

#if USE_MIS
  // Direct light
  {
    // Light and environment contribution
    vec3 Ld = vec3(0);
    bool visible;
    LightSamplingRecord lRec;
    vec3 radiance = sampleLights(state.pos, state.ffN, visible, lRec);

    if (visible) {
      float bsdfPdf;
      vec3 bsdfWeight = eval(lRec.d, state.V, state.ffN, state.X, state.Y, mat,
                             eta, EArea, bsdfPdf);

      // Multi importance sampling
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
                               state.X, state.Y, mat, eta, bRec);

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