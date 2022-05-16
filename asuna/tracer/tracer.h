#pragma once

#include "context/context.h"
#include "pipeline/pipeline_graphics.h"
#include "pipeline/pipeline_post.h"
#include "pipeline/pipeline_raytrace.h"
#include "scene/scene.h"

struct TracerInitState
{
  bool   offline    = false;
  string scenefile  = "";
  string outputname = "";
};

class Tracer
{
public:
  void init(TracerInitState tis);
  void run();
  void deinit();
  void resetFrame();

private:
  TracerInitState  m_tis;
  ContextAware     m_context;           // context
  Scene            m_scene;             // scene
  PipelineGraphics m_pipelineGraphics;  // pipelines
  PipelineRaytrace m_pipelineRaytrace;
  PipelinePost     m_pipelinePost;

private:
  void runOnline();
  void runOffline();
  void imageToBuffer(const nvvk::Texture& imgIn, const VkBuffer& pixelBufferOut);
  void saveImageTest();
  void saveImage(nvvk::Buffer pixelBuffer, std::string outputpath, int channelId = -1);

private:
  void renderGUI();
  bool guiCamera();
  bool guiEnvironment();
  bool guiTonemapper();
};