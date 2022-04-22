#include "material.h"

#include <nvvk/buffers_vk.hpp>

MaterialAlloc::MaterialAlloc(ContextAware *pContext, MaterialInterface *pMaterial,
                             const VkCommandBuffer &cmdBuf)
{
	auto              &m_alloc         = pContext->m_alloc;
	auto              &m_material_data = std::vector<GPUMaterial>{pMaterial->getMaterial()};
	VkBufferUsageFlags flag            = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	m_bMaterial =
	    m_alloc.createBuffer(cmdBuf, m_material_data, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
}

void MaterialAlloc::deinit(ContextAware *pContext)
{
	pContext->m_alloc.destroy(m_bMaterial);
	intoReleased();
}

MaterialBrdfHongzhi::MaterialBrdfHongzhi(int diffuseTextureId, int specularTextureId,
                                         int alphaTextureId, int normalTextureId,
                                         int tangentTextureId)
{
	m_material.diffuseTextureId  = diffuseTextureId;
	m_material.specularTextureId = specularTextureId;
	m_material.alphaTextureId    = alphaTextureId;
	m_material.normalTextureId   = normalTextureId;
	m_material.tangentTextureId  = tangentTextureId;
}

MaterialBrdfLambertian::MaterialBrdfLambertian(vec3 diffuse)
{
	m_material.diffuse = diffuse;
}