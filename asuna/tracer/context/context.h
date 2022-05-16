#pragma once

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>

#include <nvvk/appbase_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/debug_util_vk.hpp>
#include <nvvk/gizmos_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>
#include <nvvk/structs_vk.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <map>

struct ContextInitState
{
  bool offline{false};
};

using std::string;
using std::vector;
using std::array;

class ContextAware : public nvvk::AppBaseVk
{
public:
  void init(ContextInitState cis);
  void deinit();
  void setViewport(const VkCommandBuffer& cmdBuf);
  void resizeGlfwWindow();
  void createOfflineResources();
  bool shouldGlfwCloseWindow();

public:
  void                              setSize(VkExtent2D& size) { m_size = size; }
  VkExtent2D&                       getSize() { return m_size; }
  nvvk::ResourceAllocatorDedicated& getAlloc() { return m_alloc; }
  nvvk::DebugUtil&                  getDebug() { return m_debug; }
  vector<string>&                   getRoot() { return m_root; }
  bool                              getOfflineMode() { return m_cis.offline; }
  nvvk::Texture                     getOfflineColor() { return m_offlineColor; }
  nvvk::Texture                     getOfflineDepth() { return m_offlineDepth; }
  VkFramebuffer                     getFramebuffer(int curFrame = 0);
  VkRenderPass                      getRenderPass();

private:
  void createGlfwWindow();
  void initializeVulkan();
  void createAppContext();

private:
  VkRenderPass  m_offlineRenderPass{VK_NULL_HANDLE};
  VkFramebuffer m_offlineFramebuffer{VK_NULL_HANDLE};
  nvvk::Texture m_offlineColor;
  nvvk::Texture m_offlineDepth;

private:
  ContextInitState                 m_cis;
  nvvk::ResourceAllocatorDedicated m_alloc;        // Allocator for buffer, images, acceleration structures
  nvvk::DebugUtil                  m_debug;        // Debugger to name objects
  nvvk::Context                    m_vkcontext{};  // Vulkan context
  std::vector<std::string>         m_root{};       // Filesystem searching root
};