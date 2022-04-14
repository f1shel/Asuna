#pragma once

#include <backends/imgui_impl_glfw.h>
#include <imgui.h>
#include <GLFW/glfw3.h>

#include <nvvk/appbase_vk.hpp>
#include <nvvk/resourceallocator_vk.hpp>
#include <nvvk/debug_util_vk.hpp>
#include <nvvk/gizmos_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/structs_vk.hpp>

#include <iostream>

class ContextAware : public nvvk::AppBaseVk
{
public:
    GLFWwindow* m_glfw = NULL;
    // Allocator for buffer, images, acceleration structures
    nvvk::ResourceAllocatorDedicated m_alloc;
    // Debugger to name objects
    nvvk::DebugUtil m_debug;
    // Vulkan context
    nvvk::Context m_vkcontext{};
    // Filesystem searching root
    std::vector<std::string> m_root{};
public:
    void init() {
        createGlfwWindow();
        initializeVulkan();
        createAppContext();
        // Search path for shaders and other media
        m_root.emplace_back(NVPSystem::exePath());
        m_root.emplace_back(NVPSystem::exePath() + "..");
    }
    void deinit() {
        m_root.clear();
        m_alloc.deinit();
        AppBaseVk::destroy();
        glfwDestroyWindow(m_glfw);
        m_glfw = NULL;
        glfwTerminate();
    }
private:
    void createGlfwWindow() {
        const size_t defaultWindowWidth = 400;
        const size_t defaultWindowHeight = 300;
        // Check initialization of glfw library
        if (!glfwInit()) {
            LOGE("[!] Context Error: failed to initalize glfw");
            exit(1);
        }
        // Create a window without OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_glfw = glfwCreateWindow(defaultWindowWidth, defaultWindowHeight, PROJECT_NAME, nullptr, nullptr);
        // Check glfw support for Vulkan
        if (!glfwVulkanSupported()) {
            std::cerr << "[!] Context Error: glfw does not support vulkan" << std::endl;
            exit(1);
        }
        assert(glfwVulkanSupported() == 1);
    }
    void initializeVulkan() {
        nvvk::ContextCreateInfo contextInfo;
        // Using Vulkan 1.3
        contextInfo.setVersion(1, 3);
        // Set up Vulkan extensions required by glfw(surface, win32, linux, ..)
        uint32_t count{ 0 };
        const char** reqExtensions = glfwGetRequiredInstanceExtensions(&count);
        for (uint32_t ext_id = 0; ext_id < count; ext_id++)
            contextInfo.addInstanceExtension(reqExtensions[ext_id]);
        // Allow debug names
        contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
        // Enabling ability to present rendering
        contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        // Allow pointers to buffer memory in shaders
        contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        // Activate the ray tracing extension
        // Required by KHR_acceleration_structure; allows work to be offloaded onto background threads and parallelized
        contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        // KHR_acceleration_structure
        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = nvvk::make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
        contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &asFeatures);
        // KHR_raytracing_pipeline
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = nvvk::make<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>();
        contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeatures);
        // Add the required device extensions for Debug Printf. If this is confusing,
        contextInfo.addDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
        VkValidationFeaturesEXT      validationInfo = nvvk::make<VkValidationFeaturesEXT>();
        VkValidationFeatureEnableEXT validationFeatureToEnable = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
        validationInfo.enabledValidationFeatureCount = 1;
        validationInfo.pEnabledValidationFeatures = &validationFeatureToEnable;
        contextInfo.instanceCreateInfoExt = &validationInfo;
#ifdef _WIN32
        _putenv_s("DEBUG_PRINTF_TO_STDOUT", "1");
#else   // If not _WIN32
        putenv("DEBUG_PRINTF_TO_STDOUT=1");
#endif  // _WIN32
        // Create the Vulkan instance and then first compatible device based on info
        m_vkcontext.init(contextInfo);
        // Device must support acceleration structures and ray tracing pipelines:
        if (asFeatures.accelerationStructure != VK_TRUE || rtPipelineFeatures.rayTracingPipeline != VK_TRUE) {
            std::cerr << "[!] Vulkan: device does not support acceleration structures and ray tracing pipelines" << std::endl;
            exit(1);
        }
    }
    void createAppContext() {
        // Window need to be opened to get the surface on which we will draw
        const VkSurfaceKHR surface = getVkSurface(m_vkcontext.m_instance, m_glfw);
        m_vkcontext.setGCTQueueWithPresent(surface);
        // Creation of the application
        nvvk::AppBaseVkCreateInfo info;
        info.instance = m_vkcontext.m_instance;
        info.device = m_vkcontext.m_device;
        info.physicalDevice = m_vkcontext.m_physicalDevice;
        int width, height;
        glfwGetWindowSize(m_glfw, &width, &height);
        info.size = { (unsigned)width, (unsigned)height };
        info.surface = surface;
        info.window = m_glfw;
        info.queueIndices.push_back(m_vkcontext.m_queueGCT.familyIndex);
        create(info);
        m_alloc.init(m_instance, m_device, m_physicalDevice);
        m_debug.setup(m_device);
    }
};