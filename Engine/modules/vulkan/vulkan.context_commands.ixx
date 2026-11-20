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
#include <functional>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <source_location>
#include <stdexcept>
#include <utility>
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
import core.logger;
import context.type;
import atlas.texture;
import render.preview_grid;
import render.camera;
import render.lighting;
import render.preview_atmosphere;
import render.context_frame;
import render.arcade;
import render.canvas2d;
import render.water_frame_evidence;


namespace epochengine::vulkancontext
{

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
        if ((previewSceneFrame
            ? previewgrid::sampled_render_surface_markers_for(*previewSceneFrame)
            : previewgrid::sampled_render_surface_markers_for(bound_context())).empty())
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
            preview_color_to_vulkan(epochengine::previewgrid::scene_clear_color(previewSceneFrame.get()));
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
                ctx->gui_overlay_priority(),
                ctx->frame_window_state()
            });
        }

        if (frame.scene_visible && ctx)
        {
            const std::uint64_t previewRevision =
                previewSceneFrame ? previewSceneFrame->revision
                    : epochengine::previewgrid::preview_geometry_revision_for(ctx);
            const bool geometryChanged = previewSceneFrame
                ? !uploadedPreviewSceneFrame
                    || !preview_geometry_matches(*previewSceneFrame, *uploadedPreviewSceneFrame)
                : uploadedPreviewSceneFrame || previewRevision != previewGeometryRevision;
            if (geometryChanged)
            {
                const bool emptyScene = previewSceneFrame
                    && previewSceneFrame->solids->geometry->empty()
                    && previewSceneFrame->lines->empty()
                    && (!previewSceneFrame->wire->depth_triangles
                        || previewSceneFrame->wire->depth_triangles->empty())
                    && (!previewSceneFrame->wire->helper_lines
                        || previewSceneFrame->wire->helper_lines->empty())
                    && previewgrid::sampled_render_surface_markers_for(*previewSceneFrame).empty();
                if (emptyScene)
                {
                    indexCount = solidIndexCount = lineIndexCount = 0;
                    arcadeScreenIndexOffset = arcadeScreenIndexCount = 0;
                    previewIndicesSequential = false;
                }
                else
                {
                    createVertexBuffer();
                    createIndexBuffer();
                }
                previewGeometryRevision = previewRevision;
                uploadedPreviewSceneFrame = previewSceneFrame;
            }
        }

        const bool renderScenePreview = frame.scene_visible && indexCount > 0;
        const auto sky = frame.scene_visible && previewSceneFrame && atmospherePipeline
            ? previewgrid::planet_atmosphere_view(*previewSceneFrame, float(frame.scene.width) / float(frame.scene.height))
            : std::nullopt;
        const bool cloudVisible = frame.scene_visible && cloudPipeline && previewSceneFrame
            && !previewSceneFrame->clouds.empty();
        if (renderScenePreview || sky || cloudVisible)
        {
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
            if (sky)
            {
                // Five vec4s match the fragment shader's push-constant block.
                const AtmosphereConstants constants{
                    sky->observer.x, sky->observer.y, sky->observer.z, sky->thickness,
                    sky->forward.x, sky->forward.y, sky->forward.z, 0.0f,
                    sky->right.x, sky->right.y, sky->right.z, 0.0f,
                    sky->up.x, sky->up.y, sky->up.z, 0.0f,
                    sky->sun.x, sky->sun.y, sky->sun.z, sky->sun_visible ? 1.0f : 0.0f};
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *atmospherePipeline);
                cmd.pushConstants(*atmospherePipelineLayout, vk::ShaderStageFlagBits::eFragment,
                    0u, static_cast<std::uint32_t>(sizeof(constants)), constants.data());
                cmd.draw(3u, 1u, 0u, 0u);
            }
        }

        if (renderScenePreview)
        {
            const vk::Buffer vb[] = { *vertexBuffer };
            const vk::DeviceSize offsets[] = { 0 };
            cmd.bindVertexBuffers(0, 1, vb, offsets);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout,
                0u, 1u, &*descriptorSets[imageIndex], 0u, nullptr);
            cmd.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

            if (solidIndexCount > 0u)
            {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *solidGraphicsPipeline);
                cmd.drawIndexed(solidIndexCount, 1, 0, 0, 0);
                if (previewSceneFrame && render_water_evidence::enabled())
                    render_water_evidence::record(*previewSceneFrame, "vulkan",
                        previewgrid::water_detail_for(*previewSceneFrame).has_value());
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
        if (cloudVisible)
            recordCloudCommands(cmd, frame.projection_aspect);
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

    void Application::recordCloudCommands(vk::CommandBuffer cmd, float aspect)
    {
        const auto& scene = *previewSceneFrame;
        if (!scene.solids || !previewgrid::admitted_cloud_volumes(scene.clouds)
            || std::ranges::any_of(scene.markers, [](const auto& marker) { return marker.wireframe; }))
            return;
        auto& frame = cloudFrames[currentFrame];
        if (!frame.set || !frame.mapped)
            return;

        const auto& camera = scene.camera;
        const auto forward = previewgrid::normalize(previewgrid::subtract(camera.target, camera.eye));
        const auto right = previewgrid::normalize(previewgrid::cross(forward, camera.up));
        const auto up = previewgrid::cross(right, forward);
        const bool orthographic = camera.projection == render_camera::ProjectionKind::orthographic;
        const float halfHeight = orthographic ? (std::max)(0.025f, camera.orthographicVerticalSize * 0.5f)
            : std::tan(camera.fovRadians * 0.5f);
        previewgrid::Vec3 sun{0, 1, 0}, sunColor{};
        float brightest{};
        for (const auto& light : scene.solids->lighting.lights)
        {
            const auto& source = light.desc;
            if (source.enabled && source.kind == lighting::LightKind::Directional && source.intensity > brightest)
            {
                brightest = source.intensity;
                sun = {-source.direction.x, -source.direction.y, -source.direction.z};
                sunColor = {source.color.r * brightest, source.color.g * brightest, source.color.b * brightest};
            }
        }
        sun = previewgrid::normalize(sun);
        const auto slot = [](const previewgrid::Vec3& value) {
            return std::array<float,4>{value.x, value.y, value.z, 0.0f};
        };
        const auto& ambient = scene.solids->lighting.environment.ambient;
        CloudConstants constants{};
        constants.forward = slot(forward);
        constants.right = slot(right);
        constants.up = slot(up);
        constants.sun = slot(sun);
        constants.sunColor = slot(sunColor);
        constants.ambient = {ambient.r, ambient.g, ambient.b, 0.0f};
        constants.projectionNearFar = {halfHeight * (std::max)(0.001f, aspect), halfHeight,
            camera.nearPlane, camera.farPlane};
        constants.orthographic = orthographic;
        constants.encodeSrgb = !swapchainIsSrgb();

        std::array<const previewgrid::CloudVolume*,maxCloudVolumes> ordered{};
        const auto count = scene.clouds.size();
        for (std::size_t index = 0; index < count; ++index)
            ordered[index] = &scene.clouds[index];
        const auto distanceSquared = [&](const previewgrid::CloudVolume* cloud) {
            const double x = cloud->center[0] - camera.eye.x;
            const double y = cloud->center[1] - camera.eye.y;
            const double z = cloud->center[2] - camera.eye.z;
            return x*x + y*y + z*z;
        };
        std::ranges::sort(std::span{ordered.data(), count}, std::greater<>{}, distanceSquared);

        // The slot's fence has retired. Each draw keeps a separate aligned UBO range.
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto view = previewgrid::cloud_volume_view(*ordered[index], camera);
            if (!view)
                return;
            constants.center = {view->center[0], view->center[1], view->center[2], 0.0f};
            const auto& matrix = view->world_to_body;
            for (std::size_t column = 0; column < constants.worldToBody.size(); ++column)
                constants.worldToBody[column] = {matrix[3u*column], matrix[3u*column+1u], matrix[3u*column+2u], 0.0f};
            constants.windEvolution = {view->wind[0], view->wind[1], view->evolution[0], view->evolution[1]};
            constants.radiiBaseThickness = {view->radius_meters, view->radius_world, view->base, view->thickness};
            constants.density = {view->noise_scale, view->coverage, view->extinction, view->lower_extinction};
            constants.viewOccluderRadius = view->view_occluder_radius;
            constants.seed = view->seed;
            std::memcpy(frame.mapped + index * cloudUniformStride, &constants, sizeof(constants));
        }

        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *cloudPipeline);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const std::uint32_t offset = index * cloudUniformStride;
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *cloudPipelineLayout,
                0u, 1u, &*frame.set, 1u, &offset);
            cmd.draw(3u, 1u, 0u, 0u);
        }
        if (!std::exchange(cloudDrawReported, true))
            logger::get("Vulkan.Clouds").logf(logger::LogLevel::INFO, std::source_location::current(),
                "First density pass recorded with opaque depth and {} volume(s).", count);
    }

    void Application::recordCanvas2DCommands(
        vk::CommandBuffer cmd,
        std::uint32_t imageIndex)
    {
        auto* guiState = find_gui_state(bound_context());
        if (!guiState)
            return;
        const Canvas2DContextState& state = guiState->canvas2d;
        const auto& descriptorSets = state.linearCanvasSampling
            ? state.linearDescriptorSets
            : state.nearestDescriptorSets;
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

    void Application::createPresentSyncObjects()
    {
        // Presentation waits retire when this image is acquired again, rather
        // than when a CPU frame slot's graphics fence signals.
        if (swapChainImages.empty())
            throw std::runtime_error("[ Vulkan ] - Cannot create presentation sync without swapchain images.");
        std::vector<vk::UniqueSemaphore> semaphores;
        semaphores.reserve(swapChainImages.size());
        const vk::SemaphoreCreateInfo info{};
        for (std::size_t i = 0; i < swapChainImages.size(); ++i)
        {
            auto [result, semaphore] = device->createSemaphoreUnique(info);
            if (result != vk::Result::eSuccess)
                throw std::runtime_error("[ Vulkan ] - createSemaphoreUnique(presentation) failed.");
            semaphores.push_back(std::move(semaphore));
        }
        renderFinishedSemaphores.swap(semaphores);
    }

    void Application::createSyncObjects()
    {
        imageAvailableSemaphores.resize(maxFramesInFlight);
        inFlightFences.resize(maxFramesInFlight);

        vk::SemaphoreCreateInfo semInfo{};
        vk::FenceCreateInfo fenceInfo{};
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (std::size_t i = 0; i < maxFramesInFlight; ++i)
        {
            {
                auto [r, sem] = device->createSemaphoreUnique(semInfo);
                if (r != vk::Result::eSuccess)
                    throw std::runtime_error("[ Vulkan ] - createSemaphoreUnique(imageAvailable) failed.");
                imageAvailableSemaphores[i] = std::move(sem);
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

        previewSceneFrame = previewgrid::scene_frame_for(bound_context());
        updateUniformBuffer(imageIndex, cam);
        updateGuiUniformBuffer(imageIndex);
        recordCommandBuffer(imageIndex);

        const vk::Semaphore waitSems[] = { *imageAvailableSemaphores[currentFrame] };
        const vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        const vk::CommandBuffer submitCmds[] = { *commandBuffers[imageIndex] };
        const vk::Semaphore signalSems[] = { *renderFinishedSemaphores.at(imageIndex) };

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

        if (presentRes == vk::Result::eErrorOutOfDateKHR)
        {
            // OUT_OF_DATE invalidates presentation; rebuild before the next frame.
            recreateSwapChain();
        }
        else if (presentRes == vk::Result::eSuboptimalKHR)
        {
            // SUBOPTIMAL is still presentable. Do not destroy the GUI atlases,
            // pipelines and swapchain on a transient driver/compositor hint.
            // An actual resize, pacing-mode change or OUT_OF_DATE result still
            // requests recreation through the existing paths.
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

        currentFrame = (currentFrame + 1) % maxFramesInFlight;
    }
} // namespace epochengine::vulkancontext
