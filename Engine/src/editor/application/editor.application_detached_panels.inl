// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
    EditorFrameResult editor_run_context_panel(
        const std::shared_ptr<core::Context>& ctx,
        std::string_view route_id)
    {
        EditorFrameResult result{};
        if (!ctx)
            return result;

        auto& editor = editor_state_for(ctx);
        ctx->set_gui_overlay_priority(true);
        // Detached panes share the canonical editor's effective palette.
        const gui::ScopedTheme editorThemeScope{ gui::published_window_theme_variant() };
        const gui::ScopedRoundedRectangles roundedRectScope{ editor.roundedRectangles };
        int resolvedWidth = 0;
        int resolvedHeight = 0;
        if (ctx->windowData)
        {
            resolvedWidth = ctx->windowData->get_width();
            resolvedHeight = ctx->windowData->get_height();
        }
        if (resolvedWidth <= 0 || resolvedHeight <= 0)
        {
            resolvedWidth = ctx->get_width_safe();
            resolvedHeight = ctx->get_height_safe();
        }

        const float w = static_cast<float>((std::max)(1, resolvedWidth));
        const float h = static_cast<float>((std::max)(1, resolvedHeight));
        const auto routedToolPane = editor_tool_pane_from_route(route_id);
        const bool outlinerRoute =
            routedToolPane && is_outliner_tool_pane(*routedToolPane);
        const bool inspectorRoute =
            routedToolPane && is_inspector_tool_pane(*routedToolPane);
        const bool consoleRoute =
            routedToolPane && is_console_tool_pane(*routedToolPane);
        const bool aiChatRoute =
            routedToolPane && *routedToolPane == EditorToolPane::AiChat;
        const bool floatingGuiRoute = route_id == "floating.gui";
        const bool sourcePaneRoute =
            outlinerRoute || inspectorRoute || consoleRoute || aiChatRoute;
        const bool externallyRoutedWorkspace = sourcePaneRoute;
        if (externallyRoutedWorkspace
            && consume_detached_pane_redock_request(route_id))
        {
            editor_mark_context_panel_detached(route_id, false);
            result.command = EditorCommand::Exit;
            result.command_argument = "dock_back";
            result.scene_input_captured = true;
            return result;
        }
        const bool paneRoute = sourcePaneRoute || floatingGuiRoute;
        const DetachedPaneProjection paneProjection =
            externallyRoutedWorkspace
                ? detached_pane_projection()
                : DetachedPaneProjection{};
        const auto& application = editor_application_profile(
            paneProjection.ready
                ? paneProjection.applicationKind
                : editor.applicationKind);
        const bool routeAllowed = application.allow_floating_panes
            && ((outlinerRoute && application.panes.outliner)
                || (inspectorRoute && application.panes.inspector)
                || (consoleRoute && application.panes.console)
                || (aiChatRoute && application.panes.ai_chat)
                || floatingGuiRoute);
        if (paneRoute && !routeAllowed)
        {
            editor_mark_context_panel_detached(route_id, false);
            result.command = EditorCommand::Exit;
            result.command_argument = "application_route_unavailable";
            result.scene_input_captured = true;
            return result;
        }

        const std::string_view toolWindowTitle =
            routedToolPane
                ? editor_tool_pane_labels()[tool_pane_index(*routedToolPane)]
                : std::string_view{};
        const std::string_view windowTitle =
            routedToolPane ? toolWindowTitle
            : floatingGuiRoute ? std::string_view{ "Floating GUI" }
            : std::string_view{ "Context Driver" };
        gui::begin_window(windowTitle, { 0.0f, 0.0f }, { w, h });

        const float contentWidth = (std::max)(1.0f, w - 32.0f);
        // Detached routed panes must never instantiate a second AiChat/model owner.
        // They keep only a local text-edit buffer and communicate with the canonical
        // editor through the bounded mailbox/projection below. This is especially
        // important while a project runtime is active.
        if (paneRoute)
        {
            gui::label(windowTitle);
            gui::property_row("Route", std::string(route_id), 118.0f);
            gui::property_row("Renderer", renderer_name(ctx), 118.0f);
            const std::string& paneProject = paneProjection.ready
                ? paneProjection.projectName
                : editor.projectName;
            gui::property_row("Project", paneProject.empty() ? std::string("(none)") : paneProject, 118.0f);
            gui::property_row(
                "Host",
                paneProjection.ready
                    ? "Live projection from the canonical editor session"
                    : "Detached context projection is waiting for the editor session",
                118.0f);
            if (paneProjection.ready)
            {
                gui::property_row(
                    "Scene revision",
                    std::to_string(paneProjection.sceneRevision),
                    118.0f);
            }

            if (outlinerRoute)
            {
                const auto& paneEntities = paneProjection.ready
                    ? paneProjection.entities
                    : editor.entities;
                const scene::SceneObjectId paneSelection =
                    paneProjection.ready
                        ? paneProjection.selectedEntityId
                        : editor.selectedEntityId;
                std::vector<std::string> lines;
                lines.reserve(paneEntities.size() + 3u);
                lines.push_back(
                    "Scene: "
                    + (paneProjection.ready
                        ? paneProjection.activeWorld
                        : editor.activeWorld));
                lines.push_back(
                    "Entities: " + std::to_string(paneEntities.size()));
                for (const auto& entity : paneEntities)
                {
                    lines.push_back(std::string(entity.sceneObjectId == paneSelection ? "> " : "  ")
                        + entity.name + " | " + entity.type + " | " + entity.category);
                }
                (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                    .id = "popout-world-outliner",
                    .size = { contentWidth, (std::max)(120.0f, h - 178.0f) },
                    .lines = lines,
                    .max_line_chars = 160,
                    .selectable = true
                });
            }
            else if (inspectorRoute)
            {
                if (routedToolPane == EditorToolPane::WorldSettings)
                {
                    const auto view =
                        previewgrid::camera_descriptor_for(ctx.get());
                    const auto resolvedView = render_camera::resolve(view);
                    gui::property_row(
                        "World",
                        paneProjection.ready
                            ? paneProjection.activeWorld
                            : editor.activeWorld,
                        118.0f);
                    gui::property_row("Renderer", renderer_name(ctx), 118.0f);
                    gui::property_row(
                        "View",
                        std::string(previewgrid::editor_view_name(
                            previewgrid::editor_view_for(ctx.get()))),
                        118.0f);
                    gui::property_row(
                        "Projection",
                        std::string(render_camera::projection_name(
                            view.projection)),
                        118.0f);
                    gui::property_row(
                        "View revision",
                        std::to_string(view.source_revision),
                        118.0f);
                    gui::property_row(
                        "View valid",
                        resolvedView.valid ? std::string("true")
                                           : std::string("false"),
                        118.0f);
                    gui::property_row(
                        "Objects",
                        std::to_string(
                            paneProjection.ready
                                ? paneProjection.entities.size()
                                : visible_entity_count(editor)),
                        118.0f);
                }
                else
                {
                const auto& paneEntities = paneProjection.ready
                    ? paneProjection.entities
                    : editor.entities;
                const scene::SceneObjectId paneSelection =
                    paneProjection.ready
                        ? paneProjection.selectedEntityId
                        : editor.selectedEntityId;
                const auto selected = std::find_if(
                    paneEntities.begin(),
                    paneEntities.end(),
                    [&](const EditorEntity& entity)
                    {
                        return entity.sceneObjectId == paneSelection;
                    });
                if (selected != paneEntities.end())
                {
                    const auto& entity = *selected;
                    gui::property_row("Selected", entity.name, 118.0f);
                    gui::property_row("Type", entity.type, 118.0f);
                    gui::property_row("Category", entity.category, 118.0f);
                    gui::property_row("Position", vec3_text(entity.position), 118.0f);
                    gui::property_row("Rotation", vec3_text(entity.rotation), 118.0f);
                    gui::property_row("Scale", vec3_text(entity.scale), 118.0f);
                    gui::property_row("Visible", entity.visible ? std::string("true") : std::string("false"), 118.0f);
                    gui::property_row("EditorOnly", entity.editorOnly ? std::string("true") : std::string("false"), 118.0f);
                }
                else
                {
                    gui::property_row("Selected", std::string("<none>"), 118.0f);
                }
                gui::property_row(
                    "Preview",
                    std::string(preview_mode_name(
                        paneProjection.ready
                            ? paneProjection.previewMode
                            : editor.previewMode)),
                    118.0f);
                gui::property_row("Camera", preview_camera_name(ctx), 118.0f);
                gui::property_row("Zoom", preview_zoom_text(ctx), 118.0f);
                }
            }
            else if (consoleRoute)
            {
                const auto& paneLogs = paneProjection.ready
                    ? paneProjection.logLines
                    : editor.logLines;
                (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                    .id = "popout-console-dock",
                    .size = { contentWidth, (std::max)(120.0f, h - 178.0f) },
                    .lines = paneLogs,
                    .max_line_chars = 1024,
                    .selectable = true,
                    .stick_to_bottom = true
                });
            }
            else if (aiChatRoute)
            {
                static const std::vector<std::string> emptyDetachedChatLines{};
                const auto& paneChatLines = paneProjection.ready
                    ? paneProjection.chatLines
                    : emptyDetachedChatLines;
                const bool queued = paneProjection.ready
                    && (paneProjection.aiDeferredChatQueued
                        || paneProjection.aiDetachedSubmitQueued);
                const bool pending = paneProjection.ready
                    && paneProjection.aiRequestPending;
                (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                    .id = "popout-ai-chat",
                    .size = { contentWidth, (std::max)(80.0f, h - 212.0f
                        - (queued ? 136.0f : 0.0f)
                        - (pending ? 34.0f : 0.0f)) },
                    .lines = paneChatLines,
                    .line_roles = ai_chat_message_roles(paneChatLines),
                    .line_run_details = paneProjection.chatRunDetails
                        ? std::span<const gui::ChatMessageRunInfo>{
                            *paneProjection.chatRunDetails}
                        : std::span<const gui::ChatMessageRunInfo>{},
                    .message_bubbles = true,
                    .conversation_style = true,
                    .max_line_chars = 240,
                    .selectable = true,
                    .stick_to_bottom = true
                });
                const auto inputResult = gui::edit_box(
                    editor.detachedAiChatInput, { contentWidth, 30.0f }, 4096, false);
                if (inputResult.submitted
                    && paneProjection.ready
                    && paneProjection.ownerContext
                    && !editor.detachedAiChatInput.empty()
                    && !is_ws_only(editor.detachedAiChatInput))
                {
                    if (queue_detached_ai_chat_command(
                            paneProjection.ownerContext,
                            DetachedAiChatCommandKind::submit,
                            editor.detachedAiChatInput))
                    {
                        editor.detachedAiChatInput.clear();
                    }
                }
                if (queued)
                {
                    gui::wrapped_label(
                        paneProjection.aiStatus.empty()
                            ? std::string_view{
                                "Message queued for the canonical editor AI session."}
                            : std::string_view{paneProjection.aiStatus},
                        contentWidth);
                    if (paneProjection.aiDeferredChatQueued
                        && !paneProjection.aiDeferredDispatchQueued
                        && gui::button(
                            "Retry Queued Message", {contentWidth, 30.0f}))
                    {
                        (void)queue_detached_ai_chat_command(
                            paneProjection.ownerContext,
                            DetachedAiChatCommandKind::retry_queued);
                    }
                    if (gui::button(
                            "Cancel Queued Message", {contentWidth, 30.0f}))
                    {
                        (void)queue_detached_ai_chat_command(
                            paneProjection.ownerContext,
                            DetachedAiChatCommandKind::cancel_queued);
                    }
                }
                if (pending
                    && gui::button(
                        "Stop Model Request", {contentWidth, 30.0f}))
                {
                    (void)queue_detached_ai_chat_command(
                        paneProjection.ownerContext,
                        DetachedAiChatCommandKind::stop_pending);
                }
            }

            else
            {
                gui::wrapped_label(
                    "Reusable EpochGui host surface routed through a native context panel. This route is infrastructure; current editor window popouts use concrete pane routes.",
                    contentWidth);
            }
        }
        else
        {
            gui::label("Context Driver");
            gui::wrapped_label(
                "Detached context host: the engine owns the native context/window driver, while this routed panel is editor-owned GUI content.",
                contentWidth);
            gui::property_row("Route", std::string(route_id), 132.0f);
            gui::property_row("Renderer", renderer_name(ctx), 132.0f);
            gui::property_row("Version", std::string("v") + epochengine::GetEngineVersionString(), 132.0f);
            gui::property_row("Build", epochengine::GetEngineBuildTagString(), 132.0f);
            gui::property_row("Host", "Detached native context", 132.0f);
            gui::property_row("GUI", "EpochGui primitives through editor adapter", 132.0f);
            const auto availableBackends = available_context_backend_options();
            gui::property_row("Available", context_backend_summary(availableBackends), 132.0f);
            gui::property_row("Context picker", "Editor Settings", 132.0f);
        }


        if (!paneRoute)
        {
            gui::wrapped_label(
                "Floating GUI and context selection are separate: this routed window is a GUI host proof, while backend selection lives in the docked editor toolbar.",
                contentWidth);
        }
        gui::end_window();
        result.scene_input_captured = true;
        return result;
    }

