#pragma once

#include "pipeline.h"
#include "pipeline_graphic.h"

class PipelineCorrelatedPost : public PipelineCorrelated
{
  public:
	PipelineGraphic *m_pPipGraphic = nullptr;
};

class PipelinePost : public PipelineAware
{
  public:
	virtual void init(PipelineCorrelated *pPipCorr);
	virtual void run(const VkCommandBuffer &cmdBuf);
	virtual void deinit();

  private:
	// Accompanied graphic pipeline
	PipelineGraphic *m_pPipGraphic = nullptr;

  private:
	void createPostDescriptorSetLayout();
	// Create post-processing pipeline
	void createPostPipeline();
	// Update the descriptor pointer
	void updatePostDescriptorSet();
};