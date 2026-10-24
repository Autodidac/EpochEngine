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
#include "ai/editor.build_pressure.hpp"
#include "../render/context.passive_scoring.hpp"

module editor.core;

import gui.engine;
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
import ai.model_install;
import systems.registry;
import editor.task_scheduler;
import editor.systems_panel;
import editor.workspace_layout;
import editor.workspace_commands;
import editor.authoring_history;
import voxel.field;
import forest.factory;
import media.timeline_preview;
import package.catalog;
import package.registry;
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
import render.event_debug;
import render.lighting;
import render.math;
import render.ray;
import render.preview_grid;
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
#include "application/editor.application_pane_routing.inl"
#include "application/editor.application_scene_transactions.inl"
#include "application/editor.application_layout_persistence.inl"
#include "application/editor.application_runtime_diagnostics.inl"
#include "application/editor.application_project_documents.inl"
#include "application/editor.application_assistant_authoring.inl"
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
#include "application/editor.frame_assistant_entry.inl"
#include "application/editor.frame_source_dispatch.inl"
#include "application/editor.frame_assistant_planning.inl"
#include "application/editor.frame_assistant_conversation.inl"
#include "application/editor.frame_project_runtime.inl"
#include "application/editor.frame_workspace_docking.inl"
#include "application/editor.frame_toolbar_dock_targets.inl"
#include "application/editor.frame_outliner.inl"
#include "application/editor.frame_inspector.inl"
#include "application/editor.frame_viewport.inl"
#include "application/editor.frame_bottom_docks.inl"
#include "application/editor.frame_assistant_chat.inl"
#include "application/editor.frame_command_menus.inl"
#include "application/editor.frame_consent_settings.inl"
#include "application/editor.frame_model_manager.inl"
#include "application/editor.frame_packages_about.inl"
#include "application/editor.frame_finish.inl"
    }

} // namespace epochenginenamespace
