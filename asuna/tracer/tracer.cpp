#include "tracer.h"

#include <iostream>
using namespace std;

#include <nvvk/structs_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <backends/imgui_impl_glfw.h>

// **************************************************************************
// functions exposed to main.cpp
// **************************************************************************
void Tracer::init()
{
    m_context.init();
}

void Tracer::run()
{
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { 0.0f,0.0f,0.0f,0.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };
    // Main loop
    while (!glfwWindowShouldClose(m_context.m_glfw)) {
        glfwPollEvents();
        if (m_context.isMinimized()) continue;
        // Start the Dear ImGui frame
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Start rendering the scene
        m_context.prepareFrame();
        // Start command buffer of this frame
        uint32_t curFrame = m_context.getCurFrame();
        const VkCommandBuffer& cmdBuf = m_context.getCommandBuffers()[curFrame];
        VkCommandBufferBeginInfo beginInfo = nvvk::make<VkCommandBufferBeginInfo>();
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(cmdBuf, &beginInfo);
        {
            {
                VkRenderPassBeginInfo postRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
                postRenderPassBeginInfo.clearValueCount = 2;
                postRenderPassBeginInfo.pClearValues = clearValues.data();
                postRenderPassBeginInfo.renderPass = m_context.getRenderPass();
                postRenderPassBeginInfo.framebuffer = m_context.getFramebuffers()[curFrame];
                postRenderPassBeginInfo.renderArea = { {0, 0}, m_context.getSize() };

                // Rendering to the swapchain framebuffer the rendered image and apply a tonemapper
                vkCmdBeginRenderPass(cmdBuf, &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                // Rendering UI
                ImGui::Render();
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

                // Display axis in the lower left corner.
                //vkAxis.display(cmdBuf, CameraManip.getMatrix(), vkSample.getSize());

                vkCmdEndRenderPass(cmdBuf);
            }
        }
        vkEndCommandBuffer(cmdBuf);
        m_context.submitFrame();
    }
    vkDeviceWaitIdle(m_context.getDevice());
}

void Tracer::deinit()
{
    m_context.deinit();
}