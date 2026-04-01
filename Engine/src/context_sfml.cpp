#include <algorithm>
#include <atomic>
#include <memory>
#include <string>

#include <include/aengine.config.hpp>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <wingdi.h>
#endif

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
#define SFML_STATIC
#include <SFML/Graphics.hpp>
#endif

#include "core_context_backends.hpp"

import core.context;
import context.multiplexer;

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import aengine.gui;
import aengine.input;
import atlas.texture;
import context.commandqueue;
import image.loader;
import sfml.state;
import sfml.textures;

namespace
{
    std::unique_ptr<sf::RenderWindow> s_window{};
    int s_width = 0;
    int s_height = 0;

#if defined(_WIN32)
    HWND s_hostWindow = nullptr;
    HWND s_childWindow = nullptr;
    HDC s_hdc = nullptr;
    HGLRC s_glContext = nullptr;
#endif

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

    void apply_view_size() noexcept
    {
        if (!s_window)
            return;

        s_window->setView(sf::View(
            sf::FloatRect(
                0.0f,
                0.0f,
                static_cast<float>(s_width),
                static_cast<float>(s_height))));
    }

    void request_host_shutdown(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
        if (ctx && ctx->windowData)
            ctx->windowData->set_should_close(true);
#if defined(_WIN32)
        HWND closeTarget = s_childWindow;
        if (!closeTarget || ::IsWindow(closeTarget) == FALSE)
            closeTarget = s_hostWindow;
        if (closeTarget && ::IsWindow(closeTarget) != FALSE)
            ::PostMessageW(closeTarget, WM_CLOSE, 0, 0);
#endif
    }

    void refresh_dimensions(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) != FALSE)
        {
            RECT client{};
            if (::GetClientRect(s_childWindow, &client))
            {
                s_width = (std::max)(1, static_cast<int>(client.right - client.left));
                s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));
            }
        }
        else
#endif
        if (s_window)
        {
            const auto size = s_window->getSize();
            s_width = static_cast<int>((std::max)(1u, size.x));
            s_height = static_cast<int>((std::max)(1u, size.y));
        }

        if (ctx)
        {
            ctx->width = s_width;
            ctx->height = s_height;
            ctx->virtualWidth = s_width;
            ctx->virtualHeight = s_height;
            ctx->framebufferWidth = s_width;
            ctx->framebufferHeight = s_height;
            if (ctx->windowData)
            {
                ctx->windowData->sfml_window = s_window.get();
                ctx->windowData->width = s_width;
                ctx->windowData->height = s_height;
            }
        }

        auto& state = epochnamespace::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = s_window.get();
        state.set_dimensions(s_width, s_height);
        state.running = (s_window && s_window->isOpen());
    }

    void sync_docked_child_size(const std::shared_ptr<epochnamespace::core::Context>& ctx) noexcept
    {
#if defined(_WIN32)
        if (!s_hostWindow || !s_childWindow
            || ::IsWindow(s_hostWindow) == FALSE
            || ::IsWindow(s_childWindow) == FALSE)
            return;

        RECT client{};
        if (!::GetClientRect(s_hostWindow, &client))
            return;

        const int width = (std::max)(1, static_cast<int>(client.right - client.left));
        const int height = (std::max)(1, static_cast<int>(client.bottom - client.top));
        if (width == s_width && height == s_height)
            return;

        s_width = width;
        s_height = height;
        ::SetWindowPos(
            s_childWindow,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        apply_view_size();
        refresh_dimensions(ctx);

        if (ctx && ctx->onResize)
            ctx->onResize(width, height);
#else
        (void)ctx;
#endif
    }

    void sfml_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        s_width = (std::max)(1, ctx->width);
        s_height = (std::max)(1, ctx->height);

#if defined(_WIN32)
        s_hostWindow = ctx->get_hwnd();
#endif

        if (!s_window || !s_window->isOpen())
        {
            sf::ContextSettings settings{};
            settings.majorVersion = 2;
            settings.minorVersion = 1;
            settings.attributeFlags = sf::ContextSettings::Default;

            s_window = std::make_unique<sf::RenderWindow>(
                sf::VideoMode(static_cast<unsigned>(s_width), static_cast<unsigned>(s_height)),
                ctx->backendName.empty() ? "SFML" : ctx->backendName,
                sf::Style::Default,
                settings);

            if (!s_window || !s_window->isOpen())
                return;

            s_window->setVerticalSyncEnabled(true);
            s_window->setFramerateLimit(60);
            s_window->setKeyRepeatEnabled(false);
        }

        if (!s_window || !s_window->isOpen())
            return;

        apply_view_size();

#if defined(_WIN32)
        s_childWindow = static_cast<HWND>(s_window->getSystemHandle());
        if (!s_childWindow)
            return;

        if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE)
        {
            ::SetParent(s_childWindow, s_hostWindow);

            LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
            style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
            style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
            ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);

            epochnamespace::core::MakeDockable(s_childWindow, s_hostWindow);

            RECT client{};
            ::GetClientRect(s_hostWindow, &client);
            s_width = (std::max)(1, static_cast<int>(client.right - client.left));
            s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));

            ::SetWindowPos(
                s_childWindow,
                nullptr,
                0,
                0,
                s_width,
                s_height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            ::ShowWindow(s_hostWindow, SW_HIDE);
            apply_view_size();
        }

        const HWND liveWindow = s_childWindow;

        if (!s_window->setActive(true))
            return;

        s_hdc = liveWindow ? ::GetDC(liveWindow) : nullptr;
        s_glContext = ::wglGetCurrentContext();
        (void)s_window->setActive(false);

        if (ctx)
        {
            ctx->hwnd = liveWindow;
            ctx->hdc = s_hdc;
            ctx->hglrc = s_glContext;
            ctx->native_window = liveWindow;
            ctx->native_drawable = s_hdc;
            ctx->native_gl_context = s_glContext;
        }

        if (ctx && ctx->windowData)
        {
            ctx->windowData->hwnd = liveWindow;
            ctx->windowData->hdc = s_hdc;
            ctx->windowData->host_hwnd = s_hostWindow;
            ctx->windowData->hwndChild = s_childWindow;
            ctx->windowData->sfml_window = s_window.get();
            ctx->windowData->set_size(s_width, s_height);
        }

        if (liveWindow && ::IsWindow(liveWindow) != FALSE)
        {
            ::SetFocus(liveWindow);
            s_window->requestFocus();
        }
#else
        if (ctx)
            ctx->native_window = reinterpret_cast<void*>(s_window->getSystemHandle());

        if (ctx && ctx->windowData)
            ctx->windowData->sfml_window = s_window.get();
#endif

        refresh_dimensions(ctx);
    }

    void sfml_cleanup_adapter()
    {
#if defined(_WIN32)
        if (s_window)
            (void)s_window->setActive(false);
#endif

        if (s_window)
            s_window->close();

        s_window.reset();

        auto& state = epochnamespace::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = nullptr;
        state.running = false;

        s_width = 0;
        s_height = 0;
#if defined(_WIN32)
        s_hostWindow = nullptr;
        s_childWindow = nullptr;
        s_hdc = nullptr;
        s_glContext = nullptr;
#endif
    }

    bool sfml_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx || !s_window || !s_window->isOpen())
            return false;

#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) == FALSE)
            return false;
#endif

        if (!s_window->setActive(true))
            return false;

        sf::Event event{};
        while (s_window->pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                request_host_shutdown(ctx);
                (void)s_window->setActive(false);
                return false;
            }

            if (event.type == sf::Event::Resized)
            {
                refresh_dimensions(ctx);
                apply_view_size();
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }

        sync_docked_child_size(ctx);
        refresh_dimensions(ctx);
        s_window->resetGLStates();
        s_window->clear(sf::Color(0, 0, 0));
        s_window->resetGLStates();
        (void)queue.drain();
        s_window->resetGLStates();
        (void)epochnamespace::gui::render_deferred_batch(ctx.get());
        s_window->display();
        (void)s_window->setActive(false);
        return s_window->isOpen();
    }
}

namespace epochnamespace::core::detail
{
    void register_sfml_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::SFML;
        ctx->backendName = "SFML";
        ctx->initialize = sfml_initialize_adapter;
        ctx->cleanup = sfml_cleanup_adapter;
        ctx->process = sfml_process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return s_width; };
        ctx->get_height = []() { return s_height; };
        ctx->draw_sprite = epochnamespace::sfmlcontext::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochnamespace::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::SFML, std::move(ctx));
    }
}
#endif
