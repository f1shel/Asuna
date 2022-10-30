#include "context.h"

#include <nvvk/commands_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/structs_vk.hpp>
#include "nvvk/renderpasses_vk.hpp"

void ContextAware::init(ContextInitSetting cis) {
  LOG_INFO("{}: creating vulkan instance", "Context");

  m_cis = cis;

  // Extensions, context and instance
  initializeVulkan();

  // Swap chain, window and queue
  createAppContext();

  // Search path for shaders and other media
  m_root = NVPSystem::exePath();

  // Create offline resources for offline mode
  if (getOfflineMode()) createOfflineResources();

  // Create parallel queues
  createParallelQueues();
}

void ContextAware::deinit() {
  m_root.clear();
  m_alloc.deinit();
  AppBaseVk::destroy();
  glfwDestroyWindow(m_window);
  m_window = NULL;
  glfwTerminate();
}

void ContextAware::resizeGlfwWindow() {
  glfwSetWindowSize(m_window, m_size.width, m_size.height);
}

string& ContextAware::getRoot() { return m_root; }

nvvk::Texture ContextAware::getOfflineColor() { return m_offlineColor; }

nvvk::Texture ContextAware::getOfflineDepth() { return m_offlineDepth; }

VkFramebuffer ContextAware::getFramebuffer(int onlineCurFrame) {
  if (!getOfflineMode())
    return AppBaseVk::getFramebuffers()[onlineCurFrame];
  else
    return m_offlineFramebuffer;
}

VkRenderPass ContextAware::getRenderPass() {
  if (!getOfflineMode())
    return AppBaseVk::getRenderPass();
  else
    return m_offlineRenderPass;
}

vector<nvvk::Context::Queue>& ContextAware::getParallelQueues() {
  return m_parallelQueues;
}

void ContextAware::setViewport(const VkCommandBuffer& cmdBuf) {
  if (!getOfflineMode())
    AppBaseVk::setViewport(cmdBuf);
  else {
    VkViewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(m_size.width),
                        static_cast<float>(m_size.height),
                        0.0f,
                        1.0f};
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {m_size.width, m_size.height}};
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
  }
}

void ContextAware::createOfflineResources() {
  m_alloc.destroy(m_offlineColor);
  m_alloc.destroy(m_offlineDepth);
  vkDestroyRenderPass(m_device, m_offlineRenderPass, nullptr);
  vkDestroyFramebuffer(m_device, m_offlineFramebuffer, nullptr);
  m_offlineRenderPass = VK_NULL_HANDLE;
  m_offlineFramebuffer = VK_NULL_HANDLE;

  VkFormat colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  VkFormat depthFormat = nvvk::findDepthFormat(m_physicalDevice);

  // Creating the color image
  {
    auto colorCreateInfo = nvvk::makeImage2DCreateInfo(
        m_size, colorFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image image = m_alloc.createImage(colorCreateInfo);
    NAME2_VK(image.image, "Offline Color");

    VkImageViewCreateInfo ivInfo =
        nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offlineColor = m_alloc.createTexture(image, ivInfo, sampler);
    m_offlineColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }

  // Creating the depth buffer
  auto depthCreateInfo = nvvk::makeImage2DCreateInfo(
      m_size, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  {
    nvvk::Image image = m_alloc.createImage(depthCreateInfo);
    NAME2_VK(image.image, "Offline Depth");

    VkImageViewCreateInfo depthStencilView{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format = depthFormat;
    depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthStencilView.image = image.image;

    m_offlineDepth = m_alloc.createTexture(image, depthStencilView);
  }

  // Setting the image layout for both color and depth
  {
    nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
    auto cmdBuf = genCmdBuf.createCommandBuffer();
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offlineColor.image,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(
        cmdBuf, m_offlineDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    genCmdBuf.submitAndWait(cmdBuf);
  }

  // Creating a renderpass for the offscreen
  m_offlineRenderPass = nvvk::createRenderPass(
      m_device, {colorFormat}, depthFormat, 1, true, true,
      VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
  NAME2_VK(m_offlineRenderPass, "Offline RenderPass");

  // Creating the frame buffer for offscreen
  std::vector<VkImageView> attachments = {m_offlineColor.descriptor.imageView,
                                          m_offlineDepth.descriptor.imageView};

  VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  info.renderPass = m_offlineRenderPass;
  info.attachmentCount = 2;
  info.pAttachments = attachments.data();
  info.width = m_size.width;
  info.height = m_size.height;
  info.layers = 1;
  vkCreateFramebuffer(m_device, &info, nullptr, &m_offlineFramebuffer);
  NAME2_VK(m_offlineFramebuffer, "Offline Framebuffer");
}

void ContextAware::createParallelQueues() {
  auto qGCT1 =
      m_vkcontext.createQueue(m_contextInfo.defaultQueueGCT, "GCT1", 1.0f);
  m_parallelQueues.push_back(qGCT1);
  m_parallelQueues.push_back(m_vkcontext.m_queueC);
  m_parallelQueues.push_back(m_vkcontext.m_queueT);
}

void ContextAware::createSwapchain(const VkSurfaceKHR& surface, uint32_t width,
                                   uint32_t height, VkFormat colorFormat,
                                   VkFormat depthFormat, bool vsync) {
  AppBaseVk::createSwapchain(surface, width, height, colorFormat, depthFormat,
                             vsync);

  std::vector<VkCommandBuffer> commandBuffers(m_swapChain.getImageCount());
  VkCommandBufferAllocateInfo allocateInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocateInfo.commandPool = m_cmdPool;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandBufferCount = m_swapChain.getImageCount();

  VkResult result =
      vkAllocateCommandBuffers(m_device, &allocateInfo, commandBuffers.data());
  m_commandBuffers.insert(m_commandBuffers.end(), commandBuffers.begin(),
                          commandBuffers.end());
}

bool ContextAware::shouldGlfwCloseWindow() {
  return glfwWindowShouldClose(m_window);
}

void ContextAware::setSize(VkExtent2D& size) { m_size = size; }

VkExtent2D& ContextAware::getSize() { return m_size; }

nvvk::ResourceAllocatorDedicated& ContextAware::getAlloc() { return m_alloc; }

nvvk::DebugUtil& ContextAware::getDebug() { return m_debug; }

bool ContextAware::getOfflineMode() { return m_cis.offline; }

void ContextAware::createGlfwWindow() {
  // Check initialization of glfw library
  if (!glfwInit()) {
    LOG_ERROR("{}: failed to initalize glfw", "Context");
    exit(1);
  }
  // Create a window without OpenGL context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  m_window = glfwCreateWindow(m_size.width, m_size.height, PROJECT_NAME,
                              nullptr, nullptr);
  // Check glfw support for Vulkan
  if (!glfwVulkanSupported()) {
    LOG_ERROR("{}: glfw does not support vulkan", "Context");
    exit(1);
  }
  assert(glfwVulkanSupported() == 1);
}

void ContextAware::initializeVulkan() {
  if (!getOfflineMode()) {
    createGlfwWindow();
    // Set up Vulkan extensions required by glfw(surface, win32, linux, ..)
    uint32_t count{0};
    const char** reqExtensions = glfwGetRequiredInstanceExtensions(&count);
    for (uint32_t ext_id = 0; ext_id < count; ext_id++)
      m_contextInfo.addInstanceExtension(reqExtensions[ext_id]);
    // Enabling ability to present rendering
    m_contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
  // Using Vulkan 1.3
  m_contextInfo.setVersion(1, 3);
  // FPS in titlebar
  m_contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);
  // Allow debug names
  m_contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
  // Allow pointers to buffer memory in shaders
  m_contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
  // Activate the ray tracing extension
  // Required by KHR_acceleration_structure; allows work to be offloaded onto
  // background threads and parallelized
  m_contextInfo.addDeviceExtension(
      VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  // KHR_acceleration_structure
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures =
      nvvk::make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
  m_contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                   false, &asFeatures);
  // KHR_raytracing_pipeline
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures =
      nvvk::make<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>();
  m_contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                   false, &rtPipelineFeatures);
  // Extra queues for parallel load/build
  m_contextInfo.addRequestedQueue(m_contextInfo.defaultQueueGCT, 1, 1.0f);
  // Add the required device extensions for Debug Printf. If this is
  // confusing,
  m_contextInfo.addDeviceExtension(
      VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
  VkValidationFeaturesEXT validationInfo =
      nvvk::make<VkValidationFeaturesEXT>();
  VkValidationFeatureEnableEXT validationFeatureToEnable =
      VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
  validationInfo.disabledValidationFeatureCount = 0;
  validationInfo.pDisabledValidationFeatures = nullptr;
  validationInfo.enabledValidationFeatureCount = 1;
  validationInfo.pEnabledValidationFeatures = &validationFeatureToEnable;
  m_contextInfo.instanceCreateInfoExt = &validationInfo;

  // Semaphores - interop Vulkan/Cuda
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME);
#ifdef WIN32
  m_contextInfo.addDeviceExtension(
      VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME);
#else
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME);
#endif

  // Buffer - interop
  m_contextInfo.addDeviceExtension(
      VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
  m_contextInfo.addDeviceExtension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);

  // Synchronization (mix of timeline and binary semaphores)
  m_contextInfo.addDeviceExtension(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
                                   false);

#ifdef _WIN32
  _putenv_s("DEBUG_PRINTF_TO_STDOUT", "1");
#else  // If not _WIN32
  putenv("DEBUG_PRINTF_TO_STDOUT=1");
#endif
  m_contextInfo.verboseAvailable = false;
  m_contextInfo.verboseCompatibleDevices = true;
  m_contextInfo.verboseUsed = false;
  m_contextInfo.compatibleDeviceIndex = m_cis.useGpuId;

  // Create the Vulkan instance and then first compatible device based on info
  m_vkcontext.init(m_contextInfo);

  auto devs = m_vkcontext.getCompatibleDevices(m_contextInfo);
  for (auto devId : devs) {
    LOG_INFO("[Vulkan] Compatible Device Index: {}\n", devId);
  }

  // Device must support acceleration structures and ray tracing pipelines:
  if (asFeatures.accelerationStructure != VK_TRUE ||
      rtPipelineFeatures.rayTracingPipeline != VK_TRUE) {
    std::cerr << "[!] Vulkan: device does not support acceleration "
                 "structures and ray "
                 "tracing pipelines"
              << std::endl;
    exit(1);
  }
}

void ContextAware::createAppContext() {
  if (!getOfflineMode()) {
    // Creation of the application
    nvvk::AppBaseVkCreateInfo info;
    info.instance = m_vkcontext.m_instance;
    info.device = m_vkcontext.m_device;
    info.physicalDevice = m_vkcontext.m_physicalDevice;
    info.queueIndices.push_back(m_vkcontext.m_queueGCT.familyIndex);
    // Window need to be opened to get the surface on which we will draw
    const VkSurfaceKHR surface = getVkSurface(m_vkcontext.m_instance, m_window);
    m_vkcontext.setGCTQueueWithPresent(surface);
    info.size = m_size;
    info.surface = surface;
    info.window = m_window;
    AppBaseVk::create(info);
  } else {
    AppBaseVk::setup(m_vkcontext.m_instance, m_vkcontext.m_device,
                     m_vkcontext.m_physicalDevice,
                     m_vkcontext.m_queueGCT.familyIndex);
  }
  m_alloc.init(m_instance, m_device, m_physicalDevice);
  m_debug.setup(m_device);
}