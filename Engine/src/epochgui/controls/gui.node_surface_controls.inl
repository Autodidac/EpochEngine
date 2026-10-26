// Epoch feature implementation; included by its owning module only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    NodeGraphCanvasResult node_graph_canvas(
        const NodeGraphCanvasOptions& options) noexcept
    {
        NodeGraphCanvasResult result{};
        if (!g_frame.insideWindow || !g_frame.ctx || options.id.empty()
            || options.size.x < 1.0f || options.size.y < 1.0f
            || options.nodes.size() > 4'096u
            || options.edges.size() > 16'384u)
        {
            return result;
        }

        const Vec2 origin = g_frame.cursor;
        const float width = options.size.x;
        const float height = options.size.y;
        result.bounds = {origin, options.size};
        result.hovered = options.enabled
            && point_in_rect(
                g_frame.mousePos, origin.x, origin.y, width, height)
            && point_in_active_clip(g_frame.mousePos);
        namespace graph_workspace = gui_lib::node_graph_workspace;
        auto& graphState = g_nodeGraphCanvasStates[widget_state_key(options.id)];
        std::erase_if(
            graphState.nodeOffsets,
            [&](const auto& offset)
            {
                return std::ranges::none_of(
                    options.nodes,
                    [&](const NodeGraphCanvasNode& node)
                    {
                        return node.id == offset.first;
                    });
            });

        std::vector<graph_workspace::NodeLayout> graphNodes{};
        std::vector<graph_workspace::PinLayout> graphPins{};
        std::vector<graph_workspace::EdgeLayout> graphEdges{};
        std::unordered_map<std::uint64_t, std::size_t> nodeIndices{};
        graphNodes.reserve(options.nodes.size());
        graphPins.reserve(options.nodes.size() * 2u);
        graphEdges.reserve(options.edges.size());
        nodeIndices.reserve(options.nodes.size());

        std::uint64_t sourceRevision = 1'469'598'103'934'665'603ull;
        const auto hashValue = [&](std::uint64_t value) noexcept
        {
            sourceRevision ^= value;
            sourceRevision *= 1'099'511'628'211ull;
        };
        for (std::size_t index = 0u; index < options.nodes.size(); ++index)
        {
            const NodeGraphCanvasNode& node = options.nodes[index];
            if (node.id == graph_workspace::invalid_node_id
                || nodeIndices.contains(node.id))
            {
                result.valid = false;
                return result;
            }
            nodeIndices.emplace(node.id, index);
            const auto offset = graphState.nodeOffsets.contains(node.id)
                ? graphState.nodeOffsets.at(node.id)
                : graph_workspace::Vec2{};
            const double nodeWidth = (std::max)(64.0, static_cast<double>(node.size.x));
            const double nodeHeight = (std::max)(36.0, static_cast<double>(node.size.y));
            const graph_workspace::Rect bounds{
                static_cast<double>(node.position.x) + offset.x,
                static_cast<double>(node.position.y) + offset.y,
                nodeWidth,
                nodeHeight};
            graphNodes.push_back({
                .id = node.id,
                .bounds = bounds,
                .layout_order = index,
                .selectable = node.enabled});
            const graph_workspace::PinId inputPin =
                static_cast<graph_workspace::PinId>(index * 2u + 1u);
            const graph_workspace::PinId outputPin = inputPin + 1u;
            graphPins.push_back({
                .id = inputPin,
                .node_id = node.id,
                .direction = graph_workspace::PinDirection::input,
                .position = {bounds.x, bounds.y + bounds.height * 0.5},
                .layout_order = index * 2u,
                .connectable = options.allow_connections && node.enabled});
            graphPins.push_back({
                .id = outputPin,
                .node_id = node.id,
                .direction = graph_workspace::PinDirection::output,
                .position = {bounds.x + bounds.width, bounds.y + bounds.height * 0.5},
                .layout_order = index * 2u + 1u,
                .connectable = options.allow_connections && node.enabled});
            hashValue(node.id);
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.x * 1'000.0)));
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.y * 1'000.0)));
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.width * 1'000.0)));
            hashValue(static_cast<std::uint64_t>(std::llround(bounds.height * 1'000.0)));
        }
        for (std::size_t index = 0u; index < options.edges.size(); ++index)
        {
            const NodeGraphCanvasEdge& edge = options.edges[index];
            const auto sourceNode = nodeIndices.find(edge.source_node);
            const auto targetNode = nodeIndices.find(edge.target_node);
            if (!edge.enabled || edge.id == graph_workspace::invalid_edge_id
                || sourceNode == nodeIndices.end()
                || targetNode == nodeIndices.end())
            {
                continue;
            }
            graphEdges.push_back({
                .id = edge.id,
                .output_pin_id = static_cast<graph_workspace::PinId>(
                    sourceNode->second * 2u + 2u),
                .input_pin_id = static_cast<graph_workspace::PinId>(
                    targetNode->second * 2u + 1u),
                .layout_order = index,
                .selectable = edge.enabled});
            hashValue(edge.id);
            hashValue(edge.source_node);
            hashValue(edge.target_node);
        }
        if (sourceRevision == 0u)
            sourceRevision = 1u;

        const auto replaced = graphState.controller.replace_graph({
            .nodes = graphNodes,
            .pins = graphPins,
            .edges = graphEdges,
            .source_revision = sourceRevision});
        const auto viewportSet = graphState.controller.set_viewport({
            origin.x, origin.y, width, height});
        if (!replaced.committed || !viewportSet.accepted)
        {
            result.valid = false;
            return result;
        }

        std::uint64_t externalSelectionSignature = 1u;
        for (const NodeGraphCanvasNode& node : options.nodes)
        {
            if (node.selected)
            {
                externalSelectionSignature ^= node.id;
                externalSelectionSignature *= 1'099'511'628'211ull;
            }
        }
        if (externalSelectionSignature != graphState.externalSelectionSignature)
        {
            (void)graphState.controller.clear_selection();
            bool first = true;
            for (const NodeGraphCanvasNode& node : options.nodes)
            {
                if (!node.selected)
                    continue;
                (void)graphState.controller.select_node(
                    node.id,
                    first ? graph_workspace::SelectionMode::replace
                          : graph_workspace::SelectionMode::add);
                first = false;
            }
            graphState.externalSelectionSignature = externalSelectionSignature;
        }
        if (!graphState.initialized || options.reset_view)
        {
            (void)graphState.controller.set_view({});
            graphState.initialized = true;
            result.view_changed = true;
        }
        if (options.fit_to_content && !options.nodes.empty())
        {
            const auto fitted = graphState.controller.fit_to_content(28.0);
            result.view_changed = result.view_changed || fitted.changed;
        }
        if (result.hovered && g_frame.mouseWheelDelta != 0)
        {
            const double wheelSteps =
                static_cast<double>(g_frame.mouseWheelDelta) / 120.0;
            const auto zoomed = graphState.controller.zoom_at(
                {g_frame.mousePos.x, g_frame.mousePos.y},
                wheelSteps);
            result.view_changed = result.view_changed || zoomed.changed;
            g_frame.mouseWheelDelta = 0;
        }
        result.valid = true;

        const auto& palette = active_palette();
        draw_sprite(
            palette.panelBackground,
            origin.x, origin.y, width, height);
        ContentClipScope graphClip{
            origin,
            {origin.x + width, origin.y + height}};
        const auto graphView = graphState.controller.view();
        const float gridStep = (std::clamp)(
            options.grid_step * static_cast<float>(graphView.zoom),
            12.0f,
            96.0f);
        float gridOffsetX = std::fmod(
            static_cast<float>(graphView.pan.x), gridStep);
        float gridOffsetY = std::fmod(
            static_cast<float>(graphView.pan.y), gridStep);
        if (gridOffsetX < 0.0f)
            gridOffsetX += gridStep;
        if (gridOffsetY < 0.0f)
            gridOffsetY += gridStep;
        for (float x = gridOffsetX; x < width; x += gridStep)
        {
            draw_sprite(
                palette.titleBar,
                origin.x + x, origin.y, 1.0f, height);
        }
        for (float y = gridOffsetY; y < height; y += gridStep)
        {
            draw_sprite(
                palette.titleBar,
                origin.x, origin.y + y, width, 1.0f);
        }

        std::unordered_map<std::uint64_t,
            const graph_workspace::NodeProjection*> projectedNodes{};
        projectedNodes.reserve(options.nodes.size());
        for (const graph_workspace::NodeProjection& projection
             : graphState.controller.node_projections())
        {
            projectedNodes.emplace(projection.id, &projection);
        }
        std::unordered_map<std::uint64_t, Vec2> projectedPins{};
        projectedPins.reserve(graphPins.size());
        for (const graph_workspace::PinProjection& projection
             : graphState.controller.pin_projections())
        {
            projectedPins.emplace(
                projection.id,
                Vec2{
                    static_cast<float>(projection.screen_position.x),
                    static_cast<float>(projection.screen_position.y)});
        }
        const auto drawHorizontal = [&](float first, float second, float y,
            const SpriteHandle& sprite)
        {
            const float x = (std::min)(first, second);
            draw_sprite(
                sprite,
                x, y - 1.0f,
                (std::max)(2.0f, std::abs(second - first)),
                2.0f);
        };
        const auto drawVertical = [&](float x, float first, float second,
            const SpriteHandle& sprite)
        {
            const float y = (std::min)(first, second);
            draw_sprite(
                sprite,
                x - 1.0f, y,
                2.0f,
                (std::max)(2.0f, std::abs(second - first)));
        };
        const auto edgeScreenPositions = [&](
            const NodeGraphCanvasEdge& edge)
            -> std::optional<std::pair<Vec2, Vec2>>
        {
            const auto sourceNode = nodeIndices.find(edge.source_node);
            const auto targetNode = nodeIndices.find(edge.target_node);
            if (sourceNode == nodeIndices.end()
                || targetNode == nodeIndices.end())
            {
                return std::nullopt;
            }
            const std::uint64_t outputPin = sourceNode->second * 2u + 2u;
            const std::uint64_t inputPin = targetNode->second * 2u + 1u;
            const auto source = projectedPins.find(outputPin);
            const auto target = projectedPins.find(inputPin);
            if (source == projectedPins.end() || target == projectedPins.end())
                return std::nullopt;
            return std::pair{source->second, target->second};
        };
        const auto edgeHovered = [&](const NodeGraphCanvasEdge& edge)
        {
            if (!result.hovered || !edge.enabled)
                return false;
            const auto positions = edgeScreenPositions(edge);
            if (!positions)
                return false;
            const auto [source, target] = *positions;
            const float middleX = (source.x + target.x) * 0.5f;
            constexpr float tolerance = 6.0f;
            const auto nearHorizontal = [&](float first, float second, float y)
            {
                return g_frame.mousePos.x >= (std::min)(first, second) - tolerance
                    && g_frame.mousePos.x <= (std::max)(first, second) + tolerance
                    && std::abs(g_frame.mousePos.y - y) <= tolerance;
            };
            const auto nearVertical = [&](float x, float first, float second)
            {
                return g_frame.mousePos.y >= (std::min)(first, second) - tolerance
                    && g_frame.mousePos.y <= (std::max)(first, second) + tolerance
                    && std::abs(g_frame.mousePos.x - x) <= tolerance;
            };
            return nearHorizontal(source.x, middleX, source.y)
                || nearVertical(middleX, source.y, target.y)
                || nearHorizontal(middleX, target.x, target.y);
        };

        std::optional<std::uint64_t> hoveredEdge{};
        for (const NodeGraphCanvasEdge& edge : options.edges)
        {
            if (!edge.enabled)
                continue;
            const auto positions = edgeScreenPositions(edge);
            if (!positions)
                continue;
            const auto [source, target] = *positions;
            const float middleX = (source.x + target.x) * 0.5f;
            const bool hovered = edgeHovered(edge);
            const SpriteHandle lineSprite = edge.selected || hovered
                ? palette.textFieldActive
                : palette.buttonActive;
            drawHorizontal(source.x, middleX, source.y, lineSprite);
            drawVertical(middleX, source.y, target.y, lineSprite);
            drawHorizontal(middleX, target.x, target.y, lineSprite);
            draw_sprite(
                lineSprite,
                target.x - 4.0f, target.y - 4.0f,
                8.0f, 8.0f);
            if (hovered)
                hoveredEdge = edge.id;
        }

        std::optional<std::uint64_t> hoveredNode{};
        for (const NodeGraphCanvasNode& node : options.nodes)
        {
            const auto projected = projectedNodes.find(node.id);
            if (projected == projectedNodes.end())
                continue;
            const auto& projection = *projected->second;
            const Vec2 nodePosition{
                static_cast<float>(projection.screen_bounds.x),
                static_cast<float>(projection.screen_bounds.y)};
            const float nodeWidth = static_cast<float>(
                projection.screen_bounds.width);
            const float nodeHeight = static_cast<float>(
                projection.screen_bounds.height);
            const bool hovered = options.enabled && node.enabled
                && point_in_rect(
                    g_frame.mousePos,
                    nodePosition.x,
                    nodePosition.y,
                    nodeWidth,
                    nodeHeight)
                && point_in_active_clip(g_frame.mousePos);
            SpriteHandle fill = palette.textField;
            switch (node.role)
            {
            case NodeGraphNodeRole::document:
                fill = palette.textFieldActive;
                break;
            case NodeGraphNodeRole::container:
                fill = palette.buttonNormal;
                break;
            case NodeGraphNodeRole::action:
                fill = palette.buttonActive;
                break;
            case NodeGraphNodeRole::control:
            default:
                break;
            }
            if (!node.enabled)
                fill = palette.panelBackground;
            else if (hovered)
                fill = palette.buttonHover;
            draw_sprite(
                fill,
                nodePosition.x,
                nodePosition.y,
                nodeWidth,
                nodeHeight);

            const bool selected = node.selected || projection.selected;
            const SpriteHandle border = selected
                ? palette.textFieldActive
                : palette.titleBar;
            const float borderWidth = selected ? 3.0f : 2.0f;
            draw_sprite(border, nodePosition.x, nodePosition.y,
                nodeWidth, borderWidth);
            draw_sprite(border, nodePosition.x,
                nodePosition.y + nodeHeight - borderWidth,
                nodeWidth, borderWidth);
            draw_sprite(border, nodePosition.x, nodePosition.y,
                borderWidth, nodeHeight);
            draw_sprite(border,
                nodePosition.x + nodeWidth - borderWidth,
                nodePosition.y, borderWidth, nodeHeight);
            draw_sprite(
                palette.textFieldActive,
                nodePosition.x - 4.0f,
                nodePosition.y + nodeHeight * 0.5f - 4.0f,
                8.0f, 8.0f);
            draw_sprite(
                palette.buttonActive,
                nodePosition.x + nodeWidth - 4.0f,
                nodePosition.y + nodeHeight * 0.5f - 4.0f,
                8.0f, 8.0f);

            const std::string title = fit_text_to_width(
                node.title,
                (std::max)(1.0f, nodeWidth - 18.0f),
                kFontScale);
            draw_text_line(
                title.empty() ? node.title : std::string_view{title},
                nodePosition.x + 9.0f,
                nodePosition.y + 6.0f,
                kFontScale);
            if (!node.subtitle.empty() && nodeHeight >= 44.0f)
            {
                const std::string subtitle = fit_text_to_width(
                    node.subtitle,
                    (std::max)(1.0f, nodeWidth - 18.0f),
                    kFontScale * 0.82f);
                draw_text_line(
                    subtitle.empty()
                        ? node.subtitle
                        : std::string_view{subtitle},
                    nodePosition.x + 9.0f,
                    nodePosition.y + nodeHeight - 18.0f,
                    kFontScale * 0.82f);
            }
            if (hovered)
                hoveredNode = node.id;
        }

        const std::size_t pressKey = widget_press_key(
            options.id, origin, options.size);
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        const auto nodeForPin = [&](graph_workspace::PinId id)
            -> std::optional<std::uint64_t>
        {
            const auto pins = graphState.controller.pins();
            const auto found = std::ranges::find(
                pins, id, &graph_workspace::PinLayout::id);
            return found == pins.end()
                ? std::nullopt
                : std::optional<std::uint64_t>{found->node_id};
        };
        const auto commitIntent = [&](
            const graph_workspace::WorkspaceIntent& intent)
        {
            if (intent.phase != graph_workspace::IntentPhase::commit)
                return;
            if (intent.kind == graph_workspace::IntentKind::move_nodes
                && options.allow_node_movement)
            {
                for (const std::uint64_t id : intent.node_ids)
                {
                    auto& offset = graphState.nodeOffsets[id];
                    offset.x = (std::clamp)(
                        offset.x + intent.world_delta.x,
                        -1'000'000.0,
                        1'000'000.0);
                    offset.y = (std::clamp)(
                        offset.y + intent.world_delta.y,
                        -1'000'000.0,
                        1'000'000.0);
                    result.moved_nodes.push_back({
                        .id = id,
                        .delta = {
                            static_cast<float>(intent.world_delta.x),
                            static_cast<float>(intent.world_delta.y)}
                    });
                }
                result.layout_changed = !result.moved_nodes.empty();
                return;
            }
            if (intent.kind == graph_workspace::IntentKind::connect_pins
                && options.allow_connections
                && intent.valid_target)
            {
                const auto source = nodeForPin(intent.output_pin_id);
                const auto target = nodeForPin(intent.input_pin_id);
                if (source && target && *source != *target)
                {
                    result.connection = NodeGraphCanvasConnection{
                        .source_node = *source,
                        .target_node = *target};
                }
                return;
            }
            if (intent.kind
                    == graph_workspace::IntentKind::disconnect_edges
                && options.allow_disconnection)
            {
                result.disconnected_edges.assign(
                    intent.edge_ids.begin(), intent.edge_ids.end());
            }
        };

        if (options.interactive && options.allow_disconnection
            && result.hovered && right_press_available())
        {
            const auto pointer = graphState.controller.pointer_down({
                .screen_position = {g_frame.mousePos.x, g_frame.mousePos.y},
                .button = graph_workspace::PointerButton::secondary,
                .disconnect_gesture = true});
            commitIntent(pointer.intent);
            if (pointer.hit.kind == graph_workspace::HitKind::edge)
                result.clicked_edge = pointer.hit.edge_id;
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            consume_right_press();
        }
        if (options.interactive && result.hovered && left_press_available())
        {
            const auto hit = graphState.controller.hit_test(
                {g_frame.mousePos.x, g_frame.mousePos.y});
            const auto pointer = graphState.controller.pointer_down({
                .screen_position = {g_frame.mousePos.x, g_frame.mousePos.y},
                .pan_gesture = hit.kind == graph_workspace::HitKind::none});
            if (hit.kind == graph_workspace::HitKind::node)
                result.clicked_node = hit.node_id;
            else if (hit.kind == graph_workspace::HitKind::pin)
                result.clicked_node = hit.node_id;
            else if (hit.kind == graph_workspace::HitKind::edge)
                result.clicked_edge = hit.edge_id;
            else
                result.clicked_background = true;
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            result.view_changed = result.view_changed || pointer.view_changed;
            pressedKey = pressKey;
            consume_left_press();
        }
        if (options.interactive && graphState.controller.pointer_active()
            && g_frame.mouseDown && pressedKey == pressKey)
        {
            const auto pointer = graphState.controller.pointer_move(
                {g_frame.mousePos.x, g_frame.mousePos.y});
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            result.view_changed = result.view_changed || pointer.view_changed;
        }
        if (options.interactive && graphState.controller.pointer_active()
            && g_frame.justReleased && pressedKey == pressKey)
        {
            const auto pointer = graphState.controller.pointer_up(
                {g_frame.mousePos.x, g_frame.mousePos.y});
            commitIntent(pointer.intent);
            result.selection_changed = result.selection_changed
                || pointer.selection_changed;
            result.view_changed = result.view_changed || pointer.view_changed;
            if (left_release_available())
                consume_left_release();
            pressedKey = 0u;
        }

        result.selected_nodes.assign(
            graphState.controller.selected_node_ids().begin(),
            graphState.controller.selected_node_ids().end());
        const auto finalView = graphState.controller.view();
        result.pan = {
            static_cast<float>(finalView.pan.x),
            static_cast<float>(finalView.pan.y)};
        result.zoom = static_cast<float>(finalView.zoom);

        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({0.0f, height + kContentPadding});
        return result;
    }

    SliderResult slider(const SliderOptions& options) noexcept
    {
        SliderResult result{};
        result.value = options.value;
        if (!g_frame.insideWindow || !g_frame.ctx || options.id.empty()
            || !std::isfinite(options.minimum)
            || !std::isfinite(options.maximum)
            || !std::isfinite(options.value)
            || !std::isfinite(options.step)
            || options.maximum <= options.minimum || options.step <= 0.0f)
        {
            return result;
        }

        const Vec2 position = g_frame.cursor;
        const float width = (std::max)(options.size.x, 40.0f);
        const float height = (std::max)(
            options.size.y, base_line_height(kFontScale) + 8.0f);
        result.bounds = {position, {width, height}};
        const gui_lib::SliderLayoutOptions layoutOptions{
            .bounds = {
                {position.x, position.y},
                {width, height}},
            .minimum = options.minimum,
            .maximum = options.maximum,
            .value = options.value,
            .step = options.step,
            .horizontal_padding = 12.0f,
            .track_height = 7.0f,
            .thumb_width = 16.0f,
            .thumb_height = 21.0f
        };
        if (!gui_lib::make_slider_layout(layoutOptions).valid)
            return result;
        result.hovered = options.enabled
            && point_in_rect(
                g_frame.mousePos, position.x, position.y, width, height)
            && point_in_active_clip(g_frame.mousePos);

        const std::size_t pressKey = widget_press_key(
            options.id, position, {width, height});
        auto& pressedKey = g_contextPressedButtonKeys[g_frame.ctx];
        if (result.hovered && left_press_available())
        {
            pressedKey = pressKey;
            consume_left_press();
        }
        const bool active = options.enabled && g_frame.mouseDown
            && pressedKey == pressKey;
        if (active || (result.hovered && g_frame.justReleased
                && pressedKey == pressKey))
        {
            result.value = gui_lib::slider_value_from_position(
                layoutOptions,
                g_frame.mousePos.x);
            result.changed = result.value != options.value;
        }
        if (g_frame.justReleased && pressedKey == pressKey)
        {
            if (result.hovered)
                consume_left_release();
            pressedKey = 0u;
        }

        const auto& palette = active_palette();
        draw_sprite(
            options.enabled ? palette.textField : palette.panelBackground,
            position.x, position.y, width, height);
        auto renderedLayoutOptions = layoutOptions;
        renderedLayoutOptions.value = result.value;
        const gui_lib::SliderLayout layout =
            gui_lib::make_slider_layout(renderedLayoutOptions);
        const bool lightSlider = active_palette_is_light();
        draw_sprite(lightSlider ? palette.buttonActive : palette.titleBar,
            layout.track.position.x, layout.track.position.y,
            layout.track.size.x, layout.track.size.y);
        if (layout.fill.size.x > 0.0f)
            draw_sprite(lightSlider ? palette.modalScrim : palette.buttonActive,
                layout.fill.position.x, layout.fill.position.y,
                layout.fill.size.x, layout.fill.size.y);
        // A 16px handle with a contrasting 2px frame remains visible on the
        // Light theme, unlike the old 5px grey-on-grey handle.
        draw_sprite(lightSlider ? palette.consoleBackground : palette.textField,
            layout.thumb.position.x - 2.0f, layout.thumb.position.y - 2.0f,
            layout.thumb.size.x + 4.0f, layout.thumb.size.y + 4.0f);
        draw_sprite(lightSlider ? palette.modalScrim : palette.buttonHover,
            layout.thumb.position.x, layout.thumb.position.y,
            layout.thumb.size.x, layout.thumb.size.y);
        if (!options.label.empty())
        {
            const std::string fitted = fit_text_to_width(
                options.label,
                (std::max)(1.0f, width - 2.0f * kContentPadding),
                kFontScale);
            draw_text_line(
                fitted.empty() ? options.label : std::string_view{fitted},
                position.x + kContentPadding,
                position.y + 2.0f,
                kFontScale);
        }
        g_frame.lastButtonBounds = result.bounds;
        advance_cursor({0.0f, height + kContentPadding});
        return result;
    }
    void image(const SpriteHandle& sprite, Vec2 size) noexcept
    {
        if (!g_frame.insideWindow || !g_frame.ctx) return;

        const Vec2 pos = g_frame.cursor;
        const float width = (std::max)(static_cast<float>(size.x), 1.0f);
        const float height = (std::max)(static_cast<float>(size.y), 1.0f);

        draw_sprite(sprite, pos.x, pos.y, width, height);
        advance_cursor({ 0.0f, height + kContentPadding });
    }

    RuntimeSurfaceAtlasResult register_runtime_surface_atlas(
        std::span<const RuntimeSurfaceDescriptor> surfaces) noexcept
    {
        RuntimeSurfaceAtlasResult result{};
        result.descriptor_count = surfaces.size();
        try
        {
            result.handles.resize(surfaces.size());
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }

        if (surfaces.empty())
            return result;
        if (surfaces.size() > runtime_surface_batch_maximum_entries)
        {
            result.status = RuntimeSurfaceBatchStatus::BatchLimitExceeded;
            return result;
        }

        struct PreparedSurface
        {
            const RuntimeSurfaceDescriptor* descriptor{};
            std::size_t inputIndex{};
            std::string key{};
            std::uint64_t contentHash{};
        };

        std::vector<PreparedSurface> prepared{};
        result.status = RuntimeSurfaceBatchStatus::Ready;
        try
        {
            prepared.reserve(surfaces.size());
            std::size_t totalBytes = 0u;
            for (std::size_t index = 0u;
                 index < surfaces.size();
                 ++index)
            {
                const RuntimeSurfaceDescriptor& surface =
                    surfaces[index];
                const auto reject = [&](RuntimeSurfaceBatchStatus status)
                {
                    if (result.status == RuntimeSurfaceBatchStatus::Ready)
                        result.status = status;
                };

                if (surface.id.empty()
                    || surface.id.size()
                        > runtime_surface_identifier_maximum_bytes
                    || surface.width == 0u
                    || surface.height == 0u
                    || surface.width > runtime_surface_maximum_extent
                    || surface.height > runtime_surface_maximum_extent)
                {
                    reject(RuntimeSurfaceBatchStatus::InvalidDescriptor);
                    continue;
                }

                const std::uint64_t expectedBytes =
                    static_cast<std::uint64_t>(surface.width)
                    * static_cast<std::uint64_t>(surface.height) * 4ull;
                if (expectedBytes != surface.rgba_pixels.size())
                {
                    reject(RuntimeSurfaceBatchStatus::InvalidDescriptor);
                    continue;
                }
                if (expectedBytes
                        > runtime_surface_batch_maximum_rgba_bytes
                    || totalBytes
                        > runtime_surface_batch_maximum_rgba_bytes
                            - static_cast<std::size_t>(expectedBytes))
                {
                    reject(RuntimeSurfaceBatchStatus::ByteBudgetExceeded);
                    continue;
                }

                const auto duplicate = std::find_if(
                    prepared.begin(),
                    prepared.end(),
                    [&](const PreparedSurface& accepted) noexcept
                    {
                        return accepted.key == surface.id;
                    });
                if (duplicate != prepared.end())
                {
                    reject(RuntimeSurfaceBatchStatus::DuplicateIdentifier);
                    continue;
                }

                totalBytes += static_cast<std::size_t>(expectedBytes);
                prepared.push_back(PreparedSurface{
                    .descriptor = &surface,
                    .inputIndex = index,
                    .key = std::string{surface.id},
                    .contentHash = hash_surface_pixels(
                        surface.rgba_pixels,
                        surface.width,
                        surface.height)
                });
            }
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }

        if (result.status != RuntimeSurfaceBatchStatus::Ready
            || prepared.empty())
            return result;

        try
        {
            ensure_resources();
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }

        try
        {
            std::scoped_lock locks(
                g_runtimeSurfaceCacheMutex,
                g_resourceMutex);
            TextureAtlas* runtimeAtlas =
                ensure_runtime_surface_atlas_locked();
            if (!runtimeAtlas)
            {
                result.status =
                    RuntimeSurfaceBatchStatus::ResourceUnavailable;
                return result;
            }

            bool mutationFailed = false;
            try
            {
                for (const PreparedSurface& surface : prepared)
                {
                    const RuntimeSurfaceDescriptor& descriptor =
                        *surface.descriptor;
                    auto entryIt =
                        g_runtimeSurfaceCache.find(surface.key);
                    if (entryIt != g_runtimeSurfaceCache.end()
                        && entryIt->second.handle.is_valid()
                        && entryIt->second.contentHash
                            == surface.contentHash
                        && entryIt->second.width == descriptor.width
                        && entryIt->second.height == descriptor.height)
                    {
                        result.handles[surface.inputIndex] =
                            entryIt->second.handle;
                        ++result.accepted_count;
                        ++result.unchanged_count;
                        continue;
                    }

                    if (entryIt != g_runtimeSurfaceCache.end()
                        && entryIt->second.handle.is_valid()
                        && entryIt->second.width == descriptor.width
                        && entryIt->second.height == descriptor.height
                        && !entryIt->second.spriteName.empty())
                    {
                        Texture texture{};
                        texture.name = entryIt->second.spriteName;
                        texture.width = descriptor.width;
                        texture.height = descriptor.height;
                        texture.channels = 4;
                        texture.pixels.assign(
                            descriptor.rgba_pixels.begin(),
                            descriptor.rgba_pixels.end());
                        if (!runtimeAtlas->replace_entry_pixels(
                                entryIt->second.spriteName,
                                texture))
                        {
                            mutationFailed = true;
                            break;
                        }

                        entryIt->second.contentHash =
                            surface.contentHash;
                        ++entryIt->second.version;
                        result.handles[surface.inputIndex] =
                            entryIt->second.handle;
                        ++result.accepted_count;
                        ++result.replaced_count;
                        continue;
                    }

                    const std::uint32_t nextVersion =
                        entryIt == g_runtimeSurfaceCache.end()
                            ? 1u
                            : entryIt->second.version + 1u;
                    std::vector<std::uint8_t> pixels(
                        descriptor.rgba_pixels.begin(),
                        descriptor.rgba_pixels.end());
                    std::string spriteName =
                        "__agui/runtime_surface/" + surface.key + "/"
                        + std::to_string(nextVersion);
                    SpriteHandle handle = try_add_sprite(
                        *runtimeAtlas,
                        spriteName,
                        pixels,
                        descriptor.width,
                        descriptor.height,
                        false);
                    if (!handle.is_valid())
                    {
                        mutationFailed = true;
                        break;
                    }

                    CachedRuntimeSurface cacheEntry{
                        .handle = handle,
                        .contentHash = surface.contentHash,
                        .width = descriptor.width,
                        .height = descriptor.height,
                        .version = nextVersion,
                        .spriteName = std::move(spriteName)
                    };
                    if (entryIt == g_runtimeSurfaceCache.end())
                    {
                        g_runtimeSurfaceCache.emplace(
                            surface.key,
                            std::move(cacheEntry));
                    }
                    else
                    {
                        entryIt->second = std::move(cacheEntry);
                    }

                    result.handles[surface.inputIndex] = handle;
                    ++result.accepted_count;
                    ++result.added_count;
                }
            }
            catch (...)
            {
                result.status =
                    RuntimeSurfaceBatchStatus::ResourceUnavailable;
            }

            if (mutationFailed)
            {
                result.status =
                    RuntimeSurfaceBatchStatus::AtlasMutationFailed;
            }

            try
            {
                epochengine::atlasmanager::ensure_uploaded(
                    *runtimeAtlas);
                result.uploaded = true;
            }
            catch (...)
            {
                result.status = RuntimeSurfaceBatchStatus::UploadFailed;
            }
            return result;
        }
        catch (...)
        {
            result.status = RuntimeSurfaceBatchStatus::ResourceUnavailable;
            return result;
        }
    }

    [[nodiscard]] static bool nested_content_clip_contract() noexcept
    {
        const bool previousInsideWindow = g_frame.insideWindow;
        const Vec2 previousMin = g_frame.contentMin;
        const Vec2 previousMax = g_frame.contentMax;

        g_frame.insideWindow = true;
        g_frame.contentMin = {10.0f, 20.0f};
        g_frame.contentMax = {110.0f, 120.0f};

        bool emptyIntersectionRemainsClipped = false;
        {
            ContentClipScope disjoint{
                {130.0f, 140.0f},
                {150.0f, 160.0f}};
            emptyIntersectionRemainsClipped =
                has_content_clip()
                && g_frame.contentMin.x == 130.0f
                && g_frame.contentMin.y == 140.0f
                && g_frame.contentMax.x == 110.0f
                && g_frame.contentMax.y == 120.0f
                && content_right() == 110.0f
                && content_bottom() == 120.0f;
        }

        const bool parentRestored =
            has_content_clip()
            && g_frame.contentMin.x == 10.0f
            && g_frame.contentMin.y == 20.0f
            && g_frame.contentMax.x == 110.0f
            && g_frame.contentMax.y == 120.0f;

        g_frame.insideWindow = previousInsideWindow;
        g_frame.contentMin = previousMin;
        g_frame.contentMax = previousMax;
        return emptyIntersectionRemainsClipped && parentRestored;
    }

    [[nodiscard]] static bool source_editor_navigation_contract() noexcept;
    [[nodiscard]] static bool inline_button_auto_width_contract();
    [[nodiscard]] static bool status_strip_allocation_contract() noexcept;

    [[nodiscard]] static bool inactive_input_contract()
    {
        const std::array click{
            InputEvent{.type = EventType::MouseDown, .mouse_pos = {10.0f, 20.0f}},
            InputEvent{.type = EventType::MouseUp, .mouse_pos = {10.0f, 20.0f}}};
        if (!completed_stationary_click(click, 0, {10.0f, 20.0f})
            || completed_stationary_click(click, 1, {10.0f, 20.0f})
            || completed_stationary_click(click, 0, {30.0f, 20.0f})
            || completed_stationary_click(std::span{click}.first(1), 0, {10.0f, 20.0f}))
            return false;
        const auto previousEvents = std::move(g_frame.events);
        const bool previousPointerAllowed = g_frame.pointerInputAllowed;
        g_frame.events = {
            {.type = EventType::MouseDown},
            {.type = EventType::TextInput, .text = "stale"},
            {.type = EventType::FocusLost},
            {.type = EventType::KeyDown, .key = 65} };
        const bool cancelled = filter_frame_input(true, true);
        const bool freshOnly = cancelled && g_frame.events.size() == 1
            && g_frame.events.front().type == EventType::KeyDown;
        g_frame.events.push_back({.type = EventType::MouseWheel, .wheel_delta = 120});
        const bool covered = filter_frame_input(true, false)
            && g_frame.events.size() == 1
            && !point_in_modal_input_capture({0.0f, 0.0f});
        const bool inactive = filter_frame_input(false, false) && g_frame.events.empty();
        g_frame.events = {{.type = EventType::MouseDown},
            {.type = EventType::TextInput, .text = "blocked"},
            {.type = EventType::MouseWheel, .wheel_delta = 120}};
        const bool hoverWheelOnly = filter_frame_input(false, false, true)
            && g_frame.events.size() == 1u
            && g_frame.events.front().type == EventType::MouseWheel;
        const bool occludedWheel = filter_frame_input(false, false, false)
            && g_frame.events.empty();
        g_frame.events = {{.type = EventType::FileDrop, .mouse_pos = {10.0f, 20.0f}, .files = {"reference.png"}},
            {.type = EventType::FocusLost}, {.type = EventType::TextInput, .text = "not focused"}};
        (void)filter_frame_input(false, false);
        const bool shellDropRetained = g_frame.events.size() == 1u
            && g_frame.events.front().type == EventType::FileDrop
            && g_frame.events.front().files == std::vector<std::string>{"reference.png"};
        g_frame.events = previousEvents;
        g_frame.pointerInputAllowed = previousPointerAllowed;
        return freshOnly && covered && inactive && hoverWheelOnly && occludedWheel && shellDropRetained;
    }

    bool run_runtime_surface_contract() noexcept
    {
        if (!inactive_input_contract()
            || !nested_content_clip_contract()
            || !menu_item_geometry_contract()
            || !inline_button_auto_width_contract()
            || !status_strip_allocation_contract()
            || !source_editor_navigation_contract())
            return false;
        constexpr std::string_view utf8Sample{
            "A\xE2\x96\x88\xF0\x9F\x9A\x80"};
        const DecodedUtf8Codepoint ascii =
            decode_utf8_codepoint(utf8Sample, 0u);
        const DecodedUtf8Codepoint block =
            decode_utf8_codepoint(utf8Sample, 1u);
        const DecodedUtf8Codepoint rocket =
            decode_utf8_codepoint(utf8Sample, 4u);
        constexpr std::string_view malformed{"\xE2\x28\xA1"};
        const DecodedUtf8Codepoint rejected =
            decode_utf8_codepoint(malformed, 0u);
        if (ascii.codepoint != U'A' || ascii.byte_count != 1u
            || block.codepoint != U'\u2588'
            || block.byte_count != 3u
            || rocket.codepoint != U'\U0001F680'
            || rocket.byte_count != 4u
            || rejected.codepoint != U'?'
            || rejected.byte_count != 1u
            || safe_draw_codepoint(block.codepoint) != U'\u2588'
            || safe_draw_codepoint(U'\u2550') != U'\u2550'
            || safe_draw_codepoint(U'\u2551') != U'\u2551'
            || safe_draw_codepoint(U'\u2557') != U'\u2557'
            || next_utf8_codepoint_start(utf8Sample, 0u) != 1u
            || next_utf8_codepoint_start(utf8Sample, 1u) != 4u
            || next_utf8_codepoint_start(utf8Sample, 4u) != 8u
            || previous_utf8_codepoint_start(utf8Sample, 8u) != 4u
            || previous_utf8_codepoint_start(utf8Sample, 4u) != 1u)
        {
            return false;
        }
        constexpr std::array<std::uint8_t, 16> firstPixels{
            255u, 0u, 0u, 255u,
            0u, 255u, 0u, 255u,
            0u, 0u, 255u, 255u,
            255u, 255u, 255u, 255u};
        constexpr std::array<std::uint8_t, 16> changedPixels{
            0u, 0u, 0u, 255u,
            255u, 255u, 0u, 255u,
            0u, 255u, 255u, 255u,
            255u, 0u, 255u, 255u};
        const RuntimeSurfaceDescriptor first{
            .id = "__agui_contract/runtime_surface",
            .rgba_pixels = firstPixels,
            .width = 2u,
            .height = 2u};
        const RuntimeSurfaceAtlasResult initial =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{&first, 1u});
        if (!initial || initial.handles.size() != 1u
            || !initial.handles.front().is_valid())
        {
            return false;
        }

        const RuntimeSurfaceAtlasResult unchanged =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{&first, 1u});
        if (!unchanged || unchanged.unchanged_count != 1u
            || unchanged.handles.front() != initial.handles.front())
        {
            return false;
        }

        const RuntimeSurfaceDescriptor changed{
            .id = first.id,
            .rgba_pixels = changedPixels,
            .width = first.width,
            .height = first.height};
        const RuntimeSurfaceAtlasResult replaced =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{&changed, 1u});
        return replaced
            && replaced.replaced_count == 1u
            && replaced.handles.front() == initial.handles.front();
    }
    SpriteHandle register_runtime_surface(
        std::string_view id,
        std::span<const std::uint8_t> rgba_pixels,
        std::uint32_t width,
        std::uint32_t height) noexcept
    {
        const RuntimeSurfaceDescriptor descriptor{
            .id = id,
            .rgba_pixels = rgba_pixels,
            .width = width,
            .height = height
        };
        RuntimeSurfaceAtlasResult result =
            register_runtime_surface_atlas(
                std::span<const RuntimeSurfaceDescriptor>{
                    &descriptor,
                    1u
                });
        return result && !result.handles.empty()
            ? result.handles.front()
            : SpriteHandle{};
    }
