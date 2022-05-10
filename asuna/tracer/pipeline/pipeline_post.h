#pragma once

#include "../../hostdevice/pushconstant.h"
#include "pipeline.h"
#include "pipeline_graphics.h"

class PipelinePost : public PipelineAware
{
  public:
    // pis.
    virtual void init(PipelineInitState pis);
    virtual void run(const VkCommandBuffer &cmdBuf);
    virtual void deinit();

  private:
    // Accompanied graphic pipeline
    PipelineGraphics   *m_pPipGraphics = nullptr;

  public:
    GPUPushConstantPost m_pcPost       = {
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
    void updatePostDescriptorSet();
};