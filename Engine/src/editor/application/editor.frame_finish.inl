// Epoch feature implementation; included by editor.core application owner only.
// SPDX-License-Identifier: LicenseRef-MIT-NoSell


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
        ctx->set_gui_overlay_priority(editor.openMenu != TopMenu::None || modal_visible_now() || floating_gui_visible());
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
