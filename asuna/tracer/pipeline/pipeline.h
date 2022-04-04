#pragma once

#include "../context/context.h"

#include <nvvk/descriptorsets_vk.hpp>

// All it needs to create a pipeline
class PipelineAware
{
public:
    virtual void init(ContextAware* pContext) = 0;
    virtual void run() = 0;
    virtual void deinit() {
        assert((m_pContext != nullptr) && "[!] Pipeline Error: failed to find belonging context when deinit.");

        auto& m_alloc = m_pContext->m_alloc;
        auto& m_device = m_pContext->m_vkcontext.m_device;

        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyDescriptorPool(m_device, m_dstPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_dstLayout, nullptr);
        m_pipeline = VK_NULL_HANDLE;
        m_pipelineLayout = VK_NULL_HANDLE;
        m_dstPool = VK_NULL_HANDLE;
        m_dstLayout = VK_NULL_HANDLE;
        m_dstSetLayoutBind = {};
    };
public:
    ContextAware* m_pContext = nullptr;
    VkPipeline m_pipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_dstSet{ VK_NULL_HANDLE };
    VkDescriptorSetLayout m_dstLayout{ VK_NULL_HANDLE };
    VkDescriptorPool m_dstPool{ VK_NULL_HANDLE };
    nvvk::DescriptorSetBindings m_dstSetLayoutBind{};
};