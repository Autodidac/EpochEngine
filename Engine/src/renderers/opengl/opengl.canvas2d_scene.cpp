/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/

#include "opengl.canvas2d_scene.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import core.commandline;
import core.context;
import core.logger;
import opengl.canvas2d;
import opengl.textures;
import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_evidence;
import render.canvas2d_limits;
import render.canvas2d_presentation;
import render.canvas2d_scene;
import render.device;
import render.device_opengl_family;

namespace epochengine::openglcanvas2d
{
    namespace
    {
        struct LiveCanvasPresenter final
        {
            std::mutex mutex{};
            OpenGLFamilyRenderDevice device{RendererBackendKind::opengl};
            canvas2d::limits::NativeExecutionLimits execution_limits{
                canvas2d::limits::for_backend(RendererBackendKind::opengl)};
            std::unique_ptr<openglcanvas2d::Presenter> presenter{};
            canvas2d::Canvas2DFramePlan frame{};
            canvas2d::cpu::RasterResult raster{};
            canvas2d::CanvasExtent output{};
            std::uint64_t content_hash{};
            Canvas2DPixelEvidenceSnapshot evidence{};
            std::uint32_t evidence_frames{};

            explicit LiveCanvasPresenter(std::uint64_t backendEpoch)
            {
                device.set_native_texture_hooks(
                    opengltextures::make_native_texture_hooks());
                if (execution_limits.valid()
                    && device.native_texture_hooks_ready())
                {
                    presenter = std::make_unique<openglcanvas2d::Presenter>(
                        device,
                        backendEpoch,
                        execution_limits.residency);
                }
            }

            [[nodiscard]] bool ready() const noexcept
            {
                return presenter != nullptr;
            }

            [[nodiscard]] bool render(
                const canvas2d::scene_content::AcquiredScene& scene,
                canvas2d::CanvasExtent nextOutput,
                canvas2d::presentation::PresentationSurface surface)
            {
                std::scoped_lock lock(mutex);
                if (!presenter || !scene || nextOutput.empty())
                    return false;
                if (content_hash != scene.content_hash || output != nextOutput
                    || !frame || !raster)
                {
                    canvas2d::Canvas2DFramePlan nextFrame =
                        canvas2d::scene_content::compile(
                            scene,
                            nextOutput,
                            execution_limits.canvas);
                    if (!nextFrame)
                        return false;
                    canvas2d::cpu::RasterResult nextRaster =
                        canvas2d::cpu::rasterize(
                            nextFrame,
                            scene.content->resources.bindings,
                            execution_limits.raster);
                    if (!nextRaster)
                        return false;

                    frame = std::move(nextFrame);
                    raster = std::move(nextRaster);
                    output = nextOutput;
                    content_hash = scene.content_hash;
                    evidence = {};
                    evidence.frame_sequence = frame.frame_sequence;
                    evidence.content_hash = content_hash;
                    evidence_frames = 0;
                }

                const bool evidenceTerminal =
                    evidence.state == Canvas2DPixelEvidenceState::match
                    || evidence.state == Canvas2DPixelEvidenceState::mismatch
                    || evidence.state == Canvas2DPixelEvidenceState::unavailable;
                const bool evidenceRequested =
                    core::cli::capture_requested && !evidenceTerminal;
                if (evidenceRequested)
                {
                    evidence.state = Canvas2DPixelEvidenceState::warming;
                    ++evidence_frames;
                }

                const canvas2d::presentation::PresentationResult presented =
                    presenter->present(frame, raster, surface);
                if (!presented)
                    return false;

                const std::uint32_t warmupFrames = (std::max)(
                    std::uint32_t{1},
                    core::cli::capture_warmup_frames);
                if (evidenceRequested && evidence_frames >= warmupFrames)
                {
                    canvas2d::evidence::PixelEvidencePolicy policy{};
                    policy.channel_tolerance =
                        frame.compose.presentation_filter
                            == FilterMode::nearest
                        ? 0u
                        : 1u;
                    const canvas2d::evidence::PixelEvidenceResult compared =
                        presenter->compare_native(
                            raster.presentation,
                            raster.presentation_hash,
                            surface,
                            policy);
                    evidence.frame_sequence = frame.frame_sequence;
                    evidence.content_hash = content_hash;
                    evidence.reference_hash = compared.reference_hash;
                    evidence.observed_hash = compared.observed_hash;
                    evidence.pixels_compared = compared.pixels_compared;
                    evidence.nonidentical_pixels =
                        compared.nonidentical_pixels;
                    evidence.outlier_pixels = compared.outlier_pixels;
                    evidence.mean_absolute_error_millionths =
                        compared.mean_absolute_error_millionths;
                    evidence.maximum_channel_error =
                        compared.maximum_channel_error;
                    evidence.first_outlier_x = compared.first_outlier_x;
                    evidence.first_outlier_y = compared.first_outlier_y;
                    if (compared.matched())
                    {
                        evidence.state = Canvas2DPixelEvidenceState::match;
                    }
                    else if (compared.code
                        == canvas2d::evidence::PixelEvidenceCode::mismatch)
                    {
                        evidence.state = Canvas2DPixelEvidenceState::mismatch;
                    }
                    else
                    {
                        evidence.state =
                            Canvas2DPixelEvidenceState::unavailable;
                    }

                    std::string message =
                        "frame=" + std::to_string(evidence.frame_sequence)
                        + " content=" + std::to_string(evidence.content_hash)
                        + " result="
                        + std::string{
                            canvas2d::evidence::pixel_evidence_code_name(
                                compared.code)}
                        + " pixels="
                        + std::to_string(evidence.pixels_compared)
                        + " outliers="
                        + std::to_string(evidence.outlier_pixels)
                        + " max_channel_error="
                        + std::to_string(evidence.maximum_channel_error);
                    if (compared.matched())
                    {
                        logger::info(
                            "OpenGL.Canvas2D.Evidence",
                            message);
                    }
                    else
                    {
                        logger::warn(
                            "OpenGL.Canvas2D.Evidence",
                            message);
                    }
                }
                return true;
            }

            [[nodiscard]] Canvas2DPixelEvidenceSnapshot snapshot() noexcept
            {
                std::scoped_lock lock(mutex);
                return evidence;
            }

            void retire() noexcept
            {
                std::scoped_lock lock(mutex);
                if (presenter)
                    presenter->retire_all();
                presenter.reset();
                frame = {};
                raster = {};
                output = {};
                content_hash = 0;
                evidence = {};
                evidence_frames = 0;
            }
        };

        struct PresenterStorage final
        {
            std::mutex mutex{};
            std::unordered_map<
                const core::Context*,
                std::shared_ptr<LiveCanvasPresenter>> presenters{};
            std::atomic<std::uint64_t> next_backend_epoch{1};
        };

        [[nodiscard]] PresenterStorage& presenter_storage() noexcept
        {
            static PresenterStorage value{};
            return value;
        }

        [[nodiscard]] std::shared_ptr<LiveCanvasPresenter> presenter_for(
            const core::Context* ctx) noexcept
        {
            if (!ctx)
                return {};
            auto& storage = presenter_storage();
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.presenters.find(ctx);
            if (found != storage.presenters.end())
                return found->second;
            try
            {
                std::uint64_t epoch = storage.next_backend_epoch.fetch_add(
                    1,
                    std::memory_order_relaxed);
                if (epoch == 0)
                {
                    epoch = storage.next_backend_epoch.fetch_add(
                        1,
                        std::memory_order_relaxed);
                }
                auto presenter = std::make_shared<LiveCanvasPresenter>(epoch);
                if (!presenter->ready())
                    return {};
                storage.presenters.emplace(ctx, presenter);
                return presenter;
            }
            catch (...)
            {
                return {};
            }
        }
    }

    bool render_canvas2d_scene_content(
        const std::shared_ptr<core::Context>& ctx,
        const core::RenderViewport& viewport,
        int framebufferWidth,
        int framebufferHeight)
    {
        if (!ctx)
            return false;
        const auto scene = canvas2d::scene_content::acquire(ctx.get());
        if (!scene)
        {
            release_canvas2d_scene_renderer(ctx.get());
            return false;
        }
        if (framebufferWidth <= 0 || framebufferHeight <= 0
            || viewport.x < 0 || viewport.y < 0
            || viewport.width <= 0 || viewport.height <= 0
            || viewport.x + viewport.width > framebufferWidth
            || viewport.y + viewport.height > framebufferHeight)
        {
            return false;
        }

        const std::shared_ptr<LiveCanvasPresenter> presenter = presenter_for(ctx.get());
        const canvas2d::CanvasExtent output{
            static_cast<std::uint32_t>(viewport.width),
            static_cast<std::uint32_t>(viewport.height)};
        if (!presenter)
            return false;
        return presenter->render(
            scene,
            output,
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

    Canvas2DPixelEvidenceSnapshot canvas2d_scene_pixel_evidence(
        const core::Context* ctx) noexcept
    {
        if (!ctx)
            return {};
        auto& storage = presenter_storage();
        std::shared_ptr<LiveCanvasPresenter> presenter{};
        {
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.presenters.find(ctx);
            if (found == storage.presenters.end())
                return {};
            presenter = found->second;
        }
        return presenter ? presenter->snapshot()
                         : Canvas2DPixelEvidenceSnapshot{};
    }

    void release_canvas2d_scene_renderer(const core::Context* ctx) noexcept
    {
        if (!ctx)
            return;
        std::shared_ptr<LiveCanvasPresenter> retired{};
        auto& storage = presenter_storage();
        {
            std::scoped_lock lock(storage.mutex);
            const auto found = storage.presenters.find(ctx);
            if (found == storage.presenters.end())
                return;
            retired = std::move(found->second);
            storage.presenters.erase(found);
        }
        if (retired)
            retired->retire();
    }
}
#else
namespace epochengine::openglcanvas2d
{
    bool render_canvas2d_scene_content(
        const std::shared_ptr<core::Context>&,
        const core::RenderViewport&,
        int,
        int)
    {
        return false;
    }

    Canvas2DPixelEvidenceSnapshot canvas2d_scene_pixel_evidence(
        const core::Context*) noexcept
    {
        return {};
    }

    void release_canvas2d_scene_renderer(const core::Context*) noexcept
    {
    }
}
#endif
