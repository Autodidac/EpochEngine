/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

export module render.canvas2d_presentation;

import render.canvas2d;
import render.canvas2d_cpu;
import render.device;
import render.texture_residency;

export namespace epochengine::canvas2d::presentation
{
    enum class PresentationCode : std::uint8_t
    {
        presented,
        invalid_frame,
        invalid_raster,
        invalid_policy,
        missing_native_hook,
        residency_failed,
        native_present_failed
    };

    [[nodiscard]] constexpr const char* presentation_code_name(
        PresentationCode code) noexcept
    {
        switch (code)
        {
        case PresentationCode::presented: return "presented";
        case PresentationCode::invalid_frame: return "invalid_frame";
        case PresentationCode::invalid_raster: return "invalid_raster";
        case PresentationCode::invalid_policy: return "invalid_policy";
        case PresentationCode::missing_native_hook: return "missing_native_hook";
        case PresentationCode::residency_failed: return "residency_failed";
        case PresentationCode::native_present_failed: return "native_present_failed";
        }
        return "unknown";
    }

    enum class PixelOrigin : std::uint8_t
    {
        top_left
    };

    struct ImageContract final
    {
        CanvasExtent extent{};
        TextureFormat format{TextureFormat::rgba8_unorm};
        PixelOrigin origin{PixelOrigin::top_left};
        SpriteColorSpace color_space{SpriteColorSpace::linear};
        cpu::AlphaEncoding alpha_encoding{cpu::AlphaEncoding::premultiplied};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !extent.empty()
                && format == TextureFormat::rgba8_unorm
                && origin == PixelOrigin::top_left
                && color_space == SpriteColorSpace::linear
                && alpha_encoding == cpu::AlphaEncoding::premultiplied;
        }
    };

    struct PresentationSurface final
    {
        CanvasExtent framebuffer_extent{};
        RectI viewport{};

        [[nodiscard]] constexpr bool valid_for(CanvasExtent output) const noexcept
        {
            if (framebuffer_extent.empty() || viewport.empty()
                || viewport.x < 0 || viewport.y < 0
                || viewport.width != output.width
                || viewport.height != output.height)
            {
                return false;
            }
            const std::uint64_t right = static_cast<std::uint64_t>(viewport.x)
                + viewport.width;
            const std::uint64_t bottom = static_cast<std::uint64_t>(viewport.y)
                + viewport.height;
            return right <= framebuffer_extent.width
                && bottom <= framebuffer_extent.height;
        }
    };

    [[nodiscard]] constexpr PresentationSurface full_surface(
        CanvasExtent output) noexcept
    {
        return {output, {0, 0, output.width, output.height}};
    }

    struct NativePresentationPacket final
    {
        TextureHandle texture{};
        const FinalComposePlan* compose{};
        PresentationSurface surface{};
        ImageContract image{};
        std::uint64_t frame_sequence{};
        std::uint64_t content_hash{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return texture && compose && static_cast<bool>(*compose)
                && surface.valid_for(compose->viewport.output_surface)
                && image
                && image.extent == compose->viewport.render_extent
                && frame_sequence != 0 && content_hash != 0;
        }
    };

    struct NativePresentationHooks final
    {
        using PresentFn = bool (*)(
            void* user,
            const NativePresentationPacket& packet);

        void* user{};
        PresentFn present{};

        [[nodiscard]] constexpr bool ready() const noexcept
        {
            return present != nullptr;
        }
    };

    struct PresentationPolicy final
    {
        bool allow_eviction{true};
        bool require_backend_ready{true};
        std::uint32_t priority{1};
    };

    [[nodiscard]] constexpr bool valid(const PresentationPolicy&) noexcept
    {
        return true;
    }

    struct PresentationMetrics final
    {
        std::uint64_t requests{};
        std::uint64_t accepted_frames{};
        std::uint64_t rejected_frames{};
        std::uint64_t raster_failures{};
        std::uint64_t residency_failures{};
        std::uint64_t native_present_calls{};
        std::uint64_t native_present_failures{};
        std::uint64_t uploaded_frames{};
        std::uint64_t reused_frames{};
        std::uint64_t presented_frames{};
        std::uint64_t presented_pixels{};
        std::uint64_t presented_bytes{};
        std::uint64_t last_frame_sequence{};
        std::uint64_t last_canvas_hash{};
    };

    struct PresentationResult final
    {
        PresentationCode code{PresentationCode::invalid_frame};
        cpu::RasterCode raster_code{cpu::RasterCode::invalid_frame};
        texture_residency::ResidencyCode residency_code{
            texture_residency::ResidencyCode::invalid_request};
        texture_residency::ResidencyHandle residency{};
        TextureHandle physical{};
        std::uint64_t frame_sequence{};
        std::uint64_t canvas_hash{};
        std::uint64_t resident_bytes{};
        bool cache_reused{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == PresentationCode::presented
                && static_cast<bool>(residency)
                && static_cast<bool>(physical)
                && canvas_hash != 0;
        }
    };

    namespace detail
    {
        inline constexpr std::uint64_t canvas_output_asset_key =
            0x45504F4348324443ull;
        inline constexpr std::uint32_t canvas_compiler_schema_version = 1;

        [[nodiscard]] constexpr std::uint64_t avalanche(
            std::uint64_t value) noexcept
        {
            value ^= value >> 30;
            value *= 0xbf58476d1ce4e5b9ull;
            value ^= value >> 27;
            value *= 0x94d049bb133111ebull;
            value ^= value >> 31;
            return value == 0 ? 1 : value;
        }

        [[nodiscard]] inline texture_residency::ArtifactDigest canvas_digest(
            const cpu::Image& image) noexcept
        {
            const std::uint64_t packed_extent =
                (static_cast<std::uint64_t>(image.extent.width) << 32)
                | image.extent.height;
            std::array<std::uint64_t, 4> state{{
                0xcbf29ce484222325ull ^ packed_extent,
                0x84222325cbf29ce4ull ^ (packed_extent << 1),
                0x9e3779b97f4a7c15ull ^ (packed_extent >> 1),
                0xd6e8feb86659fd93ull ^ static_cast<std::uint64_t>(image.format)
            }};
            constexpr std::array<std::uint64_t, 4> primes{{
                0x100000001b3ull,
                0x100000001e7ull,
                0x10000000233ull,
                0x10000000291ull
            }};
            for (const cpu::Rgba8 pixel : image.pixels)
            {
                const std::array<std::uint8_t, 4> bytes{{
                    pixel.r, pixel.g, pixel.b, pixel.a}};
                for (const std::uint8_t byte : bytes)
                {
                    for (std::size_t lane = 0; lane < state.size(); ++lane)
                    {
                        state[lane] ^= static_cast<std::uint64_t>(byte)
                            + static_cast<std::uint64_t>(lane * 0x3d);
                        state[lane] *= primes[lane];
                    }
                }
            }
            for (std::uint64_t& lane : state)
                lane = avalanche(lane);
            return {state};
        }

        [[nodiscard]] constexpr bool accepted_frame(
            const Canvas2DFramePlan& frame,
            const PresentationPolicy&) noexcept
        {
            return frame.frame_sequence != 0
                && static_cast<bool>(frame.compose)
                && frame.code == ResultCode::success
                && frame.compose.destination == ComposeTargetKind::presentation_surface;
        }

        [[nodiscard]] inline bool accepted_raster(
            const Canvas2DFramePlan& frame,
            const cpu::RasterResult& raster,
            const PresentationPolicy&) noexcept
        {
            if (raster.code != cpu::RasterCode::success
                || !raster.canvas.valid() || raster.canvas_hash == 0)
                return false;
            return raster.frame_sequence == frame.frame_sequence
                && raster.canvas.extent == frame.compose.viewport.render_extent
                && raster.canvas.format == frame.compose.canvas_target.render_target.color_format;
        }

        [[nodiscard]] constexpr std::uint64_t image_bytes(
            const cpu::Image& image) noexcept
        {
            constexpr std::uint64_t maximum =
                (std::numeric_limits<std::uint64_t>::max)();
            const std::uint64_t texels = static_cast<std::uint64_t>(image.extent.width)
                * static_cast<std::uint64_t>(image.extent.height);
            if (image.extent.width != 0
                && texels / image.extent.width != image.extent.height)
            {
                return 0;
            }
            if (texels > maximum / sizeof(cpu::Rgba8))
                return 0;
            return texels * sizeof(cpu::Rgba8);
        }

        class ContractCommandContext final : public ICommandContext
        {
        public:
            void begin(const char*) override {}
            void end() override {}
            void debug_marker(const char*) override {}
            void barrier() override {}
        };

        class ContractDevice final : public IRenderDevice
        {
        public:
            struct Record final
            {
                TextureDesc desc{};
                bool active{};
                bool ready{};
            };

            std::string backend_name() const override
            {
                return "canvas2d-presentation-contract";
            }

            BufferHandle create_buffer(const BufferDesc&) override
            {
                return BufferHandle{1};
            }

            TextureHandle create_texture(const TextureDesc& desc) override
            {
                if (desc.width == 0 || desc.height == 0
                    || desc.format != TextureFormat::rgba8_unorm)
                {
                    return {};
                }
                for (std::uint32_t index = 0; index < records.size(); ++index)
                {
                    if (!records[index].active)
                    {
                        records[index] = Record{desc, true, false};
                        ++create_count;
                        return TextureHandle{index + 1};
                    }
                }
                records.push_back(Record{desc, true, false});
                ++create_count;
                return TextureHandle{static_cast<std::uint32_t>(records.size())};
            }

            bool upload_texture(
                TextureHandle handle,
                const TextureUploadDesc& upload) override
            {
                Record* const record = resolve(handle);
                if (!record || !epochengine::valid(upload)
                    || upload.width != record->desc.width
                    || upload.height != record->desc.height
                    || upload.format != record->desc.format)
                {
                    return false;
                }
                record->ready = true;
                ++upload_count;
                return true;
            }

            bool texture_ready(TextureHandle handle) const noexcept override
            {
                const Record* const record = resolve(handle);
                return record && record->ready;
            }

            void destroy(BufferHandle) noexcept override {}

            void destroy(TextureHandle handle) noexcept override
            {
                Record* const record = resolve(handle);
                if (!record)
                    return;
                *record = {};
                ++destroy_count;
            }

            ICommandContext& acquire_graphics_context() override
            {
                return command_context;
            }

            void present(ISwapchain&) override {}

            [[nodiscard]] Record* resolve(TextureHandle handle) noexcept
            {
                if (!handle || handle.value > records.size())
                    return nullptr;
                Record& record = records[handle.value - 1];
                return record.active ? &record : nullptr;
            }

            [[nodiscard]] const Record* resolve(TextureHandle handle) const noexcept
            {
                if (!handle || handle.value > records.size())
                    return nullptr;
                const Record& record = records[handle.value - 1];
                return record.active ? &record : nullptr;
            }

            ContractCommandContext command_context{};
            std::vector<Record> records{};
            std::uint32_t create_count{};
            std::uint32_t upload_count{};
            std::uint32_t destroy_count{};
        };

        struct ContractNativeState final
        {
            std::uint32_t calls{};
            TextureHandle last_texture{};
            bool fail_next{};
        };

        [[nodiscard]] inline bool contract_present(
            void* user,
            const NativePresentationPacket& packet) noexcept
        {
            auto* const state = static_cast<ContractNativeState*>(user);
            if (!state || !packet)
                return false;
            ++state->calls;
            state->last_texture = packet.texture;
            if (state->fail_next)
            {
                state->fail_next = false;
                return false;
            }
            return true;
        }
    }

    [[nodiscard]] inline texture_residency::ArtifactKey make_canvas_artifact_key(
        const cpu::Image& image,
        std::uint64_t image_hash) noexcept
    {
        if (!image.valid() || image_hash == 0)
            return {};
        return {
            LogicalTextureReference{detail::canvas_output_asset_key, image_hash},
            detail::canvas_digest(image),
            detail::canvas_compiler_schema_version,
            image.extent.width,
            image.extent.height,
            1,
            image.format
        };
    }

    class Canvas2DPresenter final
    {
    public:
        Canvas2DPresenter(
            IRenderDevice& device,
            std::uint64_t backend_epoch,
            NativePresentationHooks hooks,
            texture_residency::ResidencyLimits limits = {}) noexcept
            : residency_(device, backend_epoch, limits), hooks_(hooks)
        {
        }

        Canvas2DPresenter(const Canvas2DPresenter&) = delete;
        Canvas2DPresenter& operator=(const Canvas2DPresenter&) = delete;
        Canvas2DPresenter(Canvas2DPresenter&&) = delete;
        Canvas2DPresenter& operator=(Canvas2DPresenter&&) = delete;

        [[nodiscard]] PresentationResult present(
            const Canvas2DFramePlan& frame,
            const cpu::RasterResult& raster,
            const PresentationPolicy& policy = {})
        {
            return present(
                frame,
                raster,
                full_surface(frame.compose.viewport.output_surface),
                policy);
        }

        [[nodiscard]] PresentationResult present(
            const Canvas2DFramePlan& frame,
            const cpu::RasterResult& raster,
            PresentationSurface surface,
            const PresentationPolicy& policy = {})
        {
            ++metrics_.requests;
            PresentationResult output{};
            output.raster_code = raster.code;
            output.frame_sequence = frame.frame_sequence;
            output.canvas_hash = raster.canvas_hash;

            if (!valid(policy))
                return reject(output, PresentationCode::invalid_policy);
            if (!detail::accepted_frame(frame, policy))
                return reject(output, PresentationCode::invalid_frame);
            ++metrics_.accepted_frames;
            if (!detail::accepted_raster(frame, raster, policy))
            {
                ++metrics_.raster_failures;
                return reject(output, PresentationCode::invalid_raster, false);
            }
            if (!surface.valid_for(frame.compose.viewport.output_surface))
                return reject(output, PresentationCode::invalid_frame, false);
            if (!hooks_.ready())
                return reject(output, PresentationCode::missing_native_hook, false);

            const std::uint64_t bytes = detail::image_bytes(raster.canvas);
            if (bytes == 0 || bytes > raster.canvas.pixels.size() * sizeof(cpu::Rgba8))
            {
                ++metrics_.raster_failures;
                return reject(output, PresentationCode::invalid_raster, false);
            }

            texture_residency::ResidencyRequest request{};
            request.artifact = make_canvas_artifact_key(raster.canvas, raster.canvas_hash);
            request.payload = {
                raster.canvas.pixels.data(),
                bytes,
                raster.canvas.extent.width * static_cast<std::uint32_t>(sizeof(cpu::Rgba8))
            };
            request.policy.allow_eviction = policy.allow_eviction;
            request.policy.require_backend_ready = policy.require_backend_ready;
            request.policy.pin = false;
            request.policy.evictable = true;
            request.policy.priority = policy.priority;
            request.frame_sequence = frame.frame_sequence;
            request.debug_name = "Canvas2D.CpuCanvas";

            const texture_residency::AcquireResult acquired = residency_.acquire(request);
            output.residency_code = acquired.code;
            output.residency = acquired.handle;
            output.physical = acquired.physical;
            output.resident_bytes = acquired.resident_bytes;
            output.cache_reused = acquired.code == texture_residency::ResidencyCode::reused;
            if (!acquired)
            {
                ++metrics_.residency_failures;
                return reject(output, PresentationCode::residency_failed, false);
            }

            ++metrics_.native_present_calls;
            const NativePresentationPacket packet{
                acquired.physical,
                &frame.compose,
                surface,
                ImageContract{
                    raster.canvas.extent,
                    raster.canvas.format,
                    PixelOrigin::top_left,
                    SpriteColorSpace::linear,
                    cpu::AlphaEncoding::premultiplied},
                frame.frame_sequence,
                raster.canvas_hash};
            if (!packet || !hooks_.present(hooks_.user, packet))
            {
                ++metrics_.native_present_failures;
                return reject(output, PresentationCode::native_present_failed, false);
            }

            if (output.cache_reused)
                ++metrics_.reused_frames;
            else
                ++metrics_.uploaded_frames;
            ++metrics_.presented_frames;
            metrics_.presented_pixels += raster.canvas.pixels.size();
            metrics_.presented_bytes += bytes;
            metrics_.last_frame_sequence = frame.frame_sequence;
            metrics_.last_canvas_hash = raster.canvas_hash;
            output.code = PresentationCode::presented;
            return output;
        }

        [[nodiscard]] PresentationResult rasterize_and_present(
            const Canvas2DFramePlan& frame,
            const cpu::ResourceBindings& resources = {},
            const cpu::RasterLimits& raster_limits = {},
            const cpu::RasterPolicy& raster_policy = {},
            const PresentationPolicy& presentation_policy = {})
        {
            const cpu::RasterResult raster = cpu::rasterize(
                frame,
                resources,
                raster_limits,
                raster_policy);
            return present(frame, raster, presentation_policy);
        }

        [[nodiscard]] bool reset_backend_epoch(std::uint64_t epoch) noexcept
        {
            return residency_.reset_backend_epoch(epoch);
        }

        [[nodiscard]] bool replace_native_hooks(
            NativePresentationHooks hooks,
            std::uint64_t backend_epoch) noexcept
        {
            if (!hooks.ready() || !residency_.reset_backend_epoch(backend_epoch))
                return false;
            hooks_ = hooks;
            return true;
        }

        void retire_all() noexcept
        {
            residency_.retire_all();
        }

        [[nodiscard]] const PresentationMetrics& metrics() const noexcept
        {
            return metrics_;
        }

        [[nodiscard]] const texture_residency::TextureResidencyCache& residency() const noexcept
        {
            return residency_;
        }

    private:
        [[nodiscard]] PresentationResult reject(
            PresentationResult output,
            PresentationCode code,
            bool rejected_frame = true) noexcept
        {
            output.code = code;
            if (rejected_frame)
                ++metrics_.rejected_frames;
            return output;
        }

        texture_residency::TextureResidencyCache residency_;
        NativePresentationHooks hooks_{};
        PresentationMetrics metrics_{};
    };

    enum class PresentationContractFailure : std::uint8_t
    {
        none,
        frame_compile,
        cpu_raster,
        missing_hook,
        first_present,
        cache_reuse,
        native_failure,
        backend_reset,
        metrics
    };

    [[nodiscard]] constexpr const char* presentation_contract_failure_name(
        PresentationContractFailure failure) noexcept
    {
        switch (failure)
        {
        case PresentationContractFailure::none: return "pass";
        case PresentationContractFailure::frame_compile: return "frame_compile";
        case PresentationContractFailure::cpu_raster: return "cpu_raster";
        case PresentationContractFailure::missing_hook: return "missing_hook";
        case PresentationContractFailure::first_present: return "first_present";
        case PresentationContractFailure::cache_reuse: return "cache_reuse";
        case PresentationContractFailure::native_failure: return "native_failure";
        case PresentationContractFailure::backend_reset: return "backend_reset";
        case PresentationContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] PresentationContractFailure
        canvas2d_presentation_runtime_contract_failure()
    {
        SpriteIdentityRegistry identities{4};
        const auto sprite = identities.create();
        if (!sprite)
            return PresentationContractFailure::frame_compile;

        ProjectSettings project{};
        project.logical_canvas = {8, 8};
        project.pixels_per_world_unit = 1.0f;
        project.viewport_policy = ViewportPolicy::integer_scale;
        project.presentation_filter = FilterMode::nearest;
        project.pixel_snap = PixelSnapMode::camera_and_sprites;

        SpriteMaterialDeclaration solid{};
        solid.stable_key = 1;
        solid.source = SpriteSourceKind::solid_color;
        solid.alpha = SpriteAlphaMode::opaque;
        const std::array<SpriteSubmission, 1> sprites{{
            {*sprite, solid, {{0.0f, 0.0f}, {4.0f, 4.0f}},
                {0.0f, 0.0f, 1.0f, 1.0f},
                {0.2f, 0.7f, 1.0f, 1.0f}, SpritePhase::world, 0, 0, 0, 1}
        }};
        Canvas2DSubmission submission{};
        submission.project = project;
        submission.sprites = sprites;
        submission.frame_sequence = 1;
        const Canvas2DFramePlan frame = compile_canvas2d_submission(submission, {16, 16});
        if (!frame)
            return PresentationContractFailure::frame_compile;
        const cpu::RasterResult raster = cpu::rasterize(frame);
        if (!raster || raster.canvas_hash == 0)
            return PresentationContractFailure::cpu_raster;

        detail::ContractDevice device{};
        Canvas2DPresenter missingHook{device, 1, {}};
        if (missingHook.present(frame, raster).code
            != PresentationCode::missing_native_hook)
        {
            return PresentationContractFailure::missing_hook;
        }

        detail::ContractNativeState native{};
        Canvas2DPresenter presenter{
            device,
            1,
            NativePresentationHooks{&native, detail::contract_present}};
        const PresentationResult first = presenter.present(frame, raster);
        if (!first || first.cache_reused || native.calls != 1
            || native.last_texture != first.physical
            || device.create_count != 1 || device.upload_count != 1)
        {
            return PresentationContractFailure::first_present;
        }

        const PresentationResult reused = presenter.present(frame, raster);
        if (!reused || !reused.cache_reused
            || reused.residency != first.residency
            || reused.physical != first.physical
            || native.calls != 2 || device.create_count != 1
            || device.upload_count != 1)
        {
            return PresentationContractFailure::cache_reuse;
        }

        native.fail_next = true;
        if (presenter.present(frame, raster).code
                != PresentationCode::native_present_failed
            || native.calls != 3)
        {
            return PresentationContractFailure::native_failure;
        }

        if (!presenter.reset_backend_epoch(2))
            return PresentationContractFailure::backend_reset;
        const PresentationResult recreated = presenter.present(frame, raster);
        if (!recreated || recreated.cache_reused
            || device.create_count != 2 || device.upload_count != 2
            || device.destroy_count != 1)
        {
            return PresentationContractFailure::backend_reset;
        }

        const PresentationMetrics& metrics = presenter.metrics();
        if (metrics.requests != 4 || metrics.accepted_frames != 4
            || metrics.native_present_calls != 4
            || metrics.native_present_failures != 1
            || metrics.uploaded_frames != 2
            || metrics.reused_frames != 1
            || metrics.presented_frames != 3
            || metrics.last_frame_sequence != 1
            || metrics.last_canvas_hash != raster.canvas_hash)
        {
            return PresentationContractFailure::metrics;
        }
        return PresentationContractFailure::none;
    }
}
