#include "asuna_tracer.h"

#include <iostream>
using namespace std;

#include <nvvk/structs_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/gizmos_vk.hpp>

void AsunaTracer::init()
{
    // Create glfw window
    const size_t defaultWindowWidth = 400;
    const size_t defaultWindowHeight = 300;
    {
        // Check initialization of glfw library
        if (!glfwInit()) {
            std::cerr << "[!] GLFW Error: Failed to initalize" << std::endl;
            exit(1);
        }
        // Create a window without OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(defaultWindowWidth, defaultWindowHeight, PROJECT_NAME, nullptr, nullptr);
        // Check glfw support for Vulkan
        if (!glfwVulkanSupported()) {
            std::cerr << "[!] GLFW Error: Vulkan not supported" << std::endl;
            exit(1);
        }
        assert(glfwVulkanSupported() == 1);
    }
    // Initalize Vulkan
    nvvk::Context vkContext{};
    // Vulkan context creation
    {
        nvvk::ContextCreateInfo contextInfo;
        // Using Vulkan 1.2
        contextInfo.setVersion(1, 2);
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
        vkContext.init(contextInfo);
        // Device must support acceleration structures and ray tracing pipelines:
        if (asFeatures.accelerationStructure != VK_TRUE || rtPipelineFeatures.rayTracingPipeline != VK_TRUE) {
            std::cerr << "[!] Vulkan: device does not support acceleration structures and ray tracing pipelines" << std::endl;
            exit(1);
        }
    }
    // Window need to be opened to get the surface on which we will draw
    const VkSurfaceKHR surface = this->getVkSurface(vkContext.m_instance, window);
    vkContext.setGCTQueueWithPresent(surface);
    // Creation of the application
    nvvk::AppBaseVkCreateInfo info;
    info.instance = vkContext.m_instance;
    info.device = vkContext.m_device;
    info.physicalDevice = vkContext.m_physicalDevice;
    info.size = { defaultWindowWidth, defaultWindowHeight };
    info.surface = surface;
    info.window = window;
    info.queueIndices.push_back(vkContext.m_queueGCT.familyIndex);
    this->create(info);
    // Mini coordinate system at left bottom corner
    nvvk::AxisVK vkAxis;
    vkAxis.init(this->getDevice(), this->getRenderPass());
}

void AsunaTracer::create(const nvvk::AppBaseVkCreateInfo& info)
{
    AppBaseVk::create(info);
    m_alloc.init(m_instance, m_device, m_physicalDevice);
    m_debug.setup(m_device);
}

void AsunaTracer::destroy()
{
    m_alloc.deinit();
    AppBaseVk::destroy();
    glfwDestroyWindow(window);
    window = NULL;
    glfwTerminate();
}

bool AsunaTracer::glfwShouldClose()
{
    return glfwWindowShouldClose(window);
}

void AsunaTracer::glfwPoll()
{
    glfwPollEvents();
}
