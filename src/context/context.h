#pragma once

#include <array>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <nvvk/appbase_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/debug_util_vk.hpp>
#include <nvvk/gizmos_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>
#include <nvvk/structs_vk.hpp>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#define LOG_INFO (spdlog::info)
#define LOG_ERROR (spdlog::error)
#define LOG_WARN (spdlog::warn)

using std::array;
using std::string;
using std::vector;

// We are using this to change the image to display on the fly
constexpr int FRAMES_IN_FLIGHT = 3;

struct ContextInitSetting {
  bool offline{false};
  int useGpuId{0};
};

class ContextAware : public nvvk::AppBaseVk {
public:
  void init(ContextInitSetting cis);
  void deinit();
  void setViewport(const VkCommandBuffer& cmdBuf);

  // Online mode: resize glfw window according to ContextAware::getSize()
  void resizeGlfwWindow();

  // Online mode: whether glfw has been closed
  bool shouldGlfwCloseWindow();

public:
  // Set window size
  void setSize(VkExtent2D& size);

  // Get window size
  VkExtent2D& getSize();

  // Get vulkan resource allocator
  nvvk::ResourceAllocatorDedicated& getAlloc();

  // Get vulkan debugger
  nvvk::DebugUtil& getDebug();

  // If in offline mode, return true
  bool getOfflineMode();

  // Path of exectuable program
  string& getRoot();

  // Offline rgba32f buffer(ldr)
  nvvk::Texture getOfflineColor();

  // Offline depth buffer(only used for renderpass)
  nvvk::Texture getOfflineDepth();

  // Offline framebuffer(attachment: offlineColor and offlineDepth)
  VkFramebuffer getFramebuffer(int curFrame = 0);

  // Offline render pass
  VkRenderPass getRenderPass();

  vector<nvvk::Context::Queue>& getParallelQueues();

private:
  void createGlfwWindow();
  void initializeVulkan();
  void createAppContext();
  void createOfflineResources();
  void createParallelQueues();

  // Overriding to create 2x more command buffer per frame
  void createSwapchain(const VkSurfaceKHR& surface, uint32_t width,
                       uint32_t height,
                       VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM,
                       VkFormat depthFormat = VK_FORMAT_UNDEFINED,
                       bool vsync = false) override;

private:
  VkRenderPass m_offlineRenderPass{VK_NULL_HANDLE};
  VkFramebuffer m_offlineFramebuffer{VK_NULL_HANDLE};
  nvvk::Texture m_offlineColor;
  nvvk::Texture m_offlineDepth;

private:
  ContextInitSetting m_cis;
  nvvk::ResourceAllocatorDedicated m_alloc;
  nvvk::DebugUtil m_debug;
  nvvk::Context m_vkcontext{};
  nvvk::ContextCreateInfo m_contextInfo;
  std::string m_root{};

  // Collecting all the Queues the application will need.
  // - GTC1 for scene assets loading and pipeline creation
  // - Compute for tlas creation
  vector<nvvk::Context::Queue> m_parallelQueues{};
};