#pragma once

#include "pipeline.h"
#include "pipeline_graphics.h"

class PipelinePost : public PipelineAware
{
  public:
    // pis.
    virtual void init(PipelineInitState pis);
    virtual void run(const VkCommandBuffer &cmdBuf);
    virtual void deinit();

  private:
    // Accompanied graphic pipeline
    PipelineGraphics *m_pPipGraphics = nullptr;

  private:
    void createPostDescriptorSetLayout();
    // Create post-processing pipeline
    void createPostPipeline();
    // Update the descriptor pointer
    void updatePostDescriptorSet();
};