#pragma once

#include <vector>
#include <shared/light.h>
#include <context/context.h>
#include "alloc.h"

class LightsAlloc : public GpuAlloc
{
public:
  LightsAlloc(ContextAware* pContext, vector<GpuLight>& lights, const VkCommandBuffer& cmdBuf);
  void     deinit(ContextAware* pContext);
  VkBuffer getBuffer() { return m_bLights.buffer; }

private:
  nvvk::Buffer m_bLights;
};