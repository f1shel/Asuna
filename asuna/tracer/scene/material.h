#pragma once

#include "../../hostdevice/material.h"
#include "../context/context.h"
#include "alloc.h"

enum GPUMaterialType
{
	eMaterialBrdfHongzhi = 0,
	eMaterialTypeCount   = 1
};

class MaterialInterface
{
  public:
	MaterialInterface()
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

  protected:
	GPUMaterial m_material;
};

class MaterialBrdfLambertian : public MaterialInterface
{
  public:
	MaterialBrdfLambertian(vec3 diffuse = {0.3});
};

class MaterialBrdfHongzhi : public MaterialInterface
{
  public:
	MaterialBrdfHongzhi(int diffuseTextureId, int specularTextureId, int alphaTextureId,
	                    int normalTextureId, int tangentTextureId);
};

class MaterialAlloc : public GPUAlloc
{
  public:
	MaterialAlloc(ContextAware *pContext, MaterialInterface *pMaterial,
	              const VkCommandBuffer &cmdBuf);
	void     deinit(ContextAware *pContext);
	VkBuffer getBuffer()
	{
		return m_bMaterial.buffer;
	}

  private:
	nvvk::Buffer m_bMaterial;
};