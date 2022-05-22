#include "pipeline.h"

void DescriptorSetWrapper::deinit(ContextAware* pContext)
{
  assert((pContext != nullptr) && "[!] Pipeline Error: failed to find belonging context when deinit.");

  auto& m_alloc  = pContext->getAlloc();
  auto  m_device = pContext->getDevice();

  vkDestroyDescriptorPool(m_device, m_dstPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_dstLayout, nullptr);
  m_dstPool   = VK_NULL_HANDLE;
  m_dstLayout = VK_NULL_HANDLE;
  m_dstBind   = {};
}

PipelineAware::PipelineAware(uint numHoldSets, uint numBindSets)
{
  m_holdSetWrappers.reserve(numHoldSets);
  for(int setId = 0; setId < numHoldSets; setId++)
    m_holdSetWrappers.emplace_back(DescriptorSetWrapper());
  m_bindSetWrappers.reserve(numBindSets);
  m_bindSets.reserve(numBindSets);
  for(int setId = 0; setId < numBindSets; setId++)
  {
    m_bindSetWrappers.emplace_back(nullptr);
    m_bindSets.emplace_back(VK_NULL_HANDLE);
  }
}

void PipelineAware::deinit()
{
  assert((m_pContext != nullptr) && "[!] Pipeline Error: failed to find belonging context when deinit.");

  auto& m_alloc  = m_pContext->getAlloc();
  auto  m_device = m_pContext->getDevice();

  vkDestroyPipeline(m_device, m_pipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

  for(auto& descriptorSetWrapper : m_holdSetWrappers)
    descriptorSetWrapper.deinit(m_pContext);
  m_holdSetWrappers.clear();
  m_bindSetWrappers.clear();
  m_bindSets.clear();

  m_pContext = nullptr;
  m_pScene   = nullptr;
}

void PipelineAware::bind(uint bindPoint, DescriptorSetWrapper* bindSet)
{
  m_bindSetWrappers[bindPoint] = bindSet;
  m_bindSets[bindPoint]        = bindSet->getDescriptorSet();
}