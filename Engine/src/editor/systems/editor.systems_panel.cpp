/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include "core.format_text.hpp"

#include <gui/node_graph_workspace.hpp>
#include <gui/system_workspace.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

module editor.systems_panel;

import authoring.task_graph;
import editor.systems_workspace;
import editor.task_scheduler;
import gui.engine;
import sprite.handle;
import taskgraph.dotsystem;

namespace epochengine::editor_systems_panel
{
    namespace
    {
        struct PixelCanvas final
        {
            int width{};
            int height{};
            std::vector<std::uint8_t> pixels{};

            PixelCanvas(int canvasWidth, int canvasHeight, gui::Color clear)
                : width(canvasWidth)
                , height(canvasHeight)
                , pixels(static_cast<std::size_t>(canvasWidth)
                    * static_cast<std::size_t>(canvasHeight) * 4u, 0u)
            {
                fill_rect(0, 0, width, height, clear);
            }

            void fill_rect(int x, int y, int extentWidth, int extentHeight,
                gui::Color color) noexcept
            {
                const int x0 = (std::clamp)(x, 0, width);
                const int y0 = (std::clamp)(y, 0, height);
                const int x1 = (std::clamp)(x + extentWidth, 0, width);
                const int y1 = (std::clamp)(y + extentHeight, 0, height);
                for (int row = y0; row < y1; ++row)
                {
                    for (int column = x0; column < x1; ++column)
                    {
                        const std::size_t offset =
                            (static_cast<std::size_t>(row) * static_cast<std::size_t>(width)
                                + static_cast<std::size_t>(column)) * 4u;
                        pixels[offset] = color.r;
                        pixels[offset + 1u] = color.g;
                        pixels[offset + 2u] = color.b;
                        pixels[offset + 3u] = color.a;
                    }
                }
            }

            void stroke_rect(int x, int y, int extentWidth, int extentHeight,
                gui::Color color, int thickness = 2) noexcept
            {
                fill_rect(x, y, extentWidth, thickness, color);
                fill_rect(x, y + extentHeight - thickness, extentWidth, thickness, color);
                fill_rect(x, y, thickness, extentHeight, color);
                fill_rect(x + extentWidth - thickness, y, thickness, extentHeight, color);
            }

            void hline(int x, int y, int extent, gui::Color color,
                int thickness = 2) noexcept
            {
                fill_rect(x, y - thickness / 2, extent, thickness, color);
            }

            void vline(int x, int y, int extent, gui::Color color,
                int thickness = 2) noexcept
            {
                fill_rect(x - thickness / 2, y, thickness, extent, color);
            }

            void line(int x0, int y0, int x1, int y1,
                gui::Color color, int thickness = 2) noexcept
            {
                const int deltaX = std::abs(x1 - x0);
                const int stepX = x0 < x1 ? 1 : -1;
                const int deltaY = -std::abs(y1 - y0);
                const int stepY = y0 < y1 ? 1 : -1;
                int error = deltaX + deltaY;
                for (;;)
                {
                    fill_rect(x0 - thickness / 2, y0 - thickness / 2,
                        thickness, thickness, color);
                    if (x0 == x1 && y0 == y1)
                        break;
                    const int doubled = error * 2;
                    if (doubled >= deltaY)
                    {
                        error += deltaY;
                        x0 += stepX;
                    }
                    if (doubled <= deltaX)
                    {
                        error += deltaX;
                        y0 += stepY;
                    }
                }
            }
        };

        [[nodiscard]] constexpr
            gui_lib::node_graph_workspace::NodeId graph_node_id(
                authoring::task_graph::NodeHandle handle) noexcept
        {
            return (static_cast<std::uint64_t>(handle.generation) << 32u)
                | (static_cast<std::uint64_t>(handle.index) + 1u);
        }

        [[nodiscard]] constexpr
            gui_lib::node_graph_workspace::PinId graph_pin_id(
                authoring::task_graph::NodeHandle handle,
                bool output) noexcept
        {
            return graph_node_id(handle) * 2u + (output ? 1u : 0u);
        }

        [[nodiscard]] constexpr
            gui_lib::node_graph_workspace::EdgeId graph_edge_id(
                authoring::task_graph::NodeHandle prerequisite,
                authoring::task_graph::NodeHandle dependent) noexcept
        {
            std::uint64_t value = graph_node_id(prerequisite);
            value ^= graph_node_id(dependent)
                + 0x9e3779b97f4a7c15ull
                + (value << 6u) + (value >> 2u);
            return value == 0u ? 1u : value;
        }

        struct NodePlacement final
        {
            authoring::task_graph::NodeHandle handle{};
            int x{};
            int y{};
            int width{};
            int height{};
            std::uint32_t wave{};
            bool critical{};
        };

        using NodeOffsetMap = std::unordered_map<
            gui_lib::node_graph_workspace::NodeId,
            gui_lib::node_graph_workspace::Vec2>;

        [[nodiscard]] PixelCanvas build_learning_graph_surface(
            const authoring::task_graph::GraphDiagnostics& diagnostics,
            float zoom,
            int pan,
            authoring::task_graph::NodeHandle selected,
            const NodeOffsetMap& nodeOffsets)
        {
            constexpr int canvasWidth = 1'280;
            constexpr int canvasHeight = 300;
            PixelCanvas canvas(canvasWidth, canvasHeight, gui::Color{15, 18, 24, 255});
            canvas.fill_rect(18, 14, canvasWidth - 36, 24,
                gui::Color{30, 38, 48, 255});
            canvas.fill_rect(18, canvasHeight - 30, canvasWidth - 36, 16,
                gui::Color{28, 33, 41, 255});

            const auto nodes = diagnostics.nodes();
            if (nodes.empty())
            {
                canvas.stroke_rect(40, 72, canvasWidth - 80, 130,
                    gui::Color{92, 106, 128, 255});
                return canvas;
            }

            constexpr std::size_t maximumVisibleNodes = 32u;
            const std::size_t visibleCount = (std::min)(
                nodes.size(), maximumVisibleNodes);
            const int nodeWidth = (std::max)(
                92, static_cast<int>(132.0f * zoom));
            constexpr int nodeHeight = 42;
            const int waveGap = (std::max)(
                38, static_cast<int>(58.0f * zoom));
            const int baseX = 42 - pan;
            const std::uint32_t waveCount = (std::max)(
                diagnostics.metrics().parallel_waves, 1u);
            std::vector<std::uint32_t> ordinals(waveCount, 0u);
            std::vector<NodePlacement> placements{};
            placements.reserve(visibleCount);

            for (std::size_t index = 0u; index < visibleCount; ++index)
            {
                const auto& node = nodes[index];
                if (ordinals.size() <= node.parallel_wave)
                    ordinals.resize(node.parallel_wave + 1u, 0u);
                const std::uint32_t ordinal = ordinals[node.parallel_wave]++;
                const auto offset = nodeOffsets.find(graph_node_id(node.handle));
                const int offsetX = offset == nodeOffsets.end()
                    ? 0 : static_cast<int>(std::llround(offset->second.x));
                const int offsetY = offset == nodeOffsets.end()
                    ? 0 : static_cast<int>(std::llround(offset->second.y));
                placements.push_back(NodePlacement{
                    .handle = node.handle,
                    .x = baseX + static_cast<int>(node.parallel_wave)
                        * (nodeWidth + waveGap) + offsetX,
                    .y = 58 + static_cast<int>(ordinal) * (nodeHeight + 12)
                        + offsetY,
                    .width = nodeWidth,
                    .height = nodeHeight,
                    .wave = node.parallel_wave,
                    .critical = node.on_critical_path
                });
            }

            auto placement_for = [&placements](
                authoring::task_graph::NodeHandle handle)
                    -> const NodePlacement*
            {
                const auto found = std::find_if(
                    placements.begin(), placements.end(),
                    [handle](const NodePlacement& placement)
                    {
                        return placement.handle == handle;
                    });
                return found == placements.end() ? nullptr : &*found;
            };

            for (const auto& edge : diagnostics.edges())
            {
                const NodePlacement* source = placement_for(edge.prerequisite);
                const NodePlacement* target = placement_for(edge.dependent);
                if (!source || !target)
                    continue;
                const int sourceX = source->x + source->width;
                const int sourceY = source->y + source->height / 2;
                const int targetX = target->x;
                const int targetY = target->y + target->height / 2;
                const int elbowX = sourceX + (targetX - sourceX) / 2;
                canvas.hline(sourceX, sourceY,
                    (std::max)(1, elbowX - sourceX),
                    gui::Color{82, 94, 116, 255});
                canvas.vline(elbowX, (std::min)(sourceY, targetY),
                    (std::max)(2, std::abs(targetY - sourceY)),
                    gui::Color{82, 94, 116, 255});
                canvas.hline(elbowX, targetY,
                    (std::max)(1, targetX - elbowX),
                    gui::Color{82, 94, 116, 255});
            }

            constexpr std::array<gui::Color, 6> waveColors{{
                {67, 112, 174, 255},
                {66, 145, 111, 255},
                {160, 125, 54, 255},
                {123, 82, 155, 255},
                {153, 76, 104, 255},
                {65, 137, 148, 255}
            }};
            for (const NodePlacement& placement : placements)
            {
                const gui::Color fill = waveColors[
                    placement.wave % waveColors.size()];
                canvas.fill_rect(placement.x, placement.y,
                    placement.width, placement.height, fill);
                canvas.stroke_rect(placement.x, placement.y,
                    placement.width, placement.height,
                    placement.handle == selected
                        ? gui::Color{111, 210, 255, 255}
                        : placement.critical
                            ? gui::Color{255, 208, 92, 255}
                            : gui::Color{255, 255, 255, 52},
                    placement.handle == selected ? 3 : 2);
                canvas.fill_rect(placement.x + 8, placement.y + 9,
                    (std::max)(12, placement.width - 30), 5,
                    gui::Color{244, 247, 252, 190});
                canvas.fill_rect(placement.x + 8, placement.y + 24,
                    (std::max)(12, placement.width / 2), 4,
                    gui::Color{220, 228, 238, 150});
                const int pinY = placement.y + placement.height / 2 - 4;
                canvas.fill_rect(placement.x - 4, pinY, 8, 8,
                    gui::Color{112, 196, 246, 255});
                canvas.fill_rect(
                    placement.x + placement.width - 4,
                    pinY, 8, 8,
                    gui::Color{246, 183, 92, 255});
            }
            return canvas;
        }

        [[nodiscard]] bool prepare_learning_graph_workspace(
            gui_lib::node_graph_workspace::Controller& workspace,
            const authoring::task_graph::GraphDiagnostics& diagnostics,
            float zoom,
            int pan,
            gui::WidgetBounds imageBounds,
            const NodeOffsetMap& nodeOffsets)
        {
            namespace graph_gui = gui_lib::node_graph_workspace;
            constexpr std::size_t maximumVisibleNodes = 32u;
            const auto nodes = diagnostics.nodes();
            const std::size_t visibleCount = (std::min)(
                nodes.size(), maximumVisibleNodes);
            const int nodeWidth = (std::max)(
                92, static_cast<int>(132.0f * zoom));
            constexpr int nodeHeight = 42;
            const int waveGap = (std::max)(
                38, static_cast<int>(58.0f * zoom));
            const int baseX = 42 - pan;
            const std::uint32_t waveCount = (std::max)(
                diagnostics.metrics().parallel_waves, 1u);
            std::vector<std::uint32_t> ordinals(waveCount, 0u);
            std::vector<graph_gui::NodeLayout> graphNodes{};
            std::vector<graph_gui::PinLayout> graphPins{};
            std::vector<graph_gui::EdgeLayout> graphEdges{};
            graphNodes.reserve(visibleCount);
            graphPins.reserve(visibleCount * 2u);

            for (std::size_t index = 0u; index < visibleCount; ++index)
            {
                const auto& node = nodes[index];
                if (ordinals.size() <= node.parallel_wave)
                    ordinals.resize(node.parallel_wave + 1u, 0u);
                const std::uint32_t ordinal =
                    ordinals[node.parallel_wave]++;
                const auto offset = nodeOffsets.find(graph_node_id(node.handle));
                const int offsetX = offset == nodeOffsets.end()
                    ? 0 : static_cast<int>(std::llround(offset->second.x));
                const int offsetY = offset == nodeOffsets.end()
                    ? 0 : static_cast<int>(std::llround(offset->second.y));
                const int x = baseX
                    + static_cast<int>(node.parallel_wave)
                        * (nodeWidth + waveGap) + offsetX;
                const int y = 58
                    + static_cast<int>(ordinal)
                        * (nodeHeight + 12) + offsetY;
                const graph_gui::NodeId id = graph_node_id(node.handle);
                const graph_gui::PinId inputPin =
                    graph_pin_id(node.handle, false);
                const graph_gui::PinId outputPin =
                    graph_pin_id(node.handle, true);
                graphNodes.push_back(graph_gui::NodeLayout{
                    .id = id,
                    .bounds = {
                        static_cast<double>(x),
                        static_cast<double>(y),
                        static_cast<double>(nodeWidth),
                        static_cast<double>(nodeHeight)},
                    .layout_order = index,
                    .selectable = true});
                graphPins.push_back(graph_gui::PinLayout{
                    .id = inputPin,
                    .node_id = id,
                    .direction = graph_gui::PinDirection::input,
                    .position = {
                        static_cast<double>(x),
                        static_cast<double>(y + nodeHeight / 2)},
                    .hit_radius = 7.0,
                    .layout_order = index * 2u,
                    .connectable = true});
                graphPins.push_back(graph_gui::PinLayout{
                    .id = outputPin,
                    .node_id = id,
                    .direction = graph_gui::PinDirection::output,
                    .position = {
                        static_cast<double>(x + nodeWidth),
                        static_cast<double>(y + nodeHeight / 2)},
                    .hit_radius = 7.0,
                    .layout_order = index * 2u + 1u,
                    .connectable = true});
            }

            const auto visible_node_id =
                [&graphNodes](authoring::task_graph::NodeHandle handle)
                    -> std::optional<graph_gui::NodeId>
            {
                const graph_gui::NodeId id = graph_node_id(handle);
                const auto found = std::find_if(
                    graphNodes.begin(),
                    graphNodes.end(),
                    [id](const graph_gui::NodeLayout& node)
                    {
                        return node.id == id;
                    });
                return found == graphNodes.end()
                    ? std::nullopt
                    : std::optional<graph_gui::NodeId>{id};
            };

            for (const auto& edge : diagnostics.edges())
            {
                const auto source = visible_node_id(edge.prerequisite);
                const auto target = visible_node_id(edge.dependent);
                if (!source || !target)
                    continue;
                graphEdges.push_back(graph_gui::EdgeLayout{
                    .id = graph_edge_id(
                        edge.prerequisite, edge.dependent),
                    .output_pin_id = graph_pin_id(
                        edge.prerequisite, true),
                    .input_pin_id = graph_pin_id(
                        edge.dependent, false),
                    .layout_order = graphEdges.size(),
                    .selectable = true});
            }

            const graph_gui::GraphLayoutInput input{
                .nodes = graphNodes,
                .pins = graphPins,
                .edges = graphEdges,
                .source_revision = diagnostics.revision().sequence};
            const auto replaced = workspace.replace_graph(input);
            if (!replaced.committed
                || imageBounds.size.x <= 0.0f
                || imageBounds.size.y <= 0.0f)
            {
                return false;
            }

            const double displayScale = static_cast<double>(
                imageBounds.size.x) / 1'280.0;
            const auto viewport = workspace.set_viewport({
                imageBounds.position.x,
                imageBounds.position.y,
                imageBounds.size.x,
                imageBounds.size.y});
            const auto view = workspace.set_view({
                .pan = {},
                .zoom = displayScale});
            return viewport.accepted && view.accepted;
        }
        [[nodiscard]] constexpr std::size_t scheduler_state_index(
            taskgraph::NodeState state) noexcept
        {
            switch (state)
            {
            case taskgraph::NodeState::pending: return 0u;
            case taskgraph::NodeState::queued: return 1u;
            case taskgraph::NodeState::running: return 2u;
            case taskgraph::NodeState::completed: return 3u;
            case taskgraph::NodeState::failed: return 4u;
            case taskgraph::NodeState::cancelled: return 5u;
            }
            return 0u;
        }

        [[nodiscard]] constexpr std::string_view scheduler_state_name(
            taskgraph::NodeState state) noexcept
        {
            switch (state)
            {
            case taskgraph::NodeState::pending: return "Pending";
            case taskgraph::NodeState::queued: return "Queued";
            case taskgraph::NodeState::running: return "Running";
            case taskgraph::NodeState::completed: return "Completed";
            case taskgraph::NodeState::failed: return "Failed";
            case taskgraph::NodeState::cancelled: return "Cancelled";
            }
            return "Unknown";
        }

        [[nodiscard]] PixelCanvas build_live_scheduler_surface(
            const taskgraph::GraphSnapshot& snapshot)
        {
            constexpr int canvasWidth = 1'280;
            constexpr int canvasHeight = 300;
            constexpr int nodeWidth = 128;
            constexpr int nodeHeight = 34;
            constexpr int horizontalGap = 58;
            constexpr int verticalGap = 12;
            constexpr int marginX = 24;
            constexpr int marginY = 30;
            constexpr std::size_t maximumVisibleNodes = 72u;
            PixelCanvas canvas(canvasWidth, canvasHeight,
                gui::Color{15, 18, 24, 255});
            constexpr std::array<gui::Color, 6> stateColors{{
                {104, 116, 135, 255},
                {62, 124, 176, 255},
                {208, 153, 54, 255},
                {66, 151, 103, 255},
                {186, 69, 77, 255},
                {116, 105, 133, 255}
            }};

            struct Placement final
            {
                std::uint64_t id{};
                int x{};
                int y{};
                taskgraph::NodeState state{taskgraph::NodeState::pending};
            };

            const std::size_t visibleCount = (std::min)(
                snapshot.nodes.size(),
                maximumVisibleNodes);
            std::vector<std::size_t> depths(visibleCount, 0u);
            const auto indexForId = [&](std::uint64_t id)
                -> std::optional<std::size_t>
            {
                for (std::size_t index = 0u; index < visibleCount; ++index)
                {
                    if (snapshot.nodes[index].id == id)
                        return index;
                }
                return std::nullopt;
            };

            for (std::size_t pass = 0u; pass < visibleCount; ++pass)
            {
                bool changed = false;
                for (const auto& edge : snapshot.edges)
                {
                    const auto prerequisite =
                        indexForId(edge.prerequisite_id);
                    const auto dependent =
                        indexForId(edge.dependent_id);
                    if (!prerequisite || !dependent)
                        continue;
                    const std::size_t candidate =
                        depths[*prerequisite] + 1u;
                    if (candidate > depths[*dependent]
                        && candidate < visibleCount)
                    {
                        depths[*dependent] = candidate;
                        changed = true;
                    }
                }
                if (!changed)
                    break;
            }

            std::size_t maximumDepth = 0u;
            for (const std::size_t depth : depths)
                maximumDepth = (std::max)(maximumDepth, depth);
            std::vector<std::size_t> ordinals(maximumDepth + 1u, 0u);
            std::vector<Placement> placements{};
            placements.reserve(visibleCount);
            for (std::size_t index = 0u; index < visibleCount; ++index)
            {
                const std::size_t depth = depths[index];
                const std::size_t ordinal = ordinals[depth]++;
                placements.push_back({
                    .id = snapshot.nodes[index].id,
                    .x = marginX
                        + static_cast<int>(depth)
                            * (nodeWidth + horizontalGap),
                    .y = marginY
                        + static_cast<int>(ordinal)
                            * (nodeHeight + verticalGap),
                    .state = snapshot.nodes[index].state
                });
            }

            const auto placementFor = [&](std::uint64_t id)
                -> const Placement*
            {
                const auto found = std::find_if(
                    placements.begin(),
                    placements.end(),
                    [id](const Placement& placement)
                    {
                        return placement.id == id;
                    });
                return found == placements.end() ? nullptr : &*found;
            };

            for (const auto& edge : snapshot.edges)
            {
                const Placement* source =
                    placementFor(edge.prerequisite_id);
                const Placement* target =
                    placementFor(edge.dependent_id);
                if (!source || !target)
                    continue;
                const int sourceX = source->x + nodeWidth;
                const int sourceY = source->y + nodeHeight / 2;
                const int targetX = target->x;
                const int targetY = target->y + nodeHeight / 2;
                const int elbowX = sourceX
                    + (targetX - sourceX) / 2;
                canvas.hline(
                    sourceX, sourceY,
                    (std::max)(1, elbowX - sourceX),
                    gui::Color{92, 111, 138, 255}, 2);
                canvas.vline(
                    elbowX,
                    (std::min)(sourceY, targetY),
                    (std::max)(2, std::abs(targetY - sourceY)),
                    gui::Color{92, 111, 138, 255}, 2);
                canvas.hline(
                    elbowX, targetY,
                    (std::max)(1, targetX - elbowX),
                    gui::Color{92, 111, 138, 255}, 2);
                canvas.fill_rect(
                    targetX - 4, targetY - 3, 6, 6,
                    gui::Color{152, 177, 213, 255});
            }

            for (const Placement& placement : placements)
            {
                if (placement.x >= canvasWidth
                    || placement.y >= canvasHeight)
                {
                    continue;
                }
                const gui::Color color =
                    stateColors[scheduler_state_index(placement.state)];
                canvas.fill_rect(
                    placement.x, placement.y,
                    nodeWidth, nodeHeight, color);
                canvas.stroke_rect(
                    placement.x, placement.y,
                    nodeWidth, nodeHeight,
                    gui::Color{225, 232, 242, 80},
                    placement.state == taskgraph::NodeState::running
                        ? 3
                        : 1);
                canvas.fill_rect(
                    placement.x + 9, placement.y + 9,
                    nodeWidth - 28, 5,
                    gui::Color{245, 248, 252, 190});
                canvas.fill_rect(
                    placement.x + 9, placement.y + 22,
                    nodeWidth / 2, 4,
                    gui::Color{222, 230, 240, 140});
            }

            if (snapshot.omittedNodeCount != 0u
                || snapshot.omittedEdgeCount != 0u)
            {
                canvas.fill_rect(
                    canvasWidth - 180, canvasHeight - 20,
                    156, 5,
                    gui::Color{208, 153, 54, 255});
            }
            return canvas;
        }
        [[nodiscard]] PixelCanvas build_time_surface(
            const editor_systems::TimeDiagnosticsSnapshot& diagnostics)
        {
            constexpr int canvasWidth = 1'280;
            constexpr int canvasHeight = 260;
            constexpr int left = 28;
            constexpr int right = canvasWidth - 22;
            constexpr int top = 20;
            constexpr int bottom = canvasHeight - 28;
            PixelCanvas canvas(canvasWidth, canvasHeight,
                gui::Color{15, 18, 24, 255});
            for (int line = 0; line <= 4; ++line)
            {
                const int y = top + (bottom - top) * line / 4;
                canvas.hline(left, y, right - left,
                    gui::Color{45, 53, 65, 255}, 1);
            }
            canvas.stroke_rect(left, top, right - left, bottom - top,
                gui::Color{76, 86, 101, 255}, 1);
            if (diagnostics.samples.empty())
                return canvas;

            constexpr std::size_t maximumVisibleSamples = 240u;
            const std::size_t first = diagnostics.samples.size()
                > maximumVisibleSamples
                ? diagnostics.samples.size() - maximumVisibleSamples
                : 0u;
            std::uint64_t maximum = 1u;
            for (std::size_t index = first;
                index < diagnostics.samples.size(); ++index)
            {
                maximum = (std::max)(maximum,
                    (std::max)(diagnostics.samples[index].frame_nanoseconds,
                        diagnostics.samples[index].systems_nanoseconds));
            }

            const std::size_t count = diagnostics.samples.size() - first;
            auto point = [count, maximum, top, bottom](std::size_t ordinal,
                std::uint64_t value) -> std::pair<int, int>
            {
                const int x = count <= 1u
                    ? left
                    : left + static_cast<int>(ordinal)
                        * (right - left) / static_cast<int>(count - 1u);
                const int y = bottom - static_cast<int>(
                    (static_cast<long double>(value)
                        / static_cast<long double>(maximum))
                    * static_cast<long double>(bottom - top));
                return {x, (std::clamp)(y, top, bottom)};
            };

            for (std::size_t ordinal = 1u; ordinal < count; ++ordinal)
            {
                const auto frame0 = point(ordinal - 1u,
                    diagnostics.samples[first + ordinal - 1u].frame_nanoseconds);
                const auto frame1 = point(ordinal,
                    diagnostics.samples[first + ordinal].frame_nanoseconds);
                canvas.line(frame0.first, frame0.second, frame1.first, frame1.second,
                    gui::Color{76, 164, 225, 255}, 2);
                const auto systems0 = point(ordinal - 1u,
                    diagnostics.samples[first + ordinal - 1u].systems_nanoseconds);
                const auto systems1 = point(ordinal,
                    diagnostics.samples[first + ordinal].systems_nanoseconds);
                canvas.line(systems0.first, systems0.second,
                    systems1.first, systems1.second,
                    gui::Color{235, 176, 65, 255}, 2);
            }
            return canvas;
        }

        [[nodiscard]] std::string duration_text(std::uint64_t nanoseconds)
        {
            return epochengine::format_text(
                "{:.3f} ms",
                static_cast<double>(nanoseconds) / 1'000'000.0);
        }
    }

    struct Panel::Implementation final
    {
        editor_systems::SystemsWorkspace workspace{};
        std::string text_filter{};
        std::string new_task_name{"Gameplay Update"};
        std::string new_task_category{"Learning"};
        std::string new_task_cost{"5"};
        std::string interaction_status{
            "Learning graph is isolated from the live scheduler."};
        std::size_t prerequisite_index{};
        std::size_t dependent_index{1u};
        authoring::task_graph::NodeHandle selected_task{};
        std::string selected_task_name{};
        std::string selected_task_category{};
        std::string selected_task_cost{};
        float task_zoom{1.0f};
        int task_pan{};
        gui_lib::node_graph_workspace::Controller task_workspace{};
        NodeOffsetMap task_node_offsets{};
        std::uint64_t task_surface_revision{};
        SpriteHandle task_surface{};
        std::uint64_t scheduler_surface_revision{};
        SpriteHandle scheduler_surface{};
        std::uint64_t time_surface_revision{};
        SpriteHandle time_surface{};

        void invalidate_graph_surface() noexcept
        {
            task_surface = {};
            task_surface_revision = 0u;
        }
    };

    Panel::Panel()
        : implementation_(std::make_unique<Implementation>())
    {
    }

    Panel::~Panel() = default;
    Panel::Panel(Panel&&) noexcept = default;
    Panel& Panel::operator=(Panel&&) noexcept = default;

    bool Panel::diagnostics_sampling_intent() const noexcept
    {
        return implementation_
            && implementation_->workspace.diagnostics_sampling_intent();
    }

    void Panel::render(
        float availableWidth,
        systems::Registry& registry,
        const Snapshot& snapshot,
        const taskgraph::TaskGraph* liveScheduler,
        std::string_view liveSchedulerOwner)
    {
        if (!implementation_)
            return;
        auto& state = *implementation_;
        const float width = (std::max)(180.0f, availableWidth);
        (void)state.workspace.set_diagnostics_sampling_intent(true);
        const auto refresh = state.workspace.refresh_from_registry(registry);
        const auto schedulerRefresh = state.workspace.refresh_live_scheduler(
            liveScheduler, liveSchedulerOwner);

        const std::array tabs{
            gui::TabButtonSpec{
                .id = "systems.overview",
                .label = "Overview",
                .width = 78.0f,
                .active = state.workspace.tab()
                    == editor_systems::Tab::overview},
            gui::TabButtonSpec{
                .id = "systems.registry",
                .label = "Systems",
                .width = 72.0f,
                .active = state.workspace.tab()
                    == editor_systems::Tab::systems},
            gui::TabButtonSpec{
                .id = "systems.scheduler_graph",
                .label = "Scheduler",
                .width = 82.0f,
                .active = state.workspace.tab()
                    == editor_systems::Tab::live_scheduler},
            gui::TabButtonSpec{
                .id = "systems.learning_graph",
                .label = "Task Graph",
                .width = 88.0f,
                .active = state.workspace.tab()
                    == editor_systems::Tab::learning_graph},
            gui::TabButtonSpec{
                .id = "systems.time",
                .label = "Time",
                .width = 54.0f,
                .active = state.workspace.tab()
                    == editor_systems::Tab::time},
            gui::TabButtonSpec{
                .id = "systems.renderer",
                .label = "Renderer",
                .width = 76.0f,
                .active = state.workspace.tab()
                    == editor_systems::Tab::renderer}
        };
        if (const auto tabResult = gui::tab_bar_buttons(tabs);
            tabResult.selected_index)
        {
            constexpr std::array values{
                editor_systems::Tab::overview,
                editor_systems::Tab::systems,
                editor_systems::Tab::live_scheduler,
                editor_systems::Tab::learning_graph,
                editor_systems::Tab::time,
                editor_systems::Tab::renderer};
            (void)state.workspace.set_tab(
                values[*tabResult.selected_index]);
        }
        const auto& registrySnapshot = state.workspace.registry_snapshot();

        if (state.workspace.tab() == editor_systems::Tab::overview)
        {
            const auto graph = state.workspace.learning_diagnostics();
            const auto& time = state.workspace.time_diagnostics();
            const std::string_view registryModel =
                refresh.result.code == editor_systems::ControllerCode::ready
                    ? "updated"
                    : refresh.result.code
                            == editor_systems::ControllerCode::unchanged
                        ? "current"
                        : "last valid retained";
            gui::label("Runtime Overview");
            gui::property_row("Runtime",
                epochengine::format_text("{} | {} | {}",
                    snapshot.renderer, snapshot.capability,
                    snapshot.support_tier),
                92.0f);
            gui::property_row("Registry",
                epochengine::format_text(
                    "{} systems | {} | sampling {} | last {} | peak {}",
                    registrySnapshot.systems.size(), registryModel,
                    registrySnapshot.diagnostics_sampling ? "on" : "warming",
                    duration_text(registrySnapshot.last_frame_nanoseconds),
                    duration_text(registrySnapshot.peak_frame_nanoseconds)),
                92.0f);
            gui::property_row("Scheduler",
                epochengine::format_text("{} | {}",
                    editor_systems::live_data_state_name(
                        schedulerRefresh.state),
                    state.workspace.live_scheduler_snapshot().owner_label.empty()
                        ? "no owner"
                        : state.workspace.live_scheduler_snapshot().owner_label),
                92.0f);
            gui::property_row("Engine threads",
                epochengine::format_text("{} tracked (includes waiting workers)",
                    snapshot.live_threads),
                92.0f);
            gui::property_row("CPU capacity",
                epochengine::format_text("{} logical processors",
                    snapshot.hardware_threads),
                92.0f);
            const auto& liveScheduler = state.workspace.live_scheduler_snapshot();
            if (liveScheduler.state == editor_systems::LiveDataState::available)
            {
                const auto activity = editor_tasks::activity_snapshot(liveScheduler.graph);
                gui::property_row("Background tasks",
                    epochengine::format_text("{} running | {} queued | {} waiting on dependencies",
                        activity.running_tasks, activity.queued_tasks,
                        activity.waiting_tasks),
                    92.0f);
                gui::property_row("Worker pool",
                    epochengine::format_text("{} waiting / {} owned workers",
                        activity.idle_workers, activity.worker_count),
                    92.0f);
            }
            gui::wrapped_label(
                "Tracked Engine threads are not the operating system's total thread count "
                "or an active-job count. Idle scheduler workers sleep until work arrives; "
                "they are joined when their editor context closes. Model requests and "
                "candidate processes report their own activity in AI Controls.", width);
            gui::property_row("Evidence",
                epochengine::format_text(
                    "{} / {} time samples | {} graph nodes | {} unit critical path",
                    time.samples.size(), time.capacity,
                    graph.metrics().active_nodes,
                    graph.metrics().critical_path_units),
                92.0f);
            gui::property_row("Builds",
                epochengine::format_text("project: {} | script: {}",
                    snapshot.project_build, snapshot.script_build),
                92.0f);
            if (!registrySnapshot.last_error.empty())
            {
                gui::property_row("Registry error",
                    registrySnapshot.last_error, 92.0f);
            }
            gui::wrapped_label(state.workspace.status(), width);
            gui::wrapped_label(
                "Live timing is sampled only while this workspace is visible. "
                "The learning graph is isolated and cannot mutate or stall the "
                "runtime scheduler.",
                width);
            return;
        }

        if (state.workspace.tab() == editor_systems::Tab::systems)
        {
            gui::label("System Registry");
            const auto filterEdit = gui::edit_box(
                state.text_filter,
                {(std::min)(width, 420.0f), 30.0f},
                256,
                false);
            if (filterEdit.changed)
                (void)state.workspace.set_text_filter(state.text_filter);

            const std::array actions{
                gui::InlineButtonSpec{.label = "Clear Filter", .width = 104.0f},
                gui::InlineButtonSpec{.label = "Refresh", .width = 76.0f}
            };
            if (const auto action = gui::inline_button_row(actions, 28.0f, 6.0f))
            {
                if (*action == 0u)
                {
                    state.text_filter.clear();
                    (void)state.workspace.clear_filters();
                }
                else
                {
                    (void)state.workspace.refresh_from_registry(registry);
                }
            }

            const auto& model = state.workspace.workspace();
            const auto summary = model.summary();
            gui::property_row("[systems] Visible",
                std::to_string(summary.visible_rows) + " / "
                    + std::to_string(summary.total_rows),
                126.0f);
            constexpr std::size_t maximumRows = 128u;
            const auto rows = model.visible_rows();
            const std::size_t count = (std::min)(rows.size(), maximumRows);
            for (std::size_t index = 0u; index < count; ++index)
            {
                const auto& row = rows[index];
                if (!row.row)
                    continue;
                const std::string label = row.row->label + "  |  "
                    + std::string(gui_lib::system_workspace::row_status_name(
                        row.row->status));
                if (gui::button_selected(label, {width, 28.0f}, row.selected))
                    (void)state.workspace.select(row.row->id);
            }
            if (rows.size() > count)
            {
                gui::property_row("[systems] Bounded view",
                    std::to_string(rows.size() - count) + " additional rows",
                    126.0f);
            }
            if (const auto* selected = model.selected_row())
            {
                gui::label("Selected System");
                gui::property_row("[system] Name", selected->label, 126.0f);
                gui::property_row("[system] State",
                    gui_lib::system_workspace::row_status_name(selected->status),
                    126.0f);
                gui::wrapped_label(selected->detail, width);
            }
            return;
        }

        if (state.workspace.tab() == editor_systems::Tab::live_scheduler)
        {
            const auto& scheduler = state.workspace.live_scheduler_snapshot();
            gui::label("Live Scheduler");
            if (scheduler.state != editor_systems::LiveDataState::available)
            {
                state.scheduler_surface = {};
                state.scheduler_surface_revision = 0u;
                gui::property_row("Source",
                    epochengine::format_text("{} | {}",
                        scheduler.owner_label.empty()
                            ? "no runtime owner"
                            : scheduler.owner_label,
                        editor_systems::live_data_state_name(scheduler.state)),
                    82.0f);
                gui::wrapped_label(scheduler.status, width);
                gui::wrapped_label(
                    "No scheduler activity is inferred. Attach the runtime-owned "
                    "TaskGraph to populate this read-only view.",
                    width);
                return;
            }

            const auto& graph = scheduler.graph;
            const auto activity = editor_tasks::activity_snapshot(graph);
            gui::property_row("Tasks now",
                epochengine::format_text("{} running | {} queued | {} waiting on dependencies",
                    activity.running_tasks, activity.queued_tasks, activity.waiting_tasks),
                82.0f);
            gui::property_row("Workers now",
                epochengine::format_text("{} waiting / {} owned (not running jobs)",
                    activity.idle_workers, activity.worker_count),
                82.0f);
            const std::string_view lifecycle = graph.stopped
                ? "Stopped"
                : graph.draining
                    ? "Draining"
                    : graph.accepting
                        ? "Accepting work"
                        : "Unknown";
            if (graph.nodes.empty())
            {
                state.scheduler_surface = {};
                state.scheduler_surface_revision = 0u;
                gui::label("No captured tasks");
                gui::wrapped_label(
                    "The runtime scheduler is available, but this snapshot "
                    "contains zero captured nodes. No topology is inferred.",
                    width);
                gui::property_row("Source",
                    epochengine::format_text("{} | {} | {} workers",
                        scheduler.owner_label.empty()
                            ? "unknown owner"
                            : scheduler.owner_label,
                        lifecycle, graph.workerCount),
                    82.0f);
                gui::property_row("Snapshot",
                    epochengine::format_text(
                        "{} total nodes | {} total edges | {} nodes omitted | "
                        "{} edges omitted",
                        graph.nodeCount, graph.edgeCount,
                        graph.omittedNodeCount, graph.omittedEdgeCount),
                    82.0f);
                gui::property_row("Flow",
                    epochengine::format_text(
                        "{} accepted | {} outstanding | {} completed",
                        graph.acceptedCount, graph.outstandingCount,
                        graph.completedCount),
                    82.0f);
                gui::wrapped_label(scheduler.status, width);
                return;
            }

            if (!state.scheduler_surface.is_valid()
                || state.scheduler_surface_revision != scheduler.view_revision)
            {
                const PixelCanvas canvas = build_live_scheduler_surface(graph);
                state.scheduler_surface = gui::register_runtime_surface(
                    "systems-live-scheduler-state-lanes",
                    std::span<const std::uint8_t>(
                        canvas.pixels.data(), canvas.pixels.size()),
                    static_cast<std::uint32_t>(canvas.width),
                    static_cast<std::uint32_t>(canvas.height));
                state.scheduler_surface_revision = scheduler.view_revision;
            }
            if (state.scheduler_surface.is_valid())
            {
                (void)gui::image_box({
                    .id = "systems.live_scheduler_graph",
                    .sprite = state.scheduler_surface,
                    .size = {width, 300.0f},
                    .source_size = {1'280.0f, 300.0f},
                    .caption = "Captured runtime topology",
                    .fit = gui::ImageFit::Contain
                });
            }
            gui::property_row("Source",
                epochengine::format_text("{} | {} | revision {} | {} workers",
                    scheduler.owner_label.empty()
                        ? "unknown owner"
                        : scheduler.owner_label,
                    lifecycle, graph.revision, graph.workerCount),
                82.0f);
            gui::property_row("Topology",
                epochengine::format_text(
                    "{} / {} nodes | {} / {} edges | {} nodes omitted | "
                    "{} edges omitted",
                    graph.nodes.size(), graph.nodeCount,
                    graph.edges.size(), graph.edgeCount,
                    graph.omittedNodeCount, graph.omittedEdgeCount),
                82.0f);
            gui::property_row("In flight",
                epochengine::format_text(
                    "{} pending | {} queued | {} running | {} outstanding",
                    graph.pendingCount, graph.queueCount,
                    graph.runningCount, graph.outstandingCount),
                82.0f);
            gui::property_row("Flow",
                epochengine::format_text(
                    "{} accepted | {} enqueued | {} completed",
                    graph.acceptedCount, graph.enqueuedCount,
                    graph.completedCount),
                82.0f);
            gui::property_row("Failures",
                epochengine::format_text(
                    "{} failed | {} cancelled | {} rejected | {} cancel requests",
                    graph.failedCount, graph.cancelledCount,
                    graph.rejectedCount, graph.cancellationRequestCount),
                82.0f);
            gui::property_row("Queue time",
                epochengine::format_text("peak {} | total {}",
                    duration_text(graph.peakQueueLatencyNanoseconds),
                    duration_text(graph.totalQueueLatencyNanoseconds)),
                82.0f);
            gui::property_row("Run time",
                epochengine::format_text("peak {} | total {}",
                    duration_text(graph.peakRunNanoseconds),
                    duration_text(graph.totalRunNanoseconds)),
                82.0f);
            gui::wrapped_label(scheduler.status, width);

            gui::label("Captured Tasks");
            const auto filterEdit = gui::edit_box(
                state.text_filter,
                {(std::min)(width, 420.0f), 30.0f},
                256,
                false);
            if (filterEdit.changed)
                (void)state.workspace.set_text_filter(state.text_filter);
            const auto& model = state.workspace.workspace();
            constexpr std::size_t maximumRows = 128u;
            const auto rows = model.visible_rows();
            const std::size_t count = (std::min)(rows.size(), maximumRows);
            for (std::size_t index = 0u; index < count; ++index)
            {
                const auto& row = rows[index];
                if (!row.row)
                    continue;
                const std::string label = row.row->label + "  |  "
                    + std::string(gui_lib::system_workspace::row_status_name(
                        row.row->status));
                const bool selected = state.workspace.selected_live_scheduler_node()
                    && row.row->id == "scheduler:"
                        + std::to_string(
                            state.workspace.selected_live_scheduler_node()->id);
                if (gui::button_selected(label, {width, 28.0f}, selected))
                {
                    const std::string_view id = row.row->id;
                    std::uint64_t nodeId = 0u;
                    const auto parsed = std::from_chars(
                        id.data() + std::string_view{"scheduler:"}.size(),
                        id.data() + id.size(), nodeId);
                    if (parsed.ec == std::errc{})
                        (void)state.workspace.select_live_scheduler_node(nodeId);
                }
            }
            if (rows.size() > count)
            {
                gui::property_row("[scheduler] Bounded view",
                    std::to_string(rows.size() - count) + " additional tasks",
                    142.0f);
            }
            if (const auto* selected =
                    state.workspace.selected_live_scheduler_node())
            {
                gui::label("Selected Task Evidence");
                gui::property_row("Task",
                    epochengine::format_text("#{} | {} | {}",
                        selected->id,
                        selected->label.empty()
                            ? "Unnamed"
                            : selected->label,
                        scheduler_state_name(selected->state)),
                    94.0f);
                gui::property_row("Dependencies",
                    epochengine::format_text(
                        "{} remaining prerequisites | {} dependents",
                        selected->remainingPrerequisites,
                        selected->dependentCount),
                    94.0f);
                gui::property_row("Queue wait",
                    selected->wasStarted
                        ? duration_text(selected->queueLatencyNanoseconds)
                        : "Not started",
                    94.0f);
                gui::property_row("Run duration",
                    selected->wasFinished && selected->wasStarted
                        ? duration_text(selected->runDurationNanoseconds)
                        : selected->wasFinished
                            ? "Not run"
                            : selected->wasStarted ? "Running" : "Not started",
                    94.0f);
                const std::string queuedTick = selected->wasQueued
                    ? std::to_string(selected->queuedNanoseconds)
                    : "not queued";
                const std::string startedTick = selected->wasStarted
                    ? std::to_string(selected->startedNanoseconds)
                    : "not started";
                const std::string finishedTick = selected->wasFinished
                    ? std::to_string(selected->finishedNanoseconds)
                    : "not finished";
                gui::property_row("Capture ticks",
                    epochengine::format_text(
                        "accepted {} | queued {} | started {} | finished {}",
                        selected->acceptedNanoseconds, queuedTick,
                        startedTick, finishedTick),
                    94.0f);
            }
            return;
        }

        if (state.workspace.tab() == editor_systems::Tab::time)
        {
            const auto& diagnostics = state.workspace.time_diagnostics();
            gui::label("Time Diagnostics");
            if (diagnostics.state != editor_systems::LiveDataState::available
                || diagnostics.samples.empty())
            {
                state.time_surface = {};
                state.time_surface_revision = 0u;
                gui::label("No timing samples");
                gui::property_row("Source",
                    editor_systems::live_data_state_name(diagnostics.state),
                    82.0f);
                gui::wrapped_label(diagnostics.status, width);
                return;
            }

            const bool timeSurfaceDue = !state.time_surface.is_valid()
                || diagnostics.revision < state.time_surface_revision
                || diagnostics.revision - state.time_surface_revision >= 6u;
            if (timeSurfaceDue)
            {
                const PixelCanvas canvas = build_time_surface(diagnostics);
                state.time_surface = gui::register_runtime_surface(
                    "systems-time-diagnostics",
                    std::span<const std::uint8_t>(
                        canvas.pixels.data(), canvas.pixels.size()),
                    static_cast<std::uint32_t>(canvas.width),
                    static_cast<std::uint32_t>(canvas.height));
                state.time_surface_revision = diagnostics.revision;
            }
            if (state.time_surface.is_valid())
            {
                (void)gui::image_box({
                    .id = "systems.time.graph",
                    .sprite = state.time_surface,
                    .size = {width, 260.0f},
                    .source_size = {1'280.0f, 260.0f},
                    .caption = "Registry and system update spans",
                    .fit = gui::ImageFit::Contain
                });
            }
            gui::property_row("Buffer",
                epochengine::format_text("{} / {} retained | {} dropped",
                    diagnostics.samples.size(), diagnostics.capacity,
                    diagnostics.dropped_samples),
                82.0f);
            gui::wrapped_label(
                "Blue is the measured registry update span, not whole-editor "
                "frame time. Gold is the sum of measured system updates. "
                "Sampling occurs only while this workspace is visible.",
                width);

            const auto& latest = diagnostics.samples.back();
            gui::label("Latest Sample");
            gui::property_row("Timing",
                epochengine::format_text(
                    "#{} | registry {} | systems {} | peak {}",
                    latest.frame_count,
                    duration_text(latest.frame_nanoseconds),
                    duration_text(latest.systems_nanoseconds),
                    duration_text(latest.peak_frame_nanoseconds)),
                82.0f);
            gui::property_row("Busiest",
                epochengine::format_text("{} | {}",
                    latest.busiest_system.empty()
                        ? "Unavailable"
                        : latest.busiest_system,
                    duration_text(latest.busiest_system_nanoseconds)),
                82.0f);

            gui::label("Recent Samples");
            constexpr std::size_t maximumSelectableSamples = 12u;
            const std::size_t first = diagnostics.samples.size()
                > maximumSelectableSamples
                ? diagnostics.samples.size() - maximumSelectableSamples
                : 0u;
            for (std::size_t index = first;
                index < diagnostics.samples.size(); ++index)
            {
                const auto& sample = diagnostics.samples[index];
                const auto* selected = state.workspace.selected_time_sample();
                const std::string label = "Registry sample "
                    + std::to_string(sample.frame_count) + "  |  "
                    + duration_text(sample.frame_nanoseconds);
                if (gui::button_selected(label, {width, 28.0f},
                        selected && selected->sequence == sample.sequence))
                {
                    (void)state.workspace.select_time_sample(sample.sequence);
                }
            }
            if (const auto* selected = state.workspace.selected_time_sample())
            {
                gui::label("Selected Sample");
                gui::property_row("Identity",
                    epochengine::format_text(
                        "sequence {} | registry revision {}",
                        selected->sequence,
                        selected->registry_revision),
                    82.0f);
                gui::property_row("Timing",
                    epochengine::format_text(
                        "registry {} | systems {} | busiest {}",
                        duration_text(selected->frame_nanoseconds),
                        duration_text(selected->systems_nanoseconds),
                        selected->busiest_system.empty()
                            ? "Unavailable"
                            : selected->busiest_system),
                    82.0f);
            }
            return;
        }

        if (state.workspace.tab() == editor_systems::Tab::learning_graph)
        {
            const auto graph = state.workspace.learning_diagnostics();
            const auto simulation = state.workspace.simulate_learning_graph();
            const auto tasks = state.workspace.learning_tasks();
            for (auto offset = state.task_node_offsets.begin();
                 offset != state.task_node_offsets.end();)
            {
                const bool retained = std::ranges::any_of(
                    tasks,
                    [&](const auto& task)
                    {
                        return graph_node_id(task.handle) == offset->first;
                    });
                if (retained)
                    ++offset;
                else
                    offset = state.task_node_offsets.erase(offset);
            }

            gui::label("Learning Task Graph");
            gui::wrapped_label(
                "Editable temporal document with semantic history, isolated "
                "from the live scheduler.",
                width);
            const std::array viewActions{
                gui::InlineButtonSpec{
                    .label = "<", .width = 34.0f, .enabled = !tasks.empty()},
                gui::InlineButtonSpec{
                    .label = "-", .width = 34.0f, .enabled = !tasks.empty()},
                gui::InlineButtonSpec{
                    .label = "+", .width = 34.0f, .enabled = !tasks.empty()},
                gui::InlineButtonSpec{
                    .label = ">", .width = 34.0f, .enabled = !tasks.empty()},
                gui::InlineButtonSpec{
                    .label = "1:1", .width = 46.0f, .enabled = !tasks.empty()}
            };
            if (const auto action = gui::inline_button_row(
                    viewActions, 28.0f, 4.0f))
            {
                switch (*action)
                {
                case 0u:
                    state.task_pan = (std::max)(0, state.task_pan - 64);
                    break;
                case 1u:
                    state.task_zoom = (std::max)(
                        0.75f, state.task_zoom - 0.20f);
                    break;
                case 2u:
                    state.task_zoom = (std::min)(
                        2.5f, state.task_zoom + 0.20f);
                    break;
                case 3u:
                    state.task_pan += 64;
                    break;
                case 4u:
                    state.task_pan = 0;
                    state.task_zoom = 1.0f;
                    state.task_node_offsets.clear();
                    break;
                default:
                    break;
                }
                state.invalidate_graph_surface();
            }

            if (tasks.empty())
            {
                state.task_surface = {};
                state.task_surface_revision = graph.revision().sequence;
                gui::wrapped_label(
                    "No learning tasks. Add a task below to begin a graph.",
                    width);
            }
            else if (!state.task_surface.is_valid()
                || state.task_surface_revision != graph.revision().sequence)
            {
                const PixelCanvas canvas = build_learning_graph_surface(
                    graph, state.task_zoom, state.task_pan,
                    state.selected_task, state.task_node_offsets);
                state.task_surface = gui::register_runtime_surface(
                    "systems-learning-task-graph",
                    std::span<const std::uint8_t>(
                        canvas.pixels.data(), canvas.pixels.size()),
                    static_cast<std::uint32_t>(canvas.width),
                    static_cast<std::uint32_t>(canvas.height));
                state.task_surface_revision = graph.revision().sequence;
            }

            if (state.task_surface.is_valid())
            {
                namespace graph_gui =
                    gui_lib::node_graph_workspace;
                const gui::Vec2 mouse = gui::mouse_position();
                const bool leftPressed = gui::was_mouse_pressed();
                const bool leftReleased = gui::was_mouse_released();
                const bool rightPressed = gui::was_mouse_right_pressed();
                const gui::ImageBoxResult graphImage = gui::image_box({
                    .id = "systems.learning_graph.canvas",
                    .sprite = state.task_surface,
                    .size = {width, 300.0f},
                    .source_size = {1'280.0f, 300.0f},
                    .fit = gui::ImageFit::Contain,
                    .interactive = true,
                    .selected = static_cast<bool>(state.selected_task)
                });
                const bool graphWorkspaceReady =
                    prepare_learning_graph_workspace(
                        state.task_workspace,
                        graph,
                        state.task_zoom,
                        state.task_pan,
                        graphImage.image_bounds,
                        state.task_node_offsets);

                const auto taskForNodeId =
                    [&](graph_gui::NodeId id)
                        -> std::optional<
                            authoring::task_graph::NodeHandle>
                {
                    const auto found = std::ranges::find_if(
                        tasks,
                        [&](const auto& task)
                        {
                            return graph_node_id(task.handle) == id;
                        });
                    return found == tasks.end()
                        ? std::nullopt
                        : std::optional<
                            authoring::task_graph::NodeHandle>{
                                found->handle};
                };
                const auto taskForPinId =
                    [&](graph_gui::PinId id)
                        -> std::optional<
                            authoring::task_graph::NodeHandle>
                {
                    const auto pins = state.task_workspace.pins();
                    const auto found = std::ranges::find(
                        pins, id, &graph_gui::PinLayout::id);
                    return found == pins.end()
                        ? std::nullopt
                        : taskForNodeId(found->node_id);
                };
                const auto synchronizeSelection = [&]()
                {
                    const auto selectedIds =
                        state.task_workspace.selected_node_ids();
                    if (selectedIds.empty())
                    {
                        state.selected_task = {};
                        return;
                    }
                    const auto selected = taskForNodeId(
                        selectedIds.front());
                    if (!selected)
                        return;
                    state.selected_task = *selected;
                    const auto found = std::ranges::find(
                        tasks,
                        *selected,
                        &authoring::task_graph::NodeView::handle);
                    if (found == tasks.end())
                        return;
                    state.selected_task_name =
                        found->descriptor.name;
                    state.selected_task_category =
                        found->descriptor.category;
                    state.selected_task_cost = std::to_string(
                        found->descriptor.estimated_cost_units);
                };
                const auto commitIntent =
                    [&](const graph_gui::WorkspaceIntent& intent)
                {
                    if (intent.phase
                        != graph_gui::IntentPhase::commit)
                    {
                        return;
                    }

                    if (intent.kind
                        == graph_gui::IntentKind::move_nodes)
                    {
                        for (const graph_gui::NodeId id
                            : intent.node_ids)
                        {
                            auto& offset =
                                state.task_node_offsets[id];
                            offset.x = std::clamp(
                                offset.x + intent.world_delta.x,
                                -4'000.0,
                                4'000.0);
                            offset.y = std::clamp(
                                offset.y + intent.world_delta.y,
                                -4'000.0,
                                4'000.0);
                        }
                        state.interaction_status =
                            "Graph node layout updated.";
                        state.invalidate_graph_surface();
                        return;
                    }

                    if (intent.kind
                        == graph_gui::IntentKind::connect_pins)
                    {
                        if (!intent.valid_target)
                        {
                            state.interaction_status =
                                "Connection cancelled: choose an "
                                "output and a different input pin.";
                            return;
                        }
                        const auto prerequisite = taskForPinId(
                            intent.output_pin_id);
                        const auto dependent = taskForPinId(
                            intent.input_pin_id);
                        if (!prerequisite || !dependent)
                        {
                            state.interaction_status =
                                "Connection rejected: pin ownership "
                                "is stale.";
                            return;
                        }
                        const auto operation =
                            state.workspace.connect_learning_tasks(
                                *prerequisite,
                                *dependent);
                        state.interaction_status = operation
                            ? "Dependency connected as one semantic "
                                "graph operation."
                            : std::string{
                                "Dependency rejected: "}
                                + std::string(
                                    authoring::task_graph::
                                        result_code_name(
                                            operation.result.graph_code));
                        state.invalidate_graph_surface();
                        return;
                    }

                    if (intent.kind
                        != graph_gui::IntentKind::disconnect_edges)
                    {
                        return;
                    }

                    bool changed = false;
                    std::string rejection{};
                    for (const graph_gui::EdgeId id
                        : intent.edge_ids)
                    {
                        const auto edges = graph.edges();
                        const auto found = std::ranges::find_if(
                            edges,
                            [&](const auto& edge)
                            {
                                return graph_edge_id(
                                    edge.prerequisite,
                                    edge.dependent) == id;
                            });
                        if (found == edges.end())
                        {
                            rejection =
                                "edge ownership is stale";
                            continue;
                        }
                        const auto operation =
                            state.workspace.
                                disconnect_learning_tasks(
                                    found->prerequisite,
                                    found->dependent);
                        changed = changed
                            || static_cast<bool>(operation);
                        if (!operation)
                        {
                            rejection = std::string(
                                authoring::task_graph::
                                    result_code_name(
                                        operation.result.graph_code));
                        }
                    }
                    state.interaction_status = changed
                        ? "Dependency disconnected as one semantic "
                            "graph operation."
                        : std::string{"Disconnect rejected: "}
                            + (rejection.empty()
                                ? "no selected edge"
                                : rejection);
                    state.invalidate_graph_surface();
                };

                if (graphWorkspaceReady
                    && rightPressed
                    && graphImage.hovered)
                {
                    const auto pointer =
                        state.task_workspace.pointer_down({
                            .screen_position = {mouse.x, mouse.y},
                            .button =
                                graph_gui::PointerButton::secondary,
                            .disconnect_gesture = true
                        });
                    commitIntent(pointer.intent);
                }
                if (graphWorkspaceReady
                    && leftPressed
                    && graphImage.hovered)
                {
                    const auto pointer =
                        state.task_workspace.pointer_down({
                            .screen_position = {mouse.x, mouse.y}
                        });
                    if (!pointer.accepted)
                    {
                        state.interaction_status =
                            std::string{"Graph pointer rejected: "}
                            + std::string(
                                graph_gui::error_code_name(
                                    pointer.error));
                    }
                    synchronizeSelection();
                    state.invalidate_graph_surface();
                }
                if (graphWorkspaceReady
                    && state.task_workspace.pointer_active()
                    && gui::is_mouse_down())
                {
                    (void)state.task_workspace.pointer_move(
                        {mouse.x, mouse.y});
                }
                if (graphWorkspaceReady
                    && state.task_workspace.pointer_active()
                    && leftReleased)
                {
                    const auto pointer =
                        state.task_workspace.pointer_up(
                            {mouse.x, mouse.y});
                    commitIntent(pointer.intent);
                    synchronizeSelection();
                    state.invalidate_graph_surface();
                }
            }
            gui::property_row("Topology",
                epochengine::format_text(
                    "{} nodes | {} edges | {} waves | max width {}",
                    graph.metrics().active_nodes, graph.metrics().edges,
                    graph.metrics().parallel_waves,
                    graph.metrics().maximum_parallel_width),
                82.0f);
            gui::property_row("Simulation",
                epochengine::format_text(
                    "{} total work | {} barrier | {} critical path",
                    simulation.total_work_units,
                    simulation.barrier_elapsed_units,
                    graph.metrics().critical_path_units),
                82.0f);
            gui::property_row("History",
                epochengine::format_text("{} applied / {} retained",
                    graph.metrics().applied_operations,
                    graph.metrics().retained_operations),
                82.0f);

            gui::label("Tasks");
            constexpr std::size_t maximumNodeRows = 32u;
            for (std::size_t index = 0u;
                 index < (std::min)(tasks.size(), maximumNodeRows);
                 ++index)
            {
                const auto& task = tasks[index];
                const auto diagnostic = std::find_if(
                    graph.nodes().begin(),
                    graph.nodes().end(),
                    [&](const auto& node)
                    {
                        return node.handle == task.handle;
                    });
                const std::uint32_t wave = diagnostic
                        == graph.nodes().end()
                    ? 0u
                    : diagnostic->parallel_wave;
                const bool critical = diagnostic
                        != graph.nodes().end()
                    && diagnostic->on_critical_path;
                const std::string nodeLabel = epochengine::format_text(
                    "{}  |  {}  |  wave {}  |  {} units{}",
                    task.descriptor.name,
                    task.descriptor.category,
                    wave,
                    task.descriptor.estimated_cost_units,
                    critical ? "  |  critical" : "");
                if (gui::button_selected(
                        nodeLabel,
                        {width, 27.0f},
                        task.handle == state.selected_task))
                {
                    state.selected_task = task.handle;
                    state.selected_task_name = task.descriptor.name;
                    state.selected_task_category =
                        task.descriptor.category;
                    state.selected_task_cost = std::to_string(
                        task.descriptor.estimated_cost_units);
                    state.interaction_status =
                        "Selected graph node for editing.";
                    state.invalidate_graph_surface();
                }
            }
            if (tasks.size() > maximumNodeRows)
            {
                gui::property_row(
                    "[graph] Additional nodes",
                    std::to_string(tasks.size() - maximumNodeRows),
                    150.0f);
            }



            if (state.selected_task)
            {
                const auto selected = std::find_if(
                    tasks.begin(),
                    tasks.end(),
                    [&](const auto& task)
                    {
                        return task.handle == state.selected_task;
                    });
                if (selected == tasks.end())
                {
                    state.selected_task = {};
                    state.selected_task_name.clear();
                    state.selected_task_category.clear();
                    state.selected_task_cost.clear();
                }
                else
                {
                    gui::label("Selected Task");
                    gui::property_row(
                        "[node] Identity",
                        epochengine::format_text(
                            "{}:{}",
                            selected->handle.index,
                            selected->handle.generation),
                        116.0f);
                    (void)gui::edit_box(
                        state.selected_task_name,
                        {(std::min)(width, 340.0f), 28.0f},
                        128u,
                        false);
                    (void)gui::edit_box(
                        state.selected_task_category,
                        {(std::min)(width, 240.0f), 28.0f},
                        64u,
                        false);
                    (void)gui::edit_box(
                        state.selected_task_cost,
                        {140.0f, 28.0f},
                        12u,
                        false);
                    if (gui::button("Apply Task Changes", {176.0f, 28.0f}))
                    {
                        std::uint64_t cost = 0u;
                        const auto parsed = std::from_chars(
                            state.selected_task_cost.data(),
                            state.selected_task_cost.data()
                                + state.selected_task_cost.size(),
                            cost);
                        if (parsed.ec != std::errc{}
                            || parsed.ptr != state.selected_task_cost.data()
                                + state.selected_task_cost.size()
                            || cost == 0u)
                        {
                            state.interaction_status =
                                "Task cost must be a positive whole number.";
                        }
                        else
                        {
                            const auto changed =
                                state.workspace.rename_learning_task(
                                    state.selected_task,
                                    state.selected_task_name,
                                    state.selected_task_category,
                                    cost);
                            state.interaction_status = changed
                                ? "Task descriptor committed as one semantic operation."
                                : std::string{"Task edit rejected: "}
                                    + std::string(
                                        authoring::task_graph::result_code_name(
                                            changed.result.graph_code));
                            state.invalidate_graph_surface();
                        }
                    }
                }
            }
            gui::label("Add Task");
            (void)gui::edit_box(state.new_task_name,
                {(std::min)(width, 340.0f), 28.0f}, 128, false);
            (void)gui::edit_box(state.new_task_category,
                {(std::min)(width, 240.0f), 28.0f}, 64, false);
            (void)gui::edit_box(state.new_task_cost, {140.0f, 28.0f}, 12, false);
            if (gui::button("Add Learning Task", {190.0f, 30.0f}))
            {
                std::uint64_t cost = 0u;
                const auto parsed = std::from_chars(
                    state.new_task_cost.data(),
                    state.new_task_cost.data() + state.new_task_cost.size(),
                    cost);
                if (parsed.ec != std::errc{}
                    || parsed.ptr != state.new_task_cost.data()
                        + state.new_task_cost.size()
                    || cost == 0u)
                {
                    state.interaction_status =
                        "Task cost must be a positive whole number.";
                }
                else
                {
                    const auto added = state.workspace.add_learning_task(
                        state.new_task_name,
                        state.new_task_category,
                        cost);
                    state.interaction_status = added
                        ? "Task added as a semantic graph operation."
                        : std::string{"Task rejected: "}
                            + std::string(authoring::task_graph::result_code_name(
                                added.result.graph_code));
                    state.invalidate_graph_surface();
                }
            }

            if (!tasks.empty())
            {
                std::vector<std::string> labels{};
                std::vector<std::string_view> views{};
                labels.reserve(tasks.size());
                views.reserve(tasks.size());
                for (const auto& task : tasks)
                    labels.push_back(task.descriptor.name);
                for (const auto& label : labels)
                    views.push_back(label);
                state.prerequisite_index = (std::min)(
                    state.prerequisite_index, tasks.size() - 1u);
                state.dependent_index = (std::min)(
                    state.dependent_index, tasks.size() - 1u);

                const auto prerequisite = gui::select_box(gui::SelectBoxOptions{
                    .id = "systems-learning-prerequisite",
                    .placeholder = "Prerequisite task",
                    .selected = views[state.prerequisite_index],
                    .options = std::span<const std::string_view>{
                        views.data(), views.size()},
                    .size = {(std::min)(width, 320.0f), 30.0f},
                    .row_height = 28.0f,
                    .max_visible_options = 8
                });
                if (prerequisite.changed && prerequisite.selected_index)
                    state.prerequisite_index = *prerequisite.selected_index;
                const auto dependent = gui::select_box(gui::SelectBoxOptions{
                    .id = "systems-learning-dependent",
                    .placeholder = "Dependent task",
                    .selected = views[state.dependent_index],
                    .options = std::span<const std::string_view>{
                        views.data(), views.size()},
                    .size = {(std::min)(width, 320.0f), 30.0f},
                    .row_height = 28.0f,
                    .max_visible_options = 8
                });
                if (dependent.changed && dependent.selected_index)
                    state.dependent_index = *dependent.selected_index;

                const bool distinctEndpoints =
                    state.prerequisite_index
                        != state.dependent_index;
                const std::array edgeActions{
                    gui::InlineButtonSpec{
                        .label = "Connect",
                        .width = 90.0f,
                        .enabled = distinctEndpoints},
                    gui::InlineButtonSpec{
                        .label = "Disconnect",
                        .width = 100.0f,
                        .enabled = distinctEndpoints},
                    gui::InlineButtonSpec{
                        .label = "Remove Selected",
                        .width = 126.0f,
                        .enabled = static_cast<bool>(
                            state.selected_task)}
                };
                if (const auto action = gui::inline_button_row(
                        edgeActions, 28.0f, 6.0f))
                {
                    editor_systems::ControllerResult result{};
                    if (*action == 0u)
                    {
                        result = state.workspace.connect_learning_tasks(
                            tasks[state.prerequisite_index].handle,
                            tasks[state.dependent_index].handle).result;
                    }
                    else if (*action == 1u)
                    {
                        result = state.workspace.disconnect_learning_tasks(
                            tasks[state.prerequisite_index].handle,
                            tasks[state.dependent_index].handle).result;
                    }
                    else if (state.selected_task)
                    {
                        const auto removed = state.selected_task;
                        result = state.workspace.remove_learning_task(
                            removed).result;
                        if (result)
                        {
                            state.task_node_offsets.erase(
                                graph_node_id(removed));
                            state.selected_task = {};
                        }
                    }
                    state.interaction_status = result
                        ? "Graph operation committed."
                        : std::string{"Graph operation rejected: "}
                            + std::string(authoring::task_graph::result_code_name(
                                result.graph_code));
                    state.invalidate_graph_surface();
                }

                const std::array historyActions{
                    gui::InlineButtonSpec{
                        .label = "Undo",
                        .width = 72.0f,
                        .enabled =
                            state.workspace.can_undo_learning_graph()},
                    gui::InlineButtonSpec{
                        .label = "Redo",
                        .width = 72.0f,
                        .enabled =
                            state.workspace.can_redo_learning_graph()},
                    gui::InlineButtonSpec{
                        .label = "Reset Graph",
                        .width = 106.0f,
                        .enabled = !tasks.empty()}
                };
                if (const auto action = gui::inline_button_row(
                        historyActions, 28.0f, 6.0f))
                {
                    editor_systems::ControllerResult result{};
                    if (*action == 0u)
                        result = state.workspace.undo_learning_graph().result;
                    else if (*action == 1u)
                        result = state.workspace.redo_learning_graph().result;
                    else
                    {
                        result = state.workspace.reset_learning_graph();
                        state.selected_task = {};
                        state.task_node_offsets.clear();
                    }
                    state.interaction_status = result
                        ? "Graph history operation committed."
                        : std::string{"Graph operation rejected: "}
                            + std::string(authoring::task_graph::result_code_name(
                                result.graph_code));
                    state.invalidate_graph_surface();
                }
            }
            gui::wrapped_label(state.interaction_status, width);
            return;
        }

        gui::label("Renderer Evidence");
        gui::property_row("Runtime",
            epochengine::format_text("{} | {}",
                snapshot.renderer, snapshot.capability),
            94.0f);
        gui::property_row("Admission",
            epochengine::format_text("editor: {} | project: {}",
                snapshot.editor_admission, snapshot.project_admission),
            94.0f);
        gui::property_row("Frame graph",
            epochengine::format_text("{} | {}",
                snapshot.resource_spine, snapshot.proof_stages),
            94.0f);
        gui::property_row("Presentation",
            epochengine::format_text("overall: {} | output: {}",
                snapshot.rtt_overall, snapshot.rtt_presentation),
            94.0f);
        gui::property_row("Geometry", snapshot.mesh_model, 94.0f);
        gui::property_row("Next gate", snapshot.next_gate, 94.0f);
        gui::wrapped_label(snapshot.backend_guidance, width);
        gui::wrapped_label(snapshot.convergence_focus, width);
    }
}
