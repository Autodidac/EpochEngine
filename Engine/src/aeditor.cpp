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
#include <cstdlib>
#include <cstddef>
#include <format>
#include <fstream>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

module aeditor;

import aengine.gui;
import aengine.version;
import core.context;
import context.type;
import aengine.input;
import ascripting.system;
import epoch.ai;
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
                lines.emplace_back("bot> Ready. Provider: " + epoch::ai::active_provider_summary());
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

        struct EditorState
        {
            bool initialized{ false };
            TopMenu openMenu{ TopMenu::None };
            std::string projectId{ "sandbox" };
            std::string projectName{ "Sandbox" };
            std::string projectPath{ "Projects/Sandbox/scene.epoch" };
            std::string activeScript{ "rotate_all_entities" };
            std::string activeRuntimeScene{ "project:sandbox" };
            std::string activeWorld{ "PersistentLevel" };
            std::vector<EditorEntity> entities{};
            std::size_t selectedEntity{ 0 };
            std::vector<std::string> logLines{};
            bool helpersVisible{ true };
            core::ScenePreviewMode previewMode{ core::ScenePreviewMode::Editor };
            EditorWorkspaceTab workspaceTab{ EditorWorkspaceTab::Output };
            bool showAboutModal{ false };
            bool showUpdateConfirmModal{ false };
            bool showSourceUpdateConfirmModal{ false };
            EditorAutomationCommand automationCommand{ EditorAutomationCommand::None };
            bool automationConsumed{ false };
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
            state.projectPath = std::string(profile->scene_path);
            state.activeWorld = std::string(profile->world_name);
            state.activeScript = std::string(profile->default_script);
            state.activeRuntimeScene = std::string(profile->runtime_scene_id);

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
                set_project(it->second, "sandbox", false);
                if (ctx)
                    epochnamespace::previewgrid::set_camera_mode(ctx.get(), epochnamespace::previewgrid::CameraMode::Editor);
                push_editor_log(it->second, "[info] Editor scene initialized.");
                push_editor_log(it->second, "[info] Use the Project, Scripts, AI, Systems, and Output workspaces to drive scene play, scripting, and assistant work.");
                push_editor_log(it->second, "[info] Scene viewport is owned by the active backend.");
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

    bool editor_run_script(const core::Context* ctx, std::string_view script_name)
    {
        if (!ctx)
            return false;

        scripting::ScriptLoadReport report;
        EpochScriptHost host{
            .user_data = const_cast<core::Context*>(ctx),
            .log = &script_log_callback,
            .rotate_all_entities_yaw = &script_rotate_all_entities_yaw_callback
        };

        const bool ok = scripting::load_or_reload_script(std::string(script_name), &host, &report);

        auto& storage = editor_storage();
        std::scoped_lock lock(storage.mutex);
        const auto it = storage.states.find(ctx);
        if (it != storage.states.end())
        {
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

        const float w = static_cast<float>(ctx->get_width_safe());
        const float h = static_cast<float>(ctx->get_height_safe());

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
            const std::string label = editor.openMenu == item.menu
                ? std::string("[") + std::string(item.label) + "]"
                : std::string(item.label);
            if (gui::button(label, { item.width, toolbar_button_h }))
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
        const std::string add_cube_tab = "Add Cube";
        const std::string add_light_tab = "Add Light";
        const std::string ask_ai_tab = "Ask AI";

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(std::string("[") + editor_tab + "]", { 180.0f, tab_h }))
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
        if (gui::button(add_cube_tab, { 164.0f, tab_h }))
            add_entity(editor, "cube");
        tab_x += 164.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(add_light_tab, { 164.0f, tab_h }))
            add_entity(editor, "light");
        tab_x += 164.0f + tab_gap;

        gui::set_cursor({ tab_x, tab_y });
        if (gui::button(ask_ai_tab, { 180.0f, tab_h }))
            submit_ai_prompt(build_ai_scene_prompt(editor), "[ai] Asked for scene guidance.");

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

        auto open_dropdown = [&](std::string_view title, TopMenu menu, gui::Vec2 size, auto&& body)
        {
            if (editor.openMenu != menu)
                return;
            const auto pos = dropdown_position_for(menu);
            gui::begin_window(title, pos, size);
            body(pos);
            gui::end_window();
        };

        gui::begin_window("World Outliner", outliner_pos, outliner_size);
        gui::label(std::string("Scene: ") + editor.activeWorld);
        gui::label(std::string("Project Root: ") + editor.projectPath);
        gui::label(std::string("Entities: ") + std::to_string(editor.entities.size()));
        if (gui::button("+ Cube", { 92.0f, 26.0f }))
            add_entity(editor, "cube");
        if (gui::button("+ Light", { 92.0f, 26.0f }))
            add_entity(editor, "light");
        if (gui::button("+ Spawn", { 92.0f, 26.0f }))
            add_entity(editor, "spawn");
        if (gui::button("Duplicate", { 92.0f, 26.0f }))
            duplicate_selected_entity(editor);
        if (gui::button("Delete", { 92.0f, 26.0f }))
            delete_selected_entity(editor);
        for (std::size_t i = 0; i < editor.entities.size(); ++i)
        {
            const auto& entity = editor.entities[i];
            std::string label = (i == editor.selectedEntity ? "> " : "") + entity.name + " [" + entity.type + "]";
            if (!entity.visible)
                label += " (hidden)";
            if (gui::button(label, { (std::max)(120.0f, outliner_size.x - 18.0f), 28.0f }))
            {
                editor.selectedEntity = i;
                push_editor_log(editor, std::string("[select] ") + entity.name);
            }
        }
        gui::end_window();

        gui::begin_window("Inspector", details_pos, details_size);
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
        gui::label(std::string("Editor Script: ") + editor.activeScript);
        gui::label(std::string("Runtime Target: ") + editor.activeRuntimeScene);
        gui::label("Viewport Input: LMB pan  |  RMB orbit  |  Wheel zoom");
        gui::end_window();

        result.scene_viewport = gui::scene_viewport("Perspective", viewport_pos, viewport_size);
        ctx->set_scene_preview_mode(editor.previewMode);
        ctx->set_scene_viewport(core::RenderViewport{
            static_cast<int>((std::max)(0.0f, result.scene_viewport.position.x)),
            static_cast<int>((std::max)(0.0f, result.scene_viewport.position.y)),
            static_cast<int>((std::max)(0.0f, result.scene_viewport.size.x)),
            static_cast<int>((std::max)(0.0f, result.scene_viewport.size.y))
        });

        const float split = 0.55f;
        const float left_bottom_w = w * split;
        const gui::Vec2 log_pos{ 0.0f, bottom_pos.y };
        const gui::Vec2 log_size{ left_bottom_w, bottom_h };
        const gui::Vec2 chat_pos{ left_bottom_w, bottom_pos.y };
        const gui::Vec2 chat_size{ (std::max)(0.0f, w - left_bottom_w), bottom_h };

        gui::begin_window("Workspace", log_pos, log_size);
        const std::array<gui::SegmentedButtonSpec, 5> workspaceTabs{{
            { "Output", 82.0f, editor.workspaceTab == EditorWorkspaceTab::Output },
            { "Project", 82.0f, editor.workspaceTab == EditorWorkspaceTab::Project },
            { "Scripts", 82.0f, editor.workspaceTab == EditorWorkspaceTab::Scripts },
            { "AI", 68.0f, editor.workspaceTab == EditorWorkspaceTab::AI },
            { "Systems", 90.0f, editor.workspaceTab == EditorWorkspaceTab::Systems }
        }};
        if (const auto selected = gui::segmented_button_row(workspaceTabs))
            editor.workspaceTab = static_cast<EditorWorkspaceTab>(*selected);

        switch (editor.workspaceTab)
        {
        case EditorWorkspaceTab::Project:
        {
            const auto* activeProfile = editor_find_project_profile(editor.projectId);
            if (!activeProfile)
                activeProfile = &editor_default_project_profile();

            gui::property_row("[project] Active", activeProfile->display_name.data());
            gui::property_row("[project] Scene", activeProfile->scene_path.data());
            gui::property_row("[project] World", activeProfile->world_name.data());
            gui::property_row("[project] Runtime", activeProfile->runtime_scene_id.data());
            gui::wrapped_label(activeProfile->description.data(), (std::max)(180.0f, log_size.x - 24.0f));

            for (const auto& profile : editor_project_profiles())
            {
                const std::string buttonLabel =
                    (editor.projectId == profile.id ? std::string("> ") : std::string())
                    + std::string(profile.display_name);
                if (gui::button(buttonLabel, { (std::max)(180.0f, log_size.x - 24.0f), 28.0f }))
                {
                    set_project(editor, profile.id, true);
                    epochnamespace::previewgrid::reset_camera(ctx.get());
                }
            }

            if (gui::button("Play Current Project", { 180.0f, 30.0f }))
            {
                emit_command(EditorCommand::RunGame, editor.activeRuntimeScene);
                push_editor_log(editor, std::string("[project] Play requested for ") + editor.projectName + ".");
            }
            break;
        }
        case EditorWorkspaceTab::Scripts:
        {
            gui::property_row("[script] Active", editor.activeScript);
            gui::wrapped_label(
                "Scripts compile with the engine/project and use the editor host API for callbacks.",
                (std::max)(180.0f, log_size.x - 24.0f));

            for (const auto& script : editor_script_profiles())
            {
                const std::string buttonLabel =
                    (editor.activeScript == script.id ? std::string("> ") : std::string())
                    + std::string(script.display_name);
                if (gui::button(buttonLabel, { (std::max)(180.0f, log_size.x - 24.0f), 28.0f }))
                {
                    editor.activeScript = std::string(script.id);
                    push_editor_log(editor, std::string("[script] Selected ") + editor.activeScript + ".");
                }
                gui::property_row("  source", script.source_path.data());
                gui::wrapped_label(script.description.data(), (std::max)(160.0f, log_size.x - 36.0f));
            }

            if (gui::button("Run Selected Script", { 180.0f, 30.0f }))
            {
                emit_command(EditorCommand::RunScript, editor.activeScript);
                push_editor_log(editor, std::string("[script] Run requested for '") + editor.activeScript + "'.");
            }
            break;
        }
        case EditorWorkspaceTab::AI:
        {
            const auto manifest = epoch::ai::active_model_manifest();
            const auto training = epoch::ai::default_training_paths();
            gui::property_row("[ai] Provider", std::string(epoch::ai::provider_mode_name(epoch::ai::current_provider_mode())));
            gui::property_row("[ai] Active model", manifest.display_name.empty() ? std::string("(detecting)") : manifest.display_name);
            gui::property_row("[ai] Oracle endpoint", manifest.endpoint);
            gui::property_row("[ai] Oracle manifest", manifest.manifest_path);
            gui::property_row("[ai] Curated datasets", training.curated_dataset_root);
            gui::property_row("[ai] Raw capture", training.local_capture_jsonl);
            gui::property_row("[ai] Local models", training.model_root);
            gui::property_row("[ai] Checkpoints", training.checkpoint_root);
            gui::wrapped_label(
                "Epoch now tracks three AI roles: a tiny embedded engine model, an MCP-backed operating layer for retrieval and tool use, and an LM Studio teacher/oracle for evals, bootstrapping, and live editor help.",
                (std::max)(180.0f, log_size.x - 24.0f));
            gui::wrapped_label(
                "Raw chat captures land in workspace/auto_train.jsonl as Git-safe staging data and can be curated into Engine/ai/datasets/curated, while compiled checkpoints/models/caches stay local-only.",
                (std::max)(180.0f, log_size.x - 24.0f));
            break;
        }
        case EditorWorkspaceTab::Systems:
            gui::property_row("[systems] Render graph", "planned as a first-class systems surface");
            gui::property_row("[systems] Task graph", "active through engine-owned async work and script jobs");
            gui::property_row("[systems] Renderer", renderer_name(ctx));
            gui::property_row("[systems] Preview camera", preview_camera_name(ctx));
            gui::property_row("[systems] Runtime target", editor.activeRuntimeScene);
            gui::property_row("[systems] Graph surface", "next step is an engine-generated texture preview for frame/task graph output");
            gui::wrapped_label(
                "This tab is the landing zone for render graph, frame graph, threading, and multi-backend diagnostics as the editor grows toward an Unreal/Godot-style systems interface.",
                (std::max)(180.0f, log_size.x - 24.0f));
            break;
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
            for (const auto& line : editor.logLines)
                gui::label(line);
            break;
        }
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

        open_dropdown("File", TopMenu::File, { 220.0f, 144.0f }, [&](gui::Vec2 pos)
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

        open_dropdown("Edit", TopMenu::Edit, { 220.0f, 144.0f }, [&](gui::Vec2 pos)
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

        open_dropdown("Asset", TopMenu::Asset, { 248.0f, 212.0f }, [&](gui::Vec2 pos)
        {
            menu_item("Add Static Mesh", { pos.x + 12.0f, pos.y + 14.0f }, 220.0f, [&]() {
                add_entity(editor, "cube");
            });
            menu_item("Add Light", { pos.x + 12.0f, pos.y + 48.0f }, 220.0f, [&]() {
                add_entity(editor, "light");
            });
            menu_item("Add Spawn", { pos.x + 12.0f, pos.y + 82.0f }, 220.0f, [&]() {
                add_entity(editor, "spawn");
            });
            menu_item("Duplicate Selected", { pos.x + 12.0f, pos.y + 116.0f }, 220.0f, [&]() {
                duplicate_selected_entity(editor);
            });
            menu_item("Delete Selected", { pos.x + 12.0f, pos.y + 150.0f }, 220.0f, [&]() {
                delete_selected_entity(editor);
            });
        });

        open_dropdown("Window", TopMenu::Window, { 244.0f, 178.0f }, [&](gui::Vec2 pos)
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

        open_dropdown("Tools", TopMenu::Tools, { 260.0f, 212.0f }, [&](gui::Vec2 pos)
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

        open_dropdown("Help", TopMenu::Help, { 220.0f, 110.0f }, [&](gui::Vec2 pos)
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
            const float contentY = modalPos.y + 38.0f;
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_window("Update Epoch", modalPos, modalSize);
            gui::set_cursor({ modalPos.x + 16.0f, contentY });
            gui::wrapped_label("Update to Latest checks the newest packaged release first.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 36.0f });
            gui::wrapped_label("If the packaged release is already current, Epoch falls back to the latest main source.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 84.0f });
            gui::wrapped_label("That source fallback restores dependencies, rebuilds Epoch, and replaces this runtime.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 132.0f });
            gui::wrapped_label("Use Advanced Source only when you intentionally want to skip straight to a rebuild from main.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, modalPos.y + 218.0f });
            if (gui::button("Cancel", { 120.0f, 30.0f }))
            {
                editor.showUpdateConfirmModal = false;
                push_editor_log(editor, "[command] Update canceled.");
            }
            gui::set_cursor({ modalPos.x + 156.0f, modalPos.y + 218.0f });
            if (gui::button("Update to Latest", { 168.0f, 30.0f }))
            {
                editor.showUpdateConfirmModal = false;
                emit_command(EditorCommand::UpdateApplication);
                push_editor_log(editor, "[command] Smart update confirmed.");
            }
            gui::set_cursor({ modalPos.x + 340.0f, modalPos.y + 218.0f });
            if (gui::button("Advanced Source...", { 176.0f, 30.0f }))
            {
                editor.showUpdateConfirmModal = false;
                editor.showSourceUpdateConfirmModal = true;
                push_editor_log(editor, "[command] Advanced source rebuild requested. Awaiting confirmation.");
            }
            gui::end_window();
        }

        if (editor.showSourceUpdateConfirmModal)
        {
            const gui::Vec2 modalSize{ 620.0f, 292.0f };
            const gui::Vec2 modalPos{
                (std::max)(0.0f, (w - modalSize.x) * 0.5f),
                (std::max)(0.0f, (h - modalSize.y) * 0.5f)
            };
            const float contentY = modalPos.y + 38.0f;
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_window("Rebuild From Main Source", modalPos, modalSize);
            gui::set_cursor({ modalPos.x + 16.0f, contentY });
            gui::wrapped_label("This skips the packaged release check and goes straight to the latest main source.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 36.0f });
            gui::wrapped_label("Use it when you explicitly want to test current source before a release exists.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 84.0f });
            gui::wrapped_label("Epoch restores dependencies, rebuilds from source, and replaces this runtime.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 132.0f });
            gui::wrapped_label("For normal updates, use Update to Latest and let it fall back automatically when needed.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, modalPos.y + 226.0f });
            if (gui::button("Back", { 120.0f, 30.0f }))
            {
                editor.showSourceUpdateConfirmModal = false;
                editor.showUpdateConfirmModal = true;
            }
            gui::set_cursor({ modalPos.x + 156.0f, modalPos.y + 226.0f });
            if (gui::button("Cancel", { 120.0f, 30.0f }))
            {
                editor.showSourceUpdateConfirmModal = false;
                push_editor_log(editor, "[command] Advanced source rebuild canceled.");
            }
            gui::set_cursor({ modalPos.x + 292.0f, modalPos.y + 226.0f });
            if (gui::button("Rebuild From Source", { 176.0f, 30.0f }))
            {
                editor.showSourceUpdateConfirmModal = false;
                emit_command(EditorCommand::UpdateApplicationFromSource);
                push_editor_log(editor, "[command] Advanced source rebuild confirmed.");
            }
            gui::end_window();
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
            const float contentY = modalPos.y + 38.0f;
            const float contentWidth = modalSize.x - 32.0f;
            gui::begin_window("About Epoch", modalPos, modalSize);
            gui::set_cursor({ modalPos.x + 16.0f, contentY });
            gui::label("Epoch Editor");
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 22.0f });
            gui::label(std::string("Version: ") + epochnamespace::GetEngineDisplayString());
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 46.0f });
        gui::wrapped_label("Multi-backend engine/editor shell with project-driven scene play, docked scripting, and engine-owned tools.", contentWidth);
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 86.0f });
            gui::label(std::string("Renderer: ") + renderer_name(ctx));
            gui::set_cursor({ modalPos.x + 16.0f, contentY + 110.0f });
            gui::label(std::string("Project: ") + editor.projectName);
            gui::set_cursor({ modalPos.x + 16.0f, modalPos.y + 170.0f });
            if (gui::button("Close", { 120.0f, 30.0f }))
                editor.showAboutModal = false;
            gui::end_window();
        }

        return result;
    }

} // namespace epochnamespace


