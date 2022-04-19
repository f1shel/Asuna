#include "pipeline_post.h"

#include "../../hostdevice/binding.h"

#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/debug_util_vk.hpp>

#include <cstdint>
#include <vector>

void PipelinePost::init(PipelineCorrelated *pPipCorr)
{
	m_pContext     = pPipCorr->m_pContext;
	m_pScene       = pPipCorr->m_pScene;
	m_pPipGraphics = ((PipelineCorrelatedPost *) pPipCorr)->m_pPipGraphics;
	// Ray tracing
	nvh::Stopwatch sw_;
	createPostDescriptorSetLayout();
	createPostPipeline();
	updatePostDescriptorSet();
	LOGI("[ ] Pipeline: %6.2fms Post pipeline creation\n", sw_.elapsed());
}

void PipelinePost::run(const VkCommandBuffer &cmdBuf)
{
	auto &m_debug = m_pContext->m_debug;

	LABEL_SCOPE_VK(cmdBuf);

	m_pContext->setViewport(cmdBuf);
	// vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
	// sizeof(Tonemapper), &m_tonemapper);
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
	                        &m_dstSet, 0, nullptr);
	vkCmdDraw(cmdBuf, 3, 1, 0, 0);
}

void PipelinePost::deinit()
{
	m_pPipGraphics = nullptr;

	PipelineAware::deinit();
}

void PipelinePost::createPostDescriptorSetLayout()
{
	auto m_device = m_pContext->getDevice();

	// The descriptor layout is the description of the data that is passed to the vertex or the
	// fragment program.
	nvvk::DescriptorSetBindings &bind = m_dstSetLayoutBind;
	bind.addBinding(GPUBindingPost::eGPUBindingPostImage,
	                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_dstLayout = bind.createLayout(m_device);
	m_dstPool   = bind.createPool(m_device);
	m_dstSet    = nvvk::allocateDescriptorSet(m_device, m_dstPool, m_dstLayout);
}

void PipelinePost::createPostPipeline()
{
	auto &m_debug  = m_pContext->m_debug;
	auto  m_device = m_pContext->getDevice();

	//// Push constants in the fragment shader
	// VkPushConstantRange pushConstantRanges = { VK_SHADER_STAGE_FRAGMENT_BIT, 0,
	// sizeof(Tonemapper) };

	// Creating the pipeline layout
	VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	createInfo.setLayoutCount = 1;
	createInfo.pSetLayouts    = &m_dstLayout;
	// createInfo.pushConstantRangeCount = 1;
	createInfo.pushConstantRangeCount = 0;
	createInfo.pPushConstantRanges    = VK_NULL_HANDLE;
	vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);

	// Pipeline: completely generic, no vertices
	nvvk::GraphicsPipelineGeneratorCombined pipelineGenerator(m_device, m_pipelineLayout,
	                                                          m_pContext->getRenderPass());
	pipelineGenerator.addShader(
	    nvh::loadFile("shaders/post.idle.vert.spv", true, m_pContext->m_root),
	    VK_SHADER_STAGE_VERTEX_BIT);
	pipelineGenerator.addShader(
	    nvh::loadFile("shaders/post.idle.frag.spv", true, m_pContext->m_root),
	    VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineGenerator.rasterizationState.cullMode = VK_CULL_MODE_NONE;

	m_pipeline = pipelineGenerator.createPipeline();
	NAME2_VK(m_pipeline, "Post");
}

void PipelinePost::updatePostDescriptorSet()
{
	auto m_device = m_pContext->getDevice();

	VkWriteDescriptorSet wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	wds.dstSet          = m_dstSet;
	wds.dstBinding      = GPUBindingPost::eGPUBindingPostImage;
	wds.descriptorCount = 1;
	wds.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	wds.pImageInfo      = &m_pPipGraphics->m_tChannels[0].descriptor;
	vkUpdateDescriptorSets(m_device, 1, &wds, 0, nullptr);
}
