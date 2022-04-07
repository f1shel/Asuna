#include "pipeline_raytrace.h"

#include <nvh/timesampler.hpp>
#include "nvvk/shaders_vk.hpp"

#include "../../assets/@autogen/raytrace.uvtest.rgen.h"
#include "../../assets/@autogen/raytrace.uvtest.rchit.h"
#include "../../assets/@autogen/raytrace.uvtest.rmiss.h"

void PipelineRaytrace::init(PipelineCorrelated* pPipCorr)
{
    m_pContext = pPipCorr->m_pContext;
    m_pScene = pPipCorr->m_pScene;
    m_pPipGraphic = ((PipelineCorrelatedRaytrace*)pPipCorr)->m_pPipGraphic;
    // Ray tracing
    // 创建光线追踪管线
    nvh::Stopwatch sw_;
    initRayTracing();
    createBottomLevelAS();
    createTopLevelAS();
    createRtDescriptorSetLayout();
    createRtPipeline();
    updateRtDescriptorSet();
    LOGI("[ ] Pipeline: %6.2fms Raytrace pipeline creation\n", sw_.elapsed());
}

void PipelineRaytrace::deinit()
{
}

void PipelineRaytrace::initRayTracing()
{
    auto& m_alloc = m_pContext->m_alloc;
    auto& m_device = m_pContext->m_vkcontext.m_device;
    auto& m_physicalDevice = m_pContext->m_vkcontext.m_physicalDevice;
    auto m_graphicsQueueIndex = m_pContext->getAppGraphicsQueueIndex();

    // Requesting ray tracing properties
    VkPhysicalDeviceProperties2 prop2 = nvvk::make<VkPhysicalDeviceProperties2>();;
    prop2.pNext = &m_rtProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

    m_rtBuilder.setup(m_device, &m_alloc, m_graphicsQueueIndex);
    m_sbt.setup(m_device, m_graphicsQueueIndex, &m_alloc, m_rtProperties);
}

// TODO: add geometry
void PipelineRaytrace::createBottomLevelAS()
{
    // BLAS - Storing each primitive in a geometry
    m_blas.clear();
    //m_rtBuilder.buildBlas(m_blas, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
    //    | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

// TODO: add geometry
void PipelineRaytrace::createTopLevelAS()
{
    m_tlas.clear();
    //m_rtBuilder.buildTlas(m_tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
    //    | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
}

void PipelineRaytrace::createRtDescriptorSetLayout()
{
    auto& m_device = m_pContext->m_vkcontext.m_device;

    // This descriptor set, holds the top level acceleration structure and the output image
    nvvk::DescriptorSetBindings& bind = m_dstSetLayoutBind;

    // Create Binding Set
    //bind.addBinding(BindingsRaytrace::eBindingRaytraceTlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_ALL);
    bind.addBinding(BindingsRaytrace::eBindingRaytraceImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL);
    m_dstPool = bind.createPool(m_device);
    m_dstLayout = bind.createLayout(m_device);
    m_dstSet = nvvk::allocateDescriptorSet(m_device, m_dstPool, m_dstLayout);
}

void PipelineRaytrace::createRtPipeline()
{
    auto& m_debug = m_pContext->m_debug;
    auto& m_device = m_pContext->m_vkcontext.m_device;

    // Creating all shaders
    enum StageIndices
    {
        eRaygen,
        eRayMiss,
        eClosestHit,
        eShadowMiss,
        eShaderGroupCount
    };
    std::array<VkPipelineShaderStageCreateInfo, 4> stages{};
    VkPipelineShaderStageCreateInfo stage =
        nvvk::make<VkPipelineShaderStageCreateInfo>();
    stage.pName = "main";  // All the same entry point
    // Raygen
    stage.module = nvvk::createShaderModule(m_device, raytrace_uvtest_rgen, sizeof(raytrace_uvtest_rgen));
    stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[eRaygen] = stage;
    NAME2_VK(stage.module, "Raygen");
    // Miss
    stage.module = nvvk::createShaderModule(m_device, raytrace_uvtest_rmiss, sizeof(raytrace_uvtest_rmiss));
    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eRayMiss] = stage;
    NAME2_VK(stage.module, "RayMiss");
    // Miss
    stage.module = nvvk::createShaderModule(m_device, raytrace_uvtest_rmiss, sizeof(raytrace_uvtest_rmiss));
    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eShadowMiss] = stage;
    NAME2_VK(stage.module, "ShadowMiss");
    // Closet hit
    stage.module = nvvk::createShaderModule(m_device, raytrace_uvtest_rchit, sizeof(raytrace_uvtest_rchit));
    stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[eClosestHit] = stage;
    NAME2_VK(stage.module, "Closethit");
    // Shader groups
    VkRayTracingShaderGroupCreateInfoKHR group =
        nvvk::make<VkRayTracingShaderGroupCreateInfoKHR>();
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

    // Raygen
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.generalShader = eRaygen;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(group);

    // Push constant: we want to be able to update constants used by the shaders
    VkPushConstantRange pushConstant{ VK_SHADER_STAGE_ALL, 0, sizeof(PushConstantRaytrace) };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
    std::vector<VkDescriptorSetLayout> rtDescSetLayouts = { m_dstLayout, m_pPipGraphic->m_dstLayout };
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayouts.data();
    vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);

    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
    rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());  // Stages are shaders
    rayPipelineInfo.pStages = stages.data();
    rayPipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    rayPipelineInfo.pGroups = shaderGroups.data();
    rayPipelineInfo.maxPipelineRayRecursionDepth = 2;  // Ray depth
    rayPipelineInfo.layout = m_pipelineLayout;
    vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayPipelineInfo, nullptr, &m_pipeline);

    // Creating the SBT
    m_sbt.create(m_pipeline, rayPipelineInfo);

    // Removing temp modules
    for (auto& s : stages)
        vkDestroyShaderModule(m_device, s.module, nullptr);
}

// TODO: add geometry
void PipelineRaytrace::updateRtDescriptorSet()
{
    auto& m_device = m_pContext->m_vkcontext.m_device;
    // This descriptor set, holds the top level acceleration structure and the output image
    nvvk::DescriptorSetBindings& bind = m_dstSetLayoutBind;
    // Write to descriptors
    //VkAccelerationStructureKHR tlas = m_rtBuilder.getAccelerationStructure();
    //VkWriteDescriptorSetAccelerationStructureKHR descASInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
    //descASInfo.accelerationStructureCount = 1;
    //descASInfo.pAccelerationStructures = &tlas;
    VkDescriptorImageInfo imageInfo{ {}, m_pPipGraphic->m_tColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

    std::vector<VkWriteDescriptorSet> writes;
    //writes.emplace_back(bind.makeWrite(m_dstSet, BindingsRaytrace::eBindingRaytraceTlas, &descASInfo));
    writes.emplace_back(bind.makeWrite(m_dstSet, BindingsRaytrace::eBindingRaytraceImage, &imageInfo));
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
