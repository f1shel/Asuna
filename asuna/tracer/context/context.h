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

#define ASUNA_DEFAULT_WINDOW_WIDTH 1
#define ASUNA_DEFAULT_WINDOW_HEIGHT 1

class ContextAware : public nvvk::AppBaseVk
{
  public:
	bool        m_offline = true;
	GLFWwindow *m_glfw    = NULL;
	// Allocator for buffer, images, acceleration structures
	nvvk::ResourceAllocatorDedicated m_alloc;
	// Debugger to name objects
	nvvk::DebugUtil m_debug;
	// Vulkan context
	nvvk::Context m_vkcontext{};
	VkExtent2D    m_size = {ASUNA_DEFAULT_WINDOW_WIDTH, ASUNA_DEFAULT_WINDOW_HEIGHT};
	// Filesystem searching root
	std::vector<std::string> m_root{};

  public:
	void          init();
	void          deinit();
	VkExtent2D    getSize();
	VkFramebuffer getFramebuffer(int onlineCurFrame = 0);
	VkRenderPass  getRenderPass();
	void          setViewport(const VkCommandBuffer &cmdBuf);
	void          resizeGlfwWindow();
	void          createOfflineResources();
	nvvk::Texture getOfflineFramebufferTexture();

  private:
	void createGlfwWindow();
	void initializeVulkan();
	void createAppContext();

  private:
	VkRenderPass  m_offlineRenderPass{VK_NULL_HANDLE};
	VkFramebuffer m_offlineFramebuffer{VK_NULL_HANDLE};
	nvvk::Texture m_offlineColor;
	nvvk::Texture m_offlineDepth;
};