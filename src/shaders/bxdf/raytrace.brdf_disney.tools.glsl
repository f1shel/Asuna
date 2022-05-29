#ifndef RT_BRDF_DISNEY_TOOLS_GLSL
#define RT_BRDF_DISNEY_TOOLS_GLSL

/* References:
 * [1] [Physically Based Shading at Disney] https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
 * [2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering] https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
 * [3] [The Disney BRDF Explorer] https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf
 * [4] [Miles Macklin's implementation] https://github.com/mmacklin/tinsel/blob/master/src/disney.h
 * [5] [Simon Kallweit's project report] http://simon-kallweit.me/rendercompo2015/report/
 * [6] [Microfacet Models for Refraction through Rough Surfaces] https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
 * [7] [Sampling the GGX Distribution of Visible Normals] https://jcgt.org/published/0007/04/01/paper.pdf
 * [8] [Pixar's Foundation for Materials] https://graphics.pixar.com/library/PxrMaterialsCourse2017/paper.pdf
 */

#include "../utils/math.glsl"
#include "../../shared/material.h"

float Luminance(vec3 c)
{
  return 0.212671 * c.x + 0.715160 * c.y + 0.072169 * c.z;
}

float GTR1(float NDotH, float a)
{
  if(a >= 1.0)
    return INV_PI;
  float a2 = a * a;
  float t  = 1.0 + (a2 - 1.0) * NDotH * NDotH;
  return (a2 - 1.0) / (PI * log(a2) * t);
}

vec3 SampleGTR1(float rgh, float r1, float r2)
{
  float a  = max(0.001, rgh);
  float a2 = a * a;

  float phi = r1 * TWO_PI;

  float cosTheta = sqrt((1.0 - pow(a2, 1.0 - r1)) / (1.0 - a2));
  float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
  float sinPhi   = sin(phi);
  float cosPhi   = cos(phi);

  return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

float GTR2(float NDotH, float a)
{
  float a2 = a * a;
  float t  = 1.0 + (a2 - 1.0) * NDotH * NDotH;
  return a2 / (PI * t * t);
}

vec3 SampleGTR2(float rgh, float r1, float r2)
{
  float a = max(0.001, rgh);

  float phi = r1 * TWO_PI;

  float cosTheta = sqrt((1.0 - r2) / (1.0 + (a * a - 1.0) * r2));
  float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
  float sinPhi   = sin(phi);
  float cosPhi   = cos(phi);

  return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

vec3 SampleGGXVNDF(vec3 V, float rgh, float r1, float r2)
{
  vec3 Vh = normalize(vec3(rgh * V.x, rgh * V.y, V.z));

  float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
  vec3  T1    = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
  vec3  T2    = cross(Vh, T1);

  float r   = sqrt(r1);
  float phi = 2.0 * PI * r2;
  float t1  = r * cos(phi);
  float t2  = r * sin(phi);
  float s   = 0.5 * (1.0 + Vh.z);
  t2        = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

  vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

  return normalize(vec3(rgh * Nh.x, rgh * Nh.y, max(0.0, Nh.z)));
}

float GTR2Aniso(float NDotH, float HDotX, float HDotY, float ax, float ay)
{
  float a = HDotX / ax;
  float b = HDotY / ay;
  float c = a * a + b * b + NDotH * NDotH;
  return 1.0 / (PI * ax * ay * c * c);
}

vec3 SampleGTR2Aniso(float ax, float ay, float r1, float r2)
{
  float phi = r1 * TWO_PI;

  float sinPhi   = ay * sin(phi);
  float cosPhi   = ax * cos(phi);
  float tanTheta = sqrt(r2 / (1 - r2));

  return vec3(tanTheta * cosPhi, tanTheta * sinPhi, 1.0);
}

float SmithG(float NDotV, float alphaG)
{
  float a = alphaG * alphaG;
  float b = NDotV * NDotV;
  return (2.0 * NDotV) / (NDotV + sqrt(a + b - a * b));
}

float SmithGAniso(float NDotV, float VDotX, float VDotY, float ax, float ay)
{
  float a = VDotX * ax;
  float b = VDotY * ay;
  float c = NDotV;
  return 1.0 / (NDotV + sqrt(a * a + b * b + c * c));
}

float SchlickFresnel(float u)
{
  float m  = clamp(1.0 - u, 0.0, 1.0);
  float m2 = m * m;
  return m2 * m2 * m;
}

float DielectricFresnel(float cosThetaI, float eta)
{
  float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

  // Total internal reflection
  if(sinThetaTSq > 1.0)
    return 1.0;

  return sinThetaTSq;

  float cosThetaT = sqrt(max(1.0 - sinThetaTSq, 0.0));

  float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
  float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

  return 0.5f * (rs * rs + rp * rp);
}

vec3 ToWorld(vec3 X, vec3 Y, vec3 Z, vec3 V)
{
  return V.x * X + V.y * Y + V.z * Z;
}

vec3 ToLocal(vec3 X, vec3 Y, vec3 Z, vec3 V)
{
  return vec3(dot(V, X), dot(V, Y), dot(V, Z));
}

float DisneyFresnel(float metallic, float eta, float LDotH, float VDotH)
{
  float metallicFresnel   = SchlickFresnel(LDotH);
  float dielectricFresnel = DielectricFresnel(abs(VDotH), eta);
  return mix(dielectricFresnel, metallicFresnel, metallic);
}

vec3 EvalDiffuse(float roughness, float metallic, float subsurface, float sheen, vec3 baseColor, vec3 Csheen, vec3 V, vec3 L, vec3 H, out float pdf)
{
  pdf = 0.0;
  if(L.z <= 0.0)
    return vec3(0.0);

  // Diffuse
  float FL   = SchlickFresnel(L.z);
  float FV   = SchlickFresnel(V.z);
  float FH   = SchlickFresnel(dot(L, H));
  float Fd90 = 0.5 + 2.0 * dot(L, H) * dot(L, H) * roughness;
  float Fd   = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

  // Fake Subsurface TODO: Replace with volumetric scattering
  float Fss90 = dot(L, H) * dot(L, H) * roughness;
  float Fss   = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
  float ss    = 1.25 * (Fss * (1.0 / (L.z + V.z) - 0.5) + 0.5);

  // Sheen
  vec3 Fsheen = FH * sheen * Csheen;

  pdf = L.z * INV_PI;
  return (1.0 - metallic) * (INV_PI * mix(Fd, ss, subsurface) * baseColor + Fsheen);
}

vec3 EvalSpecReflection(float metallic, float roughness, float eta, vec3 specCol, vec3 V, vec3 L, vec3 H, out float pdf)
{
  pdf = 0.0;
  if(L.z <= 0.0)
    return vec3(0.0);

  float FM = DisneyFresnel(metallic, eta, dot(L, H), dot(V, H));
  vec3  F  = mix(specCol, vec3(1.0), FM);
  float D  = GTR2(H.z, roughness);
  float G1 = SmithG(abs(V.z), roughness);
  float G2 = G1 * SmithG(abs(L.z), roughness);

  pdf = G1 * D / (4.0 * V.z);
  return G2 * D * F / (4.0 * L.z * V.z);
}

vec3 EvalClearcoat(float clearcoatRoughness, float clearcoat, vec3 V, vec3 L, vec3 H, out float pdf)
{
  pdf = 0.0;
  if(L.z <= 0.0)
    return vec3(0.0);

  float FH       = DielectricFresnel(dot(V, H), 1.0 / 1.5);
  float F        = mix(0.04, 1.0, FH);
  float D        = GTR1(H.z, clearcoatRoughness);
  float G        = SmithG(L.z, 0.25) * SmithG(V.z, 0.25);
  float jacobian = 1.0 / (4.0 * dot(V, H));

  pdf = D * H.z * jacobian;
  return vec3(0.25) * clearcoat * F * D * G / (4.0 * L.z * V.z);
}

void GetSpecColor(vec3 baseColor, float metallic, float specularTint, float sheenTint, float eta, out vec3 specCol, out vec3 sheenCol)
{
  float lum   = Luminance(baseColor);
  vec3  ctint = lum > 0.0 ? baseColor / lum : vec3(1.0f);
  float F0    = (1.0 - eta) / (1.0 + eta);
  specCol     = mix(F0 * F0 * mix(vec3(1.0), ctint, specularTint), baseColor, metallic);
  sheenCol    = mix(vec3(1.0), ctint, sheenTint);
}

void GetLobeProbabilities(float metallic, float clearcoat, vec3 baseColor, vec3 specCol, float approxFresnel, out float diffuseWt, out float specReflectWt, out float clearcoatWt)
{
    diffuseWt = Luminance(baseColor) * (1.0 - metallic);
    specReflectWt = Luminance(mix(specCol, vec3(1.0), approxFresnel));
    clearcoatWt = 0.25 * clearcoat * (1.0 - metallic);
    float totalWt = diffuseWt + specReflectWt + clearcoatWt;

    diffuseWt /= totalWt;
    specReflectWt /= totalWt;
    clearcoatWt /= totalWt;
}

vec3 DisneySample(in GpuMaterial mat, inout uint seed, vec3 V, vec3 N, out vec3 L, out float pdf)
{
  pdf    = 0.0;
  vec3 f = vec3(0.0);

  float r1 = rand(seed);
  float r2 = rand(seed);

  // TODO: Tangent and bitangent should be calculated from mesh (provided, the mesh has proper uvs)
  vec3 T, B;
  basis(N, T, B);
  V = ToLocal(T, B, N, V);  // NDotL = L.z; NDotV = V.z; NDotH = H.z

  // Specular and sheen color
  vec3 specCol, sheenCol;
  GetSpecColor(mat.diffuse, mat.metalness, mat.specularTint, mat.sheenTint, 1.5, specCol, sheenCol);

  // Lobe weights
  float diffuseWt, specReflectWt, specRefractWt, clearcoatWt;
  // Note: Fresnel is approx and based on N and not H since H isn't available at this stage.
  float approxFresnel = DisneyFresnel(mat.metalness, 1.5, V.z, V.z);
  GetLobeProbabilities(mat.metalness, mat.clearcoat, mat.diffuse, specCol, approxFresnel, diffuseWt, specReflectWt, clearcoatWt);

  // CDF for picking a lobe
  float cdf[3];
  cdf[0] = diffuseWt;
  cdf[1] = cdf[0] + clearcoatWt;
  cdf[2] = cdf[1] + specReflectWt;

  float clearcoatRoughness = mix(0.1, 0.001, mat.clearcoatGloss);
  float r0 = rand(seed);

  if(r0 < cdf[0])  // Diffuse Reflection Lobe
  {
    L = cosineSampleHemisphere(vec2(r1, r2));

    vec3 H = normalize(L + V);

    f = EvalDiffuse(mat.roughness, mat.metalness, mat.subsurface, mat.sheen, mat.diffuse, sheenCol, V, L, H, pdf);
    pdf *= diffuseWt;
  }
  else if(r0 < cdf[1])  // Clearcoat Lobe
  {
    vec3 H = SampleGTR1(clearcoatRoughness, r1, r2);

    if(H.z < 0.0)
      H = -H;

    L = normalize(reflect(-V, H));

    f = EvalClearcoat(clearcoatRoughness, mat.clearcoat, V, L, H, pdf);
    pdf *= clearcoatWt;
  }
  else  // Specular Reflection/Refraction Lobes
  {
    vec3 H = SampleGGXVNDF(V, mat.roughness, r1, r2);

    if(H.z < 0.0)
      H = -H;

    // TODO: Refactor into metallic BRDF and specular BSDF

    L = normalize(reflect(-V, H));
//    L = cosineSampleHemisphere(vec2(r1, r2));

    f = EvalSpecReflection(mat.metalness, mat.roughness, 1.5, specCol, V, L, H, pdf);

    pdf *= specReflectWt;
  }

  return f;
}

vec3 DisneyEval(in GpuMaterial mat, vec3 V, vec3 N, vec3 L, out float bsdfPdf)
{
  bsdfPdf = 0.0;
  vec3 f  = vec3(0.0);

  // TODO: Tangent and bitangent should be calculated from mesh (provided, the mesh has proper uvs)
  vec3 T, B;
  basis(N, T, B);
  V = ToLocal(T, B, N, V);  // NDotL = L.z; NDotV = V.z; NDotH = H.z
  L = ToLocal(T, B, N, L);

  vec3 H;
  if(L.z > 0.0)
    H = normalize(L + V);
  else
    H = normalize(L + V * 1.5);

  if(H.z < 0.0)
    H = -H;

  // Specular and sheen color
  vec3 specCol, sheenCol;
  GetSpecColor(mat.diffuse, mat.metalness, mat.specularTint, mat.sheenTint, 1.5, specCol, sheenCol);

  // Lobe weights
  float diffuseWt, specReflectWt, clearcoatWt;
  float fresnel = DisneyFresnel(mat.metalness, 1.5, dot(L, H), dot(V, H));
  GetLobeProbabilities(mat.metalness, mat.clearcoat, mat.diffuse, specCol, fresnel, diffuseWt, specReflectWt, clearcoatWt);

  float pdf;
  float clearcoatRoughness = mix(0.1, 0.001, mat.clearcoatGloss);

  // Diffuse
  if(diffuseWt > 0.0 && L.z > 0.0)
  {
    f += EvalDiffuse(mat.roughness, mat.metalness, mat.subsurface, mat.sheen, mat.diffuse, sheenCol, V, L, H, pdf);
    bsdfPdf += pdf * diffuseWt;
  }

  // Specular Reflection
  if(specReflectWt > 0.0 && L.z > 0.0 && V.z > 0.0)
  {
    f += EvalSpecReflection(mat.metalness, mat.roughness, 1.5, specCol, V, L, H, pdf);
    bsdfPdf += pdf * specReflectWt;
  }

  // Clearcoat
  if(clearcoatWt > 0.0 && L.z > 0.0 && V.z > 0.0)
  {
    f += EvalClearcoat(clearcoatRoughness, mat.clearcoat, V, L, H, pdf);
    bsdfPdf += pdf * clearcoatWt;
  }

  return f;
}

#endif