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
// modules/vulkan.context-commands.ixx
// Partition implementation: vulkan.context:commands
// Command buffers + sync + per-frame submit/present.
// ============================================================================

module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

#ifndef EPOCH_USING_VULKAN
#   define EPOCH_USING_VULKAN 1
#endif

#include <../src/renderers/vulkan/vulkan.context_shared.hpp>
// Include Vulkan-Hpp after config.
#include <compare>
#include <vulkan/vulkan.hpp>

export module vulkan.context:commands;

import :shared_vk;
import core.context;
import context.type;
import atlas.texture;
import render.preview_grid;
import render.context_frame;
import render.arcade;
import render.canvas2d;


namespace epochengine::vulkancontext
{
    inline constexpr std::size_t kMaxFramesInFlight = 2;

    namespace
    {
        [[nodiscard]] inline std::array<float, 4> preview_color_to_vulkan(
            std::array<float, 4> color) noexcept
        {
            const auto to_linear = [](float value) noexcept
            {
                return std::pow((std::clamp)(value, 0.0f, 1.0f), 2.2f);
            };

            color[0] = to_linear(color[0]);
            color[1] = to_linear(color[1]);
            color[2] = to_linear(color[2]);
            return color;
        }

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
            throw std::runtime_error("[ Vulkan ] - allocateCommandBuffersUnique failed.");
        commandBuffers = std::move(bufs);
    }

    void Application::recordArcadeRenderTexturePass(vk::CommandBuffer commandBuffer)
    {
        if (epochengine::previewgrid::sampled_render_surface_markers_for(bound_context()).empty())
            return;
        if (!arcadeRenderPass
            || !arcadeRenderFramebuffer
            || !arcadeRenderImage
            || arcadeRenderExtent.width == 0u
            || arcadeRenderExtent.height == 0u)
        {
            throw std::runtime_error(
                "[ Vulkan ] - Engine Arcade sampled screen requested without a complete render target.");
        }

        vk::ClearValue clearValue{};
        clearValue.setColor(vk::ClearColorValue{
            std::array<float, 4>{ 0.008f, 0.012f, 0.024f, 1.0f }
        });

        vk::RenderPassBeginInfo beginInfo{};
        beginInfo.renderPass = *arcadeRenderPass;
        beginInfo.framebuffer = *arcadeRenderFramebuffer;
        beginInfo.renderArea = vk::Rect2D{ vk::Offset2D{ 0, 0 }, arcadeRenderExtent };
        beginInfo.clearValueCount = 1u;
        beginInfo.pClearValues = &clearValue;
        commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

        const int width = static_cast<int>(arcadeRenderExtent.width);
        const int height = static_cast<int>(arcadeRenderExtent.height);
        epochengine::render_arcade::emit_arcade_attract_pattern(
            width,
            height,
            arcadePreviewFrame++,
            [&](const epochengine::render_arcade::ArcadePreviewRect& source)
            {
                const int left = (std::clamp)(source.x, 0, width);
                const int top = (std::clamp)(source.y, 0, height);
                const int right = (std::clamp)(source.x + source.width, left, width);
                const int bottom = (std::clamp)(source.y + source.height, top, height);
                if (right <= left || bottom <= top)
                    return;

                vk::ClearAttachment attachment{};
                attachment.aspectMask = vk::ImageAspectFlagBits::eColor;
                attachment.colorAttachment = 0u;
                attachment.clearValue.setColor(vk::ClearColorValue{ source.color });

                vk::ClearRect rect{};
                rect.rect.offset = vk::Offset2D{ left, top };
                rect.rect.extent = vk::Extent2D{
                    static_cast<std::uint32_t>(right - left),
                    static_cast<std::uint32_t>(bottom - top)
                };
                rect.baseArrayLayer = 0u;
                rect.layerCount = 1u;
                commandBuffer.clearAttachments(1u, &attachment, 1u, &rect);
            });

        commandBuffer.endRenderPass();
    }

    void Application::recordCommandBuffer(std::uint32_t imageIndex)
    {
        if (imageIndex >= commandBuffers.size())
            throw std::runtime_error("[ Vulkan ] - recordCommandBuffer image index out of range.");

        vk::CommandBuffer cmd = *commandBuffers[imageIndex];
        (void)cmd.reset();

        vk::CommandBufferBeginInfo beginInfo{};
        if (cmd.begin(beginInfo) != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - CommandBuffer::begin failed.");

        recordArcadeRenderTexturePass(cmd);
#if EPOCH_USE_CLEAR_COLOR_VULKAN
        std::array<vk::ClearValue, 2> clearValues{};
        const auto frameClearColor = epochengine::core::clear_color_for_context(
            epochengine::core::ContextType::Vulkan);
        const std::array<float, 4> sceneClearColor =
            preview_color_to_vulkan(epochengine::previewgrid::kClearColor);
        clearValues[0].setColor(vk::ClearColorValue{ frameClearColor });
        clearValues[1].setDepthStencil(vk::ClearDepthStencilValue{ 1.0f, 0 });
#endif
        vk::RenderPassBeginInfo renderPassInfo{};
        renderPassInfo.renderPass = *renderPass;
        renderPassInfo.framebuffer = *framebuffers[imageIndex];
        renderPassInfo.renderArea = vk::Rect2D{ vk::Offset2D{0, 0}, swapChainExtent };
#if EPOCH_USE_CLEAR_COLOR_VULKAN
        renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();
#endif

        cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

        const auto* ctx = bound_context();
        rendercontext::FramePlan frame{};
        if (ctx)
        {
            const auto requested = ctx->scene_viewport();
            frame = rendercontext::resolve_frame_plan({
                static_cast<int>(swapChainExtent.width),
                static_cast<int>(swapChainExtent.height),
                { requested.x, requested.y, requested.width, requested.height },
                ctx->scene_preview_mode() == epochengine::core::ScenePreviewMode::Editor,
                ctx->gui_overlay_priority()
            });
        }

        const bool renderScenePreview = frame.scene_visible && indexCount > 0;
        if (renderScenePreview && ctx)
        {
            const std::uint64_t previewRevision =
                epochengine::previewgrid::preview_geometry_revision_for(ctx);
            if (previewRevision != previewGeometryRevision)
            {
                createVertexBuffer();
                createIndexBuffer();
                previewGeometryRevision = previewRevision;
            }
        }

        if (renderScenePreview)
        {
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

            vk::Viewport viewport{};
            viewport.x = static_cast<float>(frame.scene.x);
            viewport.y = static_cast<float>(frame.scene.y);
            viewport.width = static_cast<float>(frame.scene.width);
            viewport.height = static_cast<float>(frame.scene.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            cmd.setViewport(0, viewport);

            vk::Rect2D scissor{};
            scissor.offset = vk::Offset2D{ frame.scene.x, frame.scene.y };
            scissor.extent = vk::Extent2D{
                static_cast<std::uint32_t>(frame.scene.width),
                static_cast<std::uint32_t>(frame.scene.height)
            };
            cmd.setScissor(0, scissor);

#if EPOCH_USE_CLEAR_COLOR_VULKAN
            vk::ClearAttachment sceneAttachment{};
            sceneAttachment.aspectMask = vk::ImageAspectFlagBits::eColor;
            sceneAttachment.colorAttachment = 0;
            sceneAttachment.clearValue.setColor(vk::ClearColorValue{ sceneClearColor });

            vk::ClearRect sceneRect{};
            sceneRect.rect.offset = vk::Offset2D{ frame.scene.x, frame.scene.y };
            sceneRect.rect.extent = vk::Extent2D{
                static_cast<std::uint32_t>(frame.scene.width),
                static_cast<std::uint32_t>(frame.scene.height)
            };
            sceneRect.baseArrayLayer = 0;
            sceneRect.layerCount = 1;
            cmd.clearAttachments(1, &sceneAttachment, 1, &sceneRect);
#endif
            if (solidIndexCount > 0u)
            {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *solidGraphicsPipeline);
                cmd.drawIndexed(solidIndexCount, 1, 0, 0, 0);
            }
            if (arcadeScreenIndexCount > 0u)
            {
                if (!arcadeScreenPipeline || imageIndex >= arcadeDescriptorSets.size())
                {
                    throw std::runtime_error(
                        "[ Vulkan ] - Engine Arcade screen geometry has no sampled descriptor path.");
                }
                vk::DescriptorSet arcadeSet = *arcadeDescriptorSets[imageIndex];
                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    *pipelineLayout,
                    0u,
                    1u,
                    &arcadeSet,
                    0u,
                    nullptr);
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *arcadeScreenPipeline);
                cmd.drawIndexed(
                    arcadeScreenIndexCount,
                    1u,
                    arcadeScreenIndexOffset,
                    0,
                    0u);
            }
            if (lineIndexCount > 0u)
            {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
                const std::uint32_t lineIndexOffset = solidIndexCount + arcadeScreenIndexCount;
                cmd.drawIndexed(lineIndexCount, 1, lineIndexOffset, 0, 0);
            }
        }

        cmd.nextSubpass(vk::SubpassContents::eInline);
        recordCanvas2DCommands(cmd, imageIndex);

        if (auto* guiState = find_gui_state(bound_context()))
        {
            if (!guiState->guiDraws.empty())
                recordGuiCommands(cmd, imageIndex, *guiState);
        }

        cmd.endRenderPass();

        if (cmd.end() != vk::Result::eSuccess)
            throw std::runtime_error("[ Vulkan ] - CommandBuffer::end failed.");
    }

    void Application::recordCanvas2DCommands(
        vk::CommandBuffer cmd,
        std::uint32_t imageIndex)
    {
        auto* guiState = find_gui_state(bound_context());
        if (!guiState)
            return;
        const Canvas2DContextState& state = guiState->canvas2d;
        const auto& descriptorSets = state.filter == FilterMode::nearest
            ? state.nearestDescriptorSets
            : state.linearDescriptorSets;
        if (!state.ready || !state.pipeline || !state.vertexBuffer
            || !state.indexBuffer || imageIndex >= descriptorSets.size())
        {
            return;
        }

        if (state.clearLetterbox)
        {
            vk::ClearAttachment attachment{};
            attachment.aspectMask = vk::ImageAspectFlagBits::eColor;
            attachment.colorAttachment = 0u;
            attachment.clearValue.setColor(vk::ClearColorValue{std::array<float, 4>{
                state.letterboxColor.r,
                state.letterboxColor.g,
                state.letterboxColor.b,
                state.letterboxColor.a}});
            vk::ClearRect clearRect{};
            clearRect.rect.offset = vk::Offset2D{state.surface.x, state.surface.y};
            clearRect.rect.extent = vk::Extent2D{
                state.surface.width, state.surface.height};
            clearRect.baseArrayLayer = 0u;
            clearRect.layerCount = 1u;
            cmd.clearAttachments(1u, &attachment, 1u, &clearRect);
        }

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *state.pipeline);
        vk::Viewport viewport{};
        viewport.x = 0.0f;
        viewport.y = static_cast<float>(swapChainExtent.height);
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = -static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        cmd.setViewport(0u, viewport);

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{state.surface.x, state.surface.y};
        scissor.extent = vk::Extent2D{state.surface.width, state.surface.height};
        cmd.setScissor(0u, scissor);

        const vk::Buffer vertexBuffers[]{*state.vertexBuffer};
        constexpr vk::DeviceSize offsets[]{0u};
        cmd.bindVertexBuffers(0u, 1u, vertexBuffers, offsets);
        cmd.bindIndexBuffer(*state.indexBuffer, 0u, vk::IndexType::eUint32);
        vk::DescriptorSet descriptorSet = *descriptorSets[imageIndex];
        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipelineLayout,
            0u,
            1u,
            &descriptorSet,
            0u,
            nullptr);
        cmd.drawIndexed(6u, 1u, 0u, 0, 0u);
    }

    void Application::enqueue_gui_draw(
        const epochengine::core::Context* ctx,
        const epochengine::SpriteHandle& sprite,
        std::span<const epochengine::TextureAtlas* const> atlases,
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
                throw std::runtime_error("[ Vulkan ] - Failed to map GUI vertex buffer.");
            std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
            device->unmapMemory(*guiState.guiVertexBufferMemory);
        }

        {
            const vk::DeviceSize indexBytes = sizeof(std::uint32_t) * indexCount;
            auto [mapRes, mapped] = device->mapMemory(*guiState.guiIndexBufferMemory, 0, indexBytes);
            if (mapRes != vk::Result::eSuccess || !mapped)
                throw std::runtime_error("[ Vulkan ] - Failed to map GUI index buffer.");
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
                    throw std::runtime_error("[ Vulkan ] - createSemaphoreUnique(imageAvailable) failed.");
                imageAvailableSemaphores[i] = std::move(sem);
            }
            {
                auto [r, sem] = device->createSemaphoreUnique(semInfo);
                if (r != vk::Result::eSuccess)
                    throw std::runtime_error("[ Vulkan ] - createSemaphoreUnique(renderFinished) failed.");
                renderFinishedSemaphores[i] = std::move(sem);
            }
            {
                auto [r, f] = device->createFenceUnique(fenceInfo);
                if (r != vk::Result::eSuccess)
                    throw std::runtime_error("[ Vulkan ] - createFenceUnique failed.");
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
                throw std::runtime_error("[ Vulkan ] - waitForFences failed.");
        }

        if (consume_framebuffer_resize_intent())
        {
            recreateSwapChain();
            return;
        }

        std::uint32_t imageIndex = 0;
        (void)prepareCanvas2D();
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
            throw std::runtime_error("[ Vulkan ] - Failed to acquire swap chain image.");

        // Reset fence for this frame.
        {
            vk::Fence f = *inFlightFences[currentFrame];
            const vk::Result r = device->resetFences(1, &f);
            if (r != vk::Result::eSuccess)
                throw std::runtime_error("[ Vulkan ] - resetFences failed.");
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
                throw std::runtime_error("[ Vulkan ] - graphicsQueue.submit failed.");
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
            throw std::runtime_error("[ Vulkan ] - presentKHR failed.");
        }

        currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
    }
} // namespace epochengine::vulkancontext
