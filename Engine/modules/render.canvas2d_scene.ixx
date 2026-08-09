/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

export module render.canvas2d_scene;

import render.canvas2d;
import render.canvas2d_cpu;

export namespace epochengine::canvas2d::scene_content
{
    enum class SceneCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_owner,
        invalid_content,
        invalid_resources,
        capacity_exceeded,
        allocation_failure,
        not_found
    };

    [[nodiscard]] constexpr std::string_view scene_code_name(SceneCode code) noexcept
    {
        switch (code)
        {
        case SceneCode::ready: return "ready";
        case SceneCode::unchanged: return "unchanged";
        case SceneCode::invalid_owner: return "invalid_owner";
        case SceneCode::invalid_content: return "invalid_content";
        case SceneCode::invalid_resources: return "invalid_resources";
        case SceneCode::capacity_exceeded: return "capacity_exceeded";
        case SceneCode::allocation_failure: return "allocation_failure";
        case SceneCode::not_found: return "not_found";
        }
        return "unknown";
    }

    class ResourceLifetime final
    {
    public:
        ResourceLifetime() noexcept = default;

        template <typename Resource>
        [[nodiscard]] static ResourceLifetime retain(
            std::shared_ptr<const Resource> resource) noexcept
        {
            ResourceLifetime result{};
            result.state_ = std::move(resource);
            return result;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(state_);
        }

        [[nodiscard]] long use_count() const noexcept
        {
            return state_.use_count();
        }

    private:
        std::shared_ptr<const void> state_{};
    };

    struct ResourceLease
    {
        ResourceLifetime owner{};
        cpu::ResourceBindings bindings{};

        [[nodiscard]] bool valid() const noexcept;
    };

    enum class ResourceClosureCode : std::uint8_t
    {
        ready,
        invalid_lease,
        invalid_reference,
        invalid_binding,
        duplicate_binding,
        missing_binding,
        mismatched_binding,
        unexpected_binding,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view resource_closure_code_name(
        ResourceClosureCode code) noexcept
    {
        switch (code)
        {
        case ResourceClosureCode::ready: return "ready";
        case ResourceClosureCode::invalid_lease: return "invalid_lease";
        case ResourceClosureCode::allocation_failure: return "allocation_failure";
        case ResourceClosureCode::invalid_reference: return "invalid_reference";
        case ResourceClosureCode::invalid_binding: return "invalid_binding";
        case ResourceClosureCode::duplicate_binding: return "duplicate_binding";
        case ResourceClosureCode::missing_binding: return "missing_binding";
        case ResourceClosureCode::mismatched_binding: return "mismatched_binding";
        case ResourceClosureCode::unexpected_binding: return "unexpected_binding";
        }
        return "unknown";
    }

    struct LogicalTextureReferenceHash final
    {
        [[nodiscard]] std::size_t operator()(
            LogicalTextureReference reference) const noexcept
        {
            std::uint64_t value = reference.asset_key;
            value ^= reference.artifact_revision + 0x9e3779b97f4a7c15ull
                + (value << 6u) + (value >> 2u);
            if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t))
                value ^= value >> 32u;
            return static_cast<std::size_t>(value);
        }
    };

    [[nodiscard]] inline ResourceClosureCode validate_resource_closure(
        std::span<const SpriteSubmission> sprites,
        const ResourceLease& lease) noexcept
    {
        if ((!lease.bindings.textures.empty() || !lease.bindings.clips.empty())
            && !lease.owner)
        {
            return ResourceClosureCode::invalid_lease;
        }

        const auto validTextureView = [](const cpu::TextureView& view) noexcept
        {
            if (!view.logical || view.extent.empty()
                || view.row_stride_pixels < view.extent.width)
            {
                return false;
            }
            const std::uint64_t required = static_cast<std::uint64_t>(
                view.row_stride_pixels) * view.extent.height;
            return required <= view.pixels.size()
                && (view.color_space == SpriteColorSpace::linear
                    || view.color_space == SpriteColorSpace::srgb)
                && (view.alpha_encoding == cpu::AlphaEncoding::straight
                    || view.alpha_encoding == cpu::AlphaEncoding::premultiplied);
        };

        using LogicalSet = std::unordered_set<
            LogicalTextureReference, LogicalTextureReferenceHash>;
        LogicalSet boundTextures{};
        LogicalSet requiredTextures{};
        std::unordered_set<std::uint32_t> clipKeys{};
        try
        {
            boundTextures.reserve(lease.bindings.textures.size());
            requiredTextures.reserve((std::min)(
                sprites.size(), lease.bindings.textures.size()));
            clipKeys.reserve(lease.bindings.clips.size());

            for (const cpu::TextureView& view : lease.bindings.textures)
            {
                if (!validTextureView(view))
                    return ResourceClosureCode::invalid_binding;
                if (!boundTextures.insert(view.logical).second)
                    return ResourceClosureCode::duplicate_binding;
            }
            for (const cpu::ClipRect& clip : lease.bindings.clips)
            {
                if (clip.key == 0 || clip.rectangle.empty())
                    return ResourceClosureCode::invalid_binding;
                if (!clipKeys.insert(clip.key).second)
                    return ResourceClosureCode::duplicate_binding;
            }

            for (const SpriteSubmission& sprite : sprites)
            {
                const bool textureSource =
                    sprite.material.source == SpriteSourceKind::texture;
                if (!textureSource)
                {
                    if (sprite.material.logical_texture)
                        return ResourceClosureCode::invalid_reference;
                    continue;
                }
                if (!sprite.material.logical_texture)
                    return ResourceClosureCode::invalid_reference;
                requiredTextures.insert(sprite.material.logical_texture);
            }
        }
        catch (...)
        {
            return ResourceClosureCode::allocation_failure;
        }

        for (const LogicalTextureReference required : requiredTextures)
        {
            if (boundTextures.contains(required))
                continue;
            for (const LogicalTextureReference bound : boundTextures)
            {
                if (bound.asset_key == required.asset_key)
                    return ResourceClosureCode::mismatched_binding;
            }
            return ResourceClosureCode::missing_binding;
        }

        for (const LogicalTextureReference bound : boundTextures)
        {
            if (requiredTextures.contains(bound))
                continue;
            for (const LogicalTextureReference required : requiredTextures)
            {
                if (required.asset_key == bound.asset_key)
                    return ResourceClosureCode::mismatched_binding;
            }
            return ResourceClosureCode::unexpected_binding;
        }
        return ResourceClosureCode::ready;
    }

    struct SceneContent final
    {
        ProjectSettings project{};
        CameraState camera{};
        std::vector<SpriteSubmission> sprites{};
        ResourceLease resources{};
        LinearColor clear_color{0.03f, 0.04f, 0.06f, 1.0f};
        LinearColor letterbox_color{0.0f, 0.0f, 0.0f, 1.0f};
        std::uint64_t source_revision{1};

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] Canvas2DSubmission submission(
            std::uint64_t frame_sequence) const noexcept;
    };

    struct SceneLimits final
    {
        std::uint32_t maximum_slots{64};
        std::uint32_t maximum_sprites{65'536};
        std::uint32_t maximum_textures{4'096};
        std::uint32_t maximum_clips{4'096};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_slots != 0 && maximum_sprites != 0
                && maximum_textures != 0 && maximum_clips != 0;
        }
    };

    struct SceneMetrics final
    {
        std::uint32_t active_slots{};
        std::uint64_t publication_requests{};
        std::uint64_t publications{};
        std::uint64_t unchanged_publications{};
        std::uint64_t rejected_publications{};
        std::uint64_t acquisitions{};
        std::uint64_t misses{};
        std::uint64_t retirements{};
        std::uint64_t published_sprites{};
        std::uint64_t published_texture_views{};
    };

    struct PublicationResult final
    {
        SceneCode code{SceneCode::invalid_content};
        std::uint64_t generation{};
        std::uint64_t frame_sequence{};
        std::uint64_t content_hash{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return (code == SceneCode::ready || code == SceneCode::unchanged)
                && generation != 0 && frame_sequence != 0 && content_hash != 0;
        }
    };

    struct AcquiredScene final
    {
        std::shared_ptr<const SceneContent> content{};
        std::uint64_t generation{};
        std::uint64_t frame_sequence{};
        std::uint64_t content_hash{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return content && generation != 0 && frame_sequence != 0
                && content_hash != 0;
        }
    };

    [[nodiscard]] std::uint64_t content_hash(const SceneContent& content) noexcept;

    [[nodiscard]] PublicationResult publish(
        const void* owner,
        SceneContent content,
        const SceneLimits& limits = {}) noexcept;

    [[nodiscard]] AcquiredScene acquire(const void* owner) noexcept;

    [[nodiscard]] SceneCode retire(const void* owner) noexcept;

    void retire_all() noexcept;

    [[nodiscard]] SceneMetrics metrics() noexcept;

    [[nodiscard]] Canvas2DFramePlan compile(
        const AcquiredScene& scene,
        CanvasExtent output_surface,
        const CanvasLimits& canvas_limits = {}) noexcept;
    enum class ResourceClosureContractFailure : std::uint8_t
    {
        none,
        valid_closure,
        duplicate_binding,
        missing_binding,
        mismatched_binding,
        unexpected_binding,
        atomic_rejection,
        old_reader_lifetime,
        retirement
    };

    [[nodiscard]] constexpr std::string_view resource_closure_contract_failure_name(
        ResourceClosureContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ResourceClosureContractFailure::none: return "pass";
        case ResourceClosureContractFailure::valid_closure: return "valid_closure";
        case ResourceClosureContractFailure::duplicate_binding:
            return "duplicate_binding";
        case ResourceClosureContractFailure::missing_binding: return "missing_binding";
        case ResourceClosureContractFailure::mismatched_binding:
            return "mismatched_binding";
        case ResourceClosureContractFailure::unexpected_binding:
            return "unexpected_binding";
        case ResourceClosureContractFailure::atomic_rejection:
            return "atomic_rejection";
        case ResourceClosureContractFailure::old_reader_lifetime:
            return "old_reader_lifetime";
        case ResourceClosureContractFailure::retirement: return "retirement";
        }
        return "unknown";
    }

    [[nodiscard]] inline ResourceClosureContractFailure
        run_resource_closure_contract() noexcept
    {
        const LogicalTextureReference requested{17, 23};
        using ContractPixels = std::array<cpu::Rgba8, 4>;
        std::shared_ptr<const ContractPixels> storage{};
        try
        {
            storage = std::make_shared<const ContractPixels>(ContractPixels{{
                {255, 0, 0, 255},
                {0, 255, 0, 255},
                {0, 0, 255, 255},
                {255, 255, 255, 255}}});
        }
        catch (...)
        {
            return ResourceClosureContractFailure::valid_closure;
        }
        const std::weak_ptr<const ContractPixels> retained = storage;

        const cpu::TextureView exactView{
            .logical = requested,
            .extent = {2, 2},
            .row_stride_pixels = 2,
            .pixels = *storage,
            .color_space = SpriteColorSpace::linear,
            .alpha_encoding = cpu::AlphaEncoding::straight};
        const cpu::TextureView mismatchedView{
            .logical = {requested.asset_key, requested.artifact_revision + 1},
            .extent = {2, 2},
            .row_stride_pixels = 2,
            .pixels = *storage,
            .color_space = SpriteColorSpace::linear,
            .alpha_encoding = cpu::AlphaEncoding::straight};
        const cpu::TextureView unexpectedView{
            .logical = {requested.asset_key + 1, requested.artifact_revision},
            .extent = {2, 2},
            .row_stride_pixels = 2,
            .pixels = *storage,
            .color_space = SpriteColorSpace::linear,
            .alpha_encoding = cpu::AlphaEncoding::straight};
        const std::array exactBindings{exactView};
        const std::array duplicateBindings{exactView, exactView};
        const std::array mismatchedBindings{mismatchedView};
        const std::array unexpectedBindings{exactView, unexpectedView};

        SceneContent content{};
        content.project.logical_canvas = {32, 18};
        content.project.pixels_per_world_unit = 1.0f;
        content.camera.pixels_per_world_unit = 1.0f;
        content.source_revision = 1;
        SpriteSubmission sprite{};
        sprite.sprite = {0, 1};
        sprite.material.stable_key = 1;
        sprite.material.source = SpriteSourceKind::texture;
        sprite.material.logical_texture = requested;
        sprite.material.alpha = SpriteAlphaMode::opaque;
        sprite.material.color_space = SpriteColorSpace::linear;
        sprite.stable_sequence = 1;
        content.sprites.push_back(sprite);
        content.resources.owner = ResourceLifetime::retain(storage);
        content.resources.bindings = {exactBindings, {}};
        if (!content.valid()
            || validate_resource_closure(content.sprites, content.resources)
                != ResourceClosureCode::ready)
        {
            return ResourceClosureContractFailure::valid_closure;
        }

        {
            SceneContent duplicate = content;
            duplicate.resources.bindings = {duplicateBindings, {}};
            if (duplicate.valid()
                || validate_resource_closure(
                    duplicate.sprites, duplicate.resources)
                    != ResourceClosureCode::duplicate_binding)
            {
                return ResourceClosureContractFailure::duplicate_binding;
            }
        }
        {
            SceneContent missing = content;
            missing.resources.bindings = {};
            if (missing.valid()
                || validate_resource_closure(
                    missing.sprites, missing.resources)
                    != ResourceClosureCode::missing_binding)
            {
                return ResourceClosureContractFailure::missing_binding;
            }
        }
        {
            SceneContent mismatched = content;
            mismatched.resources.bindings = {mismatchedBindings, {}};
            if (mismatched.valid()
                || validate_resource_closure(
                    mismatched.sprites, mismatched.resources)
                    != ResourceClosureCode::mismatched_binding)
            {
                return ResourceClosureContractFailure::mismatched_binding;
            }
        }
        {
            SceneContent unexpected = content;
            unexpected.resources.bindings = {unexpectedBindings, {}};
            if (unexpected.valid()
                || validate_resource_closure(
                    unexpected.sprites, unexpected.resources)
                    != ResourceClosureCode::unexpected_binding)
            {
                return ResourceClosureContractFailure::unexpected_binding;
            }
        }

        int publicationOwner = 0;
        (void)retire(&publicationOwner);
        const PublicationResult first = publish(&publicationOwner, content);
        AcquiredScene oldReader = acquire(&publicationOwner);
        if (!first || !oldReader)
        {
            (void)retire(&publicationOwner);
            return ResourceClosureContractFailure::atomic_rejection;
        }
        const auto preservesPublishedScene = [&](cpu::ResourceBindings bindings,
                                                 std::uint64_t revision)
        {
            SceneContent rejected = content;
            rejected.source_revision = revision;
            rejected.resources.bindings = bindings;
            const PublicationResult rejection = publish(
                &publicationOwner, std::move(rejected));
            const AcquiredScene unchanged = acquire(&publicationOwner);
            return rejection.code == SceneCode::invalid_content && unchanged
                && unchanged.generation == first.generation
                && unchanged.content_hash == first.content_hash;
        };
        if (!preservesPublishedScene({duplicateBindings, {}}, 2)
            || !preservesPublishedScene({}, 3)
            || !preservesPublishedScene({mismatchedBindings, {}}, 4)
            || !preservesPublishedScene({unexpectedBindings, {}}, 5))
        {
            (void)retire(&publicationOwner);
            return ResourceClosureContractFailure::atomic_rejection;
        }

        SceneContent replacement{};
        replacement.project = content.project;
        replacement.camera = content.camera;
        replacement.source_revision = 6;
        content.resources.owner = {};
        storage.reset();
        const PublicationResult replaced = publish(
            &publicationOwner, std::move(replacement));
        if (!replaced || retained.expired())
        {
            (void)retire(&publicationOwner);
            return ResourceClosureContractFailure::old_reader_lifetime;
        }

        oldReader = {};
        if (retire(&publicationOwner) != SceneCode::ready || !retained.expired())
            return ResourceClosureContractFailure::retirement;
        return ResourceClosureContractFailure::none;
    }


    enum class SceneContractFailure : std::uint8_t
    {
        none,
        invalid_rejection,
        first_publication,
        acquisition,
        frame_compilation,
        cpu_raster,
        unchanged_publication,
        replacement,
        immutable_reader,
        owner_isolation,
        retirement,
        metrics
    };

    [[nodiscard]] constexpr std::string_view scene_contract_failure_name(
        SceneContractFailure failure) noexcept
    {
        switch (failure)
        {
        case SceneContractFailure::none: return "pass";
        case SceneContractFailure::invalid_rejection: return "invalid_rejection";
        case SceneContractFailure::first_publication: return "first_publication";
        case SceneContractFailure::acquisition: return "acquisition";
        case SceneContractFailure::frame_compilation: return "frame_compilation";
        case SceneContractFailure::cpu_raster: return "cpu_raster";
        case SceneContractFailure::unchanged_publication: return "unchanged_publication";
        case SceneContractFailure::replacement: return "replacement";
        case SceneContractFailure::immutable_reader: return "immutable_reader";
        case SceneContractFailure::owner_isolation: return "owner_isolation";
        case SceneContractFailure::retirement: return "retirement";
        case SceneContractFailure::metrics: return "metrics";
        }
        return "unknown";
    }

    [[nodiscard]] SceneContractFailure run_scene_content_contract() noexcept;
}
