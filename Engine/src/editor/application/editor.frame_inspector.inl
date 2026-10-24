// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto render_inspector_window = [&](
            gui::Vec2 pane_pos,
            gui::Vec2 pane_size,
            InspectorToolTab pane_tab,
            std::string_view pane_route,
            std::string_view pane_label)
        {
        if (pane_size.x > 1.0f && pane_size.y > 1.0f)
        {
        editor.inspectorToolTab = pane_tab;
        // The dock tab already owns the visible title; avoid a second title
        // taking content height and causing duplicate headings.
        gui::begin_window(pane_label, pane_pos, pane_size, true, false);
        handle_pane_title_drag_to_tab(
            pane_route, pane_label, pane_pos, pane_size);
        const gui::Vec2 inspectorScrollStart = gui::cursor_position();
        const float inspectorScrollHeight = (std::max)(
            48.0f,
            pane_pos.y + pane_size.y - inspectorScrollStart.y - 4.0f);
        (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
            .id = pane_tab == InspectorToolTab::AiDevelopment ? "inspector-ai-work-body"
                : "inspector-scene-body",
            .size = { (std::max)(80.0f, pane_size.x - 12.0f), inspectorScrollHeight },
            .draw_background = true,
            .show_scrollbar = true
        });
        if (editor.inspectorToolTab == InspectorToolTab::WorldSettings)
        {
            const auto view = previewgrid::camera_descriptor_for(ctx.get());
            const auto resolvedView = render_camera::resolve(view);
            gui::label("Global Settings");
            gui::property_row("World", editor.activeWorld.empty() ? std::string("Untitled World") : editor.activeWorld, 118.0f);
            gui::property_row("Scene", editor.activeRuntimeScene.empty() ? std::string("Editor Scene") : editor.activeRuntimeScene, 118.0f);
            gui::property_row("Renderer", renderer_name(ctx), 118.0f);
            gui::property_row("View", std::string(previewgrid::editor_view_name(previewgrid::editor_view_for(ctx.get()))), 118.0f);
            gui::property_row("Projection", std::string(render_camera::projection_name(view.projection)), 118.0f);
            gui::property_row("View revision", std::to_string(view.source_revision), 118.0f);
            gui::property_row("View valid", resolvedView.valid ? std::string("true") : std::string("false"), 118.0f);
            gui::property_row("Fly speed", epochengine::format_text("{:.2f}", previewgrid::fly_speed_for(ctx.get())), 118.0f);
            gui::property_row("Objects", std::to_string(visible_entity_count(editor)), 118.0f);
            gui::property_row("Project camera", std::string(project_camera_label(editor.projectCameraMode)), 118.0f);

            const auto eventRenderDebug = epochengine::render_event_debug::snapshot();
            gui::label("Event-Driven Rendering");
            gui::property_row(
                "Path",
                std::string(epochengine::render_event_debug::path_name(eventRenderDebug.path)),
                118.0f);
            gui::property_row(
                "Fallback",
                std::string(epochengine::render_event_debug::fallback_name(eventRenderDebug.fallback)),
                118.0f);
            gui::property_row(
                "Dirty regions",
                std::to_string(eventRenderDebug.dirty_regions),
                118.0f);
            gui::property_row(
                "Vacated regions",
                std::to_string(eventRenderDebug.vacated_regions),
                118.0f);
            gui::property_row(
                "Dirty coverage",
                epochengine::format_text("{:.2f}%", eventRenderDebug.dirty_coverage_percent),
                118.0f);
            gui::property_row(
                "Render time",
                epochengine::format_text("{} us", eventRenderDebug.render_time_us),
                118.0f);
            gui::property_row(
                "Frame path totals",
                epochengine::format_text(
                    "Cached {} / Partial {} / Full {} / Conventional {}",
                    eventRenderDebug.cached_frames,
                    eventRenderDebug.partial_frames,
                    eventRenderDebug.full_frames,
                    eventRenderDebug.conventional_frames),
                118.0f);
            const auto pathTimingText = [](
                const epochengine::render_event_debug::BenchmarkTiming& timing)
            {
                if (timing.samples == 0u)
                    return std::string{"waiting"};
                return epochengine::format_text(
                    "{:.1f} us avg / {:.0f} render FPS",
                    timing.average_time_us,
                    epochengine::render_event_debug::render_fps(timing));
            };
            gui::property_row("Cached avg / rate", pathTimingText(eventRenderDebug.cached), 118.0f);
            gui::property_row("Partial avg / rate", pathTimingText(eventRenderDebug.partial), 118.0f);
            gui::property_row("Full avg / rate", pathTimingText(eventRenderDebug.full), 118.0f);
            gui::property_row("Conventional avg / rate", pathTimingText(eventRenderDebug.conventional), 118.0f);
            gui::property_row(
                "Benchmark lane",
                std::string(epochengine::render_event_debug::benchmark_lane_name(eventRenderDebug.benchmark_lane)),
                118.0f);
            gui::property_row(
                "Selective samples",
                std::to_string(eventRenderDebug.selective.samples),
                118.0f);
            gui::property_row(
                "Selective last / avg",
                eventRenderDebug.selective.samples == 0u
                    ? std::string("waiting")
                    : epochengine::format_text(
                        "{:.1f} / {:.1f} us",
                        static_cast<double>(eventRenderDebug.selective.last_time_ns) / 1000.0,
                        eventRenderDebug.selective.average_time_us),
                118.0f);
            gui::property_row(
                "Selective total",
                epochengine::format_text(
                    "{:.3f} ms",
                    static_cast<double>(eventRenderDebug.selective.total_time_ns) / 1'000'000.0),
                118.0f);
            gui::property_row(
                "Full samples",
                std::to_string(eventRenderDebug.full_baseline.samples),
                118.0f);
            gui::property_row(
                "Full last / avg",
                eventRenderDebug.full_baseline.samples == 0u
                    ? std::string("waiting")
                    : epochengine::format_text(
                        "{:.1f} / {:.1f} us",
                        static_cast<double>(eventRenderDebug.full_baseline.last_time_ns) / 1000.0,
                        eventRenderDebug.full_baseline.average_time_us),
                118.0f);
            gui::property_row(
                "Full total",
                epochengine::format_text(
                    "{:.3f} ms",
                    static_cast<double>(eventRenderDebug.full_baseline.total_time_ns) / 1'000'000.0),
                118.0f);
            gui::property_row(
                "Average comparison",
                eventRenderDebug.selective.samples == 0u || eventRenderDebug.full_baseline.samples == 0u
                    ? std::string("waiting for both lanes")
                    : epochengine::format_text(
                        "{:.2f}x speedup / {:.1f}% less time",
                        eventRenderDebug.average_speedup,
                        eventRenderDebug.average_time_saved_percent),
                118.0f);

            bool eventPathEnabled = eventRenderDebug.event_path_enabled;
            if (gui::toggle_switch("Event renderer", eventPathEnabled, { 210.0f, 28.0f }))
            {
                epochengine::render_event_debug::set_event_path_enabled(eventPathEnabled);
                if (!eventPathEnabled)
                    epochengine::render_event_debug::set_benchmark_enabled(false);
                push_editor_log(
                    editor,
                    eventPathEnabled
                        ? "[renderer] Event-driven OpenGL scene reuse enabled."
                        : "[renderer] Conventional OpenGL scene rendering forced; A/B benchmark disabled.");
            }
            bool benchmarkEnabled = eventRenderDebug.benchmark_enabled;
            if (gui::toggle_switch("A/B render benchmark", benchmarkEnabled, { 210.0f, 28.0f }))
            {
                epochengine::render_event_debug::set_benchmark_enabled(benchmarkEnabled);
                push_editor_log(
                    editor,
                    benchmarkEnabled
                        ? "[renderer] A/B benchmark enabled: alternate SELECTIVE and FULL BASELINE frames with synchronized GPU-complete timing."
                        : "[renderer] A/B benchmark disabled.");
            }
            if (gui::button("Reset render benchmark", { 210.0f, 27.0f }))
            {
                epochengine::render_event_debug::reset_benchmark_statistics();
                push_editor_log(editor, "[renderer] A/B benchmark statistics reset.");
            }
            if (gui::button("Reset render path stats", { 210.0f, 27.0f }))
            {
                epochengine::render_event_debug::reset_path_statistics();
                push_editor_log(editor, "[renderer] Cached/partial/full/conventional path statistics reset.");
            }
            bool dirtyOverlayEnabled = eventRenderDebug.dirty_overlay_enabled;
            if (gui::toggle_switch("Dirty-region overlay", dirtyOverlayEnabled, { 210.0f, 28.0f }))
            {
                epochengine::render_event_debug::set_dirty_overlay_enabled(dirtyOverlayEnabled);
                push_editor_log(
                    editor,
                    dirtyOverlayEnabled
                        ? "[renderer] Dirty-region overlay enabled."
                        : "[renderer] Dirty-region overlay disabled.");
            }
            bool vacatedOverlayEnabled = eventRenderDebug.vacated_overlay_enabled;
            if (gui::toggle_switch("Vacated-region overlay", vacatedOverlayEnabled, { 210.0f, 28.0f }))
            {
                epochengine::render_event_debug::set_vacated_overlay_enabled(vacatedOverlayEnabled);
                push_editor_log(
                    editor,
                    vacatedOverlayEnabled
                        ? "[renderer] Vacated-region overlay enabled (previous object bounds)."
                        : "[renderer] Vacated-region overlay disabled.");
            }

            if (gui::toggle_switch("Scene helpers", editor.helpersVisible, { 210.0f, 28.0f }))
                push_editor_log(editor, editor.helpersVisible ? "[world] Scene helpers enabled." : "[world] Scene helpers disabled.");
            if (gui::toggle_switch("Rounded controls", editor.roundedRectangles, { 210.0f, 28.0f }))
                push_editor_log(editor, editor.roundedRectangles ? "[ui] Rounded controls enabled." : "[ui] Rounded controls disabled.");
            const std::array worldButtons{
                gui::InlineButtonSpec{ .label = "Focus Selection", .width = 112.0f },
                gui::InlineButtonSpec{ .label = "Reset View", .width = 86.0f }
            };
            if (const auto action = gui::inline_button_row(worldButtons, 26.0f, 4.0f))
            {
                if (*action == 0u)
                    handle_scene_tool(editor, ctx.get(), "focus_selection");
                else
                    previewgrid::reset_camera(ctx.get());
            }
            gui::wrapped_label(
                "Editor navigation is session-owned. Project, capture, RTT, and portal cameras keep independent immutable view descriptors.",
                (std::max)(160.0f, pane_size.x - 24.0f));
            gui::end_scroll_area();
            gui::end_window();
            return;
        }
        if (pane_tab == InspectorToolTab::AiDevelopment)
        {
            const std::filesystem::path inspectorBuildLog =
                project_build_log_path(editor.projectRoot);
            const std::filesystem::path inspectorOutputExe =
                project_output_exe_path(editor.projectRoot);
            const float inspectorWidth =
                (std::max)(180.0f, pane_size.x - 24.0f);
            if (editor.aiWorkspaceDomain != AiWorkspaceDomain::Engine
                && editor.aiWorkspaceDomain != AiWorkspaceDomain::Control)
            {
                editor.aiWorkspaceDomain = AiWorkspaceDomain::Engine;
            }
            const std::array aiInspectorTabs{
                gui::TabButtonSpec{
                    .id = "inspector.ai.authoring",
                    .label = "Project Assistant",
                    .width = 0.0f,
                    .active = editor.aiWorkspaceDomain
                        == AiWorkspaceDomain::Engine},
                gui::TabButtonSpec{
                    .id = "inspector.ai.engine_development",
                    .label = "Assistant",
                    .width = 0.0f,
                    .active = editor.aiWorkspaceDomain
                        == AiWorkspaceDomain::Control}
            };
            if (const auto selected = gui::tab_bar_buttons(
                    aiInspectorTabs, 30.0f, 2.0f);
                selected.selected_index)
            {
                if (*selected.selected_index == 0u)
                {
                    editor.aiWorkspaceDomain = AiWorkspaceDomain::Engine;
                    if (!main_surface_is_ai_authoring(editor.mainSurface))
                    {
                        apply_editor_surface(
                            editor.aiAuthoringSurface,
                            "Assistant authoring");
                    }
                }
                else
                {
                    editor.aiWorkspaceDomain = AiWorkspaceDomain::Control;
                    if (editor.mainSurface != EditorMainSurface::AISandbox)
                    {
                        apply_editor_surface(
                            EditorMainSurface::AISandbox,
                            "Assistant");
                    }
                }
            }

            // The workbench contains project and candidate operations only.
            // Model/runtime/role/routing/training settings live exclusively in
            // the dedicated Model Manager, not nested beneath this pane.

            if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Engine)
            {
                gui::semantic_block(
                    "PROJECT ASSISTANT · ACTIVE PROJECT\n"
                    "Conversation receives bounded active-project context. Chat project goals and planned project changes stage scene/GUI work; nothing is applied until you approve the exact staged call.",
                    gui::SemanticTone::neutral,
                    inspectorWidth);

                gui::label("What should change in this project?");
                const auto requestInput = gui::edit_box(
                    editor.aiAuthoringRequest,
                    {inspectorWidth, 92.0f},
                    2'048u,
                    true);
                const bool planAwaitingApproval = !editor.aiProjectScriptDraft.empty()
                    || (editor.aiAuthoringPlan && !editor.aiAuthoringPlanApplied && !editor.aiAuthoringApplyPending);
                const std::array requestActions{
                    gui::InlineButtonSpec{
                        .label = editor.aiAuthoringAwaitingReply
                            ? "Planning..."
                            : planAwaitingApproval
                                ? "Plan Staged In Chat"
                                : "Plan Changes",
                        .width = planAwaitingApproval ? 168.0f : 132.0f,
                        .enabled = !editor.aiAuthoringAwaitingReply
                            && !editor.aiAuthoringApplyPending && !planAwaitingApproval},
                    gui::InlineButtonSpec{
                        .label = "Discard",
                        .width = 88.0f,
                        .enabled = !editor.aiAuthoringApplyPending}
                };
                const auto requestAction = gui::inline_button_row(
                    requestActions,
                    30.0f,
                    6.0f);
                if (requestInput.submitted)
                {
                    (void)request_ai_authoring_plan();
                }
                if (requestAction && *requestAction == 0u)
                {
                    (void)request_ai_authoring_plan();
                }
                if (requestAction && *requestAction == 1u)
                {
                    editor.aiProjectScriptDraft.clear();
                    editor.aiAuthoringPlan = {};
                    editor.aiAuthoringPlanApplied = false;
                    editor.aiAuthoringAwaitingReply = false;
                    editor.aiAuthoringStatus =
                        "The staged authoring plan was discarded.";
                }

                gui::property_row(
                    "Project",
                    editor.projectName.empty()
                        ? std::string{"No project selected"}
                        : editor.projectName,
                    84.0f);
                gui::property_row(
                    "Authority",
                    "proposal only; explicit apply",
                    84.0f);
                gui::wrapped_label(
                    editor.aiAuthoringStatus,
                    inspectorWidth);

                if (editor.aiAuthoringPlan)
                {
                    gui::label("Staged Plan");
                    gui::property_row(
                        "Title",
                        editor.aiAuthoringPlan.plan.title,
                        72.0f);
                    gui::wrapped_label(
                        editor.aiAuthoringPlan.plan.summary,
                        inspectorWidth);
                    for (const auto& call :
                        editor.aiAuthoringPlan.plan.calls)
                    {
                        gui::wrapped_label(
                            ai_authoring_call_summary(call),
                            inspectorWidth);
                    }
                }

                gui::wrapped_label(
                    "Conversation and response history remain in the Assistant "
                    "dock so the scene stays visible while authoring.",
                    inspectorWidth);
                gui::end_scroll_area();
                gui::end_window();
                return;
            }

#if EPOCH_EDITION_PUBLIC
            gui::label("Assistant settings");
            render_ai_model_picker(editor, inspectorWidth, "inspector-model-picker");
#endif


        }
        else
        {
            bool specializedProperties = false;
            if (editor.mainSurface == EditorMainSurface::Assets)
            {
                specializedProperties = true;
                const float inspectorWidth =
                    (std::max)(160.0f, pane_size.x - 24.0f);
                gui::label("Asset Properties");
                if (editor.selectedAssetPath.empty())
                {
                    gui::wrapped_label(
                        "Select an asset in the browser.",
                        inspectorWidth);
                }
                else
                {
                    const std::filesystem::path selectedPath =
                        resolve_editor_path(
                            std::filesystem::path{
                                editor.selectedAssetPath});
                    std::error_code assetError{};
                    const bool exists =
                        std::filesystem::exists(
                            selectedPath,
                            assetError)
                        && !assetError;
                    const bool directory =
                        exists
                        && std::filesystem::is_directory(
                            selectedPath,
                            assetError)
                        && !assetError;
                    const std::uintmax_t bytes =
                        exists && !directory
                        ? std::filesystem::file_size(
                            selectedPath,
                            assetError)
                        : 0u;
                    gui::property_row(
                        "Name",
                        selectedPath.filename().string(),
                        76.0f);
                    gui::property_row(
                        "Type",
                        directory
                            ? std::string{"Folder"}
                            : selectedPath.extension()
                                .string(),
                        76.0f);
                    gui::property_row(
                        "Path",
                        display_project_path(selectedPath),
                        76.0f);
                    gui::property_row(
                        "Size",
                        exists && !directory && !assetError
                            ? epochengine::format_text(
                                "{} KiB",
                                (bytes + 1023u) / 1024u)
                            : std::string{"Not available"},
                        76.0f);

                    const ProjectTextureThumbnailState* assetThumbnail =
                        nullptr;
                    if (exists && !directory
                        && project_texture_source_path(
                            editor.selectedAssetPath))
                    {
                        if (auto* controller =
                                project_texture_controller(editor))
                        {
                            if (const auto* texture =
                                    project_texture_for_asset_path(
                                        *controller,
                                        editor.projectRoot,
                                        editor.selectedAssetPath))
                            {
                                assetThumbnail =
                                    project_texture_thumbnail_state(
                                        editor,
                                        *controller,
                                        *texture);
                            }
                        }
                        if (!assetThumbnail)
                        {
                            assetThumbnail =
                                project_source_texture_thumbnail(
                                    editor,
                                    editor.selectedAssetPath);
                        }
                    }
                    if (assetThumbnail
                        && assetThumbnail->sprite.is_valid())
                    {
                        const std::uint32_t sourceWidth = (std::max)(
                            1u,
                            assetThumbnail->source_width);
                        const std::uint32_t sourceHeight = (std::max)(
                            1u,
                            assetThumbnail->source_height);
                        gui::property_row(
                            "Dimensions",
                            epochengine::format_text(
                                "{} x {}",
                                sourceWidth,
                                sourceHeight),
                            76.0f);
                        (void)gui::image_box({
                            .id = "asset-properties.texture-preview",
                            .sprite = assetThumbnail->sprite,
                            .size = {inspectorWidth, 180.0f},
                            .source_size = {
                                static_cast<float>(sourceWidth),
                                static_cast<float>(sourceHeight)},
                            .caption = selectedPath.filename().string(),
                            .fit = gui::ImageFit::Contain,
                            .interactive = false,
                            .selected = false
                        });
                    }

                    const std::array assetPropertyActions{
                        gui::InlineButtonSpec{
                            .label = "Open",
                            .width = 72.0f,
                            .enabled = exists},
                        gui::InlineButtonSpec{
                            .label = "Copy Path",
                            .width = 98.0f},
                        gui::InlineButtonSpec{
                            .label = "Refresh",
                            .width = 82.0f}
                    };
                    if (const auto action =
                            gui::inline_button_row(
                                assetPropertyActions,
                                28.0f,
                                4.0f))
                    {
                        if (*action == 0u && exists)
                        {
                            if (directory)
                            {
                                editor.assetWorkspaceSection =
                                    0u;
                            }
                            else if (selectedPath
                                .filename()
                                .string()
                                .ends_with(
                                    ".ascript.cpp"))
                            {
                                editor.activeScript =
                                    script_id_from_source_path(
                                        selectedPath);
                                editor.assetWorkspaceSection =
                                    2u;
                                (void)load_script_source_editor(
                                    editor,
                                    selectedPath,
                                    false);
                            }
                            else
                            {
                                editor.assetWorkspaceSection =
                                    1u;
                            }
                        }
                        else if (*action == 1u)
                        {
                            (void)gui::set_clipboard_text(
                                display_project_path(
                                    selectedPath));
                        }
                        else if (*action == 2u)
                        {
                            editor.assetGridVisibleIds.clear();
                            editor.projectTextureThumbnails.clear();
                            editor.projectSourceTextureThumbnails.clear();
                            editor.projectTextureThumbnailAtlasSignature = 0u;
                            if (auto* controller =
                                    project_texture_controller(editor))
                            {
                                (void)controller->refresh_catalog();
                            }
                            editor.projectTextureStatus =
                                "Asset catalog refreshed.";
                        }
                    }
                }
            }
            if (editor.mainSurface == EditorMainSurface::Game2D
                && editor.outlinerToolTab == OutlinerToolTab::Gui)
            {
                auto* document = synchronize_gui_document(editor);
                const auto mapped =
                    editor.guiDocumentWidgets.find(
                        editor.selectedEntityId);
                const auto widget =
                    document
                    && mapped != editor.guiDocumentWidgets.end()
                    ? document->widget(mapped->second)
                    : std::optional<
                        authoring::gui::WidgetView>{};
                if (document && widget)
                {
                    using namespace authoring::gui;
                    specializedProperties = true;
                    hydrate_gui_content_editor(
                        editor,
                        editor.selectedEntityId,
                        *widget);
                    const float inspectorWidth =
                        (std::max)(160.0f, pane_size.x - 24.0f);

                    gui::label("Selected GUI Element");
                    gui::label("Identity");
                    gui::property_row(
                        "Name",
                        widget->descriptor.name,
                        84.0f);
                    gui::property_row(
                        "Kind",
                        std::string{widget_kind_name(
                            widget->descriptor.kind)},
                        84.0f);
                    gui::property_row(
                        "Handle",
                        epochengine::format_text(
                            "{}:{}",
                            widget->handle.index,
                            widget->handle.generation),
                        84.0f);
                    gui::property_row(
                        "Parent",
                        widget->parent
                            ? epochengine::format_text(
                                "{}:{}",
                                widget->parent.index,
                                widget->parent.generation)
                            : std::string{"Canvas"},
                        84.0f);

                    gui::label("Content");
                    (void)gui::edit_box(
                        editor.guiContentPrimary,
                        {inspectorWidth, 28.0f},
                        512u,
                        false);
                    (void)gui::edit_box(
                        editor.guiContentSecondary,
                        {inspectorWidth, 28.0f},
                        512u,
                        false);
                    gui::label("Action");
                    (void)gui::edit_box(
                        editor.guiActionEditor,
                        {inspectorWidth, 28.0f},
                        256u,
                        false);
                    gui::label("Tooltip");
                    (void)gui::edit_box(
                        editor.guiTooltipEditor,
                        {inspectorWidth, 28.0f},
                        256u,
                        false);
                    if (gui::button(
                            "Apply Widget Content",
                            {inspectorWidth, 29.0f})
                        && apply_gui_content_editor(
                            editor, *document, *widget))
                    {
                        (void)project_gui_document_layout(
                            editor);
                        editor.guiDocumentStatus =
                            "Applied widget content and interaction.";
                    }

                    gui::label("Layout");
                    gui::wrapped_label(
                        "Position and size use the project canvas coordinate space.",
                        inspectorWidth);
                    LayoutDescriptor layout =
                        widget->descriptor.layout;
                    bool layoutChanged = false;
                    const auto xSlider = gui::slider({
                        .id = "gui.inspector.x",
                        .label = "X",
                        .minimum = 0.0f,
                        .maximum = 1'280.0f,
                        .value = layout.x,
                        .step = 1.0f,
                        .size = {inspectorWidth, 28.0f}});
                    if (xSlider.changed)
                    {
                        layout.x = xSlider.value;
                        layoutChanged = true;
                    }
                    const auto ySlider = gui::slider({
                        .id = "gui.inspector.y",
                        .label = "Y",
                        .minimum = 0.0f,
                        .maximum = 720.0f,
                        .value = layout.y,
                        .step = 1.0f,
                        .size = {inspectorWidth, 28.0f}});
                    if (ySlider.changed)
                    {
                        layout.y = ySlider.value;
                        layoutChanged = true;
                    }
                    const auto widthSlider = gui::slider({
                        .id = "gui.inspector.width",
                        .label = "Width",
                        .minimum = 18.0f,
                        .maximum = 1'280.0f,
                        .value = layout.width,
                        .step = 1.0f,
                        .size = {inspectorWidth, 28.0f}});
                    if (widthSlider.changed)
                    {
                        layout.width = widthSlider.value;
                        layoutChanged = true;
                    }
                    const auto heightSlider = gui::slider({
                        .id = "gui.inspector.height",
                        .label = "Height",
                        .minimum = 18.0f,
                        .maximum = 720.0f,
                        .value = layout.height,
                        .step = 1.0f,
                        .size = {inspectorWidth, 28.0f}});
                    if (heightSlider.changed)
                    {
                        layout.height = heightSlider.value;
                        layoutChanged = true;
                    }
                    if (layoutChanged
                        && document->update_layout(
                            widget->handle,
                            std::move(layout)))
                    {
                        (void)project_gui_document_layout(
                            editor);
                    }

                    gui::label("Interaction");
                    InteractionDescriptor interaction =
                        widget->descriptor.interaction;
                    bool interactionChanged = false;
                    interactionChanged =
                        gui::toggle_switch(
                            "Visible",
                            interaction.visible,
                            {inspectorWidth, 28.0f})
                        || interactionChanged;
                    interactionChanged =
                        gui::toggle_switch(
                            "Enabled",
                            interaction.enabled,
                            {inspectorWidth, 28.0f})
                        || interactionChanged;
                    interactionChanged =
                        gui::toggle_switch(
                            "Pointer events",
                            interaction.accepts_pointer,
                            {inspectorWidth, 28.0f})
                        || interactionChanged;
                    interactionChanged =
                        gui::toggle_switch(
                            "Keyboard focus",
                            interaction.focusable,
                            {inspectorWidth, 28.0f})
                        || interactionChanged;
                    if (interactionChanged)
                    {
                        (void)document->update_interaction(
                            widget->handle,
                            std::move(interaction));
                    }

                    gui::label("Appearance");
                    StyleDescriptor style =
                        widget->descriptor.style;
                    const auto opacitySlider = gui::slider({
                        .id = "gui.inspector.opacity",
                        .label = "Opacity",
                        .minimum = 0.0f,
                        .maximum = 1.0f,
                        .value = style.opacity,
                        .step = 0.01f,
                        .size = {inspectorWidth, 28.0f}});
                    const auto radiusSlider = gui::slider({
                        .id = "gui.inspector.radius",
                        .label = "Corner radius",
                        .minimum = 0.0f,
                        .maximum = 48.0f,
                        .value = style.corner_radius,
                        .step = 1.0f,
                        .size = {inspectorWidth, 28.0f}});
                    if (opacitySlider.changed
                        || radiusSlider.changed)
                    {
                        style.opacity = opacitySlider.value;
                        style.corner_radius =
                            radiusSlider.value;
                        (void)document->update_style(
                            widget->handle,
                            std::move(style));
                    }
                    gui::wrapped_label(
                        editor.guiDocumentStatus,
                        inspectorWidth);
                }
            }
            const auto selectedIndex = editor_entity_index(
                editor, editor.selectedEntityId);
            if (!specializedProperties && selectedIndex)
            {
                const auto& entity = editor.entities[*selectedIndex];
                gui::label("Selected Object");
                gui::property_row("Name", entity.name, 84.0f);
                gui::property_row("Type", entity.type, 84.0f);
                gui::property_row("Category", entity.category, 84.0f);
                gui::label("Transform");
                gui::property_row(
                    "Position", vec3_text(entity.position), 84.0f);
                gui::property_row(
                    "Rotation", vec3_text(entity.rotation), 84.0f);
                gui::property_row("Scale", vec3_text(entity.scale), 84.0f);
                gui::label("Object State");
                gui::property_row(
                    "Visible", entity.visible ? "Yes" : "No", 84.0f);
                gui::property_row(
                    "Editor only", entity.editorOnly ? "Yes" : "No", 84.0f);
                gui::property_row(
                    "Selection",
                    epochengine::format_text(
                        "{} object(s)", editor.selectedEntityIds.size()),
                    84.0f);

                gui::label("Movement / Benchmark");
                const bool movable = !entity.editorOnly
                    && entity.type != "Level"
                    && entity.type != "Camera";
                const bool oscillatorAttached = editor.scriptOscillator.enabled
                    && editor.scriptOscillator.entity_id == entity.sceneObjectId
                    || editor.scriptCubeOscillators.contains(entity.sceneObjectId);
                const std::array oscillatorActions{
                    gui::InlineButtonSpec{
                        .label = oscillatorAttached ? "Reattach Oscillator" : "Attach Oscillator",
                        .width = 132.0f,
                        .enabled = movable},
                    gui::InlineButtonSpec{
                        .label = "Detach",
                        .width = 64.0f,
                        .enabled = oscillatorAttached},
                    gui::InlineButtonSpec{
                        .label = "Open Source",
                        .width = 94.0f}
                };
                if (const auto action = gui::inline_button_row(
                        oscillatorActions, 27.0f, 4.0f))
                {
                    if (*action == 0u && movable)
                    {
                        editor.scriptOscillator.enabled = true;
                        editor.scriptCubeOscillators.erase(entity.sceneObjectId);
                        editor.scriptOscillator.entity_id = entity.sceneObjectId;
                        editor.scriptOscillator.base_position = entity.position;
                        editor.scriptOscillator.axis = 0u;
                        editor.scriptOscillator.amplitude = 2.0f;
                        editor.scriptOscillator.frequency_hz = 0.5f;
                        push_editor_log(
                            editor,
                            "[script] Oscillator attached from Properties: X +/-2 units at 0.5 Hz. Press Play to animate.");
                    }
                    else if (*action == 1u)
                    {
                        editor.scriptOscillator.enabled = false;
                        editor.scriptCubeOscillators.erase(entity.sceneObjectId);
                        editor.scriptOscillator.entity_id = scene::kInvalidSceneObjectId;
                        push_editor_log(editor, "[script] Oscillator detached.");
                    }
                    else if (*action == 2u)
                    {
                        editor.activeScript = "oscillate_selected_entity";
                        const std::string oscillatorSource =
                            editor_resolve_script_source_path(
                                editor.activeScript, editor.projectRoot);
                        editor.mainSurface = EditorMainSurface::Assets;
                        editor.assetWorkspaceSection = 2u;
                        editor.workspaceTab = EditorWorkspaceTab::Assets;
                        editor.outlinerToolTab = OutlinerToolTab::Scripting;
                        // Opening source does not overwrite saved pane visibility.
                        (void)load_script_source_editor(
                            editor, oscillatorSource, false);
                    }
                }
                gui::wrapped_label(
                    "Select a Cube, Attach Oscillator, press Play, then enable A/B Render Benchmark in Global Settings.",
                    (std::max)(160.0f, pane_size.x - 24.0f));
            }
            else if (!specializedProperties)
            {
                gui::label("No Object Selected");
                gui::wrapped_label(
                    "Select an object in the viewport or World Outliner to inspect it.",
                    (std::max)(160.0f, pane_size.x - 24.0f));
            }
            if (!specializedProperties)
            {
                gui::label("Scene Preview");
                gui::property_row(
                    "Renderer", renderer_name(ctx), 92.0f);
                gui::property_row(
                    "Helpers", editor.helpersVisible ? "Visible" : "Hidden", 92.0f);
                gui::property_row(
                    "Mode", std::string(preview_mode_name(editor.previewMode)), 92.0f);
                gui::property_row(
                    "Camera", preview_camera_name(ctx), 92.0f);
                gui::property_row(
                    "Zoom", preview_zoom_text(ctx), 92.0f);
                gui::property_row(
                    "Objects", std::to_string(visible_entity_count(editor)), 92.0f);
                gui::label("Project Runtime");
                gui::property_row(
                    "Editor script", editor.activeScript, 92.0f);
                gui::property_row(
                    "Run target", editor.activeRuntimeScene, 92.0f);
                gui::label("Viewport Controls");
                gui::wrapped_label(
                    "Select: left mouse  |  Add/remove: Shift + left mouse  |  "
                    "Marquee: drag empty viewport  |  Group move: drag any selected object  |  "
                    "Pan: middle mouse  |  Orbit: Alt + left mouse  |  "
                    "Dolly: Alt + right mouse  |  Fly: right mouse + WASD/QE  |  Focus: F",
                    (std::max)(160.0f, pane_size.x - 24.0f));
            }
        }
        gui::end_scroll_area();
        gui::end_window();
        }
        };

        bool timelineControlsPrepared = false;
        const auto prepare_timeline_time_controls = [&]()
        {
            if (timelineControlsPrepared) return;
            ensure_timeline_defaults(editor);
            const auto timelineStats = timeline_stats_from_editor(editor);
            epochengine::timeline::sync_to_simulation(editor.timelineState, timelineStats);
            epochengine::timeline::clamp_state(editor.timelineState);
            timelineControlsPrepared = true;
        };
        const auto timeline_playhead_readout = [&]()
        {
            return epochengine::format_text("Playhead {:.2f}s / {:.2f}s  |  {}",
                editor.timelineState.playhead_seconds, editor.timelineState.duration_seconds,
                editor.timelineState.playing ? "Playing" : "Paused");
        };
        const auto timeline_playback_buttons = [&]()
        {
            return std::array{
                gui::InlineButtonSpec{.label = "Rewind"},
                gui::InlineButtonSpec{.label = "-1s"},
                gui::InlineButtonSpec{.label = editor.timelineState.playing ? "Pause" : "Play"},
                gui::InlineButtonSpec{.label = "+1s"}};
        };
        const auto timeline_action_layout = [](float width)
        {
            return gui::InlineButtonRowOptions{
                .sizing = gui::InlineButtonSizing::FitContent,
                .alignment = gui::InlineButtonAlignment::Center,
                .wrap = true,
                .available_width = width};
        };
        const auto timeline_compact_controls_height = [&](float width)
        {
            prepare_timeline_time_controls();
            const float rowHeight = gui::line_height() + 10.0f;
            return gui::measure_wrapped_label_height(timeline_playhead_readout(), width)
                + 6.0f + 14.0f + 6.0f + rowHeight + 6.0f
                + gui::inline_button_row_height(timeline_playback_buttons(), rowHeight,
                    4.0f, timeline_action_layout(width)) + 6.0f;
        };
        auto render_timeline_time_controls = [&](float availableWidth, bool compact)
        {
            prepare_timeline_time_controls();

            const float width = (std::max)(1.0f, availableWidth);
            const float rowHeight = gui::line_height() + 10.0f;
            // Keep text outside the thin progress track. The scrubber owns
            // the full track rather than painting its label over the thumb.
            gui::wrapped_label(timeline_playhead_readout(), width);
            gui::advance_cursor({0.0f, 6.0f});
            const float playheadValue = static_cast<float>(std::clamp(
                editor.timelineState.duration_seconds > 0.0
                    ? editor.timelineState.playhead_seconds / editor.timelineState.duration_seconds
                    : 0.0,
                0.0,
                1.0));
            gui::progress_bar(gui::ProgressBarOptions{
                .value = playheadValue,
                .size = {width, 14.0f},
                .show_percent = false
            });
            const auto scrubber = gui::slider(gui::SliderOptions{
                .id = compact
                    ? "scene-timeline-playhead"
                    : "timeline-workspace-playhead",
                .minimum = 0.0f,
                .maximum = static_cast<float>(
                    editor.timelineState.duration_seconds),
                .value = static_cast<float>(
                    editor.timelineState.playhead_seconds),
                .step = static_cast<float>(
                    editor.timelineState.fixed_dt_seconds),
                .size = {width, rowHeight}
            });
            if (scrubber.changed)
            {
                editor.timelineState.playhead_seconds =
                    static_cast<double>(scrubber.value);
                editor.timelineState.playing = false;
                editor.timeControl.paused = true;
                epochengine::timeline::clamp_state(editor.timelineState);
            }

            if (compact)
            {
                const auto compactPlaybackButtons = timeline_playback_buttons();
                if (const auto action = gui::inline_button_row(
                        compactPlaybackButtons, rowHeight, 4.0f, timeline_action_layout(width)))
                {
                    switch (*action)
                    {
                    case 0:
                        editor.timelineState.playhead_seconds = 0.0;
                        editor.timelineState.playhead_frame = 0u;
                        editor.timelineState.playing = false;
                        editor.timeControl.paused = true;
                        break;
                    case 1:
                        epochengine::timeline::scrub_seconds(
                            editor.timelineState, -1.0);
                        editor.timelineState.playing = false;
                        editor.timeControl.paused = true;
                        break;
                    case 2:
                        editor.timelineState.playing =
                            !editor.timelineState.playing;
                        editor.timeControl.paused =
                            !editor.timelineState.playing;
                        break;
                    case 3:
                        epochengine::timeline::scrub_seconds(
                            editor.timelineState, 1.0);
                        editor.timelineState.playing = false;
                        editor.timeControl.paused = true;
                        break;
                    default:
                        break;
                    }
                }
                return;
            }

            const std::string pacingHealth = pacing_health_summary(editor.timeSnapshot);
            gui::label("Time Spine Controls");
            gui::property_row("[time] State", editor.timeSnapshot.paused ? "Paused" : "Running", 132.0f);
            gui::property_row("[time] Frame dt", format_ms(editor.timeSnapshot.real_dt_seconds), 132.0f);
            gui::property_row("[time] Scaled dt", format_ms(editor.timeSnapshot.scaled_dt_seconds), 132.0f);
            gui::property_row(
                "[time] Fixed step",
                std::string(format_ms(editor.timeSnapshot.fixed_dt_seconds)) + " / " + format_rate(editor.timeSnapshot.fixed_dt_seconds),
                132.0f);
            gui::property_row("[time] Simulated", format_seconds(editor.timeSnapshot.simulated_seconds), 132.0f);
            gui::property_row("[time] Accumulator", format_ms(editor.timeSnapshot.accumulator_seconds), 132.0f);
            gui::property_row(
                "[time] Step budget",
                epochengine::format_text("{} / {}", editor.timeSnapshot.step_budget, editor.timeSnapshot.max_steps_per_frame),
                132.0f);
            gui::property_row("[time] Time scale", epochengine::format_text("{:.2f}x", editor.timeSnapshot.time_scale), 132.0f);
            gui::property_row("[time] Pacing health", pacingHealth, 132.0f);

            const std::array timeButtons{
                gui::InlineButtonSpec{ .label = editor.timeControl.paused ? "Resume" : "Pause" },
                gui::InlineButtonSpec{ .label = "Step" },
                gui::InlineButtonSpec{ .label = "0.5x" },
                gui::InlineButtonSpec{ .label = "1x" },
                gui::InlineButtonSpec{ .label = "2x" }
            };
            if (const auto action = gui::inline_button_row(timeButtons, rowHeight, 6.0f, timeline_action_layout(width)))
            {
                switch (*action)
                {
                case 0: editor.timeControl.paused = !editor.timeControl.paused; break;
                case 1: editor.timeControl.step_once = true; break;
                case 2: editor.timeControl.time_scale = 0.5; break;
                case 3: editor.timeControl.time_scale = 1.0; break;
                case 4: editor.timeControl.time_scale = 2.0; break;
                default: break;
                }
            }

            const std::array cadenceButtons{
                gui::InlineButtonSpec{ .label = "30 Hz" },
                gui::InlineButtonSpec{ .label = "60 Hz" },
                gui::InlineButtonSpec{ .label = "120 Hz" }
            };
            if (const auto action = gui::inline_button_row(cadenceButtons, rowHeight, 6.0f, timeline_action_layout(width)))
            {
                switch (*action)
                {
                case 0: editor.timeControl.fixed_dt_seconds = 1.0 / 30.0; break;
                case 1: editor.timeControl.fixed_dt_seconds = 1.0 / 60.0; break;
                case 2: editor.timeControl.fixed_dt_seconds = 1.0 / 120.0; break;
                default: break;
                }
            }

            const std::array budgetButtons{
                gui::InlineButtonSpec{ .label = "4 steps" },
                gui::InlineButtonSpec{ .label = "8 steps" },
                gui::InlineButtonSpec{ .label = "12 steps" }
            };
            if (const auto action = gui::inline_button_row(budgetButtons, rowHeight, 6.0f, timeline_action_layout(width)))
            {
                switch (*action)
                {
                case 0: editor.timeControl.max_steps_per_frame = 4; break;
                case 1: editor.timeControl.max_steps_per_frame = 8; break;
                case 2: editor.timeControl.max_steps_per_frame = 12; break;
                default: break;
                }
            }
        };
