module;

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include <include/aengine.config.hpp>

module core.context;

#if defined(EPOCH_USING_SOFTWARE_RENDERER) && (EPOCH_USING_SOFTWARE_RENDERER == 1)
import aengine.input;
import atlas.manager;
import core.logger;
import software.context;

namespace
{
    constexpr std::string_view kLogSoftRenderer = "Context.SoftRenderer";

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

    void softrenderer_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            (void)epochnamespace::anativecontext::softrenderer_initialize(
                ctx,
                ctx->get_hwnd(),
                static_cast<unsigned>((std::max)(1, ctx->width)),
                static_cast<unsigned>((std::max)(1, ctx->height)),
                ctx->onResize);
        }
        catch (const std::exception& e)
        {
            epochnamespace::logger::get(kLogSoftRenderer).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...)
        {
            epochnamespace::logger::get(kLogSoftRenderer).log(
                epochnamespace::logger::LogLevel::Error,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void softrenderer_cleanup_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            auto copy = ctx;
            epochnamespace::anativecontext::softrenderer_cleanup(copy);
        }
        catch (const std::exception& e)
        {
            epochnamespace::logger::get(kLogSoftRenderer).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...)
        {
            epochnamespace::logger::get(kLogSoftRenderer).log(
                epochnamespace::logger::LogLevel::Error,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool softrenderer_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx)
            return false;
        return epochnamespace::anativecontext::softrenderer_process(*ctx, queue);
    }
}

namespace epochnamespace::core::detail
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

        ctx->draw_sprite = epochnamespace::anativecontext::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& a) { return default_add_atlas(a, ContextType::Software); };

        AddContextForBackend(ContextType::Software, std::move(ctx));
    }
}
#endif
