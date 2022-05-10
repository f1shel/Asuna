#include "material.h"

#include <nvvk/buffers_vk.hpp>

MaterialAlloc::MaterialAlloc(ContextAware *pContext, Material *pMaterial,
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