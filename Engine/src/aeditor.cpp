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

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <format>
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
import aengine.core.context;
import aengine.input;
import epoch.ai;

namespace epochnamespace
{
    namespace
    {
        using namespace std::chrono_literals;

        enum class CommandPanel : unsigned char
        {
            Projects,
            Games,
            Scene
        };

        struct CommandChip
        {
            std::string_view label{};
            std::string_view argument{};
            float width{};
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
            std::optional<std::future<std::string>> pending{};

            AiChat()
            {
                lines.emplace_back("bot> Ready. Endpoint: http://localhost:1234");
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
                    trim_lines();
                }
                catch (const std::exception& e)
                {
                    lines.emplace_back(std::string("bot> (error) ") + e.what());
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
            CommandPanel activePanel{ CommandPanel::Projects };
            std::string projectName{ "Sandbox" };
            std::string projectPath{ "Projects/Sandbox/scene.epoch" };
            std::string activeWorld{ "PersistentLevel" };
            std::vector<EditorEntity> entities{};
            std::size_t selectedEntity{ 0 };
            std::vector<std::string> logLines{};
            bool helpersVisible{ true };
            std::size_t projectCommandSelection{ 0 };
            std::size_t gameCommandSelection{ 0 };
            std::size_t sceneCommandSelection{ 0 };
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

        [[nodiscard]] std::vector<EditorEntity> sandbox_entities()
        {
            return {
                { "PersistentLevel", "Level", "World" },
                { "EditorCamera", "Camera", "Editor", { 0.0f, 1.5f, 5.0f } },
                { "DirectionalLight", "Light", "Lighting", { 2.0f, 4.0f, 1.0f }, { -35.0f, 45.0f, 0.0f } },
                { "WorldGrid", "Helper", "Editor", { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 10.0f, 1.0f, 10.0f }, true, true },
                { "StarterCube", "StaticMesh", "Gameplay", { 0.0f, 0.5f, 0.0f } },
                { "PlayerStart", "Spawn", "Gameplay", { 0.0f, 0.0f, -2.0f } }
            };
        }

        [[nodiscard]] std::vector<EditorEntity> platformer_entities()
        {
            return {
                { "PlatformerLevel", "Level", "World" },
                { "GameplayCamera", "Camera", "Gameplay", { 0.0f, 3.0f, 8.0f }, { -18.0f, 0.0f, 0.0f } },
                { "SkyLight", "Light", "Lighting", { 3.0f, 6.0f, 2.0f }, { -25.0f, 35.0f, 0.0f } },
                { "GroundPlane", "StaticMesh", "Gameplay", { 0.0f, -0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 16.0f, 1.0f, 4.0f } },
                { "PlayerStart", "Spawn", "Gameplay", { -4.0f, 0.0f, 0.0f } },
                { "MovingPlatform_A", "Mover", "Gameplay", { 1.5f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 2.5f, 0.4f, 1.0f } },
                { "CoinArc", "CollectibleSet", "Gameplay", { 4.0f, 2.5f, 0.0f } }
            };
        }

        [[nodiscard]] std::vector<EditorEntity> puzzle_entities()
        {
            return {
                { "PuzzleWorld", "Level", "World" },
                { "OverviewCamera", "Camera", "Gameplay", { 0.0f, 7.0f, 9.0f }, { -38.0f, 0.0f, 0.0f } },
                { "KeyLight", "Light", "Lighting", { 1.5f, 5.5f, 2.0f }, { -40.0f, 25.0f, 0.0f } },
                { "PuzzleGrid", "Grid", "Gameplay", { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 6.0f, 1.0f, 6.0f } },
                { "SlidingBoard", "PuzzleBoard", "Gameplay", { 0.0f, 0.5f, 0.0f } },
                { "HintTerminal", "Interactable", "Gameplay", { -2.5f, 0.0f, 1.5f } },
                { "GoalSocket", "Target", "Gameplay", { 2.5f, 0.0f, -1.5f } }
            };
        }

        void set_project(EditorState& state, std::string_view projectId, bool writeLog)
        {
            state.helpersVisible = true;

            if (projectId == "platformer")
            {
                state.projectName = "PlatformerDemo";
                state.projectPath = "Projects/PlatformerDemo/worlds/platformer.epoch";
                state.activeWorld = "Platformer_Main";
                state.entities = platformer_entities();
            }
            else if (projectId == "puzzle")
            {
                state.projectName = "PuzzleLab";
                state.projectPath = "Projects/PuzzleLab/worlds/puzzle.epoch";
                state.activeWorld = "Puzzle_Testbed";
                state.entities = puzzle_entities();
            }
            else
            {
                state.projectName = "Sandbox";
                state.projectPath = "Projects/Sandbox/scene.epoch";
                state.activeWorld = "PersistentLevel";
                state.entities = sandbox_entities();
            }

            state.selectedEntity = state.entities.empty() ? 0u : (std::min)(state.selectedEntity, state.entities.size() - 1u);
            if (writeLog)
                push_editor_log(state, std::string("[project] Loaded ") + state.projectName + ".");
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

        [[nodiscard]] std::string_view panel_name(CommandPanel panel) noexcept
        {
            switch (panel)
            {
            case CommandPanel::Projects: return "Projects";
            case CommandPanel::Games: return "Play";
            case CommandPanel::Scene: return "Scene";
            default: return "Projects";
            }
        }

        [[nodiscard]] std::string_view panel_hint(CommandPanel panel) noexcept
        {
            switch (panel)
            {
            case CommandPanel::Projects: return "Switch projects from the editor without opening a separate overlay.";
            case CommandPanel::Games: return "Launch playable scenes directly from the editor command row.";
            case CommandPanel::Scene: return "Quick scene tools for the fake viewport and editor helpers.";
            default: return "";
            }
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
                it->second.activePanel = CommandPanel::Projects;
                set_project(it->second, "sandbox", false);
                push_editor_log(it->second, "[info] Editor scene initialized.");
                push_editor_log(it->second, "[info] Command menu lives in the editor toolbar now.");
                push_editor_log(it->second, "[info] Scene viewport is owned by the active backend.");
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

    EditorFrameResult editor_run(const std::shared_ptr<core::Context>& ctx)
    {
        EditorFrameResult result{};
        if (!ctx)
            return result;

        auto& editor = editor_state_for(ctx);

        const float w = static_cast<float>(ctx->get_width_safe());
        const float h = static_cast<float>(ctx->get_height_safe());

        const float toolbar_h = 176.0f;
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

        const bool up_pressed = epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Up);
        const bool down_pressed = epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Down);
        const bool left_pressed = epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Left);
        const bool right_pressed = epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Right);
        const bool enter_pressed = epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Enter);
        const bool tab_pressed = epochnamespace::input::keyPressed.test(epochnamespace::input::Key::Tab);

        gui::begin_window("Epoch Editor", toolbar_pos, toolbar_size);
        const float toolbar_button_y = toolbar_pos.y + 38.0f;
        const float toolbar_button_h = 28.0f;
        float toolbar_x = toolbar_pos.x + 8.0f;

        auto panel_button = [&](std::string_view label, float width, CommandPanel panel)
        {
            std::string buttonLabel = editor.activePanel == panel
                ? std::string("> ") + std::string(label)
                : std::string(label);
            gui::set_cursor({ toolbar_x, toolbar_button_y });
            if (gui::button(buttonLabel, { width, toolbar_button_h }))
                editor.activePanel = panel;
            toolbar_x += width + 8.0f;
        };

        auto action_button = [&](std::string_view label, float width, EditorCommand command)
        {
            gui::set_cursor({ toolbar_x, toolbar_button_y });
            if (gui::button(label, { width, toolbar_button_h }))
            {
                emit_command(command);
                push_editor_log(editor, std::string("[cmd] ") + std::string(label));
            }
            toolbar_x += width + 8.0f;
        };

        panel_button("Projects", 126.0f, CommandPanel::Projects);
        panel_button("Play", 96.0f, CommandPanel::Games);
        panel_button("Scene", 104.0f, CommandPanel::Scene);
        action_button("Settings", 116.0f, EditorCommand::Settings);
        action_button("Exit", 88.0f, EditorCommand::Exit);

        gui::set_cursor({ toolbar_x + 16.0f, toolbar_button_y + 5.0f });
        const std::string projectLabel = std::string("Project: ") + editor.projectName + "  |  Renderer: " + renderer_name(ctx);
        gui::label(projectLabel);

        gui::set_cursor({ 14.0f, toolbar_pos.y + 84.0f });
        gui::label(std::string("Command Menu: ") + std::string(panel_name(editor.activePanel)));
        gui::set_cursor({ 14.0f, toolbar_pos.y + 104.0f });
        gui::label(std::string(panel_hint(editor.activePanel)));

        float chip_x = 14.0f;
        float chip_y = toolbar_pos.y + 128.0f;
        constexpr float chip_h = 28.0f;
        constexpr float chip_gap = 8.0f;

        auto draw_chip = [&](std::string_view label, float width, auto&& on_click)
        {
            if (chip_x + width > w - 14.0f)
            {
                chip_x = 14.0f;
                chip_y += chip_h + chip_gap;
            }

            gui::set_cursor({ chip_x, chip_y });
            if (gui::button(label, { width, chip_h }))
                on_click();
            chip_x += width + chip_gap;
        };

        static constexpr std::array projectChips = {
            CommandChip{ "Sandbox", "sandbox", 112.0f },
            CommandChip{ "Platformer", "platformer", 124.0f },
            CommandChip{ "Puzzle Lab", "puzzle", 122.0f }
        };

        static constexpr std::array gameChips = {
            CommandChip{ "Snake", "snake", 96.0f },
            CommandChip{ "Tetris", "tetris", 96.0f },
            CommandChip{ "Pacman", "pacman", 96.0f },
            CommandChip{ "Frogger", "frogger", 104.0f },
            CommandChip{ "Sokoban", "sokoban", 104.0f },
            CommandChip{ "Minesweeper", "minesweep", 128.0f },
            CommandChip{ "Sliding Puzzle", "puzzle", 132.0f },
            CommandChip{ "Bejeweled", "bejeweled", 120.0f },
            CommandChip{ "2048", "fourty", 88.0f },
            CommandChip{ "Sand Sim", "sandsim", 110.0f },
            CommandChip{ "Cellular", "cellular", 108.0f }
        };

        static constexpr std::array sceneChips = {
            CommandChip{ "Focus Selection", "focus_selection", 156.0f },
            CommandChip{ "Reset Camera", "reset_camera", 128.0f },
            CommandChip{ "Toggle Helpers", "toggle_helpers", 142.0f }
        };

        auto command_selection = [&]() -> std::size_t&
        {
            switch (editor.activePanel)
            {
            case CommandPanel::Projects: return editor.projectCommandSelection;
            case CommandPanel::Games: return editor.gameCommandSelection;
            case CommandPanel::Scene: return editor.sceneCommandSelection;
            default: return editor.projectCommandSelection;
            }
        };

        auto command_count = [&]() -> std::size_t
        {
            switch (editor.activePanel)
            {
            case CommandPanel::Projects: return projectChips.size();
            case CommandPanel::Games: return gameChips.size();
            case CommandPanel::Scene: return sceneChips.size();
            default: return 0u;
            }
        };

        auto cycle_panel = [&]()
        {
            switch (editor.activePanel)
            {
            case CommandPanel::Projects: editor.activePanel = CommandPanel::Games; break;
            case CommandPanel::Games: editor.activePanel = CommandPanel::Scene; break;
            case CommandPanel::Scene: editor.activePanel = CommandPanel::Projects; break;
            }
        };

        auto trigger_command_chip = [&](std::size_t index)
        {
            if (editor.activePanel == CommandPanel::Projects)
            {
                const auto& chip = projectChips[index % projectChips.size()];
                set_project(editor, chip.argument, true);
                emit_command(EditorCommand::OpenProject, chip.argument);
                return;
            }

            if (editor.activePanel == CommandPanel::Games)
            {
                const auto& chip = gameChips[index % gameChips.size()];
                push_editor_log(editor, std::string("[play] Launch request: ") + std::string(chip.label));
                emit_command(EditorCommand::RunGame, chip.argument);
                return;
            }

            const auto& chip = sceneChips[index % sceneChips.size()];
            handle_scene_tool(editor, chip.argument);
        };

        if (tab_pressed)
            cycle_panel();

        if (!editor.entities.empty())
        {
            if (up_pressed)
            {
                editor.selectedEntity = editor.selectedEntity == 0
                    ? editor.entities.size() - 1u
                    : editor.selectedEntity - 1u;
            }
            if (down_pressed)
            {
                editor.selectedEntity = (editor.selectedEntity + 1u) % editor.entities.size();
            }
        }

        const auto availableCommandCount = command_count();
        if (availableCommandCount > 0)
        {
            auto& selection = command_selection();
            selection = (std::min)(selection, availableCommandCount - 1u);
            if (left_pressed)
                selection = selection == 0 ? availableCommandCount - 1u : selection - 1u;
            if (right_pressed)
                selection = (selection + 1u) % availableCommandCount;
            if (enter_pressed)
                trigger_command_chip(selection);
        }

        if (editor.activePanel == CommandPanel::Projects)
        {
            for (std::size_t i = 0; i < projectChips.size(); ++i)
            {
                const auto& chip = projectChips[i];
                const std::string label = i == editor.projectCommandSelection
                    ? std::string("> ") + std::string(chip.label)
                    : std::string(chip.label);
                draw_chip(label, chip.width, [&]() {
                    editor.projectCommandSelection = i;
                    trigger_command_chip(i);
                });
            }
        }
        else if (editor.activePanel == CommandPanel::Games)
        {
            for (std::size_t i = 0; i < gameChips.size(); ++i)
            {
                const auto& chip = gameChips[i];
                const std::string label = i == editor.gameCommandSelection
                    ? std::string("> ") + std::string(chip.label)
                    : std::string(chip.label);
                draw_chip(label, chip.width, [&]() {
                    editor.gameCommandSelection = i;
                    trigger_command_chip(i);
                });
            }
        }
        else
        {
            for (std::size_t i = 0; i < sceneChips.size(); ++i)
            {
                const auto& chip = sceneChips[i];
                const std::string label = i == editor.sceneCommandSelection
                    ? std::string("> ") + std::string(chip.label)
                    : std::string(chip.label);
                draw_chip(label, chip.width, [&]() {
                    editor.sceneCommandSelection = i;
                    trigger_command_chip(i);
                });
            }
        }

        gui::end_window();

        gui::begin_window("World Outliner", outliner_pos, outliner_size);
        gui::label(std::string("Scene: ") + editor.activeWorld);
        gui::label(std::string("Project Root: ") + editor.projectPath);
        gui::label(std::string("Entities: ") + std::to_string(editor.entities.size()));
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

        gui::begin_window("Details", details_pos, details_size);
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
        gui::end_window();

        result.scene_viewport = gui::scene_viewport("Scene View", viewport_pos, viewport_size);
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

        gui::begin_window("Output Log", log_pos, log_size);
        gui::label(std::string("[info] Scene viewport: ")
            + std::to_string(static_cast<int>(result.scene_viewport.size.x))
            + "x"
            + std::to_string(static_cast<int>(result.scene_viewport.size.y)));
        gui::label(std::string("[info] Active renderer: ") + renderer_name(ctx));
        gui::label(std::string("[info] Active command menu: ") + std::string(panel_name(editor.activePanel)));
        for (const auto& line : editor.logLines)
            gui::label(line);
        gui::end_window();

        auto& chat = chat_state_for(ctx);
        chat.pump();

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

        return result;
    }

} // namespace epochnamespace


