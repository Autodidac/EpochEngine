// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Actual editor adapter + authoring document consumer; no renderer or native UI.
#include <cstdint>
#include <cstddef>
#include <concepts>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

import authoring.gui_document;
import editor.hierarchy_adapter;
import editor.authoring_history;

namespace
{
    namespace gui = epochengine::authoring::gui;
    namespace hierarchy = epochengine::editor_hierarchy;

    void require(bool accepted, int line)
    {
        if (!accepted)
        {
            std::fprintf(stderr, "GUI hierarchy contract failed at line %d\n", line);
            std::exit(81);
        }
    }
#define REQUIRE(value) require(static_cast<bool>(value), __LINE__)

    gui::WidgetDescriptor canvas(std::string name)
    {
        gui::WidgetDescriptor result{};
        result.kind = gui::WidgetKind::canvas;
        result.name = std::move(name);
        result.content = gui::CanvasContent{};
        return result;
    }

    gui::WidgetDescriptor panel(std::string name)
    {
        gui::WidgetDescriptor result{};
        result.kind = gui::WidgetKind::panel;
        result.name = std::move(name);
        result.content = gui::PanelContent{};
        return result;
    }

    gui::WidgetDescriptor text(std::string name, std::string content)
    {
        gui::WidgetDescriptor result{};
        result.kind = gui::WidgetKind::text;
        result.name = std::move(name);
        result.content = gui::TextContent{std::move(content), true};
        return result;
    }
}

#include "gui_owner_contract.hpp"

int main()
{
    gui::GuiDocument document{gui::DocumentHandle{17u, 2u},
        gui::BranchIdentity{29u, 5u}};
    REQUIRE(document.valid());
    const auto root = document.create_widget(canvas("Canvas")).widget;
    const auto body = document.create_widget(panel("Body"), root).widget;
    const auto caption = document.create_widget(
        text("Caption", std::string(32'768u, 'x')), body).widget;
    const auto sibling = document.create_widget(panel("Sibling"), root).widget;
    const auto secondRoot = document.create_widget(canvas("OtherCanvas")).widget;
    const auto unrelated = document.create_widget(panel("OtherTree"), secondRoot).widget;
    REQUIRE(root && body && caption && sibling && secondRoot && unrelated);

    auto projected = hierarchy::project_gui_hierarchy(document, root);
    REQUIRE(projected.ready && projected.root == root);
    REQUIRE(projected.document == document.document());
    REQUIRE(projected.branch == document.branch());
    REQUIRE(projected.revision == document.revision());
    REQUIRE(projected.rows.size() == 3u);
    REQUIRE(projected.rows[0].handle == body && projected.rows[0].depth == 0u);
    REQUIRE(projected.rows[1].handle == caption && projected.rows[1].depth == 1u);
    REQUIRE(projected.rows[2].handle == sibling && projected.rows[2].depth == 0u);
    REQUIRE(projected.rows[0].name == "Body");
    REQUIRE(projected.rows[1].name == "Caption");
    REQUIRE(projected.rows[1].kind == gui::WidgetKind::text);
    // The payload is hierarchy metadata, never all widget text/content.
    std::size_t nameBytes{};
    for (const auto& row : projected.rows)
    {
        nameBytes += row.name.size();
        REQUIRE(row.handle != root && row.handle != unrelated);
        REQUIRE(row.depth < document.limits().maximum_hierarchy_depth);
    }
    REQUIRE(nameBytes == 18u);

    const auto before = projected.revision;
    REQUIRE(document.reorder_widget(sibling, 0u));
    projected = hierarchy::project_gui_hierarchy(document, root);
    REQUIRE(projected.revision != before);
    REQUIRE(projected.rows[0].handle == sibling);
    REQUIRE(projected.rows[1].handle == body);
    REQUIRE(projected.rows[2].handle == caption);
    REQUIRE(document.undo());
    projected = hierarchy::project_gui_hierarchy(document, root);
    REQUIRE(projected.rows[0].handle == body);

    REQUIRE(!hierarchy::project_gui_hierarchy(document, {}).ready);
    REQUIRE(!hierarchy::project_gui_hierarchy(document,
        gui::WidgetHandle{root.index, root.generation + 1u}).ready);
    REQUIRE(document.remove_widget(caption));
    const auto replacement = document.create_widget(text("Replacement", "new"), body).widget;
    REQUIRE(replacement && replacement != caption);
    REQUIRE(!document.widget(caption));
    projected = hierarchy::project_gui_hierarchy(document, root);
    REQUIRE(projected.rows.size() == 3u);
    REQUIRE(projected.rows[1].handle == replacement);

    const auto emptyRoot = document.create_widget(canvas("EmptyCanvas")).widget;
    const auto empty = hierarchy::project_gui_hierarchy(document, emptyRoot);
    REQUIRE(empty.ready && empty.rows.empty());
    gui::GuiDocument invalid{gui::DocumentHandle{}, gui::BranchIdentity{29u, 5u}};
    REQUIRE(!hierarchy::project_gui_hierarchy(invalid, root).ready);

    gui::DocumentLimits limits{};
    limits.maximum_active_widgets = 3u;
    limits.maximum_widget_slots = 3u;
    limits.maximum_children_per_widget = 2u;
    limits.maximum_hierarchy_depth = 2u;
    limits.maximum_name_bytes = 8u;
    gui::GuiDocument bounded{gui::DocumentHandle{18u, 2u},
        gui::BranchIdentity{29u, 5u}, limits};
    REQUIRE(bounded.valid());
    const auto boundedRoot = bounded.create_widget(canvas("Bounded")).widget;
    const auto left = bounded.create_widget(panel("Left"), boundedRoot).widget;
    const auto right = bounded.create_widget(panel("Right"), boundedRoot).widget;
    REQUIRE(boundedRoot && left && right);
    REQUIRE(!bounded.create_widget(panel("Overflow"), boundedRoot));
    const auto boundedRows = hierarchy::project_gui_hierarchy(bounded, boundedRoot);
    REQUIRE(boundedRows.ready && boundedRows.rows.size() == 2u);
    for (const auto& row : boundedRows.rows)
        REQUIRE(row.name.size() <= bounded.limits().maximum_name_bytes);
    owner_contract::run();
    return 0;
}
