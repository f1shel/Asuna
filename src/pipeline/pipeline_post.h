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

  protected:
  virtual void initPushconstant();

private:
  GpuPushConstantPost m_pushconstant = {};

private:
  void createPostDescriptorSetLayout();
  // Create post-processing pipeline
  void createPostPipeline();
  // Update the descriptor pointer
  void updatePostDescriptorSet(const VkDescriptorImageInfo* pImageInfo);
};