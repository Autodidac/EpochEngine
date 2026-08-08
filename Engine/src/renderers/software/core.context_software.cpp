module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include <include/engine.config.hpp>

module core.context;

#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
import input.engine;
import atlas.manager;
import core.logger;
import software.context;

namespace
{
    constexpr std::string_view kLogSoftRenderer = "Context.SoftRenderer";

    std::uint32_t default_add_texture(epochengine::TextureAtlas&, std::string, const epochengine::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(
        const epochengine::TextureAtlas& atlas,
        const epochengine::core::ContextType type) noexcept
    {
        try
        {
            epochengine::atlasmanager::ensure_uploaded(atlas);
            epochengine::atlasmanager::process_pending_uploads(type);
        }
        catch (...)
        {
        }

        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    void bind_default_input(const std::shared_ptr<epochengine::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochengine::input::Key k) { return epochengine::input::is_key_held(k); };
        ctx->is_key_down = [](epochengine::input::Key k) { return epochengine::input::is_key_down(k); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochengine::input::mouseX.load(std::memory_order_relaxed);
            y = epochengine::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochengine::input::MouseButton b) { return epochengine::input::is_mouse_button_held(b); };
        ctx->is_mouse_button_down = [](epochengine::input::MouseButton b) { return epochengine::input::is_mouse_button_down(b); };
    }

    void softrenderer_initialize_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        ctx->init_failed = false;
        try
        {
            ctx->init_failed = !epochengine::anativecontext::softrenderer_initialize(
                ctx,
                ctx->get_hwnd(),
                static_cast<unsigned>((std::max)(1, ctx->width)),
                static_cast<unsigned>((std::max)(1, ctx->height)),
                ctx->onResize);
        }
        catch (const std::exception& e)
        {
            ctx->init_failed = true;
            epochengine::logger::get(kLogSoftRenderer).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...)
        {
            ctx->init_failed = true;
            epochengine::logger::get(kLogSoftRenderer).log(
                epochengine::logger::LogLevel::Error,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void softrenderer_cleanup_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            auto copy = ctx;
            epochengine::anativecontext::softrenderer_cleanup(copy);
        }
        catch (const std::exception& e)
        {
            epochengine::logger::get(kLogSoftRenderer).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...)
        {
            epochengine::logger::get(kLogSoftRenderer).log(
                epochengine::logger::LogLevel::Error,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool softrenderer_process_adapter(
        std::shared_ptr<epochengine::core::Context> ctx,
        epochengine::core::CommandQueue& queue)
    {
        if (!ctx)
            return false;
        return epochengine::anativecontext::softrenderer_process(*ctx, queue);
    }
}

namespace epochengine::core::detail
{
    void register_software_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::Software;
        ctx->backendName = "Software";

        ctx->initialize = softrenderer_initialize_adapter;
        ctx->cleanup = softrenderer_cleanup_adapter;
        ctx->process = softrenderer_process_adapter;

        bind_default_input(ctx);

        ctx->draw_sprite = epochengine::anativecontext::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& a) { return default_add_atlas(a, ContextType::Software); };

        AddContextForBackend(ContextType::Software, std::move(ctx));
    }
}
#endif
