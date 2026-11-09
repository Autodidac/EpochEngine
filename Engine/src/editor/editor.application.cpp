/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
// Epoch system notes (auto-checked):
// Epoch Context: window/context state, dispatch and input ownership remain scoped.
// Epoch Editor: keep editor state and source changes within their owning workflows.
/**************************************************************
 *   epochengine - Modular C++ Framework
 *   Editor Implementation
 *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
 **************************************************************/
module;

#include "engine.config.hpp"
#include "epoch.edition.hpp"
#include "epoch.build_layout.hpp"
#if EPOCH_EDITION_PUBLIC
#include "ai/editor.ai_file_scope.hpp"
#endif
#include "ai/editor.ai_source_intent.hpp"
#include "ai/editor.ai_session_diagnostics.hpp"
#include "core.format_text.hpp"
#include "../../include/ai.token_metrics.hpp"

#include <include/scripting.epoch_api.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <source_location>
#include <span>
#include <stop_token>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "editor.update_modal_layout.hpp"
#include "editor.frame_performance.hpp"
#include "editor.fps_title_presentation.hpp"
#include "../renderers/host/host.frame_rate_sample.hpp"
#include "../../dep/EpochPlatformEngine/include/build.cpp_feature_probe.hpp"
#include "../ai/ai.native_runtime.hpp"
#include "../ai/ai.voice_runtime.hpp"
#include "ai/editor.gui_text.hpp"
#include "ai/editor.clipboard_screenshot.hpp"
#include "ai/editor.ai_chat_composer_queue.hpp"
#include "ai/editor.ai_activity_presentation.hpp"
#include "ai/editor.ai_chat_goal.hpp"
#include "ai/editor.ai_project_model_recovery.hpp"
#include "ai/editor.build_pressure.hpp"
#include "../render/context.passive_scoring.hpp"

module editor.core;

import gui.engine;
import gui.frame_limit_control;
import epoch.gui;
import editor.hierarchy_adapter;
import editor.code_workspace;
import visuals.engine;
import epoch.version;
import sprite.handle;
import core.context;
import core.env;
import core.path;
import core.logger;
import core.time;
import core.sha256;
import context.commandqueue;
import context.type;
import context.multiplexer;
import input.engine;
import input.controller;
import epoch.cli;
import asset.texture_import;
import asset.texture_artifact;
import authoring.texture;
import authoring.gui_document;
import authoring.gui_compiler;
import capability.profile;
import platform.budgets;
import platform.work_admission;
import project.texture_admission;
import project.texture_pipeline;
import project.texture_resources;
import project.texture_source;
import project.asset_registry;
import project.forest_library;
import project.gui_library;
import project.input_profile;
import editor.project_input_settings;
import project.lifecycle;
import platform.child_process;
import platform.filesystem;
import scripting.system;
import scripting.compiler;
import ai.engine;
import ai.project_profile;
import ai.model_install;
import systems.registry;
import editor.task_scheduler;
import editor.asset_generation;
import editor.systems_panel;
import editor.workspace_layout;
import editor.workspace_commands;
import editor.authoring_history;
import voxel.field;
import forest.factory;
import forest.preview_mesh;
import media.timeline_preview;
import package.catalog;
import package.registry;
import package.source_install;
import project.planetary;
import perf.tier;
import render.arcade;
import editor.canvas2d_scene;
import editor.project_textures;
#if EPOCH_ENABLE_AUTHORING_PLATFORM && EPOCH_ENABLE_TILEMAP_EDITOR
import editor.tilemap_workspace;
#endif
import render.canvas2d;
import render.canvas2d_scene;
import render.device;
import render.lighting;
import render.math;
import render.ray;
import render.preview_grid;
import render.texture_artifact;
import asset.mesh_artifact;
import water.surface;
import water.system;
import project.motion_track;
import asset.model_import;
import project.texture_library;
import saveload.system;
import scene.document;
import scene.interaction;
import scene.persistence;
import scene.surface_alignment;
import scene.tier0;
import scene.snapshot;
import timeline.system;
import updater.config;
import updater.source_access;
import updater.system;

namespace epochengine
{
    namespace
    {
        using namespace std::chrono_literals;

        enum class TopMenu : unsigned char
        {
#include "application/editor.application_state.inl"
#include "application/editor.application_work_admission.inl"
#include "application/editor.application_scene_selection.inl"
        [[nodiscard]] bool editor_automation_input_reserved(const EditorState&, const AiChat&);
#include "application/editor.application_pane_routing.inl"
#include "application/editor.application_scene_transactions.inl"
#include "application/editor.application_layout_persistence.inl"
#include "application/editor.application_runtime_diagnostics.inl"
#include "application/editor.application_project_documents.inl"
#include "application/editor.application_assistant_authoring.inl"
        [[nodiscard]] project_planetary::SceneMeshBudget editor_planetary_mesh_budget(const EditorState& editor);
        [[nodiscard]] bool editor_project_viewport_covered(const EditorState& state) noexcept;
        [[nodiscard]] bool editor_project_viewport_input_reserved(const EditorState& state) noexcept;
        void refresh_editor_planetary_body_inventory(EditorState& editor,const EditorEntity& entity);
        [[nodiscard]] bool editor_planetary_stream_identity_matches(const EditorState& editor,const EditorEntity& entity);
        [[nodiscard]] std::optional<scene_interaction::FocusTarget> editor_planetary_body_focus(
            const EditorState& editor,const EditorEntity& entity);
#include "application/editor.application_scene_preview.inl"
#include "application/editor.application_document_editing.inl"
#include "application/editor.application_scripting_renderers.inl"
#include "application/editor.application_runtime_source.inl"
#include "application/editor.application_code_workspaces.inl"
#include "application/editor.application_asset_browsing.inl"
#include "application/editor.application_gui_authoring.inl"
#include "application/editor.application_texture_authoring.inl"
#include "application/editor.application_asset_audio.inl"
#include "application/editor.application_packages_creation.inl"
#include "application/editor.application_project_runtime.inl"
#include "application/editor.application_models_attachments.inl"
#include "application/editor.application_context_session.inl"
    }

#include "application/editor.application_contracts.inl"
#include "application/editor.application_context_lifecycle.inl"
#include "application/editor.application_context_snapshot.inl"
#include "application/editor.application_detached_panels.inl"
    EditorFrameResult editor_run(const std::shared_ptr<core::Context>& ctx)
    {
#include "application/editor.frame_bootstrap.inl"
        // Session/UI calls are not render-thread TLS. Retain only a weak
        // identity for this exact Editor's ordinary project viewport.
        editor.projectRuntimeViewportContext = ctx;
        // Poll owned local source work even after a modal/context transition.
        // Project-owned attachment and Water prerequisite work can outlive
        // the source modal. Only an idle review follows hidden-modal cleanup;
        // explicit Close/Cancel still revokes its owned operation immediately.
        if (!editor.showPackageManagerModal && !planetary_project_busy(editor))
            cancel_package_source_task(editor);
        poll_package_source_task(editor);
        poll_planetary_project_task(editor);
        poll_asset_generation_task(editor);
        poll_forest_surface_placement(editor);
        pump_planetary_stream(editor, ctx.get());
#include "application/editor.frame_assistant_entry.inl"
#include "application/editor.frame_source_dispatch.inl"
        const auto stopCurrentWork = [&]()
            {
                request_editor_automation_stop(editor);
                epochengine::ai::voice_runtime::stop(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&editor)));
                editor.aiVoiceSession.cancel();
                editor.aiVoiceConversation = false;
                editor.aiProjectScriptRepairQueued = false;
                if (editor.aiProjectScriptRunAfterBuild)
                    (void)cancel_script_build(editor);
                // Cancellation is request-level: do not unload weights.
                // Cancel both live inference and not-yet-dispatched turns.
                const bool hadQueuedRequest = !chat.queuedInput.empty();
                if (hadQueuedRequest)
                {
                    if (chat.input.empty()) chat.input = chat.queuedDraftText;
                    chat.restore_reference_images(chat.queuedReferenceImages);
                    chat.queuedInput.clear();
                    chat.queuedInputAt = {};
                    chat.queuedBlockReported.clear();
                    editor.aiChatQueuedMethodSelection = -1;
                    editor.aiChatQueuedModel.clear();
                }
                bool hadDeferredRequest = false;
                if (editor.aiDeferredRequestKind != AiDeferredRequestKind::None)
                {
                    hadDeferredRequest = true;
                    if (chat.input.empty()) chat.input = editor.aiDeferredDisplay;
                    editor.aiDeferredRequestKind = AiDeferredRequestKind::None;
                    editor.aiDeferredDispatchQueued = false;
                    editor.aiDeferredPrompt.clear();
                    editor.aiDeferredDisplay.clear();
                    editor.aiDeferredModelLease.reset();
                }
                // Interrupt the active transport immediately instead of waiting
                // for the session controller to reach its next host stage.
                const bool hadPendingModelRequest = chat.cancel_pending();
#if EPOCH_EDITION_PUBLIC
                constexpr bool hadPendingCodexTurn = false;
#endif
                if (hadPendingModelRequest || hadPendingCodexTurn)
                    chat.append_status("STOP requested; waiting for the active transport to retire. Model remains loaded.");
                else if (hadQueuedRequest || hadDeferredRequest)
                    chat.append_status("STOP: queued request cancelled before dispatch; draft preserved. Model remains loaded.");
            };
        if (automationEmergencyStopRequested) stopCurrentWork();
#include "application/editor.frame_assistant_planning.inl"
#include "application/editor.frame_assistant_conversation.inl"
#include "application/editor.frame_project_runtime.inl"
#include "application/editor.frame_workspace_docking.inl"
        if(editor.forestPlacementFocusPending)
        {
            editor.forestPlacementFocusPending=false;
            apply_editor_surface(EditorMainSurface::Scene,"Plant surface placement");
            (void)focus_forest_factory_placement(ctx.get(),editor);
        }
        if (editor.planetaryPreviewFocusPending)
        {
            editor.planetaryPreviewFocusPending = false;
            open_editor_surface(EditorMainSurface::Scene, "Planetary Terrain");
            handle_scene_tool(editor, ctx.get(), "focus_selection");
            editor.planetaryStatus = save_editor_scene_snapshot(editor)
                ? "Terrain generated and saved in this project. Close and reopen the project to return to this world."
                : "Terrain generated. Save Project failed; see Output before closing.";
            push_editor_log(editor, "[planet] " + editor.planetaryStatus);
        }
        automationInputReserved = editor_automation_input_reserved(editor, chat);
        if (automationInputReserved)
        {
            result.scene_input_captured = true;
            editor.openMenu = TopMenu::None;
            editor.layoutDrag = EditorLayoutDrag::None;
            editor.layoutDividerDrag = {};
            // End older floating-owner gates before this narrower frame scope;
            // their RAII input state must retire in the original nesting order.
            aiModelManagerBackgroundInput.reset();
            editorSettingsBackgroundInput.reset();
        }
        std::optional<gui::ScopedInputSuppression> automationBackgroundInput;
        if (automationInputReserved) automationBackgroundInput.emplace();
#include "application/editor.frame_toolbar_dock_targets.inl"
#include "application/editor.frame_outliner.inl"
#include "application/editor.frame_inspector.inl"
#include "application/editor.frame_viewport.inl"
#include "application/editor.frame_bottom_docks.inl"
        // Assistant Stop and human approval are never hidden behind the
        // background editor reservation. Native Close stays backend-owned.
        automationBackgroundInput.reset();
#include "application/editor.frame_assistant_chat.inl"
        automationInputReserved = editor_automation_input_reserved(editor, chat);
        if (automationInputReserved)
        {
            result.scene_input_captured = true;
            aiModelManagerBackgroundInput.reset();
            editorSettingsBackgroundInput.reset();
            automationBackgroundInput.emplace();
        }
#include "application/editor.frame_command_menus.inl"
#include "application/editor.frame_consent_settings.inl"
#include "application/editor.frame_model_manager.inl"
#include "application/editor.frame_packages_about.inl"
        automationBackgroundInput.reset();
#include "application/editor.frame_finish.inl"
    }

} // namespace epochenginenamespace
