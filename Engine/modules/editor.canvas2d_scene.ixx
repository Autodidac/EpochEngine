/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

export module editor.canvas2d_scene;

import render.canvas2d;
import render.canvas2d_scene;

export namespace epochengine::editor_canvas2d
{
    struct MaterialView final
    {
        std::uint32_t stable_key{1};
        canvas2d::LogicalTextureReference logical_texture{};
        canvas2d::SpriteSamplerDeclaration sampler{};
        canvas2d::SpriteAlphaMode alpha{canvas2d::SpriteAlphaMode::opaque};
        canvas2d::SpriteColorSpace color_space{
            canvas2d::SpriteColorSpace::linear};
        float alpha_cutoff{0.5f};
    };

    struct EntityView final
    {
        std::uint64_t stable_id{};
        std::uint32_t generation{};
        std::string_view type{};
        std::string_view category{};
        std::array<float, 3> position{};
        std::array<float, 3> rotation{};
        std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
        bool visible{true};
        bool editor_only{};
        bool selected{};
        MaterialView material{};
    };

    struct BuildRequest final
    {
        canvas2d::ProjectSettings project{};
        std::span<const EntityView> entities{};
        canvas2d::scene_content::ResourceLease resources{};
        std::uint64_t source_revision{1};
        bool include_helpers{true};
    };

    enum class BuildCode : std::uint8_t
    {
        ready,
        invalid_request,
        invalid_identity,
        identity_collision,
        capacity_exceeded,
        allocation_failure,
        invalid_resources,
        invalid_scene
    };

    [[nodiscard]] constexpr std::string_view build_code_name(BuildCode code) noexcept
    {
        switch (code)
        {
        case BuildCode::ready: return "ready";
        case BuildCode::invalid_request: return "invalid_request";
        case BuildCode::invalid_identity: return "invalid_identity";
        case BuildCode::identity_collision: return "identity_collision";
        case BuildCode::capacity_exceeded: return "capacity_exceeded";
        case BuildCode::allocation_failure: return "allocation_failure";
        case BuildCode::invalid_resources: return "invalid_resources";
        case BuildCode::invalid_scene: return "invalid_scene";
        }
        return "unknown";
    }

    struct BuildDiagnostics final
    {
        std::uint64_t submitted_entities{};
        std::uint64_t emitted_sprites{};
        std::uint64_t hidden_entities{};
        std::uint64_t editor_only_entities{};
        std::uint64_t structural_entities{};
        std::uint64_t helper_entities{};
        std::uint64_t solid_materials{};
        std::uint64_t textured_materials{};
    };

    struct BuildResult final
    {
        BuildCode code{BuildCode::invalid_request};
        canvas2d::scene_content::SceneContent content{};
        BuildDiagnostics diagnostics{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == BuildCode::ready && content.valid();
        }
    };

    [[nodiscard]] BuildResult build_scene(const BuildRequest& request) noexcept;

    enum class ContractFailure : std::uint8_t
    {
        none,
        invalid_request,
        build,
        filtering,
        identity,
        selected_color,
        textured_material,
        resource_closure,
        cpu_frame
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::invalid_request: return "invalid_request";
        case ContractFailure::build: return "build";
        case ContractFailure::filtering: return "filtering";
        case ContractFailure::identity: return "identity";
        case ContractFailure::selected_color: return "selected_color";
        case ContractFailure::textured_material: return "textured_material";
        case ContractFailure::resource_closure: return "resource_closure";
        case ContractFailure::cpu_frame: return "cpu_frame";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract() noexcept;
}
