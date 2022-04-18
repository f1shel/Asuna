#pragma once

#include "pipeline.h"
#include "pipeline_graphics.h"

class PipelineCorrelatedPost : public PipelineCorrelated
{
  public:
	PipelineGraphics *m_pPipGraphics = nullptr;
};

class PipelinePost : public PipelineAware
{
  public:
	virtual void init(PipelineCorrelated *pPipCorr);
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