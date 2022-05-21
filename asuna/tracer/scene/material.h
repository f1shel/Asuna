#pragma once

#include "../../hostdevice/material.h"
#include "../context/context.h"
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
      vec3(0.0),  // specular albedo
      vec2(0.0),  // alpha x and y
      vec2(0.0),  // roughness x and y
      vec3(0.0),  // emittance
      -1,         // diffuse albedo texture id
      -1,         // specular albedo texture id
      -1,         // alpha texture id
      -1,         // roughness texture id
      -1,         // emittance texture id
      -1,         // normal texture id
      -1,         // tangent texture id
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