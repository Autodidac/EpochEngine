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
module;

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>

#include <include/engine.config.hpp>

export module raylib.renderer;

import core.context;
import core.logger;
import raylib.state;
import raylib.textures;
import atlas.texture;
import sprite.handle;
import raylib.api;
import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_limits;
import render.canvas2d_presentation;
import render.canvas2d_runtime;
import render.device;
import render.device_raylib;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)

namespace epochengine::raylibrenderer
{
    export struct RaylibSpriteDiagnostics
    {
        std::uint32_t attempted = 0;
        std::uint32_t resolved = 0;
        std::uint32_t uploadReady = 0;
        std::uint32_t textureReady = 0;
        std::uint32_t frameReady = 0;
        std::uint32_t submitted = 0;
    };

    inline std::atomic_uint32_t s_spriteAttempts{ 0 };
    inline std::atomic_uint32_t s_spriteResolved{ 0 };
    inline std::atomic_uint32_t s_spriteUploadReady{ 0 };
    inline std::atomic_uint32_t s_spriteTextureReady{ 0 };
    inline std::atomic_uint32_t s_spriteFrameReady{ 0 };
    inline std::atomic_uint32_t s_spriteSubmitted{ 0 };
    inline std::atomic_uint32_t s_canvasNoContextRefusals{ 0 };

    namespace detail
    {
        struct PresentationBinding final
        {
            RaylibRenderDevice* device{};
            const core::Context* owner{};

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return device != nullptr && owner != nullptr;
            }
        };

        [[nodiscard]] inline std::uint8_t to_channel(float value) noexcept
        {
            if (!std::isfinite(value) || value <= 0.0f)
                return 0u;
            if (value >= 1.0f)
                return 255u;
            return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
        }

        inline void log_no_context_refusal(const char* operation) noexcept
        {
            std::uint32_t observed = s_canvasNoContextRefusals.load(
                std::memory_order_relaxed);
            while (observed < 8u
                && !s_canvasNoContextRefusals.compare_exchange_weak(
                    observed,
                    observed + 1u,
                    std::memory_order_relaxed))
            {
            }
            if (observed >= 8u)
                return;

            try
            {
                logger::warn(
                    "Raylib.Canvas2D",
                    std::string{"Raylib3 Canvas2D "}
                        + (operation ? operation : "operation")
                        + " refused because its owner GL context is not current.");
            }
            catch (...)
            {
            }
        }

        [[nodiscard]] inline bool valid_compose_bounds(
            const canvas2d::presentation::NativePresentationPacket& packet,
            const RaylibTextureRecord& record) noexcept
        {
            if (packet.compose == nullptr || !record.ready())
                return false;

            const auto& compose = *packet.compose;
            const auto& viewport = compose.viewport;
            const auto& destination = viewport.clipped_destination;
            const auto& visible = viewport.visible_canvas;
            const auto nativePolicy =
                canvas2d::presentation::make_native_compose_policy(
                    compose, packet.image);
            if (!nativePolicy)
                return false;
            constexpr std::uint32_t maximumRaylibSize =
                static_cast<std::uint32_t>((std::numeric_limits<int>::max)());
            if (!compose.requires_offscreen_canvas)
                return false;
            if (compose.destination
                != canvas2d::ComposeTargetKind::presentation_surface)
                return false;
            if (viewport.output_surface.empty() || destination.empty())
                return false;
            if (viewport.output_surface.width > maximumRaylibSize
                || viewport.output_surface.height > maximumRaylibSize)
                return false;
            if (record.desc.width != viewport.render_extent.width
                || record.desc.height != viewport.render_extent.height)
                return false;
            if (record.desc.format != TextureFormat::rgba8_unorm)
                return false;
            if (record.texture.width != static_cast<int>(record.desc.width)
                || record.texture.height != static_cast<int>(record.desc.height))
                return false;
            if (!std::isfinite(visible.x) || !std::isfinite(visible.y)
                || !std::isfinite(visible.width) || !std::isfinite(visible.height))
                return false;
            if (visible.x < 0.0f || visible.y < 0.0f
                || visible.width <= 0.0f || visible.height <= 0.0f)
                return false;
            if (!packet.surface.valid_for(viewport.output_surface))
                return false;
            if (packet.image.extent != viewport.render_extent
                || packet.image.format != TextureFormat::rgba8_unorm)
                return false;
            if (visible.x + visible.width
                > static_cast<float>(record.desc.width) + 0.001f)
                return false;
            if (visible.y + visible.height
                > static_cast<float>(record.desc.height) + 0.001f)
                return false;
            return true;
        }

        [[nodiscard]] inline bool present_native_canvas2d(
            void* user,
            const canvas2d::presentation::NativePresentationPacket& packet)
        {
            auto* const binding = static_cast<PresentationBinding*>(user);
            if (!binding || !static_cast<bool>(*binding)
                || !binding->device->native_presentation_context_available(
                    binding->owner))
            {
                log_no_context_refusal("presentation");
                return false;
            }

            const RaylibTextureRecord* const record =
                binding->device->resolve_texture(packet.texture);
            if (!record || !valid_compose_bounds(packet, *record))
                return false;

            const auto& compose = *packet.compose;
            const auto& surface = packet.surface;
            const auto& destination = compose.viewport.clipped_destination;
            const auto nativePolicy =
                canvas2d::presentation::make_native_compose_policy(
                    compose, packet.image);
            if (!nativePolicy)
                return false;
            const auto& visible = compose.viewport.visible_canvas;

            epochengine::raylib_api::begin_scissor_mode(
                surface.viewport.x,
                surface.viewport.y,
                static_cast<int>(surface.viewport.width),
                static_cast<int>(surface.viewport.height));

            if (compose.clear_letterbox)
            {
                epochengine::raylib_api::draw_rectangle_rec(
                    epochengine::raylib_api::Rectangle{
                        static_cast<float>(surface.viewport.x),
                        static_cast<float>(surface.viewport.y),
                        static_cast<float>(surface.viewport.width),
                        static_cast<float>(surface.viewport.height)},
                    epochengine::raylib_api::Color{
                        to_channel(compose.letterbox_color.r),
                        to_channel(compose.letterbox_color.g),
                        to_channel(compose.letterbox_color.b),
                        to_channel(compose.letterbox_color.a)});
            }

            const int filter = nativePolicy.sample_filter
                    == canvas2d::presentation::NativeSampleFilter::nearest
                ? epochengine::raylib_api::texture_filter_point
                : epochengine::raylib_api::texture_filter_bilinear;
            epochengine::raylib_api::set_texture_filter(record->texture, filter);
            epochengine::raylib_api::begin_blend_mode(
                epochengine::raylib_api::blend_alpha_premultiply);
            epochengine::raylib_api::draw_texture_pro(
                record->texture,
                epochengine::raylib_api::Rectangle{
                    visible.x,
                    visible.y,
                    visible.width,
                    visible.height},
                epochengine::raylib_api::Rectangle{
                    static_cast<float>(surface.viewport.x + destination.x),
                    static_cast<float>(surface.viewport.y + destination.y),
                    static_cast<float>(destination.width),
                    static_cast<float>(destination.height)},
                epochengine::raylib_api::Vector2{0.0f, 0.0f},
                0.0f,
                epochengine::raylib_api::white);
            epochengine::raylib_api::end_blend_mode();
            epochengine::raylib_api::end_scissor_mode();
            return true;
        }

        struct LiveCanvasPresenter final
        {
            std::mutex mutex{};
            RaylibRenderDevice device{};
            canvas2d::limits::NativeExecutionLimits executionLimits{
                canvas2d::limits::for_backend(RendererBackendKind::raylib3)};
            PresentationBinding binding{};
            std::unique_ptr<canvas2d::presentation::Canvas2DPresenter> presenter{};
            canvas2d::runtime::SceneRasterSession rasterSession{};

            LiveCanvasPresenter(
                const core::Context* owner,
                std::uint64_t backendEpoch)
                : binding{&device, owner}
            {
                if (!executionLimits.valid())
                    return;
                presenter =
                    std::make_unique<canvas2d::presentation::Canvas2DPresenter>(
                        device,
                        backendEpoch,
                        canvas2d::presentation::NativePresentationHooks{
                            &binding,
                            &present_native_canvas2d},
                        executionLimits.residency);
            }

            [[nodiscard]] bool render(
                canvas2d::CanvasExtent output,
                canvas2d::presentation::PresentationSurface surface)
            {
                std::scoped_lock lock(mutex);
                if (!presenter
                    || !device.native_presentation_context_available(binding.owner))
                {
                    log_no_context_refusal("scene preparation");
                    return false;
                }

                const canvas2d::runtime::PreparedSceneView prepared =
                    rasterSession.prepare(
                        binding.owner,
                        output,
                        executionLimits.canvas,
                        executionLimits.raster);
                if (!prepared)
                {
                    if (prepared.code
                        == canvas2d::runtime::PrepareCode::missing_scene)
                    {
                        presenter->retire_all();
                    }
                    return false;
                }

                return static_cast<bool>(
                    presenter->present(*prepared.frame, *prepared.raster, surface));
            }

            void retire() noexcept
            {
                std::scoped_lock lock(mutex);
                if (presenter)
                    presenter->retire_all();
                rasterSession.reset();
                presenter.reset();
            }
        };

        struct PresenterStorage final
        {
            std::mutex mutex{};
            std::unordered_map<
                const core::Context*,
                std::shared_ptr<LiveCanvasPresenter>> presenters{};
            std::atomic<std::uint64_t> nextBackendEpoch{1u};
        };

        [[nodiscard]] inline PresenterStorage& presenter_storage() noexcept
        {
            static PresenterStorage storage{};
            return storage;
        }

        [[nodiscard]] inline std::shared_ptr<LiveCanvasPresenter> presenter_for(
            const core::Context* owner) noexcept
        {
            if (!owner)
                return {};

            auto& storage = presenter_storage();
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.presenters.find(owner);
            if (found != storage.presenters.end())
                return found->second;

            try
            {
                std::uint64_t epoch = storage.nextBackendEpoch.fetch_add(
                    1u,
                    std::memory_order_relaxed);
                if (epoch == 0u)
                {
                    epoch = storage.nextBackendEpoch.fetch_add(
                        1u,
                        std::memory_order_relaxed);
                }
                auto presenter =
                    std::make_shared<LiveCanvasPresenter>(owner, epoch);
                storage.presenters.emplace(owner, presenter);
                return presenter;
            }
            catch (...)
            {
                return {};
            }
        }
    }

    export [[nodiscard]] inline bool render_canvas2d_scene_content(
        const std::shared_ptr<core::Context>& context,
        const core::RenderViewport& viewport,
        int framebufferWidth,
        int framebufferHeight) noexcept
    {
        if (!context || framebufferWidth <= 0 || framebufferHeight <= 0
            || viewport.x < 0 || viewport.y < 0
            || viewport.width <= 0 || viewport.height <= 0
            || viewport.x + viewport.width > framebufferWidth
            || viewport.y + viewport.height > framebufferHeight)
        {
            return false;
        }

        try
        {
            const auto presenter = detail::presenter_for(context.get());
            if (!presenter)
                return false;
            return presenter->render(
                canvas2d::CanvasExtent{
                    static_cast<std::uint32_t>(viewport.width),
                    static_cast<std::uint32_t>(viewport.height)},
                canvas2d::presentation::PresentationSurface{
                    {
                        static_cast<std::uint32_t>(framebufferWidth),
                        static_cast<std::uint32_t>(framebufferHeight)},
                    {
                        viewport.x,
                        viewport.y,
                        static_cast<std::uint32_t>(viewport.width),
                        static_cast<std::uint32_t>(viewport.height)}});
        }
        catch (...)
        {
            return false;
        }
    }

    export inline void release_canvas2d_scene_renderer(
        const core::Context* owner) noexcept
    {
        if (!owner)
            return;

        std::shared_ptr<detail::LiveCanvasPresenter> retired{};
        auto& storage = detail::presenter_storage();
        {
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.presenters.find(owner);
            if (found == storage.presenters.end())
                return;
            retired = std::move(found->second);
            storage.presenters.erase(found);
        }
        if (retired)
            retired->retire();
    }

    export inline void reset_sprite_diagnostics() noexcept
    {
        s_spriteAttempts.store(0, std::memory_order_relaxed);
        s_spriteResolved.store(0, std::memory_order_relaxed);
        s_spriteUploadReady.store(0, std::memory_order_relaxed);
        s_spriteTextureReady.store(0, std::memory_order_relaxed);
        s_spriteFrameReady.store(0, std::memory_order_relaxed);
        s_spriteSubmitted.store(0, std::memory_order_relaxed);
    }

    export [[nodiscard]] inline RaylibSpriteDiagnostics sprite_diagnostics() noexcept
    {
        return {
            s_spriteAttempts.load(std::memory_order_relaxed),
            s_spriteResolved.load(std::memory_order_relaxed),
            s_spriteUploadReady.load(std::memory_order_relaxed),
            s_spriteTextureReady.load(std::memory_order_relaxed),
            s_spriteFrameReady.load(std::memory_order_relaxed),
            s_spriteSubmitted.load(std::memory_order_relaxed)
        };
    }

    // DO NOT call BeginDrawing/EndDrawing here.
    // The context layer owns frame boundaries; this renderer only issues draw calls.
    export inline void begin_frame() {}
    export inline void end_frame() {}

    export inline void draw_sprite(
        SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x, float y, float width, float height) noexcept
    {
        s_spriteAttempts.fetch_add(1, std::memory_order_relaxed);

        if (!handle.is_valid())
            return;

        const int a = static_cast<int>(handle.atlasIndex);
        const int i = static_cast<int>(handle.localIndex);
        if (a < 0 || a >= static_cast<int>(atlases.size()))
            return;

        const TextureAtlas* atlas = atlases[static_cast<std::size_t>(a)];
        if (!atlas)
            return;

        AtlasRegion r{};
        if (!atlas->try_get_entry_info(i, r))
            return;
        s_spriteResolved.fetch_add(1, std::memory_order_relaxed);


        auto& st = epochengine::raylibstate::s_raylibstate;
        if (!st.running)
            return;

        if (!st.frameActive && st.offscreen.id != 0)
        {
            epochengine::raylib_api::begin_texture_mode(st.offscreen);
            st.frameActive = true;
            st.frameInTextureMode = true;
        }

        // Upload (this will no-op if cached + correct version).
        if (epochengine::raylibtextures::ensure_uploaded(*atlas))
            s_spriteUploadReady.fetch_add(1, std::memory_order_relaxed);

        epochengine::raylib_api::Texture2D tex{};
        if (!epochengine::raylibtextures::try_copy_texture(*atlas, tex)
            || tex.id == 0)
            return;

        s_spriteTextureReady.fetch_add(1, std::memory_order_relaxed);

        if (!st.frameActive)
            return;

        s_spriteFrameReady.fetch_add(1, std::memory_order_relaxed);

        const epochengine::raylib_api::Rectangle src{
            static_cast<float>(r.x),
            static_cast<float>(r.y),
            static_cast<float>(r.width),
            static_cast<float>(r.height)
        };

        const auto fit = epochengine::raylibstate::get_last_viewport_fit();

        const float viewportScale = (fit.scale > 0.0f) ? fit.scale : 1.0f;
        const float designWidth = static_cast<float>((std::max)(1, fit.refW));
        const float designHeight = static_cast<float>((std::max)(1, fit.refH));

        const float baseOffsetX = static_cast<float>(fit.vpX);
        const float baseOffsetY = static_cast<float>(fit.vpY);

        const bool normalized =
            (x >= 0.f && x <= 1.f && y >= 0.f && y <= 1.f) &&
            ((width <= 1.f && width >= 0.f) || width <= 0.f) &&
            ((height <= 1.f && height >= 0.f) || height <= 0.f);

        float px{}, py{}, pw{}, ph{};
        if (normalized)
        {
            px = baseOffsetX + x * designWidth * viewportScale;
            py = baseOffsetY + y * designHeight * viewportScale;

            const float scaledW = (width > 0.f)
                ? (width * designWidth * viewportScale)
                : (static_cast<float>(r.width) * viewportScale);

            const float scaledH = (height > 0.f)
                ? (height * designHeight * viewportScale)
                : (static_cast<float>(r.height) * viewportScale);

            pw = (std::max)(scaledW, 1.0f);
            ph = (std::max)(scaledH, 1.0f);
        }
        else
        {
            px = baseOffsetX + x * viewportScale;
            py = baseOffsetY + y * viewportScale;

            const float scaledW = (width > 0.f)
                ? (width * viewportScale)
                : (static_cast<float>(r.width) * viewportScale);

            const float scaledH = (height > 0.f)
                ? (height * viewportScale)
                : (static_cast<float>(r.height) * viewportScale);

            pw = (std::max)(scaledW, 1.0f);
            ph = (std::max)(scaledH, 1.0f);
        }

        const epochengine::raylib_api::Rectangle dst{ px, py, pw, ph };

        epochengine::raylib_api::draw_texture_pro(
            tex,
            src,
            dst,
            epochengine::raylib_api::Vector2{ 0.0f, 0.0f },
            0.0f,
            epochengine::raylib_api::white);
        s_spriteSubmitted.fetch_add(1, std::memory_order_relaxed);

    }
}

#endif // EPOCH_USING_RAYLIB
