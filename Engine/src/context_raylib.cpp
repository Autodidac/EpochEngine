#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

#include <include/aengine.config.hpp>
#include "core_context_backends.hpp"

import core.context;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import aengine.gui;
import aengine.input;
import atlas.texture;
import context.commandqueue;
import image.loader;
import raylib.api;
import raylib.renderer;
import raylib.state;

namespace
{
    std::uint32_t default_add_texture(
        epochnamespace::TextureAtlas&,
        std::string,
        const epochnamespace::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(const epochnamespace::TextureAtlas& atlas) noexcept
    {
        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    void bind_default_input(const std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochnamespace::input::Key key) { return epochnamespace::input::is_key_held(key); };
        ctx->is_key_down = [](epochnamespace::input::Key key) { return epochnamespace::input::is_key_down(key); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochnamespace::input::mouseX.load(std::memory_order_relaxed);
            y = epochnamespace::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochnamespace::input::MouseButton button) { return epochnamespace::input::is_mouse_button_held(button); };
        ctx->is_mouse_button_down = [](epochnamespace::input::MouseButton button) { return epochnamespace::input::is_mouse_button_down(button); };
    }

    void raylib_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        const int width = (std::max)(1, ctx->width);
        const int height = (std::max)(1, ctx->height);
        auto& st = epochnamespace::raylibstate::s_raylibstate;

        if (!epochnamespace::raylib_api::is_window_ready())
        {
            epochnamespace::raylib_api::set_config_flags(
                epochnamespace::raylib_api::flag_msaa_4x_hint
                | epochnamespace::raylib_api::flag_vsync_hint);
            epochnamespace::raylib_api::set_trace_log_level(epochnamespace::raylib_api::log_warning);
            epochnamespace::raylib_api::init_window(width, height, ctx->backendName.c_str());
            epochnamespace::raylib_api::set_target_fps(60);
        }
        else
        {
            epochnamespace::raylib_api::set_window_title(ctx->backendName.c_str());
            epochnamespace::raylib_api::set_window_size(width, height);
        }

        st.running = epochnamespace::raylib_api::is_window_ready();
        st.owner_ctx = ctx.get();
        st.width = static_cast<unsigned>(width);
        st.height = static_cast<unsigned>(height);
        st.frameActive = false;
        st.frameInTextureMode = false;
    }

    void raylib_cleanup_adapter()
    {
        auto& st = epochnamespace::raylibstate::s_raylibstate;
        st.frameActive = false;
        st.frameInTextureMode = false;
        st.owner_ctx = nullptr;
        st.running = false;

        if (epochnamespace::raylib_api::is_window_ready())
            epochnamespace::raylib_api::close_window();
    }

    bool raylib_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx || !epochnamespace::raylib_api::is_window_ready())
            return false;

        if (epochnamespace::raylib_api::window_should_close())
            return false;

        auto& st = epochnamespace::raylibstate::s_raylibstate;
        st.running = true;
        st.owner_ctx = ctx.get();
        st.width = static_cast<unsigned>((std::max)(1, epochnamespace::raylib_api::get_render_width()));
        st.height = static_cast<unsigned>((std::max)(1, epochnamespace::raylib_api::get_render_height()));
        st.frameActive = true;
        st.frameInTextureMode = false;

        epochnamespace::raylib_api::begin_drawing();
        epochnamespace::raylib_api::clear_background({ 0, 0, 0, 255 });
        (void)queue.drain();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        epochnamespace::raylib_api::end_drawing();

        st.frameActive = false;
        return !epochnamespace::raylib_api::window_should_close();
    }
}

namespace epochnamespace::core::detail
{
    extern "C" void epoch_register_raylib_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::RayLib;
        ctx->backendName = "RayLib";
        ctx->initialize = raylib_initialize_adapter;
        ctx->cleanup = raylib_cleanup_adapter;
        ctx->process = raylib_process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return epochnamespace::raylib_api::get_render_width(); };
        ctx->get_height = []() { return epochnamespace::raylib_api::get_render_height(); };
        ctx->draw_sprite = epochnamespace::raylibrenderer::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochnamespace::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::RayLib, std::move(ctx));
    }
}
#endif
