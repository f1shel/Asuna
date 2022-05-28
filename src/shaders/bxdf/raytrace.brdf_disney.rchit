#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

float sqr(float x)
{
  return x * x;
}

float SchlickFresnel(float u)
{
  float m  = clamp(1 - u, 0, 1);
  float m2 = m * m;
  return m2 * m2 * m;  // pow(m,5)
}

// Here a is clearcoatRoughness
float GTR1(float NdotH, float a)
{
  if(a >= 1)
    return 1 / PI;
  float a2 = a * a;
  float t  = 1 + (a2 - 1) * NdotH * NdotH;
  return (a2 - 1) / (PI * log(a2) * t);
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

float GTR2(float NdotH, float a)
{
  float a2 = a * a;
  float t  = 1 + (a2 - 1) * NdotH * NdotH;
  return a2 / (PI * t * t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay)
{
  if(NdotH <= 0)
    return 0.0;
  float result = 1 / (PI * ax * ay * sqr(sqr(HdotX / ax) + sqr(HdotY / ay) + NdotH * NdotH));
  if(result * NdotH < 1e-20)
    result = 0;
  return result;
}

float smithG_GGX(float NdotV, float alphaG)
{
  float a = alphaG * alphaG;
  float b = NdotV * NdotV;
  return 1 / (NdotV + sqrt(a + b - a * b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay)
{
  return 1 / (NdotV + sqrt(sqr(VdotX * ax) + sqr(VdotY * ay) + sqr(NdotV)));
}

float smithG1(vec3 v, vec3 m, in float ax, in float ay)
{
  /* Ensure consistent orientation (can't see the back
    of the microfacet from the front and vice versa) */
  if(dot(v, m) * v.z <= 0)
    return 0.0f;

  /* Perpendicular incidence -- no shadowing/masking */
  float tanTheta = abs(tanTheta(v));
  if(tanTheta == 0.0f)
    return 1.0f;

  float invSinTheta2 = 1 / (1 - v.z * v.z);
  float cosPhi2      = v.x * v.x * invSinTheta2;
  float sinPhi2      = v.y * v.y * invSinTheta2;

  float alpha = sqrt(cosPhi2 * ax * ax + sinPhi2 * ay * ay);

  float root = alpha * tanTheta;
  return 2.0 / (1.0 + hypot2(1.0f, root));
}

vec3 evalDisneyBrdf(in HitState state, in vec3 L)
{
  vec3  N = state.ffnormal, V = state.viewDir, X = state.tangent, Y = state.bitangent;
  float NdotL = dot(N, L);
  float NdotV = dot(N, V);
  if(NdotL <= 0 || NdotV <= 0)
    return vec3(0);

  vec3  H     = normalize(L + V);
  float NdotH = dot(N, H);
  float LdotH = dot(L, H);

  vec3  Cdlin = state.mat.diffuse;
  float Cdlum = .3 * Cdlin[0] + .6 * Cdlin[1] + .1 * Cdlin[2];  // luminance approx.

  vec3 Ctint  = Cdlum > 0 ? Cdlin / Cdlum : vec3(1);  // normalize lum. to isolate hue+sat
  vec3 Cspec0 = mix(state.mat.specular * .08 * mix(vec3(1), Ctint, state.mat.specularTint), Cdlin, state.mat.metalness);
  vec3 Csheen = mix(vec3(1), Ctint, state.mat.sheenTint);

  // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
  // and mix in diffuse retro-reflection based on roughness
  float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
  float Fd90 = 0.5 + 2 * LdotH * LdotH * state.mat.roughness;
  float Fd   = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

  // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
  // 1.25 scale is used to (roughly) preserve albedo
  // Fss90 used to "flatten" retroreflection based on roughness
  float Fss90 = LdotH * LdotH * state.mat.roughness;
  float Fss   = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
  float ss    = 1.25 * (Fss * (1 / (NdotL + NdotV) - .5) + .5);

  // specular
  float aspect = sqrt(1 - state.mat.anisotropic * .9);
  float ax     = max(.001, sqr(state.mat.roughness) / aspect);
  float ay     = max(.001, sqr(state.mat.roughness) * aspect);
  float Ds     = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
  float FH     = SchlickFresnel(LdotH);
  vec3  Fs     = mix(Cspec0, vec3(1), FH);
  float Gs;
  Gs = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
  Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);

  // sheen
  vec3 Fsheen = FH * state.mat.sheen * Csheen;

  // clearcoat (ior = 1.5 -> F0 = 0.04)
  float clearcoatRoughness = mix(.1, .001, state.mat.clearcoatGloss);
  float Dr                 = GTR1(NdotH, clearcoatRoughness);
  float Fr                 = mix(.04, 1.0, FH);
  float Gr                 = smithG_GGX(NdotL, .25) * smithG_GGX(NdotV, .25);

  // The denominator for specular reflection (4 * LdotN * VdotN) has been already
  // separated into smithG_GGX. We don't have to divide it again.
  return ((1 / PI) * mix(Fd, ss, state.mat.subsurface) * Cdlin + Fsheen) * (1 - state.mat.metalness) + Gs * Fs * Ds
         + .25 * state.mat.clearcoat * Gr * Fr * Dr;
}


// Sample hemisphere according to cosine-weighted solidangle.
vec3 sampleDiffuse()
{
  return cosineSampleHemisphere(vec2(rand(payload.seed), rand(payload.seed)));
}

vec2 sampleVisible11(in float thetaI, in vec2 p)
{
  vec2 slope;
  // Special case (normal incidence)
  if(thetaI < 1e-4)
  {
    float sinPhi = sin(2 * PI * p.y), cosPhi = cos(2 * PI * p.y);
    float r = safeSqrt(p.x / (1 - p.x));
    return vec2(r * cosPhi, r * sinPhi);
  }

  // Precomputations
  float tanThetaI = tan(thetaI);
  float a         = 1 / tanThetaI;
  float G1        = 2.0 / (1.0 + safeSqrt(1.0 + 1.0 / (a * a)));

  // Simulate X component
  float A = 2.0 * p.x / G1 - 1.0;
  if(abs(A) == 1)
    A -= sign(A) * EPS;
  float tmp       = 1.0 / (A * A - 1.0);
  float B         = tanThetaI;
  float D         = safeSqrt(B * B * tmp * tmp - (A * A - B * B) * tmp);
  float slope_x_1 = B * tmp - D;
  float slope_x_2 = B * tmp + D;
  slope.x         = (A < 0.0 || slope_x_2 > 1.0 / tanThetaI) ? slope_x_1 : slope_x_2;

  // Simulate Y component
  float S;
  if(p.y > 0.5)
  {
    S   = 1.0;
    p.y = 2.0 * (p.y - 0.5);
  }
  else
  {
    S   = -1.0;
    p.y = 2.0 * (0.5 - p.y);
  }

  // Improved fit
  float z = (p.y * (p.y * (p.y * (-0.365728915865723) + 0.790235037209296) - 0.424965825137544) + 0.000152998850436920)
            / (p.y * (p.y * (p.y * (p.y * 0.169507819808272 - 0.397203533833404) - 0.232500544458471) + 1) - 0.539825872510702);

  slope.y = S * z * sqrt(1.0 + slope.x * slope.x);

  return slope;
}

// The sampling routine from Heitz and d'Eon.
// https://hal.inria.fr/hal-00996995v2/file/supplemental1.pdf
// https://github.com/mitsuba-renderer/mitsuba/blob/master/src/bsdfs/microfacet.h
vec3 sampleVisible(in vec2 p, in vec3 wo, in float ax, in float ay)
{
  // Step 1: Stretch view direction
  vec3 wh = normalize(vec3(ax * wo.x, ay * wo.y, wo.z));

  // Get polar coordinates
  float theta = 0, phi = 0;
  if(wo.z < 0.99999)
  {
    theta = acos(wo.z);
    phi   = atan(wo.y / wo.x);
  }
  float sinPhi = sin(phi), cosPhi = cos(phi);

  // Step 2: simulate P22_{wi}(slope.x, slope.y, 1, 1)
  vec2 slope = sampleVisible11(theta, p);

  // Step 3: rotate
  slope = vec2(cosPhi * slope.x - sinPhi * slope.y, sinPhi * slope.x + cosPhi * slope.y);

  // Step 4: unstretch
  slope.x *= ax;
  slope.y *= ay;

  return normalize(vec3(-slope.x, -slope.y, 1));
}

vec3 SampleGGXVNDF(vec3 wo, float rgh, float r1, float r2)
{
  vec3 wh = normalize(vec3(rgh * wo.x, rgh * wo.y, wo.z));

  float lensq = wh.x * wh.x + wh.y * wh.y;
  vec3  T1    = lensq > 0 ? vec3(-wh.y, wh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
  vec3  T2    = cross(wh, T1);

  float r   = sqrt(r1);
  float phi = 2.0 * PI * r2;
  float t1  = r * cos(phi);
  float t2  = r * sin(phi);
  float s   = 0.5 * (1.0 + wh.z);
  t2        = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

  vec3 nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * wh;

  return normalize(vec3(rgh * nh.x, rgh * nh.y, max(0.0, nh.z)));
}

vec3 sampleGGXAniso(float ax, float ay, vec2 p)
{
  /* Sample phi component (anisotropic case) */
  float phiM    = atan(ay / ax * tan(PI + TWO_PI * p.y)) + PI * floor(2 * p.y + 0.5f);
  float sinPhiM = sin(phiM), cosPhiM = cos(phiM);
  float cosSc = cosPhiM / ax, sinSc = sinPhiM / ay;
  float alphaSqr = 1.0f / (cosSc * cosSc + sinSc * sinSc);

  /* Sample theta component */
  float tanThetaMSqr = alphaSqr * p.x / (1.0f - p.x);
  float cosThetaM    = 1.0f / sqrt(1.0f + tanThetaMSqr);

  /* Compute probability density of the sampled position */
  float temp = 1 + tanThetaMSqr / alphaSqr;
  float pdf  = INV_PI / (ax * ay * cosThetaM * cosThetaM * cosThetaM * temp * temp);

  /* Prevent potential numerical issues in other stages of the model */
  if(pdf < 1e-20)
    pdf = 0;

  float sinThetaM = sqrt(max(0, 1 - cosThetaM * cosThetaM));

  return vec3(sinThetaM * cosPhiM, sinThetaM * sinPhiM, cosThetaM);
}

vec3 sampleSpecular(in float clearcoat, in vec3 wo, in float clearcoatRoughness, in float ax, in float ay)
{
  // Select lobe by the relative weights.
  // Sample the microfacet normal first, then compute the reflect direction.
  vec3  wm;
  float gtr2Weight = 1.f / (clearcoat + 1.f);
  float rx = rand(payload.seed), ry = rand(payload.seed);

  // if(rand(payload.seed) < gtr2Weight)
  // Sample visible normal
  // wm = sampleVisible(vec2(rx, ry), wo, ax, ay);
  // wm = SampleGGXVNDF(wo, ax, rx, ry);
  wm = sampleGGXAniso(ax, ay, vec2(rx, ry));
  // else
  // wm = SampleGTR1(clearcoatRoughness, rx, ry);
  return reflect(-wo, wm);
}

float specularPdf(in vec3 wm, in vec3 wo, in float clearcoat, in float clearcoatRoughness, in float ax, in float ay)
{
  vec3 wi = reflect(-wo, wm);
  if(wi.z <= 0.0 || wo.z <= 0.0)
    return 0.0;

  float IdotM = abs(dot(wi, wm));

  float clearcoatWeight = clearcoat / (clearcoat + 1.0f);

  float VdotN = max(1e-4, wo.z);
  float Dw    = smithG_GGX_aniso(wo.z, wo.x, wo.y, ax, ay) * GTR2_aniso(wm.z, wm.x, wm.y, ax, ay) * 2.0f * IdotM / wo.z;
  // float D  = mix(Dw, GTR1(wm.z, clearcoatRoughness) * abs(wm.z) / IdotM, clearcoatWeight);
  float D = smithG1(wo, wm, ax, ay) * IdotM * GTR2_aniso(wm.z, wm.x, wm.y, ax, ay) / abs(wi.z);
  // return D * 0.25f;
  return GTR2_aniso(wm.z, wm.x, wm.y, ax, ay) * wm.z / (4*abs(dot(wo, wm)));
}

void main()
{
  // Get hit state
  HitState state = getHitState();

  // Hit Light
  if(state.lightId > 0)
  {
    hitLight(state.lightId, state.hitPos);
    return;
  }

  // Build local frame
  mat3 local2global = mat3(state.tangent, state.bitangent, state.ffnormal);
  mat3 global2local = transpose(local2global);

  // Fetch textures
  if(state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = texture(textureSamplers[nonuniformEXT(state.mat.diffuseTextureId)], state.uv).rgb;
  if(state.mat.metalnessTextureId >= 0)
    state.mat.metalness = texture(textureSamplers[nonuniformEXT(state.mat.metalnessTextureId)], state.uv).r;
  if(state.mat.roughnessTextureId >= 0)
    state.mat.roughness = texture(textureSamplers[nonuniformEXT(state.mat.roughnessTextureId)], state.uv).r;
  if(state.mat.normalTextureId >= 0)
  {
    vec3 c         = texture(textureSamplers[nonuniformEXT(state.mat.normalTextureId)], state.uv).rgb;
    vec3 n         = 2 * c - 1;
    state.ffnormal = normalize(local2global * n);
    if(dot(state.ffnormal, state.viewDir) < 0)
      state.ffnormal = -state.ffnormal;
    // Rebuild frame
    basis(state.ffnormal, state.tangent, state.bitangent);
    local2global = mat3(state.tangent, state.bitangent, state.ffnormal);
    global2local = transpose(local2global);
  }
  float aspect             = sqrt(1 - state.mat.anisotropic * .9);
  float ax                 = max(.001, sqr(state.mat.roughness) / aspect);
  float ay                 = max(.001, sqr(state.mat.roughness) * aspect);
  float clearcoatRoughness = mix(.1, .001, state.mat.clearcoatGloss);

  // Direct light
  {
    // Light and environment contribution
    payload.lightRadiance = vec3(0);
    payload.lightVisible  = 0;
    LightSample lightSample;

    vec3  Li               = vec3(0);
    bool  allowDoubleSide  = false;
    float envOrAnalyticPdf = 0.5;

    if(pc.numLights == 0)
      envOrAnalyticPdf = 1.0;
    if(rand(payload.seed) < envOrAnalyticPdf)
    {
      sampleEnvironmentLight(lightSample);
      lightSample.normal = -state.ffnormal;
      lightSample.emittance /= envOrAnalyticPdf;
    }
    else
    {
      // sample analytic light
      int      lightIndex = min(1 + int(rand(payload.seed) * pc.numLights), pc.numLights);
      GpuLight light      = lights.l[lightIndex];
      sampleOneLight(payload.seed, light, state.hitPos, lightSample);
      lightSample.emittance *= pc.numLights;  // selection pdf
      lightSample.emittance /= 1.0 - envOrAnalyticPdf;
      allowDoubleSide = (light.doubleSide == 1);
    }

    if(dot(lightSample.direction, state.ffnormal) > 0.0 && (dot(lightSample.normal, lightSample.direction) < 0 || allowDoubleSide))
    {
      BsdfSample bsdfSample;

      vec3 bsdfSampleVal = evalDisneyBrdf(state, lightSample.direction);

      vec3 wo        = normalize(global2local * state.viewDir);
      vec3 wi        = normalize(global2local * lightSample.direction);
      vec3 wh        = normalize(wi + wo);
      bsdfSample.pdf = specularPdf(wh, wo, state.mat.clearcoat, clearcoatRoughness, ax, ay);
      // bsdfSample.pdf = cosineHemispherePdf(wi.z);

      float misWeight = powerHeuristic(lightSample.pdf, bsdfSample.pdf);

      Li += misWeight * bsdfSampleVal * max(dot(lightSample.direction, state.ffnormal), 0.0) * lightSample.emittance
            / (lightSample.pdf + EPS);

      payload.lightVisible  = 1;
      payload.lightDir      = lightSample.direction;
      payload.lightDist     = lightSample.dist;
      payload.lightRadiance = Li * payload.throughput;
    }

    payload.shouldDirectLight = 1;
    payload.lightHitPos       = offsetPositionAlongNormal(state.hitPos, state.ffnormal);
  }

  if(payload.depth >= pc.maxPathDepth)
  {
    payload.stop = 1;
    return;
  }

  // Sample next ray
  BsdfSample bsdfSample;
  vec3       wo        = normalize(global2local * state.viewDir);
  vec3       wi        = sampleSpecular(state.mat.clearcoat, wo, state.mat.roughness, ax, ay);
  // vec3       wi        = cosineSampleHemisphere(vec2(rand(payload.seed), rand(payload.seed)));
  vec3       wh        = normalize(wi + wo);
  bsdfSample.direction = normalize(local2global * wi);
  bsdfSample.pdf       = specularPdf(wh, wo, state.mat.clearcoat, clearcoatRoughness, ax, ay);
  // bsdfSample.pdf       = cosineHemispherePdf(wi.z);
  vec3 bsdfSampleVal   = evalDisneyBrdf(state, bsdfSample.direction);

  if(bsdfSample.pdf < 0.0)
  {
    payload.stop = 1;
    return;
  }
  // Next ray
  payload.bsdfPdf       = bsdfSample.pdf;
  payload.ray.direction = bsdfSample.direction;
  payload.throughput *= bsdfSampleVal * abs(dot(state.ffnormal, bsdfSample.direction)) / (bsdfSample.pdf + EPS);
  payload.ray.origin = payload.lightHitPos;
}