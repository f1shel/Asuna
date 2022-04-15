#include "context.h"

#include "nvvk/renderpasses_vk.hpp"
#include <nvvk/commands_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/structs_vk.hpp>

void ContextAware::init()
{
	initializeVulkan();
	createAppContext();
	// Search path for shaders and other media
	m_root.emplace_back(NVPSystem::exePath());
	m_root.emplace_back(NVPSystem::exePath() + "..");
}

void ContextAware::deinit()
{
	m_root.clear();
	m_alloc.deinit();
	AppBaseVk::destroy();
	glfwDestroyWindow(m_glfw);
	m_glfw = NULL;
	glfwTerminate();
}

void ContextAware::resizeGlfwWindow()
{
	glfwSetWindowSize(m_glfw, m_size.width, m_size.height);
}

VkExtent2D ContextAware::getSize()
{
	return m_size;
}

VkFramebuffer ContextAware::getFramebuffer(int onlineCurFrame)
{
	if (!m_offline)
		return AppBaseVk::getFramebuffers()[onlineCurFrame];
	else
		return m_offlineFramebuffer;
}

VkRenderPass ContextAware::getRenderPass()
{
	if (!m_offline)
		return AppBaseVk::getRenderPass();
	else
		return m_offlineRenderPass;
}

void ContextAware::setViewport(const VkCommandBuffer &cmdBuf)
{
	if (!m_offline)
		AppBaseVk::setViewport(cmdBuf);
	else
	{
		VkViewport viewport{0.0f, 0.0f, static_cast<float>(m_size.width), static_cast<float>(m_size.height), 0.0f, 1.0f};
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{{0, 0}, {m_size.width, m_size.height}};
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
	}
}

void ContextAware::createOfflineResources()
{
	m_alloc.destroy(m_offlineColor);
	m_alloc.destroy(m_offlineDepth);
	vkDestroyRenderPass(m_device, m_offlineRenderPass, nullptr);
	vkDestroyFramebuffer(m_device, m_offlineFramebuffer, nullptr);
	m_offlineRenderPass  = VK_NULL_HANDLE;
	m_offlineFramebuffer = VK_NULL_HANDLE;

	VkFormat colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
	VkFormat depthFormat = nvvk::findDepthFormat(m_physicalDevice);

	// Creating the color image
	{
		auto colorCreateInfo = nvvk::makeImage2DCreateInfo(
		    m_size, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

		nvvk::Image image = m_alloc.createImage(colorCreateInfo);
		NAME2_VK(image.image, "Offline Color");

		VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
		VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		m_offlineColor                        = m_alloc.createTexture(image, ivInfo, sampler);
		m_offlineColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	// Creating the depth buffer
	auto depthCreateInfo = nvvk::makeImage2DCreateInfo(m_size, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	{
		nvvk::Image image = m_alloc.createImage(depthCreateInfo);
		NAME2_VK(image.image, "Offline Depth");

		VkImageViewCreateInfo depthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		depthStencilView.viewType         = VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format           = depthFormat;
		depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
		depthStencilView.image            = image.image;

		m_offlineDepth = m_alloc.createTexture(image, depthStencilView);
	}

	// Setting the image layout for both color and depth
	{
		nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
		auto              cmdBuf = genCmdBuf.createCommandBuffer();
		nvvk::cmdBarrierImageLayout(cmdBuf, m_offlineColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		nvvk::cmdBarrierImageLayout(cmdBuf, m_offlineDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
		                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		genCmdBuf.submitAndWait(cmdBuf);
	}

	// Creating a renderpass for the offscreen
	m_offlineRenderPass = nvvk::createRenderPass(m_device, {colorFormat}, depthFormat, 1, true, true,
	                                             VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
	NAME2_VK(m_offlineRenderPass, "Offline RenderPass");

	// Creating the frame buffer for offscreen
	std::vector<VkImageView> attachments = {m_offlineColor.descriptor.imageView, m_offlineDepth.descriptor.imageView};

	VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	info.renderPass      = m_offlineRenderPass;
	info.attachmentCount = 2;
	info.pAttachments    = attachments.data();
	info.width           = m_size.width;
	info.height          = m_size.height;
	info.layers          = 1;
	vkCreateFramebuffer(m_device, &info, nullptr, &m_offlineFramebuffer);
	NAME2_VK(m_offlineFramebuffer, "Offline Framebuffer");
}

nvvk::Texture ContextAware::getOfflineFramebufferTexture()
{
	return m_offlineColor;
}

void ContextAware::createGlfwWindow()
{
	// Check initialization of glfw library
	if (!glfwInit())
	{
		LOGE("[!] Context Error: failed to initalize glfw");
		exit(1);
	}
	// Create a window without OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_glfw = glfwCreateWindow(m_size.width, m_size.height, PROJECT_NAME, nullptr, nullptr);
	// Check glfw support for Vulkan
	if (!glfwVulkanSupported())
	{
		std::cerr << "[!] Context Error: glfw does not support vulkan" << std::endl;
		exit(1);
	}
	assert(glfwVulkanSupported() == 1);
}

void ContextAware::initializeVulkan()
{
	nvvk::ContextCreateInfo contextInfo;
	if (!m_offline)
	{
		createGlfwWindow();
		// Set up Vulkan extensions required by glfw(surface, win32, linux, ..)
		uint32_t     count{0};
		const char **reqExtensions = glfwGetRequiredInstanceExtensions(&count);
		for (uint32_t ext_id = 0; ext_id < count; ext_id++)
			contextInfo.addInstanceExtension(reqExtensions[ext_id]);
		// Enabling ability to present rendering
		contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}
	// Using Vulkan 1.3
	contextInfo.setVersion(1, 3);
	// Allow debug names
	contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
	// Allow pointers to buffer memory in shaders
	contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
	// Activate the ray tracing extension
	// Required by KHR_acceleration_structure; allows work to be offloaded onto background threads and parallelized
	contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	// KHR_acceleration_structure
	VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures =
	    nvvk::make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
	contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &asFeatures);
	// KHR_raytracing_pipeline
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures =
	    nvvk::make<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>();
	contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeatures);
	// Add the required device extensions for Debug Printf. If this is confusing,
	contextInfo.addDeviceExtension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
	VkValidationFeaturesEXT      validationInfo            = nvvk::make<VkValidationFeaturesEXT>();
	VkValidationFeatureEnableEXT validationFeatureToEnable = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
	validationInfo.disabledValidationFeatureCount          = 0;
	validationInfo.pDisabledValidationFeatures             = nullptr;
	validationInfo.enabledValidationFeatureCount           = 1;
	validationInfo.pEnabledValidationFeatures              = &validationFeatureToEnable;
	contextInfo.instanceCreateInfoExt                      = &validationInfo;
#ifdef _WIN32
	_putenv_s("DEBUG_PRINTF_TO_STDOUT", "1");
#else         // If not _WIN32
	putenv("DEBUG_PRINTF_TO_STDOUT=1");
#endif        // _WIN32
    // Create the Vulkan instance and then first compatible device based on info
	m_vkcontext.init(contextInfo);
	// Device must support acceleration structures and ray tracing pipelines:
	if (asFeatures.accelerationStructure != VK_TRUE || rtPipelineFeatures.rayTracingPipeline != VK_TRUE)
	{
		std::cerr << "[!] Vulkan: device does not support acceleration structures and ray tracing pipelines" << std::endl;
		exit(1);
	}
}

void ContextAware::createAppContext()
{
	if (!m_offline)
	{
		// Creation of the application
		nvvk::AppBaseVkCreateInfo info;
		info.instance       = m_vkcontext.m_instance;
		info.device         = m_vkcontext.m_device;
		info.physicalDevice = m_vkcontext.m_physicalDevice;
		info.queueIndices.push_back(m_vkcontext.m_queueGCT.familyIndex);
		// Window need to be opened to get the surface on which we will draw
		const VkSurfaceKHR surface = getVkSurface(m_vkcontext.m_instance, m_glfw);
		m_vkcontext.setGCTQueueWithPresent(surface);
		info.size    = m_size;
		info.surface = surface;
		info.window  = m_glfw;
		AppBaseVk::create(info);
	}
	else
	{
		AppBaseVk::setup(m_vkcontext.m_instance, m_vkcontext.m_device, m_vkcontext.m_physicalDevice,
		                 m_vkcontext.m_queueGCT.familyIndex);
	}
	m_alloc.init(m_instance, m_device, m_physicalDevice);
	m_debug.setup(m_device);
}