#include "tracer.h"
#include <loader/loader.h>

#include <backends/imgui_impl_glfw.h>
#include <nvh/timesampler.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/structs_vk.hpp>
#include <ext/tqdm.h>

#include <iostream>

#include <filesystem/path.h>
using namespace filesystem;

void Tracer::init(TracerInitSettings tis) {
  m_tis = tis;

  // Get film size and set size for context
  auto filmResolution =
      Loader().loadSizeFirst(m_tis.scenefile, ContextAware::getRoot());
  ContextAware::setSize(filmResolution);
  if (tis.sceneSpp != 0) m_scene.setSpp(tis.sceneSpp);

  // Initialize context and set context pointer for scene
  ContextAware::init({m_tis.offline});
  m_scene.init(reinterpret_cast<ContextAware*>(this));

#ifdef NVP_SUPPORTS_OPTIX7
  m_denoiser.init(reinterpret_cast<ContextAware*>(this));

  OptixDenoiserOptions dOptions;
  dOptions.guideAlbedo = true;
  dOptions.guideNormal = true;
  m_denoiser.initOptiX(dOptions, OPTIX_PIXEL_FORMAT_FLOAT4, true);
#endif               // NVP_SUPPORTS_OPTIX7
  createGbuffers();  // #OPTIX_D
  // #OPTIX_D
#ifdef NVP_SUPPORTS_OPTIX7
  {  // Denoiser
    m_denoiser.allocateBuffers(m_size);
    m_denoiser.createSemaphore();
    m_denoiser.createCopyPipeline();
    LOG_INFO("{}: creation done", "Denoiser");
  }
#endif  // NVP_SUPPORTS_OPTIX7

  if (ContextAware::getOfflineMode())
    parallelLoading();
  else {
    m_busy = true;
    std::thread([&] {
      m_busyReasonText = "Loading Scene";
      parallelLoading();
      m_busy = false;
    }).detach();
  }
}

void Tracer::run() {
  if (ContextAware::getOfflineMode())
    runOffline();
  else
    runOnline();
}

void Tracer::deinit() {
  m_pipelineGraphics.deinit();
  m_pipelineRaytrace.deinit();
  m_pipelinePost.deinit();
  m_scene.deinit();
  ContextAware::deinit();
}

void Tracer::runOnline() {
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
  clearValues[1].depthStencil = {1.0f, 0};

  // Main loop
  while (!ContextAware::shouldGlfwCloseWindow()) {
    glfwPollEvents();
    if (ContextAware::isMinimized()) continue;

    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    renderGUI();

    // Acquire swap chain
    ContextAware::prepareFrame();

    // Start command buffer of this frame
    uint32_t curFrame = ContextAware::getCurFrame();

    // Two command buffer in a frame, before and after denoiser
    const VkCommandBuffer& cmdBuf1 =
        ContextAware::getCommandBuffers()[2 * curFrame + 0];
    const VkCommandBuffer& cmdBuf2 =
        ContextAware::getCommandBuffers()[2 * curFrame + 1];

    VkCommandBufferBeginInfo beginInfo = nvvk::make<VkCommandBufferBeginInfo>();
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    {
      do {
        if (m_busy) break;
        setImageToDisplay();
        vkBeginCommandBuffer(cmdBuf1, &beginInfo);

        // For procedural rendering
        m_pipelineRaytrace.setSpp(1);

        // Update camera and sunsky
        m_pipelineGraphics.run(cmdBuf1);

        // Ray tracing
        m_pipelineRaytrace.run(cmdBuf1);

        copyImagesToCuda(cmdBuf1);

        vkEndCommandBuffer(cmdBuf1);
        submitWithTLSemaphore(cmdBuf1);

      } while (0);

      // ---- Cuda part --
      denoise();
      // ----

      do {
        vkBeginCommandBuffer(cmdBuf2, &beginInfo);
        copyCudaImagesToVulkan(cmdBuf2);

        // Post processing
        {
          VkRenderPassBeginInfo postRenderPassBeginInfo{
              VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
          postRenderPassBeginInfo.clearValueCount = 2;
          postRenderPassBeginInfo.pClearValues = clearValues.data();
          postRenderPassBeginInfo.renderPass = ContextAware::getRenderPass();
          postRenderPassBeginInfo.framebuffer =
              ContextAware::getFramebuffer(curFrame);
          postRenderPassBeginInfo.renderArea = {{0, 0},
                                                ContextAware::getSize()};

          // Rendering to the swapchain framebuffer the rendered image and
          // apply a tonemapper
          vkCmdBeginRenderPass(cmdBuf2, &postRenderPassBeginInfo,
                               VK_SUBPASS_CONTENTS_INLINE);

          if (!m_busy) m_pipelinePost.run(cmdBuf2);

          // Rendering UI
          ImGui::Render();
          ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf2);

          // Display axis in the lower left corner.
          // vkAxis.display(cmdBuf, CameraManip.getMatrix(),
          // vkSample.getSize());

          vkCmdEndRenderPass(cmdBuf2);
        }
        vkEndCommandBuffer(cmdBuf2);
      } while (0);
    }

    submitFrame(cmdBuf2);
  }
  vkDeviceWaitIdle(ContextAware::getDevice());
}

void Tracer::runOffline() {
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
  clearValues[1].depthStencil = {1.0f, 0};

  // Vulkan allocator and image size
  auto& m_alloc = ContextAware::getAlloc();
  auto m_size = ContextAware::getSize();

  // Create a temporary buffer to hold the output pixels of the image
  VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT};
  VkDeviceSize bufferSize = 4 * sizeof(float) * m_size.width * m_size.height;
  nvvk::Buffer pixelBuffer = m_alloc.createBuffer(
      bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  nvvk::CommandPool genCmdBuf(ContextAware::getDevice(),
                              ContextAware::getQueueFamily());

  // Multi-view rendering
  int shotsNum = m_scene.getShotsNum();
  for (int shotId = 0; shotId < shotsNum; shotId++) {
    // Set camera pose and state of pipelines
    m_scene.setShot(shotId);

    // Main loop of single image rendering
    int tot = m_scene.getPipelineState().rtxState.spp;
    // Still procedural rendering, but in offscreen this time
    m_pipelineRaytrace.setSpp(1);
    m_pipelineRaytrace.resetFrame();

    // Progress bar
    tqdm bar;
    bar.set_theme_arrow();

    for (int spp = 0; spp < tot; spp++) {
      bar.progress(spp, tot);
      const VkCommandBuffer& cmdBuf = genCmdBuf.createCommandBuffer();

      // Update camera and sunsky
      m_pipelineGraphics.run(cmdBuf);

      // Ray tracing and do not render gui
      m_pipelineRaytrace.run(cmdBuf);

      // Only post-processing in the last pass since
      // we do not care the intermediate result in offline mode
      if (spp == tot - 1) {
        VkRenderPassBeginInfo postRenderPassBeginInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        postRenderPassBeginInfo.clearValueCount = 2;
        postRenderPassBeginInfo.pClearValues = clearValues.data();
        postRenderPassBeginInfo.renderPass = ContextAware::getRenderPass();
        postRenderPassBeginInfo.framebuffer = ContextAware::getFramebuffer();
        postRenderPassBeginInfo.renderArea = {{0, 0}, ContextAware::getSize()};
        vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);
        m_pipelinePost.run(cmdBuf);
        vkCmdEndRenderPass(cmdBuf);
      }
      genCmdBuf.submitAndWait(cmdBuf);
    }
    bar.finish();
    vkDeviceWaitIdle(ContextAware::getDevice());

    // Save image
    static char outputName[200];
    // sprintf(outputName, "%s_shot_%04d.png", m_tis.outputname.c_str(),
    // shotId); saveBufferToImage(pixelBuffer, outputName); sprintf(outputName,
    // "%s_shot_%04d.exr", m_tis.outputname.c_str(), shotId);
    // saveBufferToImage(pixelBuffer, outputName, 0);
    // sprintf(outputName, "%s_shot_%04d_channel_1.exr",
    // m_tis.outputname.c_str(), shotId); saveBufferToImage(pixelBuffer,
    // outputName, 1);
    //for (int channelId = 1; channelId < 6; channelId++) {
    //  sprintf(outputName, "%s_shot_%04d_channel_%04d.exr",
    //          m_tis.outputname.c_str(), shotId, channelId);
    //  saveBufferToImage(pixelBuffer, outputName, channelId);
    //}
    sprintf(outputName, "%s_shot_%04d_channel_%04d.exr",
            m_tis.outputname.c_str(), shotId, 6);
    saveBufferToImage(pixelBuffer, outputName, 6);
  }
  // Destroy temporary buffer
  m_alloc.destroy(pixelBuffer);
}

void Tracer::parallelLoading() {
  // Load resources into scene
  Loader().loadSceneFromJson(m_tis.scenefile, ContextAware::getRoot(),
                             &m_scene);

  // Create graphics pipeline
  m_pipelineGraphics.init(reinterpret_cast<ContextAware*>(this), &m_scene);

  // Raytrace pipeline use some resources from graphics pipeline
  PipelineRaytraceInitSetting pis;
  pis.pDswOut = &m_pipelineGraphics.getOutDescriptorSet();
  pis.pDswEnv = &m_pipelineGraphics.getEnvDescriptorSet();
  pis.pDswScene = &m_pipelineGraphics.getSceneDescriptorSet();
  m_pipelineRaytrace.init(reinterpret_cast<ContextAware*>(this), &m_scene, pis);

  // Post pipeline processes hdr output
  m_pipelinePost.init(reinterpret_cast<ContextAware*>(this), &m_scene,
                      &m_pipelineGraphics.getHdrOutImageInfo());
}

void Tracer::vkTextureToBuffer(const nvvk::Texture& imgIn,
                               const VkBuffer& pixelBufferOut) {
  nvvk::CommandPool genCmdBuf(ContextAware::getDevice(),
                              ContextAware::getQueueFamily());
  VkCommandBuffer cmdBuf = genCmdBuf.createCommandBuffer();

  // Make the image layout eTransferSrcOptimal to copy to buffer
  nvvk::cmdBarrierImageLayout(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT);

  // Copy the image to the buffer
  VkBufferImageCopy copyRegion;
  copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copyRegion.imageExtent = {ContextAware::getSize().width,
                            ContextAware::getSize().height, 1};
  copyRegion.imageOffset = {0};
  copyRegion.bufferOffset = 0;
  copyRegion.bufferImageHeight = ContextAware::getSize().height;
  copyRegion.bufferRowLength = ContextAware::getSize().width;
  vkCmdCopyImageToBuffer(cmdBuf, imgIn.image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pixelBufferOut,
                         1, &copyRegion);

  // Put back the image as it was
  nvvk::cmdBarrierImageLayout(
      cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
  genCmdBuf.submitAndWait(cmdBuf);
}

void Tracer::saveBufferToImage(nvvk::Buffer pixelBuffer, std::string outputpath,
                               int channelId) {
  auto fp = path(outputpath);
  bool isRelativePath = !fp.is_absolute();
  if (isRelativePath) outputpath = NVPSystem::exePath() + outputpath;

  auto& m_alloc = ContextAware::getAlloc();
  auto m_size = ContextAware::getSize();

  // Default framebuffer color after post processing
  if (channelId == -1)
    vkTextureToBuffer(ContextAware::getOfflineColor(), pixelBuffer.buffer);
  // Hdr channel before post processing
  else
    vkTextureToBuffer(m_pipelineGraphics.getColorTexture(channelId),
                      pixelBuffer.buffer);

  // Write the image to disk
  void* data = m_alloc.map(pixelBuffer);
  writeImage(outputpath.c_str(), m_size.width, m_size.height,
             reinterpret_cast<float*>(data));
  m_alloc.unmap(pixelBuffer);
}

void Tracer::submitWithTLSemaphore(const VkCommandBuffer& cmdBuf) {
  // Increment for signaling
  m_fenceValue++;

  VkCommandBufferSubmitInfoKHR cmdBufInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR};
  cmdBufInfo.commandBuffer = cmdBuf;

  VkSemaphoreSubmitInfoKHR waitSemaphore{
      VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR};
  waitSemaphore.semaphore = m_swapChain.getActiveReadSemaphore();
  waitSemaphore.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

#ifdef NVP_SUPPORTS_OPTIX7
  VkSemaphoreSubmitInfoKHR signalSemaphore{
      VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR};
  signalSemaphore.semaphore = m_denoiser.getTLSemaphore();
  signalSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
  signalSemaphore.value = m_fenceValue;
#endif  // NVP_SUPPORTS_OPTIX7

  VkSubmitInfo2KHR submits{VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR};
  submits.commandBufferInfoCount = 1;
  submits.pCommandBufferInfos = &cmdBufInfo;
  submits.waitSemaphoreInfoCount = 1;
  submits.pWaitSemaphoreInfos = &waitSemaphore;
#ifdef NVP_SUPPORTS_OPTIX7
  submits.signalSemaphoreInfoCount = 1;
  submits.pSignalSemaphoreInfos = &signalSemaphore;
#endif  // _DEBUG

  vkQueueSubmit2(m_queue, 1, &submits, {});
}

void Tracer::submitFrame(const VkCommandBuffer& cmdBuf) {
  uint32_t imageIndex = m_swapChain.getActiveImageIndex();
  VkFence fence = m_waitFences[imageIndex];
  vkResetFences(m_device, 1, &fence);

  VkCommandBufferSubmitInfoKHR cmdBufInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR};
  cmdBufInfo.commandBuffer = cmdBuf;

#ifdef NVP_SUPPORTS_OPTIX7
  VkSemaphoreSubmitInfoKHR waitSemaphore{
      VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR};
  waitSemaphore.semaphore = m_denoiser.getTLSemaphore();
  waitSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
  waitSemaphore.value = m_fenceValue;
#endif  // NVP_SUPPORTS_OPTIX7

  VkSemaphoreSubmitInfoKHR signalSemaphore{
      VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR};
  signalSemaphore.semaphore = m_swapChain.getActiveWrittenSemaphore();
  signalSemaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

  VkSubmitInfo2KHR submits{VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR};
  submits.commandBufferInfoCount = 1;
  submits.pCommandBufferInfos = &cmdBufInfo;
#ifdef NVP_SUPPORTS_OPTIX7
  submits.waitSemaphoreInfoCount = 1;
  submits.pWaitSemaphoreInfos = &waitSemaphore;
#endif  // NVP_SUPPORTS_OPTIX7
  submits.signalSemaphoreInfoCount = 1;
  submits.pSignalSemaphoreInfos = &signalSemaphore;

  vkQueueSubmit2(m_queue, 1, &submits, fence);

  // Presenting frame
  m_swapChain.present(m_queue);
}

void Tracer::createGbuffers() {
  auto& m_alloc = ContextAware::getAlloc();
  auto& m_debug = ContextAware::getDebug();

  m_alloc.destroy(m_gAlbedo);
  m_alloc.destroy(m_gNormal);
  m_alloc.destroy(m_gDenoised);

  VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_STORAGE_BIT};
  VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  {  // Albedo RGBA8
    auto colorCreateInfo =
        nvvk::makeImage2DCreateInfo(m_size, VK_FORMAT_R8G8B8A8_UNORM, usage);
    nvvk::Image image = m_alloc.createImage(colorCreateInfo);
    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
    m_gAlbedo = m_alloc.createTexture(image, ivInfo, sampler);
    m_gAlbedo.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    NAME_VK(m_gAlbedo.image);
  }

  {  // Normal RGBA8
    auto colorCreateInfo =
        nvvk::makeImage2DCreateInfo(m_size, VK_FORMAT_R8G8B8A8_UNORM, usage);
    nvvk::Image image = m_alloc.createImage(colorCreateInfo);
    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
    m_gNormal = m_alloc.createTexture(image, ivInfo, sampler);
    m_gNormal.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    NAME_VK(m_gNormal.image);
  }

  {  // Denoised RGBA32
    auto colorCreateInfo = nvvk::makeImage2DCreateInfo(
        m_size, VK_FORMAT_R32G32B32A32_SFLOAT, usage);
    nvvk::Image image = m_alloc.createImage(colorCreateInfo);
    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
    m_gDenoised = m_alloc.createTexture(image, ivInfo, sampler);
    m_gDenoised.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    NAME_VK(m_gDenoised.image);
  }

  // Setting the image layout  to general
  {
    nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
    auto cmdBuf = genCmdBuf.createCommandBuffer();
    nvvk::cmdBarrierImageLayout(cmdBuf, m_gAlbedo.image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_gNormal.image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_gDenoised.image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL);
    genCmdBuf.submitAndWait(cmdBuf);
  }
}

void Tracer::denoise() {
  if (needToDenoise()) {
#ifdef NVP_SUPPORTS_OPTIX7
    m_denoiser.denoiseImageBuffer(m_fenceValue);
#endif  // NVP_SUPPORTS_OPTIX7
  }
}

void Tracer::setImageToDisplay() {
  auto curFrame = m_scene.getPipelineState().rtxState.curFrame;
  bool showDenoised = m_denoiseApply && (curFrame >= m_denoiseEveryNFrames ||
                                         m_denoiseFirstFrame);
  m_pipelinePost.updatePostDescriptorSet(
      showDenoised ? &m_gDenoised.descriptor
                   : &m_pipelineGraphics.getHdrOutImageInfo());
}

bool Tracer::needToDenoise() {
  if (m_denoiseApply && !m_busy) {
    auto curFrame = m_scene.getPipelineState().rtxState.curFrame;
    if (m_denoiseFirstFrame && curFrame == 0) return true;
    if (curFrame % m_denoiseEveryNFrames == 0) return true;
  }
  return false;
}

void Tracer::copyImagesToCuda(const VkCommandBuffer& cmdBuf) {
  if (needToDenoise()) {
#ifdef NVP_SUPPORTS_OPTIX7
    m_denoiser.imageToBuffer(cmdBuf, {m_pipelineGraphics.getColorTexture(0),
                                      m_pipelineGraphics.getColorTexture(1),
                                      m_pipelineGraphics.getColorTexture(2)});
#endif  // NVP_SUPPORTS_OPTIX7
  }
}

void Tracer::copyCudaImagesToVulkan(const VkCommandBuffer& cmdBuf) {
  if (needToDenoise()) {
#ifdef NVP_SUPPORTS_OPTIX7
    m_denoiser.bufferToImage(cmdBuf, &m_gDenoised);
#endif  // NVP_SUPPORTS_OPTIX7
  }
}
