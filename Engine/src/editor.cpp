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
/**************************************************************
 *   epochengine - Modular C++ Framework
 *   Editor Implementation
 *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
 **************************************************************/
module;

#include <include/epoch.script_api.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iomanip>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "epoch/core/cpp_feature_probe.hpp"

module editor;

import engine.gui;
import engine.visuals;
import engine.version;
import spritehandle;
import core.context;
import core.path;
import core.time;
import context.commandqueue;
import context.type;
import engine.input;
import engine.cli;
import scripting.system;
import epoch.ai;
import epoch.systems;
import voxel.field;
import forest.factory;
import package.registry;
import perf.tier;
import render.device;
import render.preview_grid;
import saveload.system;
import timeline.system;
import updater.config;
import updater.system;

namespace epochnamespace
{
    namespace
    {
        using namespace std::chrono_literals;

        enum class TopMenu : unsigned char
        {
            None = 0,
            File,
            Edit,
            Asset,
            Window,
            Tools,
            Help
        };

        enum class EditorAutomationCommand : unsigned char
        {
            None = 0,
            SmartUpdate,
            SourceUpdate
        };

        enum class EditorUpdateState : unsigned char
        {
            Idle = 0,
            Checking,
            Available,
            RestartReady,
            SourceWorkerRunning,
            Canceled,
            Failed
        };

        enum class AiWorkspaceDomain : unsigned char
        {
            Control = 0,
            Tooling,
            Engine,
            Software,
            Training,
            Visualizer,
            Ops
        };

        enum class EditorLayoutDrag : unsigned char
        {
            None = 0,
            Outliner,
            Inspector,
            Dock,
            Workspace
        };

        enum class EditorMainSurface : unsigned char
        {
            Scene = 0,
            Game2D,
            Assets,
            Project,
            ForestFactory,
            Timeline,
            AISandbox,
            Systems
        };

        struct SystemsSurfaceState
        {
            float renderZoom{ 1.15f };
            float taskZoom{ 1.15f };
            int renderPan{ 0 };
            int taskPan{ 0 };
            int graphInputCooldownFrames{ 0 };
            SpriteHandle renderSurface{};
            SpriteHandle taskSurface{};
            SpriteHandle supportSurface{};
            SpriteHandle aiLoopSurface{};
        };

        [[nodiscard]] static bool is_ws_only(std::string_view s) noexcept
        {
            for (unsigned char c : s)
                if (c > ' ') return false;
            return true;
        }

        [[nodiscard]] static std::string normalize_editor_text_for_gui(std::string_view text)
        {
            std::string out{};
            out.reserve(text.size());

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const auto ch = static_cast<unsigned char>(text[i]);
                if (ch == '\r')
                    continue;
                if (ch == '\n' || ch == '\t')
                {
                    out.push_back(static_cast<char>(ch));
                    continue;
                }
                if (ch >= 32u && ch < 127u)
                {
                    out.push_back(static_cast<char>(ch));
                    continue;
                }

                auto has = [&](std::initializer_list<unsigned char> bytes) noexcept {
                    if (i + bytes.size() > text.size())
                        return false;
                    std::size_t offset = 0;
                    for (const unsigned char expected : bytes)
                    {
                        if (static_cast<unsigned char>(text[i + offset]) != expected)
                            return false;
                        ++offset;
                    }
                    return true;
                };

                if (has({ 0xC2u, 0xA0u }))
                {
                    out.push_back(' ');
                    i += 1;
                }
                else if (has({ 0xE2u, 0x80u, 0x93u }) || has({ 0xE2u, 0x80u, 0x94u }) || has({ 0xE2u, 0x88u, 0x92u }))
                {
                    out.push_back('-');
                    i += 2;
                }
                else if (has({ 0xE2u, 0x80u, 0x98u }) || has({ 0xE2u, 0x80u, 0x99u }))
                {
                    out.push_back('\'');
                    i += 2;
                }
                else if (has({ 0xE2u, 0x80u, 0x9Cu }) || has({ 0xE2u, 0x80u, 0x9Du }))
                {
                    out.push_back('"');
                    i += 2;
                }
                else if (has({ 0xE2u, 0x80u, 0xA6u }))
                {
                    out += "...";
                    i += 2;
                }
                else if (has({ 0xE2u, 0x86u, 0x92u }))
                {
                    out += "->";
                    i += 2;
                }
                else
                {
                    out.push_back(' ');
                    while (i + 1 < text.size() && (static_cast<unsigned char>(text[i + 1]) & 0xC0u) == 0x80u)
                        ++i;
                }
            }

            return out;
        }

        struct AiChat
        {
            static constexpr std::size_t kMaxLines = 200;
            std::vector<std::string> lines{};
            std::string input{};
            std::string pendingPrompt{};
            std::optional<std::future<std::string>> pending{};

            AiChat()
            {
                lines.emplace_back("ai> Ready. Select a local model in OS AI or Window > AI Control.");
                trim_lines();
            }

            AiChat(const AiChat&) = delete;
            AiChat& operator=(const AiChat&) = delete;
            AiChat(AiChat&&) noexcept = default;
            AiChat& operator=(AiChat&&) noexcept = default;

            void pump()
            {
                if (!pending) return;
                if (pending->wait_for(0ms) != std::future_status::ready) return;

                try
                {
                    std::string reply = normalize_editor_text_for_gui(pending->get());
                    if (reply.empty()) reply = "(empty reply)";
                    lines.emplace_back("ai> " + reply);
                    if (!pendingPrompt.empty() && reply != "(empty reply)")
                        epoch::ai::append_training_sample(pendingPrompt, reply, "editor_ai_chat");
                    pendingPrompt.clear();
                    trim_lines();
                }
                catch (const std::exception& e)
                {
                    lines.emplace_back(std::string("ai> (error) ") + e.what());
                    pendingPrompt.clear();
                    trim_lines();
                }

                pending.reset();
            }

            void submit(std::string text)
            {
                text = normalize_editor_text_for_gui(text);
                if (text.empty() || is_ws_only(text)) return;

                if (pending)
                {
                    lines.emplace_back("ai> (busy)");
                    trim_lines();
                    return;
                }

                lines.emplace_back("you> " + text);
                trim_lines();
                pendingPrompt = text;

                pending.emplace(std::async(std::launch::async, [t = std::move(text)]() mutable {
                    epoch::systems::threading::ScopedThreadActivity threadActivity{};
                    return epoch::ai::send_to_engine_ai(t);
                }));
            }

        private:
            void trim_lines()
            {
                if (lines.size() > kMaxLines)
                {
                    lines.erase(
                        lines.begin(),
                        lines.begin() + static_cast<std::ptrdiff_t>(lines.size() - kMaxLines));
                }
            }
        };

        [[nodiscard]] static std::string last_chat_line_with_prefix(const AiChat& chat, std::string_view prefix)
        {
            for (auto it = chat.lines.rbegin(); it != chat.lines.rend(); ++it)
            {
                if (it->rfind(prefix, 0) == 0)
                    return it->substr(prefix.size());
            }
            return {};
        }

        struct EditorEntity
        {
            std::string name{};
            std::string type{};
            std::string category{};
            std::array<float, 3> position{ 0.0f, 0.0f, 0.0f };
            std::array<float, 3> rotation{ 0.0f, 0.0f, 0.0f };
            std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
            bool visible{ true };
            bool editorOnly{ false };
        };

        [[nodiscard]] EditorWorkspaceTab initial_editor_workspace_tab() noexcept
        {
            const auto parse_workspace_tab = [](std::string_view value) noexcept
            {
                if (value == "AI" || value == "ai" || value == "Ai")
                    return EditorWorkspaceTab::AI;
                if (value == "Systems" || value == "systems")
                    return EditorWorkspaceTab::Systems;
                if (value == "Scripts" || value == "scripts")
                    return EditorWorkspaceTab::Assets;
                if (value == "Assets" || value == "assets")
                    return EditorWorkspaceTab::Assets;
                if (value == "Output" || value == "output")
                    return EditorWorkspaceTab::Output;
                return EditorWorkspaceTab::Output;
            };

#if defined(_MSC_VER)
            char* rawValue = nullptr;
            std::size_t rawSize = 0;
            if (_dupenv_s(&rawValue, &rawSize, "EPOCH_EDITOR_START_WORKSPACE") != 0 || !rawValue)
                return EditorWorkspaceTab::Output;

            const EditorWorkspaceTab tab = parse_workspace_tab(std::string_view{ rawValue });
            std::free(rawValue);
            return tab;
#else
            const char* const rawValue = std::getenv("EPOCH_EDITOR_START_WORKSPACE");
            if (!rawValue)
                return EditorWorkspaceTab::Output;

            return parse_workspace_tab(std::string_view{ rawValue });
#endif
        }

        [[nodiscard]] EditorMainSurface initial_editor_main_surface(EditorWorkspaceTab workspace) noexcept
        {
            switch (workspace)
            {
            case EditorWorkspaceTab::AI:
                return EditorMainSurface::AISandbox;
            case EditorWorkspaceTab::Systems:
                return EditorMainSurface::Systems;
            case EditorWorkspaceTab::Assets:
            case EditorWorkspaceTab::Scripts:
                return EditorMainSurface::Assets;
            case EditorWorkspaceTab::Project:
                return EditorMainSurface::Project;
            case EditorWorkspaceTab::Output:
            default:
                return EditorMainSurface::Scene;
            }
        }

        struct EditorState
        {
            bool initialized{ false };
            TopMenu openMenu{ TopMenu::None };
            std::string projectId{};
            std::string projectName{};
            std::string projectRoot{};
            std::string projectScenePath{};
            std::string projectManifest{};
            std::string projectTemplate{};
            std::string projectKind{};
            std::string activeScript{};
            std::string activeRuntimeScene{};
            std::string activeWorld{};
            std::string projectStatus{};
            std::string projectBuildStatus{};
            std::string scriptBuildStatus{};
            std::string newScriptName{ "sandbox_iteration" };
            std::string scriptEditorPath{};
            std::string scriptEditorText{};
            std::string scriptEditorStatus{ "No script source loaded." };
            bool scriptEditorDirty{ false };
            std::optional<std::future<EditorProjectBuildResult>> projectBuildPending{};
            bool projectBuildRunAfterBuild{ false };
            std::string projectBuildRunScene{};
            std::string projectBuildOutputPath{};
            std::string projectBuildRunBackend{};
            double projectBuildRunFrameLimitFps{ 60.0 };
            previewgrid::CameraMode projectBuildRunCameraMode{ previewgrid::CameraMode::Editor };
            input::ProfilePreset projectBuildRunInputProfile{ input::ProfilePreset::EditorDefault };
            std::string projectRunBackend{ "opengl" };
            double projectRunFrameLimitFps{ 60.0 };
            previewgrid::CameraMode projectCameraMode{ previewgrid::CameraMode::Editor };
            input::ProfilePreset inputProfilePreset{ input::ProfilePreset::EditorDefault };
            gui::ThemePreference themePreference{ gui::ThemePreference::FollowSystemDark };
            double editorFrameLimitFps{ 120.0 };
            std::string selectedProjectFile{};
            std::string selectedAssetPath{};
            std::vector<EditorEntity> entities{};
            std::size_t selectedEntity{ 0 };
            std::vector<std::string> logLines{};
            bool helpersVisible{ true };
            bool sceneDragActive{ false };
            bool sceneLeftWasHeld{ false };
            std::size_t sceneDragEntity{ 0 };
            gui::Vec2 sceneDragStartMouse{};
            std::array<float, 3> sceneDragStartPosition{ 0.0f, 0.0f, 0.0f };
            std::array<float, 3> sceneDragStartHit{ 0.0f, 0.0f, 0.0f };
            bool sceneDragHasPlaneHit{ false };
            EditorTimeSnapshot timeSnapshot{};
            EditorTimeControl timeControl{};
            epoch::saveload::StreamingSaveConfig streamingSaveConfig{};
            epoch::saveload::StreamingSaveStatus streamingSaveStatus{};
            epoch::saveload::StreamingCheckpointRecord lastCheckpointRecord{};
            epoch::timeline::TimelineState timelineState{};
            std::vector<epoch::timeline::TimelineTrack> timelineTracks{};
            std::vector<epoch::timeline::TimelineEvent> timelineEvents{};
            core::ScenePreviewMode previewMode{ core::ScenePreviewMode::Editor };
            EditorWorkspaceTab workspaceTab{ initial_editor_workspace_tab() };
            EditorWorkspaceTab dockStatusTab{ EditorWorkspaceTab::Output };
            EditorMainSurface mainSurface{ initial_editor_main_surface(workspaceTab) };
            float workspaceSplit{ 0.68f };
            float outlinerSplit{ 0.20f };
            float inspectorSplit{ 0.22f };
            float dockSplit{ 0.24f };
            bool showOutliner{ true };
            bool showInspector{ true };
            bool showConsoleDock{ true };
            bool showAiChat{ true };
            EditorLayoutDrag layoutDrag{ EditorLayoutDrag::None };
            int surfaceSettleFrames{ 0 };
            gui::Vec2 lastLayoutExtent{};
            std::string detachedPanelHostStatus{ "Docked panels active. Borderless popout routing is disabled while the editor layout is stabilized." };
            bool projectNotesVisible{ false };
            bool showAboutModal{ false };
            bool showSettingsModal{ false };
            bool showPackageManagerModal{ false };
            std::string selectedPackageId{ "engine_arcade" };
            std::string packageInstallStatus{ "Select a package and press Install." };
            float packageInstallProgress{ 0.0f };
            EditorUpdateState updateState{ EditorUpdateState::Idle };
            std::string updateStatus{ "Updates have not been checked." };
            std::optional<std::future<updater::UpdateCommandResult>> updateCheckPending{};
            updater::UpdateCommandResult lastUpdateCheck{};
            bool autoUpdateCheckQueued{ true };
            bool showUpdateConfirmModal{ false };
            bool showSourceUpdateConfirmModal{ false };
            bool updateInstallPending{ false };
            bool updateSourceInstallPending{ false };
            std::chrono::steady_clock::time_point updateOperationStartedAt{};
            std::chrono::steady_clock::time_point updateRestartReadyAt{};
            EditorAutomationCommand automationCommand{ EditorAutomationCommand::None };
            bool automationConsumed{ false };
            SystemsSurfaceState systems{};
            AiWorkspaceDomain aiWorkspaceDomain{ AiWorkspaceDomain::Control };
            bool aiContinuousBuildEnabled{ false };
            bool aiContinuousBuildStageOnNextFrame{ false };
            std::optional<std::future<EditorProjectBuildResult>> aiContinuousBuildPending{};
            std::string aiContinuousBuildFingerprint{};
            std::string aiContinuousBuildStatus{ "Manual OS AI evidence gate is idle." };
            std::size_t aiContinuousBuildRunCount{ 0 };
            std::string aiToolHarnessStatus{ "AI tool harness has not run yet." };
            std::size_t aiToolHarnessRunCount{ 0 };
        };

        [[nodiscard]] bool load_editor_scene_snapshot(EditorState& state);
        void save_editor_scene_snapshot(const EditorState& state);

        struct ContextPtrHash
        {
            std::size_t operator()(const core::Context* p) const noexcept
            {
                return std::hash<const void*>{}(p);
            }
        };

        [[nodiscard]] static bool editor_point_in_rect(gui::Vec2 point, gui::Vec2 pos, gui::Vec2 size) noexcept
        {
            return point.x >= pos.x
                && point.y >= pos.y
                && point.x <= pos.x + size.x
                && point.y <= pos.y + size.y;
        }

        [[nodiscard]] static bool main_surface_uses_scene(EditorMainSurface surface) noexcept
        {
            return surface == EditorMainSurface::Scene
                || surface == EditorMainSurface::Game2D
                || surface == EditorMainSurface::ForestFactory
                || surface == EditorMainSurface::Timeline;
        }

        [[nodiscard]] static std::string_view main_surface_title(EditorMainSurface surface) noexcept
        {
            switch (surface)
            {
            case EditorMainSurface::Scene:
                return "3D Scene";
            case EditorMainSurface::Game2D:
                return "2D Scene/UI";
            case EditorMainSurface::Assets:
                return "Assets";
            case EditorMainSurface::Project:
                return "Project";
            case EditorMainSurface::ForestFactory:
                return "Plant Lab";
            case EditorMainSurface::Timeline:
                return "Video";
            case EditorMainSurface::AISandbox:
                return "Intelligence";
            case EditorMainSurface::Systems:
                return "System Info";
            default:
                return "Workspace";
            }
        }

        static void reset_editor_layout(EditorState& editor) noexcept
        {
            editor.outlinerSplit = 0.20f;
            editor.inspectorSplit = 0.22f;
            editor.dockSplit = 0.24f;
            editor.workspaceSplit = 0.68f;
            editor.showOutliner = true;
            editor.showInspector = true;
            editor.showConsoleDock = true;
            editor.showAiChat = true;
            editor.layoutDrag = EditorLayoutDrag::None;
            editor.surfaceSettleFrames = 0;
            editor.lastLayoutExtent = {};
            editor.detachedPanelHostStatus = "Layout reset. Docked panels active; borderless popout routing remains disabled.";
        }

        struct ContextPtrEq
        {
            bool operator()(const core::Context* a, const core::Context* b) const noexcept
            {
                return a == b;
            }
        };

        struct ChatStorage
        {
            std::mutex mutex{};
            bool bot_initialized{ false };
            std::unordered_map<const core::Context*, AiChat, ContextPtrHash, ContextPtrEq> chats{};
        };

        struct EditorStorage
        {
            std::mutex mutex{};
            std::unordered_map<const core::Context*, EditorState, ContextPtrHash, ContextPtrEq> states{};
        };

        ChatStorage& chat_storage()
        {
            static ChatStorage storage;
            return storage;
        }

        EditorStorage& editor_storage()
        {
            static EditorStorage storage;
            return storage;
        }

        void push_editor_log(EditorState& state, std::string line)
        {
            state.logLines.push_back(std::move(line));
            constexpr std::size_t kMaxLogLines = 10;
            if (state.logLines.size() > kMaxLogLines)
                state.logLines.erase(state.logLines.begin(), state.logLines.begin() + static_cast<std::ptrdiff_t>(state.logLines.size() - kMaxLogLines));
        }

        [[nodiscard]] updater::UpdateChannel editor_update_channel()
        {
            updater::UpdateChannel channel{};
            channel.version_url = updater::PROJECT_PACKAGED_VERSION_URL();
            channel.binary_url = updater::PROJECT_BINARY_URL();
            channel.source_url = updater::PROJECT_SOURCE_URL();
            channel.source_version_url = updater::PROJECT_SOURCE_VERSION_URL();
            channel.platform_build_status_url = updater::PROJECT_ACTION_RUNS_API_URL();
            channel.platform_build_job_name = updater::PROJECT_UPDATE_BUILD_JOB_NAME();
            return channel;
        }

        [[nodiscard]] std::string describe_update_result(const updater::UpdateCommandResult& result)
        {
            if (!result.status_message.empty())
                return result.status_message;

            if (!result.platform_build_ok
                && (!result.platform_build_reason.empty() || result.platform_build_checked))
            {
                std::string message = "Update withheld";
                if (!result.platform_build_job.empty())
                    message += " for " + result.platform_build_job;
                if (!result.platform_build_reason.empty())
                    message += ": " + result.platform_build_reason;
                else
                    message += ": platform build status is not proven.";
                return message;
            }

            if (result.packaged_update_available)
            {
                if (!result.remote_version.empty())
                    return std::string{ "Packaged update available: " } + result.remote_version;
                return "Packaged update available.";
            }

            if (result.source_update_available)
            {
                if (result.packaged_release_missing && !result.packaged_release_reason.empty())
                    return result.packaged_release_reason + " Source update is available.";
                if (!result.source_remote_version.empty())
                    return std::string{ "Source update available: " } + result.source_remote_version;
                return "Source update available.";
            }

            if (!result.packaged_release_reason.empty())
                return result.packaged_release_reason;

            return "Epoch is already current.";
        }

        [[nodiscard]] std::string update_toolbar_button_label(EditorUpdateState state)
        {
            switch (state)
            {
            case EditorUpdateState::Available:
                return "Update Available";
            case EditorUpdateState::RestartReady:
                return "Restart";
            case EditorUpdateState::Checking:
                return "Checking...";
            case EditorUpdateState::SourceWorkerRunning:
                return "Source Update Running";
            case EditorUpdateState::Canceled:
                return "Update Available";
            case EditorUpdateState::Failed:
                return "Update Check Failed";
            case EditorUpdateState::Idle:
            default:
                return "Update";
            }
        }

        [[nodiscard]] double editor_update_elapsed_seconds(const EditorState& editor)
        {
            if (editor.updateOperationStartedAt == std::chrono::steady_clock::time_point{})
                return 0.0;

            return std::chrono::duration<double>(
                std::chrono::steady_clock::now() - editor.updateOperationStartedAt).count();
        }

        constexpr auto kEditorUpdateRestartDelay = std::chrono::seconds{ 10 };

        void clear_editor_update_restart_countdown(EditorState& editor) noexcept
        {
            editor.updateRestartReadyAt = {};
        }

        void arm_editor_update_restart_countdown(EditorState& editor)
        {
            if (editor.updateRestartReadyAt == std::chrono::steady_clock::time_point{})
                editor.updateRestartReadyAt = std::chrono::steady_clock::now();
        }

        [[nodiscard]] int editor_update_restart_countdown_seconds(const EditorState& editor)
        {
            if (editor.updateRestartReadyAt == std::chrono::steady_clock::time_point{})
                return static_cast<int>(kEditorUpdateRestartDelay.count());

            const auto elapsed = std::chrono::steady_clock::now() - editor.updateRestartReadyAt;
            if (elapsed >= kEditorUpdateRestartDelay)
                return 0;

            const auto remaining = kEditorUpdateRestartDelay - elapsed;
            const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
            return static_cast<int>((remainingMs + 999) / 1000);
        }

        [[nodiscard]] bool editor_update_restart_countdown_elapsed(const EditorState& editor)
        {
            return editor.updateRestartReadyAt != std::chrono::steady_clock::time_point{}
                && std::chrono::steady_clock::now() - editor.updateRestartReadyAt >= kEditorUpdateRestartDelay;
        }

        [[nodiscard]] float editor_update_progress_value(const EditorState& editor)
        {
            if (editor.updateState == EditorUpdateState::SourceWorkerRunning)
            {
                const double elapsed = editor_update_elapsed_seconds(editor);
                return static_cast<float>(std::clamp(0.14 + elapsed * 0.004, 0.14, 0.98));
            }

            if (!editor.updateCheckPending)
            {
                return editor.updateState == EditorUpdateState::RestartReady ? 1.0f : 0.0f;
            }

            const double elapsed = editor_update_elapsed_seconds(editor);
            if (!editor.updateInstallPending)
                return static_cast<float>(std::clamp(0.10 + elapsed * 0.08, 0.10, 0.88));

            if (editor.updateSourceInstallPending)
                return static_cast<float>(std::clamp(0.12 + elapsed * 0.012, 0.12, 0.94));

            return static_cast<float>(std::clamp(0.12 + elapsed * 0.025, 0.12, 0.94));
        }

        [[nodiscard]] std::string editor_update_running_status(const EditorState& editor)
        {
            const double elapsed = editor_update_elapsed_seconds(editor);
            if (editor.updateSourceInstallPending)
                return std::format("Source rebuild is running ({:.0f}s). Reading epoch_source_update.log and epoch_update_handoff.log for live evidence; Cancel asks the worker to stop at the next safe checkpoint.", elapsed);

            if (editor.updateInstallPending)
                return std::format("Installing update ({:.0f}s). Checking platform release, replacing stale cache, and staging handoff.", elapsed);

            return std::format("Checking update availability ({:.0f}s). Epoch checks this platform's packaged release first.", elapsed);
        }

        [[nodiscard]] std::string read_update_log_tail(const std::filesystem::path& path, const std::uintmax_t maxBytes = 2400)
        {
            std::error_code sizeEc;
            const auto size = std::filesystem::file_size(path, sizeEc);
            if (sizeEc || size == 0)
                return {};

            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return {};

            const std::uintmax_t readSize = (std::min)(size, maxBytes);
            if (size > readSize)
                stream.seekg(static_cast<std::streamoff>(size - readSize), std::ios::beg);

            std::string text;
            text.resize(static_cast<std::size_t>(readSize));
            stream.read(text.data(), static_cast<std::streamsize>(text.size()));
            text.resize(static_cast<std::size_t>((std::max)(std::streamsize{ 0 }, stream.gcount())));
            return text;
        }

        [[nodiscard]] std::string last_nonempty_update_log_line(std::string_view text)
        {
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.remove_suffix(1);
            if (text.empty())
                return {};

            const auto pos = text.find_last_of("\r\n");
            const std::string_view line = pos == std::string_view::npos ? text : text.substr(pos + 1);
            return std::string{ line };
        }

        [[nodiscard]] bool update_log_contains(std::string_view text, std::string_view needle) noexcept
        {
            return text.find(needle) != std::string_view::npos;
        }

        [[nodiscard]] std::atomic<bool>& editor_startup_update_check_claimed() noexcept
        {
            static std::atomic<bool> claimed{ false };
            return claimed;
        }

        [[nodiscard]] bool try_claim_editor_startup_update_check() noexcept
        {
            bool expected = false;
            return editor_startup_update_check_claimed().compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel);
        }

        [[nodiscard]] std::atomic<bool>& editor_update_operation_running() noexcept
        {
            static std::atomic<bool> running{ false };
            return running;
        }

        [[nodiscard]] bool try_claim_editor_update_operation() noexcept
        {
            bool expected = false;
            return editor_update_operation_running().compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel);
        }

        void release_editor_update_operation() noexcept
        {
            editor_update_operation_running().store(false, std::memory_order_release);
        }

        struct ScopedEditorUpdateOperation final
        {
            ~ScopedEditorUpdateOperation()
            {
                release_editor_update_operation();
            }
        };

        void start_editor_update_check(EditorState& editor)
        {
            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Update check is already running.");
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Checking for updates...";
            editor.updateInstallPending = false;
            editor.updateSourceInstallPending = false;
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Checking for available Epoch updates.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epoch::systems::threading::ScopedThreadActivity threadActivity{};
                    return updater::run_update_command(editor_update_channel(), false);
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void start_editor_update_install(EditorState& editor)
        {
            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Update worker is already running.");
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Installing the best available update. Epoch checks packaged releases first, then falls back to source only when no newer package exists.";
            editor.updateInstallPending = true;
            editor.updateSourceInstallPending = false;
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Installing through the binary-first update gate.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epoch::systems::threading::ScopedThreadActivity threadActivity{};
                    return updater::run_update_command(editor_update_channel(), true);
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void start_editor_source_update_install(EditorState& editor)
        {
            if (editor.updateCheckPending.has_value())
            {
                push_editor_log(editor, "[update] Source update worker is already running.");
                return;
            }

            if (!try_claim_editor_update_operation())
            {
                editor.updateStatus = "Another editor pane is already checking or installing updates.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            editor.updateState = EditorUpdateState::Checking;
            editor.updateStatus = "Launching the advanced source rebuild worker. Use this only when you intentionally want latest main source instead of the packaged platform release.";
            editor.updateInstallPending = true;
            editor.updateSourceInstallPending = true;
            clear_editor_update_restart_countdown(editor);
            editor.updateOperationStartedAt = std::chrono::steady_clock::now();
            push_editor_log(editor, "[update] Launching advanced source rebuild worker.");

            try
            {
                editor.updateCheckPending.emplace(std::async(std::launch::async, [] {
                    ScopedEditorUpdateOperation updateOperation{};
                    epoch::systems::threading::ScopedThreadActivity threadActivity{};
                    updater::UpdateCommandResult result{};
                    result.update_available = true;
                    result.source_update_available = true;
                    result.source_fallback_attempted = true;
                    const bool workerLaunched = updater::run_source_update_command(editor_update_channel(), false);
                    result.update_performed = false;
                    result.source_update_performed = workerLaunched;
                    result.status_message = workerLaunched
                        ? "Source rebuild worker started. Epoch will restart automatically only after build and handoff evidence succeeds; watch epoch_source_update.log beside the executable."
                        : "Source update failed to start. Check epoch_source_update.log and epoch_update_handoff.log beside the executable.";
                    return result;
                }));
            }
            catch (...)
            {
                release_editor_update_operation();
                throw;
            }
        }

        void pump_editor_update_check(EditorState& editor)
        {
            if (!editor.updateCheckPending.has_value())
                return;

            if (editor.updateCheckPending->wait_for(0s) != std::future_status::ready)
            {
                editor.updateStatus = editor_update_running_status(editor);
                return;
            }

            const bool installPending = editor.updateInstallPending;
            const bool sourceInstallPending = editor.updateSourceInstallPending;

            try
            {
                editor.lastUpdateCheck = editor.updateCheckPending->get();
                editor.updateCheckPending.reset();
                const bool sourceWorkerRunning =
                    installPending
                    && editor.lastUpdateCheck.source_fallback_attempted
                    && editor.lastUpdateCheck.source_update_performed
                    && !editor.lastUpdateCheck.packaged_update_performed;
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                if (!sourceWorkerRunning)
                    editor.updateOperationStartedAt = {};

                if (installPending)
                {
                    if (sourceWorkerRunning)
                    {
                        editor.updateState = EditorUpdateState::SourceWorkerRunning;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        editor.showUpdateConfirmModal = true;
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (sourceInstallPending
                        && editor.lastUpdateCheck.source_fallback_attempted
                        && !editor.lastUpdateCheck.source_update_performed)
                    {
                        editor.updateState = EditorUpdateState::Failed;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (editor.lastUpdateCheck.platform_build_checked
                        && !editor.lastUpdateCheck.platform_build_ok)
                    {
                        editor.updateState = EditorUpdateState::Failed;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    if (editor.lastUpdateCheck.update_performed)
                    {
                        editor.updateState = EditorUpdateState::RestartReady;
                        editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                        arm_editor_update_restart_countdown(editor);
                        push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                        return;
                    }

                    editor.updateState = EditorUpdateState::Failed;
                    editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                if (editor.lastUpdateCheck.update_available || editor.lastUpdateCheck.force_required)
                {
                    editor.updateState = EditorUpdateState::Available;
                    editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                    return;
                }

                editor.updateState = EditorUpdateState::Idle;
                editor.updateStatus = describe_update_result(editor.lastUpdateCheck);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
            catch (...)
            {
                editor.updateCheckPending.reset();
                editor.updateInstallPending = false;
                editor.updateSourceInstallPending = false;
                editor.updateOperationStartedAt = {};
                editor.updateState = EditorUpdateState::Failed;
                editor.updateStatus = "Update check failed; see updater logs for details.";
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
            }
        }

        void pump_editor_source_update_worker(EditorState& editor)
        {
            if (editor.updateState != EditorUpdateState::SourceWorkerRunning)
                return;

            const std::string handoffTail = read_update_log_tail(updater::update_handoff_log_path());
            const std::string sourceTail = read_update_log_tail(updater::source_update_log_path());
            const std::string_view evidence = !handoffTail.empty() ? std::string_view{ handoffTail } : std::string_view{ sourceTail };

            if (update_log_contains(handoffTail, "Source update cancel")
                || update_log_contains(sourceTail, "Source update cancel"))
            {
                editor.updateState = EditorUpdateState::Available;
                editor.updateStatus = "Source update canceled. Update remains available if you want to retry.";
                editor.showUpdateConfirmModal = false;
                editor.updateOperationStartedAt = {};
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (update_log_contains(handoffTail, "[ERROR]") || update_log_contains(sourceTail, "[ERROR]"))
            {
                const std::string lastLine = last_nonempty_update_log_line(!handoffTail.empty() ? handoffTail : sourceTail);
                editor.updateState = EditorUpdateState::Failed;
                editor.updateStatus = lastLine.empty()
                    ? "Source update failed. Check epoch_source_update.log and epoch_update_handoff.log beside the executable."
                    : std::string{ "Source update failed: " } + lastLine;
                editor.updateOperationStartedAt = {};
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (update_log_contains(handoffTail, "Waiting for runtime handoff")
                || update_log_contains(sourceTail, "Built runtime ready at"))
            {
                editor.updateState = EditorUpdateState::RestartReady;
                editor.updateStatus = "Source rebuild is ready for runtime handoff. Press Restart to close Epoch and let the worker replace the executable.";
                editor.updateOperationStartedAt = {};
                editor.showUpdateConfirmModal = true;
                arm_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            if (update_log_contains(handoffTail, "Restarted updated runtime")
                || update_log_contains(handoffTail, "Source runtime files copied successfully"))
            {
                editor.updateState = EditorUpdateState::RestartReady;
                editor.updateStatus = "Source update handoff completed. Restart Epoch if this window did not close automatically.";
                editor.updateOperationStartedAt = {};
                editor.showUpdateConfirmModal = true;
                arm_editor_update_restart_countdown(editor);
                push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                return;
            }

            const std::string lastLine = last_nonempty_update_log_line(evidence);
            if (!lastLine.empty())
                editor.updateStatus = std::string{ "Source rebuild running. Latest evidence: " } + lastLine;
        }

        [[nodiscard]] static std::string format_ms(double seconds)
        {
            return std::format("{:.2f} ms", seconds * 1000.0);
        }

        [[nodiscard]] static std::string format_seconds(double seconds)
        {
            return std::format("{:.2f} s", seconds);
        }

        [[nodiscard]] static std::string format_rate(double dt_seconds)
        {
            if (dt_seconds <= 0.0)
                return "0 Hz";
            return std::format("{:.0f} Hz", 1.0 / dt_seconds);
        }

        [[nodiscard]] static epoch::core::time::simulation_stats timeline_stats_from_editor(
            const EditorState& editor) noexcept
        {
            return epoch::core::time::simulation_stats{
                .frame_index = editor.timeSnapshot.frame_index,
                .simulated_steps = editor.timeSnapshot.simulated_steps,
                .step_budget = editor.timeSnapshot.step_budget,
                .real_dt_seconds = editor.timeSnapshot.real_dt_seconds,
                .scaled_dt_seconds = editor.timeSnapshot.scaled_dt_seconds,
                .fixed_dt_seconds = editor.timeSnapshot.fixed_dt_seconds,
                .accumulator_seconds = editor.timeSnapshot.accumulator_seconds,
                .simulated_seconds = editor.timeSnapshot.simulated_seconds,
                .time_scale = editor.timeSnapshot.time_scale,
                .paused = editor.timeSnapshot.paused,
                .max_steps_per_frame = editor.timeSnapshot.max_steps_per_frame
            };
        }

        static void ensure_timeline_defaults(EditorState& editor)
        {
            if (editor.timelineTracks.empty())
                editor.timelineTracks = epoch::timeline::default_editor_tracks();

            if (editor.timelineEvents.empty())
            {
                editor.timelineEvents.push_back(epoch::timeline::TimelineEvent{
                    .track_id = "save",
                    .kind = epoch::timeline::TimelineEventKind::Checkpoint,
                    .simulated_seconds = 0.0,
                    .frame_index = 0,
                    .label = "Initial checkpoint gate",
                    .target_name = "PersistentLevel",
                    .payload = "streaming-save start"
                });
                epoch::timeline::sort_events(editor.timelineEvents);
            }
        }

        struct SurfaceCanvas
        {
            int width = 0;
            int height = 0;
            std::vector<std::uint8_t> pixels{};

            SurfaceCanvas(int w, int h, gui::Color clear)
                : width(w)
                , height(h)
                , pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u, 0)
            {
                fill_rect(0, 0, width, height, clear);
            }

            void set_pixel(int x, int y, gui::Color color) noexcept
            {
                if (x < 0 || y < 0 || x >= width || y >= height)
                    return;

                const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(x)) * 4u;
                pixels[idx + 0] = color.r;
                pixels[idx + 1] = color.g;
                pixels[idx + 2] = color.b;
                pixels[idx + 3] = color.a;
            }

            void fill_rect(int x, int y, int w, int h, gui::Color color) noexcept
            {
                const int x0 = (std::max)(0, x);
                const int y0 = (std::max)(0, y);
                const int x1 = (std::min)(width, x + w);
                const int y1 = (std::min)(height, y + h);

                for (int py = y0; py < y1; ++py)
                    for (int px = x0; px < x1; ++px)
                        set_pixel(px, py, color);
            }

            void stroke_rect(int x, int y, int w, int h, gui::Color color) noexcept
            {
                fill_rect(x, y, w, 1, color);
                fill_rect(x, y + h - 1, w, 1, color);
                fill_rect(x, y, 1, h, color);
                fill_rect(x + w - 1, y, 1, h, color);
            }

            void hline(int x, int y, int w, gui::Color color, int thickness = 1) noexcept
            {
                fill_rect(x, y, w, thickness, color);
            }

            void vline(int x, int y, int h, gui::Color color, int thickness = 1) noexcept
            {
                fill_rect(x, y, thickness, h, color);
            }
        };

        [[nodiscard]] static std::array<std::string_view, 7> tiny_glyph(char ch) noexcept
        {
            switch (static_cast<char>(std::toupper(static_cast<unsigned char>(ch))))
            {
            case '0': return { " ### ", "#   #", "#  ##", "# # #", "##  #", "#   #", " ### " };
            case '1': return { "  #  ", " ##  ", "# #  ", "  #  ", "  #  ", "  #  ", "#####" };
            case '2': return { " ### ", "#   #", "    #", "   # ", "  #  ", " #   ", "#####" };
            case '3': return { "#### ", "    #", "    #", " ### ", "    #", "    #", "#### " };
            case '4': return { "#   #", "#   #", "#   #", "#####", "    #", "    #", "    #" };
            case '5': return { "#####", "#    ", "#    ", "#### ", "    #", "#   #", " ### " };
            case '6': return { " ### ", "#    ", "#    ", "#### ", "#   #", "#   #", " ### " };
            case '7': return { "#####", "    #", "   # ", "  #  ", " #   ", " #   ", " #   " };
            case '8': return { " ### ", "#   #", "#   #", " ### ", "#   #", "#   #", " ### " };
            case '9': return { " ### ", "#   #", "#   #", " ####", "    #", "    #", " ### " };
            case 'A': return { " ### ", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" };
            case 'B': return { "#### ", "#   #", "#   #", "#### ", "#   #", "#   #", "#### " };
            case 'C': return { " ### ", "#   #", "#    ", "#    ", "#    ", "#   #", " ### " };
            case 'D': return { "#### ", "#   #", "#   #", "#   #", "#   #", "#   #", "#### " };
            case 'E': return { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#####" };
            case 'F': return { "#####", "#    ", "#    ", "#### ", "#    ", "#    ", "#    " };
            case 'G': return { " ### ", "#   #", "#    ", "#  ##", "#   #", "#   #", " ### " };
            case 'H': return { "#   #", "#   #", "#   #", "#####", "#   #", "#   #", "#   #" };
            case 'I': return { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "#####" };
            case 'J': return { "#####", "    #", "    #", "    #", "#   #", "#   #", " ### " };
            case 'K': return { "#   #", "#  # ", "# #  ", "##   ", "# #  ", "#  # ", "#   #" };
            case 'L': return { "#    ", "#    ", "#    ", "#    ", "#    ", "#    ", "#####" };
            case 'M': return { "#   #", "## ##", "# # #", "#   #", "#   #", "#   #", "#   #" };
            case 'N': return { "#   #", "##  #", "# # #", "#  ##", "#   #", "#   #", "#   #" };
            case 'O': return { " ### ", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " };
            case 'P': return { "#### ", "#   #", "#   #", "#### ", "#    ", "#    ", "#    " };
            case 'Q': return { " ### ", "#   #", "#   #", "#   #", "# # #", "#  # ", " ## #" };
            case 'R': return { "#### ", "#   #", "#   #", "#### ", "# #  ", "#  # ", "#   #" };
            case 'S': return { " ####", "#    ", "#    ", " ### ", "    #", "    #", "#### " };
            case 'T': return { "#####", "  #  ", "  #  ", "  #  ", "  #  ", "  #  ", "  #  " };
            case 'U': return { "#   #", "#   #", "#   #", "#   #", "#   #", "#   #", " ### " };
            case 'V': return { "#   #", "#   #", "#   #", "#   #", "#   #", " # # ", "  #  " };
            case 'W': return { "#   #", "#   #", "#   #", "# # #", "# # #", "## ##", "#   #" };
            case 'X': return { "#   #", "#   #", " # # ", "  #  ", " # # ", "#   #", "#   #" };
            case 'Y': return { "#   #", "#   #", " # # ", "  #  ", "  #  ", "  #  ", "  #  " };
            case 'Z': return { "#####", "    #", "   # ", "  #  ", " #   ", "#    ", "#####" };
            case '+': return { "     ", "  #  ", "  #  ", "#####", "  #  ", "  #  ", "     " };
            case '-': return { "     ", "     ", "     ", "#####", "     ", "     ", "     " };
            case '.': return { "     ", "     ", "     ", "     ", "     ", "     ", "  #  " };
            case '/': return { "    #", "    #", "   # ", "  #  ", " #   ", "#    ", "#    " };
            case ':': return { "     ", "  #  ", "     ", "     ", "  #  ", "     ", "     " };
            case '>': return { "#    ", " #   ", "  #  ", "   # ", "  #  ", " #   ", "#    " };
            default: return { "     ", "     ", "     ", "     ", "     ", "     ", "     " };
            }
        }

        static void draw_tiny_text(
            SurfaceCanvas& canvas,
            std::string_view text,
            int x,
            int y,
            gui::Color color,
            int scale = 2) noexcept
        {
            int penX = x;
            const int safeScale = (std::max)(1, scale);
            for (const char ch : text)
            {
                if (ch == ' ')
                {
                    penX += 4 * safeScale;
                    continue;
                }

                const auto glyph = tiny_glyph(ch);
                for (int row = 0; row < 7; ++row)
                {
                    for (int col = 0; col < 5; ++col)
                    {
                        if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] != ' ')
                            canvas.fill_rect(penX + col * safeScale, y + row * safeScale, safeScale, safeScale, color);
                    }
                }
                penX += 6 * safeScale;
            }
        }

        static void draw_tiny_text_clipped(
            SurfaceCanvas& canvas,
            std::string_view text,
            int x,
            int y,
            int maxWidth,
            gui::Color color,
            int scale = 2) noexcept
        {
            int penX = x;
            const int safeScale = (std::max)(1, scale);
            const int endX = x + (std::max)(0, maxWidth);
            for (const char ch : text)
            {
                const int advance = (ch == ' ') ? (4 * safeScale) : (6 * safeScale);
                if (penX + advance > endX)
                    break;

                if (ch == ' ')
                {
                    penX += advance;
                    continue;
                }

                const auto glyph = tiny_glyph(ch);
                for (int row = 0; row < 7; ++row)
                {
                    for (int col = 0; col < 5; ++col)
                    {
                        if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] != ' ')
                            canvas.fill_rect(penX + col * safeScale, y + row * safeScale, safeScale, safeScale, color);
                    }
                }
                penX += advance;
            }
        }

        [[nodiscard]] static std::string recommended_support_tier(
            const std::shared_ptr<core::Context>& ctx,
            std::size_t workerCount)
        {
            if (!ctx)
                return "Baseline";

            if (workerCount >= 10
                && (ctx->type == core::ContextType::Vulkan
                    || ctx->type == core::ContextType::OpenGL))
            {
                return "Extended";
            }

            switch (ctx->type)
            {
            case core::ContextType::Vulkan:
                return workerCount >= 8 ? "Standard" : "Baseline";
            case core::ContextType::OpenGL:
            case core::ContextType::RayLib:
            case core::ContextType::SFML:
            case core::ContextType::SDL:
                return workerCount >= 6 ? "Standard" : "Baseline";
            case core::ContextType::Software:
            default:
                return "Baseline";
            }
        }

        [[nodiscard]] static std::string pacing_health_summary(const EditorTimeSnapshot& snapshot)
        {
            if (snapshot.paused)
                return "Paused by editor control";

            if (snapshot.fixed_dt_seconds <= 0.0)
                return "No fixed-step pacing configured";

            const double frameBudgetSeconds = snapshot.fixed_dt_seconds
                * static_cast<double>((std::max)(1u, snapshot.max_steps_per_frame));

            if (snapshot.step_budget >= snapshot.max_steps_per_frame
                && snapshot.max_steps_per_frame > 0)
            {
                return "At frame-step cap; simulation debt is being clamped";
            }

            if (snapshot.real_dt_seconds > frameBudgetSeconds * 1.15)
            {
                return "Over budget; frame time is outrunning configured simulation pacing";
            }

            if (snapshot.accumulator_seconds > snapshot.fixed_dt_seconds * 0.5)
            {
                return "Recovering accumulated step debt";
            }

            return "Healthy; frame pacing is inside the configured budget";
        }

        [[nodiscard]] static std::string compiler_identity()
        {
#if defined(__clang__)
            return std::format("Clang {}.{}.{}", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
            return std::format("MSVC {}", _MSC_VER);
#elif defined(__GNUC__)
            return std::format("GCC {}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
            return "Unknown compiler";
#endif
        }

        [[nodiscard]] static std::string language_mode_summary()
        {
            return std::format("__cplusplus={} (C++23 baseline; latest validation is optional)",
                static_cast<long long>(__cplusplus));
        }

        [[nodiscard]] static constexpr std::string_view build_configuration_label() noexcept
        {
#if defined(NDEBUG)
            return "Release";
#else
            return "Debug";
#endif
        }

        [[nodiscard]] static std::string feature_probe_summary()
        {
            return std::format("expected={} stacktrace={} execution={} contracts={} reflection={}",
                epoch::core::has_expected ? "yes" : "no",
                epoch::core::has_stacktrace ? "yes" : "no",
                epoch::core::has_std_execution ? "yes" : "no",
                epoch::core::has_contracts ? "yes" : "no",
                epoch::core::has_static_reflection ? "yes" : "no");
        }

        [[nodiscard]] static constexpr std::string_view hosted_ci_contract() noexcept
        {
            return "C++23 headless plus MSVC C++latest headless; full graphics stays local/release until runner-safe.";
        }

        [[nodiscard]] static std::string backend_runtime_guidance(
            const std::shared_ptr<core::Context>& ctx,
            std::string_view supportTier)
        {
            if (!ctx)
                return "No active backend";

            switch (ctx->type)
            {
            case core::ContextType::OpenGL:
                return std::string("Current editor priority path; keep startup presentation clean and stay within ")
                    + std::string(supportTier) + " tier expectations.";
            case core::ContextType::Vulkan:
                return "Best fit for stronger desktop tiers; keep shader/package validation honest before treating it as default.";
            case core::ContextType::Software:
                return "Correctness and capture fallback; prioritize clarity and deterministic tooling over throughput.";
            case core::ContextType::SDL:
                return "Proxy-host backend; keep detach, input ownership, and redock behavior stable before polishing extras.";
            case core::ContextType::SFML:
                return "Proxy-host backend; preserve truthful promoted-window behavior while converging input and resize parity.";
            case core::ContextType::RayLib:
                return "Useful backend baseline for detached-window truth; protect the working ownership contract while converging behavior.";
            default:
                return "Keep runtime ownership explicit and avoid backend-specific drift.";
            }
        }

        [[nodiscard]] static std::string backend_convergence_focus(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "No active backend";

            switch (ctx->type)
            {
            case core::ContextType::OpenGL:
                return "Focus: first-present stability, startup cosmetics, and single-context editor convergence.";
            case core::ContextType::Vulkan:
                return "Focus: packaging/shader validation and support-tier honesty.";
            case core::ContextType::Software:
                return "Focus: capture fidelity, deterministic output, and launcher-shell baseline behavior.";
            case core::ContextType::SDL:
                return "Focus: promoted-window detach/redock stability and input ownership.";
            case core::ContextType::SFML:
                return "Focus: promoted-window detach/redock parity and resize/input cleanup.";
            case core::ContextType::RayLib:
                return "Focus: keep the working detach contract as the runtime truth reference for the other proxy-host backends.";
            default:
                return "Focus: converge backend behavior without adding another special-case path.";
            }
        }

        [[nodiscard]] static SurfaceCanvas build_render_graph_surface(
            const SystemsSurfaceState& systems,
            bool expose_ai_inputs)
        {
            constexpr int kSurfaceWidth = 1280;
            constexpr int kSurfaceHeight = 252;
            SurfaceCanvas canvas(kSurfaceWidth, kSurfaceHeight, gui::Color{ 14, 18, 24, 255 });

            for (int x = 0; x < kSurfaceWidth; x += 40)
                canvas.fill_rect(x, 0, 1, kSurfaceHeight, gui::Color{ 24, 30, 39, 255 });

            for (int y = 34; y < kSurfaceHeight; y += 42)
                canvas.hline(0, y, kSurfaceWidth, gui::Color{ 20, 26, 34, 255 });

            struct Stage
            {
                std::string_view name{};
                std::string_view detail{};
                int lane{};
                gui::Color fill{};
                gui::Color accent{};
            };

            const std::array<Stage, 8> stages{{
                { "CAPTURE", "FRAME", 0, { 64, 86, 135, 255 }, { 154, 190, 255, 255 } },
                { "CULL", "VISIBLE SET", 0, { 54, 92, 148, 255 }, { 135, 188, 255, 255 } },
                { "SURFACE", "SCENE TEX", 1, { 53, 117, 142, 255 }, { 102, 216, 255, 255 } },
                { "LIGHT", "HELPERS", 1, { 70, 132, 96, 255 }, { 124, 244, 159, 255 } },
                { "TEMPORAL", "TIME SPINE", 2, { 146, 123, 57, 255 }, { 255, 219, 112, 255 } },
                { "PRESENT", "ONE SWAP", 2, { 109, 84, 145, 255 }, { 203, 164, 255, 255 } },
                { "GUI TOP", "MODAL UI", 3, { 112, 96, 152, 255 }, { 222, 192, 255, 255 } },
                { "OS AI", "GATED", 3,
                  expose_ai_inputs ? gui::Color{ 157, 88, 112, 255 } : gui::Color{ 118, 86, 123, 255 },
                  expose_ai_inputs ? gui::Color{ 255, 171, 193, 255 } : gui::Color{ 205, 170, 216, 255 } }
            }};

            const int stageWidth = (std::max)(126, static_cast<int>(144.0f * systems.renderZoom));
            const int stageHeight = 54;
            const int gap = (std::max)(22, static_cast<int>(34.0f * systems.renderZoom));
            const int baseX = 150 - systems.renderPan;
            constexpr std::array<int, 4> laneY{ 58, 104, 150, 196 };
            constexpr std::array<std::string_view, 4> laneNames{ "GPU", "SCENE", "TIME", "UI/AI" };

            canvas.fill_rect(18, 14, kSurfaceWidth - 36, 24, gui::Color{ 30, 38, 48, 255 });
            draw_tiny_text_clipped(canvas, "RENDER FRAME GRAPH - SCENE ONCE - GUI TOP AFTER SCENE - ONE PRESENT", 30, 20, kSurfaceWidth - 70, gui::Color{ 210, 224, 242, 255 }, 1);
            canvas.fill_rect(18, kSurfaceHeight - 28, kSurfaceWidth - 36, 14, gui::Color{ 28, 33, 41, 255 });
            canvas.fill_rect(18, kSurfaceHeight - 28, 150, 14, gui::Color{ 89, 110, 138, 255 });
            canvas.fill_rect(180, kSurfaceHeight - 28, 150, 14, gui::Color{ 98, 152, 116, 255 });
            canvas.fill_rect(342, kSurfaceHeight - 28, 138, 14, gui::Color{ 149, 122, 60, 255 });
            canvas.fill_rect(492, kSurfaceHeight - 28, 160, 14, gui::Color{ 127, 97, 155, 255 });
            canvas.fill_rect(664, kSurfaceHeight - 28, 128, 14, gui::Color{ 157, 88, 112, 255 });

            for (std::size_t lane = 0; lane < laneY.size(); ++lane)
            {
                const int y = laneY[lane];
                canvas.fill_rect(20, y + 15, kSurfaceWidth - 40, 2, gui::Color{ 37, 43, 55, 255 });
                canvas.fill_rect(26, y, 96, 34, gui::Color{ 26, 32, 42, 255 });
                canvas.stroke_rect(26, y, 96, 34, gui::Color{ 255, 255, 255, 24 });
                draw_tiny_text(canvas, laneNames[lane], 42, y + 9, gui::Color{ 196, 208, 224, 255 }, 1);
            }

            for (std::size_t i = 0; i < stages.size(); ++i)
            {
                const int x = baseX + static_cast<int>(i) * (stageWidth + gap);
                const auto& stage = stages[i];
                const int y = laneY[static_cast<std::size_t>(stage.lane)] - 8;

                if (i != 0)
                {
                    const int prevCenter = x - gap + gap / 2;
                    canvas.fill_rect(prevCenter - 1, y + stageHeight / 2 - 2, gap + 2, 4, gui::Color{ 56, 63, 82, 255 });
                }

                canvas.fill_rect(x, y, stageWidth, stageHeight, stage.fill);
                canvas.stroke_rect(x, y, stageWidth, stageHeight, stage.accent);
                canvas.fill_rect(x + 8, y + 8, 18, stageHeight - 16, gui::Color{ 255, 255, 255, 32 });
                canvas.fill_rect(x + stageWidth - 12, y + 10, 5, stageHeight - 20, stage.accent);
                draw_tiny_text_clipped(canvas, stage.name, x + 34, y + 11, stageWidth - 48, gui::Color{ 232, 238, 248, 255 }, 1);
                draw_tiny_text_clipped(canvas, stage.detail, x + 34, y + 30, stageWidth - 48, gui::Color{ 190, 202, 218, 255 }, 1);
            }

            draw_tiny_text_clipped(canvas, "BLUE BACKEND   GREEN SCENE   GOLD TIME   PURPLE UI   PINK OS AI", 30, kSurfaceHeight - 24, kSurfaceWidth - 60, gui::Color{ 220, 226, 236, 255 }, 1);

            return canvas;
        }

        [[nodiscard]] static SurfaceCanvas build_task_graph_surface(
            const SystemsSurfaceState& systems,
            std::size_t liveThreadCount,
            std::size_t systemCount)
        {
            constexpr int kSurfaceWidth = 1280;
            constexpr int kSurfaceHeight = 260;
            SurfaceCanvas canvas(kSurfaceWidth, kSurfaceHeight, gui::Color{ 15, 18, 24, 255 });

            const int visibleLaneCount = (std::clamp)(
                liveThreadCount == 0 ? 2 : static_cast<int>((std::min)(liveThreadCount, std::size_t{ 3 })),
                2,
                3);
            const int laneGap = 8;
            const int laneHeight = (kSurfaceHeight - 68 - laneGap * (visibleLaneCount - 1)) / visibleLaneCount;
            const int baseX = 150 - systems.taskPan;
            const int taskWidth = (std::max)(96, static_cast<int>(128.0f * systems.taskZoom));
            const int taskGap = (std::max)(18, static_cast<int>(28.0f * systems.taskZoom));
            const std::string taskHeader = std::string("TASK THREAD GRAPH  LIVE THREADS ")
                + std::to_string(liveThreadCount)
                + "  SHOWN LANES "
                + std::to_string(visibleLaneCount)
                + "  SYSTEMS "
                + std::to_string(systemCount);
            canvas.fill_rect(18, 14, kSurfaceWidth - 36, 24, gui::Color{ 30, 38, 48, 255 });
            draw_tiny_text_clipped(canvas, taskHeader, 30, 20, kSurfaceWidth - 70, gui::Color{ 214, 224, 238, 255 }, 1);

            const std::array<gui::Color, 5> taskColors{{
                { 86, 142, 255, 255 },
                { 90, 193, 142, 255 },
                { 249, 190, 76, 255 },
                { 198, 116, 255, 255 },
                { 255, 118, 150, 255 }
            }};
            constexpr std::array<std::string_view, 5> taskNames{
                "INPUT",
                "SYSTEM",
                "SCRIPT",
                "AI",
                "OUTPUT"
            };

            for (int x = baseX; x < kSurfaceWidth; x += taskWidth + taskGap)
                canvas.vline(x, 44, kSurfaceHeight - 62, gui::Color{ 28, 32, 42, 255 });

            for (int lane = 0; lane < visibleLaneCount; ++lane)
            {
                const int y = 48 + lane * (laneHeight + laneGap);
                canvas.fill_rect(24, y, kSurfaceWidth - 48, laneHeight, gui::Color{ 20, 25, 34, 255 });
                canvas.stroke_rect(24, y, kSurfaceWidth - 48, laneHeight, gui::Color{ 255, 255, 255, 20 });
                canvas.fill_rect(32, y + 4, 82, laneHeight - 8, gui::Color{ 31, 38, 50, 255 });
                draw_tiny_text(canvas, std::string("LANE ") + std::to_string(lane + 1), 46, y + (std::max)(4, laneHeight / 2 - 5), gui::Color{ 198, 210, 226, 255 }, 1);

                const int blocks = 4 + static_cast<int>((systemCount + static_cast<std::size_t>(lane)) % 3u);
                for (int block = 0; block < blocks; ++block)
                {
                    const int x = baseX + block * (taskWidth + taskGap) + lane * 22;
                    const gui::Color fill = taskColors[(static_cast<std::size_t>(block) + static_cast<std::size_t>(lane)) % taskColors.size()];
                    const int taskHeight = (std::max)(12, laneHeight - 8 - (block % 3) * 3);
                    const int taskY = y + (laneHeight - taskHeight) / 2;
                    canvas.fill_rect(x, taskY, taskWidth, taskHeight, fill);
                    canvas.stroke_rect(x, taskY, taskWidth, taskHeight, gui::Color{ 255, 255, 255, 46 });
                    draw_tiny_text_clipped(
                        canvas,
                        taskNames[(static_cast<std::size_t>(block) + static_cast<std::size_t>(lane)) % taskNames.size()],
                        x + 6,
                        taskY + (std::max)(3, taskHeight / 2 - 4),
                        taskWidth - 12,
                        gui::Color{ 238, 242, 248, 255 },
                        1);
                    if (block != 0)
                        canvas.fill_rect(x - taskGap + taskGap / 2 - 1, y + laneHeight / 2 - 2, taskGap + 2, 4, gui::Color{ 58, 65, 79, 255 });
                }
            }

            canvas.fill_rect(18, kSurfaceHeight - 28, kSurfaceWidth - 36, 14, gui::Color{ 28, 33, 41, 255 });
            canvas.fill_rect(18, kSurfaceHeight - 28, (std::min)(kSurfaceWidth - 36, static_cast<int>(liveThreadCount) * 16 + 80), 14, gui::Color{ 89, 132, 184, 255 });
            draw_tiny_text_clipped(canvas, "INPUT > SYSTEM > SCRIPT > OS AI > OUTPUT   LIVE LANES SHOWN WITHOUT SQUASHING", 30, kSurfaceHeight - 24, kSurfaceWidth - 60, gui::Color{ 220, 226, 236, 255 }, 1);

            return canvas;
        }

        [[nodiscard]] static SurfaceCanvas build_support_tier_surface(
            std::string_view activeTier,
            bool prefersSoftwareFallback)
        {
            constexpr int kSurfaceWidth = 960;
            constexpr int kSurfaceHeight = 108;
            SurfaceCanvas canvas(kSurfaceWidth, kSurfaceHeight, gui::Color{ 15, 18, 24, 255 });

            struct TierCard
            {
                gui::Color fill{};
                gui::Color accent{};
                int x{};
                int w{};
                bool active{};
            };

            const std::array<TierCard, 3> cards{{
                { { 54, 92, 148, 255 }, { 135, 188, 255, 255 }, 22, 282, activeTier == "Baseline" },
                { { 70, 132, 96, 255 }, { 124, 244, 159, 255 }, 338, 282, activeTier == "Standard" },
                { { 146, 123, 57, 255 }, { 255, 219, 112, 255 }, 654, 282, activeTier == "Extended" }
            }};
            constexpr std::array<std::string_view, 3> tierNames{
                "BASELINE",
                "STANDARD",
                "EXTENDED"
            };

            for (std::size_t i = 0; i < cards.size(); ++i)
            {
                const auto& card = cards[i];
                const int y = 18;
                const int h = 72;
                canvas.fill_rect(card.x, y, card.w, h, card.fill);
                canvas.stroke_rect(card.x, y, card.w, h, card.active ? card.accent : gui::Color{ 255, 255, 255, 30 });
                canvas.fill_rect(card.x + 12, y + 12, 42, h - 24, gui::Color{ 255, 255, 255, 28 });
                draw_tiny_text(canvas, tierNames[i], card.x + 70, y + 26, gui::Color{ 232, 238, 248, 255 }, 2);
                if (card.active)
                    canvas.fill_rect(card.x + card.w - 14, y + 10, 8, h - 20, card.accent);
            }

            if (prefersSoftwareFallback)
                canvas.fill_rect(22, 92, 220, 6, gui::Color{ 174, 124, 89, 255 });
            else
                canvas.fill_rect(22, 92, 220, 6, gui::Color{ 95, 174, 127, 255 });
            draw_tiny_text(canvas, std::string("ACTIVE ") + std::string(activeTier), 260, 90, gui::Color{ 218, 226, 236, 255 }, 1);

            return canvas;
        }

        [[nodiscard]] static SurfaceCanvas build_ai_loop_surface(
            const std::array<bool, 5>& readiness,
            bool gateReady,
            bool buildPending)
        {
            constexpr int kSurfaceWidth = 960;
            constexpr int kSurfaceHeight = 124;
            SurfaceCanvas canvas(kSurfaceWidth, kSurfaceHeight, gui::Color{ 12, 15, 21, 255 });

            for (int x = 24; x < kSurfaceWidth; x += 48)
                canvas.vline(x, 0, kSurfaceHeight, gui::Color{ 20, 25, 34, 255 });
            for (int y = 20; y < kSurfaceHeight; y += 28)
                canvas.hline(0, y, kSurfaceWidth, gui::Color{ 18, 23, 31, 255 });

            const std::array<gui::Color, 5> accents{{
                { 91, 151, 255, 255 },
                { 255, 196, 88, 255 },
                { 88, 211, 159, 255 },
                { 190, 128, 255, 255 },
                { 255, 132, 155, 255 }
            }};
            constexpr std::array<std::string_view, 5> stageNames{
                "PLAN",
                "EXEC",
                "BUILD",
                "VERIFY",
                "GATE"
            };
            draw_tiny_text(canvas, "OS AI EVIDENCE LOOP", 34, 12, gui::Color{ 214, 224, 238, 255 }, 1);

            int active = 4;
            for (int i = 0; i < 5; ++i)
            {
                if (!readiness[static_cast<std::size_t>(i)])
                {
                    active = i;
                    break;
                }
            }

            const int cardW = 136;
            const int cardH = 54;
            const int gap = 44;
            const int baseX = 34;
            const int y = 38;

            for (int i = 0; i < 5; ++i)
            {
                const int x = baseX + i * (cardW + gap);
                const bool ready = readiness[static_cast<std::size_t>(i)];
                const bool current = i == active;
                const auto accent = accents[static_cast<std::size_t>(i)];
                const gui::Color fill = ready
                    ? gui::Color{ 38, 82, 66, 255 }
                    : (current ? gui::Color{ 92, 73, 38, 255 } : gui::Color{ 42, 48, 60, 255 });
                const gui::Color stroke = ready ? gui::Color{ 102, 244, 168, 255 } : (current ? accent : gui::Color{ 255, 255, 255, 36 });

                if (i != 0)
                    canvas.fill_rect(x - gap + 8, y + cardH / 2 - 3, gap - 16, 6, gui::Color{ 54, 62, 78, 255 });

                canvas.fill_rect(x, y, cardW, cardH, fill);
                canvas.stroke_rect(x, y, cardW, cardH, stroke);
                canvas.fill_rect(x + 12, y + 12, 28, cardH - 24, gui::Color{ 255, 255, 255, static_cast<std::uint8_t>(ready ? 52 : 28) });
                canvas.fill_rect(x + cardW - 14, y + 10, 6, cardH - 20, accent);
                draw_tiny_text(canvas, stageNames[static_cast<std::size_t>(i)], x + 48, y + 20, gui::Color{ 232, 238, 248, 255 }, 2);
                if (current)
                    canvas.fill_rect(x + 8, y - 10, cardW - 16, 5, accent);
            }

            const int progressW = 820;
            canvas.fill_rect(34, 104, progressW, 8, gui::Color{ 30, 35, 45, 255 });
            const int readyCount = static_cast<int>(std::count(readiness.begin(), readiness.end(), true));
            canvas.fill_rect(34, 104, (progressW * readyCount) / 5, 8, gui::Color{ 94, 201, 142, 255 });

            if (gateReady)
                canvas.fill_rect(866, 20, 68, 16, gui::Color{ 80, 167, 115, 255 });
            else
                canvas.fill_rect(866, 20, 68, 16, gui::Color{ 97, 80, 70, 255 });

            if (buildPending)
                canvas.fill_rect(866, 44, 68, 16, gui::Color{ 223, 174, 77, 255 });
            else
                canvas.fill_rect(866, 44, 68, 16, gui::Color{ 58, 68, 84, 255 });
            draw_tiny_text(canvas, gateReady ? "GATE OK" : "GATE WAIT", 764, 23, gui::Color{ 238, 242, 248, 255 }, 1);
            draw_tiny_text(canvas, buildPending ? "BUILD RUN" : "BUILD IDLE", 764, 47, gui::Color{ 238, 242, 248, 255 }, 1);

            return canvas;
        }

        [[nodiscard]] EditorAutomationCommand read_editor_automation_command() noexcept
        {
            std::string value;

#if defined(_WIN32)
            char* raw = nullptr;
            std::size_t raw_size = 0;
            if (_dupenv_s(&raw, &raw_size, "EPOCH_EDITOR_AUTO_COMMAND") != 0 || raw == nullptr)
                return EditorAutomationCommand::None;

            value.assign(raw, raw_size > 0 ? raw_size - 1 : 0);
            free(raw);
#else
            if (const char* const raw = std::getenv("EPOCH_EDITOR_AUTO_COMMAND"))
                value = raw;
            else
                return EditorAutomationCommand::None;
#endif

            if (value == "smart-update")
                return EditorAutomationCommand::SmartUpdate;
            if (value == "source-update")
                return EditorAutomationCommand::SourceUpdate;

            return EditorAutomationCommand::None;
        }

        void append_editor_automation_trace(const std::string_view message)
        {
            std::ofstream trace("epoch_editor_auto_command.log", std::ios::app | std::ios::binary);
            if (!trace)
                return;

            trace << message << '\n';
        }

        [[nodiscard]] std::string read_requested_editor_project_id() noexcept
        {
#if defined(_WIN32)
            char* raw = nullptr;
            std::size_t raw_size = 0;
            if (_dupenv_s(&raw, &raw_size, "EPOCH_EDITOR_PROJECT_ID") != 0 || raw == nullptr)
                return {};

            std::string value{ raw };
            std::free(raw);
            return value;
#else
            if (const char* const raw = std::getenv("EPOCH_EDITOR_PROJECT_ID"))
                return std::string{ raw };
            return {};
#endif
        }

        [[nodiscard]] bool try_claim_editor_automation_command(EditorAutomationCommand command) noexcept
        {
            if (command == EditorAutomationCommand::None)
                return false;

            static std::atomic<bool> claimed{ false };
            bool expected = false;
            return claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }

        void set_project(EditorState& state, std::string_view projectId, bool writeLog)
        {
            const auto* profile = editor_find_project_profile(projectId);
            if (!profile)
                profile = &editor_default_project_profile();

            state.helpersVisible = true;
            state.projectId = std::string(profile->id);
            state.projectName = std::string(profile->display_name);
            state.projectRoot = std::string(profile->root_path);
            state.projectScenePath = std::string(profile->scene_path);
            state.projectManifest = std::string(profile->manifest_path);
            state.projectTemplate = std::string(profile->template_family);
            state.projectKind = std::string(editor_project_kind_name(profile->kind));
            state.activeWorld = std::string(profile->world_name);
            state.activeScript = std::string(profile->default_script);
            state.activeRuntimeScene = std::string(profile->runtime_scene_id);
            state.projectCameraMode = profile->id == std::string_view{ "twodstudio" }
                ? previewgrid::CameraMode::Canvas2D
                : previewgrid::CameraMode::Editor;
            state.projectStatus = "Selected project profile. Use File > Save Project or the centered Run button to materialize/update generated project files.";
            state.projectBuildStatus = "Build Project creates or refreshes a repo-local child executable for the active shell after an explicit save/run.";
            state.scriptBuildStatus = "Select a script to validate or run against the active project shell.";

            state.entities.clear();
            for (const auto& seed : editor_seed_entities_for_project(profile->id))
            {
                state.entities.push_back(EditorEntity{
                    .name = std::string(seed.name),
                    .type = std::string(seed.type),
                    .category = std::string(seed.category),
                    .position = seed.position,
                    .rotation = seed.rotation,
                    .scale = seed.scale,
                    .visible = seed.visible,
                    .editorOnly = seed.editor_only
                });
            }

            (void)load_editor_scene_snapshot(state);

            state.selectedEntity = state.entities.empty() ? 0u : (std::min)(state.selectedEntity, state.entities.size() - 1u);
            if (writeLog)
                push_editor_log(
                    state,
                    std::string("[project] Loaded ")
                    + state.projectName
                    + " -> "
                    + state.activeRuntimeScene
                    + ".");
        }

        [[nodiscard]] const EditorScriptProfile* active_script_profile(const EditorState& state) noexcept
        {
            for (const auto& script : editor_script_profiles())
            {
                if (script.id == state.activeScript)
                    return &script;
            }
            return nullptr;
        }

        [[nodiscard]] std::string make_entity_name(const EditorState& state, std::string_view base)
        {
            std::size_t ordinal = 1;
            for (const auto& entity : state.entities)
            {
                if (entity.name.starts_with(base))
                    ++ordinal;
            }

            return std::format("{}_{:02}", base, ordinal);
        }

        [[nodiscard]] std::array<float, 3> next_entity_position(const EditorState& state, float baseY = 0.5f)
        {
            const float offset = static_cast<float>(state.entities.size() % 5u) * 1.35f;
            return {
                -2.7f + offset,
                baseY,
                1.6f - static_cast<float>((state.entities.size() / 5u) % 4u) * 1.15f
            };
        }

        void add_entity(EditorState& state, std::string_view archetype)
        {
            EditorEntity entity{};
            if (archetype == "cube")
            {
                entity.name = make_entity_name(state, "StaticMesh");
                entity.type = "StaticMesh";
                entity.category = "Gameplay";
                entity.position = next_entity_position(state, 0.5f);
            }
            else if (archetype == "light")
            {
                entity.name = make_entity_name(state, "PointLight");
                entity.type = "Light";
                entity.category = "Lighting";
                entity.position = next_entity_position(state, 2.4f);
            }
            else if (archetype == "spawn")
            {
                entity.name = make_entity_name(state, "PlayerStart");
                entity.type = "Spawn";
                entity.category = "Gameplay";
                entity.position = next_entity_position(state, 0.0f);
            }
            else if (archetype == "camera")
            {
                entity.name = make_entity_name(state, "PreviewCamera");
                entity.type = "Camera";
                entity.category = "Gameplay";
                entity.position = next_entity_position(state, 1.8f);
                entity.rotation = { -18.0f, 0.0f, 0.0f };
            }
            else
            {
                return;
            }

            state.entities.push_back(std::move(entity));
            state.selectedEntity = state.entities.empty() ? 0u : (state.entities.size() - 1u);
            push_editor_log(
                state,
                std::string("[entity] Added ")
                + state.entities[state.selectedEntity].name
                + " ["
                + state.entities[state.selectedEntity].type
                + "].");
        }

        void ensure_2d_canvas_entity(EditorState& state)
        {
            const auto existing = std::find_if(
                state.entities.begin(),
                state.entities.end(),
                [](const EditorEntity& entity)
                {
                    return entity.type == "Canvas2D" || entity.name == "Canvas2D";
                });
            if (existing != state.entities.end())
            {
                existing->editorOnly = true;
                existing->category = "2D";
                existing->position = { 0.0f, 1.8f, 0.0f };
                existing->rotation = { 0.0f, 0.0f, 0.0f };
                existing->scale = { 6.4f, 3.6f, 0.05f };
                state.selectedEntity = static_cast<std::size_t>(std::distance(state.entities.begin(), existing));
                return;
            }

            EditorEntity canvas{};
            canvas.name = "Canvas2D";
            canvas.type = "Canvas2D";
            canvas.category = "2D";
            canvas.position = { 0.0f, 1.8f, 0.0f };
            canvas.rotation = { 0.0f, 0.0f, 0.0f };
            canvas.scale = { 6.4f, 3.6f, 0.05f };
            canvas.editorOnly = true;
            state.entities.push_back(std::move(canvas));
            state.selectedEntity = state.entities.size() - 1u;
            push_editor_log(state, "[2d] Added editor-only Canvas2D editing plane.");
        }

        void ensure_engine_arcade_preview_entities(EditorState& state)
        {
            auto upsert = [&](std::string_view name, std::string_view type, std::array<float, 3> position, std::array<float, 3> scale)
            {
                auto existing = std::find_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [&](const EditorEntity& entity)
                    {
                        return entity.name == name;
                    });

                if (existing != state.entities.end())
                {
                    existing->type = std::string(type);
                    existing->category = "EngineArcade";
                    existing->position = position;
                    existing->rotation = { 0.0f, 0.0f, 0.0f };
                    existing->scale = scale;
                    existing->editorOnly = true;
                    existing->visible = true;
                    return static_cast<std::size_t>(std::distance(state.entities.begin(), existing));
                }

                EditorEntity entity{};
                entity.name = std::string(name);
                entity.type = std::string(type);
                entity.category = "EngineArcade";
                entity.position = position;
                entity.rotation = { 0.0f, 0.0f, 0.0f };
                entity.scale = scale;
                entity.editorOnly = true;
                entity.visible = true;
                state.entities.push_back(std::move(entity));
                return state.entities.size() - 1u;
            };

            const auto baseIndex = upsert(
                "EngineArcadeCabinetBase",
                "StaticMesh",
                { 0.0f, 0.22f, 0.36f },
                { 1.70f, 0.44f, 0.82f });
            upsert(
                "EngineArcadeCabinetBody",
                "StaticMesh",
                { 0.0f, 0.92f, 0.24f },
                { 1.38f, 1.30f, 0.54f });
            upsert(
                "EngineArcadeControlDeck",
                "StaticMesh",
                { 0.0f, 1.18f, -0.30f },
                { 1.56f, 0.20f, 0.70f });
            const auto screenIndex = upsert(
                "EngineArcadeScreen",
                "Canvas2D",
                { 0.0f, 1.78f, -0.42f },
                { 2.22f, 1.22f, 0.06f });
            upsert(
                "EngineArcadeMarquee",
                "Canvas2D",
                { 0.0f, 2.46f, -0.34f },
                { 2.10f, 0.42f, 0.05f });

            state.selectedEntity = screenIndex < state.entities.size() ? screenIndex : baseIndex;
        }

        void activate_engine_arcade_preview(EditorState& state)
        {
            state.activeRuntimeScene = std::string(epoch::package_registry::engine_arcade_default_scene_id());
            state.projectStatus =
                "Engine Arcade active: default scene selected and render-to-texture screen staged in 3D Scene.";
            state.projectCameraMode = previewgrid::CameraMode::Editor;
            state.previewMode = core::ScenePreviewMode::Editor;
            state.mainSurface = EditorMainSurface::Scene;
            state.surfaceSettleFrames = (std::max)(state.surfaceSettleFrames, 2);
            ensure_engine_arcade_preview_entities(state);
            push_editor_log(
                state,
                "[package] Engine Arcade activated: default scene "
                    + state.activeRuntimeScene
                    + " selected and visible RTT preview staged.");
        }

        void deactivate_engine_arcade_preview(EditorState& state)
        {
            std::erase_if(
                state.entities,
                [](const EditorEntity& entity)
                {
                    return entity.category == "EngineArcade";
                });
            const auto sceneIds = epoch::package_registry::engine_arcade_scene_ids();
            const std::string_view activeScene{ state.activeRuntimeScene };
            bool activeSceneIsArcade = false;
            std::string_view remaining = sceneIds;
            while (!activeScene.empty() && !remaining.empty())
            {
                const auto comma = remaining.find(',');
                const auto scene = comma == std::string_view::npos ? remaining : remaining.substr(0u, comma);
                if (scene == activeScene)
                {
                    activeSceneIsArcade = true;
                    break;
                }
                if (comma == std::string_view::npos)
                    break;
                remaining.remove_prefix(comma + 1u);
            }
            if (activeSceneIsArcade)
                state.activeRuntimeScene.clear();
            if (state.selectedEntity >= state.entities.size())
                state.selectedEntity = state.entities.empty() ? 0u : (state.entities.size() - 1u);
            state.projectStatus = "Engine Arcade removed from the active project preview.";
            state.surfaceSettleFrames = (std::max)(state.surfaceSettleFrames, 2);
            push_editor_log(state, "[package] Engine Arcade preview state removed.");
        }

        void ensure_forest_factory_preview_entities(EditorState& state)
        {
            const std::size_t beforeCount = state.entities.size();
            auto profile = epoch::forest::default_profile(epoch::forest::ForestPreset::Tree);
            profile.config.targetHeightMeters = 4.2F;
            profile.config.trunkRadiusMeters = 0.09F;
            profile.branch.levels = 4u;
            profile.branch.branchLengthMeters = 0.92F;
            profile.branch.startHeightMeters = 0.24F;
            const auto geometry = epoch::forest::build_preview_geometry(profile);

            const auto is_forest_entity = [](const EditorEntity& entity) noexcept
            {
                return entity.category == "ForestFactory";
            };

            std::string previousSelection{};
            if (!state.entities.empty())
            {
                const std::size_t selectedIndex = (std::min)(state.selectedEntity, state.entities.size() - 1u);
                if (!is_forest_entity(state.entities[selectedIndex]))
                    previousSelection = state.entities[selectedIndex].name;
            }

            std::erase_if(
                state.entities,
                is_forest_entity);

            auto upsert = [&](EditorEntity entity)
            {
                const auto existing = std::find_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [&](const EditorEntity& candidate)
                    {
                        return candidate.name == entity.name;
                    });
                if (existing != state.entities.end())
                    *existing = std::move(entity);
                else
                    state.entities.push_back(std::move(entity));
            };

            constexpr float kPreviewScale = 0.58F;
            auto scaled_position = [](epoch::voxel::Float3 value) noexcept
            {
                constexpr float scale = 0.58F;
                return std::array<float, 3>{
                    value.x * scale,
                    value.y * scale,
                    value.z * scale
                };
            };

            if (geometry.segmentCount > 0u)
            {
                const auto& trunk = geometry.segments[0];
                EditorEntity trunkEntity{};
                trunkEntity.name = "ForestFactoryTrunk";
                trunkEntity.type = "ForestTrunk";
                trunkEntity.category = "ForestFactory";
                trunkEntity.position = {
                    (trunk.start.x + trunk.end.x) * 0.5F * kPreviewScale,
                    (trunk.start.y + trunk.end.y) * 0.5F * kPreviewScale,
                    (trunk.start.z + trunk.end.z) * 0.5F * kPreviewScale
                };
                const float trunkHeight = (std::max)(0.32F, (trunk.end.y - trunk.start.y) * kPreviewScale);
                const float trunkWidth = (std::max)(0.12F, trunk.radius * 2.8F * kPreviewScale);
                trunkEntity.scale = { trunkWidth, trunkHeight, trunkWidth };
                trunkEntity.editorOnly = true;
                upsert(std::move(trunkEntity));
            }

            const std::size_t branchBudget = (std::min)(geometry.segmentCount > 0u ? geometry.segmentCount - 1u : 0u, std::size_t{ 32u });
            const std::size_t branchStep = branchBudget > 0u
                ? (std::max)(std::size_t{ 1u }, (geometry.segmentCount - 1u) / branchBudget)
                : 1u;
            std::size_t branchOrdinal = 0u;
            for (std::size_t i = 1u; i < geometry.segmentCount && branchOrdinal < branchBudget; i += branchStep, ++branchOrdinal)
            {
                const auto& segment = geometry.segments[i];
                EditorEntity node{};
                node.name = std::format("ForestFactoryBranch_{:02}", branchOrdinal);
                node.type = "ForestBranchJoint";
                node.category = "ForestFactory";
                node.position = scaled_position(segment.end);
                const float nodeSize = (std::clamp)(segment.radius * 2.1F * kPreviewScale, 0.055F, 0.16F);
                node.scale = { nodeSize, nodeSize, nodeSize };
                node.editorOnly = true;
                upsert(std::move(node));
            }

            const std::size_t canopyBudget = (std::min)(geometry.leafCount, std::size_t{ 28u });
            const std::size_t leafStep = canopyBudget > 0u
                ? (std::max)(std::size_t{ 1u }, geometry.leafCount / canopyBudget)
                : 1u;
            std::size_t canopyOrdinal = 0u;
            for (std::size_t i = 0u; i < geometry.leafCount && canopyOrdinal < canopyBudget; i += leafStep, ++canopyOrdinal)
            {
                const auto& leaf = geometry.leaves[i];
                EditorEntity leafNode{};
                leafNode.name = std::format("ForestFactoryCanopy_{:02}", canopyOrdinal);
                leafNode.type = "ForestFoliageCluster";
                leafNode.category = "ForestFactory";
                leafNode.position = scaled_position(leaf.position);
                const float leafSize = (std::clamp)(leaf.size * 1.15F * kPreviewScale, 0.10F, 0.24F);
                leafNode.scale = { leafSize, leafSize * 0.62F, leafSize };
                leafNode.editorOnly = true;
                upsert(std::move(leafNode));
            }

            auto restored = previousSelection.empty()
                ? state.entities.end()
                : std::find_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [&](const EditorEntity& entity)
                    {
                        return entity.name == previousSelection;
                    });
            if (restored == state.entities.end())
            {
                restored = std::find_if(
                    state.entities.begin(),
                    state.entities.end(),
                    [&](const EditorEntity& entity)
                    {
                        return !is_forest_entity(entity);
                    });
            }
            if (restored != state.entities.end())
                state.selectedEntity = static_cast<std::size_t>(std::distance(state.entities.begin(), restored));
            else
                state.selectedEntity = 0u;

            if (state.entities.size() != beforeCount)
            {
                push_editor_log(
                    state,
                    std::format(
                        "[forest] Rebuilt temporal graph Plant Lab preview: {} segments, {} canopy markers.",
                        geometry.segmentCount,
                        canopyBudget));
            }
        }

        [[nodiscard]] bool is_forest_factory_entity(const EditorEntity& entity) noexcept
        {
            return entity.category == "ForestFactory";
        }

        [[nodiscard]] bool is_selected_forest_factory_entity(const EditorEntity& entity, bool selected) noexcept
        {
            return selected && is_forest_factory_entity(entity);
        }

        [[nodiscard]] epochnamespace::previewgrid::Vec3 forest_factory_color_for_entity(
            const EditorEntity& entity) noexcept
        {
            if (entity.type == "ForestTrunk")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::forest_trunk());
            if (entity.type == "ForestBranchJoint")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::forest_branch_joint());
            if (entity.type == "ForestFoliageCluster")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::forest_foliage_cluster());
            return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::forest_default());
        }

        [[nodiscard]] float forest_factory_radius_for_entity(const EditorEntity& entity) noexcept
        {
            const float scaleMax = (std::max)(entity.scale[0], (std::max)(entity.scale[1], entity.scale[2]));
            if (entity.type == "ForestTrunk")
                return (std::clamp)(0.22f * scaleMax, 0.16f, 0.34f);
            if (entity.type == "ForestBranchJoint")
                return (std::clamp)(0.34f * scaleMax, 0.16f, 0.24f);
            if (entity.type == "ForestFoliageCluster")
                return (std::clamp)(0.38f * scaleMax, 0.16f, 0.28f);
            return (std::clamp)(0.34f * scaleMax, 0.16f, 0.36f);
        }

        [[nodiscard]] epochnamespace::previewgrid::Vec3 marker_color_for_entity(
            const EditorEntity& entity,
            bool selected) noexcept
        {
            if (is_selected_forest_factory_entity(entity, selected))
                return forest_factory_color_for_entity(entity);
            if (selected)
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::object_selected());
            if (entity.type == "Light")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::object_light());
            if (entity.type == "Spawn")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::object_spawn());
            if (entity.type == "Camera")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::object_camera());
            if (entity.category == "ForestFactory")
                return forest_factory_color_for_entity(entity);
            if (entity.category == "World" || entity.type == "Level")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::object_world());
            if (entity.editorOnly || entity.category == "Editor")
                return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::object_editor_helper());
            return epochnamespace::previewgrid::visual_rgb(epochnamespace::visuals::object_default());
        }

        [[nodiscard]] float marker_radius_for_entity(const EditorEntity& entity) noexcept
        {
            const float scaleMax = (std::max)(entity.scale[0], (std::max)(entity.scale[1], entity.scale[2]));
            if (entity.type == "Light")
                return 0.42f;
            if (entity.type == "Spawn")
                return 0.32f;
            if (entity.type == "Camera")
                return 0.38f;
            if (entity.category == "World" || entity.type == "Level")
                return 0.75f;
            if (entity.category == "ForestFactory")
                return forest_factory_radius_for_entity(entity);
            return (std::clamp)(0.34f * scaleMax, 0.24f, 1.20f);
        }

        [[nodiscard]] epochnamespace::previewgrid::ObjectPreviewPrimitive preview_primitive_for_entity(
            const EditorEntity& entity) noexcept
        {
            if (entity.type == "Light")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Light;
            if (entity.type == "Spawn")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Spawn;
            if (entity.type == "Camera")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Camera;
            if (entity.type == "Canvas2D")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Canvas2D;
            if (entity.type == "ForestTrunk")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::ForestTrunk;
            if (entity.type == "ForestBranchJoint")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::ForestBranch;
            if (entity.type == "ForestFoliageCluster")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::ForestLeafCluster;
            if (entity.category == "World" || entity.type == "Level")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Level;
            return epochnamespace::previewgrid::ObjectPreviewPrimitive::Cube;
        }

        [[nodiscard]] epochnamespace::previewgrid::Vec3 selection_color_for_entity(
            const EditorEntity& entity,
            bool selected) noexcept
        {
            if (is_selected_forest_factory_entity(entity, selected))
                return forest_factory_color_for_entity(entity);
            return marker_color_for_entity(entity, selected);
        }

        [[nodiscard]] bool marker_selected_for_entity(const EditorEntity& entity, bool selected) noexcept
        {
            return selected && !is_forest_factory_entity(entity);
        }

        [[nodiscard]] std::size_t visible_entity_count(const EditorState& state) noexcept
        {
            std::size_t count = 0;
            for (const auto& entity : state.entities)
                if (entity.visible)
                    ++count;
            return count;
        }

        void publish_editor_preview_markers(const core::Context* ctx, const EditorState& state)
        {
            if (!ctx || state.previewMode != core::ScenePreviewMode::Editor)
            {
                epochnamespace::previewgrid::clear_object_markers(ctx);
                return;
            }

            std::vector<epochnamespace::previewgrid::ObjectMarker> markers{};
            markers.reserve(state.entities.size());
            for (std::size_t i = 0; i < state.entities.size(); ++i)
            {
                const auto& entity = state.entities[i];
                if (!entity.visible)
                    continue;

                const bool selected = i == (std::min)(state.selectedEntity, state.entities.size() - 1u);
                markers.push_back(epochnamespace::previewgrid::ObjectMarker{
                    .position{
                        entity.position[0],
                        entity.position[1],
                        entity.position[2]
                    },
                    .color = selection_color_for_entity(entity, selected),
                    .scale{
                        entity.scale[0],
                        entity.scale[1],
                        entity.scale[2]
                    },
                    .radius = marker_radius_for_entity(entity),
                    .primitive = preview_primitive_for_entity(entity),
                    .selected = marker_selected_for_entity(entity, selected),
                    .editorOnly = entity.editorOnly || entity.category == "Editor" || is_forest_factory_entity(entity)
                });
            }

            epochnamespace::previewgrid::set_object_markers(ctx, std::move(markers));
        }

        void duplicate_selected_entity(EditorState& state)
        {
            if (state.entities.empty())
            {
                push_editor_log(state, "[entity] Nothing selected to duplicate.");
                return;
            }

            const std::size_t selectedIndex = (std::min)(state.selectedEntity, state.entities.size() - 1u);
            EditorEntity duplicate = state.entities[selectedIndex];
            duplicate.name = make_entity_name(state, duplicate.name + "_Copy");
            duplicate.position[0] += 0.85f;
            duplicate.position[2] -= 0.55f;
            state.entities.push_back(std::move(duplicate));
            state.selectedEntity = state.entities.size() - 1u;
            push_editor_log(state, std::string("[entity] Duplicated ") + state.entities[selectedIndex].name + ".");
        }

        void delete_selected_entity(EditorState& state)
        {
            if (state.entities.empty())
            {
                push_editor_log(state, "[entity] Nothing selected to delete.");
                return;
            }

            const std::size_t selectedIndex = (std::min)(state.selectedEntity, state.entities.size() - 1u);
            const std::string name = state.entities[selectedIndex].name;
            state.entities.erase(state.entities.begin() + static_cast<std::ptrdiff_t>(selectedIndex));

            if (state.entities.empty())
                state.selectedEntity = 0u;
            else if (selectedIndex >= state.entities.size())
                state.selectedEntity = state.entities.size() - 1u;
            else
                state.selectedEntity = selectedIndex;

            push_editor_log(state, std::string("[entity] Deleted ") + name + ".");
        }

        [[nodiscard]] bool project_editor_entity_to_screen(
            const core::Context* ctx,
            const EditorEntity& entity,
            const gui::WidgetBounds& viewport,
            gui::Vec2& out) noexcept
        {
            if (!ctx || viewport.size.x <= 1.0f || viewport.size.y <= 1.0f)
                return false;

            const auto camera = epochnamespace::previewgrid::camera_for(ctx);
            const float aspect = viewport.size.x / viewport.size.y;
            const auto projection = epochnamespace::previewgrid::projection_for(ctx, aspect, camera);
            const auto view = epochnamespace::previewgrid::look_at(
                camera.eye,
                camera.target,
                camera.up);
            const auto mvp = epochnamespace::previewgrid::multiply(projection, view);
            const auto clip = epochnamespace::previewgrid::transform_point(
                mvp,
                epochnamespace::previewgrid::Vec3{
                    entity.position[0],
                    entity.position[1],
                    entity.position[2]
                });

            if (clip.w <= 1.0e-4f)
                return false;

            const float invW = 1.0f / clip.w;
            const float ndcX = clip.x * invW;
            const float ndcY = clip.y * invW;
            if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
                return false;

            // Keep slightly off-center objects selectable while the preview matures.
            if (ndcX < -1.35f || ndcX > 1.35f || ndcY < -1.35f || ndcY > 1.35f)
                return false;

            out.x = viewport.position.x + ((ndcX * 0.5f) + 0.5f) * viewport.size.x;
            out.y = viewport.position.y + ((-ndcY * 0.5f) + 0.5f) * viewport.size.y;
            return true;
        }

        [[nodiscard]] std::optional<epochnamespace::previewgrid::Vec3> screen_point_to_world_plane(
            const core::Context* ctx,
            const gui::WidgetBounds& viewport,
            const gui::Vec2& mouse,
            float planeY) noexcept
        {
            if (!ctx || viewport.size.x <= 1.0f || viewport.size.y <= 1.0f)
                return std::nullopt;

            const float localX = (mouse.x - viewport.position.x) / viewport.size.x;
            const float localY = (mouse.y - viewport.position.y) / viewport.size.y;
            if (!std::isfinite(localX) || !std::isfinite(localY))
                return std::nullopt;

            const float ndcX = (localX * 2.0f) - 1.0f;
            const float ndcY = 1.0f - (localY * 2.0f);
            const float aspect = viewport.size.x / viewport.size.y;

            const auto camera = epochnamespace::previewgrid::camera_for(ctx);
            auto forward = epochnamespace::previewgrid::normalize(
                epochnamespace::previewgrid::subtract(camera.target, camera.eye));
            if (epochnamespace::previewgrid::dot(forward, forward) <= 1.0e-6f)
                forward = { 0.0f, -0.35f, -1.0f };

            auto right = epochnamespace::previewgrid::normalize(
                epochnamespace::previewgrid::cross(forward, camera.up));
            if (epochnamespace::previewgrid::dot(right, right) <= 1.0e-6f)
                right = { 1.0f, 0.0f, 0.0f };

            auto up = epochnamespace::previewgrid::normalize(
                epochnamespace::previewgrid::cross(right, forward));
            if (epochnamespace::previewgrid::dot(up, up) <= 1.0e-6f)
                up = { 0.0f, 1.0f, 0.0f };

            const float tanHalfFov = std::tan(camera.fovRadians * 0.5f);
            auto ray = epochnamespace::previewgrid::add(
                forward,
                epochnamespace::previewgrid::add(
                    epochnamespace::previewgrid::scale(right, ndcX * aspect * tanHalfFov),
                    epochnamespace::previewgrid::scale(up, ndcY * tanHalfFov)));
            ray = epochnamespace::previewgrid::normalize(ray);
            if (!std::isfinite(ray.x) || !std::isfinite(ray.y) || !std::isfinite(ray.z))
                return std::nullopt;
            if (std::abs(ray.y) <= 1.0e-4f)
                return std::nullopt;

            const float hitDistance = (planeY - camera.eye.y) / ray.y;
            if (!std::isfinite(hitDistance) || hitDistance <= 0.0f)
                return std::nullopt;

            return epochnamespace::previewgrid::add(
                camera.eye,
                epochnamespace::previewgrid::scale(ray, hitDistance));
        }

        [[nodiscard]] std::optional<std::size_t> pick_editor_scene_entity(
            const core::Context* ctx,
            const EditorState& state,
            const gui::WidgetBounds& viewport,
            const gui::Vec2& mouse) noexcept
        {
            std::optional<std::size_t> best{};
            float bestDistanceSq = 1.0e12f;

            for (std::size_t i = 0; i < state.entities.size(); ++i)
            {
                const auto& entity = state.entities[i];
                if (!entity.visible)
                    continue;

                gui::Vec2 screen{};
                if (!project_editor_entity_to_screen(ctx, entity, viewport, screen))
                    continue;

                const float dx = mouse.x - screen.x;
                const float dy = mouse.y - screen.y;
                const float distanceSq = (dx * dx) + (dy * dy);
                const float pickRadius = (std::clamp)(30.0f + marker_radius_for_entity(entity) * 18.0f, 34.0f, 72.0f);
                const float pickRadiusSq = pickRadius * pickRadius;
                if (distanceSq <= pickRadiusSq && distanceSq < bestDistanceSq)
                {
                    best = i;
                    bestDistanceSq = distanceSq;
                }
            }

            return best;
        }

        void update_scene_object_interaction(
            const std::shared_ptr<core::Context>& ctx,
            EditorState& editor,
            EditorFrameResult& result)
        {
            if (!ctx || editor.previewMode != core::ScenePreviewMode::Editor)
            {
                editor.sceneDragActive = false;
                editor.sceneDragHasPlaneHit = false;
                editor.sceneLeftWasHeld = false;
                return;
            }

            int mx = 0;
            int my = 0;
            ctx->get_mouse_position_safe(mx, my);
            const gui::Vec2 mouse{
                static_cast<float>(mx),
                static_cast<float>(my)
            };

            const auto& viewport = result.scene_viewport;
            const bool mouseInScene =
                mouse.x >= viewport.position.x
                && mouse.y >= viewport.position.y
                && mouse.x < (viewport.position.x + viewport.size.x)
                && mouse.y < (viewport.position.y + viewport.size.y);

            const bool leftHeld = ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseLeft);
            const bool rightHeld = ctx->is_mouse_button_held_safe(epochnamespace::input::MouseButton::MouseRight);
            const bool leftPressed = leftHeld && !editor.sceneLeftWasHeld;

            if (leftPressed && mouseInScene && !rightHeld)
            {
                const auto picked = pick_editor_scene_entity(ctx.get(), editor, viewport, mouse);
                if (picked)
                {
                    editor.selectedEntity = *picked;
                    editor.sceneDragActive = true;
                    editor.sceneDragEntity = *picked;
                    editor.sceneDragStartMouse = mouse;
                    editor.sceneDragStartPosition = editor.entities[*picked].position;
                    const auto startHit = screen_point_to_world_plane(
                        ctx.get(),
                        viewport,
                        mouse,
                        editor.sceneDragStartPosition[1]);
                    editor.sceneDragHasPlaneHit = startHit.has_value();
                    if (startHit)
                    {
                        editor.sceneDragStartHit = {
                            startHit->x,
                            startHit->y,
                            startHit->z
                        };
                    }
                    result.scene_input_captured = true;
                    push_editor_log(editor, "[scene] Selected " + editor.entities[*picked].name + ".");
                }
                else
                {
                    editor.sceneDragActive = false;
                    editor.sceneDragHasPlaneHit = false;
                }
            }

            if (editor.sceneDragActive && leftHeld && !rightHeld && editor.sceneDragEntity < editor.entities.size())
            {
                result.scene_input_captured = true;

                const float dx = mouse.x - editor.sceneDragStartMouse.x;
                const float dy = mouse.y - editor.sceneDragStartMouse.y;

                auto& entity = editor.entities[editor.sceneDragEntity];
                if (editor.sceneDragHasPlaneHit)
                {
                    const auto currentHit = screen_point_to_world_plane(
                        ctx.get(),
                        viewport,
                        mouse,
                        editor.sceneDragStartPosition[1]);
                    if (currentHit)
                    {
                        entity.position[0] = editor.sceneDragStartPosition[0] + (currentHit->x - editor.sceneDragStartHit[0]);
                        entity.position[2] = editor.sceneDragStartPosition[2] + (currentHit->z - editor.sceneDragStartHit[2]);
                    }
                }
                else
                {
                    const auto camera = epochnamespace::previewgrid::camera_for(ctx.get());
                    auto forward = epochnamespace::previewgrid::normalize(epochnamespace::previewgrid::subtract(camera.target, camera.eye));
                    forward.y = 0.0f;
                    forward = epochnamespace::previewgrid::normalize(forward);
                    if (epochnamespace::previewgrid::dot(forward, forward) <= 1.0e-6f)
                        forward = { 0.0f, 0.0f, -1.0f };

                    constexpr epochnamespace::previewgrid::Vec3 kWorldUp{ 0.0f, 1.0f, 0.0f };
                    auto right = epochnamespace::previewgrid::normalize(epochnamespace::previewgrid::cross(forward, kWorldUp));
                    if (epochnamespace::previewgrid::dot(right, right) <= 1.0e-6f)
                        right = { 1.0f, 0.0f, 0.0f };

                    const float dragScale = (std::clamp)(
                        epochnamespace::previewgrid::camera_distance_for(ctx.get()) * 0.00175f,
                        0.004f,
                        0.045f);
                    const auto delta = epochnamespace::previewgrid::add(
                        epochnamespace::previewgrid::scale(right, dx * dragScale),
                        epochnamespace::previewgrid::scale(forward, -dy * dragScale));
                    entity.position[0] = editor.sceneDragStartPosition[0] + delta.x;
                    entity.position[2] = editor.sceneDragStartPosition[2] + delta.z;
                }
            }

            if (!leftHeld && editor.sceneDragActive)
            {
                if (editor.sceneDragEntity < editor.entities.size())
                    push_editor_log(editor, "[scene] Moved " + editor.entities[editor.sceneDragEntity].name + ".");
                editor.sceneDragActive = false;
                editor.sceneDragHasPlaneHit = false;
            }

            editor.sceneLeftWasHeld = leftHeld;
        }

        [[nodiscard]] std::string build_ai_scene_prompt(const EditorState& state)
        {
            if (state.entities.empty())
                return std::string("Summarize the current Epoch editor scene and suggest one useful next step.");

            const std::size_t selectedIndex = (std::min)(state.selectedEntity, state.entities.size() - 1u);
            const auto& entity = state.entities[selectedIndex];
            const std::string positionText = std::format(
                "({:.1f}, {:.1f}, {:.1f})",
                entity.position[0],
                entity.position[1],
                entity.position[2]);
            return std::format(
                "In Epoch editor, project '{}' has {} entities. Selected entity is '{}' of type '{}' at {}. Suggest one concrete next edit and one gameplay follow-up.",
                state.projectName,
                state.entities.size(),
                entity.name,
                entity.type,
                positionText);
        }

        [[nodiscard]] std::string build_ai_self_iteration_prompt(const EditorState& state)
        {
            return std::format(
                "Plan one safe Epoch self-iteration pass. Active project: '{}' ({}). Project root: '{}'. Active script: '{}'. Project status: '{}'. Build status: '{}'. Keep the plan evidence-gated, editor-visible, and separate from the game scene unless the operator approves a game/editor change.",
                state.projectName,
                state.projectId,
                state.projectRoot,
                state.activeScript,
                state.projectStatus,
                state.projectBuildStatus);
        }

        [[nodiscard]] std::string build_ai_sandbox_scene_training_prompt(const EditorState& state)
        {
            const std::size_t selectedIndex = state.entities.empty()
                ? 0u
                : (std::min)(state.selectedEntity, state.entities.size() - 1u);
            const std::string selected = state.entities.empty()
                ? std::string("(none)")
                : (state.entities[selectedIndex].name + " [" + state.entities[selectedIndex].type + "]");
            return std::format(
                "Create one sandboxed 3D scene-training exercise for OS AI. The selected model must edit or inspect visible primitives in a sandbox scene, produce build/tool/runtime evidence, and report what changed. It must not answer that it is working fine unless it cites concrete evidence paths. Active project: '{}' ({}), selected object: {}, object count: {}, active script: '{}'.",
                state.projectName,
                state.projectId,
                selected,
                state.entities.size(),
                state.activeScript);
        }

        void handle_scene_tool(EditorState& state, std::string_view toolId)
        {
            if (toolId == "focus_selection")
            {
                if (!state.entities.empty())
                {
                    const auto index = (std::min)(state.selectedEntity, state.entities.size() - 1u);
                    push_editor_log(state, std::string("[scene] Focused ") + state.entities[index].name + ".");
                }
                return;
            }

            if (toolId == "reset_camera")
            {
                for (auto& entity : state.entities)
                {
                    if (entity.type == "Camera")
                    {
                        entity.position = { 0.0f, 1.5f, 5.0f };
                        entity.rotation = { 0.0f, 0.0f, 0.0f };
                    }
                }
                push_editor_log(state, "[scene] Camera rigs reset.");
                return;
            }

            if (toolId == "toggle_helpers")
            {
                state.helpersVisible = !state.helpersVisible;
                for (auto& entity : state.entities)
                    if (entity.editorOnly || entity.category == "Editor")
                        entity.visible = state.helpersVisible;
                push_editor_log(state, std::string("[scene] Helpers ") + (state.helpersVisible ? "shown." : "hidden."));
                return;
            }
        }

        std::size_t rotate_script_target_entities(EditorState& state, float deltaDegrees)
        {
            std::size_t rotated = 0;
            for (auto& entity : state.entities)
            {
                if (entity.editorOnly || entity.type == "Level" || entity.type == "Camera")
                    continue;

                entity.rotation[1] += deltaDegrees;
                if (entity.rotation[1] > 180.0f)
                    entity.rotation[1] -= 360.0f;
                if (entity.rotation[1] < -180.0f)
                    entity.rotation[1] += 360.0f;
                ++rotated;
            }
            return rotated;
        }

        [[nodiscard]] std::string editor_tooling_state_summary(const EditorState& state)
        {
            std::size_t mutableEntities = 0;
            float yawSum = 0.0f;
            std::string firstMutable = "(none)";
            for (const auto& entity : state.entities)
            {
                if (entity.editorOnly || entity.type == "Level" || entity.type == "Camera")
                    continue;

                if (mutableEntities == 0)
                    firstMutable = entity.name + " yaw=" + std::format("{:.1f}", entity.rotation[1]);
                yawSum += entity.rotation[1];
                ++mutableEntities;
            }

            return std::format(
                "project={} script={} entities={} mutable={} first_mutable={} yaw_sum={:.1f}",
                state.projectId,
                state.activeScript,
                state.entities.size(),
                mutableEntities,
                firstMutable,
                yawSum);
        }

        void script_log_callback(void* userData, const char* message)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx)
                return;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it == storage.states.end())
                return;

            push_editor_log(it->second, std::string("[script] ") + (message ? message : "(null)"));
        }

        void script_rotate_all_entities_yaw_callback(void* userData, float deltaDegrees)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx)
                return;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it == storage.states.end())
                return;

            const std::size_t rotated = rotate_script_target_entities(it->second, deltaDegrees);
            push_editor_log(
                it->second,
                std::format("[script] Rotated {} scene entities by {:.1f} degrees.", rotated, deltaDegrees));
        }

        int script_queue_model_load_callback(void* userData, const char* debugName, const char* modelPath)
        {
            auto* ctx = static_cast<core::Context*>(userData);
            if (!ctx || !modelPath || modelPath[0] == '\0')
                return -1;
            if (!ctx->add_model)
                return -1;

            const std::string path{ modelPath };
            const std::string label =
                (debugName && debugName[0] != '\0')
                ? std::string(debugName)
                : std::filesystem::path{ path }.stem().string();

            const auto invokeLoad = [ctx, label, path]() noexcept
                {
                    return ctx->add_model_safe(label.c_str(), path.c_str());
                };

            if (auto current = core::get_current_render_context(); current && current.get() == ctx)
                return invokeLoad();

            if (!ctx->windowData)
                return -1;

            const core::RenderPath renderPath =
                (ctx->type == core::ContextType::OpenGL) ? core::RenderPath::OpenGL
                : (ctx->type == core::ContextType::SFML) ? core::RenderPath::SFML
                : (ctx->type == core::ContextType::Vulkan) ? core::RenderPath::Vulkan
                : (ctx->type == core::ContextType::DirectX) ? core::RenderPath::DirectX
                : core::RenderPath::Unknown;

            ctx->windowData->commandQueue.enqueue(
                [ctx, label, path]()
                {
                    (void)ctx->add_model_safe(label.c_str(), path.c_str());
                },
                renderPath);
            return 1;
        }

        [[nodiscard]] bool is_engine_arcade_scene_id(std::string_view sceneId) noexcept
        {
            constexpr std::array<std::string_view, 11> kSceneIds{{
                "snake",
                "tetris",
                "pacman",
                "frogger",
                "sokoban",
                "bejeweled",
                "match3",
                "puzzle",
                "sliding",
                "minesweep",
                "minesweeper"
            }};

            if (sceneId == "fourty" || sceneId == "2048" || sceneId == "sandsim"
                || sceneId == "sand" || sceneId == "cellular" || sceneId == "cell")
            {
                return true;
            }

            return std::find(kSceneIds.begin(), kSceneIds.end(), sceneId) != kSceneIds.end();
        }

        int script_request_engine_scene_callback(void* userData, const char* sceneId)
        {
            const auto* ctx = static_cast<const core::Context*>(userData);
            if (!ctx || !sceneId || sceneId[0] == '\0')
                return -1;

            const std::string requestedScene{ sceneId };
            if (!is_engine_arcade_scene_id(requestedScene))
                return -1;

            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it == storage.states.end())
                return -1;

            it->second.activeRuntimeScene = requestedScene;
            it->second.projectStatus = std::string("Script selected built-in engine scene '") + requestedScene + "' for the Run button.";
            push_editor_log(
                it->second,
                std::string("[script] Built-in engine scene selected: ") + requestedScene + ".");
            push_editor_log(
                it->second,
                "[script] Press the centered Run button to launch it inside the engine runtime.");
            return 1;
        }

        [[nodiscard]] std::string renderer_name(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "Unknown";

            switch (ctx->type)
            {
            case core::ContextType::OpenGL: return "OpenGL";
            case core::ContextType::Vulkan: return "Vulkan";
            case core::ContextType::RayLib: return "Raylib";
            case core::ContextType::SDL: return "SDL";
            case core::ContextType::SFML: return "SFML";
            case core::ContextType::DirectX: return "DirectX";
            case core::ContextType::Software: return "Software";
            default: return "Unknown";
            }
        }

        [[nodiscard]] epoch::RendererBackendKind renderer_backend_kind(const std::shared_ptr<core::Context>& ctx) noexcept
        {
            if (!ctx)
                return epoch::RendererBackendKind::null;

            switch (ctx->type)
            {
            case core::ContextType::OpenGL: return epoch::RendererBackendKind::opengl;
            case core::ContextType::SDL: return epoch::RendererBackendKind::sdl3;
            case core::ContextType::SFML: return epoch::RendererBackendKind::sfml3;
            case core::ContextType::RayLib: return epoch::RendererBackendKind::raylib3;
            case core::ContextType::Vulkan: return epoch::RendererBackendKind::vulkan;
            case core::ContextType::DirectX: return epoch::RendererBackendKind::directx;
            case core::ContextType::Software: return epoch::RendererBackendKind::software;
            default: return epoch::RendererBackendKind::null;
            }
        }

        [[nodiscard]] std::string renderer_resource_spine_summary(const std::shared_ptr<core::Context>& ctx)
        {
            const auto kind = renderer_backend_kind(ctx);
            const auto caps = epoch::renderer_capabilities_for(kind);

            if (kind == epoch::RendererBackendKind::software)
                return "Software remains debug/safe-launch fallback; production renderer parity excludes it.";

            std::string summary;
            const auto append = [&summary](bool enabled, std::string_view name)
            {
                if (!enabled)
                    return;

                if (!summary.empty())
                    summary += " | ";
                summary += name;
            };

            append(caps.buffers, "buffers");
            append(caps.textures, "textures");
            append(caps.samplers, "samplers");
            append(caps.shaders, "shaders");
            append(caps.pipelines, "pipelines");
            append(caps.materials, "materials");
            append(caps.render_targets, "targets");
            append(caps.command_lists, "commands");
            append(caps.frame_graph, "graph");
            append(caps.render_to_texture, "RTT");
            append(caps.sampled_render_targets, "sampled targets");
            append(caps.binding_sets, "binding sets");
            append(caps.mesh_resources, "meshes");
            append(caps.model_resources, "models");

            if (summary.empty())
                return "No renderer-resource spine is active for this backend yet.";

            return summary;
        }

        [[nodiscard]] std::string renderer_declared_descriptor_status()
        {
            return "buffers | textures | materials | targets | command lists | mesh descriptors | model descriptors";
        }

        [[nodiscard]] std::string renderer_native_mesh_model_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto kind = renderer_backend_kind(ctx);
            if (kind == epoch::RendererBackendKind::software)
                return "debug fallback; production native mesh/model allocation is out of scope";

            const auto caps = epoch::renderer_capabilities_for(kind);
            const bool meshNative = epoch::renderer_supports_mesh_resources(caps);
            const bool modelNative = epoch::renderer_supports_model_resources(caps);
            if (meshNative && modelNative)
                return "mesh/model native allocation active";
            if (meshNative)
                return "mesh native, model allocation pending";
            if (modelNative)
                return "model native, mesh allocation pending";
            return "descriptors compile through graph; backend-native allocation pending";
        }

        [[nodiscard]] std::string renderer_native_sampled_rtt_status(const std::shared_ptr<core::Context>& ctx)
        {
            const auto kind = renderer_backend_kind(ctx);
            if (kind == epoch::RendererBackendKind::software)
                return "debug fallback; production sampled RTT allocation is out of scope";

            const auto caps = epoch::renderer_capabilities_for(kind);
            if (epoch::renderer_supports_native_sampled_render_targets(caps))
                return "native sampled RTT allocation active";
            if (epoch::renderer_supports_sampled_render_targets(caps))
                return "sampled RTT graph declared; backend-native allocation pending";
            return "sampled RTT unavailable";
        }

        [[nodiscard]] std::string renderer_next_feature_gate(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "Select a renderer before promoting backend features.";

            if (renderer_backend_kind(ctx) == epoch::RendererBackendKind::software)
                return "Keep software as fallback; prove the six production contexts through the shared spine.";

            return "Backend-native allocation behind handles, then materials, model import, normal maps, skybox, instancing, shadows, RTT, G-buffer, SSAO.";
        }

        struct ProjectRunBackendChoice
        {
            std::string_view label{};
            std::string_view argument{};
        };

        [[nodiscard]] std::span<const ProjectRunBackendChoice> project_run_backend_choices() noexcept
        {
#if defined(_WIN32)
            static constexpr std::array<ProjectRunBackendChoice, 6> kChoices{ {
                { "OpenGL single context", "opengl" },
                { "DirectX single context", "directx" },
                { "Vulkan single context", "vulkan" },
                { "Raylib single context", "raylib" },
                { "SDL single context", "sdl" },
                { "SFML single context", "sfml" }
            } };
#elif defined(__linux__)
            static constexpr std::array<ProjectRunBackendChoice, 1> kChoices{ {
                { "OpenGL single context", "opengl" }
            } };
#else
            static constexpr std::array<ProjectRunBackendChoice, 5> kChoices{ {
                { "OpenGL single context", "opengl" },
                { "Vulkan single context", "vulkan" },
                { "Raylib single context", "raylib" },
                { "SDL single context", "sdl" },
                { "SFML single context", "sfml" }
            } };
#endif
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view project_run_backend_label(std::string_view argument) noexcept
        {
            for (const auto& choice : project_run_backend_choices())
            {
                if (choice.argument == argument)
                    return choice.label;
            }
            return "OpenGL single context";
        }

        struct FrameLimitChoice
        {
            std::string_view label{};
            std::string_view argument{};
            double fps{};
        };

        struct ProjectCameraChoice
        {
            std::string_view label{};
            previewgrid::CameraMode mode{ previewgrid::CameraMode::Editor };
        };

        struct InputProfileChoice
        {
            std::string_view label{};
            input::ProfilePreset preset{ input::ProfilePreset::EditorDefault };
        };

        [[nodiscard]] std::span<const FrameLimitChoice> frame_limit_choices() noexcept
        {
            static constexpr std::array<FrameLimitChoice, 3> kChoices{ {
                { "60 FPS", "60", 60.0 },
                { "120 FPS", "120", 120.0 },
                { "Unlimited", "unlimited", 0.0 }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view frame_limit_label(double fps) noexcept
        {
            return epoch::perf::label_for_frame_limit(fps);
        }

        [[nodiscard]] std::string_view frame_limit_argument(double fps) noexcept
        {
            for (const auto& choice : frame_limit_choices())
            {
                if (choice.fps == fps)
                    return choice.argument;
            }
            return "120";
        }

        [[nodiscard]] std::span<const ProjectCameraChoice> project_camera_choices() noexcept
        {
            static constexpr std::array<ProjectCameraChoice, 3> kChoices{ {
                { "Editor orbit camera", previewgrid::CameraMode::Editor },
                { "First-person runtime camera", previewgrid::CameraMode::FPS },
                { "Locked 2D canvas camera", previewgrid::CameraMode::Canvas2D }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::span<const InputProfileChoice> input_profile_choices() noexcept
        {
            static constexpr std::array<InputProfileChoice, 4> kChoices{ {
                { "Editor Default", input::ProfilePreset::EditorDefault },
                { "Runtime WASD", input::ProfilePreset::RuntimeWASD },
                { "Arrow Pilot", input::ProfilePreset::ArrowPilot },
                { "Left-Handed IJKL", input::ProfilePreset::LeftHanded }
            } };
            return { kChoices.data(), kChoices.size() };
        }

        [[nodiscard]] std::string_view input_profile_label(input::ProfilePreset preset) noexcept
        {
            return input::profile_preset_label(preset);
        }

        [[nodiscard]] std::string_view input_profile_argument(input::ProfilePreset preset) noexcept
        {
            return input::profile_preset_id(preset);
        }

        [[nodiscard]] std::string_view project_camera_label(previewgrid::CameraMode mode) noexcept
        {
            for (const auto& choice : project_camera_choices())
            {
                if (choice.mode == mode)
                    return choice.label;
            }
            return "Editor orbit camera";
        }

        [[nodiscard]] std::string_view project_camera_argument(previewgrid::CameraMode mode) noexcept
        {
            switch (mode)
            {
            case previewgrid::CameraMode::FPS:
                return "fps";
            case previewgrid::CameraMode::Canvas2D:
                return "canvas2d";
            case previewgrid::CameraMode::Editor:
            default:
                return "editor";
            }
        }

        [[nodiscard]] std::string vec3_text(const std::array<float, 3>& value)
        {
            return std::format("({:.1f}, {:.1f}, {:.1f})", value[0], value[1], value[2]);
        }

        [[nodiscard]] std::string ellipsize(std::string_view text, std::size_t max_chars)
        {
            if (text.size() <= max_chars)
                return std::string(text);

            if (max_chars <= 3)
                return std::string(text.substr(0, max_chars));

            return std::string(text.substr(0, max_chars - 3)) + "...";
        }

        [[nodiscard]] std::string_view preview_mode_name(core::ScenePreviewMode mode) noexcept
        {
            switch (mode)
            {
            case core::ScenePreviewMode::Editor: return "Editor";
            case core::ScenePreviewMode::None: return "None";
            default: return "Editor";
            }
        }

        [[nodiscard]] std::string preview_camera_name(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "Editor";
            return std::string(epochnamespace::previewgrid::camera_mode_name(
                epochnamespace::previewgrid::camera_mode_for(ctx.get())));
        }

        [[nodiscard]] std::string preview_zoom_text(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "13.5";

            return std::format("{:.1f}", epochnamespace::previewgrid::camera_distance_for(ctx.get()));
        }

        [[nodiscard]] std::string backend_ownership_model(const std::shared_ptr<core::Context>& ctx)
        {
            if (!ctx)
                return "No active backend context.";

            switch (ctx->type)
            {
            case core::ContextType::SDL:
                return "Visible proxy shell owns the dock slot; SDL_app stays nested inside it during undock/redock.";
            case core::ContextType::SFML:
                return "Visible proxy shell owns the dock slot; SFML_Window stays nested inside it during undock/redock.";
            case core::ContextType::RayLib:
                return "Real child pane with a parked helper host; the child surface is the visible docked backend.";
            case core::ContextType::OpenGL:
            case core::ContextType::Vulkan:
            case core::ContextType::DirectX:
            case core::ContextType::Software:
                return "Direct child/editor-owned pane with no proxy-shell handoff in normal docked use.";
            default:
                return "Backend ownership model not classified yet.";
            }
        }

        [[nodiscard]] constexpr std::string_view backend_lifecycle_policy() noexcept
        {
            return "Switch deliberately; inactive backends should be torn down and recreated, not parked invisibly.";
        }

        [[nodiscard]] std::filesystem::path editor_runtime_root()
        {
            if (const auto runtimeRoot = epoch::core::path::runtime_root_dir(); !runtimeRoot.empty())
                return runtimeRoot.lexically_normal();

            if (const auto exeRoot = epoch::core::path::find_epoch_repo_root(epoch::core::path::executable_dir()); !exeRoot.empty())
                return exeRoot.lexically_normal();

            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            return ec ? std::filesystem::path{} : cwd.lexically_normal();
        }

        [[nodiscard]] std::filesystem::path resolve_editor_path(const std::filesystem::path& path)
        {
            if (path.empty())
                return {};
            if (path.is_absolute())
                return path.lexically_normal();
            return (editor_runtime_root() / path).lexically_normal();
        }

        [[nodiscard]] std::filesystem::path active_editor_scene_path(const EditorState& state)
        {
            if (state.projectScenePath.empty())
                return {};
            return resolve_editor_path(std::filesystem::path{ state.projectScenePath });
        }

        void save_editor_scene_snapshot(const EditorState& state)
        {
            const auto scenePath = active_editor_scene_path(state);
            if (scenePath.empty())
                return;

            std::error_code ec;
            std::filesystem::create_directories(scenePath.parent_path(), ec);
            if (ec)
                return;

            std::ofstream out(scenePath, std::ios::binary | std::ios::trunc);
            if (!out)
                return;

            out << std::setprecision(6);
            out << "scene " << std::quoted(state.activeWorld) << "\n";
            out << "project " << std::quoted(state.projectId) << "\n";
            out << "epoch_editor_entities 1\n";
            for (const auto& entity : state.entities)
            {
                out << "entity "
                    << std::quoted(entity.name) << ' '
                    << std::quoted(entity.type) << ' '
                    << std::quoted(entity.category) << ' '
                    << "pos " << entity.position[0] << ' ' << entity.position[1] << ' ' << entity.position[2] << ' '
                    << "rot " << entity.rotation[0] << ' ' << entity.rotation[1] << ' ' << entity.rotation[2] << ' '
                    << "scale " << entity.scale[0] << ' ' << entity.scale[1] << ' ' << entity.scale[2] << ' '
                    << "visible " << (entity.visible ? 1 : 0) << ' '
                    << "editor_only " << (entity.editorOnly ? 1 : 0)
                    << "\n";
            }
        }

        [[nodiscard]] bool load_editor_scene_snapshot(EditorState& state)
        {
            const auto scenePath = active_editor_scene_path(state);
            if (scenePath.empty())
                return false;

            std::ifstream in(scenePath, std::ios::binary);
            if (!in)
                return false;

            std::vector<EditorEntity> loaded;
            std::string line;
            while (std::getline(in, line))
            {
                std::istringstream row(line);
                std::string tag;
                row >> tag;
                if (tag != "entity")
                    continue;

                EditorEntity entity{};
                std::string posTag;
                std::string rotTag;
                std::string scaleTag;
                std::string visibleTag;
                std::string editorOnlyTag;
                int visible = 1;
                int editorOnly = 0;
                if (!(row
                    >> std::quoted(entity.name)
                    >> std::quoted(entity.type)
                    >> std::quoted(entity.category)
                    >> posTag >> entity.position[0] >> entity.position[1] >> entity.position[2]
                    >> rotTag >> entity.rotation[0] >> entity.rotation[1] >> entity.rotation[2]
                    >> scaleTag >> entity.scale[0] >> entity.scale[1] >> entity.scale[2]
                    >> visibleTag >> visible
                    >> editorOnlyTag >> editorOnly))
                {
                    continue;
                }

                if (posTag != "pos" || rotTag != "rot" || scaleTag != "scale" || visibleTag != "visible" || editorOnlyTag != "editor_only")
                    continue;

                entity.visible = visible != 0;
                entity.editorOnly = editorOnly != 0;
                loaded.push_back(std::move(entity));
            }

            if (loaded.empty())
                return false;

            state.entities = std::move(loaded);
            state.selectedEntity = (std::min)(state.selectedEntity, state.entities.size() - 1u);
            return true;
        }

        [[nodiscard]] std::filesystem::path project_entry_source_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "source" / "main.cpp";
        }

        [[nodiscard]] std::filesystem::path project_windows_build_script_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "build_project.ps1";
        }

        [[nodiscard]] std::string project_artifact_stem(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            const std::string stem = root.filename().string();
            return stem == "Sandbox" ? std::string{ "EpochEngine" } : stem;
        }

        [[nodiscard]] std::filesystem::path project_windows_vcxproj_path(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            return root / (project_artifact_stem(projectRoot) + ".vcxproj");
        }

        [[nodiscard]] std::filesystem::path project_output_exe_path(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            return root / "bin" / "windows" / "Debug" / "x64" / (project_artifact_stem(projectRoot) + ".exe");
        }

        [[nodiscard]] std::vector<std::filesystem::path> project_output_exe_candidates(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            const std::filesystem::path outputDir = root / "bin" / "windows" / "Debug" / "x64";
            std::vector<std::filesystem::path> candidates;
            const auto add_candidate = [&](std::string name) {
                if (name.empty())
                    return;
                if (!name.ends_with(".exe"))
                    name += ".exe";
                const auto path = outputDir / name;
                for (const auto& candidate : candidates)
                {
                    if (candidate == path)
                        return;
                }
                candidates.push_back(path);
            };

            add_candidate(project_artifact_stem(projectRoot));
            add_candidate(root.filename().string());
            add_candidate("EpochEngine");
            add_candidate("EpochEditor");
            add_candidate("ConsoleApplication1");
            return candidates;
        }

        [[nodiscard]] std::filesystem::path project_existing_output_exe_path(std::string_view projectRoot)
        {
            std::error_code ec;
            for (const auto& candidate : project_output_exe_candidates(projectRoot))
            {
                if (std::filesystem::exists(candidate, ec) && !ec)
                    return candidate;
                ec.clear();
            }
            return project_output_exe_path(projectRoot);
        }

        [[nodiscard]] std::filesystem::path project_build_log_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "build" / "logs" / "build-debug-x64.log";
        }

        [[nodiscard]] std::filesystem::path project_notes_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "PROJECT_NOTES.md";
        }

        [[nodiscard]] std::string read_project_notes(std::string_view projectRoot)
        {
            if (projectRoot.empty())
                return "No active project notes yet.";

            const auto notesPath = project_notes_path(projectRoot);
            std::ifstream in(notesPath, std::ios::binary);
            if (!in)
                return "No project notes have been written yet. Build, run, script, and AI sandbox actions will append notes here.";

            std::ostringstream buffer;
            buffer << in.rdbuf();
            std::string text = buffer.str();
            if (text.empty())
                return "Project notes file exists but is empty.";
            return normalize_editor_text_for_gui(text);
        }

        [[nodiscard]] std::string tail_text(std::string text, std::size_t maxChars)
        {
            if (text.size() <= maxChars)
                return text;
            return std::string("...") + text.substr(text.size() - maxChars);
        }

        void append_project_note(
            const EditorState& editor,
            std::string_view action,
            std::string_view summary,
            std::string_view usage)
        {
            if (editor.projectRoot.empty())
                return;

            const auto notesPath = project_notes_path(editor.projectRoot);
            std::error_code ec;
            std::filesystem::create_directories(notesPath.parent_path(), ec);

            const bool hadNotes = std::filesystem::exists(notesPath, ec)
                && std::filesystem::file_size(notesPath, ec) > 0u;
            std::ofstream out(notesPath, std::ios::app | std::ios::binary);
            if (!out)
                return;

            if (!hadNotes)
            {
                out << "# Epoch Project Notes\n\n";
                out << "These notes are generated by the editor so ProjectLauncher actions, script runs, and AI sandbox passes leave visible operator evidence.\n";
            }

            out << "\n## " << action << "\n\n";
            out << "- Project: " << editor.projectName << " (" << editor.projectId << ")\n";
            out << "- Summary: " << summary << "\n";
            if (!usage.empty())
                out << "- Usage: " << usage << "\n";
            out << "- Manifest: " << editor.projectManifest << "\n";
            out << "- Scene: " << editor.projectScenePath << "\n";
            out << "- Active script: " << editor.activeScript << "\n";
            out << "- Build log: " << resolve_editor_path(project_build_log_path(editor.projectRoot)).generic_string() << "\n";
        }

        [[nodiscard]] std::string ai_build_path_stamp(const std::filesystem::path& path)
        {
            if (path.empty())
                return "(empty)#missing";

            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            const auto normalized = std::filesystem::absolute(resolvedPath, ec).lexically_normal().generic_string();
            ec.clear();
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
                return normalized + "#missing";

            const auto writeTime = std::filesystem::last_write_time(resolvedPath, ec);
            if (ec)
                return normalized + "#time-error";

            std::string stamp = normalized + "#" + std::to_string(writeTime.time_since_epoch().count());
            ec.clear();
            if (std::filesystem::is_regular_file(resolvedPath, ec) && !ec)
            {
                ec.clear();
                const auto size = std::filesystem::file_size(resolvedPath, ec);
                if (!ec)
                    stamp += "#" + std::to_string(size);
            }
            return stamp;
        }

        [[nodiscard]] std::string ai_continuous_build_fingerprint(
            const EditorState& editor,
            const std::string& activeScriptSource,
            const std::filesystem::path& pathsManifest)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            std::string fingerprint = editor.projectId + "|" + editor.projectRoot + "|" + editor.activeScript;
            fingerprint += "|" + ai_build_path_stamp(project_entry_source_path(editor.projectRoot));
            fingerprint += "|" + ai_build_path_stamp(activeScriptSource);
            fingerprint += "|" + ai_build_path_stamp(editor.projectManifest);
            fingerprint += "|" + ai_build_path_stamp(pathsManifest);
            fingerprint += "|" + ai_build_path_stamp(project_windows_build_script_path(editor.projectRoot));
            fingerprint += "|" + ai_build_path_stamp(root / "scripts");
            return fingerprint;
        }

        void start_ai_continuous_project_build(
            EditorState& editor,
            const std::string& activeScriptSource,
            const std::filesystem::path& pathsManifest,
            std::string_view reason,
            bool force)
        {
            if (editor.aiContinuousBuildPending)
            {
                editor.aiContinuousBuildStatus = "Self-iteration build already running.";
                return;
            }

            if (editor.projectRoot.empty())
            {
                editor.aiContinuousBuildStatus = "Cannot build: no active project root.";
                return;
            }

            const std::string fingerprint = ai_continuous_build_fingerprint(editor, activeScriptSource, pathsManifest);
            if (!force && fingerprint == editor.aiContinuousBuildFingerprint)
            {
                editor.aiContinuousBuildStatus = "Evidence is unchanged; manual build gate remains idle.";
                return;
            }

            editor.aiContinuousBuildFingerprint = fingerprint;
            editor.aiContinuousBuildStatus = "Queued build: " + std::string(reason);
            push_editor_log(editor, "[ai-build] Queued self-iteration build: " + std::string(reason));

            editor.aiContinuousBuildPending.emplace(std::async(std::launch::async, [root = editor.projectRoot]() {
                epoch::systems::threading::ScopedThreadActivity threadActivity{};
                return editor_build_project(root);
            }));
        }

        [[nodiscard]] std::string display_project_path(const std::filesystem::path& path)
        {
            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            return std::filesystem::absolute(resolvedPath, ec).lexically_normal().generic_string();
        }

        bool load_script_source_editor(EditorState& editor, const std::filesystem::path& path, bool force = false)
        {
            if (path.empty())
            {
                editor.scriptEditorStatus = "No script source selected.";
                return false;
            }

            const auto resolvedPath = resolve_editor_path(path);
            const std::string normalizedPath = display_project_path(resolvedPath);
            if (!force && editor.scriptEditorDirty && editor.scriptEditorPath != normalizedPath)
            {
                editor.scriptEditorStatus = "Unsaved script edits; save or reload before switching source files.";
                return false;
            }

            std::error_code ec;
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
            {
                editor.scriptEditorPath = normalizedPath;
                editor.scriptEditorText.clear();
                editor.scriptEditorDirty = false;
                editor.scriptEditorStatus = "Script source is missing.";
                return false;
            }

            ec.clear();
            const auto size = std::filesystem::file_size(resolvedPath, ec);
            if (ec)
            {
                editor.scriptEditorStatus = "Could not read script source size.";
                return false;
            }

            constexpr std::uintmax_t kMaxEditableScriptBytes = 256u * 1024u;
            if (size > kMaxEditableScriptBytes)
            {
                editor.scriptEditorStatus = "Script source is too large for the current editor surface.";
                return false;
            }

            std::ifstream in(resolvedPath, std::ios::binary);
            if (!in)
            {
                editor.scriptEditorStatus = "Could not open script source for reading.";
                return false;
            }

            std::ostringstream text;
            text << in.rdbuf();
            editor.scriptEditorPath = normalizedPath;
            editor.scriptEditorText = text.str();
            editor.scriptEditorDirty = false;
            editor.scriptEditorStatus = "Loaded script source.";
            return true;
        }

        bool save_script_source_editor(EditorState& editor)
        {
            if (editor.scriptEditorPath.empty())
            {
                editor.scriptEditorStatus = "No script source loaded.";
                return false;
            }

            const auto resolvedPath = resolve_editor_path(std::filesystem::path{ editor.scriptEditorPath });
            std::error_code ec;
            std::filesystem::create_directories(resolvedPath.parent_path(), ec);
            if (ec)
            {
                editor.scriptEditorStatus = "Could not create script source directory.";
                return false;
            }

            std::ofstream out(resolvedPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                editor.scriptEditorStatus = "Could not open script source for writing.";
                return false;
            }

            out << editor.scriptEditorText;
            editor.scriptEditorDirty = false;
            editor.scriptEditorStatus = "Saved script source.";
            return true;
        }

        void sync_script_source_editor(EditorState& editor, const std::filesystem::path& path)
        {
            if (path.empty())
                return;

            const std::string normalizedPath = display_project_path(path);
            if (editor.scriptEditorPath.empty() || (!editor.scriptEditorDirty && editor.scriptEditorPath != normalizedPath))
                (void)load_script_source_editor(editor, path, false);
        }

        void draw_script_source_editor(EditorState& editor, const std::filesystem::path& sourcePath, float width, float height)
        {
            sync_script_source_editor(editor, sourcePath);

            gui::label("Script Source Editor");
            gui::property_row("[script editor] Path", editor.scriptEditorPath.empty() ? display_project_path(sourcePath) : editor.scriptEditorPath, 130.0f);
            gui::property_row(
                "[script editor] State",
                editor.scriptEditorDirty ? std::string("Modified; save before build/run.") : editor.scriptEditorStatus,
                130.0f);

            const auto sourceEditor = gui::source_editor(
                editor.scriptEditorText,
                gui::SourceEditorOptions{
                    .id = "script-source-editor",
                    .size = { (std::max)(180.0f, width), (std::max)(180.0f, height) },
                    .max_chars = 256u * 1024u,
                    .show_context_menu = true
                });
            if (sourceEditor.edit.changed)
            {
                editor.scriptEditorDirty = true;
                editor.scriptEditorStatus = "Modified.";
            }
            if (sourceEditor.selected_all)
                editor.scriptEditorStatus = "Selected script source.";
            if (sourceEditor.copied)
            {
                editor.scriptEditorStatus = "Copied script source to clipboard.";
                push_editor_log(editor, "[script] Copied source to clipboard.");
            }
            if (sourceEditor.cut)
            {
                editor.scriptEditorDirty = true;
                editor.scriptEditorStatus = "Cut script source to clipboard.";
                push_editor_log(editor, "[script] Cut source to clipboard.");
            }
            if (sourceEditor.pasted)
            {
                editor.scriptEditorDirty = true;
                editor.scriptEditorStatus = "Pasted clipboard text into script source.";
                push_editor_log(editor, "[script] Pasted clipboard text into source editor.");
            }

            gui::wrapped_label("Right-click the source editor for Select All, Copy, Cut, and Paste. Ctrl+A/C/X/V work while focused.", width);
            std::array<gui::InlineButtonSpec, 2> scriptEditActions{ {
                { "Save", 72.0f },
                { "Reload", 86.0f }
            } };
            if (auto clicked = gui::inline_button_row(scriptEditActions, 28.0f, 8.0f))
            {
                if (*clicked == 0)
                {
                    if (save_script_source_editor(editor))
                    {
                        push_editor_log(editor, "[script] Saved source: " + editor.scriptEditorPath);
                        append_project_note(
                            editor,
                            "Save Script Source",
                            "Saved script source from the editor surface.",
                            editor.scriptEditorPath);
                    }
                    else
                    {
                        push_editor_log(editor, "[script] Save failed: " + editor.scriptEditorStatus);
                    }
                }
                else if (*clicked == 1)
                {
                    if (load_script_source_editor(editor, sourcePath, true))
                        push_editor_log(editor, "[script] Reloaded source: " + editor.scriptEditorPath);
                    else
                        push_editor_log(editor, "[script] Reload failed: " + editor.scriptEditorStatus);
                }
            }
        }

        [[nodiscard]] std::string read_text_tail(const std::filesystem::path& path, std::size_t maxChars = 2400)
        {
            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
                return "(missing)";

            std::ifstream in(resolvedPath, std::ios::binary);
            if (!in)
                return "(unreadable)";

            in.seekg(0, std::ios::end);
            const auto end = in.tellg();
            const auto endOffset = static_cast<std::streamoff>(end);
            if (endOffset <= 0)
                return "(empty)";

            const auto size = static_cast<std::size_t>(endOffset);
            const std::size_t readSize = (std::min)(size, maxChars);
            in.seekg(static_cast<std::streamoff>(size - readSize), std::ios::beg);
            std::string text(readSize, '\0');
            in.read(text.data(), static_cast<std::streamsize>(text.size()));
            return text;
        }

        [[nodiscard]] std::string file_ready_summary(const std::filesystem::path& path)
        {
            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            if (!std::filesystem::exists(resolvedPath, ec) || ec)
                return "missing";

            if (std::filesystem::is_regular_file(resolvedPath, ec) && !ec)
            {
                ec.clear();
                const auto size = std::filesystem::file_size(resolvedPath, ec);
                if (!ec)
                    return std::format("ready ({} bytes)", size);
            }

            return "ready";
        }

        struct EditorBrowserEntry
        {
            std::string label{};
            std::string path{};
            std::string kind{};
            bool directory{ false };
            std::uintmax_t size{ 0 };
        };

        [[nodiscard]] static bool should_skip_browser_directory(std::string_view name) noexcept
        {
            return name == ".git"
                || name == ".vs"
                || name == "build"
                || name == "bin"
                || name == "Debug"
                || name == "Release"
                || name == "x64"
                || name == "vcpkg_installed";
        }

        [[nodiscard]] static std::string sanitize_script_id(std::string_view value)
        {
            std::string result;
            result.reserve(value.size());
            bool previousUnderscore = false;
            for (const unsigned char raw : value)
            {
                if (std::isalnum(raw))
                {
                    result.push_back(static_cast<char>(std::tolower(raw)));
                    previousUnderscore = false;
                }
                else if ((raw == '_' || raw == '-' || raw == ' ' || raw == '.') && !previousUnderscore && !result.empty())
                {
                    result.push_back('_');
                    previousUnderscore = true;
                }
            }

            while (!result.empty() && result.back() == '_')
                result.pop_back();
            if (result.empty())
                result = "sandbox_iteration";
            return result;
        }

        [[nodiscard]] static std::string script_id_from_source_path(const std::filesystem::path& path)
        {
            std::string fileName = path.filename().string();
            constexpr std::string_view suffix = ".ascript.cpp";
            if (fileName.ends_with(suffix))
                fileName.resize(fileName.size() - suffix.size());
            return sanitize_script_id(fileName);
        }

        [[nodiscard]] static std::string relative_browser_path(
            const std::filesystem::path& root,
            const std::filesystem::path& path)
        {
            std::error_code ec;
            const auto relative = std::filesystem::relative(path, root, ec);
            if (!ec && !relative.empty())
                return relative.generic_string();
            return path.filename().generic_string();
        }

        [[nodiscard]] static std::string browser_kind_for_path(const std::filesystem::path& path, bool directory)
        {
            if (directory)
                return "DIR";
            const std::string name = path.filename().string();
            const std::string ext = path.extension().string();
            if (name.ends_with(".ascript.cpp"))
                return "SCRIPT";
            if (ext == ".cpp" || ext == ".hpp" || ext == ".ixx" || ext == ".h")
                return "CODE";
            if (ext == ".epoch" || ext == ".json" || ext == ".txt" || ext == ".md")
                return "TEXT";
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
                return "IMAGE";
            if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx")
                return "MODEL";
            if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
                return "AUDIO";
            return "FILE";
        }

        [[nodiscard]] static EditorBrowserEntry make_browser_entry(
            const std::filesystem::path& root,
            const std::filesystem::path& path,
            bool directory)
        {
            std::error_code ec;
            const std::string kind = browser_kind_for_path(path, directory);
            const std::string relative = relative_browser_path(root, path);
            std::uintmax_t size = 0;
            if (!directory)
            {
                ec.clear();
                size = std::filesystem::file_size(path, ec);
                if (ec)
                    size = 0;
            }

            return EditorBrowserEntry{
                .label = std::string("[") + kind + "] " + relative + (directory ? "/" : ""),
                .path = display_project_path(path),
                .kind = kind,
                .directory = directory,
                .size = size
            };
        }

        [[nodiscard]] static std::vector<EditorBrowserEntry> collect_project_browser_entries(
            std::string_view projectRoot,
            std::size_t maxEntries = 48)
        {
            std::vector<EditorBrowserEntry> entries;
            const auto root = resolve_editor_path(std::filesystem::path{ projectRoot });
            std::error_code ec;
            if (root.empty() || !std::filesystem::exists(root, ec) || ec)
                return entries;

            for (std::filesystem::recursive_directory_iterator it(
                     root,
                     std::filesystem::directory_options::skip_permission_denied,
                     ec),
                 end;
                 it != end && entries.size() < maxEntries;
                 it.increment(ec))
            {
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                const auto path = it->path();
                const bool directory = it->is_directory(ec);
                if (ec)
                {
                    ec.clear();
                    continue;
                }

                if (directory)
                {
                    if (should_skip_browser_directory(path.filename().string()))
                    {
                        it.disable_recursion_pending();
                        continue;
                    }
                    if (it.depth() >= 2)
                        it.disable_recursion_pending();
                }

                if (it.depth() > 2)
                    continue;

                entries.push_back(make_browser_entry(root, path, directory));
            }

            return entries;
        }

        [[nodiscard]] static std::vector<EditorBrowserEntry> collect_script_browser_entries(
            std::string_view projectRoot,
            std::size_t maxEntries = 32)
        {
            std::vector<EditorBrowserEntry> entries;
            const std::array roots{
                resolve_editor_path(std::filesystem::path{ projectRoot }) / "scripts",
                resolve_editor_path(std::filesystem::path{ "Engine/src/scripts" })
            };

            for (const auto& root : roots)
            {
                std::error_code ec;
                if (root.empty() || !std::filesystem::exists(root, ec) || ec)
                    continue;

                for (const auto& entry : std::filesystem::directory_iterator(root, ec))
                {
                    if (ec || entries.size() >= maxEntries)
                    {
                        ec.clear();
                        break;
                    }

                    std::error_code entryEc;
                    if (!entry.is_regular_file(entryEc) || entryEc)
                        continue;

                    const auto path = entry.path();
                    if (!path.filename().string().ends_with(".ascript.cpp"))
                        continue;

                    auto item = make_browser_entry(root, path, false);
                    item.kind = script_id_from_source_path(path);
                    entries.push_back(std::move(item));
                }
            }

            return entries;
        }

        [[nodiscard]] static std::vector<EditorBrowserEntry> collect_asset_browser_entries(
            const EditorState& editor,
            std::size_t maxEntries = 48)
        {
            std::vector<EditorBrowserEntry> entries;
            const auto append_if_ready = [&](const std::filesystem::path& root, const std::filesystem::path& path)
            {
                if (entries.size() >= maxEntries || path.empty())
                    return;
                std::error_code ec;
                const auto resolved = resolve_editor_path(path);
                if (!std::filesystem::exists(resolved, ec) || ec)
                    return;
                entries.push_back(make_browser_entry(root.empty() ? resolved.parent_path() : root, resolved, std::filesystem::is_directory(resolved, ec) && !ec));
            };

            const auto projectRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            append_if_ready(projectRoot, editor.projectScenePath);
            const auto modelSummary = editor_project_model_summary(editor.projectId);
            append_if_ready(projectRoot, modelSummary.resolved_path);

            const std::array roots{
                projectRoot / "assets",
                resolve_editor_path(std::filesystem::path{ "Engine/assets" }),
                resolve_editor_path(std::filesystem::path{ "Engine/examples/ConsoleApplication1/assets" })
            };

            for (const auto& root : roots)
            {
                std::error_code ec;
                if (root.empty() || !std::filesystem::exists(root, ec) || ec)
                    continue;

                for (std::filesystem::recursive_directory_iterator it(
                         root,
                         std::filesystem::directory_options::skip_permission_denied,
                         ec),
                     end;
                     it != end && entries.size() < maxEntries;
                     it.increment(ec))
                {
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }
                    const bool directory = it->is_directory(ec);
                    if (ec)
                    {
                        ec.clear();
                        continue;
                    }
                    if (directory)
                    {
                        if (should_skip_browser_directory(it->path().filename().string()))
                        {
                            it.disable_recursion_pending();
                            continue;
                        }
                        if (it.depth() >= 1)
                            it.disable_recursion_pending();
                    }
                    if (it.depth() > 1)
                        continue;

                    entries.push_back(make_browser_entry(root, it->path(), directory));
                }
            }

            if (entries.size() < maxEntries)
            {
                for (auto scriptEntry : collect_script_browser_entries(editor.projectRoot, maxEntries - entries.size()))
                {
                    if (entries.size() >= maxEntries)
                        break;
                    entries.push_back(std::move(scriptEntry));
                }
            }

            return entries;
        }

        [[nodiscard]] static std::string make_project_script_starter_text(std::string_view scriptId)
        {
            return std::format(
                "#if __has_include(<epoch.script_api.h>)\n"
                "#  include <epoch.script_api.h>\n"
                "#elif __has_include(<include/epoch.script_api.h>)\n"
                "#  include <include/epoch.script_api.h>\n"
                "#else\n"
                "#  error \"Epoch script API header not found. Add Engine/include or Engine/ to include paths.\"\n"
                "#endif\n\n"
                "namespace\n"
                "{{\n"
                "    void host_log(EpochScriptHost* host, const char* message)\n"
                "    {{\n"
                "        if (host && host->log)\n"
                "            host->log(host->user_data, message);\n"
                "    }}\n"
                "}}\n\n"
                "EPOCH_SCRIPT_EXPORT void run_script(EpochScriptHost* host)\n"
                "{{\n"
                "    if (!host)\n"
                "        return;\n\n"
                "    host_log(host, \"{}: script starter executed.\");\n"
                "    if (host->rotate_all_entities_yaw)\n"
                "    {{\n"
                "        host->rotate_all_entities_yaw(host->user_data, 3.0f);\n"
                "        host_log(host, \"{}: applied +3 yaw proof step.\");\n"
                "    }}\n"
                "}}\n",
                scriptId,
                scriptId);
        }

        static void create_project_script_starter(EditorState& editor)
        {
            const std::string scriptIdBase = sanitize_script_id(editor.newScriptName);
            const auto scriptsRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot }) / "scripts";
            std::error_code ec;
            std::filesystem::create_directories(scriptsRoot, ec);
            if (ec)
            {
                editor.scriptBuildStatus = "Could not create project scripts directory.";
                push_editor_log(editor, "[script] Failed to create scripts directory: " + scriptsRoot.generic_string());
                return;
            }

            std::string scriptId = scriptIdBase;
            std::filesystem::path scriptPath = scriptsRoot / (scriptId + ".ascript.cpp");
            for (int suffix = 2; std::filesystem::exists(scriptPath, ec) && suffix < 100; ++suffix)
            {
                ec.clear();
                scriptId = scriptIdBase + "_" + std::to_string(suffix);
                scriptPath = scriptsRoot / (scriptId + ".ascript.cpp");
            }

            std::ofstream out(scriptPath, std::ios::binary);
            if (!out)
            {
                editor.scriptBuildStatus = "Could not write project script starter.";
                push_editor_log(editor, "[script] Failed to write script starter: " + display_project_path(scriptPath));
                return;
            }

            out << make_project_script_starter_text(scriptId);
            editor.activeScript = scriptId;
            editor.newScriptName = scriptId;
            editor.selectedProjectFile = display_project_path(scriptPath);
            editor.selectedAssetPath = editor.selectedProjectFile;
            (void)load_script_source_editor(editor, scriptPath, true);
            editor.scriptBuildStatus = "Created project script starter: " + editor.selectedProjectFile;
            push_editor_log(editor, "[script] Created project script starter '" + scriptId + "'.");
            append_project_note(
                editor,
                "Create Project Script Starter",
                std::string("Created ") + scriptId + ".ascript.cpp.",
                "Use Build Selected Script to validate the script asset. The centered Run button remains reserved for the active generated project shell.");
        }

        [[nodiscard]] std::string build_ai_project_output_review_prompt(
            const EditorState& state,
            const std::filesystem::path& pathsManifest,
            const std::filesystem::path& buildLog,
            const std::filesystem::path& outputExe)
        {
            return std::format(
                "You are the selected OS AI model helping Epoch through an engine-owned harness around selected Qwen/Nemotron model lanes and approved creative model package lanes, trained through editor tools, sandbox scenes, build evidence, and eval gates.\n"
                "Review the latest project output evidence and produce exactly one safe self-iteration pass.\n"
                "Do not claim anything is working unless you cite the evidence paths below.\n"
                "Return concise sections: Diagnosis, Proposed Files, Editor/Tool Actions, Build/Test Commands, Verifier Gate, Training/Eval Record.\n"
                "Keep normal ProjectLauncher game/software work separate from the Self-Iteration Sandbox unless the operator explicitly approves mixing them.\n\n"
                "Local game/tool/app/server project code is allowed when requested, and local game/tool tests may run through visible editor/tool-harness controls when evidence-captured.\n"
                "Do not create or run apps/services that expose a model-accessible bypass channel, hidden control surface, server, listener, port bind, or serving mode without an explicit human enable/run action.\n\n"
                "Project: '{}' ({})\n"
                "Project root: {}\n"
                "Scene: {}\n"
                "Active script: {}\n"
                "Project status: {}\n"
                "Build status: {}\n"
                "Tool state: {}\n"
                "Paths manifest: {} [{}]\n"
                "Build log: {} [{}]\n"
                "Output executable: {} [{}]\n"
                "Iteration packet root: {}\n"
                "Build log tail:\n{}\n",
                state.projectName,
                state.projectId,
                state.projectRoot,
                state.projectScenePath,
                state.activeScript,
                state.projectStatus,
                state.projectBuildStatus,
                editor_tooling_state_summary(state),
                display_project_path(pathsManifest),
                file_ready_summary(pathsManifest),
                display_project_path(buildLog),
                file_ready_summary(buildLog),
                display_project_path(outputExe),
                file_ready_summary(outputExe),
                display_project_path(std::filesystem::path{ epoch::ai::iteration_packet_root() }),
                read_text_tail(buildLog));
        }

        [[nodiscard]] std::string staged_packet_count_summary(const std::filesystem::path& packetRoot)
        {
            const auto resolvedRoot = resolve_editor_path(packetRoot);
            std::error_code ec;
            if (!std::filesystem::exists(resolvedRoot, ec) || ec)
                return "0 staged packets; root not created yet";

            std::size_t packetCount = 0;
            for (const auto& entry : std::filesystem::directory_iterator(resolvedRoot, ec))
            {
                if (ec)
                    break;
                std::error_code entryEc;
                if (entry.is_directory(entryEc) && !entryEc)
                    ++packetCount;
            }

            if (ec)
                return "Packet root unreadable; inspect workspace permissions";

            return std::format("{} staged packet{}", packetCount, packetCount == 1 ? "" : "s");
        }

        struct SeedObjectSummary
        {
            std::size_t total{ 0 };
            std::size_t worldCount{ 0 };
            std::size_t gameplayCount{ 0 };
            std::size_t editorCount{ 0 };
            std::size_t visibleCount{ 0 };
            std::size_t editorOnlyCount{ 0 };
            std::vector<std::string> types{};
            std::vector<std::string> categories{};
        };

        [[nodiscard]] static SeedObjectSummary summarize_seed_objects(std::span<const EditorSceneSeedEntity> seeds)
        {
            SeedObjectSummary summary{};
            summary.total = seeds.size();

            auto append_unique = [](std::vector<std::string>& values, std::string_view value)
            {
                if (value.empty())
                    return;

                const std::string owned{ value };
                if (std::find(values.begin(), values.end(), owned) == values.end())
                    values.push_back(owned);
            };

            for (const auto& seed : seeds)
            {
                if (seed.category == "World")
                    ++summary.worldCount;
                else if (seed.category == "Gameplay")
                    ++summary.gameplayCount;
                else if (seed.category == "Editor")
                    ++summary.editorCount;

                if (seed.visible)
                    ++summary.visibleCount;
                if (seed.editor_only)
                    ++summary.editorOnlyCount;

                append_unique(summary.types, seed.type);
                append_unique(summary.categories, seed.category);
            }

            return summary;
        }

        [[nodiscard]] static std::string summarize_seed_category_mix(const SeedObjectSummary& summary)
        {
            return "World "
                + std::to_string(summary.worldCount)
                + " | Gameplay "
                + std::to_string(summary.gameplayCount)
                + " | Editor "
                + std::to_string(summary.editorCount);
        }

        [[nodiscard]] static std::string summarize_seed_type_list(const std::vector<std::string>& values)
        {
            if (values.empty())
                return "None";

            std::string result;
            const std::size_t count = (std::min)(values.size(), std::size_t{ 5 });
            for (std::size_t i = 0; i < count; ++i)
            {
                if (!result.empty())
                    result += " | ";
                result += values[i];
            }
            if (values.size() > count)
                result += " | ...";
            return result;
        }

        [[nodiscard]] bool path_exists(const std::filesystem::path& path) noexcept
        {
            if (path.empty())
                return false;
            std::error_code ec;
            return std::filesystem::exists(resolve_editor_path(path), ec);
        }

        void repair_project_evidence(EditorState& editor, std::string_view projectId)
        {
            const std::string requestedProject = projectId.empty()
                ? std::string(editor_default_project_profile().id)
                : std::string(projectId);
            const std::string previousProject = editor.projectId;
            const std::vector<EditorEntity> preservedEntities = editor.entities;
            const std::size_t preservedSelected = editor.selectedEntity;
            const auto preservedPreviewMode = editor.previewMode;
            const auto preservedWorkspaceTab = editor.workspaceTab;
            const auto preservedDockStatusTab = editor.dockStatusTab;
            const auto preservedMainSurface = editor.mainSurface;
            const auto preservedProjectCameraMode = editor.projectCameraMode;
            const auto preservedInputProfile = editor.inputProfilePreset;
            const std::string preservedRunBackend = editor.projectRunBackend;
            const double preservedRunFrameLimit = editor.projectRunFrameLimitFps;
            const bool isSandbox = requestedProject == "sandbox";
            const auto ensured = editor_ensure_project_shell(requestedProject);
            editor.projectStatus = ensured.summary;
            editor.aiContinuousBuildFingerprint.clear();
            if (ensured.succeeded)
            {
                const std::string targetProject = ensured.project_id.empty() ? requestedProject : ensured.project_id;
                set_project(editor, targetProject, true);
                if (targetProject == previousProject)
                {
                    editor.previewMode = preservedPreviewMode;
                    editor.workspaceTab = preservedWorkspaceTab;
                    editor.dockStatusTab = preservedDockStatusTab;
                    editor.mainSurface = preservedMainSurface;
                    editor.projectCameraMode = preservedProjectCameraMode;
                    editor.inputProfilePreset = preservedInputProfile;
                    editor.projectRunBackend = preservedRunBackend;
                    editor.projectRunFrameLimitFps = preservedRunFrameLimit;
                    input::set_active_profile(editor.inputProfilePreset);

                    if (!preservedEntities.empty())
                    {
                        editor.entities = preservedEntities;
                        editor.selectedEntity = (std::min)(preservedSelected, editor.entities.size() - 1u);
                        save_editor_scene_snapshot(editor);
                    }
                }
                editor.projectStatus = ensured.summary + " Active project saved.";
                editor.aiContinuousBuildStatus = isSandbox
                    ? "Sandbox evidence saved; queue a self-iteration build to stage verifier evidence."
                    : "Project evidence saved; queue a build/run or explicit self-iteration handoff.";
                push_editor_log(editor, "[project] Saved active project evidence: " + ensured.summary);
                append_project_note(
                    editor,
                    isSandbox ? "Save Self-Iteration Sandbox Evidence" : "Save Active Project",
                    ensured.summary,
                    isSandbox
                        ? "Sandbox evidence is ready for a controlled engine self-iteration build pass."
                        : "Project evidence is ready for a manual run/build or an explicit AI handoff.");
            }
            else
            {
                editor.aiContinuousBuildStatus = "Project evidence save failed; inspect project status.";
                push_editor_log(editor, "[project] Save active project evidence failed: " + ensured.summary);
            }
        }

        void repair_active_project_evidence(EditorState& editor)
        {
            repair_project_evidence(editor, editor.projectId);
        }

        [[nodiscard]] std::string editor_json_escape(std::string_view text)
        {
            std::string escaped;
            escaped.reserve(text.size() + 8u);
            for (const char ch : text)
            {
                switch (ch)
                {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20u)
                        escaped += ' ';
                    else
                        escaped += ch;
                    break;
                }
            }
            return escaped;
        }

        [[nodiscard]] std::string safe_package_artifact_id(std::string_view id)
        {
            std::string safe;
            safe.reserve(id.size());
            for (const char ch : id)
            {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if (std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.')
                    safe += ch;
                else
                    safe += '_';
            }
            return safe.empty() ? std::string{ "package" } : safe;
        }

        [[nodiscard]] bool stage_model_package_opt_in(
            EditorState& editor,
            const epoch::package_registry::PackageDescriptor& package)
        {
            if (editor.projectRoot.empty())
            {
                editor.packageInstallStatus = "No active project root for model package opt-in.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const std::string safeId = safe_package_artifact_id(package.id);
            const std::filesystem::path projectRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            const std::filesystem::path packageDir = projectRoot / "assets" / "packages";
            const std::filesystem::path manifestPath = packageDir / (safeId + ".model.package.json");
            const std::filesystem::path modelCacheDir =
                resolve_editor_path(std::filesystem::path{ epoch::ai::local_model_root() }) / safeId;
            const std::filesystem::path downloadPlanPath = modelCacheDir / "download.plan.json";

            std::error_code ec;
            std::filesystem::create_directories(packageDir, ec);
            if (ec)
            {
                editor.packageInstallStatus = "Could not create project package directory.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            ec.clear();
            std::filesystem::create_directories(modelCacheDir, ec);
            if (ec)
            {
                editor.packageInstallStatus = "Could not create executable-local model cache directory.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const std::string packageId = editor_json_escape(package.id);
            const std::string displayName = editor_json_escape(package.displayName);
            const std::string repo = editor_json_escape(package.externalSourceRepo);
            const std::string cacheRoot = editor_json_escape(epoch::ai::local_model_root());
            const std::string cacheDir = editor_json_escape(modelCacheDir.generic_string());

            {
                std::ofstream out(manifestPath, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus = "Could not write project model package manifest.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.model.package.v1\",\n"
                    << "  \"package_id\": \"" << packageId << "\",\n"
                    << "  \"display_name\": \"" << displayName << "\",\n"
                    << "  \"type\": \"os_model_asset\",\n"
                    << "  \"source_repo\": \"" << repo << "\",\n"
                    << "  \"cache_root\": \"" << cacheRoot << "\",\n"
                    << "  \"download_policy\": \"operator_demand_only\",\n"
                    << "  \"include_in_project\": true,\n"
                    << "  \"weights_bundled\": false,\n"
                    << "  \"requires_license_notice_review\": true\n"
                    << "}\n";
            }

            {
                std::ofstream out(downloadPlanPath, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus = "Could not write model download plan.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.model.download.plan.v1\",\n"
                    << "  \"package_id\": \"" << packageId << "\",\n"
                    << "  \"source_repo\": \"" << repo << "\",\n"
                    << "  \"target_dir\": \"" << cacheDir << "\",\n"
                    << "  \"transfer\": \"not_started\",\n"
                    << "  \"human_approval_required\": true,\n"
                    << "  \"engine_iteration_clone\": false,\n"
                    << "  \"notes\": \"Weights are fetched on demand only; generated projects carry this manifest until an operator includes or downloads the model.\"\n"
                    << "}\n";
            }

            editor.packageInstallStatus = "Model download plan staged in cache/models; weight transfer has not started.";
            editor.packageInstallProgress = 1.0f;
            append_project_note(
                editor,
                "Stage OS Model Package",
                std::string("Staged ") + std::string(package.displayName) + " as an explicit project model package opt-in.",
                "Weights remain outside source and are downloaded only after an operator-approved package download step.");
            push_editor_log(editor, "[package] Wrote model package manifest: " + display_project_path(manifestPath));
            return true;
        }

        [[nodiscard]] bool stage_forest_factory_package_opt_in(
            EditorState& editor,
            const epoch::package_registry::PackageDescriptor& package)
        {
            if (editor.projectRoot.empty())
            {
                editor.packageInstallStatus = "No active project root for Plant Lab package activation.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const std::string safeId = safe_package_artifact_id(package.id);
            const std::filesystem::path projectRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            const std::filesystem::path packageDir = projectRoot / "assets" / "packages";
            const std::filesystem::path forestDir = packageDir / safeId;
            const std::filesystem::path manifestPath = packageDir / (safeId + ".package.json");
            const std::filesystem::path profilePath = forestDir / "default.forest.json";

            std::error_code ec;
            std::filesystem::create_directories(forestDir, ec);
            if (ec)
            {
                editor.packageInstallStatus = "Could not create Plant Lab package directory.";
                editor.packageInstallProgress = 0.0f;
                return false;
            }

            const std::string packageId = editor_json_escape(package.id);
            const std::string displayName = editor_json_escape(package.displayName);
            const std::string sourceRepo = editor_json_escape(package.externalSourceRepo);
            const std::string referenceRepo = editor_json_escape(epoch::forest::kForestFactoryReferenceRepo);
            const std::string profileFile = editor_json_escape(profilePath.generic_string());

            {
                std::ofstream out(manifestPath, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus = "Could not write Plant Lab package manifest.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.core.package.v1\",\n"
                    << "  \"package_id\": \"" << packageId << "\",\n"
                    << "  \"display_name\": \"" << displayName << "\",\n"
                    << "  \"type\": \"core_opt_in\",\n"
                    << "  \"source_repo\": \"" << sourceRepo << "\",\n"
                    << "  \"reference_repo\": \"" << referenceRepo << "\",\n"
                    << "  \"editor_workspace\": \"Plant Lab\",\n"
                    << "  \"activation\": \"main_scene_use_or_explicit_package_install\",\n"
                    << "  \"project_payload_policy\": \"emit descriptors/assets only after visible package activation\",\n"
                    << "  \"default_profile\": \"" << profileFile << "\",\n"
                    << "  \"runtime_outputs\": [\"preview_skeleton\", \"mesh_lod\", \"impostor\", \"voxel_occupancy\", \"seed_asset\"],\n"
                    << "  \"requires_human_build_gate\": true,\n"
                    << "  \"server_or_listener_allowed\": false\n"
                    << "}\n";
            }

            {
                std::ofstream out(profilePath, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    editor.packageInstallStatus = "Could not write Plant Lab default profile.";
                    editor.packageInstallProgress = 0.0f;
                    return false;
                }

                out
                    << "{\n"
                    << "  \"schema\": \"epoch.forest.profile.v1\",\n"
                    << "  \"preset\": \"tree\",\n"
                    << "  \"mode\": \"3d\",\n"
                    << "  \"seed\": 1337,\n"
                    << "  \"technique\": \"temporal_graph_parametric_l_system\",\n"
                    << "  \"source_repo\": \"" << sourceRepo << "\",\n"
                    << "  \"reference_repo\": \"" << referenceRepo << "\",\n"
                    << "  \"outputs\": {\n"
                    << "    \"preview_skeleton\": true,\n"
                    << "    \"mesh_lod\": true,\n"
                    << "    \"impostor\": true,\n"
                    << "    \"voxel_occupancy\": true,\n"
                    << "    \"seed_asset\": true\n"
                    << "  },\n"
                    << "  \"project_inclusion\": \"explicit_package_activation_only\"\n"
                    << "}\n";
            }

            editor.packageInstallStatus = "Plant Lab package staged; project payload waits for scene-use approval.";
            editor.packageInstallProgress = 0.65f;
            append_project_note(
                editor,
                "Stage Plant Lab Package",
                "Staged the core Plant Lab package manifest and default deterministic profile.",
                "Plant Lab is built into the editor, package payloads route through EpochEngineExtensions, and the Forest Factory descriptor lane remains the core engine contract.");
            push_editor_log(editor, "[package] Wrote Plant Lab package manifest: " + display_project_path(manifestPath));
            return true;
        }

        [[nodiscard]] std::string project_runtime_scene_id(const EditorState& editor)
        {
            if (!editor.activeRuntimeScene.empty() && is_engine_arcade_scene_id(editor.activeRuntimeScene))
                return editor.activeRuntimeScene;
            if (!editor.projectId.empty())
                return std::string("project:") + editor.projectId;
            return editor.activeRuntimeScene.empty() ? std::string("project:projectlauncher") : editor.activeRuntimeScene;
        }

        struct ProjectBuildFreshness
        {
            bool rebuildRequired{ true };
            std::string reason{ "build state unknown" };
        };

        [[nodiscard]] bool file_is_newer_than(
            const std::filesystem::path& input,
            const std::filesystem::file_time_type outputTime,
            std::string& changedInput)
        {
            std::error_code ec;
            const std::filesystem::path resolved = resolve_editor_path(input);
            if (resolved.empty() || !std::filesystem::exists(resolved, ec) || ec)
                return false;

            const auto inputTime = std::filesystem::last_write_time(resolved, ec);
            if (ec)
                return false;

            if (inputTime > outputTime)
            {
                changedInput = display_project_path(resolved);
                return true;
            }

            return false;
        }

        [[nodiscard]] ProjectBuildFreshness project_child_build_freshness(const EditorState& editor)
        {
            if (editor.projectRoot.empty())
                return { true, "no active project root" };

            const std::filesystem::path outputExe = project_existing_output_exe_path(editor.projectRoot);
            std::error_code ec;
            if (!std::filesystem::exists(outputExe, ec) || ec)
                return { true, "child executable missing" };

            const auto outputTime = std::filesystem::last_write_time(outputExe, ec);
            if (ec)
                return { true, "child executable timestamp unavailable" };

            std::vector<std::filesystem::path> buildInputs{
                project_entry_source_path(editor.projectRoot),
                project_windows_vcxproj_path(editor.projectRoot),
                project_windows_build_script_path(editor.projectRoot),
                resolve_editor_path(std::filesystem::path{ "x64" } / "Debug" / "StaticLib1.lib")
            };

            const std::string activeScriptSource =
                editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            if (!activeScriptSource.empty())
                buildInputs.emplace_back(activeScriptSource);

            // Scene and manifest data are runtime inputs. They are saved before launch
            // but should not force a relink; Build Active Project remains available
            // when the operator intentionally wants a fresh child executable.
            for (const auto& input : buildInputs)
            {
                std::string changedInput;
                if (file_is_newer_than(input, outputTime, changedInput))
                    return { true, std::string("build input changed: ") + changedInput };
            }

            return { false, "child executable is current" };
        }

        void start_project_build(EditorState& editor, bool runAfterBuild, std::string_view reason)
        {
            if (editor.projectBuildPending
                && editor.projectBuildPending->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
            {
                try
                {
                    (void)editor.projectBuildPending->get();
                }
                catch (...)
                {
                    push_editor_log(editor, "[project] Cleared completed build handle before queuing the next run request.");
                }

                editor.projectBuildPending.reset();
                editor.projectBuildRunAfterBuild = false;
            }

            if (editor.projectBuildPending)
            {
                editor.projectBuildStatus = "Project build already running; wait for the current build before pressing Run again.";
                push_editor_log(editor, "[project] Build/run request ignored because a project build is already running.");
                return;
            }

            if (editor.aiContinuousBuildPending)
            {
                editor.projectBuildStatus = "Self-iteration build is already running; wait before starting a project build.";
                push_editor_log(editor, "[project] Build/run request blocked while AI self-iteration build is running.");
                return;
            }

            if (editor.projectRoot.empty())
            {
                editor.projectBuildStatus = "No active project root selected.";
                push_editor_log(editor, "[project] Build/run request failed: no active project root.");
                return;
            }

            repair_active_project_evidence(editor);

            const std::filesystem::path outputExe = project_existing_output_exe_path(editor.projectRoot);
            editor.projectBuildRunAfterBuild = runAfterBuild;
            editor.projectBuildRunScene = project_runtime_scene_id(editor);
            editor.projectBuildOutputPath = display_project_path(outputExe);
            editor.projectBuildRunBackend = editor.projectRunBackend.empty() ? std::string("opengl") : editor.projectRunBackend;
            editor.projectBuildRunFrameLimitFps = editor.projectRunFrameLimitFps;
            editor.projectBuildRunCameraMode = editor.projectCameraMode;
            editor.projectBuildRunInputProfile = editor.inputProfilePreset;
            editor.projectBuildStatus =
                std::string(runAfterBuild ? "Build/run queued: " : "Build queued: ") + std::string(reason);
            push_editor_log(editor, "[project] " + editor.projectBuildStatus);

            editor.projectBuildPending.emplace(std::async(std::launch::async, [root = editor.projectRoot]() {
                epoch::systems::threading::ScopedThreadActivity threadActivity{};
                return editor_build_project(root);
            }));
        }

        void activate_self_iteration_sandbox(
            EditorState& editor,
            const std::shared_ptr<core::Context>& ctx,
            bool writeLog)
        {
            if (editor.projectId != "sandbox")
                set_project(editor, "sandbox", writeLog);
            editor.workspaceTab = EditorWorkspaceTab::AI;
            editor.aiWorkspaceDomain = AiWorkspaceDomain::Control;
            editor.projectStatus = "Self-Iteration Sandbox selected. It manipulates and tests Epoch itself, not generated game/tool projects.";
            if (ctx)
            {
                epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::Editor);
                epochnamespace::previewgrid::reset_camera(ctx.get());
            }
        }

        void repair_self_iteration_sandbox_evidence(EditorState& editor)
        {
            repair_project_evidence(editor, "sandbox");
        }

        void start_self_iteration_sandbox_build(EditorState& editor, std::string_view reason, bool force)
        {
            repair_self_iteration_sandbox_evidence(editor);
            const std::filesystem::path pathsManifest =
                resolve_editor_path(std::filesystem::path{ editor.projectRoot }) / "project.paths.txt";
            const std::string activeScriptSource =
                editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            start_ai_continuous_project_build(editor, activeScriptSource, pathsManifest, reason, force);
        }

        [[nodiscard]] std::string ready_text(bool ready)
        {
            return ready ? "Ready" : "Missing";
        }

        struct AiReviewGateStatus
        {
            bool buildEvidenceReady{ false };
            bool projectEvidenceReady{ false };
            bool captureEvidenceReady{ false };
            bool chatPairReady{ false };
            std::size_t readyEvidenceCount{ 0 };
            std::size_t totalEvidenceCount{ 0 };
            std::string packetEvidenceSummary{};
            std::string promotionSummary{};
        };

        [[nodiscard]] AiReviewGateStatus summarize_ai_review_gate(
            const EditorState& editor,
            const std::filesystem::path& buildLog,
            const std::filesystem::path& outputExe,
            const std::filesystem::path& pathsManifest,
            const epoch::ai::TrainingPaths& training,
            std::string_view latestPrompt,
            std::string_view latestReply)
        {
            AiReviewGateStatus status{};
            const bool manifestReady = !editor.projectManifest.empty() && path_exists(editor.projectManifest);
            const bool projectRootReady = !editor.projectRoot.empty() && path_exists(editor.projectRoot);
            const bool sceneReady = !editor.projectScenePath.empty() && path_exists(editor.projectScenePath);
            const bool buildLogReady = path_exists(buildLog);
            const bool outputReady = path_exists(outputExe);
            const bool pathsReady = path_exists(pathsManifest);
            const bool rawCaptureReady = path_exists(training.local_capture_jsonl);
            const bool mcpCaptureReady = path_exists(training.mcp_capture_jsonl);

            status.buildEvidenceReady = buildLogReady && outputReady;
            status.projectEvidenceReady = manifestReady && projectRootReady && pathsReady;
            status.captureEvidenceReady = rawCaptureReady || mcpCaptureReady;
            status.chatPairReady = !latestPrompt.empty()
                && !latestReply.empty()
                && latestReply != "(empty reply)";

            const std::array evidenceFlags{
                manifestReady,
                projectRootReady,
                sceneReady,
                pathsReady,
                buildLogReady,
                outputReady,
                rawCaptureReady,
                mcpCaptureReady
            };
            status.totalEvidenceCount = evidenceFlags.size();
            status.readyEvidenceCount = static_cast<std::size_t>(std::count(evidenceFlags.begin(), evidenceFlags.end(), true));
            status.packetEvidenceSummary = std::to_string(status.readyEvidenceCount)
                + "/" + std::to_string(status.totalEvidenceCount)
                + " staged evidence paths exist";

            if (status.buildEvidenceReady
                && status.projectEvidenceReady
                && status.captureEvidenceReady
                && status.chatPairReady)
            {
                status.promotionSummary = "Ready for review and curated promotion";
            }
            else if (!status.buildEvidenceReady)
            {
                status.promotionSummary = "Blocked: build log/output evidence not ready";
            }
            else if (!status.projectEvidenceReady)
            {
                status.promotionSummary = "Blocked: project manifest/paths evidence incomplete";
            }
            else if (!status.captureEvidenceReady)
            {
                status.promotionSummary = "Blocked: no raw or tool evidence capture yet";
            }
            else
            {
                status.promotionSummary = "Blocked: latest chat pair is not promotable yet";
            }

            return status;
        }

        [[nodiscard]] const char* ai_control_loop_contract() noexcept
        {
            return "planner -> executor -> builder -> verifier -> gate";
        }

        [[nodiscard]] std::string ai_control_loop_stage(const AiReviewGateStatus& status)
        {
            if (!status.projectEvidenceReady)
                return "Planner blocked: project manifest and path evidence incomplete";
            if (!status.buildEvidenceReady)
                return "Builder blocked: build log/output evidence incomplete";
            if (!status.captureEvidenceReady)
                return "Verifier blocked: no raw or tool evidence capture staged";
            if (!status.chatPairReady)
                return "Gate blocked: latest assistant exchange is not promotable yet";
            return "Gate ready: build, project, capture, and chat evidence are staged for review";
        }

        [[nodiscard]] std::string_view ai_workspace_domain_name(AiWorkspaceDomain domain) noexcept
        {
            switch (domain)
            {
            case AiWorkspaceDomain::Control:
                return "Self-Iteration Sandbox";
            case AiWorkspaceDomain::Tooling:
                return "Tool Harness";
            case AiWorkspaceDomain::Engine:
                return "Engine Assistant";
            case AiWorkspaceDomain::Software:
                return "Project Launcher";
            case AiWorkspaceDomain::Training:
                return "Training";
            case AiWorkspaceDomain::Visualizer:
                return "AI Visualizer";
            case AiWorkspaceDomain::Ops:
                return "Ops / How-To";
            default:
                return "AI";
            }
        }

        void render_ai_model_picker(EditorState& editor, float width, std::string_view selectBoxId)
        {
            const float contentWidth = (std::max)(180.0f, width);
            const auto manifest = epoch::ai::active_model_manifest();
            auto detectedModels = epoch::ai::detected_model_names();

            gui::property_row("[model] Chat provider", std::string(epoch::ai::provider_mode_name(epoch::ai::current_provider_mode())));
            gui::property_row("[model] Selected model", manifest.display_name.empty() ? std::string("(none selected)") : manifest.display_name);
            gui::property_row("[model] Endpoint", manifest.endpoint);
            gui::property_row("[model] API route", "/v1/models + /v1/chat/completions");
            gui::property_row("[model] Client state", epoch::ai::model_connection_status());
            gui::property_row("[ai] Model discovery", epoch::ai::model_detection_status());

            if (gui::button("Scan Local OpenAI Models", { 240.0f, 30.0f }))
            {
                detectedModels = epoch::ai::refresh_detected_models();
                push_editor_log(
                    editor,
                    detectedModels.empty()
                        ? "[ai] Model scan found no local OpenAI-compatible models."
                        : "[ai] Model scan refreshed; select one model below.");
            }

            if (detectedModels.empty())
            {
                gui::wrapped_label(
                    "No chat model is selected. Start LM Studio, Ollama, or another local OpenAI-compatible endpoint, scan models, then choose the exact model Epoch should call.",
                    contentWidth);
                return;
            }

            std::vector<std::string_view> modelViews;
            modelViews.reserve(detectedModels.size());
            for (const auto& modelId : detectedModels)
                modelViews.emplace_back(modelId);

            const std::string selectedModel = epoch::ai::active_model_name();
            const auto selectResult = gui::select_box(gui::SelectBoxOptions{
                .id = selectBoxId,
                .placeholder = "Choose local chat model",
                .selected = selectedModel.empty() ? std::string_view{} : std::string_view{ selectedModel },
                .options = modelViews,
                .size = { contentWidth, 30.0f },
                .row_height = 30.0f,
                .max_visible_options = 7
            });
            if (selectResult.changed && selectResult.selected_index && *selectResult.selected_index < detectedModels.size())
            {
                const auto& modelId = detectedModels[*selectResult.selected_index];
                if (epoch::ai::select_active_model(modelId))
                {
                    push_editor_log(editor, "[ai] Selected local model: " + modelId + " (" + epoch::ai::model_connection_status() + ")");
                    push_editor_log(editor, "[ai] OS AI chat will use " + modelId + " through the local OpenAI-compatible endpoint.");
                }
                else
                    push_editor_log(editor, "[ai] Could not select model: " + modelId);
            }
        }

        void render_ai_control_status_panel(
            const EditorState& editor,
            const AiReviewGateStatus& status,
            std::string_view loopStage,
            float width)
        {
            const auto ready = [](bool value) noexcept
            {
                return value ? "ready" : "missing";
            };
            const std::string evidence =
                std::to_string(status.readyEvidenceCount)
                + "/"
                + std::to_string(status.totalEvidenceCount)
                + " evidence paths staged";

            gui::wrapped_label(
                "Self-Iteration Sandbox is the separate dark-factory control room: plan, build, verify, and promote only from visible evidence. It is not the normal game/software scene editor, and it does not depend on atlas/runtime texture packing.",
                width);
            gui::property_row("[self-iteration] Loop", ai_control_loop_contract());
            gui::property_row("[self-iteration] Stage", std::string(loopStage));
            gui::property_row("[self-iteration] Evidence", evidence);
            gui::property_row("[self-iteration] Project", ready(status.projectEvidenceReady));
            gui::property_row("[self-iteration] Build", ready(status.buildEvidenceReady));
            gui::property_row("[self-iteration] Capture", ready(status.captureEvidenceReady));
            gui::property_row("[self-iteration] Chat pair", ready(status.chatPairReady));
            gui::property_row("[self-iteration] Gate", status.promotionSummary);
            gui::property_row("[self-iteration] Build gate", editor.aiContinuousBuildPending ? "build running" : "manual approval");
            gui::property_row("[self-iteration] Build pending", editor.aiContinuousBuildPending.has_value() ? "true" : "false");
            gui::property_row("[self-iteration] Build runs", std::to_string(editor.aiContinuousBuildRunCount));
            gui::property_row("[self-iteration] Tool harness runs", std::to_string(editor.aiToolHarnessRunCount));
            gui::property_row("[self-iteration] Write policy", "staged packets only; no blind write-through");
        }

        AiChat& chat_state_for(const std::shared_ptr<core::Context>& ctx)
        {
            auto& storage = chat_storage();
            std::scoped_lock lock(storage.mutex);

            if (!storage.bot_initialized)
            {
                epoch::ai::init_engine_ai();
                storage.bot_initialized = true;
            }

            auto [it, inserted] = storage.chats.try_emplace(ctx.get());
            return it->second;
        }

        EditorState& editor_state_for(const std::shared_ptr<core::Context>& ctx)
        {
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            auto [it, inserted] = storage.states.try_emplace(ctx.get());
            if (inserted)
            {
                it->second.initialized = true;
                it->second.automationCommand = read_editor_automation_command();
                it->second.timeControl.fixed_dt_seconds = 1.0 / 60.0;
                it->second.timeControl.time_scale = 1.0;
                it->second.timeControl.max_steps_per_frame = 8;
                const std::string requestedProjectId = read_requested_editor_project_id();
                if (!requestedProjectId.empty() && editor_find_project_profile(requestedProjectId))
                    set_project(it->second, requestedProjectId, false);
                else
                    set_project(it->second, editor_default_project_profile().id, false);
                if (ctx)
                    epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::Editor);
                push_editor_log(it->second, "[info] Editor scene initialized.");
                push_editor_log(it->second, "[info] Use the bottom dock tabs for Project, Scripts, AI, Systems, and Output evidence. Dedicated editor windows/views are still being built.");
                push_editor_log(it->second, "[info] Scene viewport is owned by the active backend.");
                push_editor_log(it->second, "[info] Systems now tracks the shared simulation-time spine for pacing and fixed-step diagnostics.");
                if (!requestedProjectId.empty() && it->second.projectId == requestedProjectId)
                    push_editor_log(it->second, std::string("[project] Preloaded active project from environment: ") + requestedProjectId + ".");
                if (it->second.automationCommand == EditorAutomationCommand::SmartUpdate)
                {
                    it->second.autoUpdateCheckQueued = false;
                    push_editor_log(it->second, "[info] Auto command armed: smart update.");
                }
                else if (it->second.automationCommand == EditorAutomationCommand::SourceUpdate)
                {
                    it->second.autoUpdateCheckQueued = false;
                    push_editor_log(it->second, "[info] Auto command armed: source update.");
                }
            }
            return it->second;
        }

    } // namespace

    void cleanup_chat_context(const core::Context* ctx)
    {
        if (!ctx) return;

        auto& chatStorage = chat_storage();
        auto& editorStorage = editor_storage();
        std::scoped_lock lock(chatStorage.mutex, editorStorage.mutex);
        chatStorage.chats.erase(ctx);
        editorStorage.states.erase(ctx);
        epochnamespace::previewgrid::cleanup_context(ctx);
    }

    void shutdown_chat_system()
    {
        auto& chatStorage = chat_storage();
        auto& editorStorage = editor_storage();
        std::scoped_lock lock(chatStorage.mutex, editorStorage.mutex);

        chatStorage.chats.clear();
        editorStorage.states.clear();

        if (chatStorage.bot_initialized)
        {
            epoch::ai::shutdown_engine_ai();
            chatStorage.bot_initialized = false;
        }
    }

    void editor_load_project(const std::shared_ptr<core::Context>& ctx, std::string_view project_id)
    {
        if (!ctx)
            return;

        auto& editor = editor_state_for(ctx);
        set_project(editor, project_id, true);
        epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::Editor);
        epochnamespace::previewgrid::reset_camera(ctx.get());
    }

    void editor_reset_transient_ui(const core::Context* ctx)
    {
        if (!ctx)
            return;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return;

        it->second.openMenu = TopMenu::None;
        it->second.showAboutModal = false;
        it->second.showSettingsModal = false;
        it->second.showPackageManagerModal = false;
        it->second.showUpdateConfirmModal = false;
        it->second.showSourceUpdateConfirmModal = false;
        it->second.previewMode = core::ScenePreviewMode::Editor;
        epochnamespace::previewgrid::set_camera_mode(ctx, epochnamespace::previewgrid::CameraMode::Editor);
        epochnamespace::previewgrid::reset_camera(ctx);
    }

    void editor_set_time_snapshot(const core::Context* ctx, const EditorTimeSnapshot& snapshot)
    {
        if (!ctx)
            return;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return;

        auto& state = it->second;
        state.timeSnapshot = snapshot;
    }

    EditorTimeControl editor_time_control(const core::Context* ctx)
    {
        if (!ctx)
            return {};

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        return it == storage.states.end() ? EditorTimeControl{} : it->second.timeControl;
    }

    void editor_consume_time_step_request(const core::Context* ctx)
    {
        if (!ctx)
            return;

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it == storage.states.end())
            return;

        it->second.timeControl.step_once = false;
    }

    bool editor_run_script(const core::Context* ctx, std::string_view script_name)
    {
        if (!ctx)
            return false;

        std::string projectRoot{};
        std::string projectId{};
        {
            auto& storage = editor_storage();
            std::scoped_lock lock(storage.mutex);
            const auto it = storage.states.find(ctx);
            if (it != storage.states.end())
            {
                projectRoot = it->second.projectRoot;
                projectId = it->second.projectId;
            }
        }

        scripting::ScriptLoadReport report;
        const std::string projectModelAsset = editor_project_demo_model_path(projectId);
        EpochScriptHost host{
            .user_data = const_cast<core::Context*>(ctx),
            .log = &script_log_callback,
            .rotate_all_entities_yaw = &script_rotate_all_entities_yaw_callback,
            .queue_model_load = &script_queue_model_load_callback,
            .project_model_asset = projectModelAsset.empty() ? nullptr : projectModelAsset.c_str(),
            .request_engine_scene = &script_request_engine_scene_callback
        };

        const std::string sourcePath = editor_resolve_script_source_path(script_name, projectRoot);
        const bool ok = scripting::load_or_reload_script(
            std::filesystem::path{ sourcePath },
            script_name,
            &host,
            &report);

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it != storage.states.end())
        {
            push_editor_log(it->second, std::string("[script] Source path: ") + sourcePath);
            for (const auto& message : report.messages())
                push_editor_log(it->second, message);
            push_editor_log(
                it->second,
                ok
                ? std::string("[script] Run completed successfully.")
                : std::string("[script] Run failed."));
        }

        return ok;
    }

    EditorFrameResult editor_run(const std::shared_ptr<core::Context>& ctx)
    {
        EditorFrameResult result{};
        if (!ctx)
            return result;

        auto& editor = editor_state_for(ctx);
        const gui::ScopedTheme editorThemeScope{ editor.themePreference };
        auto& chat = chat_state_for(ctx);
        chat.pump();
        if (editor.autoUpdateCheckQueued)
        {
            editor.autoUpdateCheckQueued = false;
            if (try_claim_editor_startup_update_check())
                start_editor_update_check(editor);
        }
        pump_editor_update_check(editor);
        pump_editor_source_update_worker(editor);
        if (editor.workspaceTab == EditorWorkspaceTab::AI
            && editor.aiWorkspaceDomain == AiWorkspaceDomain::Control
            && editor.projectId != "sandbox")
        {
            set_project(editor, "sandbox", true);
        }

        auto resolve_layout_extent = [&]() noexcept
        {
            int resolvedWidth = 0;
            int resolvedHeight = 0;

            // Docked/direct-child backends can report stale renderer dimensions while
            // the parent window is actively resizing. The GUI owns the full client
            // layout, so use the live window extent first and let backend dimensions
            // remain a fallback only for non-windowed/headless contexts.
            if (ctx->windowData)
            {
                const int liveWidth = ctx->windowData->get_width();
                const int liveHeight = ctx->windowData->get_height();
                if (liveWidth > 0 && liveHeight > 0)
                {
                    resolvedWidth = liveWidth;
                    resolvedHeight = liveHeight;
                }
            }

            if (resolvedWidth <= 0 || resolvedHeight <= 0)
            {
                resolvedWidth = ctx->get_width_safe();
                resolvedHeight = ctx->get_height_safe();
            }

            return gui::Vec2{
                static_cast<float>((std::max)(1, resolvedWidth)),
                static_cast<float>((std::max)(1, resolvedHeight))
            };
        };

        // Editor GUI layout must follow the live client pane, not a stale
        // startup framebuffer, or docked panes can hide Inspector/AI Chat.
        const gui::Vec2 layoutExtent = resolve_layout_extent();
        const float w = layoutExtent.x;
        const float h = layoutExtent.y;
        const auto centered_modal_position = [&](const gui::Vec2 modalSize) noexcept -> gui::Vec2
            {
                return {
                    (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                    (std::max)(0.0f, (h - modalSize.y) * 0.5f)
                };
            };
        const auto fit_modal_size = [&](const gui::Vec2 desired, const gui::Vec2 minimum) noexcept -> gui::Vec2
            {
                const float maxWidth = (std::max)(1.0f, w - 64.0f);
                const float maxHeight = (std::max)(1.0f, h - 64.0f);
                const float minWidth = (std::min)(minimum.x, maxWidth);
                const float minHeight = (std::min)(minimum.y, maxHeight);
                return {
                    std::clamp(desired.x, minWidth, maxWidth),
                    std::clamp(desired.y, minHeight, maxHeight)
                };
            };
        const auto update_confirm_modal_size = [&]() -> gui::Vec2
            {
                const bool sourceOnlyUpdate =
                    editor.lastUpdateCheck.source_update_available
                    && !editor.lastUpdateCheck.packaged_update_available;
                const bool sourceWorkerRunning = editor.updateState == EditorUpdateState::SourceWorkerRunning;
                const bool restartReady = editor.updateState == EditorUpdateState::RestartReady;

                const float modalWidth = fit_modal_size({ 760.0f, 1.0f }, { 660.0f, 1.0f }).x;
                const float contentWidth = (std::max)(1.0f, modalWidth - 56.0f);
                const auto estimated_wrapped_height = [](const std::string_view text, const float width) noexcept -> float
                    {
                        const std::size_t charsPerLine = (std::max)(
                            std::size_t{ 28 },
                            static_cast<std::size_t>((std::max)(1.0f, width) / 7.5f));
                        std::size_t lines = 1u;
                        std::size_t column = 0u;
                        for (const char c : text)
                        {
                            if (c == '\n')
                            {
                                ++lines;
                                column = 0u;
                                continue;
                            }
                            ++column;
                            if (column >= charsPerLine && (c == ' ' || c == '-' || c == '/' || c == ','))
                            {
                                ++lines;
                                column = 0u;
                            }
                            else if (column >= charsPerLine + 12u)
                            {
                                ++lines;
                                column = 0u;
                            }
                        }
                        return static_cast<float>(lines) * 18.0f;
                    };
                const auto trimmed_status = [&]() {
                    std::string text = editor.updateStatus;
                    constexpr std::size_t kMaxModalStatus = 176u;
                    if (text.size() <= kMaxModalStatus)
                        return text;
                    return text.substr(0u, kMaxModalStatus - 3u) + "...";
                    }();

                const std::string_view introText = sourceOnlyUpdate
                    ? "No packaged runtime was found for this platform, so Epoch is using the source rebuild lane."
                    : "A newer packaged Epoch runtime is available. Epoch will download, verify, stage, and hand off the replacement.";
                const std::string_view cacheText = sourceOnlyUpdate
                    ? "Smart Update checked packaged releases first; source rebuild is the available lane for this platform."
                    : "Cached packages are checked before use; stale or broken downloads are replaced.";
                const std::string_view actionText = restartReady
                    ? "The update is staged. Epoch will restart automatically unless Restart Now is pressed first."
                    : sourceWorkerRunning
                        ? "Keep Epoch open while the source worker runs. Cancel stops at the next safe checkpoint."
                        : sourceOnlyUpdate
                            ? "Use Update From Source to build the newer source locally, or Cancel to stay on this build."
                            : "Install Release is recommended. Advanced Source is only for intentionally building latest main locally.";

                float desiredHeight = 54.0f;
                desiredHeight += estimated_wrapped_height(introText, contentWidth) + 8.0f;
                desiredHeight += estimated_wrapped_height(trimmed_status, contentWidth) + 10.0f;
                desiredHeight += 22.0f + 14.0f;
                desiredHeight += estimated_wrapped_height(cacheText, contentWidth) + 8.0f;
                desiredHeight += estimated_wrapped_height(actionText, contentWidth) + 14.0f;
                desiredHeight += 30.0f + 18.0f;

                const float minHeight = sourceWorkerRunning ? 278.0f : restartReady ? 258.0f : 286.0f;
                const float maxDesiredHeight = sourceWorkerRunning ? 372.0f : restartReady ? 348.0f : 372.0f;
                desiredHeight = std::clamp(desiredHeight, minHeight, maxDesiredHeight);
                return fit_modal_size({ modalWidth, desiredHeight }, { 660.0f, minHeight });
            };
        const gui::Vec2 updateConfirmModalSize = update_confirm_modal_size();
        const gui::Vec2 sourceUpdateConfirmModalSize{ 620.0f, 292.0f };
        auto modal_visible_now = [&editor]() noexcept -> bool
        {
            return editor.showAboutModal
                || editor.showSettingsModal
                || editor.showPackageManagerModal
                || editor.showUpdateConfirmModal
                || editor.showSourceUpdateConfirmModal;
        };

        const bool modalVisible = modal_visible_now();
        if (modalVisible)
            editor.openMenu = TopMenu::None;

        const bool overlayPriorityActive =
            editor.openMenu != TopMenu::None
            || modalVisible;
        ctx->set_gui_overlay_priority(overlayPriorityActive);

        gui::clear_modal_input_capture();
        if (editor.showUpdateConfirmModal)
            gui::begin_modal_input_capture(centered_modal_position(updateConfirmModalSize), updateConfirmModalSize);
        else if (editor.showSourceUpdateConfirmModal)
            gui::begin_modal_input_capture(centered_modal_position(sourceUpdateConfirmModalSize), sourceUpdateConfirmModalSize);
        else if (editor.showSettingsModal)
            gui::begin_modal_input_capture(centered_modal_position({ 600.0f, 462.0f }), { 600.0f, 462.0f });
        else if (editor.showPackageManagerModal)
            gui::begin_modal_input_capture(centered_modal_position({ 820.0f, 560.0f }), { 820.0f, 560.0f });
        else if (editor.showAboutModal)
            gui::begin_modal_input_capture(centered_modal_position({ 456.0f, 222.0f }), { 456.0f, 222.0f });

        if (editor.lastLayoutExtent.x > 0.0f
            && editor.lastLayoutExtent.y > 0.0f
            && (std::abs(editor.lastLayoutExtent.x - layoutExtent.x) > 1.0f
                || std::abs(editor.lastLayoutExtent.y - layoutExtent.y) > 1.0f))
        {
            editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
        }
        editor.lastLayoutExtent = layoutExtent;

        auto clamp_layout = [](float value, float lo, float hi) noexcept
        {
            if (hi < lo)
                hi = lo;
            return std::clamp(value, lo, hi);
        };

        editor.outlinerSplit = std::clamp(editor.outlinerSplit, 0.06f, 0.70f);
        editor.inspectorSplit = std::clamp(editor.inspectorSplit, 0.06f, 0.70f);
        editor.dockSplit = std::clamp(editor.dockSplit, 0.08f, 0.80f);

        const bool center_uses_scene = main_surface_uses_scene(editor.mainSurface);
        const bool layout_outliner_visible = editor.showOutliner && center_uses_scene;
        const bool layout_inspector_visible = editor.showInspector;

        const float toolbar_h = 98.0f;
        const float splitter_w = 7.0f;
        const float splitter_h = 7.0f;
        const bool bottom_visible = editor.showConsoleDock || editor.showAiChat;
        const float raw_bottom_h = bottom_visible ? h * editor.dockSplit : 0.0f;
        const float bottom_h = bottom_visible
            ? clamp_layout(raw_bottom_h, (std::min)(96.0f, h * 0.22f), (std::max)(96.0f, h * 0.82f))
            : 0.0f;
        const float bottom_split_h = bottom_visible ? splitter_h : 0.0f;

        const float left_min = (std::min)(140.0f, (std::max)(0.0f, w * 0.20f));
        const float left_max = (std::max)(left_min, (std::min)(960.0f, w * 0.72f));
        const float right_min = (std::min)(160.0f, (std::max)(0.0f, w * 0.22f));
        const float right_max = (std::max)(right_min, (std::min)(960.0f, w * 0.72f));
        const float left_w = layout_outliner_visible ? clamp_layout(w * editor.outlinerSplit, left_min, left_max) : 0.0f;
        const float right_w = layout_inspector_visible ? clamp_layout(w * editor.inspectorSplit, right_min, right_max) : 0.0f;
        const float left_split_w = layout_outliner_visible ? splitter_w : 0.0f;
        const float right_split_w = layout_inspector_visible ? splitter_w : 0.0f;

        const gui::Vec2 toolbar_pos{ 0.0f, 0.0f };
        const gui::Vec2 toolbar_size{ w, toolbar_h };

        const gui::Vec2 bottom_split_pos{ 0.0f, (std::max)(0.0f, h - bottom_h - bottom_split_h) };
        const gui::Vec2 bottom_split_size{ w, bottom_split_h };
        const gui::Vec2 bottom_pos{ 0.0f, (std::max)(0.0f, h - bottom_h) };
        const gui::Vec2 bottom_size{ w, bottom_h };

        const float main_y = toolbar_h;
        const float main_h = (std::max)(0.0f, h - toolbar_h - bottom_h - bottom_split_h);

        const gui::Vec2 outliner_pos{ 0.0f, main_y };
        const gui::Vec2 outliner_size{ left_w, main_h };
        const gui::Vec2 outliner_split_pos{ left_w, main_y };
        const gui::Vec2 outliner_split_size{ left_split_w, main_h };

        const gui::Vec2 details_pos{ (std::max)(0.0f, w - right_w), main_y };
        const gui::Vec2 details_size{ right_w, main_h };
        const gui::Vec2 inspector_split_pos{ (std::max)(0.0f, details_pos.x - right_split_w), main_y };
        const gui::Vec2 inspector_split_size{ right_split_w, main_h };

        const gui::Vec2 viewport_pos{ left_w + left_split_w, main_y };
        const gui::Vec2 viewport_size{
            (std::max)(0.0f, w - left_w - left_split_w - right_w - right_split_w),
            main_h
        };

        auto emit_command = [&](EditorCommand command, std::string_view argument = {})
        {
            result.command = command;
            result.command_argument.assign(argument.begin(), argument.end());
        };

        auto submit_ai_prompt = [&](std::string prompt, std::string_view logLine)
        {
            if (chat.pending)
            {
                push_editor_log(editor, "[ai] Chat is still processing the previous request.");
                return;
            }

            chat.submit(std::move(prompt));
            push_editor_log(editor, std::string(logLine));
        };

        auto play_active_context = [&]()
        {
            save_editor_scene_snapshot(editor);
            const std::string sceneId =
                project_runtime_scene_id(editor)
                + "|camera="
                + std::string(project_camera_argument(editor.projectCameraMode))
                + "|input="
                + std::string(input_profile_argument(editor.inputProfilePreset));
            if (ctx)
                previewgrid::set_camera_mode(ctx.get(), editor.projectCameraMode);
            editor.projectStatus = std::string("Play In Editor requested using saved editor scene snapshot: ") + sceneId;
            push_editor_log(editor, std::string("[project] Play In Editor requested for ") + editor.projectName + " using saved scene snapshot: " + sceneId);
            append_project_note(
                editor,
                "Play In Editor",
                "Running the saved active project scene snapshot inside the editor without launching a child process.",
                sceneId);
            emit_command(EditorCommand::RunGame, sceneId);
        };

        auto launch_active_project_context = [&]()
        {
            const bool selectedScriptAsset = !editor.selectedAssetPath.empty()
                && std::filesystem::path{ editor.selectedAssetPath }.filename().string().ends_with(".ascript.cpp");
            if (editor.workspaceTab == EditorWorkspaceTab::Scripts
                || (editor.workspaceTab == EditorWorkspaceTab::Assets && selectedScriptAsset))
            {
                if (selectedScriptAsset)
                    editor.activeScript = script_id_from_source_path(std::filesystem::path{ editor.selectedAssetPath });
                editor.scriptBuildStatus = "Selected script assets build through Build Selected Script; Run builds and launches the active project.";
                push_editor_log(editor, "[script] Center Run kept on active project. Use Build Selected Script for script asset validation.");
                append_project_note(
                    editor,
                    "Script Run Routed To Project",
                    std::string("Script asset selected: ") + editor.activeScript + ".",
                    "Project launch remains project-owned so script assets cannot crash the project run path by accident.");
            }

            save_editor_scene_snapshot(editor);
            repair_active_project_evidence(editor);

            const ProjectBuildFreshness freshness = project_child_build_freshness(editor);
            if (freshness.rebuildRequired)
            {
                start_project_build(editor, true, std::string("Launch Single Context - ") + freshness.reason);
                return;
            }

            const std::filesystem::path outputExe = project_existing_output_exe_path(editor.projectRoot);
            if (!path_exists(outputExe))
            {
                editor.projectBuildStatus = "Launch blocked: child executable disappeared before launch.";
                push_editor_log(editor, "[project] Launch blocked: child executable disappeared before launch.");
                return;
            }

            const std::string sceneId = project_runtime_scene_id(editor);
            const std::string runBackend = editor.projectRunBackend.empty() ? std::string("opengl") : editor.projectRunBackend;
            const std::string playTarget =
                std::string("project-exe:") + display_project_path(outputExe)
                + "|scene=" + sceneId
                + "|backend=" + runBackend
                + "|fps=" + std::string(frame_limit_argument(editor.projectRunFrameLimitFps))
                + "|camera=" + std::string(project_camera_argument(editor.projectCameraMode))
                + "|input=" + std::string(input_profile_argument(editor.inputProfilePreset));

            editor.projectBuildStatus = "Launching existing child executable; build inputs are current.";
            emit_command(EditorCommand::RunGame, playTarget);
            push_editor_log(
                editor,
                std::string("[project] Launching current child executable in standalone ")
                + std::string(project_run_backend_label(runBackend))
                + " at " + std::string(frame_limit_label(editor.projectRunFrameLimitFps))
                + ": " + sceneId);
            append_project_note(
                editor,
                "Launch Single Context",
                std::string("Launching current child executable without rebuilding in standalone ")
                + std::string(project_run_backend_label(runBackend)) + ".",
                playTarget);
        };

        auto poll_project_build = [&]()
        {
            if (!editor.projectBuildPending
                || editor.projectBuildPending->wait_for(0ms) != std::future_status::ready)
                return;

            try
            {
                const auto build = editor.projectBuildPending->get();
                editor.projectBuildPending.reset();
                editor.projectBuildStatus = build.summary;
                push_editor_log(
                    editor,
                    std::string("[project] ")
                    + (build.succeeded ? "Build passed. " : "Build failed. ")
                    + build.summary);
                if (!build.output_path.empty())
                    push_editor_log(editor, std::string("[project] Output: ") + build.output_path);
                if (!build.log_path.empty())
                    push_editor_log(editor, std::string("[project] Log: ") + build.log_path);

                append_project_note(
                    editor,
                    "Project Build Completed",
                    build.summary,
                    build.succeeded ? "Project build evidence is available." : "Project build failed; inspect the build log before retrying.");

                const bool shouldRun = editor.projectBuildRunAfterBuild;
                const std::string outputPath = build.output_path.empty() ? editor.projectBuildOutputPath : build.output_path;
                std::filesystem::path resolvedOutputPath = outputPath.empty()
                    ? project_existing_output_exe_path(editor.projectRoot)
                    : resolve_editor_path(std::filesystem::path{ outputPath });
                if (!path_exists(resolvedOutputPath))
                    resolvedOutputPath = project_existing_output_exe_path(editor.projectRoot);
                const std::string launchOutputPath = display_project_path(resolvedOutputPath);
                const std::string sceneId = editor.projectBuildRunScene.empty() ? project_runtime_scene_id(editor) : editor.projectBuildRunScene;
                const std::string runBackend = editor.projectBuildRunBackend.empty() ? std::string("opengl") : editor.projectBuildRunBackend;
                const double runFrameLimit = editor.projectBuildRunFrameLimitFps;
                const auto runCameraMode = editor.projectBuildRunCameraMode;
                const auto runInputProfile = editor.projectBuildRunInputProfile;

                editor.projectBuildRunAfterBuild = false;
                editor.projectBuildRunScene.clear();
                editor.projectBuildOutputPath.clear();
                editor.projectBuildRunBackend.clear();
                editor.projectBuildRunFrameLimitFps = editor.projectRunFrameLimitFps;
                editor.projectBuildRunCameraMode = editor.projectCameraMode;
                editor.projectBuildRunInputProfile = editor.inputProfilePreset;

                if (!shouldRun)
                    return;

                if (!build.succeeded)
                {
                    push_editor_log(editor, "[project] Run canceled because the project build failed.");
                    return;
                }

                const bool hasBuiltOutput = path_exists(resolvedOutputPath);
                if (!hasBuiltOutput)
                {
                    editor.projectBuildStatus = "Launch blocked: built child executable is missing after build.";
                    push_editor_log(
                        editor,
                        "[project] Launch blocked: built child executable is missing; refusing parent multicontext fallback.");
                    append_project_note(
                        editor,
                        "Launch Single Context Blocked",
                        "The built child executable was missing after build; parent multicontext fallback was refused.",
                        launchOutputPath.empty() ? std::string("(missing output path)") : launchOutputPath);
                    return;
                }

                const std::string playTarget =
                    std::string("project-exe:") + launchOutputPath
                    + "|scene=" + sceneId
                    + "|backend=" + runBackend
                    + "|fps=" + std::string(frame_limit_argument(runFrameLimit))
                    + "|camera=" + std::string(project_camera_argument(runCameraMode))
                    + "|input=" + std::string(input_profile_argument(runInputProfile));
                emit_command(EditorCommand::RunGame, playTarget);
                push_editor_log(editor, std::string("[project] Single-context launch requested for ") + editor.projectName + ".");
                push_editor_log(
                    editor,
                    std::string("[project] Launching built child executable in standalone ")
                    + std::string(project_run_backend_label(runBackend))
                    + " at " + std::string(frame_limit_label(runFrameLimit))
                    + ": " + sceneId);
                append_project_note(
                    editor,
                    "Launch Single Context",
                    std::string("Launching built child executable in standalone ")
                    + std::string(project_run_backend_label(runBackend))
                    + " at " + std::string(frame_limit_label(runFrameLimit)) + ".",
                    playTarget);
            }
            catch (const std::exception& e)
            {
                editor.projectBuildPending.reset();
                editor.projectBuildRunAfterBuild = false;
                editor.projectBuildRunScene.clear();
                editor.projectBuildOutputPath.clear();
                editor.projectBuildRunBackend.clear();
                editor.projectBuildRunCameraMode = editor.projectCameraMode;
                editor.projectBuildRunInputProfile = editor.inputProfilePreset;
                editor.projectBuildStatus = std::string("Project build threw: ") + e.what();
                push_editor_log(editor, "[project] Build threw: " + std::string(e.what()));
            }
        };

        poll_project_build();

        auto apply_editor_surface = [&](EditorMainSurface surface, std::string_view source)
        {
            const bool changedSurface = editor.mainSurface != surface;
            editor.mainSurface = surface;
            if (changedSurface)
                editor.surfaceSettleFrames = 1;

            switch (surface)
            {
            case EditorMainSurface::Scene:
                editor.showOutliner = true;
                editor.showInspector = true;
                editor.showConsoleDock = true;
                editor.showAiChat = true;
                editor.previewMode = core::ScenePreviewMode::Editor;
                if (editor.projectCameraMode == previewgrid::CameraMode::Canvas2D)
                    editor.projectCameraMode = previewgrid::CameraMode::Editor;
                if (ctx)
                {
                    const auto currentCameraMode = epochnamespace::previewgrid::camera_mode_for(ctx.get());
                    if (currentCameraMode == epochnamespace::previewgrid::CameraMode::Canvas2D)
                        epochnamespace::previewgrid::set_camera_mode(ctx.get(), editor.projectCameraMode);
                }
                push_editor_log(editor, std::string("[editor] Scene workbench opened from ") + std::string(source) + ".");
                break;
            case EditorMainSurface::Game2D:
                editor.showOutliner = true;
                editor.showInspector = true;
                editor.showConsoleDock = true;
                editor.showAiChat = true;
                editor.workspaceTab = EditorWorkspaceTab::Project;
                editor.previewMode = core::ScenePreviewMode::Editor;
                editor.projectCameraMode = previewgrid::CameraMode::Canvas2D;
                ensure_2d_canvas_entity(editor);
                if (ctx)
                {
                    epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::Canvas2D);
                    epochnamespace::previewgrid::reset_camera(ctx.get());
                }
                push_editor_log(editor, "[editor] 2D Scene/UI opened with the locked Canvas2D camera.");
                break;
            case EditorMainSurface::Assets:
                editor.showInspector = true;
                editor.showConsoleDock = true;
                editor.showAiChat = true;
                editor.workspaceTab = EditorWorkspaceTab::Assets;
                push_editor_log(editor, "[assets] Asset Browser opened.");
                break;
            case EditorMainSurface::Project:
                editor.showInspector = true;
                editor.showConsoleDock = true;
                editor.showAiChat = true;
                editor.workspaceTab = EditorWorkspaceTab::Project;
                push_editor_log(editor, "[project] Project Workspace opened.");
                break;
            case EditorMainSurface::ForestFactory:
                editor.showOutliner = true;
                editor.showInspector = true;
                editor.showConsoleDock = true;
                editor.showAiChat = true;
                editor.workspaceTab = EditorWorkspaceTab::Assets;
                editor.previewMode = core::ScenePreviewMode::Editor;
                editor.projectCameraMode = previewgrid::CameraMode::Editor;
                ensure_forest_factory_preview_entities(editor);
                if (ctx)
                {
                    epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::Editor);
                    epochnamespace::previewgrid::reset_camera(ctx.get());
                }
                push_editor_log(editor, "[forest] Plant Lab opened with the scene-backed Forest Factory preview.");
                break;
            case EditorMainSurface::Timeline:
                editor.showInspector = true;
                editor.showConsoleDock = true;
                editor.showAiChat = true;
                editor.workspaceTab = EditorWorkspaceTab::Output;
                epoch::saveload::clamp_streaming_save_config(editor.streamingSaveConfig);
                push_editor_log(editor, "[timeline] Video opened on the shared 4D time spine.");
                break;
            case EditorMainSurface::AISandbox:
                editor.workspaceTab = EditorWorkspaceTab::AI;
                editor.aiWorkspaceDomain = AiWorkspaceDomain::Control;
                editor.showConsoleDock = true;
                editor.showInspector = true;
                editor.showAiChat = true;
                activate_self_iteration_sandbox(editor, ctx, true);
                push_editor_log(editor, "[ai] AI Control Surface opened with Inspector and AI Chat visible.");
                break;
            case EditorMainSurface::Systems:
                editor.showInspector = true;
                editor.showConsoleDock = true;
                editor.showAiChat = true;
                editor.workspaceTab = EditorWorkspaceTab::Systems;
                push_editor_log(editor, "[systems] System Info opened.");
                break;
            default:
                break;
            }
        };

        auto open_editor_surface = [&](EditorMainSurface surface, std::string_view source)
        {
            apply_editor_surface(surface, source);
        };

        auto render_main_surface_tabs = [&]()
        {
            const std::array<gui::SegmentedButtonSpec, 8> tabs{{
                { "3D Scene", 96.0f, editor.mainSurface == EditorMainSurface::Scene },
                { "2D Scene/UI", 116.0f, editor.mainSurface == EditorMainSurface::Game2D },
                { "Assets", 74.0f, editor.mainSurface == EditorMainSurface::Assets },
                { "Plant Lab", 92.0f, editor.mainSurface == EditorMainSurface::ForestFactory },
                { "Video", 70.0f, editor.mainSurface == EditorMainSurface::Timeline },
                { "Project", 84.0f, editor.mainSurface == EditorMainSurface::Project },
                { "Intelligence", 116.0f, editor.mainSurface == EditorMainSurface::AISandbox },
                { "System Info", 108.0f, editor.mainSurface == EditorMainSurface::Systems }
            }};
            const std::array<EditorMainSurface, 8> surfaces{{
                EditorMainSurface::Scene,
                EditorMainSurface::Game2D,
                EditorMainSurface::Assets,
                EditorMainSurface::ForestFactory,
                EditorMainSurface::Timeline,
                EditorMainSurface::Project,
                EditorMainSurface::AISandbox,
                EditorMainSurface::Systems
            }};

            if (const auto selected = gui::tab_bar(tabs, 28.0f, 5.0f))
                open_editor_surface(surfaces[*selected], "center tabs");
        };

        gui::begin_window("", toolbar_pos, toolbar_size);
        const float toolbar_button_y = toolbar_pos.y + 10.0f;
        const float toolbar_button_h = 24.0f;
        float toolbar_x = toolbar_pos.x + 88.0f;

        struct TopMenuButton
        {
            TopMenu menu{};
            std::string_view label{};
            float width{};
            float x{};
        };

        std::array<TopMenuButton, 6> topMenus{{
            { TopMenu::File, "File", 68.0f, 0.0f },
            { TopMenu::Edit, "Edit", 68.0f, 0.0f },
            { TopMenu::Asset, "Asset", 76.0f, 0.0f },
            { TopMenu::Window, "Window", 92.0f, 0.0f },
            { TopMenu::Tools, "Tools", 76.0f, 0.0f },
            { TopMenu::Help, "Help", 72.0f, 0.0f }
        }};

        gui::set_cursor({ 16.0f, toolbar_pos.y + 14.0f });
        gui::label("Epoch");

        const gui::Vec2 toolbarMouse = gui::mouse_position();
        const bool menusBlockedByModal = modal_visible_now();
        for (auto& item : topMenus)
        {
            item.x = toolbar_x;
            gui::set_cursor({ toolbar_x, toolbar_button_y });
            if (!menusBlockedByModal
                && editor_point_in_rect(toolbarMouse, { toolbar_x, toolbar_button_y }, { item.width, toolbar_button_h }))
                editor.openMenu = item.menu;
            if (!menusBlockedByModal
                && gui::button_selected(item.label, { item.width, toolbar_button_h }, editor.openMenu == item.menu))
                editor.openMenu = item.menu;
            toolbar_x += item.width + 6.0f;
        }

        const float run_button_w = 108.0f;
        const float run_button_x = (std::max)(toolbar_x + 12.0f, viewport_pos.x + (viewport_size.x - run_button_w) * 0.5f);
        gui::set_cursor({ run_button_x, toolbar_button_y });
        if (gui::button("Run", { run_button_w, toolbar_button_h }))
        {
            if (editor.projectKind == "Engine Self-Iteration")
                play_active_context();
            else
                launch_active_project_context();
        }

        float status_anchor_x = run_button_x + run_button_w + 14.0f;
        if (editor.updateState == EditorUpdateState::Available
            || editor.updateState == EditorUpdateState::RestartReady)
        {
            constexpr float update_button_w = 178.0f;
            gui::set_cursor({ status_anchor_x, toolbar_button_y });
            const bool updateButtonActive = editor.showUpdateConfirmModal
                || editor.showSourceUpdateConfirmModal
                || editor.updateInstallPending;
            if (gui::button_selected(update_toolbar_button_label(editor.updateState), { update_button_w, toolbar_button_h }, updateButtonActive))
            {
                if (editor.updateState == EditorUpdateState::RestartReady)
                {
                    editor.updateStatus = "Update handoff has already started. Restart Epoch to let the verified replacement finish.";
                    editor.showUpdateConfirmModal = true;
                    editor.showSourceUpdateConfirmModal = false;
                    push_editor_log(editor, "[update] Restart is ready after verified update handoff.");
                }
                else
                {
                    editor.openMenu = TopMenu::None;
                    editor.showUpdateConfirmModal = true;
                    editor.showSourceUpdateConfirmModal = false;
                    push_editor_log(editor, "[update] Update available. Awaiting confirmation.");
                }
            }
            status_anchor_x += update_button_w + 10.0f;
        }

        const std::size_t toolbarThreadCount = epoch::systems::threading::live_thread_count();
        const std::size_t toolbarCpuThreadCount = (std::max)(std::size_t{ 1 },
            std::thread::hardware_concurrency() > 0
            ? static_cast<std::size_t>(std::thread::hardware_concurrency())
            : std::size_t{ 1 });
        const float status_x = (std::max)(toolbar_x + 12.0f, status_anchor_x);
        gui::set_cursor({ status_x, toolbar_button_y + 4.0f });
        gui::wrapped_label(
            std::string("v") + epochnamespace::GetEngineVersionString()
            + "  |  " + epochnamespace::GetEngineBuildTagString()
            + "  |  Threads " + std::to_string(toolbarThreadCount)
            + "/" + std::to_string(toolbarCpuThreadCount)
            + "  |  " + renderer_name(ctx)
            + "  |  Zoom " + preview_zoom_text(ctx),
            (std::max)(180.0f, w - status_x - 12.0f));

        const float tab_y = toolbar_pos.y + 48.0f;
        const float tab_h = 34.0f;
        const float tab_gap = 8.0f;
        float tab_x = 16.0f;

        const std::string editor_tab = "3D Scene";
        const std::string runtime_tab = "2D Scene/UI";
        const std::string assets_tab = "Assets";
        const std::string forest_tab = "Plant Lab";
        const std::string timeline_tab = "Video";
        const std::string project_tab = "Project";
        const std::string ai_control_tab = "Intelligence";
        const std::string systems_tab = "System Info";

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(editor_tab, { 136.0f, tab_h }, editor.mainSurface == EditorMainSurface::Scene))
            open_editor_surface(EditorMainSurface::Scene, "toolbar");
        tab_x += 136.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(runtime_tab, { 142.0f, tab_h }, editor.mainSurface == EditorMainSurface::Game2D))
            open_editor_surface(EditorMainSurface::Game2D, "toolbar");
        tab_x += 142.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(assets_tab, { 104.0f, tab_h }, editor.mainSurface == EditorMainSurface::Assets))
            open_editor_surface(EditorMainSurface::Assets, "toolbar");
        tab_x += 104.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(forest_tab, { 116.0f, tab_h }, editor.mainSurface == EditorMainSurface::ForestFactory))
            open_editor_surface(EditorMainSurface::ForestFactory, "toolbar");
        tab_x += 116.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(timeline_tab, { 92.0f, tab_h }, editor.mainSurface == EditorMainSurface::Timeline))
            open_editor_surface(EditorMainSurface::Timeline, "toolbar");
        tab_x += 92.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(project_tab, { 112.0f, tab_h }, editor.mainSurface == EditorMainSurface::Project))
            open_editor_surface(EditorMainSurface::Project, "toolbar");
        tab_x += 112.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(ai_control_tab, { 142.0f, tab_h }, editor.mainSurface == EditorMainSurface::AISandbox))
            open_editor_surface(EditorMainSurface::AISandbox, "toolbar");
        tab_x += 142.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button_selected(systems_tab, { 132.0f, tab_h }, editor.mainSurface == EditorMainSurface::Systems))
            open_editor_surface(EditorMainSurface::Systems, "toolbar");

        gui::end_window();

        auto dropdown_position_for = [&](TopMenu menu) -> gui::Vec2
        {
            for (const auto& item : topMenus)
                if (item.menu == menu)
                    return { item.x, toolbar_button_y + toolbar_button_h + 6.0f };
            return { 14.0f, toolbar_button_y + toolbar_button_h + 6.0f };
        };

        auto menu_item = [&](std::string_view title, gui::Vec2 pos, float width, auto&& on_click)
        {
            gui::set_cursor(pos);
            if (gui::button(title, { width, 28.0f }))
            {
                editor.openMenu = TopMenu::None;
                on_click();
            }
        };

        auto dropdown_window_size = [&](float button_width, int item_count) -> gui::Vec2
        {
            constexpr float kDropdownInnerLeft = 12.0f;
            constexpr float kDropdownInnerTop = 14.0f;
            constexpr float kDropdownInnerBottom = 14.0f;
            constexpr float kDropdownItemHeight = 28.0f;
            constexpr float kDropdownItemPitch = 34.0f;

            const int clamped_items = (std::max)(0, item_count);
            const float content_height =
                clamped_items <= 0
                ? (kDropdownInnerTop + kDropdownInnerBottom)
                : (kDropdownInnerTop
                    + kDropdownItemHeight
                    + static_cast<float>(clamped_items - 1) * kDropdownItemPitch
                    + kDropdownInnerBottom);

            return {
                button_width + kDropdownInnerLeft * 2.0f,
                content_height
            };
        };

        auto top_menu_button_bounds = [&](TopMenu menu) -> std::optional<gui::WidgetBounds>
        {
            for (const auto& item : topMenus)
            {
                if (item.menu == menu)
                    return gui::WidgetBounds{
                        .position = { item.x, toolbar_button_y },
                        .size = { item.width, toolbar_button_h }
                    };
            }
            return std::nullopt;
        };

        auto dropdown_size_for = [&](TopMenu menu) -> gui::Vec2
        {
            switch (menu)
            {
            case TopMenu::File: return dropdown_window_size(192.0f, 4);
            case TopMenu::Edit: return dropdown_window_size(192.0f, 3);
            case TopMenu::Asset: return dropdown_window_size(220.0f, 7);
            case TopMenu::Window: return dropdown_window_size(248.0f, 5);
            case TopMenu::Tools: return dropdown_window_size(228.0f, 5);
            case TopMenu::Help: return dropdown_window_size(192.0f, 2);
            case TopMenu::None:
            default: return {};
            }
        };

        auto open_dropdown = [&](std::string_view title, TopMenu menu, gui::Vec2 size, auto&& body)
        {
            if (modal_visible_now())
                return;
            if (editor.openMenu != menu)
                return;
            const auto pos = dropdown_position_for(menu);
            gui::begin_top_layer();
            (void)title;
            gui::begin_window({}, pos, size);
            body(gui::cursor_position());
            gui::end_window();
            gui::end_top_layer();
        };

        const gui::Vec2 mouse = toolbarMouse;
        if (gui::was_mouse_pressed())
        {
            if (layout_outliner_visible && editor_point_in_rect(mouse, outliner_split_pos, outliner_split_size))
                editor.layoutDrag = EditorLayoutDrag::Outliner;
            else if (layout_inspector_visible && editor_point_in_rect(mouse, inspector_split_pos, inspector_split_size))
                editor.layoutDrag = EditorLayoutDrag::Inspector;
            else if (bottom_visible && editor_point_in_rect(mouse, bottom_split_pos, bottom_split_size))
                editor.layoutDrag = EditorLayoutDrag::Dock;
        }
        if (!gui::is_mouse_down())
            editor.layoutDrag = EditorLayoutDrag::None;

        if (gui::is_mouse_down())
        {
            switch (editor.layoutDrag)
            {
            case EditorLayoutDrag::Outliner:
                editor.outlinerSplit = std::clamp(mouse.x / (std::max)(1.0f, w), 0.06f, 0.70f);
                editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
                break;
            case EditorLayoutDrag::Inspector:
                editor.inspectorSplit = std::clamp((w - mouse.x) / (std::max)(1.0f, w), 0.06f, 0.70f);
                editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
                break;
            case EditorLayoutDrag::Dock:
                editor.dockSplit = std::clamp((h - mouse.y) / (std::max)(1.0f, h), 0.08f, 0.80f);
                editor.surfaceSettleFrames = (std::max)(editor.surfaceSettleFrames, 1);
                break;
            case EditorLayoutDrag::None:
            default:
                break;
            }
        }

        if (modal_visible_now())
            editor.openMenu = TopMenu::None;

        if (editor.openMenu != TopMenu::None)
        {
            const auto buttonBounds = top_menu_button_bounds(editor.openMenu);
            const gui::Vec2 dropdownPos = dropdown_position_for(editor.openMenu);
            const gui::Vec2 dropdownSize = dropdown_size_for(editor.openMenu);
            const bool inButton = buttonBounds
                && editor_point_in_rect(mouse, buttonBounds->position, buttonBounds->size);
            const bool inDropdown = dropdownSize.x > 0.0f
                && dropdownSize.y > 0.0f
                && editor_point_in_rect(mouse, dropdownPos, dropdownSize);
            const gui::Vec2 bridgePos{
                buttonBounds ? buttonBounds->position.x : dropdownPos.x,
                toolbar_button_y + toolbar_button_h
            };
            const gui::Vec2 bridgeSize{
                (std::max)(dropdownSize.x, buttonBounds ? buttonBounds->size.x : 0.0f),
                8.0f
            };
            const bool inBridge = editor_point_in_rect(mouse, bridgePos, bridgeSize);
            if (!inButton && !inDropdown && !inBridge && !gui::is_mouse_down())
                editor.openMenu = TopMenu::None;
        }

        auto render_titlebar_close = [&](gui::Vec2 panel_pos, gui::Vec2 panel_size, auto&& close_handler)
        {
            if (gui::titlebar_close_button(panel_pos, panel_size))
                close_handler();
        };

        auto render_outliner_window = [&]()
        {
        if (layout_outliner_visible && outliner_size.x > 1.0f && outliner_size.y > 1.0f)
        {
        gui::begin_window("World Outliner", outliner_pos, outliner_size);
        render_titlebar_close(outliner_pos, outliner_size, [&]() {
            editor.showOutliner = false;
            push_editor_log(editor, "[ui] World Outliner hidden. Reopen it from Window > Toggle Outliner.");
        });
        const gui::Vec2 outlinerScrollStart = gui::cursor_position();
        const float outlinerScrollHeight = (std::max)(
            48.0f,
            outliner_pos.y + outliner_size.y - outlinerScrollStart.y - 4.0f);
        (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
            .id = "world-outliner-body",
            .size = { (std::max)(80.0f, outliner_size.x - 12.0f), outlinerScrollHeight },
            .draw_background = true,
            .show_scrollbar = true
        });
        const float outlinerWidth = (std::max)(150.0f, outliner_size.x - 18.0f);
        gui::label(std::string("Scene: ") + editor.activeWorld);
        gui::label(std::string("Entities: ") + std::to_string(editor.entities.size()));
        gui::property_row("[outliner] Root", display_project_path(editor.projectRoot), 84.0f);
        gui::property_row("[outliner] Scene file", file_ready_summary(editor.projectScenePath), 104.0f);

        const std::array outlinerAddButtons{
            gui::InlineButtonSpec{ .label = "+ Cube", .width = 72.0f },
            gui::InlineButtonSpec{ .label = "+ Light", .width = 72.0f },
            gui::InlineButtonSpec{ .label = "+ Spawn", .width = 78.0f },
            gui::InlineButtonSpec{ .label = "+ Camera", .width = 88.0f }
        };
        if (const auto action = gui::inline_button_row(outlinerAddButtons, 26.0f, 5.0f))
        {
            switch (*action)
            {
            case 0: add_entity(editor, "cube"); break;
            case 1: add_entity(editor, "light"); break;
            case 2: add_entity(editor, "spawn"); break;
            case 3: add_entity(editor, "camera"); break;
            default: break;
            }
        }

        const std::array outlinerEditButtons{
            gui::InlineButtonSpec{ .label = "Duplicate", .width = 96.0f },
            gui::InlineButtonSpec{ .label = "Delete", .width = 74.0f },
            gui::InlineButtonSpec{ .label = "Focus", .width = 70.0f }
        };
        if (const auto action = gui::inline_button_row(outlinerEditButtons, 26.0f, 5.0f))
        {
            switch (*action)
            {
            case 0: duplicate_selected_entity(editor); break;
            case 1: delete_selected_entity(editor); break;
            case 2: handle_scene_tool(editor, "focus_selection"); break;
            default: break;
            }
        }

        for (std::size_t i = 0; i < editor.entities.size(); ++i)
        {
            const auto& entity = editor.entities[i];
            std::string label = (i == editor.selectedEntity ? "> " : "") + entity.name + "  |  " + entity.type + "  |  " + entity.category;
            if (!entity.visible)
                label += " (hidden)";
            if (gui::button(label, { outlinerWidth, 28.0f }))
            {
                editor.selectedEntity = i;
                push_editor_log(editor, std::string("[select] ") + entity.name);
            }
        }
        gui::end_scroll_area();
        gui::end_window();
        }
        };

        auto render_inspector_window = [&]()
        {
        if (layout_inspector_visible && details_size.x > 1.0f && details_size.y > 1.0f)
        {
        gui::begin_window("Inspector", details_pos, details_size);
        render_titlebar_close(details_pos, details_size, [&]() {
            editor.showInspector = false;
            push_editor_log(editor, "[ui] Inspector hidden. Reopen it from Window > Toggle Inspector.");
        });
        const gui::Vec2 inspectorScrollStart = gui::cursor_position();
        const float inspectorScrollHeight = (std::max)(
            48.0f,
            details_pos.y + details_size.y - inspectorScrollStart.y - 4.0f);
        (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
            .id = editor.workspaceTab == EditorWorkspaceTab::AI ? "inspector-ai-body" : "inspector-scene-body",
            .size = { (std::max)(80.0f, details_size.x - 12.0f), inspectorScrollHeight },
            .draw_background = true,
            .show_scrollbar = true
        });
        if (editor.workspaceTab == EditorWorkspaceTab::AI)
        {
            const auto inspectorTraining = epoch::ai::default_training_paths();
            const std::filesystem::path inspectorBuildLog = project_build_log_path(editor.projectRoot);
            const std::filesystem::path inspectorOutputExe = project_output_exe_path(editor.projectRoot);
            const std::filesystem::path inspectorPathsManifest = resolve_editor_path(std::filesystem::path{ editor.projectRoot }) / "project.paths.txt";
            const std::string inspectorActiveScriptSource = editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            const std::string inspectorLatestPrompt = last_chat_line_with_prefix(chat, "you> ");
            const std::string inspectorLatestReply = last_chat_line_with_prefix(chat, "ai> ");
            const auto inspectorManifest = epoch::ai::active_model_manifest();
            const auto inspectorGate = summarize_ai_review_gate(
                editor,
                inspectorBuildLog,
                inspectorOutputExe,
                inspectorPathsManifest,
                inspectorTraining,
                inspectorLatestPrompt,
                inspectorLatestReply);
            const std::string inspectorStage = ai_control_loop_stage(inspectorGate);
            const float inspectorWidth = (std::max)(180.0f, details_size.x - 24.0f);

            auto inspectorMcpRecord = [&]() {
                return epoch::ai::McpCaptureRecord{
                    .server = "editor",
                    .tool = "self-iteration-guidance",
                    .prompt = build_ai_self_iteration_prompt(editor),
                    .normalized_output = epoch::ai::active_provider_summary(),
                    .source_path = editor.projectScenePath.empty() ? editor.projectRoot : editor.projectScenePath
                };
            };

            auto inspectorIterationPacket = [&]() {
                std::vector<std::string> evidencePaths;
                auto addEvidence = [&](const std::string& path) {
                    if (path.empty())
                        return;
                    if (std::find(evidencePaths.begin(), evidencePaths.end(), path) == evidencePaths.end())
                        evidencePaths.push_back(path);
                };

                addEvidence(editor.projectManifest);
                addEvidence(editor.projectRoot);
                addEvidence(editor.projectScenePath);
                addEvidence(inspectorActiveScriptSource);
                addEvidence(inspectorPathsManifest.string());
                addEvidence(inspectorBuildLog.string());
                addEvidence(inspectorOutputExe.string());
                addEvidence(inspectorTraining.local_capture_jsonl);
                addEvidence(inspectorTraining.mcp_capture_jsonl);
                addEvidence(inspectorTraining.curated_dataset_root);
                addEvidence(inspectorTraining.eval_root);
                addEvidence(inspectorManifest.manifest_path);

                return epoch::ai::IterationPacket{
                    .packet_name = editor.projectId.empty() ? std::string("epoch-iteration") : editor.projectId + "-iteration",
                    .task_prompt = inspectorLatestPrompt.empty() ? build_ai_self_iteration_prompt(editor) : inspectorLatestPrompt,
                    .assistant_hint = inspectorLatestReply == "(empty reply)" ? std::string{} : inspectorLatestReply,
                    .operator_notes = editor.projectBuildStatus.empty()
                        ? editor.projectStatus
                        : (editor.projectStatus.empty()
                            ? editor.projectBuildStatus
                            : editor.projectBuildStatus + " | " + editor.projectStatus),
                    .control_loop_stage = inspectorStage,
                    .review_gate_state = inspectorGate.promotionSummary,
                    .review_gate_evidence = inspectorGate.packetEvidenceSummary,
                    .project_id = editor.projectId,
                    .project_name = editor.projectName,
                    .scene_id = editor.activeRuntimeScene,
                    .project_root = editor.projectRoot,
                    .scene_path = editor.projectScenePath,
                    .active_script = editor.activeScript,
                    .build_log_path = inspectorBuildLog.string(),
                    .output_path = inspectorOutputExe.string(),
                    .provider_summary = epoch::ai::active_provider_summary(),
                    .active_model = inspectorManifest.display_name,
                    .manifest_path = inspectorManifest.manifest_path,
                    .workspace_root = inspectorTraining.workspace_root,
                    .raw_capture_path = inspectorTraining.local_capture_jsonl,
                    .mcp_capture_path = inspectorTraining.mcp_capture_jsonl,
                    .checkpoint_root = inspectorTraining.checkpoint_root,
                    .model_root = inspectorTraining.model_root,
                    .cache_root = inspectorTraining.cache_root,
                    .curated_dataset_root = inspectorTraining.curated_dataset_root,
                    .eval_root = inspectorTraining.eval_root,
                    .evidence_paths = std::move(evidencePaths)
                };
            };

            auto stageInspectorPacket = [&](std::string_view successPrefix, std::string_view noteTitle, std::string_view noteBody) {
                const std::string packetDir = epoch::ai::stage_iteration_packet(inspectorIterationPacket());
                if (packetDir.empty())
                {
                    push_editor_log(editor, "[ai] Failed to stage iteration packet.");
                    return std::string{};
                }

                push_editor_log(editor, std::string(successPrefix));
                push_editor_log(editor, std::string("[ai] Iteration packet path: ") + packetDir);
                append_project_note(
                    editor,
                    std::string(noteTitle),
                    std::string("Packet staged at ") + packetDir,
                    std::string(noteBody));
                return packetDir;
            };

            gui::label("OS AI Inspector Controls");
            gui::wrapped_label(
                "OS model selection, evidence packets, and manual build gates live here. Bottom Dock > AI is status-only.",
                inspectorWidth);
            render_ai_model_picker(editor, inspectorWidth, "engine-ai-local-model-select-inspector");
            gui::property_row("[ai] Active panel", ai_workspace_domain_name(editor.aiWorkspaceDomain));
            gui::property_row("[ai] Stage", inspectorStage);
            gui::property_row("[ai] Evidence", inspectorGate.packetEvidenceSummary);
            gui::property_row("[ai] Model client", epoch::ai::model_connection_status());
            gui::property_row("[ai] Pending build", editor.aiContinuousBuildPending ? "true" : "false");
            gui::property_row("[ai] Status", editor.aiContinuousBuildStatus);
            gui::property_row("[ai] Selected model", epoch::ai::active_model_name().empty() ? "(none selected)" : epoch::ai::active_model_name());
            gui::property_row("[ai] Tool script", inspectorActiveScriptSource);
            gui::property_row("[ai] Build log", display_project_path(inspectorBuildLog));
            gui::property_row("[ai] Output", display_project_path(inspectorOutputExe));

            const bool inspectorSandboxControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Control;
            const bool inspectorToolingControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Tooling;
            const bool inspectorAssistantControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Engine;
            const bool inspectorLauncherControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Software;
            const bool inspectorTrainingControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Training;

            if (inspectorSandboxControls || inspectorLauncherControls)
            {
                const char* const saveLabel = inspectorSandboxControls
                    ? "Save Sandbox Evidence"
                    : "Save Active Project";
                if (gui::button(saveLabel, { inspectorWidth, 30.0f }))
                {
                    if (inspectorSandboxControls)
                        repair_self_iteration_sandbox_evidence(editor);
                    else
                        repair_active_project_evidence(editor);
                }
            }

            if (inspectorSandboxControls)
            {
                if (gui::button("Queue Sandbox Build Pass", { inspectorWidth, 30.0f }))
                {
                    start_self_iteration_sandbox_build(editor, "manual self-iteration sandbox build request", true);
                    append_project_note(
                        editor,
                        "Queue Sandbox Build Pass",
                        editor.aiContinuousBuildStatus,
                        "Wait for build evidence, then review the staged packet before promotion.");
                }

                if (gui::button("Stage Scene Training Task", { inspectorWidth, 30.0f }))
                {
                    auto packet = inspectorIterationPacket();
                    packet.packet_name = editor.projectId.empty()
                        ? std::string("sandbox-scene-training")
                        : editor.projectId + "-sandbox-scene-training";
                    packet.task_prompt = build_ai_sandbox_scene_training_prompt(editor);
                    packet.operator_notes =
                        "OS AI must use visible sandbox scene/tool/build evidence. Generic self-reporting like 'working fine' is invalid without paths, changed object state, and verifier output.";
                    packet.control_loop_stage = "Planner queued: sandbox scene-training task requires tool/build/runtime evidence";
                    const std::string packetDir = epoch::ai::stage_iteration_packet(packet);
                    if (packetDir.empty())
                    {
                        push_editor_log(editor, "[ai] Failed to stage sandbox scene-training task.");
                    }
                    else
                    {
                        push_editor_log(editor, "[ai] Staged sandbox scene-training task.");
                        push_editor_log(editor, std::string("[ai] Sandbox training packet path: ") + packetDir);
                        append_project_note(
                            editor,
                            "Sandbox Scene Training Task Staged",
                            std::string("Packet staged at ") + packetDir,
                            "Use this for visible OS AI scene-edit/test review; reject answers without evidence paths or visible state changes.");
                    }
                }
            }

            if (inspectorToolingControls)
            {
                if (gui::button("Run AI Tool Harness", { inspectorWidth, 30.0f }))
                {
                    const std::string before = editor_tooling_state_summary(editor);
                    const auto build = editor_build_script(editor.activeScript, editor.projectRoot);
                    editor.scriptBuildStatus = build.summary;
                    const bool ran = build.succeeded && editor_run_script(ctx.get(), editor.activeScript);
                    const std::string after = editor_tooling_state_summary(editor);
                    ++editor.aiToolHarnessRunCount;
                    editor.aiToolHarnessStatus = ran
                        ? "Harness ran selected script and captured editor before/after evidence."
                        : "Harness failed; inspect script build/run logs.";

                    epoch::ai::append_mcp_capture(epoch::ai::McpCaptureRecord{
                        .server = "editor",
                        .tool = "ai-tool-harness",
                        .prompt = std::string("Build and run selected editor tooling script: ") + editor.activeScript,
                        .normalized_output = std::string("build=") + (build.succeeded ? "pass" : "fail")
                            + "; run=" + (ran ? "pass" : "fail")
                            + "; before={" + before + "}; after={" + after + "}",
                        .source_path = inspectorActiveScriptSource
                    });

                    push_editor_log(editor, ran
                        ? "[ai-tool] Harness ran selected script and captured before/after state."
                        : "[ai-tool] Harness failed before a verified editor action.");
                    push_editor_log(editor, "[ai-tool] Before: " + before);
                    push_editor_log(editor, "[ai-tool] After: " + after);
                    append_project_note(
                        editor,
                        "Run AI Tool Harness",
                        editor.aiToolHarnessStatus,
                        std::string("Before: ") + before + " | After: " + after);

                    if (ran)
                    {
                        const std::string packetDir = epoch::ai::stage_iteration_packet(inspectorIterationPacket());
                        if (!packetDir.empty())
                        {
                            push_editor_log(editor, "[ai-tool] Staged tool-harness packet: " + packetDir);
                            append_project_note(
                                editor,
                                "Tool Harness Packet Staged",
                                std::string("Packet staged at ") + packetDir,
                                "Review this harness packet before promotion.");
                        }
                    }
                }
            }

            if (inspectorSandboxControls || inspectorAssistantControls || inspectorTrainingControls)
            {
                if (gui::button("Stage Evidence Packet", { inspectorWidth, 30.0f }))
                    (void)stageInspectorPacket(
                        "[ai] Staged AI iteration packet.",
                        "Manual Iteration Packet Staged",
                        "Use this as the visible evidence handoff for the next approved self-iteration pass.");
            }

            if (inspectorSandboxControls || inspectorAssistantControls)
            {
                if (gui::button("Ask Selected Model For Plan", { inspectorWidth, 30.0f }))
                {
                    if (epoch::ai::active_model_name().empty())
                    {
                        push_editor_log(editor, "[ai] Select a local chat model before requesting a model plan.");
                    }
                    else
                    {
                        const std::string packetDir = epoch::ai::stage_iteration_packet(inspectorIterationPacket());
                        const std::string prompt = build_ai_project_output_review_prompt(
                            editor,
                            inspectorPathsManifest,
                            inspectorBuildLog,
                            inspectorOutputExe);
                        submit_ai_prompt(prompt, "[ai] Submitted latest project output evidence to the selected local model.");
                        append_project_note(
                            editor,
                            "Selected Model Builder Plan Requested",
                            std::string("Model: ") + epoch::ai::active_model_name(),
                            packetDir.empty()
                                ? "Review the chat reply against current project/build/output evidence before approving work."
                                : std::string("Packet staged at ") + packetDir + "; review the chat reply before approving work.");
                    }
                }
            }

            if (inspectorTrainingControls)
            {
                if (gui::button("Capture Tool Evidence Snapshot", { inspectorWidth, 30.0f }))
                {
                    epoch::ai::append_mcp_capture(inspectorMcpRecord());
                    push_editor_log(editor, "[ai] Captured tool evidence training snapshot.");
                    push_editor_log(editor, std::string("[ai] Tool evidence path: ") + inspectorTraining.mcp_capture_jsonl);
                    append_project_note(
                        editor,
                        "Capture Tool Evidence Snapshot",
                        std::string("Tool evidence snapshot appended to ") + inspectorTraining.mcp_capture_jsonl,
                        "Review captured evidence before curating or promoting training data.");
                }

                if (gui::button("Promote Tool Evidence Snapshot", { inspectorWidth, 30.0f }))
                {
                    const bool ok = epoch::ai::promote_mcp_capture_record(inspectorMcpRecord(), "epoch_mcp_curated");
                    push_editor_log(editor, ok
                        ? "[ai] Promoted tool evidence snapshot into Engine/ai/datasets/curated."
                        : "[ai] Failed to promote tool evidence snapshot.");
                    if (ok)
                    {
                        push_editor_log(editor, std::string("[ai] Curated dataset root: ") + inspectorTraining.curated_dataset_root);
                        append_project_note(
                            editor,
                            "Promote Tool Evidence Snapshot",
                            std::string("Promoted into ") + inspectorTraining.curated_dataset_root,
                            "Promotion happened from the Inspector Training controls.");
                    }
                }

                if (gui::button("Promote Scene Eval", { inspectorWidth, 30.0f }))
                {
                    const bool ok = epoch::ai::promote_eval_case(epoch::ai::EvalCase{
                        .name = editor.projectId + "-scene-guidance",
                        .prompt = build_ai_scene_prompt(editor),
                        .expected_contains = editor.projectName,
                        .project_id = editor.projectId,
                        .scene_id = editor.activeRuntimeScene
                    });
                    push_editor_log(editor, ok
                        ? "[ai] Promoted scene eval into Engine/ai/evals."
                        : "[ai] Failed to promote scene eval.");
                    if (ok)
                        push_editor_log(editor, std::string("[ai] Eval suite root: ") + inspectorTraining.eval_root);
                }

                if (gui::button("Promote Latest Chat Pair", { inspectorWidth, 30.0f }))
                {
                    const bool ok = !inspectorLatestPrompt.empty() && !inspectorLatestReply.empty()
                        && inspectorLatestReply != "(empty reply)"
                        && epoch::ai::promote_dataset_record(epoch::ai::DatasetRecord{
                            .prompt = inspectorLatestPrompt,
                            .answer = inspectorLatestReply,
                            .source = "editor_ai_chat_curated",
                            .role = "assistant",
                            .tags = { editor.projectId, editor.activeRuntimeScene, "editor-chat" }
                        }, "epoch_editor_curated");
                    push_editor_log(editor, ok
                        ? "[ai] Promoted latest chat pair into curated dataset."
                        : "[ai] No promotable chat pair is available yet.");
                }
            }
        }
        else
        {
            const std::size_t selectedIndex = editor.entities.empty()
                ? 0u
                : (std::min)(editor.selectedEntity, editor.entities.size() - 1u);
            if (!editor.entities.empty())
            {
                const auto& entity = editor.entities[selectedIndex];
                gui::label(std::string("Selected: ") + entity.name);
                gui::label(std::string("Type: ") + entity.type);
                gui::label(std::string("Category: ") + entity.category);
                gui::label(std::string("Position: ") + vec3_text(entity.position));
                gui::label(std::string("Rotation: ") + vec3_text(entity.rotation));
                gui::label(std::string("Scale: ") + vec3_text(entity.scale));
                gui::label(std::string("Visible: ") + (entity.visible ? "true" : "false"));
                gui::label(std::string("EditorOnly: ") + (entity.editorOnly ? "true" : "false"));
            }
            else
            {
                gui::label("Selected: <none>");
            }
            gui::label(std::string("Viewport Target: ") + renderer_name(ctx));
            gui::label(std::string("Helpers Visible: ") + (editor.helpersVisible ? "true" : "false"));
            gui::label(std::string("Preview Mode: ") + std::string(preview_mode_name(editor.previewMode)));
            gui::label(std::string("Preview Camera: ") + preview_camera_name(ctx));
            gui::label(std::string("Preview Zoom: ") + preview_zoom_text(ctx));
            gui::label(std::string("Preview Objects: ") + std::to_string(visible_entity_count(editor)));
            gui::label(std::string("Editor Script: ") + editor.activeScript);
            gui::label(std::string("Runtime Target: ") + editor.activeRuntimeScene);
            gui::label("Viewport Input: click/drag objects  |  empty LMB pan  |  RMB orbit  |  Wheel zoom  |  Home reset");
        }
        gui::end_scroll_area();
        gui::end_window();
        }
        };

        auto render_timeline_time_controls = [&](float availableWidth, bool compact)
        {
            ensure_timeline_defaults(editor);
            const auto timelineStats = timeline_stats_from_editor(editor);
            epoch::timeline::sync_to_simulation(editor.timelineState, timelineStats);
            epoch::timeline::clamp_state(editor.timelineState);

            const float width = (std::max)(180.0f, availableWidth);
            const float playheadValue = static_cast<float>(std::clamp(
                editor.timelineState.duration_seconds > 0.0
                    ? editor.timelineState.playhead_seconds / editor.timelineState.duration_seconds
                    : 0.0,
                0.0,
                1.0));
            gui::progress_bar(gui::ProgressBarOptions{
                .label = "Timeline Playhead",
                .status = std::format("{:.2f}s / {:.2f}s", editor.timelineState.playhead_seconds, editor.timelineState.duration_seconds),
                .value = playheadValue,
                .size = { width, compact ? 18.0f : 22.0f },
                .show_percent = false
            });

            if (compact)
            {
                gui::property_row("[time] State", editor.timeSnapshot.paused ? "Paused" : "Running", 104.0f);
                gui::property_row(
                    "[time] Step",
                    std::format("{} / {} @ {}", editor.timeSnapshot.step_budget, editor.timeSnapshot.max_steps_per_frame, format_rate(editor.timeSnapshot.fixed_dt_seconds)),
                    104.0f);
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
                std::format("{} / {}", editor.timeSnapshot.step_budget, editor.timeSnapshot.max_steps_per_frame),
                132.0f);
            gui::property_row("[time] Time scale", std::format("{:.2f}x", editor.timeSnapshot.time_scale), 132.0f);
            gui::property_row("[time] Pacing health", pacingHealth, 132.0f);

            const std::array timeButtons{
                gui::InlineButtonSpec{ .label = editor.timeControl.paused ? "Resume" : "Pause", .width = 74.0f },
                gui::InlineButtonSpec{ .label = "Step", .width = 52.0f },
                gui::InlineButtonSpec{ .label = "0.5x", .width = 48.0f },
                gui::InlineButtonSpec{ .label = "1x", .width = 42.0f },
                gui::InlineButtonSpec{ .label = "2x", .width = 42.0f }
            };
            if (const auto action = gui::inline_button_row(timeButtons, 24.0f, 6.0f))
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
                gui::InlineButtonSpec{ .label = "30 Hz", .width = 56.0f },
                gui::InlineButtonSpec{ .label = "60 Hz", .width = 56.0f },
                gui::InlineButtonSpec{ .label = "120 Hz", .width = 64.0f }
            };
            if (const auto action = gui::inline_button_row(cadenceButtons, 24.0f, 6.0f))
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
                gui::InlineButtonSpec{ .label = "4 steps", .width = 64.0f },
                gui::InlineButtonSpec{ .label = "8 steps", .width = 64.0f },
                gui::InlineButtonSpec{ .label = "12 steps", .width = 72.0f }
            };
            if (const auto action = gui::inline_button_row(budgetButtons, 24.0f, 6.0f))
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

        const bool active_center_uses_scene = main_surface_uses_scene(editor.mainSurface);
        if (editor.surfaceSettleFrames > 0)
            --editor.surfaceSettleFrames;
        gui::begin_window(
            active_center_uses_scene ? std::string_view{} : std::string_view{ "Editor Workbench" },
            viewport_pos,
            viewport_size,
            !active_center_uses_scene);
        if (active_center_uses_scene)
        {
            const std::string_view sceneTitle = main_surface_title(editor.mainSurface);
            gui::label(std::string(sceneTitle));
            const gui::Vec2 scene_pos = gui::cursor_position();
            const float sceneAvailableWidth = (std::max)(48.0f, viewport_pos.x + viewport_size.x - scene_pos.x);
            const float sceneAvailableHeight = (std::max)(48.0f, viewport_pos.y + viewport_size.y - scene_pos.y);
            const bool showSceneTimeline = sceneAvailableHeight > 190.0f;
            const float timelineStripGap = showSceneTimeline ? 6.0f : 0.0f;
            const float timelineStripHeight = showSceneTimeline
                ? (std::min)(118.0f, (std::max)(82.0f, sceneAvailableHeight * 0.20f))
                : 0.0f;
            const gui::Vec2 scene_size{
                sceneAvailableWidth,
                (std::max)(48.0f, sceneAvailableHeight - timelineStripHeight - timelineStripGap)
            };
            result.scene_viewport = gui::scene_viewport({}, scene_pos, scene_size);
            ctx->set_scene_preview_mode(editor.previewMode);
            const int viewportGuard = ctx->type == core::ContextType::OpenGL ? 1 : 0;
            ctx->set_scene_viewport(core::RenderViewport{
                static_cast<int>((std::max)(0.0f, result.scene_viewport.position.x)) + viewportGuard,
                static_cast<int>((std::max)(0.0f, result.scene_viewport.position.y)) + viewportGuard,
                (std::max)(0, static_cast<int>((std::max)(0.0f, result.scene_viewport.size.x)) - viewportGuard * 2),
                (std::max)(0, static_cast<int>((std::max)(0.0f, result.scene_viewport.size.y)) - viewportGuard * 2)
            });
            update_scene_object_interaction(ctx, editor, result);
            publish_editor_preview_markers(ctx.get(), editor);

            if (showSceneTimeline)
            {
                gui::set_cursor({ scene_pos.x, scene_pos.y + scene_size.y + timelineStripGap });
                gui::label("Video Timeline");
                render_timeline_time_controls(scene_size.x, true);
            }
        }
        else
        {
            const gui::Vec2 centerSurfacePos = gui::cursor_position();
            const gui::Vec2 centerSurfaceSize{
                (std::max)(48.0f, viewport_pos.x + viewport_size.x - centerSurfacePos.x - 6.0f),
                (std::max)(48.0f, viewport_pos.y + viewport_size.y - centerSurfacePos.y - 6.0f)
            };
            result.scene_viewport = gui::WidgetBounds{ .position = centerSurfacePos, .size = centerSurfaceSize };
            ctx->set_scene_preview_mode(core::ScenePreviewMode::None);
            ctx->clear_scene_viewport();

            gui::label(std::string(main_surface_title(editor.mainSurface)));
            const gui::Vec2 centerScrollStart = gui::cursor_position();
            const float centerWidth = (std::max)(180.0f, centerSurfaceSize.x - 12.0f);
            const float centerScrollHeight = (std::max)(
                48.0f,
                viewport_pos.y + viewport_size.y - centerScrollStart.y - 6.0f);
            const std::string centerScrollId = std::string("center-surface-") + std::string(main_surface_title(editor.mainSurface));
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = centerScrollId,
                .size = { (std::max)(80.0f, centerSurfaceSize.x), centerScrollHeight },
                .draw_background = false,
                .show_scrollbar = true
            });

            switch (editor.mainSurface)
            {
            case EditorMainSurface::Project:
            {
                const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot);
                const std::filesystem::path buildLog = project_build_log_path(editor.projectRoot);
                gui::label(std::string("Active Project: ") + editor.projectName);
                gui::property_row("[project] Kind", editor.projectKind);
                gui::property_row("[project] Root", display_project_path(editor.projectRoot), 108.0f);
                gui::property_row("[project] Manifest", file_ready_summary(editor.projectManifest), 108.0f);
                gui::property_row("[project] Scene", file_ready_summary(editor.projectScenePath), 108.0f);
                gui::property_row("[project] Output", file_ready_summary(outputExe.string()), 108.0f);
                gui::property_row("[project] Build log", file_ready_summary(buildLog.string()), 108.0f);
                gui::wrapped_label(
                    "This surface is the project launcher/control workspace. Save, build, and run happen here or from the centered Run button; logs remain mirrored below.",
                    centerWidth);
                const auto runChoices = project_run_backend_choices();
                std::vector<std::string_view> runChoiceLabels;
                runChoiceLabels.reserve(runChoices.size());
                for (const auto& choice : runChoices)
                    runChoiceLabels.emplace_back(choice.label);
                const auto runBackendSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "project-run-backend-select",
                    .placeholder = "Choose project runtime backend",
                    .selected = project_run_backend_label(editor.projectRunBackend),
                    .options = std::span<const std::string_view>{ runChoiceLabels.data(), runChoiceLabels.size() },
                    .size = { (std::min)(centerWidth, 360.0f), 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 6
                });
                if (runBackendSelect.changed && runBackendSelect.selected_index && *runBackendSelect.selected_index < runChoices.size())
                {
                    editor.projectRunBackend = std::string(runChoices[*runBackendSelect.selected_index].argument);
                    push_editor_log(editor, std::string("[project] Project Run backend set to ") + std::string(runChoices[*runBackendSelect.selected_index].label) + ".");
                }
                gui::property_row("[project] Run mode", std::string(project_run_backend_label(editor.projectRunBackend)), 108.0f);
                const auto limitChoices = frame_limit_choices();
                std::vector<std::string_view> limitChoiceLabels;
                limitChoiceLabels.reserve(limitChoices.size());
                for (const auto& choice : limitChoices)
                    limitChoiceLabels.emplace_back(choice.label);
                const auto runLimitSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "project-run-frame-limit-select",
                    .placeholder = "Choose project frame limit",
                    .selected = frame_limit_label(editor.projectRunFrameLimitFps),
                    .options = std::span<const std::string_view>{ limitChoiceLabels.data(), limitChoiceLabels.size() },
                    .size = { (std::min)(centerWidth, 260.0f), 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 3
                });
                if (runLimitSelect.changed && runLimitSelect.selected_index && *runLimitSelect.selected_index < limitChoices.size())
                {
                    editor.projectRunFrameLimitFps = limitChoices[*runLimitSelect.selected_index].fps;
                    push_editor_log(editor, std::string("[project] Project Run frame limit set to ") + std::string(limitChoices[*runLimitSelect.selected_index].label) + ".");
                }
                gui::property_row("[project] Frame limit", std::string(frame_limit_label(editor.projectRunFrameLimitFps)), 108.0f);
                const bool activeProjectIsEngineSandbox = editor.projectId == "sandbox";
                if (!activeProjectIsEngineSandbox)
                {
                    const auto cameraChoices = project_camera_choices();
                    std::vector<std::string_view> cameraChoiceLabels;
                    cameraChoiceLabels.reserve(cameraChoices.size());
                    for (const auto& choice : cameraChoices)
                        cameraChoiceLabels.emplace_back(choice.label);
                    const auto cameraSelect = gui::select_box(gui::SelectBoxOptions{
                        .id = "project-camera-style-select",
                        .placeholder = "Choose project camera style",
                        .selected = project_camera_label(editor.projectCameraMode),
                        .options = std::span<const std::string_view>{ cameraChoiceLabels.data(), cameraChoiceLabels.size() },
                        .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                        .row_height = 28.0f,
                        .max_visible_options = 3
                    });
                    if (cameraSelect.changed && cameraSelect.selected_index && *cameraSelect.selected_index < cameraChoices.size())
                    {
                        editor.projectCameraMode = cameraChoices[*cameraSelect.selected_index].mode;
                        previewgrid::set_camera_mode(ctx.get(), editor.projectCameraMode);
                        if (editor.projectCameraMode == previewgrid::CameraMode::Canvas2D)
                        {
                            ensure_2d_canvas_entity(editor);
                            previewgrid::reset_camera(ctx.get());
                        }
                        push_editor_log(editor, std::string("[project] Project camera style set to ") + std::string(cameraChoices[*cameraSelect.selected_index].label) + ".");
                    }
                    gui::property_row("[project] Camera style", std::string(project_camera_label(editor.projectCameraMode)), 108.0f);
                }
                else
                {
                    gui::property_row("[project] Camera style", "Engine self-iteration mirrors the editor workbench.", 108.0f);
                }
                const auto inputChoices = input_profile_choices();
                std::vector<std::string_view> inputChoiceLabels;
                inputChoiceLabels.reserve(inputChoices.size());
                for (const auto& choice : inputChoices)
                    inputChoiceLabels.emplace_back(choice.label);
                const auto inputSelect = gui::select_box(gui::SelectBoxOptions{
                    .id = "project-input-profile-select",
                    .placeholder = "Choose shared input profile",
                    .selected = input_profile_label(editor.inputProfilePreset),
                    .options = std::span<const std::string_view>{ inputChoiceLabels.data(), inputChoiceLabels.size() },
                    .size = { (std::min)(centerWidth, 320.0f), 30.0f },
                    .row_height = 28.0f,
                    .max_visible_options = 4
                });
                if (inputSelect.changed && inputSelect.selected_index && *inputSelect.selected_index < inputChoices.size())
                {
                    editor.inputProfilePreset = inputChoices[*inputSelect.selected_index].preset;
                    input::set_active_profile(editor.inputProfilePreset);
                    push_editor_log(editor, std::string("[input] Shared profile set to ") + std::string(input_profile_label(editor.inputProfilePreset)) + ".");
                }
                gui::property_row("[project] Input profile", std::string(input_profile_label(editor.inputProfilePreset)), 108.0f);
                gui::wrapped_label("Shared input actions currently drive editor preview cameras and in-editor/project runtime cameras. Key rebinding UI is the next promotion gate; profiles keep controls universal now.", centerWidth);
                gui::wrapped_label(
                    "Play In Editor runs the active project scene inside this editor. Launch Single Context starts the current child executable when build inputs are fresh, and only rebuilds when source or project build files changed.",
                    centerWidth);
                if (gui::button("Save Active Project", { 220.0f, 30.0f }))
                    repair_active_project_evidence(editor);
                if (gui::button("Play In Editor", { 220.0f, 30.0f }))
                    play_active_context();
                if (gui::button("Build Active Project", { 220.0f, 30.0f }))
                    start_project_build(editor, false, "Project surface");
                if (gui::button("Launch Single Context", { 220.0f, 30.0f }))
                    launch_active_project_context();
                gui::wrapped_label(editor.projectStatus, centerWidth);
                gui::wrapped_label(editor.projectBuildStatus, centerWidth);
                break;
            }
            case EditorMainSurface::Assets:
            {
                gui::label("Asset Browser");
                const auto modelSummary = editor_project_model_summary(editor.projectId);
                gui::property_row("[assets] Project", editor.projectName, 120.0f);
                gui::property_row("[assets] Project root", display_project_path(editor.projectRoot), 120.0f);
                gui::property_row("[assets] Scene", display_project_path(editor.projectScenePath), 120.0f);
                gui::property_row("[assets] Demo model", modelSummary.asset_path.empty() ? std::string("(none)") : modelSummary.asset_path, 120.0f);
                gui::property_row("[assets] Model path", modelSummary.resolved_path.empty() ? std::string("(unresolved)") : modelSummary.resolved_path, 120.0f);
                gui::property_row("[assets] Model parsed", modelSummary.parsed ? "true" : "false", 120.0f);
                gui::wrapped_label(
                    "This surface is the project asset browser: scenes, models, images, text, and script files as normal project assets. Self-iteration controls stay in Intelligence and are only for engine work.",
                    centerWidth);

                const auto assetEntries = collect_asset_browser_entries(editor);
                gui::property_row("[assets] Active asset cards", std::to_string(assetEntries.size()), 120.0f);
                constexpr std::size_t kMaxVisibleAssetCards = 14;
                const std::size_t visibleAssetCards = (std::min)(assetEntries.size(), kMaxVisibleAssetCards);
                for (std::size_t assetIndex = 0; assetIndex < visibleAssetCards; ++assetIndex)
                {
                    const auto& entry = assetEntries[assetIndex];
                    const std::string buttonLabel = entry.label + (entry.directory ? "" : std::format("  [{} bytes]", entry.size));
                    if (gui::button(buttonLabel, { centerWidth, 28.0f }))
                    {
                        editor.selectedAssetPath = entry.path;
                        editor.selectedProjectFile = entry.path;
                        if (std::filesystem::path{ entry.path }.filename().string().ends_with(".ascript.cpp"))
                        {
                            editor.activeScript = script_id_from_source_path(std::filesystem::path{ entry.path });
                            editor.scriptBuildStatus = "Selected script asset: " + entry.path;
                            (void)load_script_source_editor(editor, entry.path, false);
                        }
                        push_editor_log(editor, "[assets] Selected " + entry.path);
                    }
                }
                if (assetEntries.size() > visibleAssetCards)
                {
                    gui::property_row(
                        "[assets] More",
                        std::to_string(assetEntries.size() - visibleAssetCards) + " hidden until the file-tree/thumbnail browser lands",
                        120.0f);
                }

                if (assetEntries.empty())
                    gui::wrapped_label("No active assets found yet. Add files under the project assets folder or use the project demo model path once it resolves.", centerWidth);
                gui::property_row("[assets] Selected", editor.selectedAssetPath.empty() ? std::string("(none)") : editor.selectedAssetPath, 120.0f);

                const std::string activeScriptSource = editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
                gui::label("Script Assets");
                gui::property_row("[script asset] Active", editor.activeScript, 120.0f);
                gui::property_row("[script asset] Source", activeScriptSource, 120.0f);
                gui::property_row(
                    "[script asset] Source exists",
                    std::filesystem::exists(std::filesystem::path{ activeScriptSource }) ? "true" : "false",
                    120.0f);
                if (const auto* activeScript = active_script_profile(editor))
                {
                    gui::property_row("[script asset] Build", activeScript->build_action, 120.0f);
                    gui::property_row("[script asset] Run", "active project only", 120.0f);
                    gui::property_row("[script asset] Hint", activeScript->diagnostic_hint, 120.0f);
                }
                gui::wrapped_label(editor.scriptBuildStatus, centerWidth);
                gui::property_row("[script asset] New", "type a safe id, then create a project-local .ascript.cpp", 120.0f);
                (void)gui::edit_box(editor.newScriptName, { centerWidth, 28.0f }, 64, false);
                if (gui::button("Create Script Asset", { (std::min)(220.0f, centerWidth), 30.0f }))
                    create_project_script_starter(editor);
                if (gui::button("Build Selected Script Asset", { (std::min)(240.0f, centerWidth), 30.0f }))
                {
                    if (editor.scriptEditorDirty)
                        (void)save_script_source_editor(editor);
                    const auto build = editor_build_script(editor.activeScript, editor.projectRoot);
                    editor.scriptBuildStatus = build.summary;
                    push_editor_log(
                        editor,
                        std::string("[script] ")
                        + (build.succeeded ? "Validation passed. " : "Validation failed. ")
                        + build.summary);
                    append_project_note(
                        editor,
                        "Build Script Asset",
                        build.summary,
                        build.succeeded ? "Script asset validation passed against the active project shell." : "Script asset validation failed; inspect script diagnostics before running.");
                }
                draw_script_source_editor(editor, activeScriptSource, centerWidth, 360.0f);
                break;
            }
            case EditorMainSurface::ForestFactory:
            {
                const auto forestPackage = std::find_if(
                    epoch::package_registry::kKnownPackages.begin(),
                    epoch::package_registry::kKnownPackages.end(),
                    [](const epoch::package_registry::PackageDescriptor& package) {
                        return package.id == epoch::package_registry::kEngineForestFactoryPackageId;
                    });
                const auto profile = epoch::forest::default_profile(epoch::forest::ForestPreset::Tree);
                const auto geometry = epoch::forest::build_preview_geometry(profile);
                const auto stats = geometry.stats;
                const auto voxelSummary = epoch::forest::estimate_voxel_occupancy(profile, geometry, 0.20F);
                const std::filesystem::path manifestPath =
                    resolve_editor_path(std::filesystem::path{ editor.projectRoot })
                    / "assets" / "packages" / "engine_forest_factory.package.json";
                const std::filesystem::path profilePath =
                    resolve_editor_path(std::filesystem::path{ editor.projectRoot })
                    / "assets" / "packages" / "engine_forest_factory" / "default.forest.json";
                const bool manifestReady = std::filesystem::exists(manifestPath);
                const bool profileReady = std::filesystem::exists(profilePath);

                gui::label("Plant Lab");
                gui::wrapped_label(
                    "Core temporal graph / parametric L-system vegetation lab. Plant Lab owns a live editor scene preview; generated projects receive assets only after package activation or main-scene use approval.",
                    centerWidth);
                gui::property_row("[forest] Editor name", std::string(epoch::forest::kForestFactoryWorkspace), 148.0f);
                gui::property_row("[forest] Technique", std::string(epoch::forest::kForestFactoryTechnique), 148.0f);
                gui::property_row("[forest] Reference repo", std::string(epoch::forest::kForestFactoryReferenceRepo), 148.0f);
                gui::property_row("[forest] Package", forestPackage != epoch::package_registry::kKnownPackages.end() ? std::string(forestPackage->displayName) : std::string("(missing registry entry)"), 148.0f);
                gui::property_row("[forest] Package source", forestPackage != epoch::package_registry::kKnownPackages.end() ? std::string(forestPackage->externalSourceRepo) : std::string("(missing registry entry)"), 148.0f);
                gui::property_row("[forest] Manifest", manifestReady ? display_project_path(manifestPath) : std::string("missing - activate through Package Manager"), 148.0f);
                gui::property_row("[forest] Profile", profileReady ? display_project_path(profilePath) : std::string("missing - activate through Package Manager"), 148.0f);
                gui::property_row("[forest] Preset", std::string(epoch::forest::preset_name(profile.preset)), 148.0f);
                gui::property_row("[forest] Mode", profile.previewMode == epoch::forest::ForestPreviewMode::Mode3D ? "3D" : "2D", 148.0f);
                gui::property_row("[forest] Stage", std::string(epoch::forest::stage_name(profile.editStage)), 148.0f);
                gui::property_row("[forest] Seed", std::to_string(profile.seed.value), 148.0f);
                gui::property_row("[forest] Time", std::format("{:.2f}s / {:.2f}s", profile.temporal.timeSeconds, profile.temporal.durationSeconds), 148.0f);
                gui::property_row("[forest] Speed", std::format("{:.2f}x", profile.temporal.speed), 148.0f);
                gui::property_row("[forest] Nodes", std::to_string(stats.nodes), 148.0f);
                gui::property_row("[forest] Branches", std::to_string(stats.branches), 148.0f);
                gui::property_row("[forest] Leaves", std::to_string(stats.leaves), 148.0f);
                gui::property_row("[forest] Preview segments", std::to_string(geometry.segmentCount), 148.0f);
                gui::property_row("[forest] Verts", std::to_string(stats.vertices), 148.0f);
                gui::property_row("[forest] Tris", std::to_string(stats.triangles), 148.0f);
                gui::property_row("[forest] Voxel chunk",
                    std::format(
                        "{}x{}x{} @ {:.2f}m",
                        voxelSummary.chunk.cellsX,
                        voxelSummary.chunk.cellsY,
                        voxelSummary.chunk.cellsZ,
                        voxelSummary.chunk.cellSizeMeters),
                    148.0f);
                gui::property_row("[forest] Active voxels", std::to_string(voxelSummary.activeCells), 148.0f);
                gui::property_row("[forest] Dense bytes", std::to_string(voxelSummary.denseBytes), 148.0f);
                gui::property_row("[forest] Voxel split",
                    std::format(
                        "trunk {} | branch {} | foliage {}",
                        voxelSummary.trunkCells,
                        voxelSummary.branchCells,
                        voxelSummary.foliageCells),
                    148.0f);
                gui::wrapped_label(
                    "Scene preview: Plant Lab now emits deterministic temporal graph segments, foliage clusters, and a voxel occupancy summary for future LOD, hit detection, navigation, lighting, and path-trace queries. Package activation emits reusable project assets only after an explicit install/stage gate.",
                    centerWidth);
                std::array<gui::InlineButtonSpec, 3> forestActions{ {
                    { "Regenerate Temporal Graph", 228.0f },
                    { "Select Lead Tip", 132.0f },
                    { "Reset Forest Data", 146.0f }
                } };
                if (auto clicked = gui::inline_button_row(forestActions, 30.0f, 8.0f))
                {
                    if (*clicked == 0)
                    {
                        ensure_forest_factory_preview_entities(editor);
                        push_editor_log(editor, "[forest] Regenerated deterministic temporal graph preview.");
                    }
                    else if (*clicked == 1)
                    {
                        const auto selected = std::find_if(
                            editor.entities.begin(),
                            editor.entities.end(),
                            [](const EditorEntity& entity)
                            {
                                return entity.name.rfind("ForestFactoryCanopy_", 0) == 0;
                            });
                        if (selected != editor.entities.end())
                        {
                            editor.selectedEntity = static_cast<std::size_t>(std::distance(editor.entities.begin(), selected));
                            push_editor_log(editor, "[forest] Selected the first visible Plant Lab lead tip.");
                        }
                        else
                        {
                            ensure_forest_factory_preview_entities(editor);
                        }
                    }
                    else if (*clicked == 2)
                    {
                        editor.packageInstallStatus = "Plant Lab data reset is staged behind package activation; current preview primitives remain editor-only.";
                        push_editor_log(editor, "[forest] Reset requested; package-backed data reset remains gated.");
                    }
                }
                if (gui::button("Open Package Manager", { 220.0f, 30.0f }))
                {
                    editor.showPackageManagerModal = true;
                    editor.selectedPackageId = std::string(epoch::package_registry::kEngineForestFactoryPackageId);
                    editor.packageInstallStatus = manifestReady && profileReady
                        ? "Plant Lab project package is already staged."
                        : "Select Install to stage the Plant Lab project package.";
                    push_editor_log(editor, "[forest] Package Manager opened for Plant Lab.");
                }
                if (gui::button("Stage Plant Lab Package", { 240.0f, 30.0f }))
                {
                    if (forestPackage != epoch::package_registry::kKnownPackages.end())
                        (void)stage_forest_factory_package_opt_in(editor, *forestPackage);
                    else
                        editor.packageInstallStatus = "Plant Lab package registry entry is missing.";
                }
                break;
            }
            case EditorMainSurface::Timeline:
            {
                ensure_timeline_defaults(editor);
                auto timelineStats = timeline_stats_from_editor(editor);
                epoch::timeline::sync_to_simulation(editor.timelineState, timelineStats);
                epoch::timeline::clamp_state(editor.timelineState);
                epoch::saveload::clamp_streaming_save_config(editor.streamingSaveConfig);
                editor.streamingSaveStatus.active = editor.streamingSaveConfig.enabled;
                if (epoch::saveload::should_capture_checkpoint(
                    editor.streamingSaveConfig,
                    editor.streamingSaveStatus,
                    timelineStats))
                {
                    epoch::saveload::mark_checkpoint_captured(
                        editor.streamingSaveStatus,
                        editor.streamingSaveConfig,
                        timelineStats);
                    editor.timelineEvents.push_back(epoch::timeline::make_event_from_stats(
                        "save",
                        epoch::timeline::TimelineEventKind::Checkpoint,
                        timelineStats,
                        editor.streamingSaveStatus.last_snapshot_label,
                        "PersistentLevel",
                        editor.streamingSaveStatus.last_output_path));
                    epoch::timeline::sort_events(editor.timelineEvents);
                    editor.lastCheckpointRecord = epoch::saveload::make_checkpoint_record(
                        editor.streamingSaveConfig,
                        editor.streamingSaveStatus,
                        timelineStats,
                        0u,
                        editor.timelineEvents.size());
                    push_editor_log(editor, "[timeline] Auto-staged timeline checkpoint: " + editor.streamingSaveStatus.last_snapshot_label);
                }

                gui::label("Video");
                gui::wrapped_label(
                    "Epoch treats time as a first-class 4D authoring spine. This surface owns the shared simulation clock controls, timeline graph, video-editing path, checkpoint gates, and configurable streaming-save contract without pretending scene serialization is finished.",
                    centerWidth);
                render_timeline_time_controls(centerWidth, false);
                gui::label("Timeline Data");
                gui::property_row("[timeline] Frame", std::to_string(timelineStats.frame_index), 132.0f);
                gui::property_row("[timeline] Simulated", format_seconds(timelineStats.simulated_seconds), 132.0f);
                gui::property_row("[timeline] Fixed step", std::string(format_ms(timelineStats.fixed_dt_seconds)) + " / " + format_rate(timelineStats.fixed_dt_seconds), 132.0f);
                gui::property_row("[timeline] Step budget", std::to_string(timelineStats.step_budget), 132.0f);
                gui::property_row("[timeline] Time scale", std::format("{:.2f}x", timelineStats.time_scale), 132.0f);
                gui::property_row("[timeline] Playhead", std::format("{:.2f}s / frame {}", editor.timelineState.playhead_seconds, editor.timelineState.playhead_frame), 132.0f);
                gui::property_row("[timeline] Duration", std::format("{:.2f}s", editor.timelineState.duration_seconds), 132.0f);
                gui::property_row("[timeline] Tracks", std::format("{} enabled / {}", epoch::timeline::enabled_track_count(editor.timelineTracks), editor.timelineTracks.size()), 132.0f);
                gui::property_row("[timeline] Keys", std::to_string(editor.timelineEvents.size()), 132.0f);
                const epoch::timeline::TimelineViewConfig activeTimelineView{
                    .visible_start_seconds = (std::max)(0.0, editor.timelineState.playhead_seconds - 5.0),
                    .visible_duration_seconds = 10.0,
                    .pixel_width = (std::max)(320.0f, centerWidth)
                };
                const epoch::timeline::TimelineLaneLayoutConfig activeLaneLayout{
                    .pixel_width = (std::max)(320.0f, centerWidth),
                    .header_width = 132.0,
                    .lane_height = 24.0,
                    .lane_gap = 4.0,
                    .top_padding = 0.0
                };
                const auto activeTimelineLanes = epoch::timeline::make_lane_geometry(
                    editor.timelineTracks,
                    activeLaneLayout);
                const auto activeTimelineMarkers = epoch::timeline::make_event_markers(
                    editor.timelineTracks,
                    editor.timelineEvents,
                    activeTimelineView,
                    activeLaneLayout,
                    editor.timelineState.duration_seconds);
                gui::property_row(
                    "[timeline] View",
                    epoch::timeline::describe_view(
                        editor.timelineState,
                        editor.timelineTracks,
                        editor.timelineEvents,
                        activeTimelineView),
                    132.0f);
                gui::property_row(
                    "[timeline] Layout",
                    epoch::timeline::describe_lane_layout(activeTimelineLanes, activeTimelineMarkers),
                    132.0f);
                if (const auto* nextEvent = epoch::timeline::next_event_after(editor.timelineEvents, editor.timelineState.playhead_seconds))
                    gui::property_row("[timeline] Next key", epoch::timeline::describe_event(*nextEvent), 132.0f);
                else
                    gui::property_row("[timeline] Next key", "(none)", 132.0f);
                const auto activeSaveProfile = epoch::saveload::detect_streaming_save_profile(editor.streamingSaveConfig);
                if (const auto* profileDescriptor = epoch::saveload::find_streaming_save_profile(activeSaveProfile))
                {
                    gui::property_row("[timeline] Profile id", std::string(profileDescriptor->id), 132.0f);
                    gui::property_row("[timeline] Profile detail", std::string(profileDescriptor->summary), 132.0f);
                }
                const auto timelineKeyedProfilePlan = epoch::saveload::make_streaming_save_profile_change_plan(
                    editor.streamingSaveConfig,
                    epoch::saveload::StreamingSaveProfile::TimelineKeyed);
                gui::property_row("[timeline] Stream mode", std::string(epoch::saveload::mode_name(editor.streamingSaveConfig.mode)), 132.0f);
                gui::property_row("[timeline] Save profile", std::string(epoch::saveload::stream_profile_name(activeSaveProfile)), 132.0f);
                gui::property_row("[timeline] Profile gate", epoch::saveload::streaming_save_profile_change_summary(timelineKeyedProfilePlan), 132.0f);
                gui::property_row("[timeline] Retention", epoch::saveload::describe_retention(editor.streamingSaveConfig), 132.0f);
                const auto activeSaveCadence = epoch::saveload::make_streaming_save_cadence_plan(
                    editor.streamingSaveConfig,
                    editor.streamingSaveStatus,
                    timelineStats);
                gui::property_row("[timeline] Next capture", epoch::saveload::streaming_save_cadence_summary(activeSaveCadence), 132.0f);
                gui::property_row("[timeline] Stream state", epoch::saveload::describe_streaming_save(editor.streamingSaveConfig, editor.streamingSaveStatus), 132.0f);
                gui::property_row("[timeline] Last key", editor.streamingSaveStatus.last_snapshot_label.empty() ? std::string("(none staged)") : editor.streamingSaveStatus.last_snapshot_label, 132.0f);
                gui::property_row("[timeline] Last record", epoch::saveload::checkpoint_record_summary(editor.lastCheckpointRecord), 132.0f);
                gui::property_row("[timeline] Target", editor.streamingSaveStatus.last_output_path.empty() ? editor.streamingSaveConfig.target_root : editor.streamingSaveStatus.last_output_path, 132.0f);
                gui::property_row("[timeline] Manifest", epoch::saveload::join_stream_path(editor.streamingSaveConfig.target_root, "manifest.timeline.log"), 132.0f);
                if (editor.lastCheckpointRecord.valid)
                {
                    const auto restorePlan = epoch::saveload::make_checkpoint_restore_plan(
                        editor.streamingSaveConfig,
                        editor.lastCheckpointRecord);
                    const std::array retentionRecords{
                        editor.lastCheckpointRecord
                    };
                    const auto retentionPlan = epoch::saveload::make_checkpoint_retention_plan(
                        editor.streamingSaveConfig,
                        retentionRecords);
                    gui::property_row(
                        "[timeline] Writer gate",
                        epoch::saveload::checkpoint_writer_approval_summary(epoch::saveload::StreamingCheckpointWriteApproval{}),
                        132.0f);
                    gui::property_row(
                        "[timeline] Cleanup",
                        epoch::saveload::checkpoint_retention_plan_summary(retentionPlan),
                        132.0f);
                    gui::property_row(
                        "[timeline] Scene payload",
                        restorePlan.scene_payload_path,
                        132.0f);
                    gui::property_row(
                        "[timeline] Restore",
                        epoch::saveload::checkpoint_restore_plan_summary(restorePlan),
                        132.0f);
                }

                const std::array playbackButtons{
                    gui::InlineButtonSpec{ .label = "Rewind", .width = 76.0f },
                    gui::InlineButtonSpec{ .label = "-1s", .width = 56.0f },
                    gui::InlineButtonSpec{ .label = editor.timelineState.playing ? "Pause" : "Play", .width = 72.0f },
                    gui::InlineButtonSpec{ .label = "+1s", .width = 56.0f },
                    gui::InlineButtonSpec{ .label = editor.timelineState.recording ? "Stop Rec" : "Record Gate", .width = 112.0f }
                };
                if (const auto action = gui::inline_button_row(playbackButtons, 26.0f, 6.0f))
                {
                    switch (*action)
                    {
                    case 0:
                        editor.timelineState.playhead_seconds = 0.0;
                        editor.timelineState.playhead_frame = 0;
                        push_editor_log(editor, "[timeline] Playhead rewound to the beginning.");
                        break;
                    case 1:
                        epoch::timeline::scrub_seconds(editor.timelineState, -1.0);
                        push_editor_log(editor, "[timeline] Playhead scrubbed backward.");
                        break;
                    case 2:
                        editor.timelineState.playing = !editor.timelineState.playing;
                        push_editor_log(editor, editor.timelineState.playing
                            ? "[timeline] Timeline playback follows the shared simulation clock."
                            : "[timeline] Timeline playback paused for scrubbing.");
                        break;
                    case 3:
                        epoch::timeline::scrub_seconds(editor.timelineState, 1.0);
                        push_editor_log(editor, "[timeline] Playhead scrubbed forward.");
                        break;
                    case 4:
                        editor.timelineState.recording = !editor.timelineState.recording;
                        push_editor_log(editor, editor.timelineState.recording
                            ? "[timeline] Timeline recording gate armed for visible editor events."
                            : "[timeline] Timeline recording gate paused.");
                        break;
                    default:
                        break;
                    }
                }

                const std::array streamButtons{
                    gui::InlineButtonSpec{ .label = editor.streamingSaveConfig.enabled ? "Pause Stream" : "Arm Stream", .width = 118.0f },
                    gui::InlineButtonSpec{ .label = "Manual Key", .width = 96.0f },
                    gui::InlineButtonSpec{ .label = "15s Mode", .width = 84.0f },
                    gui::InlineButtonSpec{ .label = "120f Mode", .width = 92.0f },
                    gui::InlineButtonSpec{ .label = "Timeline Key", .width = 112.0f }
                };
                if (const auto action = gui::inline_button_row(streamButtons, 26.0f, 6.0f))
                {
                    switch (*action)
                    {
                    case 0:
                        editor.streamingSaveConfig.enabled = !editor.streamingSaveConfig.enabled;
                        editor.streamingSaveStatus.active = editor.streamingSaveConfig.enabled;
                        editor.streamingSaveStatus.message = editor.streamingSaveConfig.enabled
                            ? "Timeline save stream armed; checkpoints stage as reviewable evidence."
                            : "Timeline save stream is disabled.";
                        push_editor_log(editor, editor.streamingSaveConfig.enabled
                            ? "[timeline] Streaming save contract armed."
                            : "[timeline] Streaming save contract paused.");
                        break;
                    case 1:
                        epoch::saveload::mark_checkpoint_captured(
                            editor.streamingSaveStatus,
                            editor.streamingSaveConfig,
                            timelineStats);
                        editor.timelineEvents.push_back(epoch::timeline::make_event_from_stats(
                            "save",
                            epoch::timeline::TimelineEventKind::Checkpoint,
                            timelineStats,
                            editor.streamingSaveStatus.last_snapshot_label,
                            "PersistentLevel",
                            editor.streamingSaveStatus.last_output_path));
                        epoch::timeline::sort_events(editor.timelineEvents);
                        editor.lastCheckpointRecord = epoch::saveload::make_checkpoint_record(
                            editor.streamingSaveConfig,
                            editor.streamingSaveStatus,
                            timelineStats,
                            0u,
                            editor.timelineEvents.size());
                        push_editor_log(editor, "[timeline] Manual checkpoint staged: " + editor.streamingSaveStatus.last_snapshot_label);
                        break;
                    case 2:
                        epoch::saveload::apply_streaming_save_profile(
                            editor.streamingSaveConfig,
                            epoch::saveload::StreamingSaveProfile::EditorInterval15s);
                        push_editor_log(editor, "[timeline] Streaming save mode set to 15 second intervals.");
                        break;
                    case 3:
                        epoch::saveload::apply_streaming_save_profile(
                            editor.streamingSaveConfig,
                            epoch::saveload::StreamingSaveProfile::EditorFrame120);
                        push_editor_log(editor, "[timeline] Streaming save mode set to 120 frame intervals.");
                        break;
                    case 4:
                        epoch::saveload::apply_streaming_save_profile(
                            editor.streamingSaveConfig,
                            epoch::saveload::StreamingSaveProfile::TimelineKeyed);
                        push_editor_log(editor, "[timeline] Streaming save mode set to timeline key staging.");
                        break;
                    default:
                        break;
                    }
                }
                gui::wrapped_label(editor.streamingSaveStatus.message, centerWidth);
                gui::wrapped_label(
                    "Next gate: bind timeline events to the scene parser/serializer and streaming-save writer so .epoch snapshots, replay keys, and restore points round-trip from live editor state.",
                    centerWidth);
                break;
            }
            case EditorMainSurface::AISandbox:
            {
                const auto training = epoch::ai::default_training_paths();
                const std::filesystem::path buildLog = project_build_log_path(editor.projectRoot);
                const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot);
                const std::filesystem::path pathsManifest = resolve_editor_path(std::filesystem::path{ editor.projectRoot }) / "project.paths.txt";
                const std::string latestPrompt = last_chat_line_with_prefix(chat, "you> ");
                const std::string latestReply = last_chat_line_with_prefix(chat, "ai> ");
                const auto gateStatus = summarize_ai_review_gate(
                    editor,
                    buildLog,
                    outputExe,
                    pathsManifest,
                    training,
                    latestPrompt,
                    latestReply);
                const std::array<bool, 5> aiLoopReady{{
                    gateStatus.projectEvidenceReady,
                    gateStatus.buildEvidenceReady,
                    gateStatus.captureEvidenceReady,
                    gateStatus.chatPairReady,
                    gateStatus.projectEvidenceReady
                        && gateStatus.buildEvidenceReady
                        && gateStatus.captureEvidenceReady
                        && gateStatus.chatPairReady
                }};
                const auto aiLoopCanvas = build_ai_loop_surface(
                    aiLoopReady,
                    aiLoopReady.back(),
                    editor.aiContinuousBuildPending.has_value());
                editor.systems.aiLoopSurface = gui::register_runtime_surface(
                    "ai-loop-visualizer",
                    std::span<const std::uint8_t>(aiLoopCanvas.pixels.data(), aiLoopCanvas.pixels.size()),
                    static_cast<std::uint32_t>(aiLoopCanvas.width),
                    static_cast<std::uint32_t>(aiLoopCanvas.height));

                gui::label("OS AI Self-Iteration");
                gui::property_row("[ai] Project", editor.projectName);
                gui::property_row("[ai] Root", display_project_path(editor.projectRoot), 108.0f);
                gui::property_row("[ai] Selected model", epoch::ai::active_model_name().empty() ? "(none selected)" : epoch::ai::active_model_name(), 108.0f);
                gui::property_row("[ai] Model client", epoch::ai::model_connection_status(), 108.0f);
                gui::property_row("[ai] Status", editor.aiContinuousBuildStatus, 108.0f);
                gui::property_row("[ai] Captures", display_project_path(training.local_capture_jsonl), 108.0f);
                gui::property_row("[ai] Loop stage", ai_control_loop_stage(gateStatus), 108.0f);
                gui::property_row("[ai] Evidence", gateStatus.packetEvidenceSummary, 108.0f);
                gui::label("AI Loop Visualizer");
                if (editor.systems.aiLoopSurface.is_valid())
                    gui::image(editor.systems.aiLoopSurface, { centerWidth, 112.0f });
                else
                    gui::wrapped_label("AI loop graph surface unavailable; check runtime-surface atlas diagnostics.", centerWidth);
                gui::wrapped_label(
                    "This is the visible control surface for engine self-iteration. OS AI stages evidence and build packets here; promotion remains a human-approved step.",
                    centerWidth);
                const auto sandboxNotesPath = project_notes_path(editor.projectRoot);
                const auto sandboxBuildLog = project_build_log_path(editor.projectRoot);
                const auto sandboxOutput = project_output_exe_path(editor.projectRoot);
                gui::property_row("[ai] Notes", display_project_path(sandboxNotesPath), 108.0f);
                gui::property_row("[ai] Build log", display_project_path(sandboxBuildLog), 108.0f);
                gui::property_row("[ai] Output", display_project_path(sandboxOutput), 108.0f);
                gui::label("Latest Engine Iteration Notes");
                gui::wrapped_label(tail_text(read_project_notes(editor.projectRoot), 1500), centerWidth);
                render_ai_model_picker(editor, (std::min)(centerWidth, 460.0f), "engine-ai-local-model-select-workspace");
                gui::wrapped_label(
                    "Model package lanes stage operator-approved download plans under executable-local cache/models; weights are never cloned into routine engine iterations.",
                    centerWidth);
                std::array<gui::InlineButtonSpec, 2> modelPackageActions{ {
                    { "Stage Nemotron Model", 180.0f },
                    { "Stage Qwen 27B Model", 180.0f }
                } };
                if (auto clicked = gui::inline_button_row(modelPackageActions, 28.0f, 8.0f))
                {
                    const std::string_view packageId = *clicked == 0
                        ? epoch::package_registry::kNemotronNanoPackageId
                        : epoch::package_registry::kQwenCoderPackageId;
                    editor.selectedPackageId = std::string(packageId);
                    editor.showPackageManagerModal = true;
                    editor.packageInstallStatus = "Model package selected; press Install to stage the cache/models download plan.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, std::string("[ai] Opened OS model package lane: ") + std::string(packageId) + ".");
                }

                gui::label("OS AI Chat");
                (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                    .id = "engine-ai-sandbox-chat",
                    .size = { centerWidth, 150.0f },
                    .lines = chat.lines,
                    .max_line_chars = 220,
                    .selectable = true,
                    .stick_to_bottom = true
                });
                const auto chatInput = gui::edit_box(chat.input, { centerWidth, 30.0f }, 4096, false);
                bool chatActionClicked = false;
                std::array<gui::InlineButtonSpec, 2> chatActions{ {
                    { "Send", 96.0f },
                    { "Evidence Plan", 150.0f }
                } };
                if (auto clicked = gui::inline_button_row(chatActions, 28.0f, 8.0f))
                {
                    chatActionClicked = true;
                    if (*clicked == 0)
                    {
                        chat.submit(std::move(chat.input));
                        chat.input.clear();
                    }
                    else
                    {
                        chat.submit(build_ai_self_iteration_prompt(editor));
                    }
                }
                if (chatInput.submitted && !chatActionClicked)
                {
                    chat.submit(std::move(chat.input));
                    chat.input.clear();
                }

                if (gui::button("Save Sandbox Evidence", { 240.0f, 30.0f }))
                    repair_self_iteration_sandbox_evidence(editor);
                if (gui::button("Queue Sandbox Build Pass", { 240.0f, 30.0f }))
                    start_self_iteration_sandbox_build(editor, "manual self-iteration sandbox build request", true);
                break;
            }
            case EditorMainSurface::Systems:
            {
                const auto orderedSystems = epoch::systems::Registry::instance().ordered_systems();
                const std::size_t hardwareThreadCount = (std::max)(std::size_t{ 1 },
                    std::thread::hardware_concurrency() > 0
                    ? static_cast<std::size_t>(std::thread::hardware_concurrency())
                    : std::size_t{ 6 });
                const std::size_t liveThreadCount = epoch::systems::threading::live_thread_count();
                const std::string supportTier = recommended_support_tier(ctx, hardwareThreadCount);
                const std::string backendGuidance = backend_runtime_guidance(ctx, supportTier);
                const std::string convergenceFocus = backend_convergence_focus(ctx);
                const float graphGap = 14.0f;
                const float graphWidth = (std::max)(260.0f, centerWidth);
                const float graphHeight = 264.0f;
                const float supportHeight = 118.0f;
                constexpr int kGraphInputCooldownFrames = 6;
                if (editor.systems.graphInputCooldownFrames > 0)
                    --editor.systems.graphInputCooldownFrames;

                const auto renderCanvas = build_render_graph_surface(
                    editor.systems,
                    epoch::ai::current_provider_mode() == epoch::ai::ProviderMode::McpOperations);
                const auto taskCanvas = build_task_graph_surface(
                    editor.systems,
                    liveThreadCount,
                    orderedSystems.size);
                const auto supportCanvas = build_support_tier_surface(
                    supportTier,
                    ctx && ctx->type == core::ContextType::Software);

                editor.systems.renderSurface = gui::register_runtime_surface(
                    "systems-render-graph",
                    std::span<const std::uint8_t>(renderCanvas.pixels.data(), renderCanvas.pixels.size()),
                    static_cast<std::uint32_t>(renderCanvas.width),
                    static_cast<std::uint32_t>(renderCanvas.height));
                editor.systems.taskSurface = gui::register_runtime_surface(
                    "systems-task-graph",
                    std::span<const std::uint8_t>(taskCanvas.pixels.data(), taskCanvas.pixels.size()),
                    static_cast<std::uint32_t>(taskCanvas.width),
                    static_cast<std::uint32_t>(taskCanvas.height));
                editor.systems.supportSurface = gui::register_runtime_surface(
                    "systems-support-tier",
                    std::span<const std::uint8_t>(supportCanvas.pixels.data(), supportCanvas.pixels.size()),
                    static_cast<std::uint32_t>(supportCanvas.width),
                    static_cast<std::uint32_t>(supportCanvas.height));

                gui::label("System Info");
                gui::property_row("[system] Renderer", renderer_name(ctx), 112.0f);
                gui::property_row("[system] Platform", epochnamespace::GetEngineBuildTagString(), 112.0f);
                gui::property_row("[system] Live threads", std::to_string(liveThreadCount), 112.0f);
                gui::property_row("[system] CPU threads", std::to_string(hardwareThreadCount), 112.0f);
                gui::property_row("[system] Panel host", editor.detachedPanelHostStatus, 112.0f);
                gui::property_row("[visual] Profile", std::string(epochnamespace::visuals::active_profile_name()), 112.0f);
                gui::property_row("[visual] Parity gate", std::string(epochnamespace::visuals::parity_gate()), 132.0f);
                gui::property_row("[renderer] Resource spine", renderer_resource_spine_summary(ctx), 132.0f);
                gui::property_row("[renderer] Declared desc", renderer_declared_descriptor_status(), 132.0f);
                gui::property_row("[renderer] Mesh/model", renderer_native_mesh_model_status(ctx), 132.0f);
                gui::property_row("[renderer] Sampled RTT", renderer_native_sampled_rtt_status(ctx), 132.0f);
                gui::property_row("[renderer] Next gate", renderer_next_feature_gate(ctx), 132.0f);
                gui::wrapped_label(
                    "System Info is reserved for render/backend/context routing and diagnostics. Video owns time controls, timeline graphing, streaming-save cadence, and video-authoring surfaces.",
                    centerWidth);
                gui::property_row(
                    "[timeline] Owner",
                    "Video owns playhead, step, cadence, streaming-save, and video timing controls.",
                    132.0f);
                const auto systemsOrigin = gui::cursor_position();
                const float titleY = systemsOrigin.y + 6.0f;
                const float controlsY = titleY + gui::line_height() + 4.0f;
                const float imageY = controlsY + 30.0f;

                gui::set_cursor({ systemsOrigin.x, titleY });
                gui::label("Render / Frame Graph");
                gui::set_cursor({ systemsOrigin.x, controlsY });
                const std::array renderButtons{
                    gui::InlineButtonSpec{ .label = "Pan <", .width = 58.0f },
                    gui::InlineButtonSpec{ .label = "Zoom -", .width = 66.0f },
                    gui::InlineButtonSpec{ .label = "Zoom +", .width = 66.0f },
                    gui::InlineButtonSpec{ .label = "Pan >", .width = 58.0f },
                    gui::InlineButtonSpec{ .label = "Reset", .width = 58.0f }
                };
                if (const auto action = gui::inline_button_row(renderButtons, 28.0f, 6.0f);
                    action && editor.systems.graphInputCooldownFrames == 0)
                {
                    switch (*action)
                    {
                    case 0: editor.systems.renderPan = (std::max)(0, editor.systems.renderPan - 64); break;
                    case 1: editor.systems.renderZoom = (std::max)(0.85f, editor.systems.renderZoom - 0.25f); break;
                    case 2: editor.systems.renderZoom = (std::min)(3.0f, editor.systems.renderZoom + 0.25f); break;
                    case 3: editor.systems.renderPan += 64; break;
                    case 4:
                        editor.systems.renderPan = 0;
                        editor.systems.renderZoom = 1.0f;
                        break;
                    default: break;
                    }
                    editor.systems.graphInputCooldownFrames = kGraphInputCooldownFrames;
                }
                gui::set_cursor({ systemsOrigin.x, imageY });
                if (editor.systems.renderSurface.is_valid())
                    gui::image(editor.systems.renderSurface, { graphWidth, graphHeight });
                else
                    gui::wrapped_label("Render graph surface unavailable.", graphWidth);

                const float taskTitleY = imageY + graphHeight + graphGap;
                const float taskControlsY = taskTitleY + gui::line_height() + 4.0f;
                const float taskImageY = taskControlsY + 30.0f;

                gui::set_cursor({ systemsOrigin.x, taskTitleY });
                gui::label("Task / Thread Graph");
                gui::set_cursor({ systemsOrigin.x, taskControlsY });
                const std::array taskButtons{
                    gui::InlineButtonSpec{ .label = "Pan <", .width = 58.0f },
                    gui::InlineButtonSpec{ .label = "Zoom -", .width = 66.0f },
                    gui::InlineButtonSpec{ .label = "Zoom +", .width = 66.0f },
                    gui::InlineButtonSpec{ .label = "Pan >", .width = 58.0f },
                    gui::InlineButtonSpec{ .label = "Reset", .width = 58.0f }
                };
                if (const auto action = gui::inline_button_row(taskButtons, 28.0f, 6.0f);
                    action && editor.systems.graphInputCooldownFrames == 0)
                {
                    switch (*action)
                    {
                    case 0: editor.systems.taskPan = (std::max)(0, editor.systems.taskPan - 64); break;
                    case 1: editor.systems.taskZoom = (std::max)(0.85f, editor.systems.taskZoom - 0.25f); break;
                    case 2: editor.systems.taskZoom = (std::min)(3.0f, editor.systems.taskZoom + 0.25f); break;
                    case 3: editor.systems.taskPan += 64; break;
                    case 4:
                        editor.systems.taskPan = 0;
                        editor.systems.taskZoom = 1.0f;
                        break;
                    default: break;
                    }
                    editor.systems.graphInputCooldownFrames = kGraphInputCooldownFrames;
                }
                gui::set_cursor({ systemsOrigin.x, taskImageY });
                if (editor.systems.taskSurface.is_valid())
                    gui::image(editor.systems.taskSurface, { graphWidth, graphHeight });
                else
                    gui::wrapped_label("Task graph surface unavailable.", graphWidth);

                const float supportY = taskImageY + graphHeight + graphGap;
                gui::set_cursor({ systemsOrigin.x, supportY });
                gui::label("Hardware / Support Tiers");
                gui::set_cursor({ systemsOrigin.x, supportY + gui::line_height() + 4.0f });
                if (editor.systems.supportSurface.is_valid())
                    gui::image(editor.systems.supportSurface, { centerWidth, supportHeight });
                else
                    gui::wrapped_label("Support tier surface unavailable.", centerWidth);

                gui::set_cursor({ systemsOrigin.x, supportY + gui::line_height() + 4.0f + supportHeight + 10.0f });
                gui::property_row("[systems] Render stages", "Capture | Visibility | Surface | Lighting | Temporal | Present", 132.0f);
                gui::property_row("[systems] Task lanes", "Input | Systems | Scripts | AI | Output", 132.0f);
                gui::property_row("[systems] Backend guidance", backendGuidance, 132.0f);
                gui::property_row("[systems] Convergence focus", convergenceFocus, 132.0f);
                break;
            }
            case EditorMainSurface::Scene:
            case EditorMainSurface::Game2D:
            default:
                break;
            }

            gui::end_scroll_area();
        }
        gui::end_window();

        render_outliner_window();

        const bool outlinerSplitHovered = layout_outliner_visible && editor_point_in_rect(mouse, outliner_split_pos, outliner_split_size);
        const bool inspectorSplitHovered = layout_inspector_visible && editor_point_in_rect(mouse, inspector_split_pos, inspector_split_size);
        const bool bottomSplitHovered = bottom_visible && editor_point_in_rect(mouse, bottom_split_pos, bottom_split_size);
        if (layout_outliner_visible && outliner_split_size.x > 1.0f && outliner_split_size.y > 1.0f)
            gui::splitter_bar(outliner_split_pos, outliner_split_size, outlinerSplitHovered, editor.layoutDrag == EditorLayoutDrag::Outliner);
        if (layout_inspector_visible && inspector_split_size.x > 1.0f && inspector_split_size.y > 1.0f)
            gui::splitter_bar(inspector_split_pos, inspector_split_size, inspectorSplitHovered, editor.layoutDrag == EditorLayoutDrag::Inspector);
        if (bottom_visible && bottom_split_size.x > 1.0f && bottom_split_size.y > 1.0f)
            gui::splitter_bar(bottom_split_pos, bottom_split_size, bottomSplitHovered, editor.layoutDrag == EditorLayoutDrag::Dock);

        const bool show_workspace_dock = editor.showConsoleDock && bottom_h > 1.0f;
        const bool show_chat_dock = editor.showAiChat && bottom_h > 1.0f;
        const float bottom_workspace_split_w = (show_workspace_dock && show_chat_dock) ? splitter_w : 0.0f;
        const float bottom_available_w = (std::max)(0.0f, w - bottom_workspace_split_w);
        editor.workspaceSplit = std::clamp(editor.workspaceSplit, 0.30f, 0.82f);
        const float raw_left_bottom_w = bottom_available_w * editor.workspaceSplit;
        const float min_workspace_w = (std::min)(320.0f, (std::max)(0.0f, bottom_available_w * 0.50f));
        const float min_chat_w = (std::min)(280.0f, (std::max)(0.0f, bottom_available_w * 0.32f));
        const float max_workspace_w = (std::max)(min_workspace_w, bottom_available_w - min_chat_w);
        const float left_bottom_w = (show_workspace_dock && show_chat_dock)
            ? std::clamp(raw_left_bottom_w, min_workspace_w, max_workspace_w)
            : (show_workspace_dock ? bottom_available_w : 0.0f);
        const gui::Vec2 log_pos{ 0.0f, bottom_pos.y };
        const gui::Vec2 log_size{ left_bottom_w, bottom_h };
        const gui::Vec2 chat_pos{ left_bottom_w + bottom_workspace_split_w, bottom_pos.y };
        const gui::Vec2 chat_size{ show_chat_dock ? (std::max)(0.0f, w - left_bottom_w - bottom_workspace_split_w) : 0.0f, bottom_h };
        const gui::Vec2 workspace_split_pos{ left_bottom_w, bottom_pos.y };
        const gui::Vec2 workspace_split_size{ bottom_workspace_split_w, bottom_h };
        const bool workspaceSplitHovered = show_workspace_dock
            && show_chat_dock
            && editor_point_in_rect(mouse, workspace_split_pos, workspace_split_size);
        if (show_workspace_dock && show_chat_dock && gui::was_mouse_pressed() && workspaceSplitHovered)
            editor.layoutDrag = EditorLayoutDrag::Workspace;
        if (gui::is_mouse_down() && editor.layoutDrag == EditorLayoutDrag::Workspace)
            editor.workspaceSplit = std::clamp(mouse.x / (std::max)(1.0f, bottom_available_w), 0.30f, 0.82f);

        if (show_workspace_dock)
        {
        gui::begin_window("Console Dock", log_pos, log_size);
        const std::array<gui::SegmentedButtonSpec, 5> workspaceTabs{{
            { "Output", 78.0f, editor.dockStatusTab == EditorWorkspaceTab::Output },
            { "Project", 78.0f, editor.dockStatusTab == EditorWorkspaceTab::Project },
            { "Assets", 90.0f, editor.dockStatusTab == EditorWorkspaceTab::Assets || editor.dockStatusTab == EditorWorkspaceTab::Scripts },
            { "AI", 58.0f, editor.dockStatusTab == EditorWorkspaceTab::AI },
            { "Systems", 84.0f, editor.dockStatusTab == EditorWorkspaceTab::Systems }
        }};
        const std::array<EditorWorkspaceTab, 5> workspaceTabIds{{
            EditorWorkspaceTab::Output,
            EditorWorkspaceTab::Project,
            EditorWorkspaceTab::Assets,
            EditorWorkspaceTab::AI,
            EditorWorkspaceTab::Systems
        }};
        if (const auto selected = gui::tab_bar(workspaceTabs))
        {
            editor.dockStatusTab = workspaceTabIds[*selected];
        }

        auto dockLine = [](std::string_view label, std::string_view value) {
            std::string line{ label };
            line += ": ";
            line += value;
            return line;
        };
        auto renderDockStatusPanel = [&](std::string_view id, const std::vector<std::string>& lines) {
            const gui::Vec2 panelCursor = gui::cursor_position();
            const float panelHeight = (std::max)(72.0f, bottom_pos.y + bottom_h - panelCursor.y - 12.0f);
            (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                .id = std::string("editor-dock-status-") + std::string(id),
                .size = { (std::max)(180.0f, log_size.x - 24.0f), panelHeight },
                .lines = lines,
                .max_line_chars = 1024,
                .selectable = false,
                .stick_to_bottom = false
            });
        };

        const bool dockUsesOuterScroll = false;
        if (dockUsesOuterScroll)
        {
            const gui::Vec2 dockScrollStart = gui::cursor_position();
            const float dockScrollHeight = (std::max)(
                60.0f,
                bottom_pos.y + bottom_h - dockScrollStart.y - 4.0f);
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = std::string("console-dock-body-") + std::to_string(static_cast<int>(editor.dockStatusTab)),
                .size = { (std::max)(120.0f, log_size.x - 12.0f), dockScrollHeight },
                .draw_background = true,
                .show_scrollbar = true
            });
        }

        constexpr bool showDockEditorControls = false;
        switch (editor.dockStatusTab)
        {
        case EditorWorkspaceTab::Project:
        {
            const auto* activeProfile = editor_find_project_profile(editor.projectId);
            if (!activeProfile)
                activeProfile = &editor_default_project_profile();

            const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot);
            const std::filesystem::path buildLog = project_build_log_path(editor.projectRoot);

            const std::vector<std::string> dockLines{
                dockLine("[project] Active", activeProfile->display_name),
                dockLine("[project] Kind", editor.projectKind),
                dockLine("[project] Root", display_project_path(editor.projectRoot)),
                dockLine("[project] Scene", display_project_path(editor.projectScenePath)),
                dockLine("[project] Runtime", activeProfile->runtime_scene_id),
                dockLine("[project] Run backend", project_run_backend_label(editor.projectRunBackend)),
                dockLine("[project] Frame limit", frame_limit_label(editor.projectRunFrameLimitFps)),
                dockLine("[project] Output", display_project_path(outputExe)),
                dockLine("[project] Build log", display_project_path(buildLog)),
                dockLine("[project] Play target", path_exists(outputExe) ? "ready" : "build required"),
                "[project] Controls live in the central Project workspace. Bottom Dock is status-only."
            };
            renderDockStatusPanel("project", dockLines);
            break;
        }
        case EditorWorkspaceTab::Scripts:
        {
            const std::string activeScriptSource = editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            const float scriptsContentWidth = (std::max)(180.0f, log_size.x - 24.0f);
            gui::property_row("[script] Active", editor.activeScript);
            gui::property_row("[script] Source", activeScriptSource);
            gui::property_row(
                "[script] Source exists",
                std::filesystem::exists(std::filesystem::path{ activeScriptSource }) ? "true" : "false");
            if (const auto* activeScript = active_script_profile(editor))
            {
                gui::property_row("[script] Build", activeScript->build_action);
                gui::property_row("[script] Run", activeScript->run_action);
                gui::property_row("[script] Hint", activeScript->diagnostic_hint);
            }
            gui::wrapped_label(
                "Scripts compile with the engine/project and use the editor host API for callbacks. Create project-local starters here and validate them with Build Selected Script. The centered Run button builds and launches the active project shell.",
                scriptsContentWidth);
            gui::wrapped_label(editor.scriptBuildStatus, scriptsContentWidth);

            gui::property_row("[script] New script", "type a safe id, then create a project-local .ascript.cpp");
            (void)gui::edit_box(editor.newScriptName, { scriptsContentWidth, 28.0f }, 64, false);
            if (showDockEditorControls && gui::button("Create Project Script Starter", { (std::min)(280.0f, scriptsContentWidth), 30.0f }))
                create_project_script_starter(editor);

            if (showDockEditorControls)
            {
                for (const auto& script : editor_script_profiles())
                {
                    const std::string buttonLabel = std::string(script.display_name);
                    const std::string resolvedSource = editor_resolve_script_source_path(script.id, editor.projectRoot);
                    if (gui::button(buttonLabel, { (std::max)(180.0f, log_size.x - 24.0f), 28.0f }))
                    {
                        editor.activeScript = std::string(script.id);
                        editor.scriptBuildStatus = std::string("Selected script source: ") + resolvedSource;
                        (void)load_script_source_editor(editor, resolvedSource, false);
                        push_editor_log(editor, std::string("[script] Selected ") + editor.activeScript + ".");
                    }
                    gui::property_row("  source", resolvedSource);
                    gui::wrapped_label(script.description, (std::max)(160.0f, log_size.x - 36.0f));
                }
            }

            const auto scriptEntries = collect_script_browser_entries(editor.projectRoot);
            gui::property_row("[script] Project/engine script files", std::to_string(scriptEntries.size()));
            if (showDockEditorControls)
            {
                for (const auto& entry : scriptEntries)
                {
                    if (gui::button(entry.label, { scriptsContentWidth, 28.0f }))
                    {
                        editor.activeScript = entry.kind;
                        editor.selectedProjectFile = entry.path;
                        editor.scriptBuildStatus = "Selected script source: " + entry.path;
                        push_editor_log(editor, "[script] Selected source " + entry.path);
                    }
                }
            }

            if (showDockEditorControls && gui::button("Build Selected Script", { 180.0f, 30.0f }))
            {
                if (editor.scriptEditorDirty)
                    (void)save_script_source_editor(editor);
                const auto build = editor_build_script(editor.activeScript, editor.projectRoot);
                editor.scriptBuildStatus = build.summary;
                push_editor_log(
                    editor,
                    std::string("[script] ")
                    + (build.succeeded ? "Validation passed. " : "Validation failed. ")
                    + build.summary);
                append_project_note(
                    editor,
                    "Build Selected Script",
                    build.summary,
                    build.succeeded ? "Script validation passed against the active project shell." : "Script validation failed; inspect script diagnostics before running.");
            }

            const auto projectEntries = collect_project_browser_entries(editor.projectRoot);
            gui::property_row("[files] Active project browser", std::to_string(projectEntries.size()) + " visible entries");
            gui::wrapped_label("Console Dock is status-only. Use the central Asset Browser / Scripts surface for file browsing, script editing, and build actions.", scriptsContentWidth);
            if (showDockEditorControls)
            {
                for (const auto& entry : projectEntries)
                {
                    if (gui::button(entry.label, { scriptsContentWidth, 26.0f }))
                    {
                        editor.selectedProjectFile = entry.path;
                        if (entry.kind == "SCRIPT")
                        {
                            editor.activeScript = script_id_from_source_path(std::filesystem::path{ entry.path });
                            editor.scriptBuildStatus = "Selected project script: " + entry.path;
                            (void)load_script_source_editor(editor, entry.path, false);
                        }
                        push_editor_log(editor, "[files] Selected " + entry.path);
                    }
                }
                draw_script_source_editor(editor, activeScriptSource, scriptsContentWidth, 180.0f);
            }
            gui::property_row("[files] Selected", editor.selectedProjectFile.empty() ? std::string("(none)") : editor.selectedProjectFile);
            break;
        }
        case EditorWorkspaceTab::Assets:
        {
            const auto modelSummary = editor_project_model_summary(editor.projectId);
            const std::string activeScriptSource = editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            const std::vector<std::string> dockLines{
                dockLine("[assets] Project", editor.projectName),
                dockLine("[assets] Project root", display_project_path(editor.projectRoot)),
                dockLine("[assets] Scene", display_project_path(editor.projectScenePath)),
                dockLine("[assets] Demo model", modelSummary.asset_path.empty() ? std::string("(none)") : modelSummary.asset_path),
                dockLine("[assets] Model path", modelSummary.resolved_path.empty() ? std::string("(unresolved)") : modelSummary.resolved_path),
                dockLine("[assets] Model parsed", modelSummary.parsed ? "true" : "false"),
                dockLine("[assets] Selected", editor.selectedAssetPath.empty() ? std::string("(none)") : editor.selectedAssetPath),
                dockLine("[script asset] Active", editor.activeScript),
                dockLine("[script asset] Source", activeScriptSource),
                dockLine("[script asset] Source exists", std::filesystem::exists(std::filesystem::path{ activeScriptSource }) ? "true" : "false"),
                "[assets] Use the central Asset Browser for file cards, script creation, validation, and source editing."
            };
            renderDockStatusPanel("assets", dockLines);
            break;
        }
        case EditorWorkspaceTab::AI:
        {
            const auto manifest = epoch::ai::active_model_manifest();
            const auto training = epoch::ai::default_training_paths();
            const std::filesystem::path buildLog = project_build_log_path(editor.projectRoot);
            const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot);
            const std::filesystem::path pathsManifest = resolve_editor_path(std::filesystem::path{ editor.projectRoot }) / "project.paths.txt";
            const std::string activeScriptSource = editor_resolve_script_source_path(editor.activeScript, editor.projectRoot);
            const std::string latestPrompt = last_chat_line_with_prefix(chat, "you> ");
            const std::string latestReply = last_chat_line_with_prefix(chat, "ai> ");
            const auto gateStatus = summarize_ai_review_gate(
                editor,
                buildLog,
                outputExe,
                pathsManifest,
                training,
                latestPrompt,
                latestReply);
            const std::string loopStage = ai_control_loop_stage(gateStatus);

            // Bottom Dock > AI stays diagnostic-only. Controls and model
            // selection live in the central Intelligence and Inspector panes.

            const auto currentIterationPacket = [&]() {
                std::vector<std::string> evidencePaths;
                auto addEvidence = [&](const std::string& path) {
                    if (path.empty())
                        return;
                    if (std::find(evidencePaths.begin(), evidencePaths.end(), path) == evidencePaths.end())
                        evidencePaths.push_back(path);
                };

                addEvidence(editor.projectManifest);
                addEvidence(editor.projectRoot);
                addEvidence(editor.projectScenePath);
                addEvidence(activeScriptSource);
                addEvidence(pathsManifest.string());
                addEvidence(buildLog.string());
                addEvidence(outputExe.string());
                addEvidence(training.local_capture_jsonl);
                addEvidence(training.mcp_capture_jsonl);
                addEvidence(training.curated_dataset_root);
                addEvidence(training.eval_root);
                addEvidence(manifest.manifest_path);

                return epoch::ai::IterationPacket{
                    .packet_name = editor.projectId.empty() ? std::string("epoch-iteration") : editor.projectId + "-iteration",
                    .task_prompt = latestPrompt.empty() ? build_ai_self_iteration_prompt(editor) : latestPrompt,
                    .assistant_hint = latestReply == "(empty reply)" ? std::string{} : latestReply,
                    .operator_notes = editor.projectBuildStatus.empty()
                        ? editor.projectStatus
                        : (editor.projectStatus.empty()
                            ? editor.projectBuildStatus
                            : editor.projectBuildStatus + " | " + editor.projectStatus),
                    .control_loop_stage = loopStage,
                    .review_gate_state = gateStatus.promotionSummary,
                    .review_gate_evidence = gateStatus.packetEvidenceSummary,
                    .project_id = editor.projectId,
                    .project_name = editor.projectName,
                    .scene_id = editor.activeRuntimeScene,
                    .project_root = editor.projectRoot,
                    .scene_path = editor.projectScenePath,
                    .active_script = editor.activeScript,
                    .build_log_path = buildLog.string(),
                    .output_path = outputExe.string(),
                    .provider_summary = epoch::ai::active_provider_summary(),
                    .active_model = manifest.display_name,
                    .manifest_path = manifest.manifest_path,
                    .workspace_root = training.workspace_root,
                    .raw_capture_path = training.local_capture_jsonl,
                    .mcp_capture_path = training.mcp_capture_jsonl,
                    .checkpoint_root = training.checkpoint_root,
                    .model_root = training.model_root,
                    .cache_root = training.cache_root,
                    .curated_dataset_root = training.curated_dataset_root,
                    .eval_root = training.eval_root,
                    .evidence_paths = std::move(evidencePaths)
                };
            };

            bool aiBuildCompletedThisFrame = false;
            if (editor.aiContinuousBuildPending
                && editor.aiContinuousBuildPending->wait_for(0ms) == std::future_status::ready)
            {
                try
                {
                    const auto build = editor.aiContinuousBuildPending->get();
                    editor.aiContinuousBuildPending.reset();
                    aiBuildCompletedThisFrame = true;
                    ++editor.aiContinuousBuildRunCount;
                    editor.projectBuildStatus = build.summary;
                    editor.aiContinuousBuildStatus = build.succeeded
                        ? "Last build passed; packet staging pending."
                        : "Last build failed; inspect the build log before promotion.";
                    push_editor_log(
                        editor,
                        std::string("[ai-build] ")
                        + (build.succeeded ? "Build passed. " : "Build failed. ")
                        + build.summary);
                    if (!build.output_path.empty())
                        push_editor_log(editor, "[ai-build] Output: " + build.output_path);
                    if (!build.log_path.empty())
                        push_editor_log(editor, "[ai-build] Log: " + build.log_path);
                    append_project_note(
                        editor,
                        "Self-Iteration Build Completed",
                        build.summary,
                        build.succeeded ? "Build evidence is available for packet staging and verifier/gate review." : "Build failed; review the log before allowing another self-iteration pass.");
                    editor.aiContinuousBuildStageOnNextFrame = build.succeeded;
                }
                catch (const std::exception& e)
                {
                    editor.aiContinuousBuildPending.reset();
                    aiBuildCompletedThisFrame = true;
                    editor.aiContinuousBuildStatus = std::string("Build threw: ") + e.what();
                    push_editor_log(editor, "[ai-build] Build threw: " + std::string(e.what()));
                }
            }

            if (editor.aiContinuousBuildStageOnNextFrame && !editor.aiContinuousBuildPending && !aiBuildCompletedThisFrame)
            {
                editor.aiContinuousBuildStageOnNextFrame = false;
                const std::string packetDir = epoch::ai::stage_iteration_packet(currentIterationPacket());
                if (packetDir.empty())
                {
                    editor.aiContinuousBuildStatus = "Build passed, but packet staging failed.";
                    push_editor_log(editor, "[ai-build] Failed to stage post-build packet.");
                }
                else
                {
                    editor.aiContinuousBuildStatus = "Build passed; packet staged: " + packetDir;
                    push_editor_log(editor, "[ai-build] Staged post-build AI packet.");
                    push_editor_log(editor, "[ai-build] Packet: " + packetDir);
                    append_project_note(
                        editor,
                        "Post-Build Iteration Packet Staged",
                        std::string("Packet staged at ") + packetDir,
                        "Review the staged packet before promoting data or applying any repo-changing pass.");
                }
            }

            const std::vector<std::string> dockLines{
                "AI Diagnostics",
                dockLine("[ai] Domain", ai_workspace_domain_name(editor.aiWorkspaceDomain)),
                dockLine("[model] Provider", epoch::ai::active_provider_summary()),
                dockLine("[model] Selected", epoch::ai::active_model_name().empty() ? "(none selected)" : epoch::ai::active_model_name()),
                dockLine("[model] Client", epoch::ai::model_connection_status()),
                dockLine("[ai] Loop stage", loopStage),
                dockLine("[ai] Evidence", gateStatus.packetEvidenceSummary),
                dockLine("[ai-build] Mode", "manual evidence gate"),
                dockLine("[ai-build] Pending", editor.aiContinuousBuildPending ? "true" : "false"),
                dockLine("[ai-build] Runs", std::to_string(editor.aiContinuousBuildRunCount)),
                dockLine("[ai-build] Status", editor.aiContinuousBuildStatus),
                dockLine("[ai-tool] Runs", std::to_string(editor.aiToolHarnessRunCount)),
                dockLine("[ai-tool] Status", editor.aiToolHarnessStatus),
                dockLine("[ai] Build log", display_project_path(buildLog)),
                dockLine("[ai] Output", display_project_path(outputExe)),
                "[ai] Controls and model selection live in Intelligence and Inspector; Bottom Dock is status-only."
            };
            renderDockStatusPanel("ai", dockLines);
            break;
        }
        case EditorWorkspaceTab::Systems:
        {
            const auto orderedSystems = epoch::systems::Registry::instance().ordered_systems();
            const std::size_t hardwareThreadCount = (std::max)(std::size_t{ 1 },
                std::thread::hardware_concurrency() > 0
                ? static_cast<std::size_t>(std::thread::hardware_concurrency())
                : std::size_t{ 6 });
            const std::size_t liveThreadCount = epoch::systems::threading::live_thread_count();
            const std::string supportTier = recommended_support_tier(ctx, hardwareThreadCount);
            const std::filesystem::path phase5PacketRoot{ epoch::ai::iteration_packet_root() };

            const std::vector<std::string> dockLines{
                dockLine("[systems] Renderer", renderer_name(ctx)),
                dockLine("[systems] Ownership model", backend_ownership_model(ctx)),
                dockLine("[systems] Preview camera", preview_camera_name(ctx)),
                dockLine("[systems] Runtime target", editor.activeRuntimeScene),
                dockLine("[systems] Registered systems", std::to_string(orderedSystems.size)),
                dockLine("[systems] Live threads", std::to_string(liveThreadCount)),
                dockLine("[systems] CPU threads", std::to_string(hardwareThreadCount)),
                dockLine("[systems] Support tier", supportTier),
                dockLine("[visual] Profile", std::string(epochnamespace::visuals::active_profile_name())),
                dockLine("[visual] Parity gate", std::string(epochnamespace::visuals::parity_gate())),
                dockLine("[renderer] Resource spine", renderer_resource_spine_summary(ctx)),
                dockLine("[renderer] Sampled RTT", renderer_native_sampled_rtt_status(ctx)),
                dockLine("[renderer] Next gate", renderer_next_feature_gate(ctx)),
                dockLine("[build] Compiler", compiler_identity()),
                dockLine("[build] Configuration", build_configuration_label()),
                dockLine("[phase5] Gate", "manual evidence gate"),
                dockLine("[phase5] Build status", editor.aiContinuousBuildStatus),
                dockLine("[phase5] Staged packets", staged_packet_count_summary(phase5PacketRoot)),
                "[systems] Use the central System Info workspace for graph surfaces, backend details, and live system lists. Video owns time controls and timeline/video authoring."
            };
            renderDockStatusPanel("systems", dockLines);
            break;
        }
        case EditorWorkspaceTab::Output:
        default:
            {
                const gui::Vec2 outputCursor = gui::cursor_position();
                const float outputHeight = (std::max)(72.0f, bottom_pos.y + bottom_h - outputCursor.y - 12.0f);
                (void)gui::scroll_text_panel(gui::ScrollTextPanelOptions{
                    .id = "editor-output-log",
                    .size = { (std::max)(180.0f, log_size.x - 24.0f), outputHeight },
                    .lines = editor.logLines,
                    .max_line_chars = 1024,
                    .selectable = true,
                    .stick_to_bottom = true
                });
            }
            break;
        }
        if (dockUsesOuterScroll)
            gui::end_scroll_area();
        gui::end_window();
        }

        if (show_workspace_dock && show_chat_dock && workspace_split_size.x > 1.0f && workspace_split_size.y > 1.0f)
            gui::splitter_bar(workspace_split_pos, workspace_split_size, workspaceSplitHovered, editor.layoutDrag == EditorLayoutDrag::Workspace);

        if (show_chat_dock)
        {
        gui::ConsoleWindowOptions opts{
            .title = "AI Chat",
            .position = chat_pos,
            .size = chat_size,
            .lines = chat.lines,
            .max_visible_lines = 180,
            .input = &chat.input,
            .max_input_chars = 1024,
            .multiline_input = false,
            .show_send_button = true,
            .send_button_enabled = !chat.pending.has_value(),
            .send_button_width = 88.0f,
            .send_button_label = chat.pending ? "Send > (busy)" : "Send >",
        };

        gui::ConsoleWindowResult r = gui::console_window(opts);
        if ((r.input.submitted || r.send_clicked) && !chat.pending.has_value())
        {
            std::string text = std::move(chat.input);
            chat.input.clear();
            chat.submit(std::move(text));
        }
        }

        render_inspector_window();

        open_dropdown("File", TopMenu::File, dropdown_window_size(192.0f, 4), [&](gui::Vec2 pos)
        {
            menu_item("Open Launcher", { pos.x + 12.0f, pos.y + 14.0f }, 192.0f, [&]() {
                emit_command(EditorCommand::OpenLauncher);
                push_editor_log(editor, "[file] Opening launcher.");
            });
            menu_item("Save Project", { pos.x + 12.0f, pos.y + 48.0f }, 192.0f, [&]() {
                repair_active_project_evidence(editor);
            });
            menu_item("Settings", { pos.x + 12.0f, pos.y + 82.0f }, 192.0f, [&]() {
                emit_command(EditorCommand::Settings);
                editor.openMenu = TopMenu::None;
                editor.showSettingsModal = true;
                push_editor_log(editor, "[file] Settings selected.");
            });
            menu_item("Exit", { pos.x + 12.0f, pos.y + 116.0f }, 192.0f, [&]() {
                emit_command(EditorCommand::Exit);
                push_editor_log(editor, "[file] Exit selected.");
            });
        });

        open_dropdown("Edit", TopMenu::Edit, dropdown_window_size(192.0f, 3), [&](gui::Vec2 pos)
        {
            menu_item("Focus Selection", { pos.x + 12.0f, pos.y + 14.0f }, 192.0f, [&]() {
                handle_scene_tool(editor, "focus_selection");
            });
            menu_item("Reset Camera", { pos.x + 12.0f, pos.y + 48.0f }, 192.0f, [&]() {
                handle_scene_tool(editor, "reset_camera");
                epochnamespace::previewgrid::reset_camera(ctx.get());
            });
            menu_item("Toggle Helpers", { pos.x + 12.0f, pos.y + 82.0f }, 192.0f, [&]() {
                handle_scene_tool(editor, "toggle_helpers");
            });
        });

        open_dropdown("Asset", TopMenu::Asset, dropdown_window_size(220.0f, 7), [&](gui::Vec2 pos)
        {
            menu_item("Open Asset Browser", { pos.x + 12.0f, pos.y + 14.0f }, 220.0f, [&]() {
                open_editor_surface(EditorMainSurface::Assets, "Asset menu");
            });
            menu_item("Package Manager...", { pos.x + 12.0f, pos.y + 48.0f }, 220.0f, [&]() {
                editor.openMenu = TopMenu::None;
                editor.showPackageManagerModal = true;
                editor.workspaceTab = EditorWorkspaceTab::Assets;
                push_editor_log(editor, "[assets] Package Manager opened.");
            });
            menu_item("Add Static Mesh", { pos.x + 12.0f, pos.y + 82.0f }, 220.0f, [&]() {
                add_entity(editor, "cube");
            });
            menu_item("Add Light", { pos.x + 12.0f, pos.y + 116.0f }, 220.0f, [&]() {
                add_entity(editor, "light");
            });
            menu_item("Add Spawn", { pos.x + 12.0f, pos.y + 150.0f }, 220.0f, [&]() {
                add_entity(editor, "spawn");
            });
            menu_item("Duplicate Selected", { pos.x + 12.0f, pos.y + 184.0f }, 220.0f, [&]() {
                duplicate_selected_entity(editor);
            });
            menu_item("Delete Selected", { pos.x + 12.0f, pos.y + 218.0f }, 220.0f, [&]() {
                delete_selected_entity(editor);
            });
        });

        open_dropdown("Window", TopMenu::Window, dropdown_window_size(248.0f, 5), [&](gui::Vec2 pos)
        {
            menu_item(editor.showOutliner ? "Hide Outliner" : "Show Outliner", { pos.x + 12.0f, pos.y + 14.0f }, 248.0f, [&]() {
                editor.showOutliner = !editor.showOutliner;
                push_editor_log(editor, editor.showOutliner ? "[window] World Outliner shown." : "[window] World Outliner hidden.");
            });
            menu_item(editor.showInspector ? "Hide Inspector" : "Show Inspector", { pos.x + 12.0f, pos.y + 48.0f }, 248.0f, [&]() {
                editor.showInspector = !editor.showInspector;
                push_editor_log(editor, editor.showInspector ? "[window] Inspector shown." : "[window] Inspector hidden.");
            });
            menu_item(editor.showConsoleDock ? "Hide Console Dock" : "Show Console Dock", { pos.x + 12.0f, pos.y + 82.0f }, 248.0f, [&]() {
                editor.showConsoleDock = !editor.showConsoleDock;
                push_editor_log(editor, editor.showConsoleDock ? "[window] Console Dock shown." : "[window] Console Dock hidden.");
            });
            menu_item(editor.showAiChat ? "Hide AI Chat" : "Show AI Chat", { pos.x + 12.0f, pos.y + 116.0f }, 248.0f, [&]() {
                editor.showAiChat = !editor.showAiChat;
                push_editor_log(editor, editor.showAiChat ? "[window] AI Chat shown." : "[window] AI Chat hidden.");
            });
            menu_item("Reset Editor Layout", { pos.x + 12.0f, pos.y + 150.0f }, 248.0f, [&]() {
                reset_editor_layout(editor);
                push_editor_log(editor, "[window] Editor layout reset.");
            });
        });

        open_dropdown("Tools", TopMenu::Tools, dropdown_window_size(228.0f, 5), [&](gui::Vec2 pos)
        {
            menu_item("Camera: Editor", { pos.x + 12.0f, pos.y + 14.0f }, 228.0f, [&]() {
                editor.projectCameraMode = epochnamespace::previewgrid::CameraMode::Editor;
                epochnamespace::previewgrid::set_camera_mode(ctx.get(), editor.projectCameraMode);
                push_editor_log(editor, "[tools] Camera mode set to Editor.");
            });
            menu_item("Camera: FPS", { pos.x + 12.0f, pos.y + 48.0f }, 228.0f, [&]() {
                editor.projectCameraMode = epochnamespace::previewgrid::CameraMode::FPS;
                epochnamespace::previewgrid::set_camera_mode(ctx.get(), editor.projectCameraMode);
                push_editor_log(editor, "[tools] Camera mode set to FPS.");
            });
            menu_item("Camera: 2D Canvas", { pos.x + 12.0f, pos.y + 82.0f }, 228.0f, [&]() {
                editor.projectCameraMode = epochnamespace::previewgrid::CameraMode::Canvas2D;
                open_editor_surface(EditorMainSurface::Game2D, "Tools menu");
                push_editor_log(editor, "[tools] Camera mode set to locked 2D Canvas.");
            });
            menu_item("Reset Preview Camera", { pos.x + 12.0f, pos.y + 116.0f }, 228.0f, [&]() {
                epochnamespace::previewgrid::reset_camera(ctx.get());
                push_editor_log(editor, "[tools] Preview camera reset.");
            });
            menu_item("Save Project", { pos.x + 12.0f, pos.y + 150.0f }, 228.0f, [&]() {
                repair_active_project_evidence(editor);
            });
        });

        open_dropdown("Help", TopMenu::Help, dropdown_window_size(192.0f, 2), [&](gui::Vec2 pos)
        {
            menu_item("About Epoch", { pos.x + 12.0f, pos.y + 14.0f }, 192.0f, [&]() {
                editor.openMenu = TopMenu::None;
                editor.showAboutModal = true;
            });
            menu_item("Current Project Info", { pos.x + 12.0f, pos.y + 48.0f }, 192.0f, [&]() {
                push_editor_log(editor, std::string("[help] Active project: ") + editor.projectName);
            });
        });

        if (editor.showUpdateConfirmModal)
        {
            editor.openMenu = TopMenu::None;
            const bool sourceOnlyUpdate =
                editor.lastUpdateCheck.source_update_available
                && !editor.lastUpdateCheck.packaged_update_available;
            const bool sourceWorkerRunning = editor.updateState == EditorUpdateState::SourceWorkerRunning;
            const bool updateRunning = editor.updateCheckPending.has_value() || sourceWorkerRunning;
            const bool restartReady = editor.updateState == EditorUpdateState::RestartReady;
            if (restartReady)
                arm_editor_update_restart_countdown(editor);
            const int restartSeconds = restartReady ? editor_update_restart_countdown_seconds(editor) : 0;
            const gui::Vec2 modalSize = updateConfirmModalSize;
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            constexpr float modalContentInset = 28.0f;
            constexpr float buttonHeight = 30.0f;
            constexpr float buttonBottomPad = 18.0f;
            constexpr float buttonTopPad = 12.0f;
            const float contentX = modalPos.x + modalContentInset;
            const float contentRight = modalPos.x + modalSize.x - modalContentInset;
            const float contentWidth = (std::max)(1.0f, contentRight - contentX);
            const float buttonY = modalPos.y + modalSize.y - buttonHeight - buttonBottomPad;
            const float contentBottom = (std::max)(modalPos.y + 64.0f, buttonY - buttonTopPad);
            const std::string updateStatusLine = [&]() {
                std::string text = editor.updateStatus;
                constexpr std::size_t kMaxModalStatus = 176u;
                if (text.size() <= kMaxModalStatus)
                    return text;
                return text.substr(0u, kMaxModalStatus - 3u) + "...";
            }();
            const auto requestUpdateRestart = [&]() {
                editor.showUpdateConfirmModal = false;
                editor.updateStatus = "Restarting Epoch to finish the staged update handoff.";
                clear_editor_update_restart_countdown(editor);
                push_editor_log(editor, "[update] Restart requested after verified update handoff.");
                emit_command(EditorCommand::Exit);
            };
            if (restartReady && editor_update_restart_countdown_elapsed(editor))
                requestUpdateRestart();
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Update Epoch",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 contentPos = gui::cursor_position();
            float cursorY = contentPos.y;
            const auto emitWrapped = [&](const std::string_view text, const float gap) {
                if (cursorY >= contentBottom)
                    return;
                const float textHeight = gui::wrapped_text_height(text, contentWidth);
                if (cursorY + textHeight > contentBottom)
                    return;
                gui::set_cursor({ contentX, cursorY });
                gui::wrapped_label(text, contentWidth);
                cursorY += textHeight + gap;
            };
            const std::string introText = sourceOnlyUpdate
                ? "No packaged runtime was found for this platform, so Epoch is using the source rebuild lane."
                : "A newer packaged Epoch runtime is available. Epoch will download, verify, stage, and hand off the replacement.";
            emitWrapped(introText, 8.0f);
            emitWrapped(updateStatusLine, 10.0f);
            if (cursorY + 22.0f <= contentBottom)
            {
                const float progressWidth = (std::max)(1.0f, (std::min)(contentWidth, 560.0f));
                gui::set_cursor({ contentX, cursorY });
                gui::progress_bar(gui::ProgressBarOptions{
                    .label = sourceWorkerRunning ? "Source rebuild" : updateRunning ? "Update" : restartReady ? "Update staged" : "Update ready",
                    .status = sourceWorkerRunning ? "cancel available" : updateRunning ? "downloading / staging" : restartReady ? "restart required" : "waiting",
                    .value = editor_update_progress_value(editor),
                    .size = { progressWidth, 22.0f },
                    .show_percent = true
                });
                cursorY += 36.0f;
            }
            const std::string cacheText = sourceOnlyUpdate
                ? "Smart Update checked packaged releases first; source rebuild is the available lane for this platform."
                : "Cached packages are checked before use; stale or broken downloads are replaced.";
            emitWrapped(cacheText, 8.0f);
            const std::string actionText = restartReady
                ? std::format(
                    "The update is staged. Epoch will restart automatically in {} second{}; press Restart to finish now.",
                    restartSeconds,
                    restartSeconds == 1 ? "" : "s")
                : sourceWorkerRunning
                    ? "Keep Epoch open while the source worker runs. Cancel stops at the next safe checkpoint."
                    : sourceOnlyUpdate
                        ? "Use Update From Source to build the newer source locally, or Cancel to stay on this build."
                        : "Install Release is recommended. Advanced Source is only for intentionally building latest main locally.";
            emitWrapped(actionText, 8.0f);
            const float cancelButtonWidth = sourceWorkerRunning ? 148.0f : 120.0f;
            const float primaryButtonWidth = (std::min)(220.0f, (std::max)(160.0f, contentWidth * 0.34f));
            const float advancedButtonWidth = (std::min)(190.0f, (std::max)(156.0f, contentWidth * 0.28f));
            const float buttonGap = 16.0f;
            const float primaryButtonX = sourceWorkerRunning
                ? contentX
                : (std::min)(contentRight - primaryButtonWidth, contentX + cancelButtonWidth + buttonGap);
            const float advancedButtonX = (std::max)(contentX, contentRight - advancedButtonWidth);
            gui::set_cursor({ contentX, buttonY });
            if (sourceWorkerRunning)
            {
                if (gui::button("Cancel Update", { cancelButtonWidth, buttonHeight }))
                {
                    const bool cancelRequested = updater::request_source_update_cancel();
                    editor.showUpdateConfirmModal = false;
                    editor.updateState = cancelRequested ? EditorUpdateState::Available : EditorUpdateState::Failed;
                    editor.updateInstallPending = false;
                    editor.updateSourceInstallPending = false;
                    editor.updateOperationStartedAt = {};
                    editor.updateStatus = cancelRequested
                        ? "Source update cancel requested. The worker will stop at its next safe checkpoint; Update remains available for retry."
                        : "Source update modal closed, but the cancel marker could not be written; check updater logs.";
                    push_editor_log(editor, std::string{ "[update] " } + editor.updateStatus);
                }
            }
            else if (!updateRunning && !restartReady && gui::button("Cancel", { 120.0f, 30.0f }))
            {
                editor.showUpdateConfirmModal = false;
                push_editor_log(editor, "[command] Update canceled.");
            }
            gui::set_cursor({ primaryButtonX, buttonY });
            const std::string primaryUpdateLabel = restartReady
                ? std::format("Restart Now ({})", restartSeconds)
                : sourceOnlyUpdate ? std::string{ "Update From Source" } : std::string{ "Install Release" };
            if (!updateRunning && gui::button(primaryUpdateLabel, { primaryButtonWidth, buttonHeight }))
            {
                if (restartReady)
                {
                    requestUpdateRestart();
                }
                else
                {
                    push_editor_log(editor, "[command] Smart update confirmed.");
                    start_editor_update_install(editor);
                }
            }
            const bool showAdvancedSourceButton = !updateRunning && !restartReady;
            gui::set_cursor({ advancedButtonX, buttonY });
            if (showAdvancedSourceButton && gui::button("Advanced Source...", { advancedButtonWidth, buttonHeight }))
            {
                editor.showUpdateConfirmModal = false;
                editor.showSourceUpdateConfirmModal = true;
                push_editor_log(editor, "[command] Advanced source rebuild requested. Awaiting confirmation.");
            }
            gui::end_modal_window();
        }

        if (editor.showSourceUpdateConfirmModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize = sourceUpdateConfirmModalSize;
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            constexpr float modalContentInset = 24.0f;
            const float contentWidth = modalSize.x - 2.0f * modalContentInset;
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Rebuild From Main Source",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 contentPos = gui::cursor_position();
            const float contentY = contentPos.y;
            gui::set_cursor({ contentPos.x + 8.0f, contentY });
            gui::wrapped_label("Advanced Source skips the packaged runtime and rebuilds the latest main source locally.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 36.0f });
            gui::wrapped_label("This is slower and riskier than Install Release. It is for source testing, not the default update path.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 84.0f });
            gui::wrapped_label("Epoch deletes stale source snapshots before downloading, restores dependencies, rebuilds, and records handoff evidence.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 132.0f });
            gui::wrapped_label("For normal users, press Back and choose Install Release.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentPos.y + 184.0f });
            if (gui::button("Back", { 120.0f, 30.0f }))
            {
                editor.showSourceUpdateConfirmModal = false;
                editor.showUpdateConfirmModal = true;
            }
            gui::set_cursor({ contentPos.x + 148.0f, contentPos.y + 184.0f });
            if (gui::button("Cancel", { 120.0f, 30.0f }))
            {
                editor.showSourceUpdateConfirmModal = false;
                push_editor_log(editor, "[command] Advanced source rebuild canceled.");
            }
            gui::set_cursor({ contentPos.x + 284.0f, contentPos.y + 184.0f });
            if (gui::button("Start Source Rebuild", { 176.0f, 30.0f }))
            {
                editor.showSourceUpdateConfirmModal = false;
                editor.showUpdateConfirmModal = true;
                push_editor_log(editor, "[command] Advanced source rebuild confirmed.");
                start_editor_source_update_install(editor);
            }
            gui::end_modal_window();
        }

        // Enables deterministic smoke coverage of GUI-only updater actions.
        if (!editor.automationConsumed)
        {
            if (editor.automationCommand != EditorAutomationCommand::None
                && !try_claim_editor_automation_command(editor.automationCommand))
            {
                editor.automationConsumed = true;
            }

            switch (editor.automationCommand)
            {
            case EditorAutomationCommand::SmartUpdate:
                if (editor.automationConsumed)
                    break;
                editor.automationConsumed = true;
                push_editor_log(editor, "[command] Auto command triggered: smart update.");
                append_editor_automation_trace("triggered smart-update");
                start_editor_update_install(editor);
                break;
            case EditorAutomationCommand::SourceUpdate:
                if (editor.automationConsumed)
                    break;
                editor.automationConsumed = true;
                push_editor_log(editor, "[command] Auto command triggered: advanced source rebuild.");
                append_editor_automation_trace("triggered source-update");
                start_editor_source_update_install(editor);
                break;
            case EditorAutomationCommand::None:
                break;
            }
        }

        if (editor.showSettingsModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize{ 600.0f, 462.0f };
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Editor Settings",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 contentPos = gui::cursor_position();
            const float contentY = contentPos.y;
            gui::set_cursor({ contentPos.x + 8.0f, contentY });
            gui::wrapped_label(
                "Runtime-safe editor settings live here first. Layout, workspace, preview, and OS AI controls stay visible and reviewable instead of hidden in console output.",
                contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 54.0f });
            gui::property_row("[settings] Renderer", renderer_name(ctx), 148.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 78.0f });
            gui::property_row("[settings] Project", editor.projectName, 148.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 102.0f });
            gui::property_row("[settings] Workspace", std::string(main_surface_title(editor.mainSurface)), 148.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 126.0f });
            gui::property_row("[settings] Preview", std::string(preview_mode_name(editor.previewMode)), 148.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 150.0f });
            gui::property_row("[settings] AI model", epoch::ai::active_model_name().empty() ? "(none selected)" : epoch::ai::active_model_name(), 148.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 176.0f });
            const auto settingsThemeChoices = gui::theme_preference_choices();
            std::vector<std::string_view> settingsThemeLabels;
            settingsThemeLabels.reserve(settingsThemeChoices.size());
            for (const auto& choice : settingsThemeChoices)
                settingsThemeLabels.emplace_back(choice.label);
            const auto settingsThemeSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "editor-theme-select",
                .placeholder = "Choose editor theme",
                .selected = gui::theme_preference_label(editor.themePreference),
                .options = std::span<const std::string_view>{ settingsThemeLabels.data(), settingsThemeLabels.size() },
                .size = { 260.0f, 30.0f },
                .row_height = 28.0f,
                .max_visible_options = 3
            });
            if (settingsThemeSelect.changed && settingsThemeSelect.selected_index && *settingsThemeSelect.selected_index < settingsThemeChoices.size())
            {
                editor.themePreference = settingsThemeChoices[*settingsThemeSelect.selected_index].preference;
                push_editor_log(editor, std::string("[settings] Theme set to ") + std::string(gui::theme_preference_label(editor.themePreference)) + ".");
            }
            gui::set_cursor({ contentPos.x + 286.0f, contentY + 180.0f });
            gui::property_row("[settings] Theme", std::string(gui::theme_preference_label(editor.themePreference)), 108.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 222.0f });
            const auto settingsInputChoices = input_profile_choices();
            std::vector<std::string_view> settingsInputLabels;
            settingsInputLabels.reserve(settingsInputChoices.size());
            for (const auto& choice : settingsInputChoices)
                settingsInputLabels.emplace_back(choice.label);
            const auto settingsInputSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "editor-input-profile-select",
                .placeholder = "Choose shared input profile",
                .selected = input_profile_label(editor.inputProfilePreset),
                .options = std::span<const std::string_view>{ settingsInputLabels.data(), settingsInputLabels.size() },
                .size = { 260.0f, 30.0f },
                .row_height = 28.0f,
                .max_visible_options = 4
            });
            if (settingsInputSelect.changed && settingsInputSelect.selected_index && *settingsInputSelect.selected_index < settingsInputChoices.size())
            {
                editor.inputProfilePreset = settingsInputChoices[*settingsInputSelect.selected_index].preset;
                input::set_active_profile(editor.inputProfilePreset);
                push_editor_log(editor, std::string("[input] Shared profile set to ") + std::string(input_profile_label(editor.inputProfilePreset)) + ".");
            }
            gui::set_cursor({ contentPos.x + 286.0f, contentY + 226.0f });
            gui::property_row("[settings] Input", std::string(input_profile_label(editor.inputProfilePreset)), 108.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 264.0f });
            const auto settingsLimitChoices = frame_limit_choices();
            std::vector<std::string_view> settingsLimitLabels;
            settingsLimitLabels.reserve(settingsLimitChoices.size());
            for (const auto& choice : settingsLimitChoices)
                settingsLimitLabels.emplace_back(choice.label);
            const auto settingsLimitSelect = gui::select_box(gui::SelectBoxOptions{
                .id = "editor-core-frame-limit-select",
                .placeholder = "Choose editor frame limit",
                .selected = frame_limit_label(editor.editorFrameLimitFps),
                .options = std::span<const std::string_view>{ settingsLimitLabels.data(), settingsLimitLabels.size() },
                .size = { 240.0f, 30.0f },
                .row_height = 28.0f,
                .max_visible_options = 3
            });
            if (settingsLimitSelect.changed && settingsLimitSelect.selected_index && *settingsLimitSelect.selected_index < settingsLimitChoices.size())
            {
                editor.editorFrameLimitFps = settingsLimitChoices[*settingsLimitSelect.selected_index].fps;
                core::cli::frame_limit_explicit = true;
                core::cli::frame_limit_fps = editor.editorFrameLimitFps;
                push_editor_log(editor, std::string("[settings] Editor frame limit set to ") + std::string(settingsLimitChoices[*settingsLimitSelect.selected_index].label) + ".");
            }
            gui::set_cursor({ contentPos.x + 286.0f, contentY + 268.0f });
            gui::property_row("[settings] Frame limit", std::string(frame_limit_label(editor.editorFrameLimitFps)), 148.0f);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 332.0f });
            if (gui::button("Reset Layout", { 132.0f, 30.0f }))
            {
                reset_editor_layout(editor);
                push_editor_log(editor, "[settings] Editor layout reset.");
            }
            gui::set_cursor({ contentPos.x + 150.0f, contentY + 332.0f });
            if (gui::button("Open OS AI", { 150.0f, 30.0f }))
            {
                open_editor_surface(EditorMainSurface::AISandbox, "settings");
                editor.showSettingsModal = false;
            }
            gui::set_cursor({ contentPos.x + 308.0f, contentY + 332.0f });
            if (gui::button("Close", { 88.0f, 30.0f }))
                editor.showSettingsModal = false;
            gui::end_modal_window();
        }

        if (editor.showPackageManagerModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize{ 820.0f, 560.0f };
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = modalSize.x - 32.0f;
            const auto* activeProfile = editor_find_project_profile(editor.projectId);
            const bool engineArcadeEligible =
                activeProfile
                && activeProfile->kind == EditorProjectKind::Game
                && activeProfile->id != "sandbox";
            const auto projectRoot = resolve_editor_path(std::filesystem::path{ editor.projectRoot });
            const auto engineArcadePackage = projectRoot / "assets" / "packages" / "engine_arcade.package.json";
            const auto engineArcadeScript = projectRoot / "scripts" / "engine_arcade_scene.ascript.cpp";
            const auto forestFactoryPackage = projectRoot / "assets" / "packages" / "engine_forest_factory.package.json";
            const auto forestFactoryProfile = projectRoot / "assets" / "packages" / "engine_forest_factory" / "default.forest.json";
            const auto knownPackages = epoch::package_registry::known_packages();
            const epoch::package_registry::PackageDescriptor* selectedPackage = nullptr;
            if (editor.selectedPackageId.empty() && !knownPackages.empty())
            {
                selectedPackage = &knownPackages.front();
                editor.selectedPackageId = std::string(selectedPackage->id);
            }
            for (const auto& package : knownPackages)
            {
                if (package.id == editor.selectedPackageId)
                    selectedPackage = &package;
            }
            if (!selectedPackage && !knownPackages.empty())
            {
                selectedPackage = &knownPackages.front();
                editor.selectedPackageId = std::string(selectedPackage->id);
            }
            const bool engineArcadeInstalled = path_exists(engineArcadePackage) && path_exists(engineArcadeScript);
            const bool forestFactoryStaged = path_exists(forestFactoryPackage) && path_exists(forestFactoryProfile);

            auto model_download_plan_path = [&](const epoch::package_registry::PackageDescriptor& package)
            {
                const std::string safeId = safe_package_artifact_id(package.id);
                return resolve_editor_path(std::filesystem::path{ epoch::ai::local_model_root() }) / safeId / "download.plan.json";
            };

            auto package_installed = [&](const epoch::package_registry::PackageDescriptor& package)
            {
                if (package.id == epoch::package_registry::kEngineArcadePackageId)
                    return engineArcadeInstalled;
                if (package.id == epoch::package_registry::kEngineForestFactoryPackageId)
                    return forestFactoryStaged;
                if (package.kind == epoch::package_registry::PackageKind::ModelAsset)
                    return path_exists(model_download_plan_path(package));
                return false;
            };

            auto package_status = [&](const epoch::package_registry::PackageDescriptor& package)
            {
                if (package_installed(package))
                    return std::string("staged");
                if (package.requiresExplicitNetworkApproval)
                    return std::string("blocked by approval gate");
                if (package.kind == epoch::package_registry::PackageKind::DownloadableSource
                    || package.kind == epoch::package_registry::PackageKind::ResearchPrototype)
                    return std::string("source gate available");
                if (package.shipsInCore)
                    return std::string("core");
                return std::string("available");
            };

            auto select_package = [&](const epoch::package_registry::PackageDescriptor& package)
            {
                selectedPackage = &package;
                editor.selectedPackageId = std::string(package.id);
                editor.packageInstallStatus = std::string("Selected ") + std::string(package.displayName) + ".";
                editor.packageInstallProgress = package_installed(package) ? 1.0f : 0.0f;
            };

            auto install_package = [&](const epoch::package_registry::PackageDescriptor& package)
            {
                select_package(package);
                if (package.id == epoch::package_registry::kEngineArcadePackageId)
                {
                    if (engineArcadeEligible)
                    {
                        repair_active_project_evidence(editor);
                        activate_engine_arcade_preview(editor);
                        editor.packageInstallStatus = engineArcadeInstalled
                            ? "Engine Arcade already installed; preview activated in 3D Scene."
                            : "Engine Arcade staged and activated in 3D Scene.";
                        editor.packageInstallProgress = 1.0f;
                        editor.showPackageManagerModal = false;
                        push_editor_log(editor, "[package] Requested engine_arcade local runtime-mini package materialization.");
                    }
                    else
                    {
                        const auto created = editor_create_project_shell(EditorProjectKind::Game);
                        if (created.succeeded)
                        {
                            set_project(editor, created.project_id, true);
                            editor.selectedPackageId = std::string(epoch::package_registry::kEngineArcadePackageId);
                            activate_engine_arcade_preview(editor);
                            editor.packageInstallStatus =
                                "Created game project with Engine Arcade staged and visible in 3D Scene.";
                            editor.packageInstallProgress = 1.0f;
                            editor.showPackageManagerModal = false;
                            push_editor_log(
                                editor,
                                "[package] Created game project shell with engine_arcade runtime-mini package staged.");
                        }
                        else
                        {
                            editor.packageInstallStatus =
                                "Engine Arcade needs a game project shell; auto-create failed. Inspect project status.";
                            editor.packageInstallProgress = 0.0f;
                            push_editor_log(
                                editor,
                                "[package] engine_arcade game-shell creation failed: " + created.summary);
                        }
                    }
                    return;
                }

                if (package.id == epoch::package_registry::kEngineForestFactoryPackageId)
                {
                    (void)stage_forest_factory_package_opt_in(editor, package);
                    return;
                }

                if (package.requiresExplicitNetworkApproval)
                {
                    editor.packageInstallStatus = "Blocked: network/server packages require explicit human approval.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, std::string("[package] Blocked ") + std::string(package.id) + ": explicit network approval required.");
                    return;
                }

                if (package.kind == epoch::package_registry::PackageKind::ModelAsset)
                {
                    (void)stage_model_package_opt_in(editor, package);
                    return;
                }

                if (package.kind == epoch::package_registry::PackageKind::DownloadableSource
                    || package.kind == epoch::package_registry::PackageKind::ResearchPrototype)
                {
                    editor.packageInstallStatus = "Download/build gate staged; cache/packages fetch is not automatic.";
                    editor.packageInstallProgress = 0.15f;
                    push_editor_log(editor, std::string("[package] Staged download/build gate for ") + std::string(package.id) + ".");
                    return;
                }

                editor.packageInstallStatus = "Core opt-in package selected; activate it from the matching editor surface.";
                editor.packageInstallProgress = 0.35f;
                push_editor_log(editor, std::string("[package] Selected core opt-in package ") + std::string(package.id) + ".");
            };

            auto remove_package = [&](const epoch::package_registry::PackageDescriptor& package)
            {
                select_package(package);
                std::error_code ec{};
                if (package.id == epoch::package_registry::kEngineArcadePackageId)
                {
                    (void)std::filesystem::remove(engineArcadePackage, ec);
                    ec.clear();
                    (void)std::filesystem::remove(engineArcadeScript, ec);
                    deactivate_engine_arcade_preview(editor);
                    editor.packageInstallStatus = "Engine Arcade project-local manifest/script and preview state removed.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, "[package] Removed engine_arcade project-local package files.");
                    return;
                }
                if (package.id == epoch::package_registry::kEngineForestFactoryPackageId)
                {
                    (void)std::filesystem::remove(forestFactoryPackage, ec);
                    ec.clear();
                    (void)std::filesystem::remove(forestFactoryProfile, ec);
                    editor.packageInstallStatus = "Plant Lab project manifest/profile removed; core editor workspace remains available.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, "[package] Removed forest_factory project-local package files.");
                    return;
                }
                if (package.kind == epoch::package_registry::PackageKind::ModelAsset)
                {
                    (void)std::filesystem::remove(model_download_plan_path(package), ec);
                    editor.packageInstallStatus = "Model download plan removed. Existing weights, if any, stay in cache/models for manual review.";
                    editor.packageInstallProgress = 0.0f;
                    push_editor_log(editor, std::string("[package] Removed model download plan for ") + std::string(package.id) + ".");
                    return;
                }

                editor.packageInstallStatus = "No project-local payload exists for this package yet.";
                editor.packageInstallProgress = 0.0f;
            };

            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Package Manager",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            gui::wrapped_label(
                "Local packages are reviewable engine/project assets. Downloadable source packages use a human-approved source/build gate and must never auto-run services.",
                contentWidth);

            gui::label("Available Packages");
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = "package-manager-package-list",
                .size = { contentWidth, 222.0f },
                .content_height = (std::max)(222.0f, static_cast<float>(knownPackages.size()) * 68.0f + 12.0f),
                .draw_background = true,
                .show_scrollbar = true
            });
            for (const auto& package : knownPackages)
            {
                const bool isSelected = selectedPackage && package.id == selectedPackage->id;
                const bool isInstalled = package_installed(package);
                const bool isBlocked = package.requiresExplicitNetworkApproval;
                const std::string rowLabel = std::string(isInstalled ? "[x] " : (isBlocked ? "[!] " : "[ ] "))
                    + std::string(package.displayName);
                const gui::Vec2 rowPos = gui::cursor_position();
                constexpr float kPackageActionWidth = 122.0f;
                constexpr float kPackageRowHeight = 64.0f;
                const float packageLinkWidth = (std::max)(160.0f, contentWidth - kPackageActionWidth - 24.0f);
                if (gui::text_link(rowLabel, { packageLinkWidth, 28.0f }, isSelected))
                    select_package(package);
                gui::set_cursor({ rowPos.x + contentWidth - kPackageActionWidth - 8.0f, rowPos.y });
                if (gui::button(isInstalled ? "Remove" : (isBlocked ? "Review Gate" : "Install"), { 114.0f, 28.0f }))
                {
                    if (isInstalled)
                        remove_package(package);
                    else
                        install_package(package);
                }
                gui::set_cursor({ rowPos.x + 18.0f, rowPos.y + 31.0f });
                gui::wrapped_label(
                    std::string(epoch::package_registry::package_kind_name(package.kind))
                        + " | " + package_status(package),
                    (std::max)(120.0f, packageLinkWidth - 18.0f));
                gui::set_cursor(rowPos);
                gui::advance_cursor({ 0.0f, kPackageRowHeight });
            }
            gui::end_scroll_area();

            const std::string activePackageLabel = selectedPackage
                ? std::string(selectedPackage->displayName)
                : std::string("(none)");

            gui::label("Selected Package");
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = "package-manager-detail-scroll",
                .size = { contentWidth, 150.0f },
                .content_height = 248.0f,
                .draw_background = true,
                .show_scrollbar = true
            });
            gui::property_row("Project", editor.projectName, 104.0f);
            gui::property_row("Package", activePackageLabel, 104.0f);
            gui::property_row("Type", selectedPackage ? std::string(epoch::package_registry::package_kind_name(selectedPackage->kind)) : std::string("(none)"), 104.0f);
            gui::property_row("Activation", selectedPackage ? std::string(epoch::package_registry::activation_mode_name(selectedPackage->activation)) : std::string("(none)"), 104.0f);
            gui::property_row("Status", selectedPackage ? package_status(*selectedPackage) : std::string("(none)"), 104.0f);
            gui::property_row("Source", selectedPackage && !selectedPackage->externalSourceRepo.empty()
                ? std::string(selectedPackage->externalSourceRepo)
                : std::string("engine builtin"), 104.0f);

            if (selectedPackage && !selectedPackage->summary.empty())
                gui::wrapped_label(std::string(selectedPackage->summary), contentWidth - 20.0f);

            if (selectedPackage && selectedPackage->id == epoch::package_registry::kEngineArcadePackageId)
            {
                gui::property_row("Availability", engineArcadeEligible ? "available for this project" : "install creates a game shell", 104.0f);
                gui::property_row("Manifest", path_exists(engineArcadePackage) ? "installed" : "missing", 104.0f);
                gui::property_row("Script asset", path_exists(engineArcadeScript) ? "installed" : "missing", 104.0f);
                gui::property_row("Default scene", std::string(epoch::package_registry::engine_arcade_default_scene_id()), 104.0f);
                gui::property_row("Runtime scenes", std::string(epoch::package_registry::engine_arcade_scene_ids()), 104.0f);
                gui::property_row("Render asset", std::string(epoch::package_registry::engine_arcade_render_asset_role()), 104.0f);
                gui::property_row("Renderer gate", std::string(epoch::package_registry::engine_arcade_renderer_requirements()), 104.0f);
                gui::property_row("Target", std::string(epoch::package_registry::engine_arcade_render_texture_name()), 104.0f);
                gui::property_row(
                    "RT size",
                    std::to_string(epoch::package_registry::engine_arcade_render_texture_width()) + " x " +
                        std::to_string(epoch::package_registry::engine_arcade_render_texture_height()),
                    104.0f);
            }
            else if (selectedPackage && selectedPackage->id == epoch::package_registry::kEngineForestFactoryPackageId)
            {
                gui::property_row("Workspace", "Plant Lab", 104.0f);
                gui::property_row("Manifest", path_exists(forestFactoryPackage) ? "staged" : "missing", 104.0f);
                gui::property_row("Profile", path_exists(forestFactoryProfile) ? "staged" : "missing", 104.0f);
                gui::wrapped_label(
                    "Plant Lab is the editor-facing Forest Factory workspace. Installing stages the project manifest/profile; generated project assets still require visible scene-use approval.",
                    contentWidth - 20.0f);
            }
            else if (selectedPackage && selectedPackage->requiresExplicitNetworkApproval)
            {
                gui::wrapped_label(
                    "This package can create a server, listener, or network control surface. It remains blocked until a human explicitly approves the run/build gate.",
                    contentWidth - 20.0f);
            }
            else if (selectedPackage && selectedPackage->kind == epoch::package_registry::PackageKind::ModelAsset)
            {
                const std::string safeId = safe_package_artifact_id(selectedPackage->id);
                const std::filesystem::path modelCacheDir =
                    resolve_editor_path(std::filesystem::path{ epoch::ai::local_model_root() }) / safeId;
                gui::property_row("Model cache", epoch::ai::local_model_root(), 104.0f);
                gui::property_row("Package cache", display_project_path(modelCacheDir), 104.0f);
                gui::property_row("Download plan", display_project_path(modelCacheDir / "download.plan.json"), 104.0f);
                gui::wrapped_label(
                    "OS model weights are not cloned with engine iterations. Install stages an on-demand download into cache/models; generated projects include the model only after an explicit package opt-in and license/notice review.",
                    contentWidth - 20.0f);
            }
            gui::end_scroll_area();

            const float packageProgress = (std::max)(
                editor.packageInstallProgress,
                engineArcadeInstalled && editor.selectedPackageId == "engine_arcade"
                    ? 1.0f
                    : (forestFactoryStaged && editor.selectedPackageId == epoch::package_registry::kEngineForestFactoryPackageId ? 0.65f : 0.0f));
            gui::progress_bar(gui::ProgressBarOptions{
                .label = "Install",
                .status = editor.packageInstallStatus,
                .value = packageProgress,
                .size = { contentWidth, 20.0f },
                .show_percent = true
            });

            const gui::Vec2 buttonRow = gui::cursor_position();
            const bool selectedInstalled = selectedPackage && package_installed(*selectedPackage);
            if (gui::button(selectedInstalled ? "Remove Selected Package" : "Install Selected Package", { 220.0f, 30.0f }))
            {
                if (!selectedPackage)
                {
                    editor.packageInstallStatus = "No package selected.";
                    editor.packageInstallProgress = 0.0f;
                }
                else if (selectedInstalled)
                    remove_package(*selectedPackage);
                else
                    install_package(*selectedPackage);
            }
            gui::set_cursor({ buttonRow.x + 236.0f, buttonRow.y });
            if (gui::button("Close", { 120.0f, 30.0f }))
                editor.showPackageManagerModal = false;
            gui::end_modal_window();
        }

        if (editor.showAboutModal)
        {
            editor.openMenu = TopMenu::None;
            const gui::Vec2 modalSize{ 456.0f, 222.0f };
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "About Epoch",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 contentPos = gui::cursor_position();
            const float contentY = contentPos.y;
            gui::set_cursor({ contentPos.x + 8.0f, contentY });
            gui::label("Epoch Editor");
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 22.0f });
            gui::label(std::string("Version: ") + epochnamespace::GetEngineDisplayString());
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 46.0f });
            gui::wrapped_label("Multi-backend engine/editor shell with project-driven scene play, docked scripting, and engine-owned tools.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 86.0f });
            gui::label(std::string("Renderer: ") + renderer_name(ctx));
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 110.0f });
            gui::label(std::string("Project: ") + editor.projectName);
            gui::set_cursor({ contentPos.x + 8.0f, contentPos.y + 136.0f });
            if (gui::button("Close", { 120.0f, 30.0f }))
                editor.showAboutModal = false;
            gui::end_modal_window();
        }

        ctx->set_gui_overlay_priority(editor.openMenu != TopMenu::None || modal_visible_now());

        return result;
    }

} // namespace epochnamespace
