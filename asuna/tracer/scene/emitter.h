#pragma once

#include <vector>
#include "../../hostdevice/emitter.h"
#include "../context/context.h"
#include "alloc.h"

class EmitterAlloc : public GPUAlloc
{
  public:
    EmitterAlloc(ContextAware *pContext, std::vector<GPUEmitter> &emitters,
                 const VkCommandBuffer &cmdBuf);
    void     deinit(ContextAware *pContext);
    VkBuffer getBuffer()
    {
        return m_bEmitter.buffer;
    }

  private:
    nvvk::Buffer m_bEmitter;
};