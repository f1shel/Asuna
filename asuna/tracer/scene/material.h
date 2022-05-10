#pragma once

#include "../../hostdevice/material.h"
#include "../context/context.h"
#include "alloc.h"

enum GPUMaterialType
{
    eMaterialTypeCount = 0
};

class Material
{
  public:
    Material()
    {
        m_material.diffuse            = 0.0;
        m_material.specular           = 0.0;
        m_material.axay               = 0.0;
        m_material.roughness          = 0.0;
        m_material.diffuseTextureId   = -1;
        m_material.specularTextureId  = -1;
        m_material.alphaTextureId     = -1;
        m_material.roughnessTextureId = -1;
        m_material.normalTextureId    = -1;
        m_material.tangentTextureId   = -1;
    }
    GPUMaterial &getMaterial()
    {
        return m_material;
    }

    GPUMaterialType m_type{eMaterialTypeCount};
    GPUMaterial     m_material;
};

class MaterialAlloc : public GPUAlloc
{
  public:
    MaterialAlloc(ContextAware *pContext, Material *pMaterial,
                  const VkCommandBuffer &cmdBuf);
    void     deinit(ContextAware *pContext);
    VkBuffer getBuffer()
    {
        return m_bMaterial.buffer;
    }

  private:
    nvvk::Buffer m_bMaterial;
};