#pragma once

#include <context/context.h>
#include <scene/scene.h>
#include <nvvk/descriptorsets_vk.hpp>
#include <array>

class DescriptorSetWrapper
{
public:
  void deinit(ContextAware* pContext);

public:
  VkDescriptorSet&             getDescriptorSet() { return m_dstSet; }
  VkDescriptorSetLayout&       getDescriptorSetLayout() { return m_dstLayout; }
  VkDescriptorPool&            getDescriptorPool() { return m_dstPool; }
  nvvk::DescriptorSetBindings& getDescriptorSetBindings() { return m_dstBind; }

private:
  VkDescriptorSet             m_dstSet{VK_NULL_HANDLE};
  VkDescriptorSetLayout       m_dstLayout{VK_NULL_HANDLE};
  VkDescriptorPool            m_dstPool{VK_NULL_HANDLE};
  nvvk::DescriptorSetBindings m_dstBind{};
};

// All it needs to create a pipeline
class PipelineAware
{
public:
  PipelineAware(uint numHoldSets, uint numBindSets);
  void         deinit();
  void         bind(uint bindPoint, DescriptorSetWrapper* bindSet);

protected:
  virtual void initPushconstant() {}

protected:
  ContextAware*                 m_pContext = nullptr;
  Scene*                        m_pScene   = nullptr;
  VkPipeline                    m_pipeline{VK_NULL_HANDLE};
  VkPipelineLayout              m_pipelineLayout{VK_NULL_HANDLE};
  vector<DescriptorSetWrapper>  m_holdSetWrappers{};  // hold by itself
  vector<DescriptorSetWrapper*> m_bindSetWrappers{};  // may borrow from others
  vector<VkDescriptorSet>       m_bindSets{};
};