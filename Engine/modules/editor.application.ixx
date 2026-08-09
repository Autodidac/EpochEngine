// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module editor.application;

export namespace epochengine
{
    enum class EditorApplicationKind : std::uint8_t
    {
        Standard = 0,
        PlantLab,
        GuiEditor
    };

    enum class EditorApplicationSurface : std::uint8_t
    {
        Scene = 0,
        Game2D,
        Assets,
        Project,
        ForestFactory,
        Timeline,
        AISandbox,
        Systems,
        PlantLab,
        Count
    };

    enum class EditorCameraPolicy : std::uint8_t
    {
        Perspective = 0,
        Canvas2D
    };

    enum class EditorApplicationDock : std::uint8_t
    {
        Output = 0,
        Assets,
        Project
    };

    struct EditorSceneSeedEntity
    {
        std::string name{};
        std::string type{};
        std::string category{};
        std::array<float, 3> position{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotation{ 0.0f, 0.0f, 0.0f };
        std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
        bool visible{ true };
        bool editor_only{ false };
    };

    struct EditorApplicationPanes
    {
        bool outliner{ true };
        bool inspector{ true };
        bool console{ true };
        bool ai_chat{ false };
    };

    struct EditorApplicationProfile
    {
        EditorApplicationKind kind{ EditorApplicationKind::Standard };
        std::string_view id{};
        std::string_view display_name{};
        std::string_view project_id{};
        std::string_view scene_id{};
        std::string_view scene_source_path{};
        std::string_view purpose{};
        EditorApplicationSurface default_surface{ EditorApplicationSurface::Scene };
        EditorCameraPolicy camera_policy{ EditorCameraPolicy::Perspective };
        EditorApplicationDock initial_dock{ EditorApplicationDock::Output };
        std::uint32_t surface_mask{};
        EditorApplicationPanes panes{};
        bool allow_project_run{ false };
        bool allow_entity_authoring{ false };
        bool allow_package_management{ false };
        bool allow_floating_panes{ true };
    };

    struct EditorApplicationScene
    {
        EditorApplicationKind owner{ EditorApplicationKind::Standard };
        std::string scene_id{};
        std::string world_name{};
        std::vector<EditorSceneSeedEntity> entities{};
    };

    [[nodiscard]] const EditorApplicationProfile& standard_editor_application() noexcept;
    [[nodiscard]] const EditorApplicationProfile& plant_lab_editor_application() noexcept;
    [[nodiscard]] const EditorApplicationProfile& gui_editor_application() noexcept;

    [[nodiscard]] EditorApplicationScene make_standard_editor_scene();
    [[nodiscard]] EditorApplicationScene make_plant_lab_editor_scene();
    [[nodiscard]] EditorApplicationScene make_gui_editor_scene();

    [[nodiscard]] constexpr std::uint32_t editor_surface_bit(EditorApplicationSurface surface) noexcept
    {
        const auto ordinal = static_cast<std::uint32_t>(surface);
        return ordinal < static_cast<std::uint32_t>(EditorApplicationSurface::Count)
            ? (std::uint32_t{ 1 } << ordinal)
            : 0u;
    }

    template <typename... Surfaces>
    [[nodiscard]] constexpr std::uint32_t editor_surface_mask(Surfaces... surfaces) noexcept
    {
        return (editor_surface_bit(surfaces) | ... | 0u);
    }

    [[nodiscard]] constexpr bool editor_application_supports_surface(
        const EditorApplicationProfile& application,
        EditorApplicationSurface surface) noexcept
    {
        return (application.surface_mask & editor_surface_bit(surface)) != 0u;
    }

    [[nodiscard]] inline const EditorApplicationProfile& editor_application_profile(
        EditorApplicationKind kind) noexcept
    {
        switch (kind)
        {
        case EditorApplicationKind::PlantLab:
            return plant_lab_editor_application();
        case EditorApplicationKind::GuiEditor:
            return gui_editor_application();
        case EditorApplicationKind::Standard:
        default:
            return standard_editor_application();
        }
    }

    [[nodiscard]] inline EditorApplicationScene make_editor_application_scene(
        EditorApplicationKind kind)
    {
        switch (kind)
        {
        case EditorApplicationKind::PlantLab:
            return make_plant_lab_editor_scene();
        case EditorApplicationKind::GuiEditor:
            return make_gui_editor_scene();
        case EditorApplicationKind::Standard:
        default:
            return make_standard_editor_scene();
        }
    }

    [[nodiscard]] inline const EditorApplicationProfile* editor_application_for_project(
        std::string_view project_id) noexcept
    {
        for (const EditorApplicationKind kind : {
            EditorApplicationKind::Standard,
            EditorApplicationKind::PlantLab,
            EditorApplicationKind::GuiEditor })
        {
            const auto& application = editor_application_profile(kind);
            if (application.project_id == project_id)
                return &application;
        }

        // Preserve the old launch id without reopening the authoring editor:
        // Forest Factory now belongs to the standard editor placement portal.
        if (project_id == "forestfactory")
            return &standard_editor_application();
        return nullptr;
    }

    [[nodiscard]] inline bool validate_editor_application(
        const EditorApplicationProfile& application,
        const EditorApplicationScene& scene) noexcept
    {
        if (application.id.empty()
            || application.display_name.empty()
            || application.project_id.empty()
            || application.scene_id.empty()
            || application.scene_source_path.empty()
            || application.surface_mask == 0u
            || !editor_application_supports_surface(application, application.default_surface)
            || scene.owner != application.kind
            || scene.scene_id != application.scene_id
            || scene.world_name.empty()
            || scene.entities.empty())
        {
            return false;
        }

        std::size_t camera_count = 0u;
        for (std::size_t i = 0u; i < scene.entities.size(); ++i)
        {
            const auto& entity = scene.entities[i];
            if (entity.name.empty() || entity.type.empty() || entity.category.empty())
                return false;
            if (entity.type == "Camera")
                ++camera_count;

            for (std::size_t j = i + 1u; j < scene.entities.size(); ++j)
                if (scene.entities[j].name == entity.name)
                    return false;
        }
        return camera_count == 1u;
    }
}
