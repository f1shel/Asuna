#include "material.h"

#include <nvvk/buffers_vk.hpp>

MaterialAlloc::MaterialAlloc(ContextAware* pContext, Material* pMaterial,
                             const VkCommandBuffer& cmdBuf) {
  auto& m_alloc = pContext->getAlloc();
  auto& m_materialData = vector<GpuMaterial>{pMaterial->getMaterial()};
  VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  m_type = pMaterial->getType();
  m_bMaterial = m_alloc.createBuffer(cmdBuf, m_materialData,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
}

void MaterialAlloc::deinit(ContextAware* pContext) {
  pContext->getAlloc().destroy(m_bMaterial);
  intoReleased();
}