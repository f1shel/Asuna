#pragma once

#include <shared/material.h>
#include <context/context.h>
#include "alloc.h"

class Material
{
public:
  Material() {}
  Material(const GpuMaterial& material)
      : m_material(material)
  {
  }
  GpuMaterial& getMaterial() { return m_material; }

private:
  GpuMaterial m_material = {
      vec3(0.0),  // diffuse albedo
      0.0,        // roughness
      vec3(0.0),  // emittance
      0.0,        // metalness
      vec3(0.0),  // emittance factor
      -1,         // diffuse albedo texture id
      -1,         // roughness texture id
      -1,         // roughness texture id
      -1,         // emittance texture
  };
};

class MaterialAlloc : public GpuAlloc
{
public:
  MaterialAlloc(ContextAware* pContext, Material* pMaterial, const VkCommandBuffer& cmdBuf);
  void     deinit(ContextAware* pContext);
  VkBuffer getBuffer() { return m_bMaterial.buffer; }

private:
  nvvk::Buffer m_bMaterial;
};