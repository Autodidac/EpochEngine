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

    enum class ImmutableImageTransition : std::uint8_t
    {
        invalid,
        reuse,
        replace
    };

    struct ImmutableImageSnapshot final
    {
        CanvasExtent extent{};
        std::uint64_t content_hash{};
        std::uint64_t generation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return !extent.empty() && content_hash != 0u && generation != 0u;
        }

        friend constexpr bool operator==(
            ImmutableImageSnapshot,
            ImmutableImageSnapshot) noexcept = default;
    };

    struct ImmutableImagePlan final
    {
        ImmutableImageTransition transition{ImmutableImageTransition::invalid};
        ImmutableImageSnapshot current{};
        ImmutableImageSnapshot next{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return transition != ImmutableImageTransition::invalid
                && static_cast<bool>(next);
        }
    };

    class ImmutableImageLifecycle final
    {
    public:
        [[nodiscard]] constexpr ImmutableImagePlan plan(
            CanvasExtent extent,
            std::uint64_t content_hash) const noexcept
        {
            if (extent.empty() || content_hash == 0u)
                return {};

            if (current_
                && current_.extent == extent
                && current_.content_hash == content_hash)
            {
                return {ImmutableImageTransition::reuse, current_, current_};
            }

            std::uint64_t generation = current_.generation + 1u;
            if (generation == 0u)
                generation = 1u;
            return {
                ImmutableImageTransition::replace,
                current_,
                ImmutableImageSnapshot{extent, content_hash, generation}};
        }

        [[nodiscard]] constexpr bool commit(
            const ImmutableImagePlan& plan,
            std::uint64_t* retired_generation = nullptr) noexcept
        {
            if (retired_generation)
                *retired_generation = 0u;
            if (!plan || plan.current != current_)
                return false;

            if (plan.transition == ImmutableImageTransition::reuse)
                return plan.next == current_;
            if (plan.transition != ImmutableImageTransition::replace
                || plan.next.generation == current_.generation)
            {
                return false;
            }

            if (retired_generation && current_)
                *retired_generation = current_.generation;
            current_ = plan.next;
            return true;
        }

        [[nodiscard]] constexpr std::uint64_t retire_all() noexcept
        {
            const std::uint64_t retired = current_.generation;
            current_ = {};
            return retired;
        }

        [[nodiscard]] constexpr ImmutableImageSnapshot snapshot() const noexcept
        {
            return current_;
        }

    private:
        ImmutableImageSnapshot current_{};
    };

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
        std::uint64_t superseded_releases{};
        std::uint64_t failed_present_releases{};
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

            [[nodiscard]] std::uint32_t active_count() const noexcept
            {
                std::uint32_t count{};
                for (const Record& record : records)
                {
                    if (record.active)
                        ++count;
                }
                return count;
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
            PresentationSurface last_surface{};
            ImageContract last_image{};
            CanvasExtent last_output_surface{};
            CanvasExtent last_render_extent{};
            FilterMode last_filter{FilterMode::nearest};
            std::uint64_t last_frame_sequence{};
            std::uint64_t last_content_hash{};
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
            state->last_surface = packet.surface;
            state->last_image = packet.image;
            state->last_output_surface = packet.compose->viewport.output_surface;
            state->last_render_extent = packet.compose->viewport.render_extent;
            state->last_filter = packet.compose->presentation_filter;
            state->last_frame_sequence = packet.frame_sequence;
            state->last_content_hash = packet.content_hash;
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

            const ImmutableImagePlan image_plan = image_lifecycle_.plan(
                raster.canvas.extent,
                raster.canvas_hash);
            if (!image_plan)
                return reject(output, PresentationCode::invalid_raster, false);

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
                if (acquired.handle != active_residency_
                    && residency_.release(acquired.handle)
                        == texture_residency::ResidencyCode::resident)
                {
                    ++metrics_.failed_present_releases;
                }
                ++metrics_.native_present_failures;
                return reject(output, PresentationCode::native_present_failed, false);
            }

            if (!image_lifecycle_.commit(image_plan))
            {
                if (acquired.handle != active_residency_)
                    (void)residency_.release(acquired.handle);
                ++metrics_.native_present_failures;
                return reject(output, PresentationCode::native_present_failed, false);
            }

            if (active_residency_ && active_residency_ != acquired.handle
                && residency_.release(active_residency_)
                    == texture_residency::ResidencyCode::resident)
            {
                ++metrics_.superseded_releases;
            }
            active_residency_ = acquired.handle;

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
            if (!residency_.reset_backend_epoch(epoch))
                return false;
            active_residency_ = {};
            (void)image_lifecycle_.retire_all();
            return true;
        }

        [[nodiscard]] bool replace_native_hooks(
            NativePresentationHooks hooks,
            std::uint64_t backend_epoch) noexcept
        {
            if (!hooks.ready() || !residency_.reset_backend_epoch(backend_epoch))
                return false;
            active_residency_ = {};
            (void)image_lifecycle_.retire_all();
            hooks_ = hooks;
            return true;
        }

        void retire_all() noexcept
        {
            residency_.retire_all();
            active_residency_ = {};
            (void)image_lifecycle_.retire_all();
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
        texture_residency::ResidencyHandle active_residency_{};
        ImmutableImageLifecycle image_lifecycle_{};
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
        invalid_surface,
        budget_admission,
        resize_metadata,
        native_failure,
        hook_replacement,
        revision_churn,
        backend_reset,
        lifecycle_soak,
        retirement,
        immutable_lifecycle,
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
        case PresentationContractFailure::invalid_surface: return "invalid_surface";
        case PresentationContractFailure::budget_admission: return "budget_admission";
        case PresentationContractFailure::resize_metadata: return "resize_metadata";
        case PresentationContractFailure::native_failure: return "native_failure";
        case PresentationContractFailure::hook_replacement: return "hook_replacement";
        case PresentationContractFailure::revision_churn: return "revision_churn";
        case PresentationContractFailure::backend_reset: return "backend_reset";
        case PresentationContractFailure::lifecycle_soak: return "lifecycle_soak";
        case PresentationContractFailure::retirement: return "retirement";
        case PresentationContractFailure::immutable_lifecycle: return "immutable_lifecycle";
        case PresentationContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] PresentationContractFailure
        canvas2d_presentation_runtime_contract_failure()
    {
        SpriteIdentityRegistry identities{4};
        ImmutableImageLifecycle imageLifecycle{};
        const ImmutableImagePlan invalidImage = imageLifecycle.plan({0u, 8u}, 1u);
        const ImmutableImagePlan initialImage = imageLifecycle.plan({8u, 8u}, 11u);
        if (invalidImage || !initialImage
            || initialImage.transition != ImmutableImageTransition::replace
            || imageLifecycle.snapshot()
            || !imageLifecycle.commit(initialImage)
            || imageLifecycle.snapshot().generation != 1u)
        {
            return PresentationContractFailure::immutable_lifecycle;
        }

        const ImmutableImagePlan reusedImage = imageLifecycle.plan({8u, 8u}, 11u);
        const ImmutableImagePlan sameSizeReplacement =
            imageLifecycle.plan({8u, 8u}, 12u);
        if (!reusedImage
            || reusedImage.transition != ImmutableImageTransition::reuse
            || !sameSizeReplacement
            || sameSizeReplacement.transition != ImmutableImageTransition::replace
            || sameSizeReplacement.current.generation != 1u
            || sameSizeReplacement.next.generation != 2u)
        {
            return PresentationContractFailure::immutable_lifecycle;
        }

        std::uint64_t retiredGeneration = 0u;
        if (!imageLifecycle.commit(sameSizeReplacement, &retiredGeneration)
            || retiredGeneration != 1u
            || imageLifecycle.snapshot().content_hash != 12u)
        {
            return PresentationContractFailure::immutable_lifecycle;
        }

        if (imageLifecycle.retire_all() != 2u
            || imageLifecycle.snapshot())
        {
            return PresentationContractFailure::immutable_lifecycle;
        }

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
        std::array<SpriteSubmission, 1> sprites{{
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

        const PresentationSurface invalidSurface{
            {16, 16}, {1, 0, 16, 16}};
        if (presenter.present(frame, raster, invalidSurface).code
                != PresentationCode::invalid_frame
            || native.calls != 2 || device.create_count != 1
            || device.upload_count != 1 || device.active_count() != 1)
        {
            return PresentationContractFailure::invalid_surface;
        }

        texture_residency::ResidencyLimits constrainedLimits{};
        constrainedLimits.maximum_entries = 2u;
        constrainedLimits.maximum_resident_bytes = 64u;
        constrainedLimits.maximum_single_texture_bytes = 32u;
        constrainedLimits.maximum_transient_replacement_bytes = 32u;
        constrainedLimits.maximum_upload_bytes_per_frame = 32u;
        constrainedLimits.maximum_evictions_per_acquire = 1u;
        detail::ContractDevice constrainedDevice{};
        detail::ContractNativeState constrainedNative{};
        Canvas2DPresenter constrainedPresenter{
            constrainedDevice,
            1u,
            NativePresentationHooks{
                &constrainedNative,
                detail::contract_present},
            constrainedLimits};
        const PresentationResult constrainedResult =
            constrainedPresenter.present(frame, raster);
        if (constrainedResult.code != PresentationCode::residency_failed
            || constrainedResult.residency_code
                != texture_residency::ResidencyCode::resident_budget_exceeded
            || constrainedNative.calls != 0u
            || constrainedDevice.create_count != 0u
            || constrainedDevice.active_count() != 0u
            || constrainedPresenter.metrics().residency_failures != 1u)
        {
            return PresentationContractFailure::budget_admission;
        }

        auto failedSprites = sprites;
        failedSprites[0].tint = {1.0f, 0.1f, 0.4f, 1.0f};
        Canvas2DSubmission failedSubmission = submission;
        failedSubmission.sprites = failedSprites;
        failedSubmission.frame_sequence = 2;
        const Canvas2DFramePlan failedFrame =
            compile_canvas2d_submission(failedSubmission, {16, 16});
        const cpu::RasterResult failedRaster = cpu::rasterize(failedFrame);
        native.fail_next = true;
        if (!failedFrame || !failedRaster
            || failedRaster.canvas_hash == raster.canvas_hash
            || presenter.present(failedFrame, failedRaster).code
                != PresentationCode::native_present_failed
            || native.calls != 3 || device.create_count != 2
            || device.upload_count != 2 || device.destroy_count != 1
            || device.active_count() != 1)
        {
            return PresentationContractFailure::native_failure;
        }

        project.presentation_filter = FilterMode::linear;
        submission.project = project;
        submission.frame_sequence = 3;
        const Canvas2DFramePlan resizedFrame =
            compile_canvas2d_submission(submission, {24, 20});
        const cpu::RasterResult resizedRaster = cpu::rasterize(resizedFrame);
        const PresentationSurface resizedSurface{
            {32, 24}, {4, 2, 24, 20}};
        const PresentationResult resized = presenter.present(
            resizedFrame,
            resizedRaster,
            resizedSurface);
        if (!resized || !resized.cache_reused
            || resized.physical != first.physical
            || resized.canvas_hash != raster.canvas_hash
            || native.calls != 4
            || native.last_surface.framebuffer_extent != CanvasExtent{32, 24}
            || native.last_surface.viewport.x != 4
            || native.last_surface.viewport.y != 2
            || native.last_surface.viewport.width != 24
            || native.last_surface.viewport.height != 20
            || native.last_output_surface != CanvasExtent{24, 20}
            || native.last_render_extent != CanvasExtent{8, 8}
            || native.last_image.extent != CanvasExtent{8, 8}
            || native.last_image.origin != PixelOrigin::top_left
            || native.last_image.color_space != SpriteColorSpace::linear
            || native.last_image.alpha_encoding
                != cpu::AlphaEncoding::premultiplied
            || native.last_filter != FilterMode::linear
            || native.last_frame_sequence != 3
            || native.last_content_hash != resizedRaster.canvas_hash
            || device.create_count != 2 || device.upload_count != 2
            || device.destroy_count != 1 || device.active_count() != 1)
        {
            return PresentationContractFailure::resize_metadata;
        }

        constexpr std::uint32_t revisionCycles = 64;
        std::uint64_t previousRevisionHash = resizedRaster.canvas_hash;
        for (std::uint32_t cycle = 0; cycle < revisionCycles; ++cycle)
        {
            const float red = static_cast<float>(cycle + 96u) / 255.0f;
            sprites[0].tint = {red, 0.25f, 1.0f - red, 1.0f};
            submission.frame_sequence = static_cast<std::uint64_t>(cycle) + 4u;
            const Canvas2DFramePlan revisionFrame =
                compile_canvas2d_submission(submission, {24, 20});
            const cpu::RasterResult revisionRaster = cpu::rasterize(revisionFrame);
            const PresentationResult revision = presenter.present(
                revisionFrame,
                revisionRaster,
                resizedSurface);
            if (!revisionFrame || !revisionRaster || !revision
                || revision.cache_reused
                || revisionRaster.canvas_hash == previousRevisionHash
                || native.calls != cycle + 5u
                || device.active_count() != 1
                || device.records.size() > 2u
                || device.create_count != cycle + 3u
                || device.upload_count != cycle + 3u
                || device.destroy_count != cycle + 2u)
            {
                return PresentationContractFailure::revision_churn;
            }
            previousRevisionHash = revisionRaster.canvas_hash;
        }

        detail::ContractNativeState replacementNative{};
        const std::uint32_t destroysBeforeRejectedReplacement =
            device.destroy_count;
        if (presenter.replace_native_hooks({}, 2)
            || presenter.replace_native_hooks(
                NativePresentationHooks{
                    &replacementNative,
                    detail::contract_present},
                1)
            || device.destroy_count != destroysBeforeRejectedReplacement
            || device.active_count() != 1)
        {
            return PresentationContractFailure::hook_replacement;
        }
        if (!presenter.replace_native_hooks(
                NativePresentationHooks{
                    &replacementNative,
                    detail::contract_present},
                2))
        {
            return PresentationContractFailure::backend_reset;
        }
        const PresentationResult recreated = presenter.present(
            resizedFrame,
            resizedRaster,
            resizedSurface);
        if (!recreated || recreated.cache_reused
            || device.create_count != revisionCycles + 3u
            || device.upload_count != revisionCycles + 3u
            || device.destroy_count != revisionCycles + 2u
            || device.active_count() != 1
            || native.calls != revisionCycles + 4u
            || replacementNative.calls != 1
            || replacementNative.last_texture != recreated.physical)
        {
            return PresentationContractFailure::backend_reset;
        }

        constexpr std::uint32_t replacementCycles = 64;
        for (std::uint32_t cycle = 0; cycle < replacementCycles; ++cycle)
        {
            detail::ContractNativeState& target =
                (cycle & 1u) == 0u ? native : replacementNative;
            detail::ContractNativeState& other =
                (cycle & 1u) == 0u ? replacementNative : native;
            const std::uint32_t targetCalls = target.calls;
            const std::uint32_t otherCalls = other.calls;
            if (!presenter.replace_native_hooks(
                    NativePresentationHooks{&target, detail::contract_present},
                    static_cast<std::uint64_t>(cycle) + 3u))
            {
                return PresentationContractFailure::lifecycle_soak;
            }
            const PresentationResult cycled = presenter.present(
                resizedFrame,
                resizedRaster,
                resizedSurface);
            if (!cycled || cycled.cache_reused
                || target.calls != targetCalls + 1u
                || other.calls != otherCalls
                || target.last_texture != cycled.physical
                || device.active_count() != 1
                || device.create_count != revisionCycles + cycle + 4u
                || device.upload_count != revisionCycles + cycle + 4u
                || device.destroy_count != revisionCycles + cycle + 3u)
            {
                return PresentationContractFailure::lifecycle_soak;
            }
        }

        presenter.retire_all();
        const std::uint32_t retiredDestroyCount = device.destroy_count;
        presenter.retire_all();
        const auto& residencyMetrics = presenter.residency().metrics();
        if (device.active_count() != 0
            || retiredDestroyCount != device.create_count
            || device.destroy_count != retiredDestroyCount
            || residencyMetrics.active_entries != 0
            || residencyMetrics.resident_bytes != 0
            || residencyMetrics.backend_epoch != replacementCycles + 2u
            || residencyMetrics.backend_retirements != replacementCycles + 2u
            || residencyMetrics.explicit_releases != revisionCycles + 1u
            || residencyMetrics.peak_entries != 2u
            || device.records.size() > 2u)
        {
            return PresentationContractFailure::retirement;
        }

        const PresentationMetrics& metrics = presenter.metrics();
        if (metrics.requests != revisionCycles + replacementCycles + 6u
            || metrics.accepted_frames
                != revisionCycles + replacementCycles + 6u
            || metrics.native_present_calls
                != revisionCycles + replacementCycles + 5u
            || metrics.native_present_failures != 1
            || metrics.uploaded_frames
                != revisionCycles + replacementCycles + 2u
            || metrics.reused_frames != 2
            || metrics.superseded_releases != revisionCycles
            || metrics.failed_present_releases != 1
            || metrics.presented_frames
                != revisionCycles + replacementCycles + 4u
            || metrics.last_frame_sequence != 3
            || metrics.last_canvas_hash != resizedRaster.canvas_hash)
        {
            return PresentationContractFailure::metrics;
        }
        return PresentationContractFailure::none;
    }
}
