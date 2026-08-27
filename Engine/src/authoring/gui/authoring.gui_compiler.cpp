/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

module authoring.gui_compiler;
import authoring.document;

namespace epochengine::authoring::gui
{
    namespace
    {
        [[nodiscard]] constexpr std::uint64_t widget_key(
            WidgetHandle handle) noexcept
        {
            return (static_cast<std::uint64_t>(handle.generation) << 32u)
                | static_cast<std::uint64_t>(handle.index);
        }

        [[nodiscard]] constexpr asset::gui::ContentHash content_hash(
            const authoring::ContentHash& source) noexcept
        {
            asset::gui::ContentHash result{};
            for (std::size_t word = 0u; word < source.words.size(); ++word)
            {
                for (std::size_t byte = 0u; byte < 8u; ++byte)
                {
                    result.bytes[word * 8u + byte] =
                        static_cast<std::uint8_t>(
                            source.words[word] >> (byte * 8u));
                }
            }
            return result;
        }

        [[nodiscard]] constexpr asset::gui::WidgetKind widget_kind(
            WidgetKind value) noexcept
        {
            switch (value)
            {
            case WidgetKind::canvas: return asset::gui::WidgetKind::canvas;
            case WidgetKind::panel: return asset::gui::WidgetKind::panel;
            case WidgetKind::button: return asset::gui::WidgetKind::button;
            case WidgetKind::text: return asset::gui::WidgetKind::text;
            case WidgetKind::image: return asset::gui::WidgetKind::image;
            case WidgetKind::image_button:
                return asset::gui::WidgetKind::image_button;
            case WidgetKind::text_input:
                return asset::gui::WidgetKind::text_input;
            case WidgetKind::slider: return asset::gui::WidgetKind::slider;
            case WidgetKind::scroll_area:
                return asset::gui::WidgetKind::scroll_area;
            case WidgetKind::tab_set: return asset::gui::WidgetKind::tab_set;
            case WidgetKind::tab_page: return asset::gui::WidgetKind::tab_page;
            }
            return asset::gui::WidgetKind::panel;
        }

        [[nodiscard]] constexpr asset::gui::LayoutFlow layout_flow(
            LayoutFlow value) noexcept
        {
            switch (value)
            {
            case LayoutFlow::absolute: return asset::gui::LayoutFlow::absolute;
            case LayoutFlow::horizontal:
                return asset::gui::LayoutFlow::horizontal;
            case LayoutFlow::vertical: return asset::gui::LayoutFlow::vertical;
            case LayoutFlow::overlay: return asset::gui::LayoutFlow::overlay;
            }
            return asset::gui::LayoutFlow::absolute;
        }

        [[nodiscard]] constexpr asset::gui::SizeRule size_rule(
            SizeRule value) noexcept
        {
            switch (value)
            {
            case SizeRule::fixed: return asset::gui::SizeRule::fixed;
            case SizeRule::content: return asset::gui::SizeRule::content;
            case SizeRule::fill: return asset::gui::SizeRule::fill;
            }
            return asset::gui::SizeRule::fixed;
        }

        [[nodiscard]] asset::gui::Layout layout(
            const LayoutDescriptor& source) noexcept
        {
            return {
                .flow = layout_flow(source.flow),
                .width_rule = size_rule(source.width_rule),
                .height_rule = size_rule(source.height_rule),
                .x = source.x,
                .y = source.y,
                .width = source.width,
                .height = source.height,
                .minimum_width = source.minimum_width,
                .minimum_height = source.minimum_height,
                .maximum_width = source.maximum_width,
                .maximum_height = source.maximum_height,
                .flex_grow = source.flex_grow,
                .spacing = source.spacing,
                .padding = {
                    source.padding.left,
                    source.padding.top,
                    source.padding.right,
                    source.padding.bottom},
                .clip_children = source.clip_children};
        }

        [[nodiscard]] constexpr asset::gui::Color color(Color source) noexcept
        {
            return {source.red, source.green, source.blue, source.alpha};
        }

        [[nodiscard]] asset::gui::Style style(const StyleDescriptor& source)
        {
            return {
                .background = color(source.background),
                .foreground = color(source.foreground),
                .border = color(source.border),
                .accent = color(source.accent),
                .border_width = source.border_width,
                .corner_radius = source.corner_radius,
                .opacity = source.opacity,
                .style_class = source.style_class};
        }

        void compile_content(
            const WidgetContent& source,
            asset::gui::CompiledWidget& destination,
            std::uint32_t& referencedAssets,
            const std::unordered_map<std::uint64_t, std::uint32_t>& indices,
            bool& unresolved)
        {
            if (const auto* value = std::get_if<CanvasContent>(&source))
            {
                destination.maximum = value->virtual_width;
                destination.value = value->virtual_height;
                destination.wrap = value->pixel_snap;
            }
            else if (const auto* value = std::get_if<PanelContent>(&source))
            {
                destination.primary_text = value->title;
            }
            else if (const auto* value = std::get_if<ButtonContent>(&source))
            {
                destination.primary_text = value->label;
            }
            else if (const auto* value = std::get_if<TextContent>(&source))
            {
                destination.primary_text = value->text;
                destination.wrap = value->wrap;
            }
            else if (const auto* value = std::get_if<ImageContent>(&source))
            {
                destination.asset_path = value->asset_path;
                destination.secondary_text = value->alternative_text;
                destination.preserve_aspect = value->preserve_aspect;
                if (!value->asset_path.empty())
                    ++referencedAssets;
            }
            else if (const auto* value = std::get_if<ImageButtonContent>(&source))
            {
                destination.asset_path = value->asset_path;
                destination.primary_text = value->label;
                destination.preserve_aspect = value->preserve_aspect;
                if (!value->asset_path.empty())
                    ++referencedAssets;
            }
            else if (const auto* value = std::get_if<TextInputContent>(&source))
            {
                destination.primary_text = value->text;
                destination.secondary_text = value->placeholder;
                destination.maximum_length = value->maximum_length;
                destination.multiline = value->multiline;
                destination.read_only = value->read_only;
            }
            else if (const auto* value = std::get_if<SliderContent>(&source))
            {
                destination.primary_text = value->label;
                destination.minimum = value->minimum;
                destination.maximum = value->maximum;
                destination.value = value->value;
                destination.step = value->step;
            }
            else if (const auto* value = std::get_if<ScrollAreaContent>(&source))
            {
                destination.horizontal_offset = value->horizontal_offset;
                destination.vertical_offset = value->vertical_offset;
                destination.allow_horizontal = value->allow_horizontal;
                destination.allow_vertical = value->allow_vertical;
            }
            else if (const auto* value = std::get_if<TabSetContent>(&source))
            {
                if (value->selected_page)
                {
                    const auto found = indices.find(widget_key(value->selected_page));
                    if (found == indices.end())
                        unresolved = true;
                    else
                        destination.selected_child = found->second;
                }
            }
            else if (const auto* value = std::get_if<TabPageContent>(&source))
            {
                destination.primary_text = value->label;
                destination.closeable = value->closeable;
            }
        }
    }

    CompileResult compile_gui_document(
        const GuiDocumentSnapshot& snapshot,
        const DocumentLimits& documentLimits,
        const asset::gui::ArtifactLimits& artifactLimits) noexcept
    {
        CompileResult result{};
        result.diagnostics.source_slots = static_cast<std::uint32_t>(
            (std::min)(snapshot.slots.size(),
                static_cast<std::size_t>(
                    (std::numeric_limits<std::uint32_t>::max)())));
        if (!artifactLimits.valid()
            || snapshot.slots.size() > artifactLimits.maximum_widgets)
        {
            result.code = CompileCode::capacity_exceeded;
            return result;
        }

        try
        {
            GuiDocument document{snapshot, documentLimits};
            if (!document.valid() || document.snapshot() != snapshot)
                return result;
            const auto roots = document.roots();
            if (roots.size() != 1u)
            {
                result.code = CompileCode::invalid_root;
                return result;
            }
            const auto rootView = document.widget(roots.front());
            if (!rootView || rootView->descriptor.kind != WidgetKind::canvas)
            {
                result.code = CompileCode::invalid_root;
                return result;
            }

            std::unordered_map<std::uint64_t, std::uint32_t> indices{};
            indices.reserve(snapshot.slots.size());
            result.artifact.widgets.reserve(snapshot.slots.size());
            for (std::size_t slotIndex = 0u; slotIndex < snapshot.slots.size(); ++slotIndex)
            {
                const auto& slot = snapshot.slots[slotIndex];
                if (!slot.widget)
                    continue;
                const WidgetHandle handle{
                    static_cast<std::uint32_t>(slotIndex), slot.generation};
                const auto compiledIndex = static_cast<std::uint32_t>(
                    result.artifact.widgets.size());
                if (!indices.emplace(widget_key(handle), compiledIndex).second)
                {
                    result.code = CompileCode::unresolved_reference;
                    return result;
                }
                result.artifact.widgets.push_back({});
                result.artifact.widgets.back().id = {widget_key(handle)};
            }

            const auto rootIndex = indices.find(widget_key(roots.front()));
            if (rootIndex == indices.end())
            {
                result.code = CompileCode::invalid_root;
                return result;
            }
            result.artifact.root = rootIndex->second;
            result.artifact.name = rootView->descriptor.name;
            const auto& canvas = std::get<CanvasContent>(
                rootView->descriptor.content);
            result.artifact.virtual_width = canvas.virtual_width;
            result.artifact.virtual_height = canvas.virtual_height;
            result.artifact.pixel_snap = canvas.pixel_snap;
            result.artifact.identity.source_revision = {
                content_hash(snapshot.revision.content),
                snapshot.revision.sequence};

            std::size_t compiledIndex{};
            bool unresolved{};
            for (std::size_t slotIndex = 0u; slotIndex < snapshot.slots.size(); ++slotIndex)
            {
                const auto& slot = snapshot.slots[slotIndex];
                if (!slot.widget)
                    continue;
                const auto& source = *slot.widget;
                auto& destination = result.artifact.widgets[compiledIndex++];
                destination.kind = widget_kind(source.descriptor.kind);
                destination.name = source.descriptor.name;
                destination.layout = layout(source.descriptor.layout);
                destination.style = style(source.descriptor.style);
                destination.enabled = source.descriptor.interaction.enabled;
                destination.visible = source.descriptor.interaction.visible;
                destination.focusable = source.descriptor.interaction.focusable;
                destination.accepts_pointer =
                    source.descriptor.interaction.accepts_pointer;
                destination.tab_index = source.descriptor.interaction.tab_index;
                destination.action = source.descriptor.interaction.action;
                destination.tooltip = source.descriptor.interaction.tooltip;
                if (source.parent)
                {
                    const auto found = indices.find(widget_key(source.parent));
                    if (found == indices.end())
                        unresolved = true;
                    else
                        destination.parent = found->second;
                }
                destination.children.reserve(source.children.size());
                for (const auto child : source.children)
                {
                    const auto found = indices.find(widget_key(child));
                    if (found == indices.end())
                        unresolved = true;
                    else
                        destination.children.push_back(found->second);
                }
                compile_content(
                    source.descriptor.content,
                    destination,
                    result.diagnostics.referenced_assets,
                    indices,
                    unresolved);
            }
            if (unresolved)
            {
                result.code = CompileCode::unresolved_reference;
                return result;
            }

            result.artifact.identity.key = asset::gui::payload_content(
                result.artifact);
            const auto validation = asset::gui::validate(
                result.artifact, artifactLimits);
            result.diagnostics.artifact_code = validation.code;
            if (!validation)
            {
                result.code = CompileCode::artifact_rejected;
                return result;
            }
            result.diagnostics.emitted_widgets = static_cast<std::uint32_t>(
                result.artifact.widgets.size());
            result.code = CompileCode::ready;
            return result;
        }
        catch (...)
        {
            result.code = CompileCode::allocation_failure;
            return result;
        }
    }
}
