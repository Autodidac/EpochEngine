// ============================================================================
// modules/acontext.vulkan.context-commands.ixx
// Partition implementation: acontext.vulkan.context:commands
// Command buffers + sync + per-frame submit/present.
// ============================================================================

module;

#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

export module acontext.vulkan.context:commands;

import :shared_vk;
import aengine.core.context;
import aatlas.texture;

import <array>;
import <cstdint>;
import <cstring>;
import <limits>;
import <memory>;
import <mutex>;
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
        const auto clearColor = epochnamespace::core::clear_color_for_context(
            epochnamespace::core::ContextType::Vulkan);
        clearValues[0].setColor(
            vk::ClearColorValue{ std::array<float, 4>{ clearColor[0], clearColor[1], clearColor[2], clearColor[3] } });
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

        if (auto guiState = find_gui_state(bound_context()))
        {
            std::vector<GuiDrawCommand> guiDrawSnapshot;
            {
                std::scoped_lock guiLock(guiState->mutex);
                guiDrawSnapshot = guiState->guiDraws;
            }

            if (!guiDrawSnapshot.empty())
                recordGuiCommands(cmd, imageIndex, std::move(guiState), std::move(guiDrawSnapshot));
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

        auto guiState = gui_state_for_context(ctx);
        std::scoped_lock guiLock(guiState->mutex);
        guiState->guiDraws.push_back(GuiDrawCommand{
            atlas,
            sprite.localIndex,
            x,
            y,
            w,
            h
            });
    }

    void Application::recordGuiCommands(vk::CommandBuffer cmd,
        std::uint32_t imageIndex,
        std::shared_ptr<GuiContextState> guiState,
        std::vector<GuiDrawCommand> guiDrawSnapshot)
    {
        if (!guiState || guiDrawSnapshot.empty() || !device)
            return;

        bool needsGuiPipeline = false;
        bool needsGuiUniformBuffers = false;
        {
            std::scoped_lock guiLock(guiState->mutex);
            needsGuiPipeline = !guiState->guiPipeline;
            needsGuiUniformBuffers = guiState->guiUniformBuffers.empty();
        }

        if (needsGuiPipeline)
            createGuiPipeline();
        if (needsGuiUniformBuffers)
            createGuiUniformBuffers();

        std::vector<Vertex> vertices{};
        std::vector<std::uint32_t> indices{};
        std::vector<GuiBatch> batches{};

        vertices.reserve(guiDrawSnapshot.size() * 4u);
        indices.reserve(guiDrawSnapshot.size() * 6u);
        batches.reserve(guiDrawSnapshot.size());

        const TextureAtlas* currentAtlas = nullptr;

        for (const auto& draw : guiDrawSnapshot)
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
            const float v0 = region.v1;
            const float v1 = region.v2;

            vertices.push_back(Vertex{ { x0, y0, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u1, v0 } });
            vertices.push_back(Vertex{ { x1, y0, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u2, v0 } });
            vertices.push_back(Vertex{ { x1, y1, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u2, v1 } });
            vertices.push_back(Vertex{ { x0, y1, 0.0f }, { 0.0f, 0.0f, 1.0f }, { region.u1, v1 } });

            if (draw.atlas != currentAtlas)
            {
                currentAtlas = draw.atlas;
                batches.push_back(GuiBatch{ currentAtlas, static_cast<std::uint32_t>(indices.size()), 0u });
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
            return;

        const std::size_t vertexCount = vertices.size();
        const std::size_t indexCount = indices.size();

        {
            std::scoped_lock guiLock(guiState->mutex);

            if (vertexCount > guiState->guiVertexCapacity)
            {
                guiState->guiVertexCapacity = vertexCount;
                std::tie(guiState->guiVertexBuffer, guiState->guiVertexBufferMemory) = createBuffer(
                    sizeof(Vertex) * guiState->guiVertexCapacity,
                    vk::BufferUsageFlagBits::eVertexBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            }

            if (indexCount > guiState->guiIndexCapacity)
            {
                guiState->guiIndexCapacity = indexCount;
                std::tie(guiState->guiIndexBuffer, guiState->guiIndexBufferMemory) = createBuffer(
                    sizeof(std::uint32_t) * guiState->guiIndexCapacity,
                    vk::BufferUsageFlagBits::eIndexBuffer,
                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            }

            const vk::DeviceSize vertexBytes = sizeof(Vertex) * vertexCount;
            auto [vertexMapRes, vertexMapped] = device->mapMemory(*guiState->guiVertexBufferMemory, 0, vertexBytes);
            if (vertexMapRes != vk::Result::eSuccess || !vertexMapped)
                throw std::runtime_error("[Vulkan] Failed to map GUI vertex buffer.");
            std::memcpy(vertexMapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
            device->unmapMemory(*guiState->guiVertexBufferMemory);

            const vk::DeviceSize indexBytes = sizeof(std::uint32_t) * indexCount;
            auto [indexMapRes, indexMapped] = device->mapMemory(*guiState->guiIndexBufferMemory, 0, indexBytes);
            if (indexMapRes != vk::Result::eSuccess || !indexMapped)
                throw std::runtime_error("[Vulkan] Failed to map GUI index buffer.");
            std::memcpy(indexMapped, indices.data(), static_cast<std::size_t>(indexBytes));
            device->unmapMemory(*guiState->guiIndexBufferMemory);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *guiState->guiPipeline);
            const vk::Buffer vb[] = { *guiState->guiVertexBuffer };
            const vk::DeviceSize offsets[] = { 0 };
            cmd.bindVertexBuffers(0, 1, vb, offsets);
            cmd.bindIndexBuffer(*guiState->guiIndexBuffer, 0, vk::IndexType::eUint32);
        }

        for (const auto& batch : batches)
        {
            if (!batch.atlas)
                continue;

            ensure_gui_atlas(*batch.atlas);

            vk::DescriptorSet set{};
            {
                std::scoped_lock guiLock(guiState->mutex);
                auto it = guiState->guiAtlases.find(batch.atlas);
                if (it == guiState->guiAtlases.end() || it->second.descriptorSets.empty())
                    continue;
                if (imageIndex >= it->second.descriptorSets.size())
                    continue;
                set = *it->second.descriptorSets[imageIndex];
            }

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

        if (should_stop_rendering())
            return;

        const auto has_frame_sync = [this]() noexcept
            {
                return currentFrame < imageAvailableSemaphores.size()
                    && currentFrame < renderFinishedSemaphores.size()
                    && currentFrame < inFlightFences.size()
                    && imageAvailableSemaphores[currentFrame]
                    && renderFinishedSemaphores[currentFrame]
                    && inFlightFences[currentFrame];
            };

        if (!device || !has_frame_sync())
        {
            request_render_stop();
            return;
        }

        if (!swapChain)
        {
            if (consume_framebuffer_resize_intent())
                recreateSwapChain();
            return;
        }

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


        bool context_window_is_valid = true;
        if (const auto* ctx = bound_context())
        {
            if (ctx->windowData)
            {
                if (!ctx->windowData->running || ctx->windowData->should_close)
                    context_window_is_valid = false;

#if defined(_WIN32) && !defined(ALMOND_MAIN_HEADLESS)
                if (context_window_is_valid)
                {
                    const HWND hwnd = ctx->windowData->hwnd
                        ? ctx->windowData->hwnd
                        : static_cast<HWND>(ctx->native_window);
                    if (!hwnd || ::IsWindow(hwnd) == FALSE)
                        context_window_is_valid = false;
                }
#endif
            }
        }

        if (!device || !swapChain || !has_frame_sync() || !context_window_is_valid)
        {
            request_render_stop();
            return;
        }

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
        else if (presentRes == vk::Result::eErrorDeviceLost
            || presentRes == vk::Result::eErrorOutOfHostMemory
            || presentRes == vk::Result::eErrorOutOfDeviceMemory
            || presentRes == vk::Result::eErrorFullScreenExclusiveModeLostEXT)
        {
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
