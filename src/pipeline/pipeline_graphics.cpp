#include "pipeline_graphics.h"
#include <shared/camera.h>
#include <shared/pushconstant.h>
#include <shared/vertex.h>

#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/commands_vk.hpp>
#include <nvvk/pipeline_vk.hpp>
#include <nvvk/structs_vk.hpp>
#include "nvvk/images_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"

#include <cstdint>
#include <vector>

void PipelineGraphics::init(ContextAware* pContext, Scene* pScene) {
  m_pContext = pContext;
  m_pScene = pScene;
  // Graphics pipeine
  nvh::Stopwatch sw_;
  createOffscreenResources();
  createGraphicsDescriptorSetLayout();
  createCameraBuffer();
  updateGraphicsDescriptorSet();
  LOGI("[ ] %-20s: %6.2fms Graphic pipeline creation\n", "Pipeline",
       sw_.elapsed());
}

void PipelineGraphics::run(const VkCommandBuffer& cmdBuf) {
  updateCameraBuffer(cmdBuf);
  updateSunAndSky(cmdBuf);
}

void PipelineGraphics::deinit() {
  auto& m_alloc = m_pContext->getAlloc();
  auto m_device = m_pContext->getDevice();
  auto m_cmdPool = m_pContext->getCommandPool();

  for (auto& m_tColor : m_tColors) m_alloc.destroy(m_tColor);
  m_alloc.destroy(m_tDepth);
  m_alloc.destroy(m_bCamera);
  vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
  vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);
  m_offscreenRenderPass = VK_NULL_HANDLE;
  m_offscreenFramebuffer = VK_NULL_HANDLE;

  PipelineAware::deinit();
}

void PipelineGraphics::updateCameraBuffer(const VkCommandBuffer& cmdBuf) {
  // Upload updated camera information to GPU
  // Prepare new UBO contents on host.
  auto m_size = m_pContext->getSize();
  const float aspectRatio = m_size.width / static_cast<float>(m_size.height);
  GpuCamera hostCamera = {};
  // proj[1][1] *= -1;  // Inverting Y for Vulkan (not needed with
  // perspectiveVK).

  auto& cam = m_pScene->getCamera();
  hostCamera = cam.toGpuStruct();

  // UBO on the device, and what stages access it.
  VkBuffer deviceUBO = m_bCamera.buffer;
  auto uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

  if (!m_pContext->getOfflineMode()) {
    // Ensure that the modified UBO is not visible to previous frames.
    VkBufferMemoryBarrier beforeBarrier{
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    beforeBarrier.buffer = deviceUBO;
    beforeBarrier.offset = 0;
    beforeBarrier.size = sizeof(GpuCamera);
    vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 1,
                         &beforeBarrier, 0, nullptr);
  }

  // Schedule the host-to-device upload. (hostUBO is copied into the cmd
  // buffer so it is okay to deallocate when the function returns).
  vkCmdUpdateBuffer(cmdBuf, m_bCamera.buffer, 0, sizeof(GpuCamera),
                    &hostCamera);

  if (!m_pContext->getOfflineMode()) {
    // Making sure the updated UBO will be visible.
    VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    afterBarrier.buffer = deviceUBO;
    afterBarrier.offset = 0;
    afterBarrier.size = sizeof(hostCamera);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages,
                         VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 1,
                         &afterBarrier, 0, nullptr);
  }
}

void PipelineGraphics::updateSunAndSky(const VkCommandBuffer& cmdBuf) {
  vkCmdUpdateBuffer(cmdBuf, m_pScene->getSunskyDescriptor(), 0,
                    sizeof(GpuSunAndSky), &m_pScene->getSunsky());
}

void PipelineGraphics::createOffscreenResources() {
  auto& m_alloc = m_pContext->getAlloc();
  auto& m_debug = m_pContext->getDebug();
  auto m_device = m_pContext->getDevice();
  auto m_physicalDevice = m_pContext->getPhysicalDevice();
  auto m_size = m_pContext->getSize();
  auto m_graphicsQueueIndex = m_pContext->getQueueFamily();

  for (auto& m_tColor : m_tColors) m_alloc.destroy(m_tColor);
  m_tColors.clear();
  m_alloc.destroy(m_tDepth);
  vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
  vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);
  m_offscreenRenderPass = VK_NULL_HANDLE;
  m_offscreenFramebuffer = VK_NULL_HANDLE;

  VkFormat colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  VkFormat depthFormat = nvvk::findDepthFormat(m_physicalDevice);

  // Creating the color image
  {
    for (uint channelId = 0; channelId < NUM_OUTPUT_IMAGES; channelId++) {
      VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
      auto colorCreateInfo = nvvk::makeImage2DCreateInfo(
          m_size, colorFormat,
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
              VK_IMAGE_USAGE_STORAGE_BIT);
      nvvk::Image image = m_alloc.createImage(colorCreateInfo);
      VkImageViewCreateInfo ivInfo =
          nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
      auto texture = m_alloc.createTexture(image, ivInfo, sampler);
      texture.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      m_tColors.emplace_back(texture);
    }
  }

  // Creating the depth buffer
  auto depthCreateInfo = nvvk::makeImage2DCreateInfo(
      m_size, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  {
    nvvk::Image image = m_alloc.createImage(depthCreateInfo);
    NAME2_VK(image.image, "Offscreen Depth");

    VkImageViewCreateInfo depthStencilView{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format = depthFormat;
    depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthStencilView.image = image.image;

    m_tDepth = m_alloc.createTexture(image, depthStencilView);
  }

  // Setting the image layout for both color and depth
  {
    auto& qGCT1 = m_pContext->getParallelQueues()[0];
    nvvk::CommandPool cmdBufGet(m_pContext->getDevice(), qGCT1.familyIndex,
                                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                qGCT1.queue);
    auto cmdBuf = cmdBufGet.createCommandBuffer();
    for (auto& m_tColor : m_tColors)
      nvvk::cmdBarrierImageLayout(cmdBuf, m_tColor.image,
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(
        cmdBuf, m_tDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    cmdBufGet.submitAndWait(cmdBuf);
  }

  // Creating a renderpass for the offscreen
  m_offscreenRenderPass = nvvk::createRenderPass(
      m_device, {colorFormat}, depthFormat, 1, true, true,
      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
  NAME2_VK(m_offscreenRenderPass, "Offscreen RenderPass");

  // Creating the frame buffer for offscreen
  std::vector<VkImageView> attachments = {m_tColors[0].descriptor.imageView,
                                          m_tDepth.descriptor.imageView};

  VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  info.renderPass = m_offscreenRenderPass;
  info.attachmentCount = 2;
  info.pAttachments = attachments.data();
  info.width = m_size.width;
  info.height = m_size.height;
  info.layers = 1;
  vkCreateFramebuffer(m_device, &info, nullptr, &m_offscreenFramebuffer);
  NAME2_VK(m_offscreenFramebuffer, "Offscreen FrameBuffer");
}

void PipelineGraphics::createGraphicsDescriptorSetLayout() {
  auto m_device = m_pContext->getDevice();

  // Out Set: S_Out
  auto& outWrap = m_holdSetWrappers[uint(HoldSet::Out)];
  auto& outBind = outWrap.getDescriptorSetBindings();
  auto& outPool = outWrap.getDescriptorPool();
  auto& outSet = outWrap.getDescriptorSet();
  auto& outLayout = outWrap.getDescriptorSetLayout();
  // Storage images
  outBind.addBinding(OutputBindings::OutputStore,
                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, NUM_OUTPUT_IMAGES,
                     VK_SHADER_STAGE_ALL);
  // Creation
  outLayout = outBind.createLayout(m_device);
  outPool = outBind.createPool(m_device, 1);
  outSet = nvvk::allocateDescriptorSet(m_device, outPool, outLayout);

  // Scene Set: S_SCENE
  auto& sceneWrap = m_holdSetWrappers[uint(HoldSet::Scene)];
  auto& sceneBind = sceneWrap.getDescriptorSetBindings();
  auto& scenePool = sceneWrap.getDescriptorPool();
  auto& sceneSet = sceneWrap.getDescriptorSet();
  auto& sceneLayout = sceneWrap.getDescriptorSetLayout();
  // Camera matrices
  sceneBind.addBinding(
      SceneBindings::SceneCamera, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // Instance description
  sceneBind.addBinding(
      SceneBindings::SceneInstances, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // Textures
  sceneBind.addBinding(
      SceneBindings::SceneTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      m_pScene->getTexturesNum(),
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // Lights
  sceneBind.addBinding(
      SceneBindings::SceneLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // Creation
  sceneLayout = sceneBind.createLayout(m_device);
  scenePool = sceneBind.createPool(m_device, 1);
  sceneSet = nvvk::allocateDescriptorSet(m_device, scenePool, sceneLayout);

  // Env Set: S_SCENE
  auto& envWrap = m_holdSetWrappers[uint(HoldSet::Env)];
  auto& envBind = envWrap.getDescriptorSetBindings();
  auto& envPool = envWrap.getDescriptorPool();
  auto& envSet = envWrap.getDescriptorSet();
  auto& envLayout = envWrap.getDescriptorSetLayout();
  // SunAndSky
  envBind.addBinding(
      EnvBindings::EnvSunsky, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
          VK_SHADER_STAGE_MISS_BIT_KHR);
  // Envmap Acceleration
  envBind.addBinding(
      EnvBindings::EnvAccelMap, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
          VK_SHADER_STAGE_MISS_BIT_KHR);
  // Creation
  envLayout = envBind.createLayout(m_device);
  envPool = envBind.createPool(m_device, 1);
  envSet = nvvk::allocateDescriptorSet(m_device, envPool, envLayout);
}

void PipelineGraphics::createCameraBuffer() {
  auto& m_alloc = m_pContext->getAlloc();
  auto& m_debug = m_pContext->getDebug();

  m_bCamera = m_alloc.createBuffer(
      sizeof(GpuCamera),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_debug.setObjectName(m_bCamera.buffer, "Camera");
}

void PipelineGraphics::updateGraphicsDescriptorSet() {
  auto m_device = m_pContext->getDevice();

  // S_SCENE
  auto& sceneWrap = m_holdSetWrappers[uint(HoldSet::Scene)];
  auto& sceneBind = sceneWrap.getDescriptorSetBindings();
  auto& scenePool = sceneWrap.getDescriptorPool();
  auto& sceneSet = sceneWrap.getDescriptorSet();
  auto& sceneLayout = sceneWrap.getDescriptorSetLayout();
  vector<VkWriteDescriptorSet> writesScene;
  // Camera matrices and scene description
  VkDescriptorBufferInfo dbiCamera{m_bCamera.buffer, 0, VK_WHOLE_SIZE};
  writesScene.emplace_back(
      sceneBind.makeWrite(sceneSet, SceneBindings::SceneCamera, &dbiCamera));
  // Instance description
  VkDescriptorBufferInfo dbiSceneDesc{m_pScene->getInstancesDescriptor(), 0,
                                      VK_WHOLE_SIZE};
  writesScene.emplace_back(sceneBind.makeWrite(
      sceneSet, SceneBindings::SceneInstances, &dbiSceneDesc));
  // All texture samplers
  vector<VkDescriptorImageInfo> diit{};
  diit.reserve(m_pScene->getTexturesNum());
  for (uint textureId = 0; textureId < m_pScene->getTexturesNum(); textureId++)
    diit.emplace_back(m_pScene->getTextureDescriptor(textureId));
  writesScene.emplace_back(sceneBind.makeWriteArray(
      sceneSet, SceneBindings::SceneTextures, diit.data()));
  // Lights
  VkDescriptorBufferInfo emittersInfo{m_pScene->getLightsDescriptor(), 0,
                                      VK_WHOLE_SIZE};
  writesScene.emplace_back(
      sceneBind.makeWrite(sceneSet, SceneBindings::SceneLights, &emittersInfo));
  // Writing the information
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writesScene.size()),
                         writesScene.data(), 0, nullptr);

  // S_ENV
  auto& envWrap = m_holdSetWrappers[uint(HoldSet::Env)];
  auto& envBind = envWrap.getDescriptorSetBindings();
  auto& envPool = envWrap.getDescriptorPool();
  auto& envSet = envWrap.getDescriptorSet();
  auto& envLayout = envWrap.getDescriptorSetLayout();
  vector<VkWriteDescriptorSet> writesEnv;
  VkDescriptorBufferInfo dbiSunAndSky{m_pScene->getSunskyDescriptor(), 0,
                                      VK_WHOLE_SIZE};
  writesEnv.emplace_back(
      envBind.makeWrite(envSet, EnvBindings::EnvSunsky, &dbiSunAndSky));
  auto envmapDescInfos = m_pScene->getEnvMapDescriptor();
  writesEnv.emplace_back(envBind.makeWriteArray(
      envSet, EnvBindings::EnvAccelMap, envmapDescInfos.data()));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writesEnv.size()),
                         writesEnv.data(), 0, nullptr);

  // Out Set: S_Out
  auto& outWrap = m_holdSetWrappers[uint(HoldSet::Out)];
  auto& outBind = outWrap.getDescriptorSetBindings();
  auto& outPool = outWrap.getDescriptorPool();
  auto& outSet = outWrap.getDescriptorSet();
  auto& outLayout = outWrap.getDescriptorSetLayout();
  vector<VkWriteDescriptorSet> writesOut;
  array<VkDescriptorImageInfo, NUM_OUTPUT_IMAGES> imageInfos{};
  for (uint channelId = 0; channelId < NUM_OUTPUT_IMAGES; channelId++) {
    VkDescriptorImageInfo imageInfo{
        {}, m_tColors[channelId].descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
    imageInfos[channelId] = imageInfo;
  }
  writesOut.push_back(outBind.makeWriteArray(
      outSet, OutputBindings::OutputStore, imageInfos.data()));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writesOut.size()),
                         writesOut.data(), 0, nullptr);
}
