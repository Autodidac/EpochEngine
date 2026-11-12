// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        auto render_inspector_window = [&](
            gui::Vec2 pane_pos,
            gui::Vec2 pane_size,
            InspectorToolTab pane_tab,
            std::string_view pane_label)
        {
        if (pane_size.x > 1.0f && pane_size.y > 1.0f)
        {
        editor.inspectorToolTab = pane_tab;
        // The dock tab already owns the visible title; avoid a second title
        // taking content height and causing duplicate headings.
        gui::begin_window(pane_label, pane_pos, pane_size, true, false);
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
            gui::label("Environment / Ambient Lighting");
            gui::wrapped_label(
                "Every scene has ambient lighting. It cannot be deleted with scene objects. "
                "Linear RGB values are saved with the scene; values above 1 are allowed.",
                (std::max)(1.0f, pane_size.x - 24.0f));
            const auto ambient = editor.sceneDocument.initialized()
                ? editor.sceneDocument.project_settings().ambient : scene::kDefaultAmbientLight;
            if (!editor.sceneAmbientDraftInitialized
                || editor.sceneAmbientDraftSource != ambient)
            {
                for (std::size_t channel = 0u; channel < ambient.size(); ++channel)
                    editor.sceneAmbientDraft[channel] = epochengine::format_text("{}", ambient[channel]);
                editor.sceneAmbientDraftSource = ambient;
                editor.sceneAmbientDraftInitialized = true;
            }
            constexpr std::array<std::string_view, 3> channelLabels{
                "Ambient red", "Ambient green", "Ambient blue"};
            for (std::size_t channel = 0u; channel < ambient.size(); ++channel)
            {
                gui::label(channelLabels[channel]);
                (void)gui::edit_box(editor.sceneAmbientDraft[channel],
                    {(std::min)(150.0f, (std::max)(1.0f, pane_size.x - 24.0f)), 28.0f}, 48u);
            }
            const std::array ambientActions{
                gui::InlineButtonSpec{.label = "Apply Ambient", .width = 126.0f},
                gui::InlineButtonSpec{.label = "Reset Ambient", .width = 126.0f}};
            if (const auto action = gui::inline_button_row(ambientActions, 27.0f, 5.0f))
            {
                const auto value = *action == 1u
                    ? std::optional<scene::Vec3>{scene::kDefaultAmbientLight}
                    : parse_editor_ambient_channels(editor.sceneAmbientDraft);
                if (!value)
                    editor.projectStatus = "Ambient RGB channels must be finite and nonnegative.";
                else if (set_editor_scene_ambient(editor, *value))
                {
                    editor.sceneAmbientDraftInitialized = false;
                    editor.projectStatus = "Ambient lighting applied. Save Project to keep it.";
                }
            }
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

            if (gui::toggle_switch("Scene helpers", editor.helpersVisible, { 210.0f, 28.0f }))
                push_editor_log(editor, editor.helpersVisible ? "[world] Scene helpers enabled." : "[world] Scene helpers disabled.");
            if (gui::toggle_switch("Rounded controls", editor.roundedRectangles, { 210.0f, 28.0f }))
                push_editor_log(editor, editor.roundedRectangles ? "[ui] Rounded controls enabled." : "[ui] Rounded controls disabled.");
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
                project_build_log_path(editor.projectRoot, editor.projectBuildConfiguration);
            const std::filesystem::path inspectorOutputExe =
                project_output_exe_path(editor.projectRoot, editor.projectBuildConfiguration);
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
                        && project_image_preview_path(
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
                && editor.game2DGuiWorkspace)
            {
                auto* document = synchronize_gui_document(editor);
                const auto selected = selected_gui_widget(editor);
                const auto widget =
                    document && selected
                    ? document->widget(*selected)
                    : std::optional<
                        authoring::gui::WidgetView>{};
                if (document && widget)
                {
                    using namespace authoring::gui;
                    specializedProperties = true;
                    hydrate_gui_content_editor(
                        editor,
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
                    if (layoutChanged)
                        (void)commit_gui_layout_edit(
                            editor, widget->handle, std::move(layout));

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
                if (entity.category == "Planetary Terrain")
                {
                    gui::label(editor.lodInspectionReturnCamera ? "Camera LOD: Frozen for inspection"
                        : editor.planetarySettings.stream_lod ? "Camera LOD: On" : "Camera LOD: Off");
                    if (!editor.planetaryStreamStatus.empty())
                        gui::wrapped_label(editor.planetaryStreamStatus, (std::max)(80.0f, pane_size.x - 28.0f));
                }
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

                gui::label("Project movement script");
                const bool movable = !entity.editorOnly
                    && entity.type != "Level"
                    && entity.type != "Camera";
                const bool motionAttached = editor.scriptTranslationTracks.contains(entity.sceneObjectId)
                    || editor.scriptOscillator.enabled
                    && editor.scriptOscillator.entity_id == entity.sceneObjectId
                    || editor.scriptCubeOscillators.contains(entity.sceneObjectId);
                const bool projectScriptReady=movable&&!editor.projectRoot.empty()&&!editor.scriptEditorDirty&&!editor.scriptBuildPending;
                const bool movementMutationAllowed=editor_scene_script_mutation_allowed(editor);
                const std::array movementActions{
                    gui::InlineButtonSpec{
                        .label = "Open Movement Script",
                        .width = 0.0f,
                        .enabled = projectScriptReady},
                    gui::InlineButtonSpec{
                        .label = "Run Movement Script",
                        .width = 0.0f,
                        .enabled = projectScriptReady&&movementMutationAllowed},
                    gui::InlineButtonSpec{
                        .label = "Remove Motion",
                        .width = 0.0f,
                        .enabled = motionAttached&&movementMutationAllowed}
                };
                if (const auto action = gui::inline_button_row(
                        movementActions, 27.0f, 4.0f,{.sizing=gui::InlineButtonSizing::FitContent,
                            .alignment=gui::InlineButtonAlignment::Center,.wrap=true,
                            .available_width=(std::max)(1.0f,pane_size.x-24.0f)}))
                {
                    if (*action <= 1u && projectScriptReady&&(*action==0u||movementMutationAllowed)&&prepare_project_movement_script(editor))
                    {
                        if(*action==1u)emit_command(EditorCommand::RunScript,editor.activeScript);
                        else
                        {
                            editor.mainSurface=EditorMainSurface::Assets;editor.assetWorkspaceSection=2u;
                            editor.workspaceTab=EditorWorkspaceTab::Assets;editor.outlinerToolTab=OutlinerToolTab::Scripting;
                        }
                    }
                    else if (*action == 2u&&movementMutationAllowed)
                    {
                        editor.scriptTranslationTracks.erase(entity.sceneObjectId);
                        editor.scriptCubeOscillators.erase(entity.sceneObjectId);
                        if(editor.scriptOscillator.entity_id==entity.sceneObjectId)editor.scriptOscillator={};
                        editor.sceneInteractionDirty=true;
                        push_editor_log(editor,"[script] Removed preview motion. The authored transform is unchanged.");
                    }
                }
                if(!movementMutationAllowed)
                    gui::wrapped_label("Stop the running project to run or remove Editor motion.",
                        (std::max)(1.0f,pane_size.x-24.0f));
                gui::wrapped_label(
                    "Movement belongs to scripts/timeline_translation.ascript.cpp in your project. Edit its keys, Run it for this object, then Play, Reverse or seek on the timeline. The host copies the keys; sampling does not call the script.",
                    (std::max)(160.0f, pane_size.x - 24.0f));
                if (entity.type == "IndexedMesh" && entity.meshAsset && entity.terrainAttachment)
                {
                    const bool solid = entity.terrainAttachment->solid_walk_box;
                    gui::property_row("Walk collision", solid ? "Solid box" : "Off", 108.0f);
                    const std::array collisionActions{gui::InlineButtonSpec{
                        .label = solid ? "Disable Walking Collision" : "Enable Walking Collision", .width = 0.0f,
                        .enabled = editor.selectedEntityIds.size() <= 1u && movementMutationAllowed
                            && !model_import_scene_busy(editor) && !planetary_project_busy(editor)
                            && !editor.modelShorelinePending && !editor.assetGenerationPending}};
                    if (gui::inline_button_row(collisionActions, 27.0f, 4.0f,
                            {.sizing = gui::InlineButtonSizing::FitContent, .wrap = true,
                                .available_width = (std::max)(1.0f, pane_size.x - 24.0f)}))
                    {
                        if (!set_editor_walk_box(editor, entity, !solid))
                            push_editor_log(editor, "[scene] Verify the selected terrain attachment before changing walking collision.");
                    }
                }
                if (entity.category == "Planetary Terrain" || entity.terrainAttachment)
                {
                    const std::array refreshActions{gui::InlineButtonSpec{
                        .label = "Refresh Model and Spawn Placement", .width = 0.0f,
                        .enabled = editor.selectedEntityIds.size() <= 1u && movementMutationAllowed
                            && !model_import_scene_busy(editor) && !planetary_project_busy(editor)
                            && !editor.modelShorelinePending && !editor.assetGenerationPending}};
                    if (gui::inline_button_row(refreshActions, 27.0f, 4.0f,
                            {.sizing = gui::InlineButtonSizing::FitContent, .wrap = true,
                                .available_width = (std::max)(1.0f, pane_size.x - 24.0f)}))
                        (void)begin_forest_surface_refresh(editor, true);
                }
            }
            else if (!specializedProperties)
            {
                gui::label("No Object Selected");
                gui::wrapped_label(
                    "Select an object in the viewport or 3D Objects to inspect it.",
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
                    "Focus: double-click in the viewport or World Outliner  |  "
                    "Box select: drag in Select mode  |  Add to selection: Shift + drag  |  "
                    "Move group: choose Move Free, X, Y, or Z, then drag a selected object  |  "
                    "Pan: middle mouse  |  Orbit: Alt + left mouse  |  "
                    "Dolly: Alt + right mouse  |  Fly: right mouse + WASD | Roll: Q/E | Up/down: Space/Ctrl  |  Shift: fast | Right mouse + wheel: speed",
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
            epochengine::timeline::clamp_state(editor.timelineState);
            timelineControlsPrepared = true;
        };
        const auto timeline_playhead_readout = [&]()
        {
            return epochengine::format_text("Playhead {:.2f}s / {:.2f}s  |  {}  |  {:.0f}x",
                editor.timelineState.playhead_seconds, editor.timelineState.duration_seconds,
                editor.timelineState.playing
                    ?(editor.timelineState.direction==epochengine::timeline::PlaybackDirection::reverse?"Playing backward":"Playing forward")
                    :"Paused",editor.timelineState.playback_rate);
        };
        const auto timeline_playback_buttons = [&]()
        {
            return std::array{
                gui::InlineButtonSpec{.label = "Rewind"},
                gui::InlineButtonSpec{.label = "-1s"},
                gui::InlineButtonSpec{.label = "Fast Reverse",.tooltip="Play backward at 2x, 4x or 8x; click to cycle"},
                gui::InlineButtonSpec{.label = editor.timelineState.playing&&editor.timelineState.playback_rate==1.0&&editor.timelineState.direction==epochengine::timeline::PlaybackDirection::reverse ? "Pause Reverse" : "Reverse"},
                gui::InlineButtonSpec{.label = editor.timelineState.playing&&editor.timelineState.playback_rate==1.0&&editor.timelineState.direction==epochengine::timeline::PlaybackDirection::forward ? "Pause" : "Play"},
                gui::InlineButtonSpec{.label = "Fast Forward",.tooltip="Play forward at 2x, 4x or 8x; click to cycle"},
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
        const auto timeline_compact_playback_buttons = [&]()
        {
            const bool forward=editor.timelineState.playing&&editor.timelineState.playback_rate==1.0&&editor.timelineState.direction==epochengine::timeline::PlaybackDirection::forward;
            const bool reverse=editor.timelineState.playing&&editor.timelineState.playback_rate==1.0&&editor.timelineState.direction==epochengine::timeline::PlaybackDirection::reverse;
            return std::array{
                gui::InlineButtonSpec{.label="Rewind",.icon=gui::ButtonIcon::start,.tooltip="Jump to start"},
                gui::InlineButtonSpec{.label="-1s",.icon=gui::ButtonIcon::step_back,.tooltip="Back one second"},
                gui::InlineButtonSpec{.label="Fast Reverse",.icon=gui::ButtonIcon::fast_reverse,.tooltip="Play backward at 2x, 4x or 8x; click to cycle"},
                gui::InlineButtonSpec{.label=reverse?"Pause Reverse":"Reverse",.icon=reverse?gui::ButtonIcon::pause:gui::ButtonIcon::reverse,.tooltip=reverse?"Pause playback":"Play backward at 1x"},
                gui::InlineButtonSpec{.label=forward?"Pause":"Play",.icon=forward?gui::ButtonIcon::pause:gui::ButtonIcon::play,.tooltip=forward?"Pause playback":"Play forward at 1x"},
                gui::InlineButtonSpec{.label="Fast Forward",.icon=gui::ButtonIcon::fast_forward,.tooltip="Play forward at 2x, 4x or 8x; click to cycle"},
                gui::InlineButtonSpec{.label="+1s",.icon=gui::ButtonIcon::step_forward,.tooltip="Forward one second"}};
        };
        const auto timeline_apply_playback = [&](std::size_t action)
        {
            constexpr std::array commands{
                epochengine::timeline::PlaybackCommand::rewind,
                epochengine::timeline::PlaybackCommand::step_back,
                epochengine::timeline::PlaybackCommand::fast_reverse,
                epochengine::timeline::PlaybackCommand::toggle_reverse,
                epochengine::timeline::PlaybackCommand::toggle_forward,
                epochengine::timeline::PlaybackCommand::fast_forward,
                epochengine::timeline::PlaybackCommand::step_forward};
            if(action>=commands.size())return;
            epochengine::timeline::apply_playback_command(editor.timelineState,commands[action]);
            editor.timeControl.paused=!editor.timelineState.playing;
            (void)request_project_runtime_presentation(editor);
        };
        const auto timeline_apply_scrub = [&](float seconds)
        {
            epochengine::timeline::seek_seconds(editor.timelineState,static_cast<double>(seconds));
            editor.timeControl.paused=true;
            (void)request_project_runtime_presentation(editor);
        };
        const auto timeline_compact_controls_height = [&](float width)
        {
            prepare_timeline_time_controls();
            const float actionsWidth=width>=304.0f?width-112.0f:width;
            return gui::inline_button_row_height(timeline_compact_playback_buttons(),24.0f,
                4.0f,timeline_action_layout(actionsWidth))+2.0f
                +(width>=304.0f?0.0f:gui::line_height()+2.0f)+gui::line_height()+8.0f+2.0f;
        };
        auto render_timeline_time_controls = [&](float availableWidth, bool compact)
        {
            prepare_timeline_time_controls();

            const float width = (std::max)(1.0f, availableWidth);
            const float rowHeight = gui::line_height() + 10.0f;
            if(compact)
            {
                const auto origin=gui::cursor_position();
                const auto icons=timeline_compact_playback_buttons();
                const float actionsWidth=width>=304.0f?width-112.0f:width;
                const auto options=timeline_action_layout(actionsWidth);
                const float actionsHeight=gui::inline_button_row_height(icons,24.0f,4.0f,options);
                if(const auto action=gui::inline_button_row(icons,24.0f,4.0f,options))timeline_apply_playback(*action);
                const float readoutHeight=width>=304.0f?0.0f:gui::line_height()+2.0f;
                gui::set_cursor(width>=304.0f
                    ?gui::Vec2{origin.x+width-104.0f,origin.y+(std::max)(0.0f,(24.0f-gui::line_height())*.5f)}
                    :gui::Vec2{origin.x,origin.y+actionsHeight+2.0f});
                gui::label(epochengine::format_text("{:.2f}s {}{:.0f}x",editor.timelineState.playhead_seconds,
                    editor.timelineState.direction==epochengine::timeline::PlaybackDirection::reverse?"-":"",
                    editor.timelineState.playback_rate));
                gui::set_cursor({origin.x,origin.y+actionsHeight+2.0f+readoutHeight});
                const auto scrubber=gui::slider(gui::SliderOptions{
                    .id="scene-timeline-playhead",.minimum=0.0f,
                    .maximum=static_cast<float>(editor.timelineState.duration_seconds),
                    .value=static_cast<float>(editor.timelineState.playhead_seconds),
                    .step=static_cast<float>(editor.timelineState.fixed_dt_seconds),
                    .size={width,gui::line_height()+8.0f}});
                if(scrubber.changed)timeline_apply_scrub(scrubber.value);
                gui::set_cursor({origin.x,origin.y+timeline_compact_controls_height(width)});
                return;
            }
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
                timeline_apply_scrub(scrubber.value);
            }

            const auto playbackButtons = timeline_playback_buttons();
            if (const auto action = gui::inline_button_row(
                    playbackButtons, rowHeight, 4.0f, timeline_action_layout(width)))
            {
                timeline_apply_playback(*action);
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
