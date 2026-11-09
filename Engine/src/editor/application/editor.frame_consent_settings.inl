// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell
        if (editor.showVoiceConsentModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalPos = centered_modal_position(
                voiceConsentModalSize);
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Allow Microphone For This Session?",
                .position = modalPos,
                .size = voiceConsentModalSize,
                .viewport_size = {w, h},
                .dim_background = true
            });
            gui::wrapped_label(
                "Epoch will open the configured microphone only for the visible "
                "dictation or conversation session you started. Transcript text "
                "must be reviewed before it reaches the model. Approval expires "
                "when Epoch exits and Cancel stops capture immediately.",
                voiceConsentModalSize.x - 32.0f);
            const std::array voiceConsentActions{
                gui::InlineButtonSpec{.label = "Not Now", .width = 120.0f},
                gui::InlineButtonSpec{
                    .label = "Allow Microphone",
                    .width = 176.0f}
            };
            if (const auto action = gui::inline_button_row(
                    voiceConsentActions, 32.0f, 8.0f))
            {
                const bool approved = *action == 1u;
                (void)editor.aiVoiceSession.resolve_microphone_consent(
                    approved);
                editor.showVoiceConsentModal = false;
                const auto voice = editor.aiVoiceSession.snapshot();
                editor.aiAuthoringStatus = voice.status;
                push_editor_log(editor, "[voice] " + voice.status);
            }
            gui::end_modal_window();
        }
        if (editor.showAiModelConsentModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize = aiModelConsentModalSize;
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Use Local AI?",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = {w, h},
                .dim_background = true
            });

            auto models = poll_ai_model_inventory(editor);
            if (!editor.aiModelInventoryScanned
                && !editor.aiModelInventoryPending)
            {
                request_ai_model_inventory_refresh(editor);
            }
            const bool consentInventoryPending =
                editor.aiModelInventoryPending.has_value();
            const auto manifest = epochengine::ai::active_model_manifest();
            gui::wrapped_label(
                "Epoch discovered a local model service. Discovery is "
                "read-only; no prompt, scene data, script, or microphone "
                "input is sent until you confirm a model for this session.",
                contentWidth);
            gui::property_row("Endpoint", manifest.endpoint, 96.0f);
            gui::property_row(
                "Request",
                    editor.aiDeferredRequestKind
                            == AiDeferredRequestKind::Authoring
                        ? std::string{"Scene/GUI authoring plan"}
                        : editor.aiDeferredRequestKind
                                == AiDeferredRequestKind::Tooling
                            ? std::string{"Active-project tool plan"}
                            : std::string{"AI conversation"},
                96.0f);

            std::vector<std::string_view> modelViews{};
            modelViews.reserve(models.size());
            for (const auto& model : models)
                modelViews.emplace_back(model);
            const auto selection = gui::select_box(
                gui::SelectBoxOptions{
                    .id = "ai-consent-model-select",
                    .placeholder = "Choose a discovered local model",
                    .selected = editor.aiPendingModelSelection,
                    .options = modelViews,
                    .size = {contentWidth, 32.0f},
                    .row_height = 30.0f,
                    .max_visible_options = 7});
            if (selection.changed
                && selection.selected_index
                && *selection.selected_index < models.size())
            {
                editor.aiPendingModelSelection =
                    models[*selection.selected_index];
                editor.aiModelConsentStatus =
                    "Candidate staged; confirm to send the waiting request.";
            }

            gui::wrapped_label(
                editor.aiModelConsentStatus,
                contentWidth);
            gui::wrapped_label(
                "Model approval lasts for this Epoch session. Voice input "
                "requires separate microphone and speech-provider approval.",
                contentWidth);

            const std::array actions{
                gui::InlineButtonSpec{
                    .label = consentInventoryPending
                        ? "Scanning..."
                        : "Rescan",
                    .width = 116.0f,
                    .enabled = !consentInventoryPending},
                gui::InlineButtonSpec{
                    .label = "Not Now",
                    .width = 112.0f},
                gui::InlineButtonSpec{
                    .label = "Use Model",
                    .width = 128.0f,
                    .enabled = !consentInventoryPending
                        && !editor.aiPendingModelSelection.empty()}
            };
            if (const auto action = gui::inline_button_row(
                    actions,
                    32.0f,
                    8.0f))
            {
                if (*action == 0u)
                {
                    request_ai_model_inventory_refresh(editor);
                    models.clear();
                }
                else if (*action == 1u)
                {
                    editor.showAiModelConsentModal = false;
                    editor.aiDeferredPrompt.clear();
                    editor.aiDeferredDisplay.clear();
                    editor.aiDeferredRequestKind =
                        AiDeferredRequestKind::None;
                    editor.aiDeferredModelLease.reset();
                    editor.aiDeferredDispatchQueued = false;
                    editor.aiModelConsentStatus =
                        "Local-model use was not approved.";
                    push_editor_log(
                        editor,
                        "[ai] Local-model request cancelled before any prompt was sent.");
                }
                else if (confirm_pending_ai_model(editor))
                {
                    editor.aiDeferredDispatchQueued = true;
                    editor.showAiModelConsentModal = false;
                }
            }
            gui::end_modal_window();
        }
        if (editor.showSettingsModal && editorSettingsOwnsInput)
        {
            editor.openMenu = TopMenu::None;
            editorSettingsBackgroundInput.reset();
            const auto settingsFloating = gui::begin_floating_window(
                editor.editorSettingsWindow, editorSettingsWindowOptions);
            if (settingsFloating.begun)
            {
                const float settingsWidth = (std::max)(1.0f, settingsFloating.content.size.x - 12.0f);
                const std::array settingsFooterActions{
                    gui::InlineButtonSpec{.label="Reset Layout"},
                    gui::InlineButtonSpec{.label="Model Manager"},
                    gui::InlineButtonSpec{.label="Close"}};
                const gui::InlineButtonRowOptions settingsFooterLayout{
                    .sizing=gui::InlineButtonSizing::FitContent,
                    .alignment=gui::InlineButtonAlignment::Center,
                    .wrap=true, .available_width=settingsWidth};
                const float settingsFooterHeight = gui::inline_button_row_height(
                    settingsFooterActions, 30.0f, 6.0f, settingsFooterLayout);
                const float settingsFooterY = settingsFloating.content.position.y
                    + settingsFloating.content.size.y - 6.0f - settingsFooterHeight;
                const gui::Vec2 settingsViewport{
                    settingsFloating.content.position.x + 6.0f, settingsFloating.content.position.y};
                const float settingsBodyHeight = (std::max)(1.0f,
                    settingsFooterY - settingsViewport.y - 12.0f);
                gui::set_cursor(settingsViewport);
                (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                    .id="epoch-editor-settings-body",
                    .size={settingsWidth, settingsBodyHeight},
                    .content_height=0.0f,
                    .draw_background=true, .show_scrollbar=true});
                const gui::Vec2 contentPos = gui::cursor_position();
                const float contentWidth = (std::max)(1.0f, settingsWidth - 26.0f);
                const float settingsControlWidth = (std::min)(contentWidth, 720.0f);
                const gui::InlineButtonRowOptions settingsActionLayout{
                    .sizing=gui::InlineButtonSizing::FitContent,
                    .alignment=gui::InlineButtonAlignment::Center,
                    .wrap=true, .available_width=contentWidth};
                const auto settings_row = [&](float gap = 8.0f) {
                    gui::set_cursor({contentPos.x + 8.0f, gui::cursor_position().y + gap});
                };
                settings_row(4.0f);
                gui::wrapped_label("Editor defaults and active project runtime policy.", contentWidth);
                settings_row();
                gui::section_heading("Renderer & capabilities");
                settings_row(0.0f);
                gui::wrapped_label(std::string{"Active backend: "} + std::string{renderer_name(ctx)}, contentWidth);
                settings_row();

                const auto settingsContextOptions = available_context_backend_options();
                const bool contextReplacementInProgress = editor_context_replacement_in_progress();
                std::vector<const ContextBackendOption*> settingsSelectableContexts;
                std::vector<std::string_view> settingsContextLabels;
                settingsSelectableContexts.reserve(settingsContextOptions.size());
                settingsContextLabels.reserve(settingsContextOptions.size());
                for (const auto& option : settingsContextOptions)
                {
                    if (!option.selectable)
                        continue;
                    settingsSelectableContexts.push_back(&option);
                    settingsContextLabels.emplace_back(option.label);
                }

                if (contextReplacementInProgress)
                {
                    gui::property_row("Backend selection", "Replacement in progress", 132.0f);
                }
                else if (!settingsSelectableContexts.empty())
                {
                    const auto settingsContextSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "editor-settings-context-select",
                        .placeholder = "Editor backend",
                        .selected = renderer_name(ctx),
                        .options = std::span<const std::string_view>{ settingsContextLabels.data(), settingsContextLabels.size() },
                        .size = { settingsControlWidth, 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 7
                    });
                    if (settingsContextSelect.changed
                        && settingsContextSelect.selected_index
                        && *settingsContextSelect.selected_index < settingsSelectableContexts.size())
                    {
                        request_context_backend_selection(
                            *settingsSelectableContexts[*settingsContextSelect.selected_index]);
                    }
                }
                else
                {
                    gui::property_row("Backend selection", "Unavailable on this host", 132.0f);
                }
                // Backends and capability gates are full-width rows, not competing
                // overlapping columns. Each row advances the cursor by its actual
                // wrapped font height; this works in both compact and 4K layouts.
                settings_row();
                gui::wrapped_label(
                    std::string{"Compiled: "} + context_backend_summary(settingsContextOptions),
                    contentWidth);
                const auto& projectProfile = active_project_profile(editor);
                settings_row(4.0f);
                gui::wrapped_label(std::string{"Editor gate: "} + renderer_admission_summary(
                    renderer_backend_kind(ctx), projectProfile.renderer_capability), contentWidth);
                settings_row(4.0f);
                gui::wrapped_label(std::string{"Run gate: "} + renderer_admission_summary(
                    renderer_backend_kind(editor.projectRunBackend),
                    projectProfile.renderer_capability), contentWidth);
                const std::string disabledBackends = disabled_context_backend_summary(settingsContextOptions);
                settings_row(4.0f);
                gui::wrapped_label(
                    disabledBackends == "none"
                        ? editor.contextSelectionStatus
                        : std::string{"Disabled: "} + disabledBackends + ". " + editor.contextSelectionStatus,
                    contentWidth);
                settings_row(14.0f);
                gui::section_heading("Appearance");
                settings_row(0.0f);
                gui::label("Theme");

                const auto settingsThemeChoices = gui::theme_preference_choices();
                std::vector<std::string_view> settingsThemeLabels;
                settingsThemeLabels.reserve(settingsThemeChoices.size());
                for (const auto& choice : settingsThemeChoices)
                    settingsThemeLabels.emplace_back(choice.label);
                const auto settingsThemeSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "editor-theme-select",
                    .placeholder = "Theme",
                    .selected = gui::theme_preference_label(editor.themePreference),
                    .options = std::span<const std::string_view>{ settingsThemeLabels.data(), settingsThemeLabels.size() },
                    .size = { settingsControlWidth, 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 8
                });
                if (settingsThemeSelect.changed && settingsThemeSelect.selected_index && *settingsThemeSelect.selected_index < settingsThemeChoices.size())
                {
                    editor.themePreference = settingsThemeChoices[*settingsThemeSelect.selected_index].preference;
                    push_editor_log(editor, std::string("[settings] Theme set to ") + std::string(gui::theme_preference_label(editor.themePreference)) + ".");
                }

                if (editor.themePreference == gui::ThemePreference::ThemeCycle)
                {
                    const auto cycleChoices = gui::theme_cycle_interval_choices();
                    std::vector<std::string_view> intervalLabels;
                    intervalLabels.reserve(cycleChoices.size());
                    for (const auto& choice : cycleChoices)
                        intervalLabels.emplace_back(choice.label);
                    settings_row();
                    gui::label("Theme cycle interval");
                    const auto intervalSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "editor-theme-cycle-interval-select",
                        .placeholder = "Cycle interval",
                        .selected = gui::theme_cycle_interval_label(editor.themeCycleIntervalSeconds),
                        .options = std::span<const std::string_view>{ intervalLabels.data(), intervalLabels.size() },
                        .size = { settingsControlWidth, 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 6
                    });
                    if (intervalSelect.changed && intervalSelect.selected_index
                        && *intervalSelect.selected_index < cycleChoices.size())
                    {
                        editor.themeCycleIntervalSeconds = cycleChoices[*intervalSelect.selected_index].seconds;
                        gui::set_theme_cycle_interval(editor.themeCycleIntervalSeconds);
                        push_editor_log(editor, "[settings] Theme cycle interval set to "
                            + std::string(gui::theme_cycle_interval_label(editor.themeCycleIntervalSeconds)) + ".");
                    }
                }

                settings_row();
                if (gui::toggle_switch(
                    "Rounded rectangles",
                    editor.roundedRectangles,
                    { settingsControlWidth, 30.0f }))
                {
                    push_editor_log(
                        editor,
                        editor.roundedRectangles
                            ? "[settings] Rounded GUI rectangles enabled."
                            : "[settings] Rounded GUI rectangles disabled.");
                }
                settings_row(14.0f);
                gui::section_heading("Input");
                settings_row(0.0f);
                gui::label("Shared input profile");
                const auto settingsInputChoices = input_profile_choices();
                std::vector<std::string_view> settingsInputLabels;
                settingsInputLabels.reserve(settingsInputChoices.size());
                for (const auto& choice : settingsInputChoices)
                    settingsInputLabels.emplace_back(choice.label);
                const auto settingsInputSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "editor-input-profile-select",
                    .placeholder = "Input profile",
                    .selected = input_profile_label(editor.inputProfilePreset),
                    .options = std::span<const std::string_view>{ settingsInputLabels.data(), settingsInputLabels.size() },
                    .size = { settingsControlWidth, 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 4
                });
                if (settingsInputSelect.changed && settingsInputSelect.selected_index && *settingsInputSelect.selected_index < settingsInputChoices.size())
                {
                    editor.inputProfilePreset = settingsInputChoices[*settingsInputSelect.selected_index].preset;
                    input::set_active_profile(editor.inputProfilePreset);
                    push_editor_log(editor, std::string("[input] Shared profile set to ") + std::string(input_profile_label(editor.inputProfilePreset)) + ".");
                }
                settings_row();
                if (gui::inline_button_row(std::array{gui::InlineButtonSpec{
                        .label=has_open_project(editor)
                            ? "Open Project Input Manager" : "Project Input: No Project"}},
                        30.0f, 6.0f, settingsActionLayout))
                {
                    editor.projectWorkspaceSection = 1u;
                    open_editor_surface(EditorMainSurface::Project, "Project input settings");
                    editor.showSettingsModal = false;
                    push_editor_log(editor, "[settings] Opened the active project's Input Manager.");
                }
                settings_row(14.0f);
                gui::section_heading("Frame pacing");
                settings_row(0.0f);
                gui::label("Editor frame limit");
                if (frame_limit_control("editor-core-frame-limit-select", editor.editorFrameLimitFps,
                        editor.editorFrameLimitEdit, settingsControlWidth, editor.editorFrameVsyncRequested))
                {
                    editor.editorFrameVsyncRequested = false;
                    if (ctx->windowData)
                        ctx->windowData->editorFramePacingOverride.publish(
                            editor.editorFrameLimitFps > 0.0 ? perf::frame_pacing_mode::target_hz
                                : perf::frame_pacing_mode::uncapped,
                            editor.editorFrameLimitFps);
                    push_editor_log(editor, std::string("[settings] Editor frame limit set to ") + std::string(frame_limit_label(editor.editorFrameLimitFps)) + ".");
                }
                settings_row();
                gui::label("Project Run frame limit");
                if (frame_limit_control("settings-project-frame-limit-select", editor.projectRunFrameLimitFps,
                        editor.projectFrameLimitEdit, settingsControlWidth))
                {
                    persist_project_run_settings(editor);
                    push_editor_log(editor, "[project] Project frame limit set to "
                        + frame_limit_argument(editor.projectRunFrameLimitFps) + " FPS.");
                }

                settings_row(14.0f);
                gui::section_heading("Camera & preview");
                settings_row(0.0f);
                const auto settingsCameraChoices = project_camera_choices();
                std::vector<std::string_view> settingsCameraLabels;
                settingsCameraLabels.reserve(settingsCameraChoices.size());
                for (const auto& choice : settingsCameraChoices)
                    settingsCameraLabels.emplace_back(choice.label);

                gui::label("Editor camera controls");

                const bool editorCameraAvailable = ctx && !editor_project_viewport_input_reserved(editor);
                const auto settingsEditorCameraSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "settings-editor-camera-select",
                    .placeholder = "Editor camera controls",
                    .selected = project_camera_label(editor.viewportCameraMode),
                    .options = std::span<const std::string_view>{ settingsCameraLabels.data(), settingsCameraLabels.size() },
                    .size = { settingsControlWidth, 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 3,
                    .enabled = editorCameraAvailable
                });
                if (editorCameraAvailable && settingsEditorCameraSelect.changed && settingsEditorCameraSelect.selected_index
                    && *settingsEditorCameraSelect.selected_index < settingsCameraChoices.size())
                {
                    editor.viewportCameraMode = settingsCameraChoices[*settingsEditorCameraSelect.selected_index].mode;
                    previewgrid::set_camera_mode(ctx.get(), editor.viewportCameraMode);
                    push_editor_log(editor, std::string("[settings] Editor camera controls set to ")
                        + std::string(settingsCameraChoices[*settingsEditorCameraSelect.selected_index].label) + ".");
                }
                settings_row();
                gui::label("Project Run camera");

                const auto settingsCameraSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "settings-project-camera-select",
                    .placeholder = "Project Run camera",
                    .selected = project_camera_label(editor.projectCameraMode),
                    .options = std::span<const std::string_view>{ settingsCameraLabels.data(), settingsCameraLabels.size() },
                    .size = { settingsControlWidth, 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 3
                });
                if (settingsCameraSelect.changed && settingsCameraSelect.selected_index && *settingsCameraSelect.selected_index < settingsCameraChoices.size())
                {
                    editor.projectCameraMode = settingsCameraChoices[*settingsCameraSelect.selected_index].mode;
                    if (editor.projectCameraMode == previewgrid::CameraMode::Canvas2D)
                        ensure_2d_canvas_entity(editor);
                    push_editor_log(editor, std::string("[project] Camera style set to ") + std::string(settingsCameraChoices[*settingsCameraSelect.selected_index].label) + ".");
                }

                settings_row();
                const std::array previewActions{
                    gui::InlineButtonSpec{.label=editor.previewMode == core::ScenePreviewMode::Editor
                        ? "Disable Preview" : "Enable Preview"},
                    gui::InlineButtonSpec{.label=editor.helpersVisible ? "Hide Helpers" : "Show Helpers"},
                    gui::InlineButtonSpec{.label="Reset Camera"}};
                if (const auto action = gui::inline_button_row(
                        previewActions, 30.0f, 6.0f, settingsActionLayout))
                {
                    if (*action == 0u)
                    {
                        editor.previewMode = editor.previewMode == core::ScenePreviewMode::Editor
                            ? core::ScenePreviewMode::None : core::ScenePreviewMode::Editor;
                        if (ctx)
                            ctx->set_scene_preview_mode(editor_project_viewport_input_reserved(editor)
                                ? core::ScenePreviewMode::None : editor.previewMode);
                    }
                    else if (*action == 1u)
                        handle_scene_tool(editor, ctx.get(), "toggle_helpers");
                    else
                        handle_scene_tool(editor, ctx.get(), "reset_camera");
                }
                settings_row();
                gui::end_scroll_area();
                gui::set_cursor({settingsViewport.x, settingsFooterY});
                if (const auto action = gui::inline_button_row(
                        settingsFooterActions, 30.0f, 6.0f, settingsFooterLayout))
                {
                    if (*action == 0u)
                    {
                        reset_editor_layout(editor);
                        push_editor_log(editor, "[settings] Editor layout reset.");
                    }
                    else if (*action == 1u)
                    {
                        editor.showAiModelManagerModal = true;
                        editor.aiModelManagerSection = 0;
                        editor.aiModelManagerTab = editor.aiMethodSelection == 1 ? 1 : editor.aiMethodSelection == 2 ? 2 : 0;
                        editor.showSettingsModal = false;
                    }
                    else
                        editor.showSettingsModal = false;
                }
                gui::end_floating_window();
            }
            if (settingsFloating.close_requested || !editor.editorSettingsWindow.open)
                editor.showSettingsModal = false;
            // Even Close must not pass this frame's release to later controls.
            if (!automationInputReserved) editorSettingsBackgroundInput.emplace();
        }
