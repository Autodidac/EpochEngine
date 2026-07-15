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

#if defined(EPOCH_USING_DIRECTX) && (EPOCH_USING_DIRECTX == 1)
import engine.input;
import atlas.manager;
import core.logger;
import directx.context;

namespace
{
    constexpr std::string_view kLogDirectX = "Context.DirectX";

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

    void directx_initialize_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        ctx->init_failed = false;
        try
        {
            ctx->init_failed = !epochnamespace::directxcontext::directx_initialize(
                ctx,
                ctx->get_hwnd(),
                static_cast<unsigned>((std::max)(1, ctx->width)),
                static_cast<unsigned>((std::max)(1, ctx->height)),
                ctx->onResize);
        }
        catch (const std::exception& e)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogDirectX).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogDirectX).log(
                epochnamespace::logger::LogLevel::Error,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void directx_cleanup_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            epochnamespace::directxcontext::directx_cleanup(ctx);
        }
        catch (const std::exception& e)
        {
            epochnamespace::logger::get(kLogDirectX).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...)
        {
            epochnamespace::logger::get(kLogDirectX).log(
                epochnamespace::logger::LogLevel::Error,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool directx_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx)
            return false;
        return epochnamespace::directxcontext::directx_process(std::move(ctx), queue);
    }
}

namespace epochnamespace::core::detail
{
    void register_directx_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::DirectX;
        ctx->backendName = "DirectX";

        ctx->initialize = directx_initialize_adapter;
        ctx->cleanup = directx_cleanup_adapter;
        ctx->process = directx_process_adapter;
        ctx->get_width = epochnamespace::directxcontext::directx_get_width;
        ctx->get_height = epochnamespace::directxcontext::directx_get_height;

        bind_default_input(ctx);

        ctx->draw_sprite = epochnamespace::directxcontext::directx_draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& a) { return default_add_atlas(a, ContextType::DirectX); };

        AddContextForBackend(ContextType::DirectX, std::move(ctx));
    }
}
#endif
