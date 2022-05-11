#pragma once

#include "../context/context.h"
#include "../scene/scene.h"

#include <nvvk/descriptorsets_vk.hpp>

#include <array>

class PipelineAware;

// struct PipelineInitState
//{
//     ContextAware                  *m_pContext = nullptr;
//     Scene                         *m_pScene   = nullptr;
//     std::array<PipelineAware *, 3> m_pCorrPips{};
// };

class DescriptorSetWrapper
{
  public:
    VkDescriptorSet             m_dstSet{VK_NULL_HANDLE};
    VkDescriptorSetLayout       m_dstLayout{VK_NULL_HANDLE};
    VkDescriptorPool            m_dstPool{VK_NULL_HANDLE};
    nvvk::DescriptorSetBindings m_dstSetLayoutBind{};
    void                        deinit(ContextAware *m_pContext)
    {
        assert((m_pContext != nullptr) &&
               "[!] Pipeline Error: failed to find belonging context when deinit.");

        auto &m_alloc  = m_pContext->m_alloc;
        auto  m_device = m_pContext->getDevice();

        vkDestroyDescriptorPool(m_device, m_dstPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_dstLayout, nullptr);
        m_dstPool          = VK_NULL_HANDLE;
        m_dstLayout        = VK_NULL_HANDLE;
        m_dstSetLayoutBind = {};
    };
};

// All it needs to create a pipeline
class PipelineAware
{
  public:
    // virtual void init(PipelineInitState pPis)       = 0;
    // virtual void run(const VkCommandBuffer &cmdBuf) = 0;
    PipelineAware(int nSets, int nRunSets)
    {
        m_descriptorSets.reserve(nSets);
        for (int setId = 0; setId < nSets; setId++)
            m_descriptorSets.emplace_back(DescriptorSetWrapper());
        m_runSets.reserve(nRunSets);
        for (int setId = 0; setId < nRunSets; setId++)
            m_runSets.emplace_back(VK_NULL_HANDLE);
        m_runWrappers.resize(nRunSets);
    }
    void deinit()
    {
        assert((m_pContext != nullptr) &&
               "[!] Pipeline Error: failed to find belonging context when deinit.");

        auto &m_alloc  = m_pContext->m_alloc;
        auto  m_device = m_pContext->getDevice();

        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

        for (auto &dsw : m_descriptorSets)
            dsw.deinit(m_pContext);
        m_descriptorSets.clear();

        m_pContext = nullptr;
        m_pScene   = nullptr;
    };
    void update(const std::vector<DescriptorSetWrapper *> &descSets)
    {
        assert(descSets.size() == m_runWrappers.size() &&
               "the size of descriptor sets must match!");
        m_runWrappers                   = descSets;
        std::vector<VkDescriptorSet> ds = {};
        for (auto &wrapper : m_runWrappers)
        {
            ds.emplace_back(wrapper->m_dstSet);
        }
        updateRunSets(ds);
    }

  private:
    void updateRunSets(const std::vector<VkDescriptorSet> &descSets)
    {
        assert(descSets.size() == m_runSets.size() && "the size of descriptor sets must match!");
        m_runSets = descSets;
    }

  public:
    ContextAware                       *m_pContext = nullptr;
    Scene                              *m_pScene   = nullptr;
    VkPipeline                          m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout                    m_pipelineLayout{VK_NULL_HANDLE};
    std::vector<DescriptorSetWrapper>   m_descriptorSets{};        // hold by itself
    std::vector<VkDescriptorSet>        m_runSets{};               // may borrow from others
    std::vector<DescriptorSetWrapper *> m_runWrappers{};
};