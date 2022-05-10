#include "emitter.h"

EmitterAlloc::EmitterAlloc(ContextAware *pContext, std::vector<GPUEmitter> &emitters,
                           const VkCommandBuffer &cmdBuf)
{
    auto              &m_alloc = pContext->m_alloc;
    VkBufferUsageFlags flag    = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    m_bEmitter =
        m_alloc.createBuffer(cmdBuf, emitters, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
}

void EmitterAlloc::deinit(ContextAware *pContext)
{
    pContext->m_alloc.destroy(m_bEmitter);
    intoReleased();
}