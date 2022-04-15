#pragma once

#include "context/context.h"
#include "pipeline/pipeline_graphic.h"
#include "pipeline/pipeline_post.h"
#include "pipeline/pipeline_raytrace.h"
#include "scene/scene.h"

class Tracer
{
  public:
	void init();
	void run();
	void deinit();

  private:
	// context
	ContextAware m_context;
	// scene
	Scene m_scene;
	// pipelines
	PipelineGraphic  m_pipelineGraphic;
	PipelineRaytrace m_pipelineRaytrace;
	PipelinePost     m_pipelinePost;

  private:
	void runOnline();
	void runOffline();
	void imageToBuffer(const nvvk::Texture &imgIn, const VkBuffer &pixelBufferOut);
	void saveImage();
};