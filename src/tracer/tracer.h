#pragma once

#include "context/context.h"
#include "pipeline/pipeline_graphics.h"
#include "pipeline/pipeline_post.h"
#include "pipeline/pipeline_raytrace.h"
#include "scene/scene.h"

struct TracerInitSettings {
  bool offline = false;
  string scenefile = "";
  string outputname = "";
  int sceneSpp = 0;
};

class Tracer {
public:
  void init(TracerInitSettings tis);
  void run();
  void deinit();

private:
  TracerInitSettings m_tis;
  ContextAware m_context;
  Scene m_scene;
  PipelineGraphics m_pipelineGraphics;
  PipelineRaytrace m_pipelineRaytrace;
  PipelinePost m_pipelinePost;

private:
  void runOnline();
  void runOffline();
  void parallelLoading();
  void vkTextureToBuffer(const nvvk::Texture& imgIn,
                         const VkBuffer& pixelBufferOut);

  // Transfer color data to pixelBuffer, and write it to disk as an image.
  // channelId controls which color data will be copied to pixelBuffer:
  // (1) channelId = -1, copy ldr output after post processing
  // (2) channelId > 0, copy corresponding hdr channel before post processing
  void saveBufferToImage(nvvk::Buffer pixelBuffer, std::string outputpath,
                         int channelId = -1);

private:
  bool m_busy = false;
  string m_busyReasonText = "";
  void renderGUI();
  bool guiCamera();
  bool guiEnvironment();
  bool guiTonemapper();
  bool guiPathTracer();
  void guiBusy();
};