module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>

#include <include/aengine.config.hpp>

module raylib.backend;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import aengine.gui;
import aengine.input;
import atlas.manager;
import atlas.texture;
import core.logger;
import image.loader;
import raylib.context;
import raylib.renderer;

namespace epochnamespace::raylibbackend
{
    namespace detail
    {
        constexpr std::string_view kLogRaylib = "Context.Raylib";

        void* native_window_handle(const std::shared_ptr<core::Context>& ctx) noexcept
        {
            if (!ctx)
                return nullptr;
            if (auto handle = ctx->get_hwnd())
                return handle;
            if (ctx->windowData && ctx->windowData->hwnd)
                return ctx->windowData->hwnd;
            return nullptr;
        }

        std::uint32_t default_add_texture(TextureAtlas&, std::string, const ImageData&) noexcept
        {
            return 0u;
        }

        std::uint32_t default_add_atlas(const TextureAtlas& atlas) noexcept
        {
            try
            {
                atlasmanager::ensure_uploaded(atlas);
                atlasmanager::process_pending_uploads(core::ContextType::RayLib);
            }
            catch (...)
            {
            }

            const int idx = atlas.get_index();
            return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
        }

        void bind_default_input(const std::shared_ptr<core::Context>& ctx)
        {
            ctx->is_key_held = [](input::Key key) { return input::is_key_held(key); };
            ctx->is_key_down = [](input::Key key) { return input::is_key_down(key); };
            ctx->get_mouse_position = [](int& x, int& y)
            {
                x = input::mouseX.load(std::memory_order_relaxed);
                y = input::mouseY.load(std::memory_order_relaxed);
            };
            ctx->is_mouse_button_held = [](input::MouseButton button) { return input::is_mouse_button_held(button); };
            ctx->is_mouse_button_down = [](input::MouseButton button) { return input::is_mouse_button_down(button); };
        }

        void initialize_adapter()
        {
            auto current = core::get_current_render_context();
            if (!current)
                return;

            const auto parent = native_window_handle(current);

            try
            {
                (void)raylibcontext::raylib_initialize(
                    current,
                    parent,
                    static_cast<unsigned>((std::max)(1, current->width)),
                    static_cast<unsigned>((std::max)(1, current->height)),
                    current->onResize,
                    current->backendName);
            }
            catch (const std::exception& e)
            {
                logger::get(kLogRaylib).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "init exception: {}",
                    e.what());
            }
            catch (...)
            {
                logger::get(kLogRaylib).log(
                    logger::LogLevel::Error,
                    "init unknown exception",
                    std::source_location::current());
            }
        }

        void cleanup_adapter()
        {
            auto current = core::get_current_render_context();
            if (!current)
                return;

            try
            {
                raylibcontext::raylib_cleanup(current);
            }
            catch (const std::exception& e)
            {
                logger::get(kLogRaylib).logf(
                    logger::LogLevel::Error,
                    std::source_location::current(),
                    "cleanup exception: {}",
                    e.what());
            }
            catch (...)
            {
                logger::get(kLogRaylib).log(
                    logger::LogLevel::Error,
                    "cleanup unknown exception",
                    std::source_location::current());
            }
        }

        bool process_adapter(
            std::shared_ptr<core::Context> ctx,
            core::CommandQueue& queue)
        {
            if (!ctx)
                return false;

            raylibcontext::raylib_process();
            if (!raylibcontext::raylib_is_running())
                return false;

            atlasmanager::process_pending_uploads(core::ContextType::RayLib);
            raylibcontext::raylib_clear(0.0f, 0.0f, 0.0f, 1.0f);
            raylibcontext::raylib_render_scene_preview(ctx);
            (void)queue.drain();
            (void)gui::render_deferred_batch(ctx.get());
            raylibcontext::raylib_present();

            return raylibcontext::raylib_is_running();
        }
    }

    void configure(const std::shared_ptr<core::Context>& ctx)
    {
        if (!ctx)
            return;

        ctx->initialize = detail::initialize_adapter;
        ctx->cleanup = detail::cleanup_adapter;
        ctx->process = detail::process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = raylibcontext::raylib_get_width;
        ctx->get_height = raylibcontext::raylib_get_height;
        ctx->draw_sprite = raylibrenderer::draw_sprite;
        ctx->add_texture = &detail::default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& atlas) { return detail::default_add_atlas(atlas); };
        detail::bind_default_input(ctx);
    }
}

extern "C" void epoch_register_raylib_backend()
{
    auto ctx = std::make_shared<epochnamespace::core::Context>();
    ctx->type = epochnamespace::core::ContextType::RayLib;
    ctx->backendName = "RayLib";
    epochnamespace::raylibbackend::configure(ctx);
    epochnamespace::core::AddContextForBackend(epochnamespace::core::ContextType::RayLib, std::move(ctx));
}
#endif
