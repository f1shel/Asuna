#pragma once

#include <shared/pushconstant.h>
#include "pipeline.h"
#include "pipeline_graphics.h"

class PipelinePost : public PipelineAware {
public:
#ifdef NVP_SUPPORTS_OPTIX7
  enum class HoldSet{Input = 0, Input2 = 1, Input3 = 2, Num = 3};
#else
  enum class HoldSet { Input = 0, Num = 3 };
#endif
  PipelinePost() : PipelineAware(uint(HoldSet::Num), PostBindSet::PostNum) {}
  virtual void init(ContextAware* pContext, Scene* pScene,
                    const VkDescriptorImageInfo* pImageInfo);
  virtual void run(const VkCommandBuffer& cmdBuf);
  virtual void deinit();
  GpuPushConstantPost& getPushconstant() {
    return m_pScene->getPipelineState().postState;
  }

private:
  void createPostDescriptorSetLayout();
  // Create post-processing pipeline
  void createPostPipeline();
  
  // #OPTIX_D
  uint m_postFrame = 0;

public:
  // Update the descriptor pointer
  void updatePostDescriptorSet(const VkDescriptorImageInfo* pImageInfo);
};