#include "tracer.h"
#include "../loader/loader.h"

#include <backends/imgui_impl_glfw.h>
#include <nvh/timesampler.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/images_vk.hpp>
#include <nvvk/structs_vk.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "tqdm/tqdm.h"

#include <filesystem>
#include <iostream>

using std::filesystem::path;
using GuiH = ImGuiH::Control;

void Tracer::init(TracerInitState tis)
{
    m_tis = tis;

    m_context.init({m_tis.m_offline});

    m_scene.init(&m_context);

    Loader loader(&m_scene);
    loader.loadSceneFromJson(m_tis.m_scenefile, m_context.m_root);

    m_context.m_size = m_scene.getSize();
    if (!m_tis.m_offline)
        m_context.resizeGlfwWindow();
    else
        m_context.createOfflineResources();

    PipelineInitState pis{};
    pis.m_pContext     = &m_context;
    pis.m_pScene       = &m_scene;
    pis.m_pCorrPips[0] = &m_pipelineGraphics;

    m_pipelineGraphics.init(pis);
    m_pipelineRaytrace.init(pis);
    m_pipelinePost.init(pis);
}

void Tracer::run()
{
    if (m_tis.m_offline)
        runOffline();
    else
        runOnline();
}

void Tracer::deinit()
{
    m_pipelineGraphics.deinit();
    m_pipelineRaytrace.deinit();
    m_pipelinePost.deinit();
    m_scene.deinit();
    m_context.deinit();
}

void Tracer::runOnline()
{
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    // Main loop
    while (!m_context.shouldGlfwCloseWindow())
    {
        glfwPollEvents();
        if (m_context.isMinimized())
            continue;
        // Start the Dear ImGui frame
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Start rendering the scene
        m_context.prepareFrame();
        // Start command buffer of this frame
        uint32_t                 curFrame  = m_context.getCurFrame();
        const VkCommandBuffer   &cmdBuf    = m_context.getCommandBuffers()[curFrame];
        VkCommandBufferBeginInfo beginInfo = nvvk::make<VkCommandBufferBeginInfo>();
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        {
            vkBeginCommandBuffer(cmdBuf, &beginInfo);
            {
                m_pipelineGraphics.run(cmdBuf);
            }
            {
                renderGUI();
            }
            {
                m_pipelineRaytrace.run(cmdBuf);
            }
            {
                VkRenderPassBeginInfo postRenderPassBeginInfo{
                    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
                postRenderPassBeginInfo.clearValueCount = 2;
                postRenderPassBeginInfo.pClearValues    = clearValues.data();
                postRenderPassBeginInfo.renderPass      = m_context.getRenderPass();
                postRenderPassBeginInfo.framebuffer     = m_context.getFramebuffer(curFrame);
                postRenderPassBeginInfo.renderArea      = {{0, 0}, m_context.getSize()};

                // Rendering to the swapchain framebuffer the rendered image and apply a
                // tonemapper
                vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo,
                                     VK_SUBPASS_CONTENTS_INLINE);

                m_pipelinePost.run(cmdBuf);

                // Rendering UI
                ImGui::Render();
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

                // Display axis in the lower left corner.
                // vkAxis.display(cmdBuf, CameraManip.getMatrix(), vkSample.getSize());

                vkCmdEndRenderPass(cmdBuf);
            }
        }
        vkEndCommandBuffer(cmdBuf);
        m_context.submitFrame();
    }
    vkDeviceWaitIdle(m_context.getDevice());
}

void Tracer::runOffline()
{
    return;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    nvvk::CommandPool genCmdBuf(m_context.getDevice(), m_context.getQueueFamily());

    // Main loop
    tqdm bar;
    int  tot         = m_scene.getSpp();
    int  sppPerRound = 1;
    m_pipelineRaytrace.setSpp(sppPerRound);
    for (int spp = 0; spp < tot; spp += std::min(sppPerRound, tot - spp))
    {
        bar.progress(spp, tot);
        const VkCommandBuffer &cmdBuf = genCmdBuf.createCommandBuffer();
        {
            m_pipelineGraphics.run(cmdBuf);
        }
        {
            m_pipelineRaytrace.run(cmdBuf);
        }
        if (spp == tot)
        {
            VkRenderPassBeginInfo postRenderPassBeginInfo{
                VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            postRenderPassBeginInfo.clearValueCount = 2;
            postRenderPassBeginInfo.pClearValues    = clearValues.data();
            postRenderPassBeginInfo.renderPass      = m_context.getRenderPass();
            postRenderPassBeginInfo.framebuffer     = m_context.getFramebuffer();
            postRenderPassBeginInfo.renderArea      = {{0, 0}, m_context.getSize()};

            vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            m_pipelinePost.run(cmdBuf);

            vkCmdEndRenderPass(cmdBuf);
        }
        genCmdBuf.submitAndWait(cmdBuf);
    }
    bar.finish();
    vkDeviceWaitIdle(m_context.getDevice());
    saveImageTest();
}

void Tracer::imageToBuffer(const nvvk::Texture &imgIn, const VkBuffer &pixelBufferOut)
{
    nvvk::CommandPool genCmdBuf(m_context.getDevice(), m_context.getQueueFamily());
    VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

    // Make the image layout eTransferSrcOptimal to copy to buffer
    nvvk::cmdBarrierImageLayout(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

    // Copy the image to the buffer
    VkBufferImageCopy copyRegion;
    copyRegion.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageExtent       = {m_context.getSize().width, m_context.getSize().height, 1};
    copyRegion.imageOffset       = {0};
    copyRegion.bufferOffset      = 0;
    copyRegion.bufferImageHeight = m_context.getSize().height;
    copyRegion.bufferRowLength   = m_context.getSize().width;
    vkCmdCopyImageToBuffer(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           pixelBufferOut, 1, &copyRegion);

    // Put back the image as it was
    nvvk::cmdBarrierImageLayout(cmdBuf, imgIn.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
    genCmdBuf.submitAndWait(cmdBuf);
}

void Tracer::saveImageTest()
{
    auto &m_alloc = m_context.m_alloc;
    auto  m_size  = m_context.getSize();

    // Create a temporary buffer to hold the pixels of the image
    VkBufferUsageFlags usage{VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    VkDeviceSize       bufferSize = 4 * sizeof(float) * m_size.width * m_size.height;
    nvvk::Buffer       pixelBuffer =
        m_context.m_alloc.createBuffer(bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    saveImage(pixelBuffer, m_tis.m_outputname);
    // saveImage(pixelBuffer, "channel0.hdr", 0);
    // saveImage(pixelBuffer, "channel1.hdr", 1);
    // saveImage(pixelBuffer, "channel2.hdr", 2);
    // saveImage(pixelBuffer, "channel3.hdr", 3);

    // Destroy temporary buffer
    m_alloc.destroy(pixelBuffer);
}

void Tracer::saveImage(nvvk::Buffer pixelBuffer, std::string outputpath, int channelId)
{
    bool isRelativePath = path(outputpath).is_relative();
    if (isRelativePath)
        outputpath = NVPSystem::exePath() + outputpath;

    auto &m_alloc = m_context.m_alloc;
    auto  m_size  = m_context.getSize();

    if (channelId == -1)
        imageToBuffer(m_context.getOfflineFramebufferTexture(), pixelBuffer.buffer);
    else
        imageToBuffer(m_pipelineGraphics.m_tChannels[channelId], pixelBuffer.buffer);

    // Write the buffer to disk
    void *data = m_alloc.map(pixelBuffer);
    stbi_write_hdr(outputpath.c_str(), m_size.width, m_size.height, 4,
                   reinterpret_cast<float *>(data));
    m_alloc.unmap(pixelBuffer);
}

void Tracer::renderGUI()
{
    // Show UI panel window.
    float panelAlpha                = 1.0f;
    ImGuiH::Control::style.ctrlPerc = 0.55f;
    ImGuiH::Panel::Begin(ImGuiH::Panel::Side::Right, panelAlpha);

    bool changed{false};

    if (ImGui::CollapsingHeader("Camera" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
        changed |= guiCamera();

    ImGui::End();        // ImGui::Panel::end()
}

bool Tracer::guiCamera()
{
    bool changed{false};
    changed |= ImGuiH::CameraWidget();
    return changed;
}
