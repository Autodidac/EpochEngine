/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

export module render.canvas2d_tilemap;

import asset.tilemap_artifact;
import render.canvas2d;
import render.device;

export namespace epochengine::canvas2d::tilemap_runtime
{
    enum class CompileCode : std::uint8_t
    {
        ready,
        partial,
        invalid_artifact,
        invalid_view,
        invalid_limits,
        unsupported_dependency,
        capacity_exceeded,
        arithmetic_overflow,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view compile_code_name(
        CompileCode code) noexcept
    {
        switch (code)
        {
        case CompileCode::ready: return "ready";
        case CompileCode::partial: return "partial";
        case CompileCode::invalid_artifact: return "invalid_artifact";
        case CompileCode::invalid_view: return "invalid_view";
        case CompileCode::invalid_limits: return "invalid_limits";
        case CompileCode::unsupported_dependency:
            return "unsupported_dependency";
        case CompileCode::capacity_exceeded: return "capacity_exceeded";
        case CompileCode::arithmetic_overflow: return "arithmetic_overflow";
        case CompileCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct ViewRequest final
    {
        RectF world_bounds{};
        Float2 culling_margin{};
        std::uint64_t simulated_milliseconds{};
        bool include_collision{true};
        bool include_objects{true};
    };

    struct CompileLimits final
    {
        std::uint32_t maximum_visible_chunks{65'536u};
        std::uint32_t maximum_visible_sprites{262'144u};
        std::uint32_t maximum_visible_collision{262'144u};
        std::uint32_t maximum_visible_objects{65'536u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_visible_chunks != 0u
                && maximum_visible_sprites != 0u
                && maximum_visible_collision != 0u
                && maximum_visible_objects != 0u;
        }
    };

    struct VisibleObject final
    {
        asset::tilemap::MapObjectId id{};
        std::string name{};
        std::string type{};
        Float2 position{};
        Float2 size{1.0f, 1.0f};
        float rotation_radians{};
        std::uint32_t layer_bits{1u};
        std::uint32_t user_flags{};
    };

    struct CompileMetrics final
    {
        std::uint64_t submitted_layers{};
        std::uint64_t visible_layers{};
        std::uint64_t hidden_layers{};
        std::uint64_t submitted_chunks{};
        std::uint64_t visible_chunks{};
        std::uint64_t culled_chunks{};
        std::uint64_t submitted_cells{};
        std::uint64_t emitted_sprites{};
        std::uint64_t emitted_collision{};
        std::uint64_t emitted_objects{};
        std::uint64_t capacity_rejections{};
        std::uint64_t dependency_rejections{};
    };

    struct VisibleCompilation final
    {
        CompileCode code{CompileCode::invalid_artifact};
        std::vector<SpriteSubmission> sprites{};
        std::vector<asset::tilemap::CollisionPrimitive> collision{};
        std::vector<VisibleObject> objects{};
        CompileMetrics metrics{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == CompileCode::ready || code == CompileCode::partial;
        }
    };

    [[nodiscard]] VisibleCompilation compile_visible(
        const asset::tilemap::CompiledTileMapArtifact& artifact,
        const ViewRequest& view,
        const CompileLimits& limits = {}) noexcept;

    enum class ContractFailure : std::uint8_t
    {
        none,
        artifact_contract,
        visible_compile,
        culling,
        ordering,
        material_identity,
        source_uv,
        transforms,
        animation,
        collision_filter,
        object_filter,
        capacity_bound,
        invalid_view_accepted
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::artifact_contract: return "artifact_contract";
        case ContractFailure::visible_compile: return "visible_compile";
        case ContractFailure::culling: return "culling";
        case ContractFailure::ordering: return "ordering";
        case ContractFailure::material_identity: return "material_identity";
        case ContractFailure::source_uv: return "source_uv";
        case ContractFailure::transforms: return "transforms";
        case ContractFailure::animation: return "animation";
        case ContractFailure::collision_filter: return "collision_filter";
        case ContractFailure::object_filter: return "object_filter";
        case ContractFailure::capacity_bound: return "capacity_bound";
        case ContractFailure::invalid_view_accepted:
            return "invalid_view_accepted";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
