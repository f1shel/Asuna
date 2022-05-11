#pragma once

#include "../../hostdevice/pushconstant.h"
#include "pipeline.h"
#include "pipeline_graphics.h"

class PipelinePost : public PipelineAware
{
  public:
    START_ENUM(Set) eSetOut = 0 END_ENUM();
    START_ENUM(RunSet) eRunSetOut = 0 END_ENUM();
    PipelinePost() : PipelineAware(1, 1)
    {}
    virtual void init(ContextAware *pContext, Scene *pScene, const VkDescriptorImageInfo *pImageInfo);
    virtual void run(const VkCommandBuffer &cmdBuf);
    virtual void deinit();

  public:
    GPUPushConstantPost m_pcPost = {
        1.0f,                // brightness;
        1.0f,                // contrast;
        1.0f,                // saturation;
        0.0f,                // vignette;
        1.0f,                // avgLum;
        1.0f,                // zoom;
        {1.0f, 1.0f},        // renderingRatio;
        0,                   // autoExposure;
        0.5f,                // Ywhite;  // Burning white
        0.5f,                // key;     // Log-average luminance
    };

  private:
    void createPostDescriptorSetLayout();
    // Create post-processing pipeline
    void createPostPipeline();
    // Update the descriptor pointer
    void updatePostDescriptorSet(const VkDescriptorImageInfo *pImageInfo);
};