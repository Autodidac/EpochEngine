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

module aeditor;

import aengine.gui;
import aengine.version;
import aspritehandle;
import core.context;
import core.path;
import context.commandqueue;
import context.type;
import aengine.input;
import ascripting.system;
import epoch.ai;
import epoch.systems;
import render.preview_grid;

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

        struct SystemsSurfaceState
        {
            float renderZoom{ 1.0f };
            float taskZoom{ 1.0f };
            int renderPan{ 0 };
            int taskPan{ 0 };
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

        struct AiChat
        {
            static constexpr std::size_t kMaxLines = 200;
            std::vector<std::string> lines{};
            std::string input{};
            std::string pendingPrompt{};
            std::optional<std::future<std::string>> pending{};

            AiChat()
            {
                epoch::ai::init_bot();
                lines.emplace_back("bot> Ready. No AI model selected yet. Open Bottom Dock > AI, scan local models, then choose one.");
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
                    std::string reply = pending->get();
                    if (reply.empty()) reply = "(empty reply)";
                    lines.emplace_back("bot> " + reply);
                    if (!pendingPrompt.empty() && reply != "(empty reply)")
                        epoch::ai::append_training_sample(pendingPrompt, reply, "editor_ai_chat");
                    pendingPrompt.clear();
                    trim_lines();
                }
                catch (const std::exception& e)
                {
                    lines.emplace_back(std::string("bot> (error) ") + e.what());
                    pendingPrompt.clear();
                    trim_lines();
                }

                pending.reset();
            }

            void submit(std::string text)
            {
                if (text.empty() || is_ws_only(text)) return;

                if (pending)
                {
                    lines.emplace_back("bot> (busy)");
                    trim_lines();
                    return;
                }

                lines.emplace_back("you> " + text);
                trim_lines();
                pendingPrompt = text;

                pending.emplace(std::async(std::launch::async, [t = std::move(text)]() mutable {
                    return epoch::ai::send_to_bot(t);
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
                    return EditorWorkspaceTab::Scripts;
                if (value == "Assets" || value == "assets")
                    return EditorWorkspaceTab::Assets;
                if (value == "Output" || value == "output")
                    return EditorWorkspaceTab::Output;
                return EditorWorkspaceTab::Project;
            };

#if defined(_MSC_VER)
            char* rawValue = nullptr;
            std::size_t rawSize = 0;
            if (_dupenv_s(&rawValue, &rawSize, "EPOCH_EDITOR_START_WORKSPACE") != 0 || !rawValue)
                return EditorWorkspaceTab::Project;

            const EditorWorkspaceTab tab = parse_workspace_tab(std::string_view{ rawValue });
            std::free(rawValue);
            return tab;
#else
            const char* const rawValue = std::getenv("EPOCH_EDITOR_START_WORKSPACE");
            if (!rawValue)
                return EditorWorkspaceTab::Project;

            return parse_workspace_tab(std::string_view{ rawValue });
#endif
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
            core::ScenePreviewMode previewMode{ core::ScenePreviewMode::Editor };
            EditorWorkspaceTab workspaceTab{ initial_editor_workspace_tab() };
            float workspaceSplit{ 0.68f };
            bool projectNotesVisible{ false };
            bool showAboutModal{ false };
            bool showUpdateConfirmModal{ false };
            bool showSourceUpdateConfirmModal{ false };
            EditorAutomationCommand automationCommand{ EditorAutomationCommand::None };
            bool automationConsumed{ false };
            SystemsSurfaceState systems{};
            AiWorkspaceDomain aiWorkspaceDomain{ AiWorkspaceDomain::Control };
            bool aiContinuousBuildEnabled{ false };
            bool aiContinuousBuildStageOnNextFrame{ false };
            std::optional<std::future<EditorProjectBuildResult>> aiContinuousBuildPending{};
            std::string aiContinuousBuildFingerprint{};
            std::string aiContinuousBuildStatus{ "Self-iteration watcher is off." };
            std::size_t aiContinuousBuildRunCount{ 0 };
            std::string aiToolHarnessStatus{ "AI tool harness has not run yet." };
            std::size_t aiToolHarnessRunCount{ 0 };
        };

        struct ContextPtrHash
        {
            std::size_t operator()(const core::Context* p) const noexcept
            {
                return std::hash<const void*>{}(p);
            }
        };

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
            constexpr int kSurfaceHeight = 188;
            SurfaceCanvas canvas(kSurfaceWidth, kSurfaceHeight, gui::Color{ 14, 18, 24, 255 });

            for (int x = 0; x < kSurfaceWidth; x += 40)
                canvas.fill_rect(x, 0, 1, kSurfaceHeight, gui::Color{ 24, 30, 39, 255 });

            for (int y = 24; y < kSurfaceHeight; y += 36)
                canvas.hline(0, y, kSurfaceWidth, gui::Color{ 20, 26, 34, 255 });

            struct Stage
            {
                gui::Color fill{};
                gui::Color accent{};
            };

            const std::array<Stage, 8> stages{{
                { { 64, 86, 135, 255 }, { 154, 190, 255, 255 } },
                { { 54, 92, 148, 255 }, { 135, 188, 255, 255 } },
                { { 53, 117, 142, 255 }, { 102, 216, 255, 255 } },
                { { 70, 132, 96, 255 }, { 124, 244, 159, 255 } },
                { { 146, 123, 57, 255 }, { 255, 219, 112, 255 } },
                { { 109, 84, 145, 255 }, { 203, 164, 255, 255 } },
                { { 112, 96, 152, 255 }, { 222, 192, 255, 255 } },
                { expose_ai_inputs ? gui::Color{ 157, 88, 112, 255 } : gui::Color{ 118, 86, 123, 255 },
                  expose_ai_inputs ? gui::Color{ 255, 171, 193, 255 } : gui::Color{ 205, 170, 216, 255 } }
            }};

            const int stageWidth = (std::max)(76, static_cast<int>(96.0f * systems.renderZoom));
            const int stageHeight = 56;
            const int gap = (std::max)(20, static_cast<int>(44.0f * systems.renderZoom));
            const int baseX = 26 - systems.renderPan;
            const int y = 58;

            canvas.fill_rect(18, 18, kSurfaceWidth - 36, 18, gui::Color{ 30, 38, 48, 255 });
            canvas.fill_rect(18, kSurfaceHeight - 26, kSurfaceWidth - 36, 12, gui::Color{ 28, 33, 41, 255 });
            canvas.fill_rect(18, kSurfaceHeight - 26, 120, 12, gui::Color{ 89, 110, 138, 255 });
            canvas.fill_rect(18 + 128, kSurfaceHeight - 26, 140, 12, gui::Color{ 98, 152, 116, 255 });
            canvas.fill_rect(18 + 276, kSurfaceHeight - 26, 180, 12, gui::Color{ 149, 122, 60, 255 });

            for (std::size_t i = 0; i < stages.size(); ++i)
            {
                const int x = baseX + static_cast<int>(i) * (stageWidth + gap);
                const auto& stage = stages[i];

                if (i != 0)
                {
                    const int prevCenter = x - gap + gap / 2;
                    canvas.fill_rect(prevCenter - 1, y + stageHeight / 2 - 3, gap + 2, 6, gui::Color{ 56, 63, 82, 255 });
                }

                canvas.fill_rect(x, y, stageWidth, stageHeight, stage.fill);
                canvas.stroke_rect(x, y, stageWidth, stageHeight, stage.accent);
                canvas.fill_rect(x + 10, y + 10, (std::max)(16, stageWidth / 4), stageHeight - 20, gui::Color{ 255, 255, 255, 32 });
                canvas.fill_rect(x + stageWidth - 14, y + 14, 6, stageHeight - 28, stage.accent);
                canvas.fill_rect(x + 6, y - 12, (std::max)(18, stageWidth / 3), 6, stage.accent);
                canvas.vline(x + stageWidth / 2, y + stageHeight + 8, 18, gui::Color{ 50, 58, 72, 255 }, 2);
            }

            return canvas;
        }

        [[nodiscard]] static SurfaceCanvas build_task_graph_surface(
            const SystemsSurfaceState& systems,
            std::size_t workerCount,
            std::size_t systemCount)
        {
            constexpr int kSurfaceWidth = 1280;
            constexpr int kSurfaceHeight = 188;
            SurfaceCanvas canvas(kSurfaceWidth, kSurfaceHeight, gui::Color{ 16, 16, 20, 255 });

            const int laneCount = (std::clamp)(static_cast<int>(workerCount == 0 ? 4 : workerCount), 2, 6);
            const int laneGap = 8;
            const int laneHeight = (kSurfaceHeight - 34 - laneGap * (laneCount - 1)) / laneCount;
            const int baseX = 26 - systems.taskPan;
            const int taskWidth = (std::max)(34, static_cast<int>(56.0f * systems.taskZoom));
            const int taskGap = (std::max)(10, static_cast<int>(18.0f * systems.taskZoom));

            const std::array<gui::Color, 5> taskColors{{
                { 86, 142, 255, 255 },
                { 90, 193, 142, 255 },
                { 249, 190, 76, 255 },
                { 198, 116, 255, 255 },
                { 255, 118, 150, 255 }
            }};

            for (int lane = 0; lane < laneCount; ++lane)
            {
                const int y = 20 + lane * (laneHeight + laneGap);
                canvas.fill_rect(0, y + laneHeight / 2, kSurfaceWidth, 2, gui::Color{ 38, 42, 52, 255 });
                canvas.fill_rect(4, y, 8, laneHeight, gui::Color{ 72, 76, 92, 255 });

                const int blocks = 5 + static_cast<int>((systemCount + static_cast<std::size_t>(lane)) % 4u);
                for (int block = 0; block < blocks; ++block)
                {
                    const int x = baseX + block * (taskWidth + taskGap) + lane * 18;
                    const gui::Color fill = taskColors[(static_cast<std::size_t>(block) + static_cast<std::size_t>(lane)) % taskColors.size()];
                    canvas.fill_rect(x, y + 3, taskWidth, laneHeight - 6, fill);
                    canvas.stroke_rect(x, y + 3, taskWidth, laneHeight - 6, gui::Color{ 255, 255, 255, 42 });
                    if (block != 0)
                        canvas.fill_rect(x - taskGap + taskGap / 2 - 1, y + laneHeight / 2 - 2, taskGap + 2, 4, gui::Color{ 58, 65, 79, 255 });
                }
            }

            for (int x = 18; x < kSurfaceWidth; x += 96)
                canvas.vline(x, 0, kSurfaceHeight, gui::Color{ 28, 31, 40, 255 });

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

            for (const auto& card : cards)
            {
                const int y = 18;
                const int h = 72;
                canvas.fill_rect(card.x, y, card.w, h, card.fill);
                canvas.stroke_rect(card.x, y, card.w, h, card.active ? card.accent : gui::Color{ 255, 255, 255, 30 });
                canvas.fill_rect(card.x + 12, y + 12, 42, h - 24, gui::Color{ 255, 255, 255, 28 });
                if (card.active)
                    canvas.fill_rect(card.x + card.w - 14, y + 10, 8, h - 20, card.accent);
            }

            if (prefersSoftwareFallback)
                canvas.fill_rect(22, 92, 220, 6, gui::Color{ 174, 124, 89, 255 });
            else
                canvas.fill_rect(22, 92, 220, 6, gui::Color{ 95, 174, 127, 255 });

            return canvas;
        }

        [[nodiscard]] static SurfaceCanvas build_ai_loop_surface(
            const std::array<bool, 5>& readiness,
            bool watcherEnabled,
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
                if (current)
                    canvas.fill_rect(x + 8, y - 10, cardW - 16, 5, accent);
            }

            const int progressW = 820;
            canvas.fill_rect(34, 104, progressW, 8, gui::Color{ 30, 35, 45, 255 });
            const int readyCount = static_cast<int>(std::count(readiness.begin(), readiness.end(), true));
            canvas.fill_rect(34, 104, (progressW * readyCount) / 5, 8, gui::Color{ 94, 201, 142, 255 });

            if (watcherEnabled)
                canvas.fill_rect(866, 20, 68, 16, gui::Color{ 80, 167, 115, 255 });
            else
                canvas.fill_rect(866, 20, 68, 16, gui::Color{ 97, 80, 70, 255 });

            if (buildPending)
                canvas.fill_rect(866, 44, 68, 16, gui::Color{ 223, 174, 77, 255 });
            else
                canvas.fill_rect(866, 44, 68, 16, gui::Color{ 58, 68, 84, 255 });

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
            const auto ensuredShell = editor_ensure_project_shell(profile->id);
            state.projectStatus = ensuredShell.summary.empty()
                ? (std::string("Loaded project shell at ") + state.projectRoot + ".")
                : ensuredShell.summary;
            state.projectBuildStatus = "Build Project creates or refreshes a repo-local child executable for the active shell.";
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

        [[nodiscard]] epochnamespace::previewgrid::Vec3 marker_color_for_entity(
            const EditorEntity& entity,
            bool selected) noexcept
        {
            if (selected)
                return { 1.0f, 0.93f, 0.32f };
            if (entity.type == "Light")
                return { 1.0f, 0.82f, 0.25f };
            if (entity.type == "Spawn")
                return { 0.28f, 0.94f, 0.48f };
            if (entity.type == "Camera")
                return { 0.42f, 0.80f, 1.0f };
            if (entity.category == "World" || entity.type == "Level")
                return { 0.62f, 0.78f, 0.98f };
            if (entity.editorOnly || entity.category == "Editor")
                return { 0.72f, 0.72f, 0.78f };
            return { 0.95f, 0.62f, 0.28f };
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
            if (entity.category == "World" || entity.type == "Level")
                return epochnamespace::previewgrid::ObjectPreviewPrimitive::Level;
            return epochnamespace::previewgrid::ObjectPreviewPrimitive::Cube;
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
                    .color = marker_color_for_entity(entity, selected),
                    .scale{
                        entity.scale[0],
                        entity.scale[1],
                        entity.scale[2]
                    },
                    .radius = marker_radius_for_entity(entity),
                    .primitive = preview_primitive_for_entity(entity),
                    .selected = selected,
                    .editorOnly = entity.editorOnly || entity.category == "Editor"
                });
            }

            epochnamespace::previewgrid::set_object_markers(
                ctx,
                std::span<const epochnamespace::previewgrid::ObjectMarker>{ markers.data(), markers.size() });
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
            const auto projection = epochnamespace::previewgrid::perspective(
                camera.fovRadians,
                aspect,
                camera.nearPlane,
                camera.farPlane);
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
                "Create one sandboxed 3D scene-training exercise for EpochBot. The bot must edit or inspect visible primitives in a sandbox scene, produce build/tool/runtime evidence, and report what changed. It must not answer that it is working fine unless it cites concrete evidence paths. Active project: '{}' ({}), selected object: {}, object count: {}, active script: '{}'.",
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
                : core::RenderPath::Unknown;

            ctx->windowData->commandQueue.enqueue(
                [ctx, label, path]()
                {
                    (void)ctx->add_model_safe(label.c_str(), path.c_str());
                },
                renderPath);
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
            case core::ContextType::Software: return "Software";
            default: return "Unknown";
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

        [[nodiscard]] std::filesystem::path project_entry_source_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "source" / "main.cpp";
        }

        [[nodiscard]] std::filesystem::path project_windows_build_script_path(std::string_view projectRoot)
        {
            return resolve_editor_path(std::filesystem::path{ projectRoot }) / "build_project.ps1";
        }

        [[nodiscard]] std::filesystem::path project_windows_vcxproj_path(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            return root / (root.filename().string() + ".vcxproj");
        }

        [[nodiscard]] std::filesystem::path project_output_exe_path(std::string_view projectRoot)
        {
            const std::filesystem::path root = resolve_editor_path(std::filesystem::path{ projectRoot });
            return root / "bin" / "windows" / "Debug" / "x64" / (root.filename().string() + ".exe");
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
            return text;
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
                editor.aiContinuousBuildStatus = "Watching for project/script changes.";
                return;
            }

            editor.aiContinuousBuildFingerprint = fingerprint;
            editor.aiContinuousBuildStatus = "Queued build: " + std::string(reason);
            push_editor_log(editor, "[ai-build] Queued self-iteration build: " + std::string(reason));

            editor.aiContinuousBuildPending.emplace(std::async(std::launch::async, [root = editor.projectRoot]() {
                return editor_build_project(root);
            }));
        }

        [[nodiscard]] std::string display_project_path(const std::filesystem::path& path)
        {
            const auto resolvedPath = resolve_editor_path(path);
            std::error_code ec;
            return std::filesystem::absolute(resolvedPath, ec).lexically_normal().generic_string();
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

            return entries;
        }

        [[nodiscard]] static std::string make_project_script_stub_text(std::string_view scriptId)
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
                "    host_log(host, \"{}: script stub executed.\");\n"
                "    if (host->rotate_all_entities_yaw)\n"
                "    {{\n"
                "        host->rotate_all_entities_yaw(host->user_data, 3.0f);\n"
                "        host_log(host, \"{}: applied +3 yaw proof step.\");\n"
                "    }}\n"
                "}}\n",
                scriptId,
                scriptId);
        }

        static void create_project_script_stub(EditorState& editor)
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
                editor.scriptBuildStatus = "Could not write project script stub.";
                push_editor_log(editor, "[script] Failed to write script stub: " + display_project_path(scriptPath));
                return;
            }

            out << make_project_script_stub_text(scriptId);
            editor.activeScript = scriptId;
            editor.newScriptName = scriptId;
            editor.selectedProjectFile = display_project_path(scriptPath);
            editor.scriptBuildStatus = "Created project script stub: " + editor.selectedProjectFile;
            push_editor_log(editor, "[script] Created project script stub '" + scriptId + "'.");
            append_project_note(
                editor,
                "Create Project Script Stub",
                std::string("Created ") + scriptId + ".ascript.cpp.",
                "Select Build Selected Script, then Run Selected Script. The script logs proof text and rotates scene entities through the host API.");
        }

        [[nodiscard]] std::string build_ai_project_output_review_prompt(
            const EditorState& state,
            const std::filesystem::path& pathsManifest,
            const std::filesystem::path& buildLog,
            const std::filesystem::path& outputExe)
        {
            return std::format(
                "You are the selected local coding model helping build EpochBot: a from-scratch engine-owned LLM plus backup tiny internal LLM, trained through editor tools, sandbox scenes, build evidence, and eval gates.\n"
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

        void repair_active_project_evidence(EditorState& editor)
        {
            const std::string requestedProject = editor.projectId.empty()
                ? std::string(editor_default_project_profile().id)
                : editor.projectId;
            const auto ensured = editor_ensure_project_shell(requestedProject);
            editor.projectStatus = ensured.summary;
            editor.aiContinuousBuildFingerprint.clear();
            if (ensured.succeeded)
            {
                set_project(editor, ensured.project_id.empty() ? requestedProject : ensured.project_id, true);
                editor.aiContinuousBuildStatus = "Project evidence repaired; queue a self-iteration build to stage verifier evidence.";
                push_editor_log(editor, "[self-iteration] Project evidence repaired: " + ensured.summary);
                append_project_note(
                    editor,
                    "Repair Active Project Evidence",
                    ensured.summary,
                    "Project evidence is ready for the Self-Iteration Sandbox watcher or a manual queue.");
            }
            else
            {
                editor.aiContinuousBuildStatus = "Project evidence repair failed; inspect project status.";
                push_editor_log(editor, "[self-iteration] Project evidence repair failed: " + ensured.summary);
            }
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

        void render_ai_model_picker(EditorState& editor, float width)
        {
            const float contentWidth = (std::max)(180.0f, width);
            const auto manifest = epoch::ai::active_model_manifest();
            auto detectedModels = epoch::ai::detected_model_names();

            gui::property_row("[model] Chat provider", std::string(epoch::ai::provider_mode_name(epoch::ai::current_provider_mode())));
            gui::property_row("[model] Selected model", manifest.display_name.empty() ? std::string("(none selected)") : manifest.display_name);
            gui::property_row("[model] Endpoint", manifest.endpoint);
            gui::property_row("[model] API route", "/v1/models + /v1/chat/completions");
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

            for (const auto& modelId : detectedModels)
            {
                const bool selected = modelId == epoch::ai::active_model_name();
                const std::string buttonLabel = (selected ? std::string("Selected Chat Model: ") : std::string("Use Chat Model: ")) + modelId;
                if (gui::button(buttonLabel, { contentWidth, 30.0f }))
                {
                    if (epoch::ai::select_active_model(modelId))
                        push_editor_log(editor, "[ai] Selected local model: " + modelId);
                    else
                        push_editor_log(editor, "[ai] Could not select model: " + modelId);
                }
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
            gui::property_row("[self-iteration] Build watcher", editor.aiContinuousBuildEnabled ? "enabled" : "paused");
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
                epoch::ai::init_bot();
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
                    push_editor_log(it->second, "[info] Auto command armed: smart update.");
                else if (it->second.automationCommand == EditorAutomationCommand::SourceUpdate)
                    push_editor_log(it->second, "[info] Auto command armed: source update.");
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
            epoch::ai::shutdown_bot();
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
            .project_model_asset = projectModelAsset.empty() ? nullptr : projectModelAsset.c_str()
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
        auto& chat = chat_state_for(ctx);
        chat.pump();

        // Editor GUI layout should follow the live pane client size, not the
        // backend framebuffer size, so parented multicontext panes do not
        // bleed across neighbors under Windows DPI scaling.
        const float w = static_cast<float>((std::max)(1, ctx->width));
        const float h = static_cast<float>((std::max)(1, ctx->height));

        const float toolbar_h = 98.0f;
        const float bottom_h = (std::max)(220.0f, h * 0.24f);
        const float left_w = (std::max)(280.0f, w * 0.2f);
        const float right_w = (std::max)(320.0f, w * 0.22f);

        const gui::Vec2 toolbar_pos{ 0.0f, 0.0f };
        const gui::Vec2 toolbar_size{ w, toolbar_h };

        const gui::Vec2 bottom_pos{ 0.0f, (std::max)(0.0f, h - bottom_h) };
        const gui::Vec2 bottom_size{ w, bottom_h };

        const float main_y = toolbar_h;
        const float main_h = (std::max)(0.0f, h - toolbar_h - bottom_h);

        const gui::Vec2 outliner_pos{ 0.0f, main_y };
        const gui::Vec2 outliner_size{ left_w, main_h };

        const gui::Vec2 details_pos{ (std::max)(0.0f, w - right_w), main_y };
        const gui::Vec2 details_size{ right_w, main_h };

        const gui::Vec2 viewport_pos{ left_w, main_y };
        const gui::Vec2 viewport_size{ (std::max)(0.0f, w - left_w - right_w), main_h };

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

        for (auto& item : topMenus)
        {
            item.x = toolbar_x;
            gui::set_cursor({ toolbar_x, toolbar_button_y });
            if (gui::button(item.label, { item.width, toolbar_button_h }))
                editor.openMenu = editor.openMenu == item.menu ? TopMenu::None : item.menu;
            toolbar_x += item.width + 6.0f;
        }

        const float run_button_x = (std::max)(toolbar_pos.x + 16.0f, w - 104.0f);
        gui::set_cursor({ run_button_x - 24.0f, toolbar_button_y });
        if (gui::button("Run Script", { 112.0f, toolbar_button_h }))
        {
            emit_command(EditorCommand::RunScript, editor.activeScript);
            push_editor_log(editor, std::string("[script] Run requested for '") + editor.activeScript + "'.");
        }

        const float status_x = (std::max)(toolbar_x + 12.0f, run_button_x - 480.0f);
        gui::set_cursor({ status_x, toolbar_button_y + 4.0f });
        gui::wrapped_label(
            std::string("v") + epochnamespace::GetEngineVersionString()
            + "  |  " + epochnamespace::GetEngineBuildTagString()
            + "  |  " + renderer_name(ctx)
            + "  |  " + std::string(preview_mode_name(editor.previewMode))
            + "  |  " + preview_camera_name(ctx)
            + "  |  Zoom " + preview_zoom_text(ctx),
            (std::max)(180.0f, run_button_x - status_x - 12.0f));

        const float tab_y = toolbar_pos.y + 48.0f;
        const float tab_h = 34.0f;
        const float tab_gap = 8.0f;
        float tab_x = 16.0f;

        const std::string editor_tab = "Editor Mode";
        const std::string runtime_tab = "Play Project";
        const std::string scripts_tab = "Run Script";
        const std::string project_tab = "Project Shell";
        const std::string ai_control_tab = "AI Sandbox";

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(editor_tab, { 180.0f, tab_h }))
            push_editor_log(editor, "[editor] Editor mode is active.");
        tab_x += 180.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(runtime_tab, { 156.0f, tab_h }))
        {
            emit_command(EditorCommand::RunGame, editor.activeRuntimeScene);
            push_editor_log(
                editor,
                std::string("[runtime] Launching project play target '")
                + editor.activeRuntimeScene
                + "'.");
        }
        tab_x += 156.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(scripts_tab, { 164.0f, tab_h }))
        {
            emit_command(EditorCommand::RunScript, editor.activeScript);
            push_editor_log(editor, std::string("[script] Run requested for '") + editor.activeScript + "'.");
            editor.workspaceTab = EditorWorkspaceTab::Scripts;
        }
        tab_x += 164.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(project_tab, { 164.0f, tab_h }))
            editor.workspaceTab = EditorWorkspaceTab::Project;
        tab_x += 164.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(ai_control_tab, { 180.0f, tab_h }))
        {
            editor.workspaceTab = EditorWorkspaceTab::AI;
            editor.aiWorkspaceDomain = AiWorkspaceDomain::Control;
            push_editor_log(editor, "[ai] Self-Iteration Sandbox opened.");
        }

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
                on_click();
                editor.openMenu = TopMenu::None;
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
                gui::titled_window_total_height(content_height)
            };
        };

        auto open_dropdown = [&](std::string_view title, TopMenu menu, gui::Vec2 size, auto&& body)
        {
            if (editor.openMenu != menu)
                return;
            const auto pos = dropdown_position_for(menu);
            gui::begin_window(title, pos, size);
            body(gui::cursor_position());
            gui::end_window();
        };

        gui::begin_window("World Outliner", outliner_pos, outliner_size);
        const gui::Vec2 outlinerScrollStart = gui::cursor_position();
        const float outlinerScrollHeight = (std::max)(
            48.0f,
            outliner_pos.y + outliner_size.y - outlinerScrollStart.y - 4.0f);
        (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
            .id = "world-outliner-body",
            .size = { (std::max)(80.0f, outliner_size.x - 12.0f), outlinerScrollHeight },
            .draw_background = false,
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

        gui::begin_window("Inspector", details_pos, details_size);
        const gui::Vec2 inspectorScrollStart = gui::cursor_position();
        const float inspectorScrollHeight = (std::max)(
            48.0f,
            details_pos.y + details_size.y - inspectorScrollStart.y - 4.0f);
        (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
            .id = editor.workspaceTab == EditorWorkspaceTab::AI ? "inspector-ai-body" : "inspector-scene-body",
            .size = { (std::max)(80.0f, details_size.x - 12.0f), inspectorScrollHeight },
            .draw_background = false,
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
            const std::string inspectorLatestReply = last_chat_line_with_prefix(chat, "bot> ");
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

            auto inspectorStartAiBuild = [&](std::string reason, bool force) {
                start_ai_continuous_project_build(editor, inspectorActiveScriptSource, inspectorPathsManifest, reason, force);
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

            gui::label("AI Inspector Controls");
            gui::wrapped_label(
                "AI actions live here in Inspector. Bottom Dock > AI is for status, model inventory, visual feedback, and logs until dedicated editor windows are promoted.",
                inspectorWidth);
            render_ai_model_picker(editor, inspectorWidth);
            gui::property_row("[ai] Active panel", ai_workspace_domain_name(editor.aiWorkspaceDomain));
            gui::property_row("[ai] Stage", inspectorStage);
            gui::property_row("[ai] Evidence", inspectorGate.packetEvidenceSummary);
            gui::property_row("[ai] Watcher", editor.aiContinuousBuildEnabled ? "enabled" : "paused");
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
                if (gui::button("Repair Active Project Evidence", { inspectorWidth, 30.0f }))
                    repair_active_project_evidence(editor);
            }

            if (inspectorSandboxControls)
            {
                if (gui::button(editor.aiContinuousBuildEnabled ? "Pause Self-Iteration Watcher" : "Enable Self-Iteration Watcher", { inspectorWidth, 30.0f }))
                {
                    editor.aiContinuousBuildEnabled = !editor.aiContinuousBuildEnabled;
                    editor.aiContinuousBuildStatus = editor.aiContinuousBuildEnabled
                        ? "Self-iteration watcher enabled; watching project/script evidence."
                        : "Self-iteration watcher paused.";
                    if (editor.aiContinuousBuildEnabled)
                        editor.aiContinuousBuildFingerprint.clear();
                    push_editor_log(editor, editor.aiContinuousBuildEnabled
                        ? "[ai-build] Self-iteration watcher enabled."
                        : "[ai-build] Self-iteration watcher paused.");
                    append_project_note(
                        editor,
                        editor.aiContinuousBuildEnabled ? "Self-Iteration Watcher Enabled" : "Self-Iteration Watcher Paused",
                        editor.aiContinuousBuildStatus,
                        "The watcher only stages evidence packets; it does not write repo changes blindly.");
                }

                if (gui::button("Queue Sandbox Build Pass", { inspectorWidth, 30.0f }))
                {
                    inspectorStartAiBuild("manual self-iteration build request", true);
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
                        "EpochBot must use visible sandbox scene/tool/build evidence. Generic self-reporting like 'working fine' is invalid without paths, changed object state, and verifier output.";
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
                            "Use this for watchable EpochBot scene-edit/test learning; reject answers without evidence paths or visible state changes.");
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
            gui::label("Viewport Input: click/drag objects  |  empty LMB pan  |  RMB orbit  |  Wheel zoom");
        }
        gui::end_scroll_area();
        gui::end_window();

        result.scene_viewport = gui::scene_viewport("Perspective", viewport_pos, viewport_size);
        ctx->set_scene_preview_mode(editor.previewMode);
        ctx->set_scene_viewport(core::RenderViewport{
            static_cast<int>((std::max)(0.0f, result.scene_viewport.position.x)),
            static_cast<int>((std::max)(0.0f, result.scene_viewport.position.y)),
            static_cast<int>((std::max)(0.0f, result.scene_viewport.size.x)),
            static_cast<int>((std::max)(0.0f, result.scene_viewport.size.y))
        });
        update_scene_object_interaction(ctx, editor, result);
        publish_editor_preview_markers(ctx.get(), editor);

        editor.workspaceSplit = std::clamp(editor.workspaceSplit, 0.35f, 0.80f);
        const float raw_left_bottom_w = w * editor.workspaceSplit;
        const float min_workspace_w = (std::min)(360.0f, (std::max)(0.0f, w * 0.55f));
        const float min_chat_w = (std::min)(300.0f, (std::max)(0.0f, w * 0.35f));
        const float max_workspace_w = (std::max)(min_workspace_w, w - min_chat_w);
        const float left_bottom_w = std::clamp(raw_left_bottom_w, min_workspace_w, max_workspace_w);
        const float active_workspace_split = w > 0.0f ? left_bottom_w / w : editor.workspaceSplit;
        const gui::Vec2 log_pos{ 0.0f, bottom_pos.y };
        const gui::Vec2 log_size{ left_bottom_w, bottom_h };
        const gui::Vec2 chat_pos{ left_bottom_w, bottom_pos.y };
        const gui::Vec2 chat_size{ (std::max)(0.0f, w - left_bottom_w), bottom_h };

        gui::begin_window("Console Dock", log_pos, log_size);
        const std::array<gui::SegmentedButtonSpec, 6> workspaceTabs{{
            { "Output", 78.0f, editor.workspaceTab == EditorWorkspaceTab::Output },
            { "Project", 78.0f, editor.workspaceTab == EditorWorkspaceTab::Project },
            { "Scripts", 78.0f, editor.workspaceTab == EditorWorkspaceTab::Scripts },
            { "Assets", 76.0f, editor.workspaceTab == EditorWorkspaceTab::Assets },
            { "AI", 58.0f, editor.workspaceTab == EditorWorkspaceTab::AI },
            { "Systems", 84.0f, editor.workspaceTab == EditorWorkspaceTab::Systems }
        }};
        if (const auto selected = gui::tab_bar(workspaceTabs))
            editor.workspaceTab = static_cast<EditorWorkspaceTab>(*selected);
        const std::array<gui::InlineButtonSpec, 3> workspaceColumnButtons{{
            { "Console +", 96.0f },
            { "Chat +", 76.0f },
            { "Reset Columns", 122.0f }
        }};
        if (const auto splitAction = gui::inline_button_row(workspaceColumnButtons))
        {
            if (*splitAction == 0)
                editor.workspaceSplit = (std::min)(0.80f, editor.workspaceSplit + 0.05f);
            else if (*splitAction == 1)
                editor.workspaceSplit = (std::max)(0.35f, editor.workspaceSplit - 0.05f);
            else
                editor.workspaceSplit = 0.68f;
        }
        gui::property_row(
            "[ui] Columns",
            std::format("dock {:.0f}% | chat {:.0f}%", active_workspace_split * 100.0f, (1.0f - active_workspace_split) * 100.0f),
            96.0f);

        const bool dockUsesOuterScroll = editor.workspaceTab != EditorWorkspaceTab::Output;
        if (dockUsesOuterScroll)
        {
            const gui::Vec2 dockScrollStart = gui::cursor_position();
            const float dockScrollHeight = (std::max)(
                60.0f,
                bottom_pos.y + bottom_h - dockScrollStart.y - 4.0f);
            (void)gui::begin_scroll_area(gui::ScrollAreaOptions{
                .id = std::string("console-dock-body-") + std::to_string(static_cast<int>(editor.workspaceTab)),
                .size = { (std::max)(120.0f, log_size.x - 12.0f), dockScrollHeight },
                .draw_background = false,
                .show_scrollbar = true
            });
        }

        switch (editor.workspaceTab)
        {
        case EditorWorkspaceTab::Project:
        {
            const auto* activeProfile = editor_find_project_profile(editor.projectId);
            if (!activeProfile)
                activeProfile = &editor_default_project_profile();
            const auto seedEntities = editor_seed_entities_for_project(editor.projectId);
            const auto seedSummary = summarize_seed_objects(seedEntities);

            const std::filesystem::path entrySource = project_entry_source_path(editor.projectRoot);
            const std::filesystem::path buildScript = project_windows_build_script_path(editor.projectRoot);
            const std::filesystem::path projectFile = project_windows_vcxproj_path(editor.projectRoot);
            const std::filesystem::path outputExe = project_output_exe_path(editor.projectRoot);
            const std::filesystem::path buildLog = project_build_log_path(editor.projectRoot);
            const std::filesystem::path notesPath = project_notes_path(editor.projectRoot);
            const std::filesystem::path pathsManifest = resolve_editor_path(std::filesystem::path{ editor.projectRoot }) / "project.paths.txt";
            const auto modelSummary = editor_project_model_summary(editor.projectId);

            gui::property_row("[project] Active", activeProfile->display_name);
            gui::property_row("[project] Kind", editor.projectKind);
            gui::property_row("[project] Root", editor.projectRoot);
            gui::property_row("[project] Scene", editor.projectScenePath);
            gui::property_row("[project] World", activeProfile->world_name);
            gui::property_row("[project] Runtime", activeProfile->runtime_scene_id);
            gui::property_row("[project] Manifest", editor.projectManifest);
            gui::property_row("[project] Template", editor.projectTemplate);
            gui::property_row("[project] Default script", activeProfile->default_script);
            gui::property_row("[project] Script source", editor_resolve_script_source_path(activeProfile->default_script, editor.projectRoot));
            gui::property_row("[project] Demo model", modelSummary.asset_path.empty() ? std::string("(none)") : modelSummary.asset_path);
            gui::property_row("[project] Demo model path", modelSummary.resolved_path.empty() ? std::string("(unresolved)") : modelSummary.resolved_path);
            gui::property_row("[project] Demo model exists", modelSummary.exists ? "true" : "false");
            gui::property_row("[project] Demo model parsed", modelSummary.parsed ? "true" : "false");
            gui::property_row("[project] Demo model summary", modelSummary.summary);
            gui::property_row("[project] Integration", activeProfile->engine_integration_mode);
            gui::property_row("[project] Include root", activeProfile->public_include_root);
            gui::property_row("[project] Entry source", display_project_path(entrySource));
            gui::property_row("[project] Build script", display_project_path(buildScript));
            gui::property_row("[project] Windows project", display_project_path(projectFile));
            gui::property_row("[project] Paths manifest", display_project_path(pathsManifest));
            gui::property_row("[project] Debug output", display_project_path(outputExe));
            gui::property_row("[project] Build log", display_project_path(buildLog));
            gui::property_row("[project] Notes", display_project_path(notesPath));
            gui::property_row("[project] Play target", path_exists(outputExe) ? "ready" : "build required");
            gui::property_row("[project] Build state", path_exists(buildScript) ? "script ready" : "missing build script");
            gui::property_row("[project] Manifest exists", path_exists(editor.projectManifest) ? "true" : "false");
            gui::property_row("[project] Entry exists", path_exists(entrySource) ? "true" : "false");
            gui::property_row("[project] Build script exists", path_exists(buildScript) ? "true" : "false");
            gui::property_row("[project] Paths manifest exists", path_exists(pathsManifest) ? "true" : "false");
            gui::property_row("[project] Output exists", path_exists(outputExe) ? "true" : "false");
            gui::property_row("[project] Build log exists", path_exists(buildLog) ? "true" : "false");
            gui::property_row("[project] Scene file status", file_ready_summary(editor.projectScenePath));
            gui::property_row("[project] Scene file role", "metadata shell; live preview currently uses editor seed entities");
            gui::property_row("[project] Seed objects", std::to_string(seedSummary.total));
            gui::property_row("[project] Seed mix", summarize_seed_category_mix(seedSummary));
            gui::property_row("[project] Archetype types", summarize_seed_type_list(seedSummary.types));
            gui::property_row("[project] Archetype categories", summarize_seed_type_list(seedSummary.categories));
            gui::property_row(
                "[project] Primitive/object path",
                "scene-owned seed entities are the current engine authoring baseline");
            gui::property_row(
                "[project] Visible/editor-only",
                std::to_string(seedSummary.visibleCount) + " visible | " + std::to_string(seedSummary.editorOnlyCount) + " editor-only");
            gui::wrapped_label(activeProfile->description, (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                "Epoch's current primitive/object path starts with engine-owned scene seed entities and archetype types here, then grows into fuller authoring/runtime object systems from that truthful baseline instead of from hidden sample content.",
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(editor.projectStatus, (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(editor.projectBuildStatus, (std::max)(180.0f, log_size.x - 24.0f));
            if (gui::button(editor.projectNotesVisible ? "Hide Project Notes" : "Show Project Notes", { 220.0f, 30.0f }))
                editor.projectNotesVisible = !editor.projectNotesVisible;
            if (editor.projectNotesVisible)
                gui::wrapped_label(read_project_notes(editor.projectRoot), (std::max)(180.0f, log_size.x - 24.0f));

            for (const auto& profile : editor_project_profiles())
            {
                const std::string buttonLabel = std::string(profile.display_name);
                if (gui::button(buttonLabel, { (std::max)(180.0f, log_size.x - 24.0f), 28.0f }))
                {
                    set_project(editor, profile.id, true);
                    epochnamespace::previewgrid::reset_camera(ctx.get());
                }
            }

            if (gui::button("Play Current Project", { 180.0f, 30.0f }))
            {
                const bool hasBuiltOutput = std::filesystem::exists(outputExe);
                const std::string playTarget = hasBuiltOutput
                    ? std::string("project-exe:") + display_project_path(outputExe)
                    : editor.activeRuntimeScene;
                emit_command(EditorCommand::RunGame, playTarget);
                push_editor_log(editor, std::string("[project] Play requested for ") + editor.projectName + ".");
                push_editor_log(
                    editor,
                    hasBuiltOutput
                        ? std::string("[project] Launching built child executable: ") + display_project_path(outputExe)
                        : std::string("[project] No child executable yet; falling back to in-editor runtime target '") + editor.activeRuntimeScene + "'.");
                append_project_note(
                    editor,
                    "Play Current Project",
                    hasBuiltOutput ? std::string("Launching built child executable.") : std::string("Falling back to in-editor runtime target."),
                    playTarget);
            }
            if (gui::button("Build Active Project", { 220.0f, 30.0f }))
            {
                const auto build = editor_build_project(editor.projectRoot);
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
                    "Build Active Project",
                    build.summary,
                    build.succeeded ? "Build passed; Play Current Project can use the child executable when output exists." : "Build failed; inspect the build log before retrying or promoting AI evidence.");
            }
            if (gui::button("Create Game Project Shell", { 220.0f, 30.0f }))
            {
                const auto created = editor_create_project_shell(EditorProjectKind::Game);
                editor.projectStatus = created.summary;
                push_editor_log(editor, std::string("[project] ") + created.summary);
                if (created.succeeded)
                {
                    const auto createdPathsManifest = std::filesystem::path{ created.root_path } / "project.paths.txt";
                    const auto createdProjectFile = project_windows_vcxproj_path(created.root_path);
                    push_editor_log(editor, std::string("[project] Root: ") + created.root_path);
                    push_editor_log(editor, std::string("[project] Manifest: ") + created.manifest_path);
                    push_editor_log(editor, std::string("[project] Entry source: ") + created.entry_source_path);
                    push_editor_log(editor, std::string("[project] Build script: ") + created.build_script_path);
                    push_editor_log(editor, std::string("[project] Paths manifest: ") + display_project_path(createdPathsManifest));
                    push_editor_log(editor, std::string("[project] Windows project: ") + display_project_path(createdProjectFile));
                    push_editor_log(editor, std::string("[project] Default script: ") + created.default_script_path);
                    push_editor_log(editor, std::string("[project] Embedded-engine include root: ") + created.public_include_root + " (" + created.engine_integration_mode + ").");
                    set_project(editor, created.project_id, true);
                    editor.projectStatus = created.summary + " Active project loaded.";
                    epochnamespace::previewgrid::reset_camera(ctx.get());
                    append_project_note(
                        editor,
                        "Create Game Project Shell",
                        created.summary,
                        "Use Build Active Project, then Play Current Project. This is a ProjectLauncher game/software shell, not the self-iteration sandbox.");
                }
            }
            if (gui::button("Create Tool Project Shell", { 220.0f, 30.0f }))
            {
                const auto created = editor_create_project_shell(EditorProjectKind::Tool);
                editor.projectStatus = created.summary;
                push_editor_log(editor, std::string("[project] ") + created.summary);
                if (created.succeeded)
                {
                    const auto createdPathsManifest = std::filesystem::path{ created.root_path } / "project.paths.txt";
                    const auto createdProjectFile = project_windows_vcxproj_path(created.root_path);
                    push_editor_log(editor, std::string("[project] Root: ") + created.root_path);
                    push_editor_log(editor, std::string("[project] Manifest: ") + created.manifest_path);
                    push_editor_log(editor, std::string("[project] Entry source: ") + created.entry_source_path);
                    push_editor_log(editor, std::string("[project] Build script: ") + created.build_script_path);
                    push_editor_log(editor, std::string("[project] Paths manifest: ") + display_project_path(createdPathsManifest));
                    push_editor_log(editor, std::string("[project] Windows project: ") + display_project_path(createdProjectFile));
                    push_editor_log(editor, std::string("[project] Default script: ") + created.default_script_path);
                    push_editor_log(editor, std::string("[project] Embedded-engine include root: ") + created.public_include_root + " (" + created.engine_integration_mode + ").");
                    set_project(editor, created.project_id, true);
                    editor.projectStatus = created.summary + " Active project loaded.";
                    epochnamespace::previewgrid::reset_camera(ctx.get());
                    append_project_note(
                        editor,
                        "Create Tool Project Shell",
                        created.summary,
                        "Use Scripts or Tool Harness against this project shell. Self-iteration remains controlled from the AI Sandbox.");
                }
            }
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
                "Scripts compile with the engine/project and use the editor host API for callbacks. Create project-local stubs here, build them, run them, and watch project notes/output for visible evidence.",
                scriptsContentWidth);
            gui::wrapped_label(editor.scriptBuildStatus, scriptsContentWidth);

            gui::property_row("[script] New script", "type a safe id, then create a project-local .ascript.cpp");
            (void)gui::edit_box(editor.newScriptName, { scriptsContentWidth, 28.0f }, 64, false);
            if (gui::button("Create Project Script Stub", { (std::min)(260.0f, scriptsContentWidth), 30.0f }))
                create_project_script_stub(editor);

            for (const auto& script : editor_script_profiles())
            {
                const std::string buttonLabel = std::string(script.display_name);
                const std::string resolvedSource = editor_resolve_script_source_path(script.id, editor.projectRoot);
                if (gui::button(buttonLabel, { (std::max)(180.0f, log_size.x - 24.0f), 28.0f }))
                {
                    editor.activeScript = std::string(script.id);
                    editor.scriptBuildStatus = std::string("Selected script source: ") + resolvedSource;
                    push_editor_log(editor, std::string("[script] Selected ") + editor.activeScript + ".");
                }
                gui::property_row("  source", resolvedSource);
                gui::wrapped_label(script.description, (std::max)(160.0f, log_size.x - 36.0f));
            }

            const auto scriptEntries = collect_script_browser_entries(editor.projectRoot);
            gui::property_row("[script] Project/engine script files", std::to_string(scriptEntries.size()));
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

            if (gui::button("Build Selected Script", { 180.0f, 30.0f }))
            {
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

            if (gui::button("Run Selected Script", { 180.0f, 30.0f }))
            {
                emit_command(EditorCommand::RunScript, editor.activeScript);
                push_editor_log(editor, std::string("[script] Run requested for '") + editor.activeScript + "'.");
                append_project_note(
                    editor,
                    "Run Selected Script",
                    std::string("Run requested for ") + editor.activeScript + ".",
                    "Watch the Output workspace for script-host results and editor-visible changes.");
            }

            const auto projectEntries = collect_project_browser_entries(editor.projectRoot);
            gui::property_row("[files] Active project browser", std::to_string(projectEntries.size()) + " visible entries");
            gui::wrapped_label("This is the first shallow project file/folder viewer. It skips build/bin/.vs output, selects scripts for build/run, and gives the AI sandbox visible path evidence instead of hidden filesystem magic.", scriptsContentWidth);
            for (const auto& entry : projectEntries)
            {
                if (gui::button(entry.label, { scriptsContentWidth, 26.0f }))
                {
                    editor.selectedProjectFile = entry.path;
                    if (entry.kind == "SCRIPT")
                    {
                        editor.activeScript = script_id_from_source_path(std::filesystem::path{ entry.path });
                        editor.scriptBuildStatus = "Selected project script: " + entry.path;
                    }
                    push_editor_log(editor, "[files] Selected " + entry.path);
                }
            }
            gui::property_row("[files] Selected", editor.selectedProjectFile.empty() ? std::string("(none)") : editor.selectedProjectFile);
            break;
        }
        case EditorWorkspaceTab::Assets:
        {
            const float assetsContentWidth = (std::max)(180.0f, log_size.x - 24.0f);
            const auto modelSummary = editor_project_model_summary(editor.projectId);
            gui::property_row("[assets] Project", editor.projectName);
            gui::property_row("[assets] Project root", editor.projectRoot);
            gui::property_row("[assets] Scene", editor.projectScenePath);
            gui::property_row("[assets] Demo model", modelSummary.asset_path.empty() ? std::string("(none)") : modelSummary.asset_path);
            gui::property_row("[assets] Model path", modelSummary.resolved_path.empty() ? std::string("(unresolved)") : modelSummary.resolved_path);
            gui::property_row("[assets] Model parsed", modelSummary.parsed ? "true" : "false");
            gui::wrapped_label(
                "Asset cards are first-pass file-type thumbnails: scene/model/image/audio/text assets are visible and selectable now. Decoded image/model preview thumbnails remain a next-pass GUI renderer feature.",
                assetsContentWidth);

            const auto assetEntries = collect_asset_browser_entries(editor);
            gui::property_row("[assets] Active asset cards", std::to_string(assetEntries.size()));
            for (const auto& entry : assetEntries)
            {
                const std::string buttonLabel = entry.label + (entry.directory ? "" : std::format("  [{} bytes]", entry.size));
                if (gui::button(buttonLabel, { assetsContentWidth, 28.0f }))
                {
                    editor.selectedAssetPath = entry.path;
                    push_editor_log(editor, "[assets] Selected " + entry.path);
                }
            }

            if (assetEntries.empty())
                gui::wrapped_label("No active assets found yet. Add files under the project assets folder or use the project demo model path once it resolves.", assetsContentWidth);
            gui::property_row("[assets] Selected", editor.selectedAssetPath.empty() ? std::string("(none)") : editor.selectedAssetPath);
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
            const std::string latestReply = last_chat_line_with_prefix(chat, "bot> ");
            const auto gateStatus = summarize_ai_review_gate(
                editor,
                buildLog,
                outputExe,
                pathsManifest,
                training,
                latestPrompt,
                latestReply);
            const std::string loopStage = ai_control_loop_stage(gateStatus);
            const float aiContentWidth = (std::max)(180.0f, log_size.x - 24.0f);

            const std::array<gui::SegmentedButtonSpec, 7> aiDomains{{
                { "Sandbox", 104.0f, editor.aiWorkspaceDomain == AiWorkspaceDomain::Control },
                { "Harness", 100.0f, editor.aiWorkspaceDomain == AiWorkspaceDomain::Tooling },
                { "Assistant", 112.0f, editor.aiWorkspaceDomain == AiWorkspaceDomain::Engine },
                { "Launcher", 108.0f, editor.aiWorkspaceDomain == AiWorkspaceDomain::Software },
                { "Training", 104.0f, editor.aiWorkspaceDomain == AiWorkspaceDomain::Training },
                { "Viz", 64.0f, editor.aiWorkspaceDomain == AiWorkspaceDomain::Visualizer },
                { "Ops", 72.0f, editor.aiWorkspaceDomain == AiWorkspaceDomain::Ops }
            }};
            if (const auto selectedAiDomain = gui::tab_bar(aiDomains))
                editor.aiWorkspaceDomain = static_cast<AiWorkspaceDomain>(*selectedAiDomain);

            gui::property_row("[ai] Domain", ai_workspace_domain_name(editor.aiWorkspaceDomain));
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
                editor.aiContinuousBuildEnabled,
                editor.aiContinuousBuildPending.has_value());
            editor.systems.aiLoopSurface = gui::register_runtime_surface(
                "ai-loop-visualizer",
                std::span<const std::uint8_t>(aiLoopCanvas.pixels.data(), aiLoopCanvas.pixels.size()),
                static_cast<std::uint32_t>(aiLoopCanvas.width),
                static_cast<std::uint32_t>(aiLoopCanvas.height));

            if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Control)
            {
                gui::label("AI Loop Visualizer");
                if (editor.systems.aiLoopSurface.is_valid())
                    gui::image(editor.systems.aiLoopSurface, { aiContentWidth, 92.0f });
                render_ai_control_status_panel(editor, gateStatus, loopStage, aiContentWidth);
                if (!gateStatus.projectEvidenceReady)
                {
                    gui::wrapped_label(
                        "Project evidence is incomplete. Repair Active Project Evidence rebuilds the manifest/source/build-script shell from the active profile before you queue the next self-iteration build.",
                        aiContentWidth);
                }
                if (gui::button("Repair Active Project Evidence", { 260.0f, 30.0f }))
                    repair_active_project_evidence(editor);
            }

            gui::label("Local Model Connection");
            render_ai_model_picker(editor, (std::min)(aiContentWidth, 420.0f));
            gui::property_row("[model] Chat request path", "OpenAI-compatible /v1/chat/completions");
            gui::property_row("[model] Manifest", manifest.manifest_path);
            gui::property_row("[ai] Self-iteration contract", "Engine/ai/control/continuous_build_loop.json");
            gui::property_row("[ai] Iteration packets", epoch::ai::iteration_packet_root());
            gui::property_row("[ai] Research staging", epoch::ai::research_staging_root());
            gui::property_row("[ai] Curated datasets", training.curated_dataset_root);
            gui::property_row("[ai] Eval suites", training.eval_root);
            gui::property_row("[ai] Raw capture", training.local_capture_jsonl);
            gui::property_row("[ai] Tool evidence capture", training.mcp_capture_jsonl);
            gui::property_row("[ai] Local models", training.model_root);
            gui::property_row("[ai] Checkpoints", training.checkpoint_root);
            gui::property_row("[ai] Active script source", activeScriptSource);
            gui::property_row("[ai] Paths manifest", display_project_path(pathsManifest));
            gui::property_row("[ai] Active build log", display_project_path(buildLog));
            gui::property_row("[ai] Active output", display_project_path(outputExe));
            gui::property_row("[ai] Runtime role", "EpochBot");
            gui::property_row("[ai] Sandbox role", "local tool/control capture");
            gui::property_row("[ai] Self-iteration loop", ai_control_loop_contract());
            gui::property_row("[ai] Loop stage", loopStage);
            gui::property_row("[ai] Seed/helper/verifier", "runtime seed + helper teacher + gated verifier");
            gui::property_row("[ai] Promotion gate", "capture -> review/score -> curate/promote");
            gui::property_row("[ai] Replay boundary", "staged packets only; no blind write-through");
            gui::property_row("[ai] Evidence", "build + runtime + retained logs");
            gui::property_row("[ai] Build evidence", ready_text(gateStatus.buildEvidenceReady));
            gui::property_row("[ai] Project evidence", ready_text(gateStatus.projectEvidenceReady));
            gui::property_row("[ai] Capture evidence", ready_text(gateStatus.captureEvidenceReady));
            gui::property_row("[ai] Chat pair", ready_text(gateStatus.chatPairReady));
            gui::property_row("[ai] Packet evidence", gateStatus.packetEvidenceSummary);
            gui::property_row("[ai] Review gate state", gateStatus.promotionSummary);
            const std::string captureGuidance =
                "Raw chat captures land in " + training.local_capture_jsonl
                + " as Git-safe staging data, tool/harness interaction snapshots land in "
                + training.mcp_capture_jsonl
                + ", curated JSON/JSONL stays in Engine/ai/, and outdated local checkpoints/models/caches should be deleted during training pivots when they no longer match the active data or control model.";
            const std::string iterationGuidance =
                "Iteration packets now stage the current project, scene, script, capture roots, model manifest, and concrete evidence paths into "
                + epoch::ai::iteration_packet_root()
                + " so the self-iteration loop has something explicit to build, verify, score, and either promote or discard.";
            const std::string continuousBuildGuidance =
                "The self-iteration watcher monitors the active project entry/script/manifest evidence, queues one child-project build at a time, and stages a fresh packet after a successful build so the AI loop can verify from current artifacts.";
            gui::wrapped_label(
                "Epoch separates three jobs: the normal editor builds games/software, ProjectLauncher launches those artifacts, and the Self-Iteration Sandbox is the controlled assistant/coding loop for improving Epoch itself.",
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                captureGuidance.c_str(),
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                "AI-assisted engine changes stay staged and reviewable here: capture first, score or inspect the result, then promote curated datasets/evals intentionally instead of allowing blind write-through automation.",
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                "Phase 5 starts from explicit staged packets: the local self-iteration loop may plan and replay work from evidence, but repo changes still pass through builder/verifier/gate before promotion.",
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                iterationGuidance.c_str(),
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                continuousBuildGuidance.c_str(),
                (std::max)(180.0f, log_size.x - 24.0f));
            if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Control)
            {
                gui::wrapped_label(
                    "Sandbox domain: keep the self-iteration watcher on while you edit scripts/projects, or queue one build manually. Successful builds stage packets for verifier/gate review without granting blind repo write-through.",
                    aiContentWidth);
            }
            else if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Tooling)
            {
                gui::wrapped_label(
                    "Harness domain: select a script in Scripts, then run the AI tool harness here. The harness builds/runs it through EpochScriptHost, captures before/after editor state, records tool evidence, and stages a packet.",
                    aiContentWidth);
            }
            else if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Engine)
            {
                gui::wrapped_label(
                    "Assistant domain: use this as the regular game-engine assistant surface for scene/project guidance, active model inspection, selected-model chat, and safe iteration packet staging.",
                    aiContentWidth);
            }
            else if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Software)
            {
                gui::wrapped_label(
                    "Launcher domain: tracks generated project shells, build logs, child executables, and script/source evidence so the AI can reason about real software artifacts instead of just editor chat.",
                    aiContentWidth);
            }
            else if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Training)
            {
                gui::wrapped_label(
                    "Training domain: raw captures stay local until review; curated datasets and evals live under Engine/ai. Promote only records tied to build/runtime/tool evidence.",
                    aiContentWidth);
            }
            else if (editor.aiWorkspaceDomain == AiWorkspaceDomain::Visualizer)
            {
                gui::wrapped_label(
                    "Visualizer domain: first-pass Phase 5 view. The cards represent planner/project evidence, builder/build evidence, verifier/capture evidence, gate/chat evidence, and final human review. Future work turns this into a separate AI window with live packet replay, scene-state diffs, and eventually a 3D weight/model view.",
                    aiContentWidth);
                if (editor.systems.aiLoopSurface.is_valid())
                    gui::image(editor.systems.aiLoopSurface, { aiContentWidth, 116.0f });
                gui::property_row("[viz] Planner", gateStatus.projectEvidenceReady ? "project evidence ready" : "blocked: project/manifest evidence missing");
                gui::property_row("[viz] Builder", gateStatus.buildEvidenceReady ? "build/output evidence ready" : "blocked: build log/output evidence missing");
                gui::property_row("[viz] Verifier", gateStatus.captureEvidenceReady ? "capture evidence ready" : "blocked: raw or tool evidence missing");
                gui::property_row("[viz] Gate", gateStatus.chatPairReady ? "chat/eval candidate ready" : "blocked: assistant exchange not ready");
                gui::property_row("[viz] Future window", "AI visualizer needs dedicated editor-window docking and 3D weight view");
            }
            else
            {
                gui::wrapped_label(
                    "How to run: build Epoch, launch from x64/Debug, open Bottom Dock > AI, pick a domain, then use Inspector for the actual AI controls.",
                    aiContentWidth);
            }

            const auto currentMcpRecord = [&]() {
                return epoch::ai::McpCaptureRecord{
                    .server = "editor",
                    .tool = "self-iteration-guidance",
                    .prompt = build_ai_self_iteration_prompt(editor),
                    .normalized_output = epoch::ai::active_provider_summary(),
                    .source_path = editor.projectScenePath.empty() ? editor.projectRoot : editor.projectScenePath
                };
            };
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

            auto startAiContinuousBuild = [&](std::string reason, bool force) {
                start_ai_continuous_project_build(editor, activeScriptSource, pathsManifest, reason, force);
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
                    editor.aiContinuousBuildStatus = "Build passed and staged packet for verifier/gate review.";
                    push_editor_log(editor, "[ai-build] Staged post-build AI packet.");
                    push_editor_log(editor, "[ai-build] Packet: " + packetDir);
                    append_project_note(
                        editor,
                        "Post-Build Iteration Packet Staged",
                        std::string("Packet staged at ") + packetDir,
                        "Review the staged packet before promoting data or applying any repo-changing pass.");
                }
            }

            if (editor.aiContinuousBuildEnabled && !editor.aiContinuousBuildPending)
                startAiContinuousBuild("detected project/script evidence change", false);

            const bool showSandboxControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Control;
            const bool showToolingControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Tooling;
            const bool showAssistantControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Engine;
            const bool showLauncherControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Software;
            const bool showTrainingControls = editor.aiWorkspaceDomain == AiWorkspaceDomain::Training;
            constexpr bool showWorkspaceActionButtons = false;

            if (showSandboxControls)
            {
                gui::label("Sandbox Build Controls");
                gui::property_row("[ai-build] Watcher", editor.aiContinuousBuildEnabled ? "enabled" : "paused");
                gui::property_row("[ai-build] Pending", editor.aiContinuousBuildPending ? "true" : "false");
                gui::property_row("[ai-build] Runs", std::to_string(editor.aiContinuousBuildRunCount));
                gui::property_row("[ai-build] Status", editor.aiContinuousBuildStatus);
                gui::wrapped_label(
                    "AI action buttons are in the Inspector. This workspace stays focused on status, logs, and visual feedback.",
                    aiContentWidth);
            }

            if (showToolingControls)
            {
                gui::label("Tool Harness Controls");
                gui::property_row("[ai-tool] Runs", std::to_string(editor.aiToolHarnessRunCount));
                gui::property_row("[ai-tool] Status", editor.aiToolHarnessStatus);
                gui::property_row("[ai-tool] State", editor_tooling_state_summary(editor));
            }

            if (showLauncherControls)
            {
                gui::label("ProjectLauncher Evidence");
                gui::property_row("[launcher] Project root", display_project_path(editor.projectRoot));
                gui::property_row("[launcher] Manifest", file_ready_summary(editor.projectManifest));
                gui::property_row("[launcher] Build log", file_ready_summary(buildLog));
                gui::property_row("[launcher] Output", file_ready_summary(outputExe));
                gui::wrapped_label(
                    "Launcher tracks generated game/software project evidence. Sandbox build controls are kept in the Sandbox tab so normal project work and engine self-iteration do not blur together.",
                    aiContentWidth);
            }

            if (showAssistantControls)
            {
                gui::label("Assistant Model Actions");
                gui::wrapped_label(
                    "These buttons call the selected local OpenAI-compatible model. Tool evidence files are JSONL evidence logs, not a separate hidden LLM runtime.",
                    aiContentWidth);
            }

            if (showTrainingControls)
            {
                gui::label("Training Promotion Controls");
                gui::wrapped_label(
                    "Training actions only capture or promote visible evidence. They do not call a model and they do not run servers.",
                    aiContentWidth);
            }

            if (showWorkspaceActionButtons && showSandboxControls && gui::button(editor.aiContinuousBuildEnabled ? "Pause Self-Iteration Watcher" : "Enable Self-Iteration Watcher", { 260.0f, 30.0f }))
            {
                editor.aiContinuousBuildEnabled = !editor.aiContinuousBuildEnabled;
                editor.aiContinuousBuildStatus = editor.aiContinuousBuildEnabled
                    ? "Self-iteration watcher enabled; watching project/script evidence."
                    : "Self-iteration watcher paused.";
                if (editor.aiContinuousBuildEnabled)
                    editor.aiContinuousBuildFingerprint.clear();
                push_editor_log(editor, editor.aiContinuousBuildEnabled
                    ? "[ai-build] Self-iteration watcher enabled."
                    : "[ai-build] Self-iteration watcher paused.");
                append_project_note(
                    editor,
                    editor.aiContinuousBuildEnabled ? "Self-Iteration Watcher Enabled" : "Self-Iteration Watcher Paused",
                    editor.aiContinuousBuildStatus,
                    "The watcher only stages evidence packets; it does not write repo changes blindly.");
            }

            if (showWorkspaceActionButtons && showSandboxControls && gui::button("Queue Sandbox Build Pass", { 240.0f, 30.0f }))
            {
                startAiContinuousBuild("manual self-iteration build request", true);
                append_project_note(
                    editor,
                    "Queue Sandbox Build Pass",
                    editor.aiContinuousBuildStatus,
                    "Wait for build evidence, then review the staged packet before promotion.");
            }

            if (showWorkspaceActionButtons && showToolingControls && gui::button("Run AI Tool Harness", { 220.0f, 30.0f }))
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
                    .source_path = activeScriptSource
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
                    const std::string packetDir = epoch::ai::stage_iteration_packet(currentIterationPacket());
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

            if (showWorkspaceActionButtons && (showSandboxControls || showTrainingControls) && gui::button("Stage Evidence Packet", { 220.0f, 30.0f }))
            {
                const std::string packetDir = epoch::ai::stage_iteration_packet(currentIterationPacket());
                if (packetDir.empty())
                {
                    push_editor_log(editor, "[ai] Failed to stage iteration packet.");
                }
                else
                {
                    push_editor_log(editor, "[ai] Staged AI iteration packet.");
                    push_editor_log(editor, std::string("[ai] Iteration packet path: ") + packetDir);
                    append_project_note(
                        editor,
                        "Manual Iteration Packet Staged",
                        std::string("Packet staged at ") + packetDir,
                        "Use this as the visible evidence handoff for the next approved self-iteration pass.");
                }
            }

            if (showWorkspaceActionButtons && (showSandboxControls || showAssistantControls) && gui::button("Ask Selected Model For Plan", { 260.0f, 30.0f }))
            {
                if (epoch::ai::active_model_name().empty())
                {
                    push_editor_log(editor, "[ai] Select a local chat model before requesting a model plan.");
                }
                else
                {
                    const std::string packetDir = epoch::ai::stage_iteration_packet(currentIterationPacket());
                    const std::string prompt = build_ai_project_output_review_prompt(
                        editor,
                        pathsManifest,
                        buildLog,
                        outputExe);
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

            if (showWorkspaceActionButtons && showSandboxControls && gui::button("Stage Scene Training Task", { 250.0f, 30.0f }))
            {
                auto packet = currentIterationPacket();
                packet.packet_name = editor.projectId.empty()
                    ? std::string("sandbox-scene-training")
                    : editor.projectId + "-sandbox-scene-training";
                packet.task_prompt = build_ai_sandbox_scene_training_prompt(editor);
                packet.operator_notes =
                    "EpochBot must use visible sandbox scene/tool/build evidence. Generic self-reporting like 'working fine' is invalid without paths, changed object state, and verifier output.";
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
                        "Use this for watchable EpochBot scene-edit/test learning; reject answers without evidence paths or visible state changes.");
                }
            }

            if (showWorkspaceActionButtons && showTrainingControls && gui::button("Capture Tool Evidence Snapshot", { 260.0f, 30.0f }))
            {
                epoch::ai::append_mcp_capture(currentMcpRecord());
                push_editor_log(editor, "[ai] Captured tool evidence training snapshot.");
                push_editor_log(editor, std::string("[ai] Tool evidence path: ") + training.mcp_capture_jsonl);
                append_project_note(
                    editor,
                    "Capture Tool Evidence Snapshot",
                    std::string("Tool evidence snapshot appended to ") + training.mcp_capture_jsonl,
                    "Review captured evidence before curating or promoting training data.");
            }

            if (showWorkspaceActionButtons && showTrainingControls && gui::button("Promote Tool Evidence Snapshot", { 270.0f, 30.0f }))
            {
                const bool ok = epoch::ai::promote_mcp_capture_record(currentMcpRecord(), "epoch_mcp_curated");
                push_editor_log(editor, ok
                    ? "[ai] Promoted tool evidence snapshot into Engine/ai/datasets/curated."
                    : "[ai] Failed to promote tool evidence snapshot.");
                if (ok)
                {
                    push_editor_log(editor, std::string("[ai] Curated dataset root: ") + training.curated_dataset_root);
                    append_project_note(
                        editor,
                        "Promote Tool Evidence Snapshot",
                        std::string("Promoted into ") + training.curated_dataset_root,
                        "Promotion happened only from the explicit Training domain control.");
                }
            }

            if (showWorkspaceActionButtons && showTrainingControls && gui::button("Promote Scene Eval", { 220.0f, 30.0f }))
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
                    push_editor_log(editor, std::string("[ai] Eval suite root: ") + training.eval_root);
            }

            if (showWorkspaceActionButtons && showTrainingControls && gui::button("Promote Latest Chat Pair", { 220.0f, 30.0f }))
            {
                const std::string latestPrompt = last_chat_line_with_prefix(chat, "you> ");
                const std::string latestReply = last_chat_line_with_prefix(chat, "bot> ");
                const bool ok = !latestPrompt.empty() && !latestReply.empty()
                    && latestReply != "(empty reply)"
                    && epoch::ai::promote_dataset_record(epoch::ai::DatasetRecord{
                        .prompt = latestPrompt,
                        .answer = latestReply,
                        .source = "editor_ai_chat_curated",
                        .role = "assistant",
                        .tags = { editor.projectId, editor.activeRuntimeScene, "editor-chat" }
                    }, "epoch_editor_curated");
                push_editor_log(editor, ok
                    ? "[ai] Promoted latest chat pair into Engine/ai/datasets/curated."
                    : "[ai] No valid chat pair available to curate.");
                if (ok)
                    push_editor_log(editor, std::string("[ai] Curated dataset root: ") + training.curated_dataset_root);
            }
            break;
        }
        case EditorWorkspaceTab::Systems:
        {
            const auto orderedSystems = epoch::systems::Registry::instance().ordered_systems();
            const std::size_t workerCount = (std::max)(std::size_t{ 1 },
                std::thread::hardware_concurrency() > 0
                ? static_cast<std::size_t>(std::thread::hardware_concurrency())
                : std::size_t{ 6 });
            const std::string supportTier = recommended_support_tier(ctx, workerCount);
            const std::string pacingHealth = pacing_health_summary(editor.timeSnapshot);
            const std::string backendGuidance = backend_runtime_guidance(ctx, supportTier);
            const std::string convergenceFocus = backend_convergence_focus(ctx);
            const float contentWidth = (std::max)(180.0f, log_size.x - 24.0f);
            const float graphGap = 12.0f;
            const float graphWidth = (std::max)(180.0f, (contentWidth - graphGap) * 0.5f);
            const float graphHeight = 156.0f;
            const float supportHeight = 72.0f;

            gui::property_row("[systems] Renderer", renderer_name(ctx));
            gui::property_row("[systems] Active backend", renderer_name(ctx));
            gui::property_row("[systems] Ownership model", backend_ownership_model(ctx));
            gui::property_row("[systems] Editor backend target", "Single-context OpenGL");
            gui::property_row("[systems] Launcher backend target", "Single-context software");
            gui::property_row("[systems] Backend lifecycle", backend_lifecycle_policy());
            gui::property_row("[systems] Preview camera", preview_camera_name(ctx));
            gui::property_row("[systems] Runtime target", editor.activeRuntimeScene);
            gui::property_row("[systems] Registered systems", std::to_string(orderedSystems.size));
            gui::property_row("[systems] Worker lanes", std::to_string(workerCount));
            gui::property_row("[systems] Compatibility target", "6-core / 1660 Ti-era desktop and modern Linux laptops by default");
            gui::property_row("[systems] Support tier", supportTier);
            gui::property_row("[build] Compiler", compiler_identity());
            gui::property_row("[build] Configuration", build_configuration_label());
            gui::property_row("[build] Language mode", language_mode_summary());
            gui::property_row("[build] Feature probes", feature_probe_summary());
            gui::property_row("[build] CI contract", hosted_ci_contract());
            const std::filesystem::path phase5PacketRoot{ epoch::ai::iteration_packet_root() };
            gui::property_row("[phase5] Evidence bridge", "Systems build truth -> AI staged packets");
            gui::property_row("[phase5] Watcher", editor.aiContinuousBuildEnabled ? "enabled" : "paused");
            gui::property_row("[phase5] Build pending", editor.aiContinuousBuildPending ? "true" : "false");
            gui::property_row("[phase5] Build runs", std::to_string(editor.aiContinuousBuildRunCount));
            gui::property_row("[phase5] Build status", editor.aiContinuousBuildStatus);
            gui::property_row("[phase5] Tool runs", std::to_string(editor.aiToolHarnessRunCount));
            gui::property_row("[phase5] Tool status", editor.aiToolHarnessStatus);
            gui::property_row("[phase5] Staged packets", staged_packet_count_summary(phase5PacketRoot));
            gui::property_row("[phase5] Packet root", display_project_path(phase5PacketRoot));
            gui::property_row("[phase5] Gate contract", "planner -> executor -> builder -> verifier -> gate");
            gui::property_row("[systems] Render path", "visibility -> surface -> lighting -> temporal -> reconstruction -> present");
            gui::wrapped_label(
                "The Systems workspace now shows engine-generated graph surfaces with pan/zoom controls. Backend ownership is also written down here so the active backend, the dock/undock contract, and the long-term single-backend shell targets stay visible instead of living only in roadmap text.",
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                "Phase 5 evidence is mirrored here so build confidence, tool-harness activity, staged packet counts, and the review-gate contract are visible from Systems before an AI pass is allowed to promote anything.",
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::property_row("[time] State", editor.timeSnapshot.paused ? "Paused" : "Running");
            gui::property_row("[time] Frame dt", format_ms(editor.timeSnapshot.real_dt_seconds));
            gui::property_row("[time] Scaled dt", format_ms(editor.timeSnapshot.scaled_dt_seconds));
            gui::property_row(
                "[time] Fixed step",
                std::string(format_ms(editor.timeSnapshot.fixed_dt_seconds)) + " / " + format_rate(editor.timeSnapshot.fixed_dt_seconds));
            gui::property_row("[time] Simulated", format_seconds(editor.timeSnapshot.simulated_seconds));
            gui::property_row("[time] Accumulator", format_ms(editor.timeSnapshot.accumulator_seconds));
            gui::property_row("[time] Step count", std::to_string(editor.timeSnapshot.simulated_steps));
            gui::property_row("[time] Step budget", std::to_string(editor.timeSnapshot.step_budget));
            gui::property_row("[time] Frame step cap", std::to_string(editor.timeSnapshot.max_steps_per_frame));
            gui::property_row("[time] Time scale", std::format("{:.2f}x", editor.timeSnapshot.time_scale));
            gui::property_row("[time] Pacing health", pacingHealth);

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

            gui::wrapped_label(
                "Epoch is now formalizing a shared time spine here first: fixed-step accumulation, pause/resume, time scaling, single-step controls, and frame step budgeting are owned by the engine instead of being scattered ad hoc across contexts.",
                (std::max)(180.0f, log_size.x - 24.0f));
            const auto systemsOrigin = gui::cursor_position();

            const auto renderCanvas = build_render_graph_surface(
                editor.systems,
                epoch::ai::current_provider_mode() == epoch::ai::ProviderMode::McpOperations);
            const auto taskCanvas = build_task_graph_surface(
                editor.systems,
                workerCount,
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

            const float leftX = systemsOrigin.x;
            const float rightX = systemsOrigin.x + graphWidth + graphGap;
            const float titleY = systemsOrigin.y + 6.0f;
            const float controlsY = titleY + gui::line_height() + 4.0f;
            const float imageY = controlsY + 30.0f;

            gui::set_cursor({ leftX, titleY });
            gui::label("Render / Frame Graph");
            gui::set_cursor({ leftX, controlsY });
            const std::array renderButtons{
                gui::InlineButtonSpec{ .label = "<", .width = 28.0f },
                gui::InlineButtonSpec{ .label = "-", .width = 28.0f },
                gui::InlineButtonSpec{ .label = "+", .width = 28.0f },
                gui::InlineButtonSpec{ .label = ">", .width = 28.0f }
            };
            if (const auto action = gui::inline_button_row(renderButtons, 24.0f, 6.0f))
            {
                switch (*action)
                {
                case 0: editor.systems.renderPan = (std::max)(0, editor.systems.renderPan - 64); break;
                case 1: editor.systems.renderZoom = (std::max)(0.75f, editor.systems.renderZoom - 0.2f); break;
                case 2: editor.systems.renderZoom = (std::min)(2.0f, editor.systems.renderZoom + 0.2f); break;
                case 3: editor.systems.renderPan += 64; break;
                default: break;
                }
            }
            gui::set_cursor({ leftX, imageY });
            if (editor.systems.renderSurface.is_valid())
                gui::image(editor.systems.renderSurface, { graphWidth, graphHeight });
            else
                gui::wrapped_label("Render graph surface unavailable.", graphWidth);

            gui::set_cursor({ rightX, titleY });
            gui::label("Task / Thread Graph");
            gui::set_cursor({ rightX, controlsY });
            const std::array taskButtons{
                gui::InlineButtonSpec{ .label = "<", .width = 28.0f },
                gui::InlineButtonSpec{ .label = "-", .width = 28.0f },
                gui::InlineButtonSpec{ .label = "+", .width = 28.0f },
                gui::InlineButtonSpec{ .label = ">", .width = 28.0f }
            };
            if (const auto action = gui::inline_button_row(taskButtons, 24.0f, 6.0f))
            {
                switch (*action)
                {
                case 0: editor.systems.taskPan = (std::max)(0, editor.systems.taskPan - 64); break;
                case 1: editor.systems.taskZoom = (std::max)(0.75f, editor.systems.taskZoom - 0.2f); break;
                case 2: editor.systems.taskZoom = (std::min)(2.0f, editor.systems.taskZoom + 0.2f); break;
                case 3: editor.systems.taskPan += 64; break;
                default: break;
                }
            }
            gui::set_cursor({ rightX, imageY });
            if (editor.systems.taskSurface.is_valid())
                gui::image(editor.systems.taskSurface, { graphWidth, graphHeight });
            else
                gui::wrapped_label("Task graph surface unavailable.", graphWidth);

            const float supportY = imageY + graphHeight + 10.0f;
            gui::set_cursor({ systemsOrigin.x, supportY });
            gui::label("Hardware / Support Tiers");
            gui::set_cursor({ systemsOrigin.x, supportY + gui::line_height() + 4.0f });
            if (editor.systems.supportSurface.is_valid())
                gui::image(editor.systems.supportSurface, { contentWidth, supportHeight });
            else
                gui::wrapped_label("Support tier surface unavailable.", contentWidth);

            gui::set_cursor({ systemsOrigin.x, supportY + gui::line_height() + 4.0f + supportHeight + 10.0f });
            gui::property_row("[systems] Render stages", "Capture | Visibility | Surface | Lighting | Temporal | Present");
            gui::property_row("[systems] Task lanes", "Input | Systems | Scripts | AI | Output");
            gui::property_row("[systems] Lib strategy", "auto on capable hardware; developer can trim support tiers per game");
            gui::property_row("[systems] Tier policy", "Baseline by default, Standard on stronger 6-core+ GPUs/CPUs, Extended only by project opt-in");
            gui::property_row("[systems] Backend guidance", backendGuidance);
            gui::property_row("[systems] Convergence focus", convergenceFocus);
            for (auto* system : orderedSystems)
            {
                const auto name = system->name();
                gui::property_row(
                    "  system",
                    std::string(name.data ? name.data : "", name.size));
            }
            break;
        }
        case EditorWorkspaceTab::Output:
        default:
            gui::property_row("[info] Scene viewport", std::string(std::to_string(static_cast<int>(result.scene_viewport.size.x))
                + "x"
                + std::to_string(static_cast<int>(result.scene_viewport.size.y))));
            gui::property_row("[info] Active renderer", renderer_name(ctx));
            gui::property_row("[info] Preview mode", std::string(preview_mode_name(editor.previewMode)));
            gui::property_row("[info] Camera mode", preview_camera_name(ctx));
            gui::property_row("[info] Zoom", preview_zoom_text(ctx));
            gui::property_row("[info] Viewport input", "LMB pan | RMB orbit | Wheel zoom");
            gui::property_row("[info] Active script", editor.activeScript);
            gui::property_row("[info] Project runtime", editor.activeRuntimeScene);
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
        if (r.input.submitted || r.send_clicked)
        {
            std::string text = std::move(chat.input);
            chat.input.clear();
            chat.submit(std::move(text));
        }

        open_dropdown("File", TopMenu::File, dropdown_window_size(192.0f, 3), [&](gui::Vec2 pos)
        {
            menu_item("Open Launcher", { pos.x + 12.0f, pos.y + 14.0f }, 192.0f, [&]() {
                emit_command(EditorCommand::OpenLauncher);
                push_editor_log(editor, "[file] Opening launcher.");
            });
            menu_item("Settings", { pos.x + 12.0f, pos.y + 48.0f }, 192.0f, [&]() {
                emit_command(EditorCommand::Settings);
                push_editor_log(editor, "[file] Settings selected.");
            });
            menu_item("Exit", { pos.x + 12.0f, pos.y + 82.0f }, 192.0f, [&]() {
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

        open_dropdown("Asset", TopMenu::Asset, dropdown_window_size(220.0f, 6), [&](gui::Vec2 pos)
        {
            menu_item("Open Asset Browser", { pos.x + 12.0f, pos.y + 14.0f }, 220.0f, [&]() {
                editor.workspaceTab = EditorWorkspaceTab::Assets;
                push_editor_log(editor, "[assets] Asset browser opened.");
            });
            menu_item("Add Static Mesh", { pos.x + 12.0f, pos.y + 48.0f }, 220.0f, [&]() {
                add_entity(editor, "cube");
            });
            menu_item("Add Light", { pos.x + 12.0f, pos.y + 82.0f }, 220.0f, [&]() {
                add_entity(editor, "light");
            });
            menu_item("Add Spawn", { pos.x + 12.0f, pos.y + 116.0f }, 220.0f, [&]() {
                add_entity(editor, "spawn");
            });
            menu_item("Duplicate Selected", { pos.x + 12.0f, pos.y + 150.0f }, 220.0f, [&]() {
                duplicate_selected_entity(editor);
            });
            menu_item("Delete Selected", { pos.x + 12.0f, pos.y + 184.0f }, 220.0f, [&]() {
                delete_selected_entity(editor);
            });
        });

        open_dropdown("Window", TopMenu::Window, dropdown_window_size(216.0f, 4), [&](gui::Vec2 pos)
        {
            menu_item("Preview: Editor", { pos.x + 12.0f, pos.y + 14.0f }, 216.0f, [&]() {
                editor.previewMode = core::ScenePreviewMode::Editor;
                push_editor_log(editor, "[window] Preview mode set to Editor.");
            });
            menu_item("Preview: None", { pos.x + 12.0f, pos.y + 48.0f }, 216.0f, [&]() {
                editor.previewMode = core::ScenePreviewMode::None;
                push_editor_log(editor, "[window] Preview mode set to None.");
            });
            menu_item("Focus Selection", { pos.x + 12.0f, pos.y + 82.0f }, 216.0f, [&]() {
                handle_scene_tool(editor, "focus_selection");
            });
            menu_item("Toggle Helpers", { pos.x + 12.0f, pos.y + 116.0f }, 216.0f, [&]() {
                handle_scene_tool(editor, "toggle_helpers");
            });
        });

        open_dropdown("Tools", TopMenu::Tools, dropdown_window_size(228.0f, 5), [&](gui::Vec2 pos)
        {
            menu_item("Camera: Editor", { pos.x + 12.0f, pos.y + 14.0f }, 228.0f, [&]() {
                epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::Editor);
                push_editor_log(editor, "[tools] Camera mode set to Editor.");
            });
            menu_item("Camera: FPS", { pos.x + 12.0f, pos.y + 48.0f }, 228.0f, [&]() {
                epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::FPS);
                push_editor_log(editor, "[tools] Camera mode set to FPS.");
            });
            menu_item("Reset Preview Camera", { pos.x + 12.0f, pos.y + 82.0f }, 228.0f, [&]() {
                epochnamespace::previewgrid::reset_camera(ctx.get());
                push_editor_log(editor, "[tools] Preview camera reset.");
            });
            menu_item("Run Script", { pos.x + 12.0f, pos.y + 116.0f }, 228.0f, [&]() {
                emit_command(EditorCommand::RunScript, editor.activeScript);
                push_editor_log(editor, std::string("[tools] Script run requested for '") + editor.activeScript + "'.");
            });
            menu_item("Update to Latest...", { pos.x + 12.0f, pos.y + 150.0f }, 228.0f, [&]() {
                editor.showUpdateConfirmModal = true;
                editor.showSourceUpdateConfirmModal = false;
                push_editor_log(editor, "[tools] Latest update requested. Awaiting confirmation.");
            });
        });

        open_dropdown("Help", TopMenu::Help, dropdown_window_size(192.0f, 2), [&](gui::Vec2 pos)
        {
            menu_item("About Epoch", { pos.x + 12.0f, pos.y + 14.0f }, 192.0f, [&]() {
                editor.showAboutModal = true;
            });
            menu_item("Current Project Info", { pos.x + 12.0f, pos.y + 48.0f }, 192.0f, [&]() {
                push_editor_log(editor, std::string("[help] Active project: ") + editor.projectName);
            });
        });

        if (editor.showUpdateConfirmModal)
        {
            const gui::Vec2 modalSize{ 560.0f, 284.0f };
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_modal_window(gui::ModalWindowOptions{
                .title = "Update Epoch",
                .position = modalPos,
                .size = modalSize,
                .viewport_size = { w, h },
                .dim_background = true
            });
            const gui::Vec2 contentPos = gui::cursor_position();
            const float contentY = contentPos.y;
            gui::set_cursor({ contentPos.x + 8.0f, contentY });
            gui::wrapped_label("Update to Latest checks the newest packaged release first.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 36.0f });
            gui::wrapped_label("If the packaged release is already current, Epoch falls back to the latest main source.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 84.0f });
            gui::wrapped_label("That source fallback restores dependencies, rebuilds Epoch, and replaces this runtime.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 132.0f });
            gui::wrapped_label("Use Advanced Source only when you intentionally want to skip straight to a rebuild from main.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentPos.y + 180.0f });
            if (gui::button("Cancel", { 120.0f, 30.0f }))
            {
                editor.showUpdateConfirmModal = false;
                push_editor_log(editor, "[command] Update canceled.");
            }
            gui::set_cursor({ contentPos.x + 148.0f, contentPos.y + 180.0f });
            if (gui::button("Update to Latest", { 168.0f, 30.0f }))
            {
                editor.showUpdateConfirmModal = false;
                emit_command(EditorCommand::UpdateApplication);
                push_editor_log(editor, "[command] Smart update confirmed.");
            }
            gui::set_cursor({ contentPos.x + 332.0f, contentPos.y + 180.0f });
            if (gui::button("Advanced Source...", { 176.0f, 30.0f }))
            {
                editor.showUpdateConfirmModal = false;
                editor.showSourceUpdateConfirmModal = true;
                push_editor_log(editor, "[command] Advanced source rebuild requested. Awaiting confirmation.");
            }
            gui::end_modal_window();
        }

        if (editor.showSourceUpdateConfirmModal)
        {
            const gui::Vec2 modalSize{ 620.0f, 292.0f };
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentWidth = modalSize.x - 32.0f;
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
            gui::wrapped_label("This skips the packaged release check and goes straight to the latest main source.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 36.0f });
            gui::wrapped_label("Use it when you explicitly want to test current source before a release exists.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 84.0f });
            gui::wrapped_label("Epoch restores dependencies, rebuilds from source, and replaces this runtime.", contentWidth);
            gui::set_cursor({ contentPos.x + 8.0f, contentY + 132.0f });
            gui::wrapped_label("For normal updates, use Update to Latest and let it fall back automatically when needed.", contentWidth);
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
            if (gui::button("Rebuild From Source", { 176.0f, 30.0f }))
            {
                editor.showSourceUpdateConfirmModal = false;
                emit_command(EditorCommand::UpdateApplicationFromSource);
                push_editor_log(editor, "[command] Advanced source rebuild confirmed.");
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
                emit_command(EditorCommand::UpdateApplication);
                break;
            case EditorAutomationCommand::SourceUpdate:
                if (editor.automationConsumed)
                    break;
                editor.automationConsumed = true;
                push_editor_log(editor, "[command] Auto command triggered: advanced source rebuild.");
                append_editor_automation_trace("triggered source-update");
                emit_command(EditorCommand::UpdateApplicationFromSource);
                break;
            case EditorAutomationCommand::None:
                break;
            }
        }

        if (editor.showAboutModal)
        {
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

        return result;
    }

} // namespace epochnamespace
