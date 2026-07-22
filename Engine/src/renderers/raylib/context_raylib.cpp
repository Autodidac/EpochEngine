#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>

#include <include/engine.config.hpp>

extern "C" std::uint32_t epoch_gui_render_frame_batches(void* context) noexcept;

import engine.input;
import atlas.texture;
import context.commandqueue;
import context.type;
import core.context;
import core.logger;
import image.loader;
import raylib.api;
import raylib.context;
import raylib.renderer;
import raylib.state;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
namespace
{
    struct RaylibFrameProbe
    {
        int width = 0;
        int height = 0;
        int format = 0;
        bool valid = false;
        epochengine::raylib_api::Color sample{};
        epochengine::raylib_api::Color sampleMirrored{};
        epochengine::raylib_api::Color center{};
    };

    [[nodiscard]] epochengine::raylib_api::Color sample_probe_pixel(
        const epochengine::raylib_api::Image& image,
        int x,
        int y) noexcept
    {
        if (!image.data
            || image.format != epochengine::raylib_api::pixelformat_rgba8
            || image.width <= 0
            || image.height <= 0)
        {
            return {};
        }

        x = (std::clamp)(x, 0, image.width - 1);
        y = (std::clamp)(y, 0, image.height - 1);
        const auto* pixels = static_cast<const std::uint8_t*>(image.data);
        const std::size_t offset =
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width)
                + static_cast<std::size_t>(x)) * 4u;
        return {
            pixels[offset + 0u],
            pixels[offset + 1u],
            pixels[offset + 2u],
            pixels[offset + 3u]
        };
    }

    [[nodiscard]] RaylibFrameProbe capture_raylib_frame_probe(
        int sampleX,
        int sampleY) noexcept
    {
        RaylibFrameProbe probe{};
        epochengine::raylib_api::flush_render_batch();
        const auto image = epochengine::raylib_api::load_image_from_screen();
        probe.width = image.width;
        probe.height = image.height;
        probe.format = image.format;
        probe.valid = image.data
            && image.width > 0
            && image.height > 0
            && image.format == epochengine::raylib_api::pixelformat_rgba8;
        if (probe.valid)
        {
            probe.sample = sample_probe_pixel(image, sampleX, sampleY);
            probe.sampleMirrored = sample_probe_pixel(
                image,
                sampleX,
                image.height - sampleY - 1);
            probe.center = sample_probe_pixel(image, image.width / 2, image.height / 2);
        }
        if (image.data)
            epochengine::raylib_api::unload_image(image);
        return probe;
    }

    std::uint32_t default_add_texture(
        epochengine::TextureAtlas&,
        std::string,
        const epochengine::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(const epochengine::TextureAtlas& atlas) noexcept
    {
        // Registration is CPU-side. The renderer uploads the latest atlas
        // snapshot lazily after Raylib has opened its owner-thread frame.
        const int index = atlas.get_index();
        return static_cast<std::uint32_t>(index >= 0 ? index + 1 : 1);
    }

    int default_add_model(const char*, const char* path) noexcept
    {
        return epochengine::raylib_api::load_model(path);
    }

    void bind_default_input(const std::shared_ptr<epochengine::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochengine::input::Key key) { return epochengine::input::is_key_held(key); };
        ctx->is_key_down = [](epochengine::input::Key key) { return epochengine::input::is_key_down(key); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochengine::input::mouseX.load(std::memory_order_relaxed);
            y = epochengine::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochengine::input::MouseButton button) { return epochengine::input::is_mouse_button_held(button); };
        ctx->is_mouse_button_down = [](epochengine::input::MouseButton button) { return epochengine::input::is_mouse_button_down(button); };
    }
}

namespace epochengine::core::detail
{
    extern "C" void epoch_register_raylib_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::RayLib;
        ctx->backendName = "RayLib";
        ctx->initialize = []()
        {
            auto current = epochengine::core::get_current_render_context();
            if (!current)
                return;

            current->init_failed = !epochengine::raylibcontext::raylib_initialize(
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
            auto current = epochengine::core::get_current_render_context();
            if (!current)
                return;

            epochengine::raylibcontext::raylib_cleanup(current);
        };
        ctx->process = [](std::shared_ptr<Context> current, CommandQueue& queue)
        {
            if (!current)
                return false;

            static thread_local Context* diagnosticContext = nullptr;
            static thread_local std::uint32_t diagnosticFrame = 0;
            static thread_local bool reportedFirstContentFrame = false;
            if (diagnosticContext != current.get())
            {
                diagnosticContext = current.get();
                diagnosticFrame = 0;
                reportedFirstContentFrame = false;
            }
            ++diagnosticFrame;

            if (current->windowData && current->windowData->get_should_close())
            {
                if (diagnosticFrame <= 6u)
                {
                    logger::warn(
                        "Raylib.Diagnostics",
                        "Raylib3 frame rejected because WindowData already requested close.");
                }
                queue.clear();
                return false;
            }

            const std::size_t queuedBefore = queue.depth();
            const std::uint8_t renderFlagsBefore = queue.render_flags_snapshot();
            const bool frameReady = epochengine::raylibcontext::raylib_process();
            if (!frameReady)
            {
                if (diagnosticFrame <= 6u)
                {
                    std::ostringstream message;
                    message
                        << "Raylib3 frame " << diagnosticFrame
                        << " not ready: running="
                        << epochengine::raylibcontext::raylib_is_running()
                        << " queueDepth=" << queuedBefore
                        << " renderFlags=" << static_cast<unsigned>(renderFlagsBefore);
                    logger::info("Raylib.Diagnostics", message.str());
                }
                return epochengine::raylibcontext::raylib_is_running();
            }

            if (!epochengine::raylibcontext::raylib_is_running())
                return false;

            auto& st = epochengine::raylibstate::s_raylibstate;
            st.running = true;
            st.owner_ctx = current.get();
            st.frameActive = false;
            st.frameInTextureMode = false;

            const int frameWidth = (std::max)(1, epochengine::raylib_api::get_render_width());
            const int frameHeight = (std::max)(1, epochengine::raylib_api::get_render_height());
            st.width = static_cast<unsigned>(frameWidth);
            st.height = static_cast<unsigned>(frameHeight);
            current->framebufferWidth = frameWidth;
            current->framebufferHeight = frameHeight;

            const auto viewport = current->scene_viewport();
            const auto native = epochengine::raylibcontext::raylib_native_diagnostics();
            std::uintptr_t hostWindow = 0;
            std::uintptr_t childWindow = 0;
#if defined(_WIN32)
            if (current->windowData)
            {
                hostWindow = reinterpret_cast<std::uintptr_t>(current->windowData->host_hwnd);
                childWindow = reinterpret_cast<std::uintptr_t>(
                    current->windowData->hwndChild.load(std::memory_order_acquire));
            }
#endif
            if (diagnosticFrame <= 6u)
            {
                std::ostringstream message;
                message
                    << "Raylib3 frame " << diagnosticFrame
                    << " begin: render=" << frameWidth << 'x' << frameHeight
                    << " screen=" << epochengine::raylib_api::get_screen_width()
                    << 'x' << epochengine::raylib_api::get_screen_height()
                    << " framebuffer=" << epochengine::raylib_api::get_framebuffer_width()
                    << 'x' << epochengine::raylib_api::get_framebuffer_height()
                    << " context=" << current->width << 'x' << current->height
                    << " viewport=" << viewport.x << ',' << viewport.y << ','
                    << viewport.width << 'x' << viewport.height
                    << " queueDepth=" << queuedBefore
                    << " renderFlags=" << static_cast<unsigned>(renderFlagsBefore)
                    << " ready=" << epochengine::raylib_api::is_window_ready()
                    << std::hex
                    << " hwnd=0x" << native.hwnd
                    << " parent=0x" << native.parent
                    << " host=0x" << hostWindow
                    << " child=0x" << childWindow
                    << " dc=0x" << native.dc
                    << " rc=0x" << native.glContext
                    << " currentDc=0x" << native.currentDc
                    << " currentRc=0x" << native.currentGlContext
                    << " windowFromDc=0x" << native.windowFromDc
                    << std::dec
                    << " valid=" << native.windowValid
                    << " visible=" << native.windowVisible
                    << " childStyle=" << native.childStyle
                    << " currentMatch=" << native.currentMatchesExpected
                    << " client=" << native.clientWidth << 'x' << native.clientHeight
                    << " ownerThread=" << native.ownerThread
                    << " currentThread=" << native.currentThread;
                logger::info("Raylib.Diagnostics", message.str());
            }

            // Raylib owns the frame boundary. The preview entry point starts
            // BeginDrawing exactly once, queued resource work then runs with
            // that GL context active, and this path closes the same frame.
            epochengine::raylibcontext::raylib_render_scene_preview(current);
            epochengine::raylibrenderer::reset_sprite_diagnostics();
            const bool queueDrained = queue.drain();
            const std::uint32_t guiMask = epoch_gui_render_frame_batches(current.get());
            const auto spriteDiagnostics = epochengine::raylibrenderer::sprite_diagnostics();

            const bool firstContentFrame = !reportedFirstContentFrame
                && (viewport.valid() || guiMask != 0u || spriteDiagnostics.attempted != 0u);
            const bool diagnosticMilestone = diagnosticFrame <= 6u
                || firstContentFrame
                || diagnosticFrame == 120u
                || diagnosticFrame == 240u;
            if (firstContentFrame)
                reportedFirstContentFrame = true;


            if (diagnosticMilestone)
            {
                const int sampleX = viewport.valid()
                    ? viewport.x + (viewport.width / 2)
                    : epochengine::raylib_api::get_render_width() / 2;
                const int sampleY = viewport.valid()
                    ? viewport.y + (viewport.height / 2)
                    : epochengine::raylib_api::get_render_height() / 2;
                const RaylibFrameProbe probe = capture_raylib_frame_probe(
                    sampleX,
                    sampleY);
                std::ostringstream message;
                const auto appendColor = [&message](const epochengine::raylib_api::Color color)
                {
                    message
                        << '(' << static_cast<unsigned>(color.r)
                        << ',' << static_cast<unsigned>(color.g)
                        << ',' << static_cast<unsigned>(color.b)
                        << ',' << static_cast<unsigned>(color.a) << ')';
                };
                message
                    << "Raylib3 frame " << diagnosticFrame
                    << " submitted: frameActive=" << st.frameActive
                    << " queueDrained=" << queueDrained
                    << " guiMask=" << guiMask
                    << " readbackValid=" << probe.valid
                    << " image=" << probe.width << 'x' << probe.height
                    << " format=" << probe.format
                    << " sample=";
                appendColor(probe.sample);
                message << " sampleMirror=";
                appendColor(probe.sampleMirrored);
                message << " center=";
                appendColor(probe.center);
                message
                    << " sprites(attempted/resolved/upload/texture/frame/submitted)="
                    << spriteDiagnostics.attempted << '/'
                    << spriteDiagnostics.resolved << '/'
                    << spriteDiagnostics.uploadReady << '/'
                    << spriteDiagnostics.textureReady << '/'
                    << spriteDiagnostics.frameReady << '/'
                    << spriteDiagnostics.submitted
                    << " graphicsApi=" << epochengine::raylib_api::get_graphics_api_version()
                    << " defaultTexture=" << epochengine::raylib_api::get_default_texture_id();
                logger::info("Raylib.Diagnostics", message.str());
            }

            epochengine::raylib_api::end_drawing();
            st.frameActive = false;
            st.frameInTextureMode = false;

            if (current->windowData)
                current->windowData->firstPresentComplete.store(
                    true,
                    std::memory_order_release);
            return epochengine::raylibcontext::raylib_is_running()
                && !epochengine::raylib_api::window_should_close();
        };
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return epochengine::raylib_api::get_render_width(); };
        ctx->get_height = []() { return epochengine::raylib_api::get_render_height(); };
        ctx->draw_sprite = epochengine::raylibrenderer::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochengine::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        ctx->add_model = &default_add_model;
        ctx->onResize = &epochengine::raylibcontext::raylib_resize;
        bind_default_input(ctx);
        AddContextForBackend(ContextType::RayLib, std::move(ctx));
    }
}
#endif
