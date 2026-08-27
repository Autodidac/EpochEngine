/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

module authoring.gui_document;

namespace epochengine::authoring::gui
{
    namespace
    {
        struct TemplateWidgetSpec final
        {
            WidgetKind kind{};
            std::string_view name{};
            std::string_view label{};
            float x{};
            float y{};
            float width{};
            float height{};
            std::string_view action{};
            double value{};
            bool accent{};
        };

        [[nodiscard]] std::span<const TemplateWidgetSpec> template_widgets(
            TemplatePreset preset) noexcept
        {
            static constexpr std::array desktop{
                TemplateWidgetSpec{WidgetKind::panel, "ApplicationHeader", "Application Header", 0.0f, 0.0f, 1'280.0f, 72.0f},
                TemplateWidgetSpec{WidgetKind::text, "ApplicationTitle", "New Epoch Application", 28.0f, 20.0f, 360.0f, 34.0f},
                TemplateWidgetSpec{WidgetKind::panel, "Navigation", "Navigation", 0.0f, 72.0f, 236.0f, 648.0f},
                TemplateWidgetSpec{WidgetKind::button, "NavigationHome", "Home", 20.0f, 112.0f, 196.0f, 42.0f, "navigate.home"},
                TemplateWidgetSpec{WidgetKind::button, "NavigationSettings", "Settings", 20.0f, 164.0f, 196.0f, 42.0f, "navigate.settings"},
                TemplateWidgetSpec{WidgetKind::panel, "ContentSurface", "Content", 256.0f, 96.0f, 996.0f, 596.0f},
                TemplateWidgetSpec{WidgetKind::text, "ContentHeading", "Workspace", 292.0f, 132.0f, 520.0f, 46.0f},
                TemplateWidgetSpec{WidgetKind::button, "PrimaryAction", "Create", 1'044.0f, 124.0f, 168.0f, 44.0f, "document.create", 0.0, true}
            };
            static constexpr std::array dashboard{
                TemplateWidgetSpec{WidgetKind::panel, "DashboardHeader", "Dashboard Header", 0.0f, 0.0f, 1'280.0f, 68.0f},
                TemplateWidgetSpec{WidgetKind::text, "DashboardTitle", "Operations Dashboard", 28.0f, 18.0f, 420.0f, 36.0f},
                TemplateWidgetSpec{WidgetKind::panel, "DashboardNavigation", "Navigation", 0.0f, 68.0f, 212.0f, 652.0f},
                TemplateWidgetSpec{WidgetKind::panel, "MetricCardA", "Primary Metric", 240.0f, 100.0f, 300.0f, 150.0f},
                TemplateWidgetSpec{WidgetKind::text, "MetricValueA", "1,248", 268.0f, 152.0f, 220.0f, 54.0f},
                TemplateWidgetSpec{WidgetKind::panel, "MetricCardB", "Secondary Metric", 568.0f, 100.0f, 300.0f, 150.0f},
                TemplateWidgetSpec{WidgetKind::text, "MetricValueB", "98.4%", 596.0f, 152.0f, 220.0f, 54.0f},
                TemplateWidgetSpec{WidgetKind::panel, "MetricCardC", "Current Load", 896.0f, 100.0f, 328.0f, 150.0f},
                TemplateWidgetSpec{WidgetKind::slider, "CurrentLoad", "Load", 924.0f, 164.0f, 272.0f, 32.0f, {}, 64.0, true},
                TemplateWidgetSpec{WidgetKind::panel, "ActivityFeed", "Recent Activity", 240.0f, 278.0f, 984.0f, 410.0f}
            };
            static constexpr std::array mobile{
                TemplateWidgetSpec{WidgetKind::panel, "MobileHeader", "Application", 0.0f, 0.0f, 390.0f, 72.0f},
                TemplateWidgetSpec{WidgetKind::text, "MobileTitle", "New Mobile App", 20.0f, 22.0f, 250.0f, 34.0f},
                TemplateWidgetSpec{WidgetKind::panel, "MobileContent", "Content", 16.0f, 92.0f, 358.0f, 646.0f},
                TemplateWidgetSpec{WidgetKind::button, "MobilePrimaryAction", "Continue", 32.0f, 664.0f, 326.0f, 52.0f, "navigation.continue", 0.0, true},
                TemplateWidgetSpec{WidgetKind::panel, "MobileNavigation", "Navigation", 0.0f, 764.0f, 390.0f, 80.0f},
                TemplateWidgetSpec{WidgetKind::button, "MobileHome", "Home", 18.0f, 780.0f, 104.0f, 44.0f, "navigate.home"},
                TemplateWidgetSpec{WidgetKind::button, "MobileSearch", "Search", 143.0f, 780.0f, 104.0f, 44.0f, "navigate.search"},
                TemplateWidgetSpec{WidgetKind::button, "MobileProfile", "Profile", 268.0f, 780.0f, 104.0f, 44.0f, "navigate.profile"}
            };
            static constexpr std::array hud{
                TemplateWidgetSpec{WidgetKind::panel, "PlayerStatus", "Player Status", 24.0f, 24.0f, 360.0f, 116.0f},
                TemplateWidgetSpec{WidgetKind::slider, "Health", "Health", 48.0f, 58.0f, 312.0f, 28.0f, {}, 100.0, true},
                TemplateWidgetSpec{WidgetKind::slider, "Energy", "Energy", 48.0f, 96.0f, 312.0f, 24.0f, {}, 72.0},
                TemplateWidgetSpec{WidgetKind::panel, "ObjectivePanel", "Current Objective", 440.0f, 24.0f, 400.0f, 86.0f},
                TemplateWidgetSpec{WidgetKind::text, "ObjectiveText", "Reach the marked destination", 468.0f, 54.0f, 344.0f, 34.0f},
                TemplateWidgetSpec{WidgetKind::panel, "ActionBar", "Actions", 424.0f, 620.0f, 432.0f, 76.0f},
                TemplateWidgetSpec{WidgetKind::button, "ActionPrimary", "Primary", 444.0f, 636.0f, 120.0f, 44.0f, "interact", 0.0, true},
                TemplateWidgetSpec{WidgetKind::button, "ActionSecondary", "Secondary", 580.0f, 636.0f, 120.0f, 44.0f, "jump"},
                TemplateWidgetSpec{WidgetKind::button, "Pause", "Pause", 716.0f, 636.0f, 120.0f, 44.0f, "pause"}
            };

            switch (preset)
            {
            case TemplatePreset::desktop_app: return desktop;
            case TemplatePreset::dashboard: return dashboard;
            case TemplatePreset::mobile_app: return mobile;
            case TemplatePreset::game_hud: return hud;
            case TemplatePreset::blank_canvas:
            default:
                return {};
            }
        }
    }

    std::optional<GuiDocument> make_template_document(
        TemplatePreset preset) noexcept
    {
        try
        {
            GuiDocument document{
                DocumentHandle{
                    static_cast<std::uint32_t>(preset) + 1u,
                    1u},
                BranchIdentity{1u, 1u}};
            if (!document.valid())
                return std::nullopt;

            const bool mobile = preset == TemplatePreset::mobile_app;
            const float canvasWidth = mobile ? 390.0f : 1'280.0f;
            const float canvasHeight = mobile ? 844.0f : 720.0f;
            WidgetDescriptor canvas{};
            canvas.kind = WidgetKind::canvas;
            canvas.name = "MainCanvas";
            canvas.layout.width = canvasWidth;
            canvas.layout.height = canvasHeight;
            canvas.content = CanvasContent{
                canvasWidth,
                canvasHeight,
                true};
            const MutationResult rootCreated =
                document.create_widget(std::move(canvas));
            if (!rootCreated)
                return std::nullopt;

            const StyleDescriptor surface{
                .background = {0.16f, 0.18f, 0.22f, 1.0f},
                .foreground = {0.94f, 0.95f, 0.97f, 1.0f},
                .border = {0.30f, 0.33f, 0.38f, 1.0f},
                .accent = {0.28f, 0.72f, 0.52f, 1.0f},
                .border_width = 1.0f,
                .corner_radius = 4.0f,
                .opacity = 1.0f};
            const StyleDescriptor accent{
                .background = {0.18f, 0.52f, 0.82f, 1.0f},
                .foreground = {1.0f, 1.0f, 1.0f, 1.0f},
                .border = {0.36f, 0.64f, 0.84f, 1.0f},
                .accent = {0.36f, 0.78f, 1.0f, 1.0f},
                .border_width = 1.0f,
                .corner_radius = 6.0f,
                .opacity = 1.0f};

            for (const TemplateWidgetSpec& spec : template_widgets(preset))
            {
                WidgetDescriptor descriptor{};
                descriptor.kind = spec.kind;
                descriptor.name = std::string{spec.name};
                descriptor.layout = {
                    .flow = LayoutFlow::absolute,
                    .width_rule = SizeRule::fixed,
                    .height_rule = SizeRule::fixed,
                    .x = spec.x,
                    .y = spec.y,
                    .width = spec.width,
                    .height = spec.height,
                    .minimum_width = 16.0f,
                    .minimum_height = 16.0f};
                descriptor.style = spec.accent ? accent : surface;
                switch (spec.kind)
                {
                case WidgetKind::panel:
                    descriptor.content = PanelContent{std::string{spec.label}};
                    descriptor.interaction.accepts_pointer = true;
                    break;
                case WidgetKind::button:
                    descriptor.content = ButtonContent{std::string{spec.label}};
                    descriptor.interaction.focusable = true;
                    descriptor.interaction.accepts_pointer = true;
                    descriptor.interaction.action = std::string{spec.action};
                    break;
                case WidgetKind::text:
                    descriptor.content = TextContent{
                        std::string{spec.label}, true};
                    break;
                case WidgetKind::slider:
                    descriptor.content = SliderContent{
                        std::string{spec.label},
                        0.0,
                        100.0,
                        spec.value,
                        1.0};
                    descriptor.interaction.focusable = true;
                    descriptor.interaction.accepts_pointer = true;
                    break;
                default:
                    return std::nullopt;
                }

                if (!document.create_widget(
                        std::move(descriptor), rootCreated.widget))
                {
                    return std::nullopt;
                }
            }

            if (document.validate())
                return std::nullopt;
            return document;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }
}
