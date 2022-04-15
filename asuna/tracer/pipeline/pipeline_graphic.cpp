#include "pipeline_graphic.h"
#include "../../hostdevice/camera.h"
#include "../../hostdevice/pushconstant.h"
#include "../../hostdevice/vertex.h"

#include "nvvk/images_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/commands_vk.hpp>
#include <nvvk/pipeline_vk.hpp>
#include <nvvk/structs_vk.hpp>

#include <cstdint>
#include <vector>

void PipelineGraphic::init(PipelineCorrelated *pPipCorr)
{
	m_pContext = pPipCorr->m_pContext;
	m_pScene   = pPipCorr->m_pScene;
	// Graphic
	// 创建光栅化管线
	nvh::Stopwatch sw_;
	createOffscreenResources();
	createGraphicDescriptorSetLayout();
	createGraphicPipeline();
	createCameraBuffer();
	updateGraphicDescriptorSet();
	LOGI("[ ] Pipeline: %6.2fms Graphic pipeline creation\n", sw_.elapsed());
}

void PipelineGraphic::run(const VkCommandBuffer &cmdBuf)
{
	// Prepare new UBO contents on host.
	auto        m_size      = m_pContext->getSize();
	const float aspectRatio = m_size.width / static_cast<float>(m_size.height);
	Camera      hostCamera  = {};
	const auto &view        = CameraManip.getMatrix();
	const auto &proj        = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.1f, 1000.0f);
	// proj[1][1] *= -1;  // Inverting Y for Vulkan (not needed with perspectiveVK).

	int a = rand();
	hostCamera.viewProj.set_translate(vec3(a));
	hostCamera.viewInverse = nvmath::invert(view);
	hostCamera.projInverse = nvmath::invert(proj);

	// UBO on the device, and what stages access it.
	VkBuffer deviceUBO      = m_bCamera.buffer;
	auto     uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

	if (!m_pContext->m_offline)
	{
		// Ensure that the modified UBO is not visible to previous frames.
		VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
		beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		beforeBarrier.buffer        = deviceUBO;
		beforeBarrier.offset        = 0;
		beforeBarrier.size          = sizeof(Camera);
		vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
		                     nullptr, 1, &beforeBarrier, 0, nullptr);
	}

	// Schedule the host-to-device upload. (hostUBO is copied into the cmd
	// buffer so it is okay to deallocate when the function returns).
	vkCmdUpdateBuffer(cmdBuf, m_bCamera.buffer, 0, sizeof(Camera), &hostCamera);

	if (!m_pContext->m_offline)
	{
		// Making sure the updated UBO will be visible.
		VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
		afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		afterBarrier.buffer        = deviceUBO;
		afterBarrier.offset        = 0;
		afterBarrier.size          = sizeof(hostCamera);
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
		                     nullptr, 1, &afterBarrier, 0, nullptr);
	}
}

void PipelineGraphic::deinit()
{
	auto &m_alloc   = m_pContext->m_alloc;
	auto  m_device  = m_pContext->getDevice();
	auto  m_cmdPool = m_pContext->getCommandPool();

	vkFreeCommandBuffers(m_device, m_cmdPool, 1, &m_recordedCmdBuffer);
	m_recordedCmdBuffer = VK_NULL_HANDLE;

	m_alloc.destroy(m_tColor);
	m_alloc.destroy(m_tDepth);
	m_alloc.destroy(m_bCamera);
	vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
	vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);
	m_offscreenRenderPass  = VK_NULL_HANDLE;
	m_offscreenFramebuffer = VK_NULL_HANDLE;

	m_pcGraphic = {0};

	PipelineAware::deinit();
}

void PipelineGraphic::createOffscreenResources()
{
	auto &m_alloc              = m_pContext->m_alloc;
	auto &m_debug              = m_pContext->m_debug;
	auto  m_device             = m_pContext->getDevice();
	auto  m_physicalDevice     = m_pContext->getPhysicalDevice();
	auto  m_size               = m_pContext->getSize();
	auto  m_graphicsQueueIndex = m_pContext->getQueueFamily();

	m_alloc.destroy(m_tColor);
	m_alloc.destroy(m_tDepth);
	vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
	vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);
	m_offscreenRenderPass  = VK_NULL_HANDLE;
	m_offscreenFramebuffer = VK_NULL_HANDLE;

	VkFormat colorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
	VkFormat depthFormat = nvvk::findDepthFormat(m_physicalDevice);

	// Creating the color image
	{
		auto colorCreateInfo = nvvk::makeImage2DCreateInfo(
		    m_size, colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

		nvvk::Image image = m_alloc.createImage(colorCreateInfo);
		NAME2_VK(image.image, "Offscreen Color");

		VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
		VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		m_tColor                        = m_alloc.createTexture(image, ivInfo, sampler);
		m_tColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	// Creating the depth buffer
	auto depthCreateInfo = nvvk::makeImage2DCreateInfo(m_size, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	{
		nvvk::Image image = m_alloc.createImage(depthCreateInfo);
		NAME2_VK(image.image, "Offscreen Depth");

		VkImageViewCreateInfo depthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		depthStencilView.viewType         = VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format           = depthFormat;
		depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
		depthStencilView.image            = image.image;

		m_tDepth = m_alloc.createTexture(image, depthStencilView);
	}

	// Setting the image layout for both color and depth
	{
		nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
		auto              cmdBuf = genCmdBuf.createCommandBuffer();
		nvvk::cmdBarrierImageLayout(cmdBuf, m_tColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		nvvk::cmdBarrierImageLayout(cmdBuf, m_tDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
		                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		genCmdBuf.submitAndWait(cmdBuf);
	}

	// Creating a renderpass for the offscreen
	m_offscreenRenderPass = nvvk::createRenderPass(m_device, {colorFormat}, depthFormat, 1, true, true,
	                                               VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
	NAME2_VK(m_offscreenRenderPass, "Offscreen RenderPass");

	// Creating the frame buffer for offscreen
	std::vector<VkImageView> attachments = {m_tColor.descriptor.imageView, m_tDepth.descriptor.imageView};

	VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	info.renderPass      = m_offscreenRenderPass;
	info.attachmentCount = 2;
	info.pAttachments    = attachments.data();
	info.width           = m_size.width;
	info.height          = m_size.height;
	info.layers          = 1;
	vkCreateFramebuffer(m_device, &info, nullptr, &m_offscreenFramebuffer);
	NAME2_VK(m_offscreenFramebuffer, "Offscreen FrameBuffer");
}

// TODO: nbtxt
void PipelineGraphic::createGraphicDescriptorSetLayout()
{
	auto m_device = m_pContext->getDevice();

	nvvk::DescriptorSetBindings &bind = m_dstSetLayoutBind;

	// Camera matrices
	bind.addBinding(BindingsGraphic::eBindingGraphicCamera, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
	                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	// Mesh descriptions
	bind.addBinding(BindingsGraphic::eBindingGraphicSceneDesc, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
	                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

	m_dstLayout = bind.createLayout(m_device);
	m_dstPool   = bind.createPool(m_device, 1);
	m_dstSet    = nvvk::allocateDescriptorSet(m_device, m_dstPool, m_dstLayout);
}

// TODO: push constant
void PipelineGraphic::createGraphicPipeline()
{
	auto &m_debug  = m_pContext->m_debug;
	auto  m_device = m_pContext->getDevice();

	// Creating the Pipeline Layout
	VkPushConstantRange        pushConstantRanges = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                              sizeof(PushConstantGraphic)};
	VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	createInfo.setLayoutCount         = 1;
	createInfo.pSetLayouts            = &m_dstLayout;
	createInfo.pushConstantRangeCount = 1;
	createInfo.pPushConstantRanges    = &pushConstantRanges;
	vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);

	// Creating the Pipeline
	nvvk::GraphicsPipelineGeneratorCombined gpb(m_device, m_pipelineLayout, m_offscreenRenderPass);
	gpb.depthStencilState.depthTestEnable = true;
	gpb.addShader(nvh::loadFile("shaders/graphic.idle.vert.spv", true, m_pContext->m_root), VK_SHADER_STAGE_VERTEX_BIT);
	gpb.addShader(nvh::loadFile("shaders/graphic.idle.frag.spv", true, m_pContext->m_root), VK_SHADER_STAGE_FRAGMENT_BIT);
	gpb.addBindingDescriptions({{0, sizeof(Vertex)}});
	gpb.addAttributeDescriptions({
	    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, pos))},
	    {1, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, uv))},
	    {2, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal))},
	    {3, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, tangent))},
	});
	m_pipeline = gpb.createPipeline();
	NAME2_VK(m_pipeline, "Graphics");
}

void PipelineGraphic::createCameraBuffer()
{
	auto &m_alloc = m_pContext->m_alloc;
	auto &m_debug = m_pContext->m_debug;

	m_bCamera = m_alloc.createBuffer(sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	m_debug.setObjectName(m_bCamera.buffer, "Camera");
}

// TODO: scene description
void PipelineGraphic::updateGraphicDescriptorSet()
{
	auto &bind     = m_dstSetLayoutBind;
	auto  m_device = m_pContext->getDevice();

	std::vector<VkWriteDescriptorSet> writes;

	// Camera matrices and scene description
	VkDescriptorBufferInfo dbiCamera{m_bCamera.buffer, 0, VK_WHOLE_SIZE};
	writes.emplace_back(bind.makeWrite(m_dstSet, BindingsGraphic::eBindingGraphicCamera, &dbiCamera));

	VkDescriptorBufferInfo dbiSceneDesc{m_pScene->getSceneDescBuffer().buffer, 0, VK_WHOLE_SIZE};
	writes.emplace_back(bind.makeWrite(m_dstSet, BindingsGraphic::eBindingGraphicSceneDesc, &dbiSceneDesc));

	// Writing the information
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
