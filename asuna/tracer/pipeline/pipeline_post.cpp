#include "pipeline_post.h"

#include "../../hostdevice/binding.h"

#include <nvvk/debug_util_vk.hpp>
#include <nvh/timesampler.hpp>
#include <nvh/fileoperations.hpp>

#include <cstdint>
#include <vector>

void PipelinePost::init(PipelineCorrelated* pPipCorr)
{
    m_pContext = pPipCorr->m_pContext;
    m_pScene = pPipCorr->m_pScene;
    m_pPipGraphic = ((PipelineCorrelatedPost*)pPipCorr)->m_pPipGraphic;
    // Ray tracing
    // 创建光线追踪管线
    nvh::Stopwatch sw_;
    createPostDescriptorSetLayout();
    createPostPipeline();
    updatePostDescriptorSet();
    LOGI("[ ] Pipeline: %6.2fms Post pipeline creation\n", sw_.elapsed());
}

void PipelinePost::deinit()
{
    m_pPipGraphic = nullptr;
    
    PipelineAware::deinit();
}

void PipelinePost::createPostDescriptorSetLayout()
{
    auto m_device = m_pContext->getDevice();

    // The descriptor layout is the description of the data that is passed to the vertex or the  fragment program.
    nvvk::DescriptorSetBindings& bind = m_dstSetLayoutBind;
    bind.addBinding(BindingsPost::eBindingPostImage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_dstLayout = bind.createLayout(m_device);
    m_dstPool = bind.createPool(m_device);
    m_dstSet = nvvk::allocateDescriptorSet(m_device, m_dstPool, m_dstLayout);
}

void PipelinePost::createPostPipeline()
{
    auto& m_debug = m_pContext->m_debug;
    auto m_device = m_pContext->getDevice();

    //// Push constants in the fragment shader
    //VkPushConstantRange pushConstantRanges = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Tonemapper) };

    // Creating the pipeline layout
    VkPipelineLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &m_dstLayout;
    //createInfo.pushConstantRangeCount = 1;
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges = VK_NULL_HANDLE;
    vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);

    // Pipeline: completely generic, no vertices
    nvvk::GraphicsPipelineGeneratorCombined pipelineGenerator(m_device, m_pipelineLayout, m_pContext->getRenderPass());
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

    VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds.dstSet = m_dstSet;
    wds.dstBinding = BindingsPost::eBindingPostImage;
    wds.descriptorCount = 1;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds.pImageInfo = &m_pPipGraphic->m_tColor.descriptor;
    vkUpdateDescriptorSets(m_device, 1, &wds, 0, nullptr);
}
