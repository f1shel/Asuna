#include "tracer.h"
#include <loader/loader.h>

#include <backends/imgui_impl_glfw.h>
#include <nvh/timesampler.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/structs_vk.hpp>
#include <ext/tqdm.h>

#include <filesystem>
#include <iostream>

using std::filesystem::path;

void Tracer::init(TracerInitSettings tis) {
  m_tis = tis;

  // Get film size and set size for context
  auto filmResolution =
      Loader().loadSizeFirst(m_tis.scenefile, m_context.getRoot());
  m_context.setSize(filmResolution);
  if (tis.sceneSpp != 0) m_scene.setSpp(tis.sceneSpp);

  // Initialize context and set context pointer for scene
  m_context.init({m_tis.offline});
  m_scene.init(&m_context);

  if (m_context.getOfflineMode())
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
  if (m_context.getOfflineMode())
    runOffline();
  else
    runOnline();
}

void Tracer::deinit() {
  m_pipelineGraphics.deinit();
  m_pipelineRaytrace.deinit();
  m_pipelinePost.deinit();
  m_scene.deinit();
  m_context.deinit();
}

void Tracer::runOnline() {
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
  clearValues[1].depthStencil = {1.0f, 0};

  // Main loop
  while (!m_context.shouldGlfwCloseWindow()) {
    glfwPollEvents();
    if (m_context.isMinimized()) continue;

    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Acquire swap chain
    m_context.prepareFrame();

    // Start command buffer of this frame
    uint32_t curFrame = m_context.getCurFrame();
    const VkCommandBuffer& cmdBuf = m_context.getCommandBuffers()[curFrame];
    VkCommandBufferBeginInfo beginInfo = nvvk::make<VkCommandBufferBeginInfo>();
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    {
      vkBeginCommandBuffer(cmdBuf, &beginInfo);

      renderGUI();

      do {
        if (m_busy) break;

        // For procedural rendering
        m_pipelineRaytrace.setSpp(1);

        // Update camera and sunsky
        m_pipelineGraphics.run(cmdBuf);

        // Ray tracing
        m_pipelineRaytrace.run(cmdBuf);
      } while (0);

      // Post processing
      {
        VkRenderPassBeginInfo postRenderPassBeginInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        postRenderPassBeginInfo.clearValueCount = 2;
        postRenderPassBeginInfo.pClearValues = clearValues.data();
        postRenderPassBeginInfo.renderPass = m_context.getRenderPass();
        postRenderPassBeginInfo.framebuffer =
            m_context.getFramebuffer(curFrame);
        postRenderPassBeginInfo.renderArea = {{0, 0}, m_context.getSize()};

        // Rendering to the swapchain framebuffer the rendered image and
        // apply a tonemapper
        vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        if (!m_busy) m_pipelinePost.run(cmdBuf);

        // Rendering UI
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

        // Display axis in the lower left corner.
        // vkAxis.display(cmdBuf, CameraManip.getMatrix(),
        // vkSample.getSize());

        vkCmdEndRenderPass(cmdBuf);
      }
    }
    vkEndCommandBuffer(cmdBuf);
    m_context.submitFrame();
  }
  vkDeviceWaitIdle(m_context.getDevice());
}

void Tracer::runOffline() {
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
  clearValues[1].depthStencil = {1.0f, 0};

  // Vulkan allocator and image size
  auto& m_alloc = m_context.getAlloc();
  auto m_size = m_context.getSize();

  // Create a temporary buffer to hold the output pixels of the image
  VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT};
  VkDeviceSize bufferSize = 4 * sizeof(float) * m_size.width * m_size.height;
  nvvk::Buffer pixelBuffer = m_alloc.createBuffer(
      bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  nvvk::CommandPool genCmdBuf(m_context.getDevice(),
                              m_context.getQueueFamily());

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
        postRenderPassBeginInfo.renderPass = m_context.getRenderPass();
        postRenderPassBeginInfo.framebuffer = m_context.getFramebuffer();
        postRenderPassBeginInfo.renderArea = {{0, 0}, m_context.getSize()};
        vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);
        m_pipelinePost.run(cmdBuf);
        vkCmdEndRenderPass(cmdBuf);
      }
      genCmdBuf.submitAndWait(cmdBuf);
    }
    bar.finish();
    vkDeviceWaitIdle(m_context.getDevice());

    // Save image
    static char outputName[50];
    sprintf(outputName, "%s_shot_%04d.png", m_tis.outputname.c_str(), shotId);
    saveBufferToImage(pixelBuffer, outputName);
    sprintf(outputName, "%s_shot_%04d.exr", m_tis.outputname.c_str(), shotId);
    saveBufferToImage(pixelBuffer, outputName, 0);
  }
  // Destroy temporary buffer
  m_alloc.destroy(pixelBuffer);
}

void Tracer::parallelLoading() {
  // Load resources into scene
  Loader().loadSceneFromJson(m_tis.scenefile, m_context.getRoot(), &m_scene);

  // Create graphics pipeline
  m_pipelineGraphics.init(&m_context, &m_scene);

  // Raytrace pipeline use some resources from graphics pipeline
  PipelineRaytraceInitSetting pis;
  pis.pDswOut = &m_pipelineGraphics.getOutDescriptorSet();
  pis.pDswEnv = &m_pipelineGraphics.getEnvDescriptorSet();
  pis.pDswScene = &m_pipelineGraphics.getSceneDescriptorSet();
  m_pipelineRaytrace.init(&m_context, &m_scene, pis);

  // Post pipeline processes hdr output
  m_pipelinePost.init(&m_context, &m_scene,
                      &m_pipelineGraphics.getHdrOutImageInfo());
}

void Tracer::vkTextureToBuffer(const nvvk::Texture& imgIn,
                               const VkBuffer& pixelBufferOut) {
  nvvk::CommandPool genCmdBuf(m_context.getDevice(),
                              m_context.getQueueFamily());
  VkCommandBuffer cmdBuf = genCmdBuf.createCommandBuffer();

  // Make the image layout eTransferSrcOptimal to copy to buffer
  nvvk::cmdBarrierImageLayout(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT);

  // Copy the image to the buffer
  VkBufferImageCopy copyRegion;
  copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copyRegion.imageExtent = {m_context.getSize().width,
                            m_context.getSize().height, 1};
  copyRegion.imageOffset = {0};
  copyRegion.bufferOffset = 0;
  copyRegion.bufferImageHeight = m_context.getSize().height;
  copyRegion.bufferRowLength = m_context.getSize().width;
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
  bool isRelativePath = fp.is_relative();
  if (isRelativePath) outputpath = NVPSystem::exePath() + outputpath;

  auto& m_alloc = m_context.getAlloc();
  auto m_size = m_context.getSize();

  // Default framebuffer color after post processing
  if (channelId == -1)
    vkTextureToBuffer(m_context.getOfflineColor(), pixelBuffer.buffer);
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

void Tracer::renderGUI() {
  static bool showGui = true;

  if (m_busy) {
    guiBusy();
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_H)) showGui = !showGui;
  if (!showGui) return;

  // Show UI panel window.
  float panelAlpha = 1.0f;
  ImGuiH::Control::style.ctrlPerc = 0.55f;
  ImGuiH::Panel::Begin(ImGuiH::Panel::Side::Right, panelAlpha);

  bool changed{false};

  if (ImGui::CollapsingHeader("Camera" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiCamera();
  if (ImGui::CollapsingHeader(
          "Environment" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiEnvironment();
  if (ImGui::CollapsingHeader(
          "PathTracer" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiPathTracer();
  if (ImGui::CollapsingHeader(
          "Tonemapper" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiTonemapper();

  ImGui::End();  // ImGui::Panel::end()

  if (changed) {
    m_pipelineRaytrace.resetFrame();
  }
}