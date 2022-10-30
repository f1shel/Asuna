#pragma once

#include "context/context.h"
#include "pipeline/pipeline_graphics.h"
#include "pipeline/pipeline_post.h"
#include "pipeline/pipeline_raytrace.h"
#include "scene/scene.h"
#include "denoiser.h"

struct TracerInitSettings {
  bool offline = false;
  bool output_scanline = false;
  string scenefile = "";
  string outputname = "";
  int sceneSpp = 0;
  int gpuId = 0;
};

class Tracer : public ContextAware {
public:
  void init(TracerInitSettings tis);
  void run();
  void deinit();

private:
  TracerInitSettings m_tis;
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
  bool guiDenoiser();
  void guiBusy();

private:
  // #OPTIX_D
#ifdef NVP_SUPPORTS_OPTIX7
  DenoiserOptix m_denoiser;
#endif  // NVP_SUPPORTS_OPTIX7

  // Timeline semaphores
  uint64_t m_fenceValue{0};
  bool m_denoiseApply{false};
  bool m_denoiseFirstFrame{false};
  int m_denoiseEveryNFrames{100};

  // #OPTIX_D
  nvvk::Texture m_gAlbedo;
  nvvk::Texture m_gNormal;
  nvvk::Texture m_gDenoised;

  // #OPTIX_D
  void submitWithTLSemaphore(const VkCommandBuffer& cmdBuf);
  void submitFrame(const VkCommandBuffer& cmdBuf);
  void createGbuffers();
  void denoise();
  void setImageToDisplay();
  bool needToDenoise();
  void copyImagesToCuda(const VkCommandBuffer& cmdBuf);
  void copyCudaImagesToVulkan(const VkCommandBuffer& cmdBuf);
};