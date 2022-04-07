#pragma once

#include "pipeline.h"
#include "pipeline_graphic.h"

class PipelineCorrelatedPost : public PipelineCorrelated
{
public:
    PipelineGraphic* m_pPipGraphic = nullptr;
};

class PipelinePost : public PipelineAware
{
public:
    virtual void init(PipelineCorrelated* pPipCorr);
    virtual void run(const VkCommandBuffer& cmdBuf) {
        auto& m_debug = m_pContext->m_debug;

        LABEL_SCOPE_VK(cmdBuf);

        m_pContext->setViewport(cmdBuf);
        //vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Tonemapper), &m_tonemapper);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_dstSet, 0, nullptr);
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    };
private:
    // Accompanied graphic pipeline
    PipelineGraphic* m_pPipGraphic = nullptr;
private:
    void createPostDescriptorSetLayout();
    // Create post-processing pipeline
    void createPostPipeline();
    // Update the descriptor pointer
    void updatePostDescriptorSet();
};