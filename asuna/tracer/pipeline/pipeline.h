#pragma once

#include "../context/context.h"
#include "../scene/scene.h"

#include <nvvk/descriptorsets_vk.hpp>

#include <array>

class PipelineAware;

struct PipelineInitState
{
  public:
	ContextAware                  *m_pContext = nullptr;
	Scene                         *m_pScene   = nullptr;
	std::array<PipelineAware *, 3> m_pCorrPips{};
};

// All it needs to create a pipeline
class PipelineAware
{
  public:
	virtual void init(PipelineInitState pPis)       = 0;
	virtual void run(const VkCommandBuffer &cmdBuf) = 0;
	virtual void deinit()
	{
		assert((m_pContext != nullptr) &&
		       "[!] Pipeline Error: failed to find belonging context when deinit.");

		auto &m_alloc  = m_pContext->m_alloc;
		auto  m_device = m_pContext->getDevice();

		vkDestroyPipeline(m_device, m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
		vkDestroyDescriptorPool(m_device, m_dstPool, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_dstLayout, nullptr);
		m_pipeline         = VK_NULL_HANDLE;
		m_pipelineLayout   = VK_NULL_HANDLE;
		m_dstPool          = VK_NULL_HANDLE;
		m_dstLayout        = VK_NULL_HANDLE;
		m_dstSetLayoutBind = {};

		m_pContext = nullptr;
		m_pScene   = nullptr;
	};

  public:
	ContextAware               *m_pContext = nullptr;
	Scene                      *m_pScene   = nullptr;
	VkPipeline                  m_pipeline{VK_NULL_HANDLE};
	VkPipelineLayout            m_pipelineLayout{VK_NULL_HANDLE};
	VkDescriptorSet             m_dstSet{VK_NULL_HANDLE};
	VkDescriptorSetLayout       m_dstLayout{VK_NULL_HANDLE};
	VkDescriptorPool            m_dstPool{VK_NULL_HANDLE};
	nvvk::DescriptorSetBindings m_dstSetLayoutBind{};
};