/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
 // ============================================================================
// modules/acontext.vulkan.context-commands.ixx
// Partition implementation: acontext.vulkan.context:commands
// Command buffers + sync + per-frame submit/present.
// ============================================================================

module;

#ifndef ALMOND_USING_VULKAN
#   define ALMOND_USING_VULKAN 1
#endif

#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

export module acontext.vulkan.context:commands;

import :shared_vk;
import aengine.core.context;
import aatlas.texture;

import <algorithm>;
import <array>;
import <cstdint>;
import <fstream>;
import <cstring>;
import <limits>;
import <span>;
import <stdexcept>;
import <vector>;

namespace epochnamespace::vulkancontext
{
    inline constexpr std::size_t kMaxFramesInFlight = 2;

    namespace
    {
        struct GuiBatch
        {
            const TextureAtlas* atlas = nullptr;
            std::uint32_t indexOffset = 0;
            std::uint32_t indexCount = 0;
        };
    }

    void Application::createCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = *commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = static_cast<std::uint32_t>(swapChainImages.size());

        auto [allocRes, bufs] = device->allocateCommandBuffersUnique(allocInfo);
        if (allocRes != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] allocateCommandBuffersUnique failed.");
        commandBuffers = std::move(bufs);
    }

    void Application::recordCommandBuffer(std::uint32_t imageIndex)
    {
        if (imageIndex >= commandBuffers.size())
            throw std::runtime_error("[Vulkan] recordCommandBuffer image index out of range.");

        vk::CommandBuffer cmd = *commandBuffers[imageIndex];
        (void)cmd.reset();

        vk::CommandBufferBeginInfo beginInfo{};
        if (cmd.begin(beginInfo) != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] CommandBuffer::begin failed.");
#if ALMOND_USE_CLEAR_COLOR_VULKAN
        std::array<vk::ClearValue, 2> clearValues{};
        const auto sceneClearColor = epochnamespace::core::clear_color_for_context(
            epochnamespace::core::ContextType::Vulkan);
        constexpr std::array<float, 4> frameClearColor{ 0.11f, 0.12f, 0.14f, 1.0f };
        clearValues[0].setColor(vk::ClearColorValue{ frameClearColor });
        clearValues[1].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });
#endif
        vk::RenderPassBeginInfo renderPassInfo{};
        renderPassInfo.renderPass = *renderPass;
        renderPassInfo.framebuffer = *framebuffers[imageIndex];
        renderPassInfo.renderArea = vk::Rect2D{ vk::Offset2D{0, 0}, swapChainExtent };
#if ALMOND_USE_CLEAR_COLOR_VULKAN
        renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
#endif

        cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

        const vk::Buffer vb[] = { *vertexBuffer };
        const vk::DeviceSize offsets[] = { 0 };
        cmd.bindVertexBuffers(0, 1, vb, offsets);

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipelineLayout,
            0,
            1,
            &*descriptorSets[imageIndex],
            0,
            nullptr
        );

        cmd.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);

        int viewportX = 0;
        int viewportY = 0;
        int viewportWidth = static_cast<int>(swapChainExtent.width);
        int viewportHeight = static_cast<int>(swapChainExtent.height);
        bool hasSceneViewport = false;

        if (const auto* ctx = bound_context())
        {
            const auto sceneViewport = ctx->scene_viewport();
            if (sceneViewport.valid())
            {
                hasSceneViewport = true;
                viewportX = (std::max)(0, (std::min)(sceneViewport.x, viewportWidth - 1));
                viewportY = (std::max)(0, (std::min)(sceneViewport.y, viewportHeight - 1));
                viewportWidth = (std::max)(1, (std::min)(sceneViewport.width, viewportWidth - viewportX));
                viewportHeight = (std::max)(1, (std::min)(sceneViewport.height, viewportHeight - viewportY));
            }
        }

        vk::Viewport viewport{};
        viewport.x = static_cast<float>(viewportX);
        viewport.y = static_cast<float>(viewportY);
        viewport.width = static_cast<float>(viewportWidth);
        viewport.height = static_cast<float>(viewportHeight);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmd.setViewport(0, viewport);

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{ viewportX, viewportY };
        scissor.extent = vk::Extent2D{
            static_cast<std::uint32_t>(viewportWidth),
            static_cast<std::uint32_t>(viewportHeight)
        };
        cmd.setScissor(0, scissor);

#if ALMOND_USE_CLEAR_COLOR_VULKAN
        if (hasSceneViewport)
        {
            vk::ClearAttachment sceneAttachment{};
            sceneAttachment.aspectMask = vk::ImageAspectFlagBits::eColor;
            sceneAttachment.colorAttachment = 0;
            sceneAttachment.clearValue.setColor(
                vk::ClearColorValue{ std::array<float, 4>{ sceneClearColor[0], sceneClearColor[1], sceneClearColor[2], sceneClearColor[3] } });

            vk::ClearRect sceneRect{};
            sceneRect.rect.offset = vk::Offset2D{ viewportX, viewportY };
            sceneRect.rect.extent = vk::Extent2D{
                static_cast<std::uint32_t>(viewportWidth),
                static_cast<std::uint32_t>(viewportHeight)
            };
            sceneRect.baseArrayLayer = 0;
            sceneRect.layerCount = 1;
            cmd.clearAttachments(1, &sceneAttachment, 1, &sceneRect);
        }
#endif

        // You MUST have this set when you create/fill the index buffer.
        const std::uint32_t safeIndexCount = indexCount;
        if (safeIndexCount == 0)
            throw std::runtime_error("[Vulkan] indexCount is 0. Set Application::indexCount when creating the index buffer.");

        cmd.drawIndexed(
            safeIndexCount,
            1u,
            0u,
            0,
            0u,
            VULKAN_HPP_DEFAULT_DISPATCHER
        );

        cmd.nextSubpass(vk::SubpassContents::eInline);

        if (auto* guiState = find_gui_state(bound_context()))
        {
            if (!guiState->guiDraws.empty())
                recordGuiCommands(cmd, imageIndex, *guiState);
        }

        cmd.endRenderPass();

        if (cmd.end() != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] CommandBuffer::end failed.");
    }

    void Application::enqueue_gui_draw(
        const epochnamespace::core::Context* ctx,
        const epochnamespace::SpriteHandle& sprite,
        std::span<const epochnamespace::TextureAtlas* const> atlases,
        float x,
        float y,
        float w,
        float h)
    {
        if (!sprite.is_valid())
            return;

        if (sprite.atlasIndex >= atlases.size())
            return;

        const auto* atlas = atlases[sprite.atlasIndex];
        if (!atlas)
            return;

        auto& guiState = gui_state_for_context(ctx);
        guiState.guiDraws.push_back(GuiDrawCommand{
            atlas,
            sprite.localIndex,
            x,
            y,
            w,
            h
            });
    }

    void Application::recordGuiCommands(vk::CommandBuffer cmd, std::uint32_t imageIndex, GuiContextState& guiState)
    {
        if (guiState.guiDraws.empty() || !device)
            return;

        if (!guiState.guiPipeline)
            createGuiPipeline();

        if (guiState.guiUniformBuffers.empty())
            createGuiUniformBuffers();

        std::vector<Vertex> vertices{};
        std::vector<std::uint32_t> indices{};
        std::vector<GuiBatch> batches{};

        vertices.reserve(guiState.guiDraws.size() * 4u);
        indices.reserve(guiState.guiDraws.size() * 6u);
        batches.reserve(guiState.guiDraws.size());

        const TextureAtlas* currentAtlas = nullptr;

        for (const auto& draw : guiState.guiDraws)
        {
            if (!draw.atlas)
                continue;

            AtlasRegion region{};
            if (!draw.atlas->try_get_entry_info(static_cast<int>(draw.localIndex), region))
                continue;

            const float x0 = draw.x;
            const float y0 = draw.y;
            const float x1 = draw.x + draw.w;
            const float y1 = draw.y + draw.h;

            const std::uint32_t baseIndex = static_cast<std::uint32_t>(vertices.size());
            // Atlas regions are stored in OpenGL UV space. Vulkan sampling uses
            // top-left image memory order here, so convert the V range once.
            const float vTop = 1.0f - region.v2;
            const float vBottom = 1.0f - region.v1;

            vertices.push_back(Vertex{ { x0, y0, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u1, vTop } });
            vertices.push_back(Vertex{ { x1, y0, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u2, vTop } });
            vertices.push_back(Vertex{ { x1, y1, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u2, vBottom } });
            vertices.push_back(Vertex{ { x0, y1, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u1, vBottom } });

            if (draw.atlas != currentAtlas)
            {
                currentAtlas = draw.atlas;
                batches.push_back(GuiBatch{
                    currentAtlas,
                    static_cast<std::uint32_t>(indices.size()),
                    0u
                    });
            }

            indices.push_back(baseIndex + 0u);
            indices.push_back(baseIndex + 1u);
            indices.push_back(baseIndex + 2u);
            indices.push_back(baseIndex + 2u);
            indices.push_back(baseIndex + 3u);
            indices.push_back(baseIndex + 0u);

            batches.back().indexCount += 6u;
        }

        if (vertices.empty() || indices.empty())
        {
            std::ofstream diag("vulkan_runtime_diag.txt", std::ios::app);
            diag << "[Vulkan] gui empty draws=" << guiState.guiDraws.size() << "\n";
            guiState.guiDraws.clear();
            return;
        }

        const std::size_t vertexCount = vertices.size();
        const std::size_t indexCount = indices.size();

        if (vertexCount > guiState.guiVertexCapacity)
        {
            guiState.guiVertexCapacity = vertexCount;
            std::tie(guiState.guiVertexBuffer, guiState.guiVertexBufferMemory) = createBuffer(
                sizeof(Vertex) * guiState.guiVertexCapacity,
                vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        if (indexCount > guiState.guiIndexCapacity)
        {
            guiState.guiIndexCapacity = indexCount;
            std::tie(guiState.guiIndexBuffer, guiState.guiIndexBufferMemory) = createBuffer(
                sizeof(std::uint32_t) * guiState.guiIndexCapacity,
                vk::BufferUsageFlagBits::eIndexBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        }

        {
            const vk::DeviceSize vertexBytes = sizeof(Vertex) * vertexCount;
            auto [mapRes, mapped] = device->mapMemory(*guiState.guiVertexBufferMemory, 0, vertexBytes);
            if (mapRes != vk::Result::eSuccess || !mapped)
                throw std::runtime_error("[Vulkan] Failed to map GUI vertex buffer.");
            std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
            device->unmapMemory(*guiState.guiVertexBufferMemory);
        }

        {
            const vk::DeviceSize indexBytes = sizeof(std::uint32_t) * indexCount;
            auto [mapRes, mapped] = device->mapMemory(*guiState.guiIndexBufferMemory, 0, indexBytes);
            if (mapRes != vk::Result::eSuccess || !mapped)
                throw std::runtime_error("[Vulkan] Failed to map GUI index buffer.");
            std::memcpy(mapped, indices.data(), static_cast<std::size_t>(indexBytes));
            device->unmapMemory(*guiState.guiIndexBufferMemory);
        }

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *guiState.guiPipeline);

        vk::Viewport guiViewport{};
        guiViewport.x = 0.0f;
        guiViewport.y = static_cast<float>(swapChainExtent.height);
        guiViewport.width = static_cast<float>(swapChainExtent.width);
        guiViewport.height = -static_cast<float>(swapChainExtent.height);
        guiViewport.minDepth = 0.0f;
        guiViewport.maxDepth = 1.0f;
        cmd.setViewport(0, guiViewport);

        vk::Rect2D guiScissor{};
        guiScissor.offset = vk::Offset2D{ 0, 0 };
        guiScissor.extent = swapChainExtent;
        cmd.setScissor(0, guiScissor);
        const vk::Buffer vb[] = { *guiState.guiVertexBuffer };
        const vk::DeviceSize offsets[] = { 0 };
        cmd.bindVertexBuffers(0, 1, vb, offsets);
        cmd.bindIndexBuffer(*guiState.guiIndexBuffer, 0, vk::IndexType::eUint32);

        for (const auto& batch : batches)
        {
            if (!batch.atlas)
                continue;

            ensure_gui_atlas(*batch.atlas);
            auto it = guiState.guiAtlases.find(batch.atlas);
            if (it == guiState.guiAtlases.end() || it->second.descriptorSets.empty())
                continue;

            if (imageIndex >= it->second.descriptorSets.size())
                continue;

            vk::DescriptorSet set = *it->second.descriptorSets[imageIndex];
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *pipelineLayout,
                0,
                1,
                &set,
                0,
                nullptr
            );

            cmd.drawIndexed(batch.indexCount, 1u, batch.indexOffset, 0, 0);
        }

        std::ofstream diag("vulkan_runtime_diag.txt", std::ios::app);
        diag << "[Vulkan] gui submitted draws=" << batches.size() << " vertices=" << vertices.size() << " indices=" << indices.size() << " image=" << imageIndex << "\n";
        guiState.guiDraws.clear();
    }

    void Application::createSyncObjects()
    {
        imageAvailableSemaphores.resize(kMaxFramesInFlight);
        renderFinishedSemaphores.resize(kMaxFramesInFlight);
        inFlightFences.resize(kMaxFramesInFlight);

        vk::SemaphoreCreateInfo semInfo{};
        vk::FenceCreateInfo fenceInfo{};
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (std::size_t i = 0; i < kMaxFramesInFlight; ++i)
        {
            {
                auto [r, sem] = device->createSemaphoreUnique(semInfo);
                if (r != vk::Result::eSuccess)
                    throw std::runtime_error("[Vulkan] createSemaphoreUnique(imageAvailable) failed.");
                imageAvailableSemaphores[i] = std::move(sem);
            }
            {
                auto [r, sem] = device->createSemaphoreUnique(semInfo);
                if (r != vk::Result::eSuccess)
                    throw std::runtime_error("[Vulkan] createSemaphoreUnique(renderFinished) failed.");
                renderFinishedSemaphores[i] = std::move(sem);
            }
            {
                auto [r, f] = device->createFenceUnique(fenceInfo);
                if (r != vk::Result::eSuccess)
                    throw std::runtime_error("[Vulkan] createFenceUnique failed.");
                inFlightFences[i] = std::move(f);
            }
        }
    }

    void Application::drawFrame()
    {
        assert_thread_affinity();
        const auto timeout = (std::numeric_limits<std::uint64_t>::max)();

        // Wait for CPU/GPU sync for this frame.
        {
            vk::Fence f = *inFlightFences[currentFrame];
            const vk::Result r = device->waitForFences(1, &f, VK_TRUE, timeout);
            if (r != vk::Result::eSuccess)
                throw std::runtime_error("[Vulkan] waitForFences failed.");
        }

        if (consume_framebuffer_resize_intent())
        {
            recreateSwapChain();
            return;
        }

        std::uint32_t imageIndex = 0;
        vk::Result acquireRes = device->acquireNextImageKHR(
            *swapChain,
            timeout,
            *imageAvailableSemaphores[currentFrame],
            vk::Fence{},
            &imageIndex
        );

        if (acquireRes == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChain();
            return;
        }
        if (acquireRes == vk::Result::eErrorSurfaceLostKHR)
        {
            set_framebuffer_resize_intent(true);
            request_render_stop();
            return;
        }
        if (acquireRes == vk::Result::eErrorDeviceLost)
        {
            request_render_stop();
            return;
        }
        if (acquireRes != vk::Result::eSuccess && acquireRes != vk::Result::eSuboptimalKHR)
            throw std::runtime_error("[Vulkan] Failed to acquire swap chain image.");


        // Reset fence for this frame.
        {
            vk::Fence f = *inFlightFences[currentFrame];
            const vk::Result r = device->resetFences(1, &f);
            if (r != vk::Result::eSuccess)
                throw std::runtime_error("[Vulkan] resetFences failed.");
        }

        updateUniformBuffer(imageIndex, cam);
        updateGuiUniformBuffer(imageIndex);
        recordCommandBuffer(imageIndex);

        const vk::Semaphore waitSems[] = { *imageAvailableSemaphores[currentFrame] };
        const vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        const vk::CommandBuffer submitCmds[] = { *commandBuffers[imageIndex] };
        const vk::Semaphore signalSems[] = { *renderFinishedSemaphores[currentFrame] };

        vk::SubmitInfo submitInfo{};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSems;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = submitCmds;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSems;

        {
            const vk::Result r = graphicsQueue.submit(1, &submitInfo, *inFlightFences[currentFrame]);
            if (r != vk::Result::eSuccess)
                throw std::runtime_error("[Vulkan] graphicsQueue.submit failed.");
        }

        const vk::SwapchainKHR scs[] = { *swapChain };

        vk::PresentInfoKHR presentInfo{};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSems;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = scs;
        presentInfo.pImageIndices = &imageIndex;


        // this needs protected from thread and cross context make current...
        // also needs an if running
        vk::Result presentRes = presentQueue.presentKHR(presentInfo);

        if (presentRes == vk::Result::eErrorOutOfDateKHR || presentRes == vk::Result::eSuboptimalKHR)
        {
            recreateSwapChain();
        }
        else if (presentRes == vk::Result::eErrorSurfaceLostKHR)
        {
            set_framebuffer_resize_intent(true);
            request_render_stop();
            return;
        }
        else if (presentRes != vk::Result::eSuccess)
        {
            throw std::runtime_error("[Vulkan] presentKHR failed.");
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
    }
} // namespace epochnamespace::vulkancontext








