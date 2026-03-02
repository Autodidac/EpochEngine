/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 **************************************************************/
 /**************************************************************
  *   AlmondShell - Modular C++ Framework
  *   Editor Implementation
  *
  *   SPDX-License-Identifier: LicenseRef-MIT-NoSell
  **************************************************************/
module;

#include <algorithm>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

module aeditor; // implements the interface unit

import aengine.gui;
import aengine.core.context;
import epoch.ai;

namespace almondnamespace
{
    namespace
    {
        using namespace std::chrono_literals;

        [[nodiscard]] static bool is_ws_only(std::string_view s) noexcept
        {
            for (unsigned char c : s)
                if (c > ' ') return false;
            return true;
        }

        struct AiChat
        {
            std::vector<std::string> lines{};
            std::string input{};

            // One request in flight.
            std::optional<std::future<std::string>> pending{};

            AiChat()
            {
                lines.emplace_back("bot> Ready. Endpoint: http://localhost:1234");
            }

            // std::future is move-only => make this move-only explicitly.
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
                }
                catch (const std::exception& e)
                {
                    lines.emplace_back(std::string("bot> (error) ") + e.what());
                }

                pending.reset();
            }

            void submit(std::string text)
            {
                if (text.empty() || is_ws_only(text)) return;

                if (pending)
                {
                    lines.emplace_back("bot> (busy)");
                    return;
                }

                lines.emplace_back("you> " + text);

                pending.emplace(std::async(std::launch::async, [t = std::move(text)]() mutable {
                    return epoch::ai::send_to_bot(t);
                }));
            }
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

        ChatStorage& chat_storage()
        {
            static ChatStorage storage;
            return storage;
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

    } // namespace

    void cleanup_chat_context(const core::Context* ctx)
    {
        if (!ctx) return;

        auto& storage = chat_storage();
        std::scoped_lock lock(storage.mutex);
        storage.chats.erase(ctx);
    }

    void shutdown_chat_system()
    {
        auto& storage = chat_storage();
        std::scoped_lock lock(storage.mutex);
        storage.chats.clear();

        if (storage.bot_initialized)
        {
            epoch::ai::shutdown_bot();
            storage.bot_initialized = false;
        }
    }

    bool editor_run(const std::shared_ptr<core::Context>& ctx, gui::WidgetBounds* out_bounds)
    {
        // Unreal-ish docked layout: Top toolbar, center viewport, left outliner, right details,
        // bottom log + AI chat.

        const float w = ctx ? static_cast<float>(ctx->get_width_safe()) : 0.0f;
        const float h = ctx ? static_cast<float>(ctx->get_height_safe()) : 0.0f;

        const float toolbar_h = 36.0f;
        const float bottom_h  = (std::max)(220.0f, h * 0.25f);
        const float left_w    = (std::max)(240.0f, w * 0.18f);
        const float right_w   = (std::max)(300.0f, w * 0.22f);

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

        if (out_bounds)
        {
            out_bounds->position = toolbar_pos;
            out_bounds->size = { w, h };
        }

        // Toolbar
        gui::begin_window("Epoch", toolbar_pos, toolbar_size);
        gui::label("File   Edit   Window   Help");
        gui::label(" ");
        gui::end_window();

        // World Outliner (stub)
        gui::begin_window("World Outliner", outliner_pos, outliner_size);
        gui::label("(stub) Scene Hierarchy");
        gui::label("- PersistentLevel");
        gui::label("  - Camera");
        gui::label("  - Light");
        gui::label("  - EditorOnly_Gizmo");
        gui::end_window();

        // Details (stub)
        gui::begin_window("Details", details_pos, details_size);
        gui::label("(stub) Selected: EditorOnly_Gizmo");
        gui::label("Transform");
        gui::label("  Location: (0,0,0)");
        gui::label("  Rotation: (0,0,0)");
        gui::label("  Scale:    (1,1,1)");
        gui::label("Rendering");
        gui::label("  Visible: true");
        gui::end_window();

        // Viewport panel
        gui::begin_window("Viewport", viewport_pos, viewport_size);
        gui::label("(placeholder) Render viewport. GUI must render on OpenGL + Vulkan here.");
        gui::label("Tip: close the window to stop the engine.");
        gui::end_window();

        // Bottom: split into Output Log + AI Chat
        const float split = 0.55f;
        const float left_bottom_w = w * split;
        const gui::Vec2 log_pos{ 0.0f, bottom_pos.y };
        const gui::Vec2 log_size{ left_bottom_w, bottom_h };
        const gui::Vec2 chat_pos{ left_bottom_w, bottom_pos.y };
        const gui::Vec2 chat_size{ (std::max)(0.0f, w - left_bottom_w), bottom_h };

        gui::begin_window("Output Log", log_pos, log_size);
        gui::label("[info] Editor loop running.");
        gui::label("[info] Backends: Raylib / SFML / SDL / Vulkan / OpenGL / Software");
        gui::label("[info] If Vulkan is alive, the clear color will pulse.");
        gui::end_window();

        auto& chat = chat_state_for(ctx);
        chat.pump();

        gui::ConsoleWindowOptions opts{
            .title = "AI Chat",
            .position = chat_pos,
            .size = chat_size,
            .lines = chat.lines,
            .max_visible_lines = 200,
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

        return false;
    }


} // namespace almondnamespace
