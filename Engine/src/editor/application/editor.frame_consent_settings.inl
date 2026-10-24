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
        if (editor.showSettingsModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize = editorSettingsModalSize;
            const gui::Vec2 modalPos = editorSettingsModalPos;
            // Leave a little room for the settings scrollbar and keep the
            // footer outside the scrolled settings page.
            const float contentWidth = modalSize.x - 58.0f;
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Editor Settings",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 settingsViewport = gui::cursor_position();
            const float settingsBodyHeight = (std::max)(1.0f,
                modalPos.y + modalSize.y - 76.0f - settingsViewport.y);
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = "epoch-editor-settings-body",
                .size = {modalSize.x - 36.0f, settingsBodyHeight},
                .content_height = 0.0f, // measured from actual widget layout
                .draw_background = false,
                .show_scrollbar = true});
            const gui::Vec2 contentPos = gui::cursor_position();
            const float contentY = contentPos.y;
            gui::set_cursor({ contentPos.x + 8.0f, contentY });
            gui::wrapped_label("Editor defaults and active project runtime policy.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, gui::cursor_position().y + 9.0f });
            gui::property_row("Active backend", renderer_name(ctx), 116.0f);
            const float contextSelectY = gui::cursor_position().y + 8.0f;

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

            gui::set_cursor({ contentPos.x + 8.0f, contextSelectY });
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
                    .size = { 300.0f, 30.0f },
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
            gui::set_cursor({contentPos.x + 8.0f,
                gui::cursor_position().y + 11.0f});
            gui::wrapped_label(
                std::string{"Compiled: "} + context_backend_summary(settingsContextOptions),
                contentWidth);
            const auto& projectProfile = active_project_profile(editor);
            gui::set_cursor({contentPos.x + 8.0f, gui::cursor_position().y + 7.0f});
            gui::wrapped_label(std::string{"Editor gate: "} + renderer_admission_summary(
                renderer_backend_kind(ctx), projectProfile.renderer_capability), contentWidth);
            gui::set_cursor({contentPos.x + 8.0f, gui::cursor_position().y + 5.0f});
            gui::wrapped_label(std::string{"Run gate: "} + renderer_admission_summary(
                renderer_backend_kind(editor.projectRunBackend),
                projectProfile.renderer_capability), contentWidth);
            const std::string disabledBackends = disabled_context_backend_summary(settingsContextOptions);
            gui::set_cursor({contentPos.x + 8.0f, gui::cursor_position().y + 7.0f});
            gui::wrapped_label(
                disabledBackends == "none"
                    ? editor.contextSelectionStatus
                    : std::string{"Disabled: "} + disabledBackends + ". " + editor.contextSelectionStatus,
                contentWidth);
            const float themeY = gui::cursor_position().y + 18.0f;

            const auto settingsThemeChoices = gui::theme_preference_choices();
            std::vector<std::string_view> settingsThemeLabels;
            settingsThemeLabels.reserve(settingsThemeChoices.size());
            for (const auto& choice : settingsThemeChoices)
                settingsThemeLabels.emplace_back(choice.label);
            const bool compactSettings = modalSize.x < 620.0f;
            const float themeControlWidth = compactSettings ? 200.0f : 300.0f;
            gui::set_cursor({ contentPos.x + 8.0f, themeY + 0.0f });
            const auto settingsThemeSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "editor-theme-select",
                .placeholder = "Theme",
                .selected = gui::theme_preference_label(editor.themePreference),
                .options = std::span<const std::string_view>{ settingsThemeLabels.data(), settingsThemeLabels.size() },
                .size = { themeControlWidth, 30.0f },
                .row_height = 28.0f,
                .max_visible_options = 8
            });
            if (settingsThemeSelect.changed && settingsThemeSelect.selected_index && *settingsThemeSelect.selected_index < settingsThemeChoices.size())
            {
                editor.themePreference = settingsThemeChoices[*settingsThemeSelect.selected_index].preference;
                push_editor_log(editor, std::string("[settings] Theme set to ") + std::string(gui::theme_preference_label(editor.themePreference)) + ".");
            }

            const float roundingControlX = compactSettings ? 216.0f : 326.0f;
            const float roundingControlWidth = (std::max)(
                156.0f,
                contentWidth - roundingControlX - 8.0f);
            if (editor.themePreference == gui::ThemePreference::ThemeCycle)
            {
                const auto cycleChoices = gui::theme_cycle_interval_choices();
                std::vector<std::string_view> intervalLabels;
                intervalLabels.reserve(cycleChoices.size());
                for (const auto& choice : cycleChoices)
                    intervalLabels.emplace_back(choice.label);
                gui::set_cursor({ contentPos.x + roundingControlX, themeY + 0.0f });
                const auto intervalSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "editor-theme-cycle-interval-select",
                    .placeholder = "Cycle interval",
                    .selected = gui::theme_cycle_interval_label(editor.themeCycleIntervalSeconds),
                    .options = std::span<const std::string_view>{ intervalLabels.data(), intervalLabels.size() },
                    .size = { roundingControlWidth, 30.0f },
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

            const auto settingsInputChoices = input_profile_choices();
            std::vector<std::string_view> settingsInputLabels;
            settingsInputLabels.reserve(settingsInputChoices.size());
            for (const auto& choice : settingsInputChoices)
                settingsInputLabels.emplace_back(choice.label);
            gui::set_cursor({ contentPos.x + 8.0f, themeY + 46.0f });
            const auto settingsInputSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "editor-input-profile-select",
                .placeholder = "Input profile",
                .selected = input_profile_label(editor.inputProfilePreset),
                .options = std::span<const std::string_view>{ settingsInputLabels.data(), settingsInputLabels.size() },
                .size = { compactSettings ? 200.0f : 300.0f, 30.0f },
                .row_height = 28.0f,
                .max_visible_options = 4
            });
            if (settingsInputSelect.changed && settingsInputSelect.selected_index && *settingsInputSelect.selected_index < settingsInputChoices.size())
            {
                editor.inputProfilePreset = settingsInputChoices[*settingsInputSelect.selected_index].preset;
                input::set_active_profile(editor.inputProfilePreset);
                push_editor_log(editor, std::string("[input] Shared profile set to ") + std::string(input_profile_label(editor.inputProfilePreset)) + ".");
            }
            gui::set_cursor({ contentPos.x + roundingControlX, themeY + 46.0f });
            if (gui::toggle_switch(
                compactSettings ? "Rounded GUI" : "Rounded rectangles",
                editor.roundedRectangles,
                { roundingControlWidth, 30.0f }))
            {
                push_editor_log(
                    editor,
                    editor.roundedRectangles
                        ? "[settings] Rounded GUI rectangles enabled."
                        : "[settings] Rounded GUI rectangles disabled.");
            }
            if (modalSize.x >= 620.0f)
            {
                // Cycling puts the interval selector beside the Theme field;
                // move this secondary project action below, not over Rounded GUI.
                const bool cycling = editor.themePreference == gui::ThemePreference::ThemeCycle;
                gui::set_cursor({ contentPos.x + (cycling ? 336.0f : 326.0f),
                    themeY + (cycling ? 184.0f : 50.0f) });
                if (gui::button(
                        has_open_project(editor)
                            ? "Open Project Input Manager"
                            : "Project Input: No Project",
                        { roundingControlWidth - (cycling ? 10.0f : 0.0f), 30.0f }))
                {
                    editor.projectWorkspaceSection = 1u;
                    open_editor_surface(
                        EditorMainSurface::Project,
                        "Project input settings");
                    editor.showSettingsModal = false;
                    push_editor_log(
                        editor,
                        "[settings] Opened the active project's Input Manager.");
                }
            }

            gui::set_cursor({ contentPos.x + 8.0f, themeY + 92.0f });
            if (frame_limit_control("editor-core-frame-limit-select", editor.editorFrameLimitFps,
                    editor.editorFrameLimitEdit, modalSize.x - 32.0f, editor.editorFrameVsyncRequested))
            {
                editor.editorFrameVsyncRequested = false;
                if (ctx->windowData)
                    ctx->windowData->editorFramePacingOverride.publish(
                        editor.editorFrameLimitFps > 0.0 ? perf::frame_pacing_mode::target_hz
                            : perf::frame_pacing_mode::uncapped,
                        editor.editorFrameLimitFps);
                push_editor_log(editor, std::string("[settings] Editor frame limit set to ") + std::string(frame_limit_label(editor.editorFrameLimitFps)) + ".");
            }
            gui::set_cursor({ contentPos.x + 8.0f, themeY + 138.0f });
            if (frame_limit_control("settings-project-frame-limit-select", editor.projectRunFrameLimitFps,
                    editor.projectFrameLimitEdit, modalSize.x - 32.0f))
            {
                push_editor_log(editor, "[project] Project frame limit set to "
                    + frame_limit_argument(editor.projectRunFrameLimitFps) + " FPS.");
            }

            const auto settingsCameraChoices = project_camera_choices();
            std::vector<std::string_view> settingsCameraLabels;
            settingsCameraLabels.reserve(settingsCameraChoices.size());
            for (const auto& choice : settingsCameraChoices)
                settingsCameraLabels.emplace_back(choice.label);
            gui::set_cursor({ contentPos.x + 8.0f, themeY + 184.0f });
            const auto settingsCameraSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "settings-project-camera-select",
                .placeholder = "Camera style",
                .selected = project_camera_label(editor.projectCameraMode),
                .options = std::span<const std::string_view>{ settingsCameraLabels.data(), settingsCameraLabels.size() },
                .size = { 320.0f, 30.0f },
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

            const float previewControlWidth = (std::min)(146.0f, (contentWidth - 20.0f) / 3.0f);
            gui::set_cursor({ contentPos.x + 8.0f, themeY + 234.0f });
            if (gui::button(editor.previewMode == core::ScenePreviewMode::Editor ? "Disable Preview" : "Enable Preview", { previewControlWidth, 30.0f }))
            {
                editor.previewMode = editor.previewMode == core::ScenePreviewMode::Editor
                    ? core::ScenePreviewMode::None
                    : core::ScenePreviewMode::Editor;
                if (ctx)
                    ctx->set_scene_preview_mode(editor.previewMode);
            }
            gui::set_cursor({ contentPos.x + 18.0f + previewControlWidth, themeY + 234.0f });
            if (gui::button(editor.helpersVisible ? "Hide Helpers" : "Show Helpers", { previewControlWidth, 30.0f }))
                handle_scene_tool(editor, ctx.get(), "toggle_helpers");
            gui::set_cursor({ contentPos.x + 28.0f + previewControlWidth * 2.0f, themeY + 234.0f });
            if (gui::button("Reset Camera", { previewControlWidth, 30.0f }))
                handle_scene_tool(editor, ctx.get(), "reset_camera");


            gui::set_cursor({contentPos.x + 8.0f, themeY + 283.0f});
            gui::end_scroll_area();
            const float actionY = modalPos.y + modalSize.y - 54.0f;
            const float actionLeft = settingsViewport.x + 8.0f;
            const float actionRight = modalPos.x + modalSize.x - 32.0f;
            const float actionGap = 10.0f;
            const float actionScale = std::clamp(
                (actionRight - actionLeft - 2.0f * actionGap) / 368.0f, 0.0f, 1.0f);
            const float resetWidth = 132.0f * actionScale;
            const float aiWidth = 148.0f * actionScale;
            const float closeWidth = 88.0f * actionScale;
            gui::set_cursor({ actionLeft, actionY });
            if (gui::button(actionScale < 0.7f ? "Reset" : "Reset Layout", { resetWidth, 30.0f }))
            {
                reset_editor_layout(editor);
                push_editor_log(editor, "[settings] Editor layout reset.");
            }
            gui::set_cursor({ actionLeft + resetWidth + actionGap, actionY });
            if (gui::button(actionScale < 0.7f ? "Models" : "Model Manager", { aiWidth, 30.0f }))
            {
                editor.showAiModelManagerModal = true;
                editor.aiModelManagerSection = 0;
                editor.aiModelManagerTab = editor.aiMethodSelection == 1 ? 1 : editor.aiMethodSelection == 2 ? 2 : 0;
                editor.showSettingsModal = false;
            }
            gui::set_cursor({ actionRight - closeWidth, actionY });
            if (gui::button("Close", { closeWidth, 30.0f }))
                editor.showSettingsModal = false;
            gui::end_modal_window();
        }
