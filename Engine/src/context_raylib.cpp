#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

#include <include/aengine.config.hpp>

import aengine.gui;
import aengine.input;
import atlas.texture;
import core.context;
import image.loader;
import raylib.api;
import raylib.context;
import raylib.renderer;
import raylib.state;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
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
}

namespace epochnamespace::core::detail
{
    extern "C" void epoch_register_raylib_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::RayLib;
        ctx->backendName = "RayLib";
        ctx->initialize = []()
        {
            auto current = epochnamespace::core::get_current_render_context();
            if (!current)
                return;

            (void)epochnamespace::raylibcontext::raylib_initialize(
                current,
                current->get_hwnd(),
                static_cast<unsigned>((std::max)(1, current->width)),
                static_cast<unsigned>((std::max)(1, current->height)),
                current->onResize,
                current->backendName);
        };
        ctx->cleanup = []()
        {
            auto current = epochnamespace::core::get_current_render_context();
            if (!current)
                return;

            epochnamespace::raylibcontext::raylib_cleanup(current);
        };
        ctx->process = [](std::shared_ptr<Context> current, CommandQueue& queue)
        {
            if (!current)
                return false;

            epochnamespace::raylibcontext::raylib_process();
            if (!epochnamespace::raylibcontext::raylib_is_running())
                return false;

            auto& st = epochnamespace::raylibstate::s_raylibstate;
            st.running = true;
            st.owner_ctx = current.get();
            st.width = static_cast<unsigned>((std::max)(1, epochnamespace::raylib_api::get_render_width()));
            st.height = static_cast<unsigned>((std::max)(1, epochnamespace::raylib_api::get_render_height()));
            st.frameActive = false;
            st.frameInTextureMode = false;

            epochnamespace::raylib_api::begin_drawing();
            epochnamespace::raylib_api::clear_background({ 0, 0, 0, 255 });
            epochnamespace::raylibcontext::raylib_render_scene_preview(current);
            (void)queue.drain();
            (void)epochnamespace::gui::render_deferred_batch(current.get());
            epochnamespace::raylib_api::end_drawing();

            st.frameActive = false;
            return !epochnamespace::raylib_api::window_should_close();
        };
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
