/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module project.gui_epochgui;

import asset.gui_artifact;
import asset.texture_artifact;
import project.texture_library;

namespace epochengine::project_gui
{
    namespace
    {
        [[nodiscard]] gui::Vec2 point(
            project_gui_runtime::Rect rect) noexcept
        {
            return {rect.x, rect.y};
        }

        [[nodiscard]] gui::Vec2 extent(
            project_gui_runtime::Rect rect) noexcept
        {
            return {
                (std::max)(1.0f, rect.width),
                (std::max)(1.0f, rect.height)};
        }

        [[nodiscard]] std::string widget_id(
            asset::gui::WidgetId id,
            std::string_view role)
        {
            return std::string{"project-gui-"} + std::to_string(id.value)
                + "-" + std::string{role};
        }

        [[nodiscard]] bool container(asset::gui::WidgetKind kind) noexcept
        {
            return kind == asset::gui::WidgetKind::panel
                || kind == asset::gui::WidgetKind::scroll_area
                || kind == asset::gui::WidgetKind::tab_page;
        }
    }

    EpochGuiAdapter::EpochGuiAdapter(
        std::string projectId,
        std::string projectRoot) noexcept
        : project_id_(std::move(projectId)),
          project_root_(std::move(projectRoot))
    {
    }

    bool EpochGuiAdapter::valid() const noexcept
    {
        return !project_id_.empty() && !project_root_.empty();
    }

    EpochGuiAdapter::~EpochGuiAdapter() noexcept
    {
        clear();
    }

    EpochGuiRenderResult EpochGuiAdapter::render(
        project_gui_runtime::RuntimeSession& runtime,
        project_gui_runtime::Float2 viewport) noexcept
    {
        EpochGuiRenderResult result{};
        if (!valid() || !runtime.valid())
            return result;
        bool topLayerBegun{};
        try
        {
            project_gui_runtime::RuntimeFrame frame =
                runtime.build_frame(viewport);
            result.code = frame.code;
            if (!frame)
                return result;

            auto observed = runtime.metrics();
            const auto refresh = [&]() {
                const auto current = runtime.metrics();
                if (current.artifact_replacements == observed.artifact_replacements
                    && current.focus_changes == observed.focus_changes
                    && current.tab_changes == observed.tab_changes
                    && current.text_changes == observed.text_changes
                    && current.slider_changes == observed.slider_changes)
                    return true;
                auto updated = runtime.build_frame(viewport);
                if (!updated) { result.code = updated.code; return false; }
                frame = std::move(updated);
                observed = runtime.metrics();
                return true;
            };

            const gui::Vec2 mouse = gui::mouse_position();
            result.pointer_captured = runtime.pointer_captured(
                {mouse.x, mouse.y});
            if (const auto press = gui::mouse_press_position(); press
                && !runtime.pointer_captured({press->x, press->y}))
                (void)runtime.clear_focus();
            if (gui::was_mouse_released())
                (void)runtime.pointer_release({mouse.x, mouse.y});
            if (!refresh()) return result;
            const auto keyboard = dispatch_keyboard(runtime, frame);
            if (keyboard != project_gui_runtime::RuntimeCode::ready
                && keyboard != project_gui_runtime::RuntimeCode::no_change)
                result.code = keyboard;
            if (!refresh()) return result;

            gui::begin_top_layer();
            topLayerBegun = true;
            for (const auto& view : frame.widgets)
            {
                if (!view.visible)
                    continue;
                ++result.visible_widgets;
                if (container(view.kind))
                    gui::panel_rect(point(view.bounds), extent(view.bounds));
            }
            render_tabs(runtime, frame);
            if (!refresh())
            {
                gui::end_top_layer();
                topLayerBegun = false;
                return result;
            }
            for (const auto& view : frame.widgets)
            {
                if (view.visible)
                    render_widget(runtime, view);
            }
            gui::end_top_layer();
            topLayerBegun = false;

            for (const auto& [path, image] : images_)
            {
                (void)path;
                if (image.sprite.is_valid())
                    ++result.resolved_images;
            }
            result.events = runtime.drain_events();
            return result;
        }
        catch (...)
        {
            if (topLayerBegun)
                gui::end_top_layer();
            result.code = project_gui_runtime::RuntimeCode::allocation_failure;
            return result;
        }
    }

    void EpochGuiAdapter::clear() noexcept
    {
        gui::release_keyboard_input(this);
        for (const auto& [id, text] : text_state_)
        { (void)id; gui::release_keyboard_input(&text); }
        text_state_.clear();
        images_.clear();
    }

    project_gui_runtime::RuntimeCode EpochGuiAdapter::dispatch_keyboard(
        project_gui_runtime::RuntimeSession& runtime,
        const project_gui_runtime::RuntimeFrame& frame)
    {
        using Kind = asset::gui::WidgetKind;
        using Code = project_gui_runtime::RuntimeCode;
        Code code = Code::ready;
        const auto record = [&](Code outcome) {
            if (code == Code::ready || code == Code::no_change) code = outcome;
            return outcome;
        };
        if (runtime.keyboard_captured()) for (const auto& view : frame.widgets)
        {
            if (view.kind != Kind::text_input) continue;
            auto [text, inserted] = text_state_.try_emplace(view.id.value, view.primary_text);
            if (!inserted && text->second != view.primary_text)
                text->second = view.primary_text;
        }
        const auto focused = [&]() -> const project_gui_runtime::WidgetView* {
            if (!runtime.keyboard_captured()) return nullptr;
            const auto id = runtime.focused_widget();
            const auto found = std::find_if(frame.widgets.begin(), frame.widgets.end(),
                [&](const auto& view) { return view.id == id; });
            return found != frame.widgets.end() && found->visible && found->enabled
                ? &*found : nullptr;
        };
        bool tabTextPending{};
        (void)gui::dispatch_keyboard_input({
            .owner = this,
            .owns_text = [&](const void* identity) {
                return std::any_of(text_state_.begin(), text_state_.end(),
                    [&](const auto& item) { return identity == &item.second; });
            },
            .target = [&]() {
                gui::KeyboardInputTarget target{};
                const auto* view = focused();
                if (!view) return target;
                target.identity = this;
                if (view->kind == Kind::text_input)
                {
                    const auto& source = runtime.artifact()->widgets[view->index];
                    target.text = &text_state_.at(view->id.value);
                    target.identity = target.text;
                    target.size = extent(view->bounds);
                    target.maximum_bytes = source.maximum_length;
                    target.multiline = source.multiline;
                    target.read_only = source.read_only;
                }
                return target;
            },
            .command = [&](const gui::InputEvent& event) {
                const auto* view = focused();
                if (!view) return false;
                if (event.type == gui::EventType::TextInput)
                {
                    if (tabTextPending && event.text == "\t")
                    { tabTextPending = false; return true; }
                    tabTextPending = false;
                    return view->kind != Kind::text_input;
                }
                if (event.type != gui::EventType::KeyDown
                    || event.ctrl_down || event.alt_down) return false;
                if (event.key == 9)
                {
                    // Multiline fields retain literal Tab. Shift+Tab leaves
                    // the field; Ctrl+Tab remains the containing group's key.
                    if (!event.shift_down && view->kind == Kind::text_input
                        && runtime.artifact()->widgets[view->index].multiline)
                        return false;
                    (void)record(runtime.focus_next(event.shift_down));
                    tabTextPending = true;
                    return true;
                }
                if (event.key == 27)
                { (void)record(runtime.clear_focus()); return true; }
                if ((event.key == 13 || event.key == 32)
                    && (view->kind == Kind::button || view->kind == Kind::image_button))
                { (void)record(runtime.activate_focused()); return true; }
                if ((event.key == 37 || event.key == 39) && view->kind == Kind::slider)
                {
                    const auto& source = runtime.artifact()->widgets[view->index];
                    const double step = source.step > 0.0 ? source.step
                        : (source.maximum - source.minimum) * 0.01;
                    const auto current = runtime.build_frame(frame.viewport);
                    const auto currentView = std::find_if(current.widgets.begin(), current.widgets.end(),
                        [&](const auto& item) { return item.id == view->id; });
                    if (!current || currentView == current.widgets.end())
                    { (void)record(Code::invalid_widget); return true; }
                    (void)record(runtime.set_slider(view->id, currentView->value
                        + (event.key == 37 ? -step : step)));
                    return true;
                }
                return false;
            },
            .cancel = [&]() { (void)record(runtime.clear_focus()); },
            .text_changed = [&]() {
                const auto* view = focused();
                if (!view || view->kind != Kind::text_input) return false;
                const auto changed = record(runtime.set_text(view->id, text_state_.at(view->id.value)));
                return changed == Code::ready || changed == Code::no_change;
            }});
        return code;
    }

    EpochGuiAdapter::ImageState& EpochGuiAdapter::resolve_image(
        std::string_view logicalPath)
    {
        auto [iterator, inserted] = images_.try_emplace(
            std::string{logicalPath});
        ImageState& state = iterator->second;
        if (!state.load_attempted)
        {
            state.load_attempted = true;
            project_textures::TextureArtifactLibrary library{
                project_id_, project_root_};
            auto loaded = library.load_latest(
                logicalPath,
                asset::texture::TextureCompileProfile{});
            if (!loaded)
            {
                asset::texture::TextureCompileProfile linear{};
                linear.format = asset::texture::ArtifactFormat::rgba8_unorm;
                linear.color_space = asset::texture::ColorSpace::linear;
                loaded = library.load_latest(logicalPath, linear);
            }
            if (loaded && !loaded.artifact.mips.empty())
            {
                const auto& mip = loaded.artifact.mips.front();
                state.width = mip.width;
                state.height = mip.height;
                state.rgba.resize(mip.texels.size());
                std::memcpy(
                    state.rgba.data(), mip.texels.data(), mip.texels.size());
            }
        }
        if (!state.sprite.is_valid() && !state.rgba.empty())
        {
            state.sprite = gui::register_runtime_surface(
                logicalPath,
                state.rgba,
                state.width,
                state.height);
        }
        (void)inserted;
        return state;
    }

    void EpochGuiAdapter::render_tabs(
        project_gui_runtime::RuntimeSession& runtime,
        const project_gui_runtime::RuntimeFrame& frame)
    {
        std::size_t begin{};
        while (begin < frame.tab_headers.size())
        {
            const std::uint32_t tabSet = frame.tab_headers[begin].tab_set;
            std::size_t end = begin + 1u;
            while (end < frame.tab_headers.size()
                && frame.tab_headers[end].tab_set == tabSet)
            {
                ++end;
            }

            const auto first = frame.tab_headers[begin].bounds;
            const auto last = frame.tab_headers[end - 1u].bounds;
            std::vector<std::string> ids{};
            std::vector<gui::TabButtonSpec> tabs{};
            ids.reserve(end - begin);
            tabs.reserve(end - begin);
            for (std::size_t index = begin; index < end; ++index)
            {
                const auto& header = frame.tab_headers[index];
                ids.push_back(widget_id(
                    runtime.artifact()->widgets[header.page].id,
                    "tab"));
            }
            for (std::size_t index = begin; index < end; ++index)
            {
                const auto& header = frame.tab_headers[index];
                tabs.push_back({
                    .id = ids[index - begin],
                    .label = header.label,
                    .width = header.bounds.width,
                    .active = header.selected,
                    .closable = header.closeable,
                    .enabled = header.enabled});
            }

            gui::begin_window(
                "",
                {first.x, first.y},
                {last.x + last.width - first.x, first.height},
                false);
            const gui::TabBarResult changed = gui::tab_bar_buttons(
                tabs, first.height, 0.0f);
            gui::end_window();
            if (changed.selected_index
                && *changed.selected_index < tabs.size())
            {
                const auto& header = frame.tab_headers[
                    begin + *changed.selected_index];
                (void)runtime.select_tab(
                    runtime.artifact()->widgets[tabSet].id,
                    runtime.artifact()->widgets[header.page].id);
            }
            begin = end;
        }
    }

    void EpochGuiAdapter::render_widget(
        project_gui_runtime::RuntimeSession& runtime,
        const project_gui_runtime::WidgetView& view)
    {
        using Kind = asset::gui::WidgetKind;
        if (view.kind == Kind::canvas || view.kind == Kind::panel
            || view.kind == Kind::scroll_area || view.kind == Kind::tab_set
            || view.kind == Kind::tab_page)
        {
            return;
        }

        const gui::Vec2 position = point(view.bounds);
        const gui::Vec2 size = extent(view.bounds);
        gui::begin_window("", position, size, false);
        {
            gui::ContentClipScope clip(point(view.clip),
                {view.clip.x + view.clip.width, view.clip.y + view.clip.height});
            gui::ScopedInputSuppression disabled{!view.enabled};
            switch (view.kind)
            {
            case Kind::button:
                (void)gui::button_selected(view.primary_text, size, view.focused);
                break;
            case Kind::text:
                gui::wrapped_label(view.primary_text, size.x);
                break;
            case Kind::image:
            case Kind::image_button:
            {
                ImageState& image = resolve_image(view.asset_path);
                const auto id = widget_id(view.id, "image");
                (void)gui::image_box({
                    .id = id,
                    .sprite = image.sprite,
                    .size = size,
                    .source_size = {
                        static_cast<float>(image.width),
                        static_cast<float>(image.height)},
                    .caption = view.secondary_text,
                    .fit = gui::ImageFit::Contain,
                    .interactive = view.kind == Kind::image_button,
                    .selected = view.focused,
                    .enabled = view.enabled});
                break;
            }
            case Kind::text_input:
            {
                auto [text, inserted] = text_state_.try_emplace(
                    view.id.value, view.primary_text);
                if (!inserted && text->second != view.primary_text
                    && !view.focused)
                {
                    text->second = view.primary_text;
                }
                const auto edited = gui::edit_box(
                    text->second,
                    size,
                    runtime.artifact()->widgets[view.index].maximum_length,
                    runtime.artifact()->widgets[view.index].multiline,
                    gui::TextInputKeyboardPolicy::focused, false, {}, true);
                if (edited.changed)
                {
                    const auto changed = runtime.set_text(view.id, text->second);
                    if (changed != project_gui_runtime::RuntimeCode::ready
                        && changed != project_gui_runtime::RuntimeCode::no_change)
                        text->second = view.primary_text;
                }
                break;
            }
            case Kind::slider:
            {
                const auto& source = runtime.artifact()->widgets[view.index];
                const auto id = widget_id(view.id, "slider");
                const auto changed = gui::slider({
                    .id = id,
                    .label = view.primary_text,
                    .minimum = static_cast<float>(source.minimum),
                    .maximum = static_cast<float>(source.maximum),
                    .value = static_cast<float>(view.value),
                    .step = static_cast<float>(source.step),
                    .size = size,
                    .enabled = view.enabled});
                if (changed.changed)
                    (void)runtime.set_slider(view.id, changed.value);
                break;
            }
            default:
                break;
            }
        }
        gui::end_window();
    }
}
