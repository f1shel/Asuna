#include "light.h"

LightsAlloc::LightsAlloc(ContextAware* pContext, vector<GpuLight>& lights, const VkCommandBuffer& cmdBuf)
{
  auto&              m_alloc = pContext->getAlloc();
  VkBufferUsageFlags flag    = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  m_bLights                  = m_alloc.createBuffer(cmdBuf, lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
}

void LightsAlloc::deinit(ContextAware* pContext)
{
  pContext->getAlloc().destroy(m_bLights);
  intoReleased();
}