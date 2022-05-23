#include "tracer.h"
#include <loader/loader.h>

#include <backends/imgui_impl_glfw.h>
#include <nvh/timesampler.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/structs_vk.hpp>
#include <imgui_helper.h>
#include <imgui_orient.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <ext/tqdm.h>

#include <bitset>  // std::bitset
#include <filesystem>
#include <iostream>

using GuiH = ImGuiH::Control;
using std::filesystem::path;

void Tracer::init(TracerInitSettings tis)
{
  m_tis = tis;

  m_context.init({m_tis.offline});

  m_scene.init(&m_context);

  Loader loader(&m_scene);
  loader.loadSceneFromJson(m_tis.scenefile, m_context.getRoot());
  if(tis.sceneSpp != 0)
    m_scene.setSpp(tis.sceneSpp);

  m_context.setSize(m_scene.getSize());
  if(!m_context.getOfflineMode())
    m_context.resizeGlfwWindow();
  else
    m_context.createOfflineResources();

  PipelineRaytraceInitState pis;
  pis.pDswOut   = &m_pipelineGraphics.getOutDescriptorSet();
  pis.pDswEnv   = &m_pipelineGraphics.getEnvDescriptorSet();
  pis.pDswScene = &m_pipelineGraphics.getSceneDescriptorSet();

  m_pipelineGraphics.init(&m_context, &m_scene);
  m_pipelineRaytrace.init(&m_context, &m_scene, pis);
  m_pipelinePost.init(&m_context, &m_scene, &m_pipelineGraphics.getHdrOutImageInfo());
}

void Tracer::run()
{
  if(m_context.getOfflineMode())
    runOffline();
  else
    runOnline();
}

void Tracer::deinit()
{
  m_pipelineGraphics.deinit();
  m_pipelineRaytrace.deinit();
  m_pipelinePost.deinit();
  m_scene.deinit();
  m_context.deinit();
}

void Tracer::resetFrame() {}

void Tracer::runOnline()
{
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color        = {0.0f, 0.0f, 0.0f, 0.0f};
  clearValues[1].depthStencil = {1.0f, 0};
  m_pipelineRaytrace.setSpp(1);
  // Main loop
  while(!m_context.shouldGlfwCloseWindow())
  {
    glfwPollEvents();
    if(m_context.isMinimized())
      continue;
    // Start the Dear ImGui frame
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // Start rendering the scene
    m_context.prepareFrame();
    // Start command buffer of this frame
    uint32_t                 curFrame  = m_context.getCurFrame();
    const VkCommandBuffer&   cmdBuf    = m_context.getCommandBuffers()[curFrame];
    VkCommandBufferBeginInfo beginInfo = nvvk::make<VkCommandBufferBeginInfo>();
    beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    {
      vkBeginCommandBuffer(cmdBuf, &beginInfo);
      {
        m_pipelineGraphics.run(cmdBuf);
      }
      {
        renderGUI();
      }
      {
        m_pipelineRaytrace.run(cmdBuf);
      }
      {
        VkRenderPassBeginInfo postRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        postRenderPassBeginInfo.clearValueCount = 2;
        postRenderPassBeginInfo.pClearValues    = clearValues.data();
        postRenderPassBeginInfo.renderPass      = m_context.getRenderPass();
        postRenderPassBeginInfo.framebuffer     = m_context.getFramebuffer(curFrame);
        postRenderPassBeginInfo.renderArea      = {{0, 0}, m_context.getSize()};

        // Rendering to the swapchain framebuffer the rendered image and apply a
        // tonemapper
        vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        m_pipelinePost.run(cmdBuf);

        // Rendering UI
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

        // Display axis in the lower left corner.
        // vkAxis.display(cmdBuf, CameraManip.getMatrix(), vkSample.getSize());

        vkCmdEndRenderPass(cmdBuf);
      }
    }
    vkEndCommandBuffer(cmdBuf);
    m_context.submitFrame();
  }
  vkDeviceWaitIdle(m_context.getDevice());
}

void Tracer::runOffline()
{
  std::array<VkClearValue, 2> clearValues{};
  clearValues[0].color        = {0.0f, 0.0f, 0.0f, 0.0f};
  clearValues[1].depthStencil = {1.0f, 0};

  nvvk::CommandPool genCmdBuf(m_context.getDevice(), m_context.getQueueFamily());

  // Main loop
  tqdm bar;
  int  tot         = m_scene.getSpp();
  int  sppPerRound = 1;
  m_pipelineRaytrace.setSpp(sppPerRound);
  for(int spp = 0; spp < tot; spp += std::min(sppPerRound, tot - spp))
  {
    bar.progress(spp, tot);
    const VkCommandBuffer& cmdBuf = genCmdBuf.createCommandBuffer();
    {
      m_pipelineGraphics.run(cmdBuf);
    }
    {
      m_pipelineRaytrace.run(cmdBuf);
    }
    if(spp == tot - 1)
    {
      VkRenderPassBeginInfo postRenderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
      postRenderPassBeginInfo.clearValueCount = 2;
      postRenderPassBeginInfo.pClearValues    = clearValues.data();
      postRenderPassBeginInfo.renderPass      = m_context.getRenderPass();
      postRenderPassBeginInfo.framebuffer     = m_context.getFramebuffer();
      postRenderPassBeginInfo.renderArea      = {{0, 0}, m_context.getSize()};

      vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      m_pipelinePost.run(cmdBuf);

      vkCmdEndRenderPass(cmdBuf);
    }
    genCmdBuf.submitAndWait(cmdBuf);
  }
  bar.finish();
  vkDeviceWaitIdle(m_context.getDevice());
  saveImageTest();
}

void Tracer::imageToBuffer(const nvvk::Texture& imgIn, const VkBuffer& pixelBufferOut)
{
  nvvk::CommandPool genCmdBuf(m_context.getDevice(), m_context.getQueueFamily());
  VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

  // Make the image layout eTransferSrcOptimal to copy to buffer
  nvvk::cmdBarrierImageLayout(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT);

  // Copy the image to the buffer
  VkBufferImageCopy copyRegion;
  copyRegion.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copyRegion.imageExtent       = {m_context.getSize().width, m_context.getSize().height, 1};
  copyRegion.imageOffset       = {0};
  copyRegion.bufferOffset      = 0;
  copyRegion.bufferImageHeight = m_context.getSize().height;
  copyRegion.bufferRowLength   = m_context.getSize().width;
  vkCmdCopyImageToBuffer(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pixelBufferOut, 1, &copyRegion);

  // Put back the image as it was
  nvvk::cmdBarrierImageLayout(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_ASPECT_COLOR_BIT);
  genCmdBuf.submitAndWait(cmdBuf);
}

void Tracer::saveImageTest()
{
  auto& m_alloc = m_context.getAlloc();
  auto  m_size  = m_context.getSize();

  // Create a temporary buffer to hold the pixels of the image
  VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
  VkDeviceSize       bufferSize  = 4 * sizeof(float) * m_size.width * m_size.height;
  nvvk::Buffer       pixelBuffer = m_alloc.createBuffer(bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

  saveImage(pixelBuffer, m_tis.outputname);
  //saveImage(pixelBuffer, "channel0.hdr", 0);
  // saveImage(pixelBuffer, "channel1.hdr", 1);
  // saveImage(pixelBuffer, "channel2.hdr", 2);
  // saveImage(pixelBuffer, "channel3.hdr", 3);

  // Destroy temporary buffer
  m_alloc.destroy(pixelBuffer);
}

void Tracer::saveImage(nvvk::Buffer pixelBuffer, std::string outputpath, int channelId)
{
  bool isRelativePath = path(outputpath).is_relative();
  if(isRelativePath)
    outputpath = NVPSystem::exePath() + outputpath;

  auto& m_alloc = m_context.getAlloc();
  auto  m_size  = m_context.getSize();

  if(channelId == -1)
    imageToBuffer(m_context.getOfflineColor(), pixelBuffer.buffer);
  else
    imageToBuffer(m_pipelineGraphics.getColorTexture(channelId), pixelBuffer.buffer);

  // Write the buffer to disk
  void* data   = m_alloc.map(pixelBuffer);
  int   result = stbi_write_hdr(outputpath.c_str(), m_size.width, m_size.height, 4, reinterpret_cast<float*>(data));
  m_alloc.unmap(pixelBuffer);
}

void Tracer::renderGUI()
{
  static bool showGui = true;
  if(ImGui::IsKeyPressed(ImGuiKey_H))
    showGui = !showGui;
  if(!showGui)
    return;

  // Show UI panel window.
  float panelAlpha                = 1.0f;
  ImGuiH::Control::style.ctrlPerc = 0.55f;
  ImGuiH::Panel::Begin(ImGuiH::Panel::Side::Right, panelAlpha);

  bool changed{false};

  if(ImGui::CollapsingHeader("Camera" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiCamera();
  if(ImGui::CollapsingHeader("Environment" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiEnvironment();
  if(ImGui::CollapsingHeader("PathTracer" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiPathTracer();
  if(ImGui::CollapsingHeader("Tonemapper" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
    changed |= guiTonemapper();

  ImGui::End();  // ImGui::Panel::end()

  if(changed)
  {
    m_pipelineRaytrace.resetFrame();
  }
}

bool Tracer::guiCamera()
{
  static GpuCamera dc{
      nvmath::mat4f_zero,        // rasterToCamera
      nvmath::mat4f_zero,        // cameraToWorld
      vec4(0.f, 0.f, 0.f, 0.f),  // fxfycxcy
      CameraTypeUndefined,       // type
      0.f,                       // aperture
      0.1f,                      // focal distance
  };

  bool changed{false};
  changed |= ImGuiH::CameraWidget();
  auto camType = m_scene.getCameraType();
  if(camType == CameraTypePerspective)
  {
    auto pCamera = static_cast<CameraPerspective*>(&m_scene.getCamera());
    changed |= GuiH::Group<bool>("Perspective", true, [&] {
      changed |= GuiH::Slider("Focal distance", "", &pCamera->getFocalDistance(), &dc.focalDistance,
                              GuiH::Flags::Normal, 0.01f, 10.f);
      changed |= GuiH::Slider("Aperture", "", &pCamera->getAperture(), &dc.aperture, GuiH::Flags::Normal, 0.f, 1.f);
      return changed;
    });
  }
  return changed;
}

bool Tracer::guiEnvironment()
{
  static GpuSunAndSky dss{
      {1, 1, 1},            // rgb_unit_conversion;
      0.0000101320f,        // multiplier;
      0.0f,                 // haze;
      0.0f,                 // redblueshift;
      1.0f,                 // saturation;
      0.0f,                 // horizon_height;
      {0.4f, 0.4f, 0.4f},   // ground_color;
      0.1f,                 // horizon_blur;
      {0.0, 0.0, 0.01f},    // night_color;
      0.8f,                 // sun_disk_intensity;
      {0.00, 0.78, 0.62f},  // sun_direction;
      5.0f,                 // sun_disk_scale;
      1.0f,                 // sun_glow_intensity;
      1,                    // y_is_up;
      1,                    // physically_scaled_sun;
      0,                    // in_use;
  };

  bool  changed{false};
  auto& sunAndSky(m_scene.getSunsky());

  changed |= ImGui::Checkbox("Use Sun & Sky", (bool*)&sunAndSky.in_use);

  // Adjusting the up with the camera
  nvmath::vec3f eye, center, up;
  CameraManip.getLookat(eye, center, up);
  sunAndSky.y_is_up = (up.y == 1);

  if(sunAndSky.in_use)
  {
    GuiH::Group<bool>("Sun", true, [&] {
      changed |= GuiH::Custom("Direction", "Sun Direction", [&] {
        float indent = ImGui::GetCursorPos().x;
        changed |= ImGui::DirectionGizmo("", &sunAndSky.sun_direction.x, true);
        ImGui::NewLine();
        ImGui::SameLine(indent);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        changed |= ImGui::InputFloat3("##IG", &sunAndSky.sun_direction.x);
        return changed;
      });
      changed |= GuiH::Slider("Disk Scale", "", &sunAndSky.sun_disk_scale, &dss.sun_disk_scale, GuiH::Flags::Normal, 0.f, 100.f);
      changed |= GuiH::Slider("Glow Intensity", "", &sunAndSky.sun_glow_intensity, &dss.sun_glow_intensity,
                              GuiH::Flags::Normal, 0.f, 5.f);
      changed |= GuiH::Slider("Disk Intensity", "", &sunAndSky.sun_disk_intensity, &dss.sun_disk_intensity,
                              GuiH::Flags::Normal, 0.f, 5.f);
      changed |= GuiH::Color("Night Color", "", &sunAndSky.night_color.x, &dss.night_color.x, GuiH::Flags::Normal);
      return changed;
    });

    GuiH::Group<bool>("Ground", true, [&] {
      changed |= GuiH::Slider("Horizon Height", "", &sunAndSky.horizon_height, &dss.horizon_height, GuiH::Flags::Normal, -1.f, 1.f);
      changed |= GuiH::Slider("Horizon Blur", "", &sunAndSky.horizon_blur, &dss.horizon_blur, GuiH::Flags::Normal, 0.f, 1.f);
      changed |= GuiH::Color("Ground Color", "", &sunAndSky.ground_color.x, &dss.ground_color.x, GuiH::Flags::Normal);
      changed |= GuiH::Slider("Haze", "", &sunAndSky.haze, &dss.haze, GuiH::Flags::Normal, 0.f, 15.f);
      return changed;
    });

    GuiH::Group<bool>("Other", false, [&] {
      changed |= GuiH::Drag("Multiplier", "", &sunAndSky.multiplier, &dss.multiplier, GuiH::Flags::Normal, 0.f,
                            std::numeric_limits<float>::max(), 2, "%5.5f");
      changed |= GuiH::Slider("Saturation", "", &sunAndSky.saturation, &dss.saturation, GuiH::Flags::Normal, 0.f, 1.f);
      changed |= GuiH::Slider("Red Blue Shift", "", &sunAndSky.redblueshift, &dss.redblueshift, GuiH::Flags::Normal, -1.f, 1.f);
      changed |= GuiH::Color("RGB Conversion", "", &sunAndSky.rgb_unit_conversion.x, &dss.rgb_unit_conversion.x, GuiH::Flags::Normal);

      nvmath::vec3f eye, center, up;
      CameraManip.getLookat(eye, center, up);
      sunAndSky.y_is_up = up.y == 1;
      changed |= GuiH::Checkbox("Y is Up", "", (bool*)&sunAndSky.y_is_up, nullptr, GuiH::Flags::Disabled);
      return changed;
    });
  }

  return changed;
}

bool Tracer::guiTonemapper()
{
  static GpuPushConstantPost default_tm{
      1.0f,          // brightness;
      1.0f,          // contrast;
      1.0f,          // saturation;
      0.0f,          // vignette;
      1.0f,          // avgLum;
      1.0f,          // zoom;
      {1.0f, 1.0f},  // renderingRatio;
      0,             // autoExposure;
      0.5f,          // Ywhite;  // Burning white
      0.5f,          // key;     // Log-average luminance
      0,             // toneMappingType
  };
  static vector<const char*> ToneMappingTypeList = {"None", "Gamma", "Reinhard", "Aces", "Filmic", "Pbrt", "Custom"};

  auto& tm = m_pipelinePost.getPushconstant();
  bool  changed{false};

  changed |= ImGui::Combo("EnvMaps", (int*)&tm.tmType, ToneMappingTypeList.data(), ToneMappingTypeList.size());

  if(tm.tmType == ToneMappingTypeCustom)
  {
    std::bitset<8> b(tm.autoExposure);
    bool           autoExposure = b.test(0);
    changed |= GuiH::Checkbox("Auto Exposure", "Adjust exposure", (bool*)&autoExposure);
    changed |= GuiH::Slider("Exposure", "Scene Exposure", &tm.avgLum, &default_tm.avgLum, GuiH::Flags::Normal, 0.001f, 5.00f);
    changed |= GuiH::Slider("Brightness", "", &tm.brightness, &default_tm.brightness, GuiH::Flags::Normal, 0.0f, 2.0f);
    changed |= GuiH::Slider("Contrast", "", &tm.contrast, &default_tm.contrast, GuiH::Flags::Normal, 0.0f, 5.0f);
    changed |= GuiH::Slider("Saturation", "", &tm.saturation, &default_tm.saturation, GuiH::Flags::Normal, 0.0f, 5.0f);
    changed |= GuiH::Slider("Vignette", "", &tm.vignette, &default_tm.vignette, GuiH::Flags::Normal, 0.0f, 2.0f);

    if(autoExposure)
    {
      bool localExposure = b.test(1);
      GuiH::Group<bool>("Auto Settings", true, [&] {
        changed |= GuiH::Checkbox("Local", "", &localExposure);
        changed |= GuiH::Slider("Burning White", "", &tm.Ywhite, &default_tm.Ywhite, GuiH::Flags::Normal, 0.0f, 1.0f);
        changed |= GuiH::Slider("Brightness", "", &tm.key, &default_tm.key, GuiH::Flags::Normal, 0.0f, 1.0f);
        b.set(1, localExposure);
        return changed;
      });
    }
    b.set(0, autoExposure);
    tm.autoExposure = b.to_ulong();
  }


  return false;  // no need to restart the renderer
}

bool Tracer::guiPathTracer()
{
  bool  changed = false;
  auto& pc      = m_pipelineRaytrace.getPushconstant();
  changed |= ImGui::Checkbox("Use Face Normal", (bool*)&pc.useFaceNormal);
  changed |= ImGui::Checkbox("Ignore Emissive", (bool*)&pc.ignoreEmissive);
  return changed;
}