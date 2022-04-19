#pragma once

#include "context/context.h"
#include "pipeline/pipeline_graphics.h"
#include "pipeline/pipeline_post.h"
#include "pipeline/pipeline_raytrace.h"
#include "scene/scene.h"

struct TracerInitState
{
	bool        m_offline    = false;
	std::string m_scenefile  = "";
	std::string m_outputname = "";
};

class Tracer
{
  public:
	void init(TracerInitState tis);
	void run();
	void deinit();

  private:
	TracerInitState m_tis;
	// context
	ContextAware m_context;
	// scene
	Scene m_scene;
	// pipelines
	PipelineGraphics m_pipelineGraphics;
	PipelineRaytrace m_pipelineRaytrace;
	PipelinePost     m_pipelinePost;

  private:
	void runOnline();
	void runOffline();
	void imageToBuffer(const nvvk::Texture &imgIn, const VkBuffer &pixelBufferOut);
	void saveImageTest();
	void saveImage(nvvk::Buffer pixelBuffer, std::string outputpath, int channelId = -1);
};