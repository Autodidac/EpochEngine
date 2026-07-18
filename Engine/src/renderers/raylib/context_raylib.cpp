#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include <include/engine.config.hpp>

import engine.gui;
import engine.input;
import atlas.texture;
import context.commandqueue;
import context.type;
import core.context;
import image.loader;
import raylib.api;
import raylib.context;
import raylib.renderer;
import raylib.state;
import raylib.textures;

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
        try
        {
            const std::uint32_t handle = epochnamespace::raylibtextures::load_atlas(atlas);
            if (handle != 0u)
                return handle;
        }
        catch (...)
        {
        }

        return 0u;
    }

    int default_add_model(const char*, const char* path) noexcept
    {
        return epochnamespace::raylib_api::load_model(path);
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

            current->init_failed = !epochnamespace::raylibcontext::raylib_initialize(
                current,
                current->get_hwnd(),
                static_cast<unsigned>((std::max)(1, current->width)),
                static_cast<unsigned>((std::max)(1, current->height)),
                current->windowData
                    ? current->windowData->onResize
                    : std::function<void(int, int)>{},
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

            if (current->windowData && current->windowData->get_should_close())
            {
                queue.clear();
                return false;
            }

            const bool frameReady = epochnamespace::raylibcontext::raylib_process();
            if (!frameReady)
                return epochnamespace::raylibcontext::raylib_is_running();

            if (!epochnamespace::raylibcontext::raylib_is_running())
                return false;

            auto& st = epochnamespace::raylibstate::s_raylibstate;
            st.running = true;
            st.owner_ctx = current.get();
            st.frameActive = false;
            st.frameInTextureMode = false;

            const int frameWidth = (std::max)(1, epochnamespace::raylib_api::get_render_width());
            const int frameHeight = (std::max)(1, epochnamespace::raylib_api::get_render_height());
            current->framebufferWidth = frameWidth;
            current->framebufferHeight = frameHeight;

            epochnamespace::raylib_api::begin_drawing();
            st.frameActive = true;
            st.frameInTextureMode = false;

            const auto clearColor = clear_color_for_context(ContextType::RayLib);
            epochnamespace::raylib_api::clear_background({
                static_cast<std::uint8_t>((std::clamp)(clearColor[0], 0.0f, 1.0f) * 255.0f),
                static_cast<std::uint8_t>((std::clamp)(clearColor[1], 0.0f, 1.0f) * 255.0f),
                static_cast<std::uint8_t>((std::clamp)(clearColor[2], 0.0f, 1.0f) * 255.0f),
                static_cast<std::uint8_t>((std::clamp)(clearColor[3], 0.0f, 1.0f) * 255.0f)
            });
            (void)queue.drain();
            epochnamespace::raylibcontext::raylib_render_scene_preview(current);
            (void)epochnamespace::gui::render_deferred_batch(current.get());
            (void)epochnamespace::gui::render_top_layer_batch(current.get());
            epochnamespace::raylib_api::end_drawing();

            st.frameActive = false;
            if (current->windowData)
                current->windowData->firstPresentComplete.store(
                    true,
                    std::memory_order_release);
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
        ctx->add_model = &default_add_model;
        ctx->onResize = &epochnamespace::raylibcontext::raylib_resize;
        bind_default_input(ctx);
        AddContextForBackend(ContextType::RayLib, std::move(ctx));
    }
}
#endif
