#pragma once

#include "pipeline.h"
#include "../../hostdevice/pushconstant.h"
#include "../../hostdevice/camera.h"

class PipelineGraphic : public PipelineAware
{
public:
    virtual void init(PipelineCorrelated* pPipCorr);
    virtual void run(const VkCommandBuffer& cmdBuf) {
        // Prepare new UBO contents on host.
        auto m_size = m_pContext->getSize();
        const float    aspectRatio = m_size.width / static_cast<float>(m_size.height);
        Camera hostCamera = {};
        const auto& view = CameraManip.getMatrix();
        const auto& proj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.1f, 1000.0f);
        // proj[1][1] *= -1;  // Inverting Y for Vulkan (not needed with perspectiveVK).

        hostCamera.viewProj = proj * view;
        hostCamera.viewInverse = nvmath::invert(view);
        hostCamera.projInverse = nvmath::invert(proj);

        // UBO on the device, and what stages access it.
        VkBuffer deviceUBO = m_bCamera.buffer;
        auto     uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

        // Ensure that the modified UBO is not visible to previous frames.
        VkBufferMemoryBarrier beforeBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        beforeBarrier.buffer = deviceUBO;
        beforeBarrier.offset = 0;
        beforeBarrier.size = sizeof(Camera);
        vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
            nullptr, 1, &beforeBarrier, 0, nullptr);


        // Schedule the host-to-device upload. (hostUBO is copied into the cmd
        // buffer so it is okay to deallocate when the function returns).
        vkCmdUpdateBuffer(cmdBuf, m_bCamera.buffer, 0, sizeof(Camera), &hostCamera);

        // Making sure the updated UBO will be visible.
        VkBufferMemoryBarrier afterBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
        afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        afterBarrier.buffer = deviceUBO;
        afterBarrier.offset = 0;
        afterBarrier.size = sizeof(hostCamera);
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
            nullptr, 1, &afterBarrier, 0, nullptr);
    };
    virtual void deinit();
public:
    // Canvas we draw things on
    nvvk::Texture m_tColor;
    // Depth buffer
    nvvk::Texture m_tDepth;
private:
    VkRenderPass  m_offscreenRenderPass{ VK_NULL_HANDLE };
    VkFramebuffer m_offscreenFramebuffer{ VK_NULL_HANDLE };
    // Record unprocessed command when interruption happen
    // -- used by raster to replay rendering commands
    VkCommandBuffer m_recordedCmdBuffer{ VK_NULL_HANDLE };
    // Device-Host of the camera matrices
    nvvk::Buffer m_bCamera;
    // Push constant
    PushConstantGraphic m_pcGraphic{ 0 };
private:
    // Creating an offscreen frame buffer and the associated render pass
    void createOffscreenResources();
    // Describing the layout pushed when rendering
    void createGraphicDescriptorSetLayout();
    // Create graphic pipeline with vertex and fragment shader
    void createGraphicPipeline();
    // Creating the uniform buffer holding the camera matrices
    void createCameraBuffer();
    // Setting up the buffers in the descriptor set
    void updateGraphicDescriptorSet();
};