#include "pipeline_post.h"

#include "../../hostdevice/binding.h"

#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/debug_util_vk.hpp>

#include <cstdint>
#include <vector>

void PipelinePost::init(ContextAware* pContext, Scene* pScene, const VkDescriptorImageInfo* pImageInfo)
{
  m_pContext = pContext;
  m_pScene   = pScene;
  // Ray tracing
  nvh::Stopwatch sw_;
  m_pushconstant.useTonemapping = m_pScene->getUseToneMapping();
  createPostDescriptorSetLayout();
  bind(PostBindSet::PostInput, {&m_holdSetWrappers[uint(HoldSet::Input)]});
  createPostPipeline();
  updatePostDescriptorSet(pImageInfo);
  LOGI("[ ] %-20s: %6.2fms Post pipeline creation\n", "Pipeline", sw_.elapsed());
}

void PipelinePost::run(const VkCommandBuffer& cmdBuf)
{
  auto& m_debug = m_pContext->getDebug();

  LABEL_SCOPE_VK(cmdBuf);

  m_pContext->setViewport(cmdBuf);
  vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GpuPushConstantPost), &m_pushconstant);
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, m_bindSets.data(), 0, nullptr);
  vkCmdDraw(cmdBuf, 3, 1, 0, 0);
}

void PipelinePost::deinit()
{
  PipelineAware::deinit();
}

void PipelinePost::createPostDescriptorSetLayout()
{
  auto m_device = m_pContext->getDevice();

  auto& inputWrap   = m_holdSetWrappers[uint(HoldSet::Input)];
  auto& inputBind   = inputWrap.getDescriptorSetBindings();
  auto& inputPool   = inputWrap.getDescriptorPool();
  auto& inputSet    = inputWrap.getDescriptorSet();
  auto& inputLayout = inputWrap.getDescriptorSetLayout();
  inputBind.addBinding(InputBindings::InputSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
  inputLayout = inputBind.createLayout(m_device);
  inputPool   = inputBind.createPool(m_device);
  inputSet    = nvvk::allocateDescriptorSet(m_device, inputPool, inputLayout);
}

void PipelinePost::createPostPipeline()
{
  auto& m_debug  = m_pContext->getDebug();
  auto  m_device = m_pContext->getDevice();

  // Push constants in the fragment shader
  VkPushConstantRange pushConstantRanges{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GpuPushConstantPost)};

  // Creating the pipeline layout
  VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  createInfo.setLayoutCount         = 1;
  createInfo.pSetLayouts            = &m_bindSetWrappers[PostBindSet::PostInput]->getDescriptorSetLayout();
  createInfo.pushConstantRangeCount = 1;
  createInfo.pPushConstantRanges    = &pushConstantRanges;
  vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);

  // Pipeline: completely generic, no vertices
  auto&                                   root = m_pContext->getRoot();
  nvvk::GraphicsPipelineGeneratorCombined pipelineGenerator(m_device, m_pipelineLayout, m_pContext->getRenderPass());
  pipelineGenerator.addShader(nvh::loadFile("shaders/post.idle.vert.spv", true, m_pContext->getRoot()), VK_SHADER_STAGE_VERTEX_BIT);
  pipelineGenerator.addShader(nvh::loadFile("shaders/post.idle.frag.spv", true, m_pContext->getRoot()), VK_SHADER_STAGE_FRAGMENT_BIT);
  pipelineGenerator.rasterizationState.cullMode = VK_CULL_MODE_NONE;

  m_pipeline = pipelineGenerator.createPipeline();
  NAME2_VK(m_pipeline, "Post");
}

void PipelinePost::updatePostDescriptorSet(const VkDescriptorImageInfo* pImageInfo)
{
  auto m_device = m_pContext->getDevice();

  auto& inputWrap   = m_holdSetWrappers[uint(HoldSet::Input)];
  auto& inputBind   = inputWrap.getDescriptorSetBindings();
  auto& inputPool   = inputWrap.getDescriptorPool();
  auto& inputSet    = inputWrap.getDescriptorSet();
  auto& inputLayout = inputWrap.getDescriptorSetLayout();

  VkWriteDescriptorSet wds{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  wds.dstSet          = inputSet;
  wds.dstBinding      = 0;
  wds.descriptorCount = 1;
  wds.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  wds.pImageInfo      = pImageInfo;
  vkUpdateDescriptorSets(m_device, 1, &wds, 0, nullptr);
}
