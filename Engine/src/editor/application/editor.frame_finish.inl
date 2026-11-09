// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell


        const bool exitedProjectFullScreen = !automationInputReserved && project_runtime_viewport_maximized(editor) && !modal_visible_now()
            && editor.openMenu == TopMenu::None && !floating_gui_visible()
            && gui::dispatch_keyboard_shortcuts([&](const gui::InputEvent& event)
                {
                    if (event.key != 27) return false;
                    exit_project_runtime_full_screen(editor);
                    return true;
                });
        if (exitedProjectFullScreen)
            result.scene_input_captured = true;

        const bool shortcutInputAvailable = workspaceShortcutInputAvailable
            && !automationInputReserved
            && !exitedProjectFullScreen
            && !modal_visible_now()
            && editor.openMenu == TopMenu::None
            && !floating_gui_visible()
            && !gui::keyboard_input_captured();
        if (shortcutInputAvailable)
        {
            // Modifiers belong to the queued key press, not the current
            // sampler state after a fast chord has already been released.
            if (gui::dispatch_keyboard_shortcuts([&](const gui::InputEvent& event)
                {
                    if (!event.ctrl_down) return false;
                    if (event.key == 'Z' || event.key == 'z')
                    {
                        (void)execute_surface_edit(editor, ctx.get(), event.shift_down
                            ? SurfaceEditAction::redo : SurfaceEditAction::undo);
                        return true;
                    }
                    if (event.key == 'Y' || event.key == 'y')
                    {
                        (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::redo);
                        return true;
                    }
                    return false;
                }))
            {
                result.scene_input_captured = true;
            }
            else if (ctx->is_key_down_safe(input::Key::Delete))
            {
                (void)execute_surface_edit(editor, ctx.get(), SurfaceEditAction::delete_selection);
                result.scene_input_captured = true;
            }
            else if (ctx->is_key_down_safe(input::Key::Escape)
                && !editor_project_viewport_input_reserved(editor)
                && editor_has_selection(editor))
            {
                clear_editor_selection(editor);
                push_editor_log(editor, "[edit] Selection cleared.");
                result.scene_input_captured = true;
            }
        }

        const auto deferredNative=epochengine::ai::native_runtime::snapshot(false);
        const bool deferredMethodReady=editor.aiMethodSelection==0
            ?deferredNative.selected&&deferredNative.loaded&&!deferredNative.busy
            :editor.aiMethodSelection==2?epochengine::ai::direct_runtime_status().ready()
            :editor.aiMethodSelection==1&&epochengine::ai::is_model_use_confirmed();
        if(editor.aiDeferredRequestKind!=AiDeferredRequestKind::None&&!editor.showAiModelConsentModal&&deferredMethodReady)
            editor.aiDeferredDispatchQueued=true;
        if (editor.aiDeferredDispatchQueued)
            (void)dispatch_deferred_ai_request();

        frameTimer.mark_ui_done();
        persist_editor_layout_preferences(editor);
        ctx->set_gui_overlay_priority(editor.openMenu != TopMenu::None || editor.outlinerContextMenu.menu.popup.open || modal_visible_now() || floating_gui_visible() || gui::select_box_popup_open());
        (void)present_project_runtime_viewport(editor);
        if (const auto perf = frameTimer.finish())
        {
            logger::get("Engine.Editor.Performance").log(
                logger::LogLevel::INFO,
                epochengine::format_text(
                    "[frame] samples={} avg={:.2f} ms max={:.2f} ms "
                    "initial={:.2f} ms pre-ui={:.2f} ms "
                    "ui={:.2f} ms persist={:.2f} ms; "
                    "CPU-side editor time only, excludes GPU present/wait",
                    perf->count, perf->average_ms, perf->maximum_ms,
                    perf->initial_average_ms, perf->pre_ui_average_ms,
                    perf->ui_average_ms, perf->persist_average_ms));
        }

        return result;
