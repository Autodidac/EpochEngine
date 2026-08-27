/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module editor.workspace_layout;

export namespace epochengine::editor_workspace
{
    template <typename Tag>
    struct StableId final
    {
        std::uint64_t value{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return value != 0u;
        }

        [[nodiscard]] friend constexpr auto operator<=>(
            const StableId&,
            const StableId&) noexcept = default;
    };

    struct ApplicationTag final {};
    struct DockGroupTag final {};
    struct TabTag final {};
    struct ContentProviderTag final {};

    using ApplicationId = StableId<ApplicationTag>;
    using DockGroupId = StableId<DockGroupTag>;
    using TabId = StableId<TabTag>;
    using ContentProviderId = StableId<ContentProviderTag>;

    [[nodiscard]] constexpr std::uint64_t stable_hash(
        std::string_view canonicalName) noexcept
    {
        std::uint64_t hash = 14'695'981'039'346'656'037ull;
        for (const unsigned char byte : canonicalName)
        {
            hash ^= byte;
            hash *= 1'099'511'628'211ull;
        }
        return hash == 0u ? 1u : hash;
    }

    template <typename Tag>
    [[nodiscard]] constexpr StableId<Tag> make_stable_id(
        std::string_view canonicalName) noexcept
    {
        return StableId<Tag>{stable_hash(canonicalName)};
    }

    enum class ApplicationKind : std::uint8_t
    {
        standard_editor,
        plant_lab,
        gui_editor
    };

    [[nodiscard]] constexpr std::string_view application_kind_name(
        ApplicationKind application) noexcept
    {
        switch (application)
        {
        case ApplicationKind::standard_editor: return "Standard Editor";
        case ApplicationKind::plant_lab: return "Plant Lab";
        case ApplicationKind::gui_editor: return "GUI Editor";
        }
        return "Unknown";
    }

    enum class DockRegion : std::uint8_t
    {
        document_tabs,
        central_document,
        right_upper,
        right_lower,
        bottom
    };

    enum class TabCategory : std::uint8_t
    {
        document,
        tool
    };

    enum class TabStripPlacement : std::uint8_t
    {
        top,
        bottom
    };

    enum class SurfaceKind : std::uint8_t
    {
        viewport_3d,
        canvas_2d,
        node_graph,
        timeline,
        text_editor,
        document_editor,
        hierarchy,
        inspector,
        service_monitor
    };

    enum class ContentRole : std::uint8_t
    {
        world_viewport,
        scene_2d,
        material_graph,
        texture_document,
        script_document,
        forest_factory,
        world_outliner,
        asset_browser,
        script_browser,
        selection_properties,
        world_settings,
        plant_preview,
        plant_graph,
        plant_growth_timeline,
        plant_species_library,
        plant_hierarchy,
        plant_asset_browser,
        plant_properties,
        plant_growth_settings,
        gui_canvas,
        gui_runtime_preview,
        gui_component_graph,
        gui_style_document,
        gui_hierarchy,
        gui_asset_browser,
        gui_properties,
        gui_canvas_settings,
        activity_log,
        task_graph,
        systems_diagnostics,
        build_pipeline
    };

    enum class VisibilityRule : std::uint8_t
    {
        always_available,
        project_required,
        document_required,
        provider_required
    };

    enum class CloseRule : std::uint8_t
    {
        fixed,
        hideable,
        document_closeable
    };

    // Workspace layout never selects a graphics API or owns a renderer context.
    // The view host binds a context to a visible viewport after layout resolution.
    enum class RendererBindingPolicy : std::uint8_t
    {
        external_view_host
    };

    struct DockGroupDescriptor final
    {
        DockGroupId id{};
        std::string canonical_name{};
        std::string label{};
        DockRegion tab_region{DockRegion::document_tabs};
        DockRegion content_region{DockRegion::central_document};
        TabCategory accepted_category{TabCategory::document};
        TabStripPlacement tab_strip{TabStripPlacement::top};
        std::uint16_t order{};
        float default_fraction{1.0f};
        std::uint16_t minimum_extent{96u};
        bool tabs_reorderable{true};
        bool tabs_detachable{true};
    };

    struct TabDescriptor final
    {
        TabId id{};
        std::string canonical_name{};
        std::string label{};
        DockGroupId group{};
        ContentProviderId provider{};
        TabCategory category{TabCategory::document};
        SurfaceKind surface{SurfaceKind::document_editor};
        ContentRole content{ContentRole::world_viewport};
        VisibilityRule visibility{VisibilityRule::provider_required};
        CloseRule close_rule{CloseRule::document_closeable};
        std::uint16_t order{};
        bool default_open{};
        bool default_active{};
        bool reorderable{true};
        bool detachable{true};
    };

    struct LayoutLimits final
    {
        std::size_t maximum_groups{16u};
        std::size_t maximum_tabs{128u};
        std::size_t maximum_name_bytes{128u};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_groups >= 4u
                && maximum_groups <= 64u
                && maximum_tabs >= 8u
                && maximum_tabs <= 1'024u
                && maximum_name_bytes >= 16u
                && maximum_name_bytes <= 1'024u;
        }
    };

    struct WorkspaceLayout final
    {
        static constexpr std::uint32_t current_schema_version = 1u;

        std::uint32_t schema_version{current_schema_version};
        ApplicationId application_id{};
        ApplicationKind application{ApplicationKind::standard_editor};
        std::string canonical_name{};
        std::string label{};
        RendererBindingPolicy renderer_binding{
            RendererBindingPolicy::external_view_host};
        LayoutLimits limits{};
        std::vector<DockGroupDescriptor> groups{};
        std::vector<TabDescriptor> tabs{};

        [[nodiscard]] const DockGroupDescriptor* find_group(
            DockGroupId id) const noexcept
        {
            const auto found = std::find_if(
                groups.begin(), groups.end(), [id](const auto& group)
                {
                    return group.id == id;
                });
            return found == groups.end() ? nullptr : &*found;
        }

        [[nodiscard]] const TabDescriptor* find_tab(TabId id) const noexcept
        {
            const auto found = std::find_if(
                tabs.begin(), tabs.end(), [id](const auto& tab)
                {
                    return tab.id == id;
                });
            return found == tabs.end() ? nullptr : &*found;
        }

        [[nodiscard]] const TabDescriptor* find_content(
            ContentRole role) const noexcept
        {
            const auto found = std::find_if(
                tabs.begin(), tabs.end(), [role](const auto& tab)
                {
                    return tab.content == role;
                });
            return found == tabs.end() ? nullptr : &*found;
        }
    };

    enum class ValidationCode : std::uint8_t
    {
        valid,
        invalid_limits,
        invalid_schema,
        invalid_application,
        invalid_renderer_binding,
        empty_application_name,
        invalid_application_identity,
        group_capacity_exceeded,
        tab_capacity_exceeded,
        invalid_group,
        invalid_group_identity,
        duplicate_group_id,
        duplicate_group_name,
        duplicate_group_order,
        invalid_group_region,
        missing_shell_region,
        invalid_tab,
        invalid_tab_identity,
        duplicate_tab_id,
        duplicate_tab_name,
        missing_tab_group,
        category_mismatch,
        invalid_surface,
        invalid_close_rule,
        invalid_default_active,
        duplicate_tab_order,
        missing_group_active_tab,
        multiple_group_active_tabs,
        missing_required_content
    };

    [[nodiscard]] constexpr std::string_view validation_code_name(
        ValidationCode code) noexcept
    {
        switch (code)
        {
        case ValidationCode::valid: return "valid";
        case ValidationCode::invalid_limits: return "invalid_limits";
        case ValidationCode::invalid_schema: return "invalid_schema";
        case ValidationCode::invalid_application: return "invalid_application";
        case ValidationCode::invalid_renderer_binding:
            return "invalid_renderer_binding";
        case ValidationCode::empty_application_name:
            return "empty_application_name";
        case ValidationCode::invalid_application_identity:
            return "invalid_application_identity";
        case ValidationCode::group_capacity_exceeded:
            return "group_capacity_exceeded";
        case ValidationCode::tab_capacity_exceeded:
            return "tab_capacity_exceeded";
        case ValidationCode::invalid_group: return "invalid_group";
        case ValidationCode::invalid_group_identity:
            return "invalid_group_identity";
        case ValidationCode::duplicate_group_id: return "duplicate_group_id";
        case ValidationCode::duplicate_group_name:
            return "duplicate_group_name";
        case ValidationCode::duplicate_group_order:
            return "duplicate_group_order";
        case ValidationCode::invalid_group_region:
            return "invalid_group_region";
        case ValidationCode::missing_shell_region:
            return "missing_shell_region";
        case ValidationCode::invalid_tab: return "invalid_tab";
        case ValidationCode::invalid_tab_identity:
            return "invalid_tab_identity";
        case ValidationCode::duplicate_tab_id: return "duplicate_tab_id";
        case ValidationCode::duplicate_tab_name: return "duplicate_tab_name";
        case ValidationCode::missing_tab_group: return "missing_tab_group";
        case ValidationCode::category_mismatch: return "category_mismatch";
        case ValidationCode::invalid_surface: return "invalid_surface";
        case ValidationCode::invalid_close_rule: return "invalid_close_rule";
        case ValidationCode::invalid_default_active:
            return "invalid_default_active";
        case ValidationCode::duplicate_tab_order:
            return "duplicate_tab_order";
        case ValidationCode::missing_group_active_tab:
            return "missing_group_active_tab";
        case ValidationCode::multiple_group_active_tabs:
            return "multiple_group_active_tabs";
        case ValidationCode::missing_required_content:
            return "missing_required_content";
        }
        return "unknown";
    }

    struct ValidationReport final
    {
        ValidationCode code{ValidationCode::valid};
        DockGroupId group{};
        TabId tab{};
        std::size_t index{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return code == ValidationCode::valid;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return passed();
        }
    };

    namespace detail
    {
        [[nodiscard]] inline bool valid_application_kind(
            ApplicationKind application) noexcept
        {
            switch (application)
            {
            case ApplicationKind::standard_editor:
            case ApplicationKind::plant_lab:
            case ApplicationKind::gui_editor:
                return true;
            }
            return false;
        }

        [[nodiscard]] inline bool valid_group_regions(
            const DockGroupDescriptor& group) noexcept
        {
            if (group.accepted_category == TabCategory::document)
            {
                return group.tab_region == DockRegion::document_tabs
                    && group.content_region == DockRegion::central_document
                    && group.tab_strip == TabStripPlacement::top;
            }
            return group.tab_region == group.content_region
                && (group.content_region == DockRegion::right_upper
                    || group.content_region == DockRegion::right_lower
                    || group.content_region == DockRegion::bottom);
        }

        [[nodiscard]] inline bool valid_surface(
            TabCategory category,
            SurfaceKind surface) noexcept
        {
            if (category == TabCategory::document)
            {
                return surface == SurfaceKind::viewport_3d
                    || surface == SurfaceKind::canvas_2d
                    || surface == SurfaceKind::node_graph
                    || surface == SurfaceKind::timeline
                    || surface == SurfaceKind::text_editor
                    || surface == SurfaceKind::document_editor;
            }
            return surface == SurfaceKind::hierarchy
                || surface == SurfaceKind::inspector
                || surface == SurfaceKind::service_monitor;
        }

        [[nodiscard]] inline bool has_region(
            const WorkspaceLayout& layout,
            DockRegion region) noexcept
        {
            return std::any_of(
                layout.groups.begin(), layout.groups.end(),
                [region](const auto& group)
                {
                    return group.tab_region == region
                        || group.content_region == region;
                });
        }

        [[nodiscard]] inline bool has_required_content(
            const WorkspaceLayout& layout,
            ContentRole role) noexcept
        {
            return layout.find_content(role) != nullptr;
        }

        [[nodiscard]] inline DockGroupDescriptor make_group(
            std::string_view canonicalName,
            std::string_view label,
            DockRegion tabRegion,
            DockRegion contentRegion,
            TabCategory category,
            std::uint16_t order,
            float fraction,
            std::uint16_t minimumExtent)
        {
            return DockGroupDescriptor{
                .id = make_stable_id<DockGroupTag>(canonicalName),
                .canonical_name = std::string{canonicalName},
                .label = std::string{label},
                .tab_region = tabRegion,
                .content_region = contentRegion,
                .accepted_category = category,
                .tab_strip = category == TabCategory::document
                    ? TabStripPlacement::top
                    : TabStripPlacement::top,
                .order = order,
                .default_fraction = fraction,
                .minimum_extent = minimumExtent,
                .tabs_reorderable = true,
                .tabs_detachable = true};
        }

        inline void add_tab(
            WorkspaceLayout& layout,
            std::string_view canonicalName,
            std::string_view label,
            std::string_view groupName,
            std::string_view providerName,
            TabCategory category,
            SurfaceKind surface,
            ContentRole content,
            VisibilityRule visibility,
            CloseRule closeRule,
            std::uint16_t order,
            bool defaultOpen,
            bool defaultActive,
            bool detachable = true)
        {
            layout.tabs.push_back(TabDescriptor{
                .id = make_stable_id<TabTag>(canonicalName),
                .canonical_name = std::string{canonicalName},
                .label = std::string{label},
                .group = make_stable_id<DockGroupTag>(groupName),
                .provider = make_stable_id<ContentProviderTag>(providerName),
                .category = category,
                .surface = surface,
                .content = content,
                .visibility = visibility,
                .close_rule = closeRule,
                .order = order,
                .default_open = defaultOpen,
                .default_active = defaultActive,
                .reorderable = true,
                .detachable = detachable});
        }

        inline void add_shell_groups(WorkspaceLayout& layout)
        {
            layout.groups.push_back(make_group(
                "epoch.workspace.documents", "Documents",
                DockRegion::document_tabs, DockRegion::central_document,
                TabCategory::document, 0u, 1.0f, 320u));
            layout.groups.push_back(make_group(
                "epoch.workspace.right.structure", "Structure",
                DockRegion::right_upper, DockRegion::right_upper,
                TabCategory::tool, 1u, 0.58f, 180u));
            layout.groups.push_back(make_group(
                "epoch.workspace.right.inspector", "Inspector",
                DockRegion::right_lower, DockRegion::right_lower,
                TabCategory::tool, 2u, 0.42f, 180u));
            layout.groups.push_back(make_group(
                "epoch.workspace.bottom.operations", "Operations",
                DockRegion::bottom, DockRegion::bottom,
                TabCategory::tool, 3u, 0.28f, 120u));
        }

        inline void add_bottom_tools(WorkspaceLayout& layout)
        {
            constexpr std::string_view group =
                "epoch.workspace.bottom.operations";
            add_tab(layout, "epoch.tab.log", "Log", group,
                "epoch.provider.log", TabCategory::tool,
                SurfaceKind::service_monitor, ContentRole::activity_log,
                VisibilityRule::always_available, CloseRule::hideable,
                0u, true, true);
            add_tab(layout, "epoch.tab.task_graph", "Task Graph", group,
                "epoch.provider.task_graph", TabCategory::tool,
                SurfaceKind::service_monitor, ContentRole::task_graph,
                VisibilityRule::provider_required, CloseRule::hideable,
                1u, true, false);
            add_tab(layout, "epoch.tab.systems", "Systems", group,
                "epoch.provider.systems", TabCategory::tool,
                SurfaceKind::service_monitor, ContentRole::systems_diagnostics,
                VisibilityRule::provider_required, CloseRule::hideable,
                2u, true, false);
            add_tab(layout, "epoch.tab.build", "Build", group,
                "epoch.provider.build", TabCategory::tool,
                SurfaceKind::service_monitor, ContentRole::build_pipeline,
                VisibilityRule::project_required, CloseRule::hideable,
                3u, true, false);
        }
    }

    [[nodiscard]] inline ValidationReport validate_layout(
        const WorkspaceLayout& layout) noexcept
    {
        if (!layout.limits.valid())
            return {ValidationCode::invalid_limits};
        if (layout.schema_version != WorkspaceLayout::current_schema_version)
            return {ValidationCode::invalid_schema};
        if (!layout.application_id.valid()
            || !detail::valid_application_kind(layout.application))
        {
            return {ValidationCode::invalid_application};
        }
        if (layout.renderer_binding
            != RendererBindingPolicy::external_view_host)
        {
            return {ValidationCode::invalid_renderer_binding};
        }
        if (layout.canonical_name.empty() || layout.label.empty()
            || layout.canonical_name.size() > layout.limits.maximum_name_bytes
            || layout.label.size() > layout.limits.maximum_name_bytes)
        {
            return {ValidationCode::empty_application_name};
        }
        if (layout.application_id
            != make_stable_id<ApplicationTag>(layout.canonical_name))
        {
            return {ValidationCode::invalid_application_identity};
        }
        if (layout.groups.size() > layout.limits.maximum_groups)
            return {ValidationCode::group_capacity_exceeded};
        if (layout.tabs.size() > layout.limits.maximum_tabs)
            return {ValidationCode::tab_capacity_exceeded};

        for (std::size_t index = 0u; index < layout.groups.size(); ++index)
        {
            const auto& group = layout.groups[index];
            if (!group.id.valid() || group.canonical_name.empty()
                || group.label.empty()
                || group.canonical_name.size() > layout.limits.maximum_name_bytes
                || group.label.size() > layout.limits.maximum_name_bytes
                || group.default_fraction <= 0.0f
                || group.default_fraction > 1.0f
                || group.minimum_extent == 0u)
            {
                return {ValidationCode::invalid_group, group.id, {}, index};
            }
            if (group.id
                != make_stable_id<DockGroupTag>(group.canonical_name))
            {
                return {
                    ValidationCode::invalid_group_identity,
                    group.id, {}, index};
            }
            if (!detail::valid_group_regions(group))
            {
                return {
                    ValidationCode::invalid_group_region,
                    group.id, {}, index};
            }
            for (std::size_t prior = 0u; prior < index; ++prior)
            {
                if (layout.groups[prior].id == group.id)
                    return {ValidationCode::duplicate_group_id, group.id, {}, index};
                if (layout.groups[prior].canonical_name == group.canonical_name)
                    return {ValidationCode::duplicate_group_name, group.id, {}, index};
                if (layout.groups[prior].order == group.order)
                    return {ValidationCode::duplicate_group_order, group.id, {}, index};
            }
        }

        constexpr DockRegion shellRegions[] = {
            DockRegion::document_tabs,
            DockRegion::central_document,
            DockRegion::right_upper,
            DockRegion::right_lower,
            DockRegion::bottom};
        for (const DockRegion region : shellRegions)
        {
            if (!detail::has_region(layout, region))
                return {ValidationCode::missing_shell_region};
        }

        for (std::size_t index = 0u; index < layout.tabs.size(); ++index)
        {
            const auto& tab = layout.tabs[index];
            if (!tab.id.valid() || !tab.group.valid() || !tab.provider.valid()
                || tab.canonical_name.empty() || tab.label.empty()
                || tab.canonical_name.size() > layout.limits.maximum_name_bytes
                || tab.label.size() > layout.limits.maximum_name_bytes)
            {
                return {ValidationCode::invalid_tab, tab.group, tab.id, index};
            }
            if (tab.id != make_stable_id<TabTag>(tab.canonical_name))
            {
                return {
                    ValidationCode::invalid_tab_identity,
                    tab.group, tab.id, index};
            }
            for (std::size_t prior = 0u; prior < index; ++prior)
            {
                if (layout.tabs[prior].id == tab.id)
                    return {ValidationCode::duplicate_tab_id, tab.group, tab.id, index};
                if (layout.tabs[prior].canonical_name == tab.canonical_name)
                    return {ValidationCode::duplicate_tab_name, tab.group, tab.id, index};
                if (layout.tabs[prior].group == tab.group
                    && layout.tabs[prior].order == tab.order)
                {
                    return {ValidationCode::duplicate_tab_order, tab.group, tab.id, index};
                }
            }
            const DockGroupDescriptor* group = layout.find_group(tab.group);
            if (!group)
                return {ValidationCode::missing_tab_group, tab.group, tab.id, index};
            if (group->accepted_category != tab.category)
                return {ValidationCode::category_mismatch, tab.group, tab.id, index};
            if (!detail::valid_surface(tab.category, tab.surface))
                return {ValidationCode::invalid_surface, tab.group, tab.id, index};
            if (tab.close_rule == CloseRule::fixed && !tab.default_open)
                return {ValidationCode::invalid_close_rule, tab.group, tab.id, index};
            if (tab.category == TabCategory::document
                && tab.close_rule == CloseRule::hideable)
            {
                return {ValidationCode::invalid_close_rule, tab.group, tab.id, index};
            }
            if (tab.category == TabCategory::tool
                && tab.close_rule == CloseRule::document_closeable)
            {
                return {ValidationCode::invalid_close_rule, tab.group, tab.id, index};
            }
            if (tab.default_active && !tab.default_open)
                return {ValidationCode::invalid_default_active, tab.group, tab.id, index};
        }

        for (const auto& group : layout.groups)
        {
            std::size_t openCount = 0u;
            std::size_t activeCount = 0u;
            for (const auto& tab : layout.tabs)
            {
                if (tab.group != group.id)
                    continue;
                openCount += tab.default_open ? 1u : 0u;
                activeCount += tab.default_active ? 1u : 0u;
            }
            if (openCount != 0u && activeCount == 0u)
                return {ValidationCode::missing_group_active_tab, group.id};
            if (activeCount > 1u)
                return {ValidationCode::multiple_group_active_tabs, group.id};
        }

        constexpr ContentRole commonRequired[] = {
            ContentRole::activity_log,
            ContentRole::task_graph,
            ContentRole::systems_diagnostics,
            ContentRole::build_pipeline};
        for (const ContentRole role : commonRequired)
        {
            if (!detail::has_required_content(layout, role))
                return {ValidationCode::missing_required_content};
        }

        const auto require = [&layout](ContentRole role)
        {
            return detail::has_required_content(layout, role);
        };
        switch (layout.application)
        {
        case ApplicationKind::standard_editor:
            if (!require(ContentRole::world_viewport)
                || !require(ContentRole::world_outliner)
                || !require(ContentRole::selection_properties)
                || !require(ContentRole::world_settings))
            {
                return {ValidationCode::missing_required_content};
            }
            break;
        case ApplicationKind::plant_lab:
            if (!require(ContentRole::plant_preview)
                || !require(ContentRole::plant_graph)
                || !require(ContentRole::plant_hierarchy)
                || !require(ContentRole::plant_properties)
                || !require(ContentRole::plant_growth_settings))
            {
                return {ValidationCode::missing_required_content};
            }
            break;
        case ApplicationKind::gui_editor:
            if (!require(ContentRole::gui_canvas)
                || !require(ContentRole::gui_runtime_preview)
                || !require(ContentRole::gui_hierarchy)
                || !require(ContentRole::gui_properties)
                || !require(ContentRole::gui_canvas_settings))
            {
                return {ValidationCode::missing_required_content};
            }
            break;
        }
        return {};
    }

    [[nodiscard]] inline WorkspaceLayout make_workspace_layout(
        ApplicationKind application)
    {
        WorkspaceLayout layout{};
        layout.application = application;
        detail::add_shell_groups(layout);

        constexpr std::string_view documents = "epoch.workspace.documents";
        constexpr std::string_view structure =
            "epoch.workspace.right.structure";
        constexpr std::string_view inspector =
            "epoch.workspace.right.inspector";

        switch (application)
        {
        case ApplicationKind::standard_editor:
            layout.application_id = make_stable_id<ApplicationTag>(
                "epoch.application.standard_editor");
            layout.canonical_name = "epoch.application.standard_editor";
            layout.label = "Standard Editor";
            detail::add_tab(layout, "epoch.tab.standard.world", "World",
                documents, "epoch.provider.scene_document",
                TabCategory::document, SurfaceKind::viewport_3d,
                ContentRole::world_viewport, VisibilityRule::project_required,
                CloseRule::document_closeable, 0u, true, true);
            detail::add_tab(layout, "epoch.tab.standard.scene_2d", "2D Scene",
                documents, "epoch.provider.canvas2d_document",
                TabCategory::document, SurfaceKind::canvas_2d,
                ContentRole::scene_2d, VisibilityRule::document_required,
                CloseRule::document_closeable, 1u, false, false);
            detail::add_tab(layout, "epoch.tab.standard.material", "Material",
                documents, "epoch.provider.material_document",
                TabCategory::document, SurfaceKind::node_graph,
                ContentRole::material_graph, VisibilityRule::document_required,
                CloseRule::document_closeable, 2u, false, false);
            detail::add_tab(layout, "epoch.tab.standard.texture", "Texture",
                documents, "epoch.provider.texture_document",
                TabCategory::document, SurfaceKind::document_editor,
                ContentRole::texture_document, VisibilityRule::document_required,
                CloseRule::document_closeable, 3u, false, false);
            detail::add_tab(layout, "epoch.tab.standard.script", "Script",
                documents, "epoch.provider.script_document",
                TabCategory::document, SurfaceKind::text_editor,
                ContentRole::script_document, VisibilityRule::document_required,
                CloseRule::document_closeable, 4u, false, false);
            detail::add_tab(layout, "epoch.tab.standard.forest_factory",
                "Forest Factory", documents,
                "epoch.provider.forest_factory_document",
                TabCategory::document, SurfaceKind::viewport_3d,
                ContentRole::forest_factory, VisibilityRule::provider_required,
                CloseRule::document_closeable, 5u, false, false);
            detail::add_tab(layout, "epoch.tab.standard.outliner", "Outliner",
                structure, "epoch.provider.scene_outliner", TabCategory::tool,
                SurfaceKind::hierarchy, ContentRole::world_outliner,
                VisibilityRule::project_required, CloseRule::hideable,
                0u, true, true);
            detail::add_tab(layout, "epoch.tab.standard.assets", "Assets",
                structure, "epoch.provider.asset_registry", TabCategory::tool,
                SurfaceKind::hierarchy, ContentRole::asset_browser,
                VisibilityRule::project_required, CloseRule::hideable,
                1u, true, false);
            detail::add_tab(layout, "epoch.tab.standard.scripts", "Scripts",
                structure, "epoch.provider.script_workspace", TabCategory::tool,
                SurfaceKind::hierarchy, ContentRole::script_browser,
                VisibilityRule::project_required, CloseRule::hideable,
                2u, false, false);
            detail::add_tab(layout, "epoch.tab.standard.properties", "Properties",
                inspector, "epoch.provider.selection_inspector", TabCategory::tool,
                SurfaceKind::inspector, ContentRole::selection_properties,
                VisibilityRule::project_required, CloseRule::hideable,
                0u, true, true);
            detail::add_tab(layout, "epoch.tab.standard.world_settings",
                "World Settings", inspector, "epoch.provider.world_settings",
                TabCategory::tool, SurfaceKind::inspector,
                ContentRole::world_settings, VisibilityRule::project_required,
                CloseRule::hideable, 1u, true, false);
            break;

        case ApplicationKind::plant_lab:
            layout.application_id = make_stable_id<ApplicationTag>(
                "epoch.application.plant_lab");
            layout.canonical_name = "epoch.application.plant_lab";
            layout.label = "Plant Lab";
            detail::add_tab(layout, "epoch.tab.plant.preview", "Plant Preview",
                documents, "epoch.provider.plant_preview",
                TabCategory::document, SurfaceKind::viewport_3d,
                ContentRole::plant_preview, VisibilityRule::project_required,
                CloseRule::document_closeable, 0u, true, true);
            detail::add_tab(layout, "epoch.tab.plant.graph", "Plant Graph",
                documents, "epoch.provider.plant_graph",
                TabCategory::document, SurfaceKind::node_graph,
                ContentRole::plant_graph, VisibilityRule::project_required,
                CloseRule::document_closeable, 1u, true, false);
            detail::add_tab(layout, "epoch.tab.plant.timeline", "Growth Timeline",
                documents, "epoch.provider.plant_timeline",
                TabCategory::document, SurfaceKind::timeline,
                ContentRole::plant_growth_timeline,
                VisibilityRule::project_required,
                CloseRule::document_closeable, 2u, true, false);
            detail::add_tab(layout, "epoch.tab.plant.species", "Species Library",
                documents, "epoch.provider.plant_species_library",
                TabCategory::document, SurfaceKind::document_editor,
                ContentRole::plant_species_library,
                VisibilityRule::project_required,
                CloseRule::document_closeable, 3u, false, false);
            detail::add_tab(layout, "epoch.tab.plant.hierarchy", "Plant Hierarchy",
                structure, "epoch.provider.plant_hierarchy", TabCategory::tool,
                SurfaceKind::hierarchy, ContentRole::plant_hierarchy,
                VisibilityRule::project_required, CloseRule::hideable,
                0u, true, true);
            detail::add_tab(layout, "epoch.tab.plant.assets", "Plant Assets",
                structure, "epoch.provider.plant_asset_registry", TabCategory::tool,
                SurfaceKind::hierarchy, ContentRole::plant_asset_browser,
                VisibilityRule::project_required, CloseRule::hideable,
                1u, true, false);
            detail::add_tab(layout, "epoch.tab.plant.properties", "Properties",
                inspector, "epoch.provider.plant_properties", TabCategory::tool,
                SurfaceKind::inspector, ContentRole::plant_properties,
                VisibilityRule::project_required, CloseRule::hideable,
                0u, true, true);
            detail::add_tab(layout, "epoch.tab.plant.growth_settings",
                "Growth Settings", inspector,
                "epoch.provider.plant_growth_settings", TabCategory::tool,
                SurfaceKind::inspector, ContentRole::plant_growth_settings,
                VisibilityRule::project_required, CloseRule::hideable,
                1u, true, false);
            break;

        case ApplicationKind::gui_editor:
            layout.application_id = make_stable_id<ApplicationTag>(
                "epoch.application.gui_editor");
            layout.canonical_name = "epoch.application.gui_editor";
            layout.label = "GUI Editor";
            detail::add_tab(layout, "epoch.tab.gui.canvas", "GUI Canvas",
                documents, "epoch.provider.gui_document",
                TabCategory::document, SurfaceKind::canvas_2d,
                ContentRole::gui_canvas, VisibilityRule::project_required,
                CloseRule::document_closeable, 0u, true, true);
            detail::add_tab(layout, "epoch.tab.gui.preview", "Runtime Preview",
                documents, "epoch.provider.gui_runtime_preview",
                TabCategory::document, SurfaceKind::viewport_3d,
                ContentRole::gui_runtime_preview,
                VisibilityRule::provider_required,
                CloseRule::document_closeable, 1u, true, false);
            detail::add_tab(layout, "epoch.tab.gui.graph", "Component Graph",
                documents, "epoch.provider.gui_component_graph",
                TabCategory::document, SurfaceKind::node_graph,
                ContentRole::gui_component_graph,
                VisibilityRule::document_required,
                CloseRule::document_closeable, 2u, false, false);
            detail::add_tab(layout, "epoch.tab.gui.styles", "Styles",
                documents, "epoch.provider.gui_style_document",
                TabCategory::document, SurfaceKind::document_editor,
                ContentRole::gui_style_document,
                VisibilityRule::document_required,
                CloseRule::document_closeable, 3u, false, false);
            detail::add_tab(layout, "epoch.tab.gui.hierarchy", "GUI Hierarchy",
                structure, "epoch.provider.gui_hierarchy", TabCategory::tool,
                SurfaceKind::hierarchy, ContentRole::gui_hierarchy,
                VisibilityRule::project_required, CloseRule::hideable,
                0u, true, true);
            detail::add_tab(layout, "epoch.tab.gui.assets", "GUI Assets",
                structure, "epoch.provider.gui_asset_registry", TabCategory::tool,
                SurfaceKind::hierarchy, ContentRole::gui_asset_browser,
                VisibilityRule::project_required, CloseRule::hideable,
                1u, true, false);
            detail::add_tab(layout, "epoch.tab.gui.properties", "Properties",
                inspector, "epoch.provider.gui_properties", TabCategory::tool,
                SurfaceKind::inspector, ContentRole::gui_properties,
                VisibilityRule::project_required, CloseRule::hideable,
                0u, true, true);
            detail::add_tab(layout, "epoch.tab.gui.canvas_settings",
                "Canvas Settings", inspector,
                "epoch.provider.gui_canvas_settings", TabCategory::tool,
                SurfaceKind::inspector, ContentRole::gui_canvas_settings,
                VisibilityRule::project_required, CloseRule::hideable,
                1u, true, false);
            break;
        }

        detail::add_bottom_tools(layout);
        return layout;
    }

    struct TabState final
    {
        TabId id{};
        DockGroupId group{};
        bool open{};
        bool active{};
    };

    struct WorkspaceState final
    {
        ApplicationId application{};
        std::vector<TabState> tabs{};
        std::uint64_t revision{1u};

        [[nodiscard]] const TabState* find(TabId id) const noexcept
        {
            const auto found = std::find_if(
                tabs.begin(), tabs.end(), [id](const auto& tab)
                {
                    return tab.id == id;
                });
            return found == tabs.end() ? nullptr : &*found;
        }

        [[nodiscard]] TabState* find(TabId id) noexcept
        {
            const auto found = std::find_if(
                tabs.begin(), tabs.end(), [id](const auto& tab)
                {
                    return tab.id == id;
                });
            return found == tabs.end() ? nullptr : &*found;
        }
    };

    enum class StateCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_layout,
        wrong_application,
        invalid_state,
        unknown_tab,
        tab_closed,
        close_forbidden,
        visibility_blocked,
        unknown_group,
        category_mismatch,
        move_forbidden
    };

    struct VisibilityContext final
    {
        bool project_open{true};
        bool document_open{true};
        bool provider_available{true};
    };

    [[nodiscard]] constexpr bool tab_available(
        const TabDescriptor& tab,
        const VisibilityContext& context) noexcept
    {
        switch (tab.visibility)
        {
        case VisibilityRule::always_available: return true;
        case VisibilityRule::project_required: return context.project_open;
        case VisibilityRule::document_required: return context.document_open;
        case VisibilityRule::provider_required:
            return context.provider_available;
        }
        return false;
    }

    struct StateResult final
    {
        StateCode code{StateCode::invalid_state};
        TabId tab{};
        std::uint64_t revision{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == StateCode::ready || code == StateCode::unchanged;
        }
    };

    [[nodiscard]] inline WorkspaceState make_default_state(
        const WorkspaceLayout& layout)
    {
        WorkspaceState state{};
        state.application = layout.application_id;
        state.tabs.reserve(layout.tabs.size());
        for (const auto& tab : layout.tabs)
        {
            state.tabs.push_back(TabState{
                .id = tab.id,
                .group = tab.group,
                .open = tab.default_open,
                .active = tab.default_active});
        }
        return state;
    }

    [[nodiscard]] inline bool validate_state(
        const WorkspaceLayout& layout,
        const WorkspaceState& state) noexcept
    {
        if (!validate_layout(layout) || state.application != layout.application_id
            || state.revision == 0u || state.tabs.size() != layout.tabs.size())
        {
            return false;
        }
        for (std::size_t index = 0u; index < state.tabs.size(); ++index)
        {
            const auto& item = state.tabs[index];
            const TabDescriptor* descriptor = layout.find_tab(item.id);
            const DockGroupDescriptor* group = layout.find_group(item.group);
            if (!descriptor || !group || (item.active && !item.open)
                || group->accepted_category != descriptor->category
                || (descriptor->category == TabCategory::document
                    && item.group != descriptor->group))
            {
                return false;
            }
            for (std::size_t prior = 0u; prior < index; ++prior)
            {
                if (state.tabs[prior].id == item.id)
                    return false;
            }
        }
        for (const auto& group : layout.groups)
        {
            std::size_t activeCount = 0u;
            for (const auto& item : state.tabs)
            {
                if (item.group == group.id && item.active)
                    ++activeCount;
            }
            if (activeCount > 1u)
                return false;
        }
        return true;
    }

    [[nodiscard]] inline StateResult activate_tab(
        const WorkspaceLayout& layout,
        WorkspaceState& state,
        TabId tabId) noexcept
    {
        if (!validate_layout(layout))
            return {StateCode::invalid_layout, tabId, state.revision};
        if (state.application != layout.application_id)
            return {StateCode::wrong_application, tabId, state.revision};
        if (!validate_state(layout, state))
            return {StateCode::invalid_state, tabId, state.revision};
        TabState* target = state.find(tabId);
        const TabDescriptor* descriptor = layout.find_tab(tabId);
        if (!target || !descriptor)
            return {StateCode::unknown_tab, tabId, state.revision};
        if (!target->open)
            return {StateCode::tab_closed, tabId, state.revision};
        if (target->active)
            return {StateCode::unchanged, tabId, state.revision};

        for (auto& item : state.tabs)
        {
            if (item.group == target->group)
                item.active = false;
        }
        target->active = true;
        ++state.revision;
        return {StateCode::ready, tabId, state.revision};
    }

    [[nodiscard]] inline StateResult set_tab_open(
        const WorkspaceLayout& layout,
        WorkspaceState& state,
        TabId tabId,
        bool open,
        VisibilityContext visibility = {}) noexcept
    {
        if (!validate_layout(layout))
            return {StateCode::invalid_layout, tabId, state.revision};
        if (state.application != layout.application_id)
            return {StateCode::wrong_application, tabId, state.revision};
        if (!validate_state(layout, state))
            return {StateCode::invalid_state, tabId, state.revision};
        TabState* target = state.find(tabId);
        const TabDescriptor* descriptor = layout.find_tab(tabId);
        if (!target || !descriptor)
            return {StateCode::unknown_tab, tabId, state.revision};
        if (target->open == open)
            return {StateCode::unchanged, tabId, state.revision};
        if (open && !tab_available(*descriptor, visibility))
            return {StateCode::visibility_blocked, tabId, state.revision};
        if (!open && descriptor->close_rule == CloseRule::fixed)
            return {StateCode::close_forbidden, tabId, state.revision};

        const bool wasActive = target->active;
        target->open = open;
        target->active = false;

        if (open)
        {
            bool groupHasActive = false;
            for (const auto& item : state.tabs)
            {
                groupHasActive = groupHasActive
                    || (item.group == target->group && item.active);
            }
            target->active = !groupHasActive;
        }
        else if (wasActive)
        {
            TabState* fallback = nullptr;
            std::uint16_t fallbackOrder = 0xffffu;
            for (auto& item : state.tabs)
            {
                const TabDescriptor* candidate = layout.find_tab(item.id);
                if (candidate && item.group == target->group
                    && item.open && candidate->order < fallbackOrder)
                {
                    fallback = &item;
                    fallbackOrder = candidate->order;
                }
            }
            if (fallback)
                fallback->active = true;
        }
        ++state.revision;
        return {StateCode::ready, tabId, state.revision};
    }

    [[nodiscard]] inline StateResult move_tool_tab(
        const WorkspaceLayout& layout,
        WorkspaceState& state,
        TabId tabId,
        DockGroupId targetGroupId) noexcept
    {
        if (!validate_layout(layout))
            return {StateCode::invalid_layout, tabId, state.revision};
        if (state.application != layout.application_id)
            return {StateCode::wrong_application, tabId, state.revision};
        if (!validate_state(layout, state))
            return {StateCode::invalid_state, tabId, state.revision};

        TabState* target = state.find(tabId);
        const TabDescriptor* descriptor = layout.find_tab(tabId);
        const DockGroupDescriptor* targetGroup =
            layout.find_group(targetGroupId);
        if (!target || !descriptor)
            return {StateCode::unknown_tab, tabId, state.revision};
        if (!targetGroup)
            return {StateCode::unknown_group, tabId, state.revision};
        if (descriptor->category != TabCategory::tool
            || !descriptor->detachable
            || !targetGroup->tabs_detachable)
        {
            return {StateCode::move_forbidden, tabId, state.revision};
        }
        if (targetGroup->accepted_category != descriptor->category)
            return {StateCode::category_mismatch, tabId, state.revision};
        if (target->group == targetGroupId)
            return {StateCode::unchanged, tabId, state.revision};

        const DockGroupId sourceGroup = target->group;
        const bool wasActive = target->active;
        for (auto& item : state.tabs)
        {
            if (item.group == targetGroupId)
                item.active = false;
        }

        target->group = targetGroupId;
        target->active = target->open;
        if (wasActive)
        {
            TabState* fallback = nullptr;
            std::uint16_t fallbackOrder = 0xffffu;
            for (auto& item : state.tabs)
            {
                const TabDescriptor* candidate = layout.find_tab(item.id);
                if (candidate && item.group == sourceGroup && item.open
                    && candidate->order < fallbackOrder)
                {
                    fallback = &item;
                    fallbackOrder = candidate->order;
                }
            }
            if (fallback)
                fallback->active = true;
        }

        ++state.revision;
        return {StateCode::ready, tabId, state.revision};
    }

    enum class ContractFailure : std::uint8_t
    {
        none,
        stable_ids,
        standard_layout,
        plant_layout,
        gui_layout,
        shell_topology,
        application_content,
        renderer_separation,
        duplicate_group_rejection,
        duplicate_tab_rejection,
        region_rejection,
        close_rule_rejection,
        fixed_close_enforcement,
        visibility_enforcement,
        default_state,
        activation,
        close_and_fallback,
        reopen,
        tool_move,
        document_move_rejection,
        wrong_application
    };

    [[nodiscard]] constexpr std::string_view contract_failure_name(
        ContractFailure failure) noexcept
    {
        switch (failure)
        {
        case ContractFailure::none: return "pass";
        case ContractFailure::stable_ids: return "stable_ids";
        case ContractFailure::standard_layout: return "standard_layout";
        case ContractFailure::plant_layout: return "plant_layout";
        case ContractFailure::gui_layout: return "gui_layout";
        case ContractFailure::shell_topology: return "shell_topology";
        case ContractFailure::application_content: return "application_content";
        case ContractFailure::renderer_separation: return "renderer_separation";
        case ContractFailure::duplicate_group_rejection:
            return "duplicate_group_rejection";
        case ContractFailure::duplicate_tab_rejection:
            return "duplicate_tab_rejection";
        case ContractFailure::region_rejection: return "region_rejection";
        case ContractFailure::close_rule_rejection:
            return "close_rule_rejection";
        case ContractFailure::fixed_close_enforcement:
            return "fixed_close_enforcement";
        case ContractFailure::visibility_enforcement:
            return "visibility_enforcement";
        case ContractFailure::default_state: return "default_state";
        case ContractFailure::activation: return "activation";
        case ContractFailure::close_and_fallback: return "close_and_fallback";
        case ContractFailure::reopen: return "reopen";
        case ContractFailure::tool_move: return "tool_move";
        case ContractFailure::document_move_rejection:
            return "document_move_rejection";
        case ContractFailure::wrong_application: return "wrong_application";
        }
        return "unknown";
    }

    [[nodiscard]] ContractFailure run_contract()
    {
        constexpr TabId stableA = make_stable_id<TabTag>(
            "epoch.contract.stable_tab");
        constexpr TabId stableB = make_stable_id<TabTag>(
            "epoch.contract.stable_tab");
        constexpr TabId different = make_stable_id<TabTag>(
            "epoch.contract.other_tab");
        if (!stableA.valid() || stableA != stableB || stableA == different)
            return ContractFailure::stable_ids;

        const WorkspaceLayout standard = make_workspace_layout(
            ApplicationKind::standard_editor);
        const WorkspaceLayout plant = make_workspace_layout(
            ApplicationKind::plant_lab);
        const WorkspaceLayout gui = make_workspace_layout(
            ApplicationKind::gui_editor);
        if (!validate_layout(standard))
            return ContractFailure::standard_layout;
        if (!validate_layout(plant))
            return ContractFailure::plant_layout;
        if (!validate_layout(gui))
            return ContractFailure::gui_layout;

        const DockGroupDescriptor* documents = standard.find_group(
            make_stable_id<DockGroupTag>("epoch.workspace.documents"));
        const DockGroupDescriptor* structure = standard.find_group(
            make_stable_id<DockGroupTag>(
                "epoch.workspace.right.structure"));
        const DockGroupDescriptor* inspector = standard.find_group(
            make_stable_id<DockGroupTag>(
                "epoch.workspace.right.inspector"));
        const DockGroupDescriptor* operations = standard.find_group(
            make_stable_id<DockGroupTag>(
                "epoch.workspace.bottom.operations"));
        if (!documents
            || documents->tab_region != DockRegion::document_tabs
            || documents->content_region != DockRegion::central_document
            || !structure
            || structure->content_region != DockRegion::right_upper
            || !inspector
            || inspector->content_region != DockRegion::right_lower
            || !operations
            || operations->content_region != DockRegion::bottom)
        {
            return ContractFailure::shell_topology;
        }

        constexpr ContentRole commonRoles[] = {
            ContentRole::activity_log,
            ContentRole::task_graph,
            ContentRole::systems_diagnostics,
            ContentRole::build_pipeline};
        for (const ContentRole role : commonRoles)
        {
            if (!standard.find_content(role)
                || !plant.find_content(role)
                || !gui.find_content(role))
            {
                return ContractFailure::shell_topology;
            }
        }
        if (!standard.find_content(ContentRole::world_viewport)
            || !standard.find_content(ContentRole::world_outliner)
            || !standard.find_content(ContentRole::selection_properties)
            || !standard.find_content(ContentRole::world_settings)
            || !plant.find_content(ContentRole::plant_preview)
            || !plant.find_content(ContentRole::plant_graph)
            || !plant.find_content(ContentRole::plant_hierarchy)
            || !plant.find_content(ContentRole::plant_growth_settings)
            || !gui.find_content(ContentRole::gui_canvas)
            || !gui.find_content(ContentRole::gui_runtime_preview)
            || !gui.find_content(ContentRole::gui_hierarchy)
            || !gui.find_content(ContentRole::gui_canvas_settings))
        {
            return ContractFailure::application_content;
        }
        if (standard.renderer_binding
                != RendererBindingPolicy::external_view_host
            || plant.renderer_binding
                != RendererBindingPolicy::external_view_host
            || gui.renderer_binding
                != RendererBindingPolicy::external_view_host)
        {
            return ContractFailure::renderer_separation;
        }

        WorkspaceLayout duplicateGroup = standard;
        duplicateGroup.groups.push_back(duplicateGroup.groups.front());
        if (validate_layout(duplicateGroup).code
            != ValidationCode::duplicate_group_id)
        {
            return ContractFailure::duplicate_group_rejection;
        }
        WorkspaceLayout duplicateTab = standard;
        duplicateTab.tabs.push_back(duplicateTab.tabs.front());
        if (validate_layout(duplicateTab).code
            != ValidationCode::duplicate_tab_id)
        {
            return ContractFailure::duplicate_tab_rejection;
        }
        WorkspaceLayout invalidRegion = standard;
        invalidRegion.groups.front().content_region = DockRegion::right_upper;
        if (validate_layout(invalidRegion).code
            != ValidationCode::invalid_group_region)
        {
            return ContractFailure::region_rejection;
        }
        WorkspaceLayout invalidCloseRule = standard;
        invalidCloseRule.tabs.front().close_rule = CloseRule::fixed;
        invalidCloseRule.tabs.front().default_open = false;
        invalidCloseRule.tabs.front().default_active = false;
        if (validate_layout(invalidCloseRule).code
            != ValidationCode::invalid_close_rule)
        {
            return ContractFailure::close_rule_rejection;
        }

        WorkspaceLayout fixedLayout = standard;
        fixedLayout.tabs.front().close_rule = CloseRule::fixed;
        WorkspaceState fixedState = make_default_state(fixedLayout);
        if (!validate_layout(fixedLayout)
            || set_tab_open(
                fixedLayout,
                fixedState,
                fixedLayout.tabs.front().id,
                false).code != StateCode::close_forbidden)
        {
            return ContractFailure::fixed_close_enforcement;
        }

        WorkspaceState state = make_default_state(standard);
        if (!validate_state(standard, state)
            || state.application != standard.application_id
            || state.revision != 1u)
        {
            return ContractFailure::default_state;
        }
        const TabDescriptor* assets =
            standard.find_content(ContentRole::asset_browser);
        const TabDescriptor* outliner =
            standard.find_content(ContentRole::world_outliner);
        const TabDescriptor* scripts =
            standard.find_content(ContentRole::script_browser);
        if (!assets || !outliner || !scripts)
            return ContractFailure::application_content;

        const StateResult blockedScripts = set_tab_open(
            standard,
            state,
            scripts->id,
            true,
            VisibilityContext{
                .project_open = false,
                .document_open = false,
                .provider_available = false});
        const TabState* scriptState = state.find(scripts->id);
        if (blockedScripts.code != StateCode::visibility_blocked
            || !scriptState || scriptState->open)
        {
            return ContractFailure::visibility_enforcement;
        }

        if (!activate_tab(standard, state, assets->id))
            return ContractFailure::activation;
        const TabState* assetState = state.find(assets->id);
        const TabState* outlinerState = state.find(outliner->id);
        if (!assetState || !assetState->active
            || !outlinerState || outlinerState->active)
        {
            return ContractFailure::activation;
        }
        if (!set_tab_open(standard, state, assets->id, false))
            return ContractFailure::close_and_fallback;
        assetState = state.find(assets->id);
        outlinerState = state.find(outliner->id);
        if (!assetState || assetState->open || assetState->active
            || !outlinerState || !outlinerState->active
            || !validate_state(standard, state))
        {
            return ContractFailure::close_and_fallback;
        }
        if (!set_tab_open(standard, state, scripts->id, true)
            || !activate_tab(standard, state, scripts->id))
        {
            return ContractFailure::reopen;
        }
        scriptState = state.find(scripts->id);
        if (!scriptState || !scriptState->open || !scriptState->active
            || !validate_state(standard, state))
        {
            return ContractFailure::reopen;
        }

        const DockGroupDescriptor* inspectorTarget =
            standard.find_group(make_stable_id<DockGroupTag>(
                "epoch.workspace.right.inspector"));
        if (!inspectorTarget
            || !move_tool_tab(
                standard, state, scripts->id, inspectorTarget->id))
        {
            return ContractFailure::tool_move;
        }
        scriptState = state.find(scripts->id);
        if (!scriptState || scriptState->group != inspectorTarget->id
            || !scriptState->active || !validate_state(standard, state))
        {
            return ContractFailure::tool_move;
        }

        const TabDescriptor* worldDocument =
            standard.find_content(ContentRole::world_viewport);
        if (!worldDocument
            || move_tool_tab(
                standard, state, worldDocument->id, inspectorTarget->id).code
                != StateCode::move_forbidden)
        {
            return ContractFailure::document_move_rejection;
        }

        WorkspaceState wrongApplication = make_default_state(standard);
        const TabDescriptor* plantPreview =
            plant.find_content(ContentRole::plant_preview);
        if (!plantPreview
            || activate_tab(
                plant, wrongApplication, plantPreview->id).code
                != StateCode::wrong_application)
        {
            return ContractFailure::wrong_application;
        }
        return ContractFailure::none;
    }
}
