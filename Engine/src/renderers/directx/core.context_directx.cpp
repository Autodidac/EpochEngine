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
import input.engine;
import atlas.manager;
import core.logger;
import directx.context;
import perf.tier;

namespace
{
    constexpr std::string_view kLogDirectX = "Context.DirectX";

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

    void directx_initialize_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        ctx->init_failed = false;
        try
        {
            ctx->init_failed = !epochengine::directxcontext::directx_initialize(
                ctx,
                ctx->get_hwnd(),
                static_cast<unsigned>((std::max)(1, ctx->width)),
                static_cast<unsigned>((std::max)(1, ctx->height)),
                ctx->onResize);
        }
        catch (const std::exception& e)
        {
            ctx->init_failed = true;
            epochengine::logger::get(kLogDirectX).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...)
        {
            ctx->init_failed = true;
            epochengine::logger::get(kLogDirectX).log(
                epochengine::logger::LogLevel::Error,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void directx_cleanup_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            epochengine::directxcontext::directx_cleanup(ctx);
        }
        catch (const std::exception& e)
        {
            epochengine::logger::get(kLogDirectX).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...)
        {
            epochengine::logger::get(kLogDirectX).log(
                epochengine::logger::LogLevel::Error,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool directx_process_adapter(
        std::shared_ptr<epochengine::core::Context> ctx,
        epochengine::core::CommandQueue& queue)
    {
        if (!ctx)
            return false;
        return epochengine::directxcontext::directx_process(std::move(ctx), queue);
    }
}

namespace epochengine::core::detail
{
    void register_directx_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::DirectX;
        ctx->backendName = "DirectX";

        ctx->initialize = directx_initialize_adapter;
        ctx->cleanup = directx_cleanup_adapter;
        ctx->process = directx_process_adapter;
        ctx->get_width = epochengine::directxcontext::directx_get_width;
        ctx->get_height = epochengine::directxcontext::directx_get_height;
        ctx->frame_pacing_capabilities =
            epochengine::perf::frame_pacing_capabilities_for(
                epochengine::perf::frame_pacing_backend::directx);
        ctx->apply_frame_pacing = [](
            const epochengine::perf::frame_pacing_mode mode,
            double)
        {
            const bool wantsVsync =
                mode == epochengine::perf::frame_pacing_mode::vsync;
            const bool configured =
                epochengine::directxcontext::directx_set_vsync(
                    epochengine::core::get_current_render_context(),
                    wantsVsync);
            return epochengine::perf::native_frame_pacing_result{
                configured,
                configured && wantsVsync,
                wantsVsync
                    ? epochengine::perf::frame_pacing_mode::vsync
                    : epochengine::perf::frame_pacing_mode::uncapped,
                0.0};
        };

        bind_default_input(ctx);

        ctx->draw_sprite = epochengine::directxcontext::directx_draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& a) { return default_add_atlas(a, ContextType::DirectX); };

        AddContextForBackend(ContextType::DirectX, std::move(ctx));
    }
}
#endif
