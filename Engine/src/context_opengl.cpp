module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   ifdef EPOCH_USING_WINMAIN
#       include "../include/aframework.hpp"
#   endif
#endif

#ifdef min
#   undef min
#endif
#ifdef max
#   undef max
#endif

module core.context;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import aengine.input;
import atlas.manager;
import core.logger;
import opengl.context;
import opengl.textures;

namespace
{
    constexpr std::string_view kLogOpenGL = "Context.OpenGL";

    void* ctx_native_window_handle(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
        if (!ctx)
            return nullptr;
        if (auto h = ctx->get_hwnd())
            return h;
        if (ctx->windowData && ctx->windowData->hwnd)
            return ctx->windowData->hwnd;
        return nullptr;
    }

    std::uint32_t default_add_texture(epochnamespace::TextureAtlas&, std::string, const epochnamespace::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(
        const epochnamespace::TextureAtlas& atlas,
        const epochnamespace::core::ContextType type) noexcept
    {
        try
        {
            epochnamespace::atlasmanager::ensure_uploaded(atlas);
            epochnamespace::atlasmanager::process_pending_uploads(type);
        }
        catch (...)
        {
        }

        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    void bind_default_input(const std::shared_ptr<epochnamespace::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochnamespace::input::Key k) { return epochnamespace::input::is_key_held(k); };
        ctx->is_key_down = [](epochnamespace::input::Key k) { return epochnamespace::input::is_key_down(k); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochnamespace::input::mouseX.load(std::memory_order_relaxed);
            y = epochnamespace::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochnamespace::input::MouseButton b) { return epochnamespace::input::is_mouse_button_held(b); };
        ctx->is_mouse_button_down = [](epochnamespace::input::MouseButton b) { return epochnamespace::input::is_mouse_button_down(b); };
    }

    void opengl_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        const auto native = ctx_native_window_handle(ctx);
        const unsigned w = static_cast<unsigned>((std::max)(1, ctx->width));
        const unsigned h = static_cast<unsigned>((std::max)(1, ctx->height));

        ctx->init_failed = false;
        try
        {
            (void)epochnamespace::openglcontext::opengl_initialize(ctx, native, w, h, ctx->onResize);
        }
        catch (const std::exception& e)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogOpenGL).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogOpenGL).log(
                epochnamespace::logger::LogLevel::Error,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void opengl_cleanup_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            epochnamespace::openglcontext::opengl_cleanup(ctx);
        }
        catch (const std::exception& e)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogOpenGL).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogOpenGL).log(
                epochnamespace::logger::LogLevel::Error,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool opengl_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx)
            return false;
        return epochnamespace::openglcontext::opengl_process(ctx, queue);
    }
}

namespace epochnamespace::core::detail
{
    void register_opengl_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::OpenGL;
        ctx->backendName = "OpenGL";
        ctx->initialize = opengl_initialize_adapter;
        ctx->cleanup = opengl_cleanup_adapter;
        ctx->process = opengl_process_adapter;
        ctx->clear = epochnamespace::openglcontext::opengl_clear;
        ctx->present = nullptr;
        ctx->get_width = epochnamespace::openglcontext::opengl_get_width;
        ctx->get_height = epochnamespace::openglcontext::opengl_get_height;
        ctx->draw_sprite = epochnamespace::opengltextures::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochnamespace::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas, ContextType::OpenGL);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::OpenGL, std::move(ctx));
    }
}
#endif
