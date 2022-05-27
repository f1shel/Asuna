#version 460
#extension GL_EXT_debug_printf : require
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../utils/rchit_layouts.glsl"

vec3 fSchlick(float cosTheta, vec3 f0)
{
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float dGGX(vec3 normal, vec3 hal, float roughness)
{
  float a      = roughness * roughness;
  float a2     = a * a;
  float nDotH  = max(dot(normal, hal), 0.0);
  float nDotH2 = nDotH * nDotH;

  float num   = a2;
  float denom = (nDotH2 * (a2 - 1.0) + 1.0);
  denom       = PI * denom * denom;

  return num / (denom + EPS);
}

// https://github.com/QianMo/PBR-White-Paper/blob/master/content/part%205/README.md
float g1SmithGGX(float nDotV, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num   = nDotV;
  float denom = nDotV * (1.0 - k) + k;

  return num / (denom + EPS);
}

float g2SmithGGX(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness)
{
  float nDotV = max(dot(normal, viewDir), 0.0);
  float nDotL = max(dot(normal, lightDir), 0.0);
  float ggx2  = g1SmithGGX(nDotV, roughness);
  float ggx1  = g1SmithGGX(nDotL, roughness);

  return ggx1 * ggx2;
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

  // Fetch textures
  if(state.mat.diffuseTextureId >= 0)
    state.mat.diffuse = texture(textureSamplers[nonuniformEXT(state.mat.diffuseTextureId)], state.uv).rgb;
  if(state.mat.metalnessTextureId >= 0)
    state.mat.metalness = texture(textureSamplers[nonuniformEXT(state.mat.metalnessTextureId)], state.uv).r;
  if(state.mat.roughnessTextureId >= 0)
    state.mat.roughness = texture(textureSamplers[nonuniformEXT(state.mat.roughnessTextureId)], state.uv).r;
  // Build local frame
  mat3 local2global = mat3(state.tangent, state.bitangent, state.ffnormal);
  mat3 global2local = transpose(local2global);

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

      vec3 f0     = vec3(0.04);
      f0          = mix(f0, state.mat.diffuse, state.mat.metalness);
      vec3  hal   = normalize(lightSample.direction + state.viewDir);
      vec3  brdfF = fSchlick(max(dot(hal, state.viewDir), 0.0), f0);
      float brdfD = dGGX(state.ffnormal, hal, state.mat.roughness);
      float brdfG = g2SmithGGX(state.ffnormal, state.viewDir, lightSample.direction, state.mat.roughness);
      float denominator =
          4.0 * max(dot(state.ffnormal, state.viewDir), 0.0) * max(dot(state.ffnormal, lightSample.direction), 0.0) + EPS;
      vec3 brdfDiffuse   = state.mat.diffuse * INV_PI;
      vec3 brdfSpecular  = brdfF * brdfD * brdfG / denominator;
      vec3 bsdfSampleVal = brdfDiffuse * (1 - state.mat.metalness) * (1 - brdfF) + brdfSpecular;

      // TODO: importance sampling ggx
      bsdfSample.pdf  = cosineHemispherePdf(dot(lightSample.direction, state.ffnormal));
      float misWeight = powerHeuristic(lightSample.pdf, bsdfSample.pdf);

      Li += misWeight * bsdfSampleVal * dot(lightSample.direction, state.ffnormal) * lightSample.emittance
            / (lightSample.pdf + EPS);

      payload.lightVisible  = 1;
      payload.lightDir      = lightSample.direction;
      payload.lightDist     = lightSample.dist;
      payload.lightRadiance = Li * payload.throughput;
    }

    payload.shouldDirectLight = 1;
    payload.lightHitPos = offsetPositionAlongNormal(state.hitPos, state.ffnormal);
  }

  if(payload.depth >= pc.maxPathDepth)
  {
    payload.stop = 1;
    return;
  }

  // Sample next ray
  BsdfSample bsdfSample;
  bsdfSample.direction = normalize(local2global * cosineSampleHemisphere(vec2(rand(payload.seed), rand(payload.seed))));
  bsdfSample.pdf       = cosineHemispherePdf(dot(bsdfSample.direction, state.ffnormal));

  vec3 f0     = vec3(0.04);
  f0          = mix(f0, state.mat.diffuse, state.mat.metalness);
  vec3  hal   = normalize(bsdfSample.direction + state.viewDir);
  vec3  brdfF = fSchlick(max(dot(hal, state.viewDir), 0.0), f0);
  float brdfD = dGGX(state.ffnormal, hal, state.mat.roughness);
  float brdfG = g2SmithGGX(state.ffnormal, state.viewDir, bsdfSample.direction, state.mat.roughness);
  float denominator =
      4.0 * max(dot(state.ffnormal, state.viewDir), 0.0) * max(dot(state.ffnormal, bsdfSample.direction), 0.0) + EPS;
  vec3 brdfDiffuse   = state.mat.diffuse * INV_PI;
  vec3 brdfSpecular  = brdfF * brdfD * brdfG / denominator;
  vec3 bsdfSampleVal = brdfDiffuse * (1 - state.mat.metalness) * (1 - brdfF) + brdfSpecular;

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