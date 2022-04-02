#pragma once

#include <nvvk/appbase_vk.hpp>
#include "nvvk/resourceallocator_vk.hpp"
#include <nvvk/debug_util_vk.hpp>

class AsunaTracer : public nvvk::AppBaseVk {
public:
    // First thing to do
    void init();
    void create(const nvvk::AppBaseVkCreateInfo& info) override;
    // Destroy everything
    void destroy() override;

public: // glfw
    GLFWwindow* window = NULL;
    bool glfwShouldClose();
    void glfwPoll();
private: // Common utilities
    // Allocator for buffer, images, acceleration structures
    nvvk::ResourceAllocatorDedicated m_alloc;
    // Debugger to name objects
    nvvk::DebugUtil m_debug;
};