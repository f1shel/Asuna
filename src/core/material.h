#pragma once

#include <shared/material.h>
#include <context/context.h>
#include "alloc.h"

class Material {
public:
  Material() {
    m_material.diffuse = vec3(0.f);
    m_material.anisoAlpha = vec2(0.f);
    m_material.rhoSpec = vec3(0.f);
    m_material.ior = 1.5f;
    m_material.roughness = 0.f;
    m_material.emittance = vec3(0.f);
    m_material.metalness = 0.f;
    m_material.roughness = 0.5f;
    m_material.subsurface = 0.f;
    m_material.specular = 0.f;
    m_material.specularTint = 0.f;
    m_material.anisotropic = 0.f;
    m_material.sheen = 0.f;
    m_material.sheenTint = 0.f;
    m_material.clearcoat = 0.f;
    m_material.clearcoatGloss = 0.f;
    m_material.emittanceFactor = 1.f;
    m_material.diffuseTextureId = -1;
    m_material.emittanceTextureId = -1;
    m_material.metalnessTextureId = -1;
    m_material.normalTextureId = -1;
    m_material.roughnessTextureId = -1;
    m_material.tangentTextureId = -1;
    m_material.type = MaterialTypeBrdfLambertian;
  }
  Material(const GpuMaterial& material) : m_material(material) {}
  GpuMaterial& getMaterial() { return m_material; }
  MaterialType getType() { return MaterialType(m_material.type); }

private:
  GpuMaterial m_material;
};

class MaterialAlloc : public GpuAlloc {
public:
  MaterialAlloc(ContextAware* pContext, Material* pMaterial,
                const VkCommandBuffer& cmdBuf);
  void deinit(ContextAware* pContext);
  VkBuffer getBuffer() { return m_bMaterial.buffer; }
  MaterialType getType() { return m_type; }

private:
  MaterialType m_type;
  nvvk::Buffer m_bMaterial;
};