// acontext.sfml.context.ixx
module;

// -----------------------------------------------------------------------------
// Global module fragment: macros + C headers MUST live here.
// -----------------------------------------------------------------------------
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
#  ifdef min
#    undef min
#  endif
#  ifdef max
#    undef max
#  endif
#endif

#define SFML_STATIC
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowStyle.hpp>
#include <SFML/Graphics.hpp>
#include <glad/glad.h>

export module acontext.sfml.context;

import aengine.core.context;
import aengine.context.window;
import aengine.context.commandqueue;
import aengine.context.multiplexer;
import aatlas.manager;
import acontext.sfml.state;
import acontext.sfml.textures;
import aengine.diagnostics;
import aengine.core.logger;
import aengine.telemetry;

import <algorithm>;
import <cstdint>;
import <functional>;
import <iostream>;
import <memory>;
import <stdexcept>;
import <string>;
import <utility>;

export namespace almondnamespace::sfmlcontext
{
#if defined(ALMOND_USING_SFML)

    // SFML NOTE:
    // - SFML's default RenderTarget path uses legacy/fixed-function OpenGL calls.
    // - Requesting a Core profile context will cause GL_INVALID_OPERATION spam.
    // - Let SFML own context activation (setActive). Do NOT wglMakeCurrent manually.

    struct SFMLState
    {
        std::unique_ptr<sf::RenderWindow> window{};

#if defined(_WIN32)
        HWND  parent = nullptr;
        HWND  hwnd = nullptr;
        HDC   hdc = nullptr;   // informational / optional
        HGLRC glContext = nullptr;   // informational / optional
#endif

        unsigned int width = 400;
        unsigned int height = 300;
        bool running = false;
        std::function<void(int, int)> onResize{};
    };

    inline SFMLState sfmlcontext{};

    inline void refresh_dimensions(const std::shared_ptr<core::Context>& ctx) noexcept
    {
        if (!ctx) return;

        ctx->width = static_cast<int>((std::max)(1u, sfmlcontext.width));
        ctx->height = static_cast<int>((std::max)(1u, sfmlcontext.height));

        ctx->virtualWidth = ctx->width;
        ctx->virtualHeight = ctx->height;

        ctx->framebufferWidth = ctx->width;
        ctx->framebufferHeight = ctx->height;
    }

    inline bool sfml_initialize(
        std::shared_ptr<core::Context> ctx,
#if defined(_WIN32)
        HWND parentWnd = nullptr,
#else
        void* parentWnd = nullptr,
#endif
        unsigned int w = 400,
        unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr,
        std::string windowTitle = {})
    {
        const unsigned int clampedWidth = (std::max)(1u, w);
        const unsigned int clampedHeight = (std::max)(1u, h);

        sfmlcontext.width = clampedWidth;
        sfmlcontext.height = clampedHeight;

#if defined(_WIN32)
        const bool attachToHostWindow =
            parentWnd && ctx && ctx->windowData && ctx->windowData->hwnd == parentWnd;
        sfmlcontext.parent = attachToHostWindow ? nullptr : parentWnd;
#else
        (void)parentWnd;
        const bool attachToHostWindow = false;
#endif

        std::weak_ptr<core::Context> weakCtx = ctx;
        auto userResize = std::move(onResize);

        sfmlcontext.onResize =
            [weakCtx, userResize = std::move(userResize)](int width, int height) mutable
            {
                sfmlcontext.width = static_cast<unsigned int>((std::max)(1, width));
                sfmlcontext.height = static_cast<unsigned int>((std::max)(1, height));

                if (sfmlcontext.window)
                    sfmlcontext.window->setView(sf::View(
                        sf::FloatRect(
                            0.0f,
                            0.0f,
                            static_cast<float>(sfmlcontext.width),
                            static_cast<float>(sfmlcontext.height))));

                auto locked = weakCtx.lock();
                refresh_dimensions(locked);

                state::s_sfmlstate.set_dimensions(
                    static_cast<int>(sfmlcontext.width),
                    static_cast<int>(sfmlcontext.height));

                if (userResize)
                    userResize(static_cast<int>(sfmlcontext.width), static_cast<int>(sfmlcontext.height));
            };

        if (ctx)
            ctx->onResize = sfmlcontext.onResize;

        // IMPORTANT: request a compatibility-ish context.
        // Using 2.1 is the safest choice for SFML's default RenderTarget path.
        sf::ContextSettings settings{};
        settings.majorVersion = 2;
        settings.minorVersion = 1;
        settings.attributeFlags = sf::ContextSettings::Default;

        if (windowTitle.empty() && ctx && ctx->windowData && !ctx->windowData->titleNarrow.empty())
            windowTitle = ctx->windowData->titleNarrow;
        if (windowTitle.empty())
            windowTitle = "SFML Window";

        if (attachToHostWindow)
        {
            sfmlcontext.window = std::make_unique<sf::RenderWindow>(
                static_cast<sf::WindowHandle>(parentWnd), settings);
        }
        else
        {
            sf::VideoMode mode(sfmlcontext.width, sfmlcontext.height, 32u);
            sfmlcontext.window = std::make_unique<sf::RenderWindow>(
                mode, windowTitle, sf::Style::Default, settings);
        }

        if (!sfmlcontext.window || !sfmlcontext.window->isOpen())
        {
            std::cerr << "[SFML] Failed to create SFML window\n";
            return false;
        }

        sfmlcontext.window->setVerticalSyncEnabled(true);
        if (sfmlcontext.parent)
            sfmlcontext.window->setFramerateLimit(60);

        auto* windowPtr = sfmlcontext.window.get();

        if (ctx && ctx->windowData)
        {
            ctx->windowData->sfml_window = windowPtr;
            ctx->windowData->set_size(
                static_cast<int>(sfmlcontext.width),
                static_cast<int>(sfmlcontext.height));
        }

        state::s_sfmlstate.window.sfml_window = windowPtr;

#if defined(_WIN32)
        sfmlcontext.hwnd = static_cast<HWND>(sfmlcontext.window->getSystemHandle());
        sfmlcontext.hdc = GetDC(sfmlcontext.hwnd);

#if !defined(ALMOND_MAIN_HEADLESS)
        if (ctx)
            ctx->hwnd = sfmlcontext.hwnd;

        if (ctx && ctx->windowData)
        {
            ctx->windowData->hwnd = sfmlcontext.hwnd;
            ctx->windowData->set_size(
                static_cast<int>(sfmlcontext.width),
                static_cast<int>(sfmlcontext.height));
        }
#endif

        // Ensure the SFML context is current *on this thread* before capturing HGLRC.
        if (!sfmlcontext.window->setActive(true))
        {
            std::cerr << "[SFML] Failed to activate SFML window for context capture\n";
            return false;
        }

        sfmlcontext.glContext = wglGetCurrentContext();
        if (!sfmlcontext.glContext)
        {
            std::cerr << "[SFML] Failed to get OpenGL context\n";
            sfmlcontext.window->setActive(false);
            return false;
        }

        // Detach for now; render thread will reactivate per-frame.
        sfmlcontext.window->setActive(false);

        if (sfmlcontext.parent)
        {
            SetParent(sfmlcontext.hwnd, sfmlcontext.parent);

            LONG_PTR style = GetWindowLongPtr(sfmlcontext.hwnd, GWL_STYLE);
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= WS_CHILD | WS_VISIBLE;
            SetWindowLongPtr(sfmlcontext.hwnd, GWL_STYLE, style);

            almondnamespace::core::MakeDockable(sfmlcontext.hwnd, sfmlcontext.parent);

            RECT client{};
            GetClientRect(sfmlcontext.parent, &client);
            const int width = static_cast<int>((std::max)(static_cast<LONG>(1), client.right - client.left));
            const int height = static_cast<int>((std::max)(static_cast<LONG>(1), client.bottom - client.top));

            sfmlcontext.width = static_cast<unsigned int>(width);
            sfmlcontext.height = static_cast<unsigned int>(height);

            SetWindowPos(
                sfmlcontext.hwnd, nullptr, 0, 0, width, height,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

            if (sfmlcontext.onResize)
                sfmlcontext.onResize(width, height);

#if !defined(ALMOND_MAIN_HEADLESS)
            if (ctx && ctx->windowData)
                ctx->windowData->set_size(width, height);
#endif
        }
#endif

        refresh_dimensions(ctx);

        state::s_sfmlstate.set_dimensions(
            static_cast<int>(sfmlcontext.width),
            static_cast<int>(sfmlcontext.height));

        state::s_sfmlstate.running = true;
        sfmlcontext.running = true;

        atlasmanager::register_backend_uploader(
            core::ContextType::SFML,
            [](const TextureAtlas& atlas)
            {
                // IMPORTANT: uploader must assume the SFML context is current in sfml_process.
                sfmlcontext::ensure_uploaded(atlas);
            });

        return true;
    }

    inline bool sfml_should_close()
    {
        return !sfmlcontext.window || !sfmlcontext.window->isOpen();
    }

    inline bool sfml_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue)
    {
        if (!sfmlcontext.running || !sfmlcontext.window || !sfmlcontext.window->isOpen())
            return false;

        const core::ContextType backendType = ctx ? ctx->type : core::ContextType::SFML;

        std::uintptr_t windowId = 0u;
#if defined(_WIN32)
        if (ctx && ctx->windowData)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd);
        else
            windowId = reinterpret_cast<std::uintptr_t>(sfmlcontext.hwnd);
#else
        if (ctx && ctx->windowData)
            windowId = reinterpret_cast<std::uintptr_t>(ctx->windowData->hwnd);
        else
            windowId = 0u;
#endif

        almond::diagnostics::FrameTiming frameTimer{ backendType, windowId, "SFML" };

        // If the HWND is already dead (e.g., external teardown), bail before any GL calls.
#if defined(_WIN32)
        if (sfmlcontext.hwnd && ::IsWindow(sfmlcontext.hwnd) == FALSE)
        {
            sfmlcontext.running = false;
            state::s_sfmlstate.running = false;
            return false;
        }
#endif

        // Let SFML own activation. Do NOT call wglMakeCurrent manually.
        if (!sfmlcontext.window->setActive(true))
        {
            std::cerr << "[SFMLRender] Failed to activate SFML window\n";
            sfmlcontext.running = false;
            state::s_sfmlstate.running = false;
            return false;
        }

        const auto renderFlags = queue.render_flags_snapshot();
        const bool hasSfmlDraws =
            (renderFlags & static_cast<std::uint8_t>(core::RenderPath::SFML)) != 0u;
        const bool hasOpenGLDraws =
            (renderFlags & static_cast<std::uint8_t>(core::RenderPath::OpenGL)) != 0u;
        const bool hasVulkanDraws =
            (renderFlags & static_cast<std::uint8_t>(core::RenderPath::Vulkan)) != 0u;
        const bool hasQueuedCommands = queue.depth() > 0;
        const bool useOpenGLPath =
            hasOpenGLDraws || (hasQueuedCommands && !hasSfmlDraws && !hasVulkanDraws);
        const bool shouldResetSfmlState = hasSfmlDraws;

#if !defined(NDEBUG)
        almondnamespace::logger::info(
            "SFML",
            useOpenGLPath ? "Frame render path: OpenGL" : "Frame render path: SFML");
#endif

        if (shouldResetSfmlState)
        {
            // Reset before doing any SFML draw calls.
            sfmlcontext.window->resetGLStates();
        }

        // If uploads use OpenGL, they must run while the SFML context is current.
        atlasmanager::process_pending_uploads(core::ContextType::SFML);

        // Reset again in case uploads touched state.
        if (shouldResetSfmlState)
            sfmlcontext.window->resetGLStates();

        sf::Event event{};
        while (sfmlcontext.window->pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                sfmlcontext.window->close();
                sfmlcontext.running = false;
                state::s_sfmlstate.mark_should_close(true);
            }
            else if (event.type == sf::Event::Resized)
            {
                const int w = static_cast<int>((std::max)(1u, event.size.width));
                const int h = static_cast<int>((std::max)(1u, event.size.height));
                if (sfmlcontext.onResize) sfmlcontext.onResize(w, h);
            }
        }

        if (!sfmlcontext.running || !sfmlcontext.window->isOpen())
        {
            (void)sfmlcontext.window->setActive(false);
            return false;
        }

        refresh_dimensions(ctx);

        const int framebufferWidth = ctx
            ? ctx->framebufferWidth
            : static_cast<int>((std::max)(1u, sfmlcontext.width));
        const int framebufferHeight = ctx
            ? ctx->framebufferHeight
            : static_cast<int>((std::max)(1u, sfmlcontext.height));

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(framebufferWidth),
            telemetry::RendererTelemetryTags{ backendType, windowId, "width" });

        telemetry::emit_gauge(
            "renderer.framebuffer.size",
            static_cast<std::int64_t>(framebufferHeight),
            telemetry::RendererTelemetryTags{ backendType, windowId, "height" });

        const auto clearColor = core::clear_color_for_context(core::ContextType::SFML);
        const auto r = static_cast<sf::Uint8>(clearColor[0] * 255.0f);
        const auto g = static_cast<sf::Uint8>(clearColor[1] * 255.0f);
        const auto b = static_cast<sf::Uint8>(clearColor[2] * 255.0f);

        if (useOpenGLPath)
        {
            glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else
        {
            sfmlcontext.window->clear(sf::Color(r, g, b));
        }

        queue.drain();

        sfmlcontext.window->display();

        frameTimer.finish();

        (void)sfmlcontext.window->setActive(false);
        return sfmlcontext.running;
    }

    inline void sfml_cleanup(std::shared_ptr<almondnamespace::core::Context>& ctx)
    {
        // Stop new uploads immediately.
        atlasmanager::unregister_backend_uploader(core::ContextType::SFML);

        if (ctx && ctx->windowData)
            ctx->windowData->sfml_window = nullptr;

        state::s_sfmlstate.window.sfml_window = nullptr;
        state::s_sfmlstate.running = false;
        sfmlcontext.running = false;

        // CRITICAL:
        // clear_gpu_atlases() calls glDeleteTextures. That MUST only happen with an active,
        // valid SFML context. If the window is already closed/destroyed, skip deletion.
        if (sfmlcontext.window && sfmlcontext.window->isOpen())
        {
            if (sfmlcontext.window->setActive(true))
            {
                clear_gpu_atlases();
                sfmlcontext.window->setActive(false);
            }
            else
            {
                std::cerr << "[SFML] WARNING: could not activate context during cleanup; skipping GPU atlas delete\n";
            }

            // Close after deleting textures (while context is still valid).
            sfmlcontext.window->setActive(true);
            sfmlcontext.window->close();
            sfmlcontext.window.reset();
        }
        else
        {
            // Window already gone -> don't touch GL.
            sfmlcontext.window.reset();
        }

#if defined(_WIN32)
        if (sfmlcontext.hdc && sfmlcontext.hwnd)
            ReleaseDC(sfmlcontext.hwnd, sfmlcontext.hdc);

        sfmlcontext.hwnd = nullptr;
        sfmlcontext.hdc = nullptr;
        sfmlcontext.glContext = nullptr;
        sfmlcontext.parent = nullptr;
#endif
    }

    inline bool SFMLIsRunning(std::shared_ptr<core::Context> ctx)
    {
        (void)ctx;
        return sfmlcontext.running;
    }

#endif // ALMOND_USING_SFML
} // namespace almondnamespace::sfmlcontext
