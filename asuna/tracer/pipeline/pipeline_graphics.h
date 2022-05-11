#pragma once

#include "../../hostdevice/camera.h"
#include "../../hostdevice/pushconstant.h"
#include "pipeline.h"

#include <vector>

class PipelineGraphics : public PipelineAware
{
  public:
    START_ENUM(Set)
    eSetOut = 0, eSetScene = 1, eSetEnv = 2 END_ENUM();
    START_ENUM(RunSet)
    eRunSetOut = 0, eRunSetScene = 1, eRunSetEnv = 2 END_ENUM();
    PipelineGraphics() : PipelineAware(3, 3)
    {}
    virtual void init(ContextAware *pContext, Scene *pScene);
    virtual void run(const VkCommandBuffer &cmdBuf);
    virtual void deinit();

    void                         updateCameraBuffer(const VkCommandBuffer &cmdBuf);
    void                         updateSunAndSky(const VkCommandBuffer &cmdBuf);
    const VkDescriptorImageInfo *getHdrOutImageInfo();
    DescriptorSetWrapper        *getOutDescriptorSet();
    DescriptorSetWrapper        *getSceneDescriptorSet();
    DescriptorSetWrapper        *getEnvDescriptorSet();

  public:
    // Canvas we draw things on
    std::vector<nvvk::Texture> m_tChannels{};
    // Depth buffer
    nvvk::Texture m_tDepth;
    // Multiple output channels

  private:
    VkRenderPass  m_offscreenRenderPass{VK_NULL_HANDLE};
    VkFramebuffer m_offscreenFramebuffer{VK_NULL_HANDLE};
    // Record unprocessed command when interruption happen
    // -- used by raster to replay rendering commands
    VkCommandBuffer m_recordedCmdBuffer{VK_NULL_HANDLE};
    // Device-Host of the camera matrices
    nvvk::Buffer m_bCamera;
    // Push constant
    GPUPushConstantGraphics m_pcGraphics{0};

  private:
    // Creating an offscreen frame buffer and the associated render pass
    void createOffscreenResources();
    // Describing the layout pushed when rendering
    void createGraphicsDescriptorSetLayout();
    // Create graphic pipeline with vertex and fragment shader
    void createGraphicsPipeline();
    // Creating the uniform buffer holding the camera matrices
    void createCameraBuffer();
    // Setting up the buffers in the descriptor set
    void updateGraphicsDescriptorSet();
};