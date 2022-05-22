#pragma once

#include <shared/pushconstant.h>
#include "pipeline.h"
#include "pipeline_graphics.h"


class PipelinePost : public PipelineAware
{
public:
  enum class HoldSet
  {
    Input = 0,
    Num   = 1
  };
  PipelinePost()
      : PipelineAware(uint(HoldSet::Num), PostBindSet::PostNum)
  {
  }
  virtual void         init(ContextAware* pContext, Scene* pScene, const VkDescriptorImageInfo* pImageInfo);
  virtual void         run(const VkCommandBuffer& cmdBuf);
  virtual void         deinit();
  GpuPushConstantPost& getPushconstant() { return m_pushconstant; }

private:
  GpuPushConstantPost m_pushconstant = {
      1.0f,          // brightness;
      1.0f,          // contrast;
      1.0f,          // saturation;
      0.0f,          // vignette;
      1.0f,          // avgLum;
      1.0f,          // zoom;
      {1.0f, 1.0f},  // renderingRatio;
      0,             // autoExposure;
      0.5f,          // Ywhite;  // Burning white
      0.5f,          // key;     // Log-average luminance
      true,          // use tone mapping
  };

private:
  void createPostDescriptorSetLayout();
  // Create post-processing pipeline
  void createPostPipeline();
  // Update the descriptor pointer
  void updatePostDescriptorSet(const VkDescriptorImageInfo* pImageInfo);
};