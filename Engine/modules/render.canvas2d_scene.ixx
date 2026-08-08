/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
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

    struct ResourceLease final
    {
        std::shared_ptr<const void> owner{};
        cpu::ResourceBindings bindings{};

        [[nodiscard]] bool valid() const noexcept;
    };

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
