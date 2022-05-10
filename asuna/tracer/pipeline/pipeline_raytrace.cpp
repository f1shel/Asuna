#include "pipeline_raytrace.h"

#include <nvh/fileoperations.hpp>
#include <nvh/timesampler.hpp>
#include <nvvk/buffers_vk.hpp>
#include "nvvk/shaders_vk.hpp"

void PipelineRaytrace::init(PipelineInitState pis)
{
    m_pContext = pis.m_pContext;
    m_pScene   = pis.m_pScene;
    // pis.m_pCorrPips[0] should points to a graphics pipeline
    m_pPipGraphics = dynamic_cast<PipelineGraphics *>(pis.m_pCorrPips[0]);
    // Ray tracing
    nvh::Stopwatch sw_;
    initRayTracing();
    createBottomLevelAS();
    createTopLevelAS();
    createRtDescriptorSetLayout();
    createRtPipeline();
    updateRtDescriptorSet();
    LOGI("[ ] %-20s: %6.2fms Raytrace pipeline creation\n", "Pipeline", sw_.elapsed());
}

void PipelineRaytrace::deinit()
{
    m_blas.clear();
    m_tlas.clear();
    m_pPipGraphics = nullptr;
    m_pcRaytrace   = {0};

    m_rtBuilder.destroy();
    m_sbt.destroy();

    PipelineAware::deinit();
}

void PipelineRaytrace::run(const VkCommandBuffer &cmdBuf)
{
    // If the camera matrix has changed, resets the frame; otherwise, increments frame.
    static nvmath::mat4f refCamMatrix{0};
    static float         refFov{CameraManip.getFov()};

    const auto &m   = CameraManip.getMatrix();
    const auto  fov = CameraManip.getFov();

    if (memcmp(&refCamMatrix.a00, &m.a00, sizeof(nvmath::mat4f)) != 0 || refFov != fov)
    {
        m_pcRaytrace.curFrame = -1;
        refCamMatrix          = m;
        refFov                = fov;
    }
    m_pcRaytrace.curFrame++;

    // Do ray tracing
    std::array<VkDescriptorSet, eGPUSetRaytraceCount> descSets{};
    descSets[eGPUSetRaytraceRaytrace] = m_dstSet;
    descSets[eGPUSetRaytraceGraphics] = m_pPipGraphics->m_dstSet;
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0,
                            (uint32_t) descSets.size(), descSets.data(), 0, nullptr);
    vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_ALL, 0,
                       sizeof(GPUPushConstantRaytrace), &m_pcRaytrace);

    const auto &regions = m_sbt.getRegions();
    auto        size    = m_pContext->getSize();
    vkCmdTraceRaysKHR(cmdBuf, &regions[0], &regions[1], &regions[2], &regions[3], size.width,
                      size.height, 1);
}

void PipelineRaytrace::setSpp(int spp)
{
    m_pcRaytrace.spp = spp;
}

void PipelineRaytrace::initRayTracing()
{
    auto &m_alloc              = m_pContext->m_alloc;
    auto  m_device             = m_pContext->getDevice();
    auto  m_physicalDevice     = m_pContext->getPhysicalDevice();
    auto  m_graphicsQueueIndex = m_pContext->getQueueFamily();

    // Requesting ray tracing properties
    VkPhysicalDeviceProperties2 prop2 = nvvk::make<VkPhysicalDeviceProperties2>();
    prop2.pNext                       = &m_rtProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

    m_rtBuilder.setup(m_device, &m_alloc, m_graphicsQueueIndex);
    m_sbt.setup(m_device, m_graphicsQueueIndex, &m_alloc, m_rtProperties);

    // Update push constant from scene
    m_pcRaytrace.curFrame          = -1;
    m_pcRaytrace.spp               = m_pScene->getSpp();
    m_pcRaytrace.maxRecursionDepth = m_pScene->getMaxRecurDepth();
    m_pcRaytrace.emittersNum         = m_pScene->getEmittersNum();
}

static nvvk::RaytracingBuilderKHR::BlasInput MeshBufferToBlas(VkDevice         device,
                                                              const MeshAlloc &meshAlloc)
{
    // BLAS builder requires raw device addresses.
    VkDeviceAddress vertexAddress =
        nvvk::getBufferDeviceAddress(device, meshAlloc.getVerticesBuffer());
    VkDeviceAddress indexAddress =
        nvvk::getBufferDeviceAddress(device, meshAlloc.getIndicesBuffer());

    uint32_t maxPrimitiveCount = meshAlloc.m_nIndices / 3;

    // Describe buffer as array of Vertex.
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;        // vec3 vertex position data.
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride             = sizeof(GPUVertex);
    // Describe index data (32-bit unsigned int)
    triangles.indexType               = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;
    // Indicate identity transform by setting transformData to null device pointer.
    // triangles.transformData = {};
    triangles.maxVertex = meshAlloc.m_nVertices;

    // Identify the above data as containing opaque triangles.
    VkAccelerationStructureGeometryKHR asGeom{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeom.geometry.triangles = triangles;

    // The entire array will be used to build the BLAS.
    VkAccelerationStructureBuildRangeInfoKHR offset;
    offset.firstVertex     = 0;
    offset.primitiveCount  = maxPrimitiveCount;
    offset.primitiveOffset = 0;
    offset.transformOffset = 0;

    // Our blas is made from only one geometry, but could be made of many geometries
    nvvk::RaytracingBuilderKHR::BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(offset);

    return input;
}

void PipelineRaytrace::createBottomLevelAS()
{
    auto m_device = m_pContext->getDevice();
    // BLAS - Storing each primitive in a geometry
    m_blas.reserve(m_pScene->getMeshesNum());
    for (uint32_t meshId = 0; meshId < m_pScene->getMeshesNum(); meshId++)
    {
        auto blas = MeshBufferToBlas(m_device, *m_pScene->getMeshAlloc(meshId));
        // We could add more geometry in each BLAS, but we add only one for now
        m_blas.push_back(blas);
    }
    m_rtBuilder.buildBlas(m_blas, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                                      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

void PipelineRaytrace::createTopLevelAS()
{
    m_tlas.reserve(m_pScene->getInstancesNum());
    const auto &instances = m_pScene->getInstances();
    for (uint32_t instId = 0; instId < m_pScene->getInstancesNum(); instId++)
    {
        auto                               pInst = instances[instId];
        VkAccelerationStructureInstanceKHR rayInst{};
        rayInst.transform           = nvvk::toTransformMatrixKHR(pInst->m_transform);
        rayInst.instanceCustomIndex = instId;        // gl_InstanceCustomIndexEXT
        rayInst.accelerationStructureReference =
            m_rtBuilder.getBlasDeviceAddress(pInst->m_meshIndex);
        rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        rayInst.mask  = 0xFF;        //  Only be hit if rayMask & instance.mask != 0
        rayInst.instanceShaderBindingTableRecordOffset =
            0;        // We will use the same hit group for all objects
        m_tlas.emplace_back(rayInst);
    }

    m_rtBuilder.buildTlas(m_tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR);
}

void PipelineRaytrace::createRtDescriptorSetLayout()
{
    auto m_device = m_pContext->getDevice();

    // This descriptor set, holds the top level acceleration structure and the output image
    nvvk::DescriptorSetBindings &bind = m_dstSetLayoutBind;

    // Create Binding Set
    bind.addBinding(eGPUBindingRaytraceTlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                    VK_SHADER_STAGE_ALL);
    bind.addBinding(eGPUBindingRaytraceChannels, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    eGPUBindingRaytraceChannelCount, VK_SHADER_STAGE_ALL);
    m_dstPool   = bind.createPool(m_device);
    m_dstLayout = bind.createLayout(m_device);
    m_dstSet    = nvvk::allocateDescriptorSet(m_device, m_dstPool, m_dstLayout);
}

void PipelineRaytrace::createRtPipeline()
{
    auto &m_debug  = m_pContext->m_debug;
    auto  m_device = m_pContext->getDevice();

    // Creating all shaders
    enum StageIndices
    {
        eRayGen,
        eRayMiss,
        eClosetHit,
        eShadowMiss,
        eShaderGroupCount
    };
    std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
    VkPipelineShaderStageCreateInfo stage = nvvk::make<VkPipelineShaderStageCreateInfo>();
    stage.pName                           = "main";        // All the same entry point
    // Raygen
    std::string modulePath = "";
    if (m_pScene->getCameraType() == CameraType::eCameraTypePerspective)
        modulePath = "shaders/raytrace.perspective.rgen.spv";
    else if (m_pScene->getCameraType() == CameraType::eCameraTypePinhole)
        modulePath = "shaders/raytrace.pinhole.rgen.spv";
    stage.module =
        nvvk::createShaderModule(m_device, nvh::loadFile(modulePath, true, m_pContext->m_root));
    stage.stage     = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    stages[eRayGen] = stage;
    NAME2_VK(stage.module, "RayGen");
    // Miss
    stage.module = nvvk::createShaderModule(
        m_device, nvh::loadFile("shaders/raytrace.default.rmiss.spv", true, m_pContext->m_root));
    stage.stage      = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eRayMiss] = stage;
    NAME2_VK(stage.module, "RayMiss");
    // Closet hit
    stage.module = nvvk::createShaderModule(
        m_device,
        nvh::loadFile("shaders/raytrace.brdf_lambertian.rchit.spv", true, m_pContext->m_root));
    stage.stage        = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    stages[eClosetHit] = stage;
    NAME2_VK(stage.module, "ClosetHit");
    // Shadow miss
    stage.module = nvvk::createShaderModule(
        m_device, nvh::loadFile("shaders/raytrace.shadow.rmiss.spv", true, m_pContext->m_root));
    stage.stage         = VK_SHADER_STAGE_MISS_BIT_KHR;
    stages[eShadowMiss] = stage;
    NAME2_VK(stage.module, "Shadowmiss");
    // Shader groups
    VkRayTracingShaderGroupCreateInfoKHR group =
        nvvk::make<VkRayTracingShaderGroupCreateInfoKHR>();
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

    group.anyHitShader       = VK_SHADER_UNUSED_KHR;
    group.closestHitShader   = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;
    group.generalShader      = VK_SHADER_UNUSED_KHR;

    // Raygen
    group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = eRayGen;
    shaderGroups.push_back(group);

    // Miss
    group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = eRayMiss;
    shaderGroups.push_back(group);

    // shadow miss
    group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = eShadowMiss;
    shaderGroups.push_back(group);

    // closest hit shader
    group.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    group.generalShader    = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = eClosetHit;
    shaderGroups.push_back(group);

    // Push constant: we want to be able to update constants used by the shaders
    VkPushConstantRange pushConstant{VK_SHADER_STAGE_ALL, 0, sizeof(GPUPushConstantRaytrace)};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;

    // Descriptor sets: one specific to ray tracing, and one shared with the rasterization
    // pipeline
    std::array<VkDescriptorSetLayout, eGPUSetRaytraceCount> rtDescSetLayouts{};

    rtDescSetLayouts[eGPUSetRaytraceRaytrace] = m_dstLayout;
    rtDescSetLayouts[eGPUSetRaytraceGraphics] = m_pPipGraphics->m_dstLayout;
    pipelineLayoutCreateInfo.setLayoutCount   = static_cast<uint32_t>(rtDescSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts      = rtDescSetLayouts.data();
    vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);

    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    rayPipelineInfo.stageCount =
        static_cast<uint32_t>(stages.size());        // Stages are shaders
    rayPipelineInfo.pStages                      = stages.data();
    rayPipelineInfo.groupCount                   = static_cast<uint32_t>(shaderGroups.size());
    rayPipelineInfo.pGroups                      = shaderGroups.data();
    rayPipelineInfo.maxPipelineRayRecursionDepth = 4;        // Ray depth
    rayPipelineInfo.layout                       = m_pipelineLayout;
    vkCreateRayTracingPipelinesKHR(m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayPipelineInfo,
                                   nullptr, &m_pipeline);

    // Creating the SBT
    m_sbt.create(m_pipeline, rayPipelineInfo);

    // Removing temp modules
    for (auto &s : stages)
        vkDestroyShaderModule(m_device, s.module, nullptr);
}

void PipelineRaytrace::updateRtDescriptorSet()
{
    auto m_device = m_pContext->getDevice();

    std::vector<VkWriteDescriptorSet> writes;
    // This descriptor set, holds the top level acceleration structure and the output image
    nvvk::DescriptorSetBindings &bind = m_dstSetLayoutBind;

    // Write to descriptors
    VkAccelerationStructureKHR                   tlas = m_rtBuilder.getAccelerationStructure();
    VkWriteDescriptorSetAccelerationStructureKHR descASInfo{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    descASInfo.accelerationStructureCount = 1;
    descASInfo.pAccelerationStructures    = &tlas;
    writes.emplace_back(
        bind.makeWrite(m_dstSet, GPUBindingRaytrace::eGPUBindingRaytraceTlas, &descASInfo));

    std::array<VkDescriptorImageInfo, eGPUBindingRaytraceChannelCount> imageInfos{};
    for (uint channelId = 0; channelId < eGPUBindingRaytraceChannelCount; channelId++)
    {
        VkDescriptorImageInfo imageInfo{
            {},
            m_pPipGraphics->m_tChannels[channelId].descriptor.imageView,
            VK_IMAGE_LAYOUT_GENERAL};
        imageInfos[channelId] = imageInfo;
    }
    writes.push_back(
        bind.makeWriteArray(m_dstSet, eGPUBindingRaytraceChannels, imageInfos.data()));
    vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}
