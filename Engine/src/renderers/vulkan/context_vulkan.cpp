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

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   ifdef EPOCH_USING_WINMAIN
#       include "../src/framework.hpp"
#   endif
#endif

#ifdef min
#   undef min
#endif
#ifdef max
#   undef max
#endif

module core.context;

#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1)
import engine.input;
import atlas.manager;
import core.logger;
import vulkan.context;

namespace
{
    constexpr std::string_view kLogVulkan = "Context.Vulkan";

    void* ctx_native_window_handle(const std::shared_ptr<epochengine::core::Context>& ctx) noexcept
    {
        if (!ctx)
            return nullptr;
        if (auto h = ctx->get_hwnd())
            return h;
        if (ctx->windowData && ctx->windowData->hwnd)
            return ctx->windowData->hwnd;
        return nullptr;
    }

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

    void vulkan_initialize_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        const auto native = ctx_native_window_handle(ctx);
        const unsigned w = static_cast<unsigned>((std::max)(1, ctx->width));
        const unsigned h = static_cast<unsigned>((std::max)(1, ctx->height));

        ctx->init_failed = false;
        try
        {
            ctx->init_failed = !epochengine::vulkancontext::vulkan_initialize(ctx, native, w, h, ctx->onResize);
        }
        catch (const std::exception& e)
        {
            ctx->init_failed = true;
            epochengine::logger::get(kLogVulkan).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...)
        {
            ctx->init_failed = true;
            epochengine::logger::get(kLogVulkan).log(
                epochengine::logger::LogLevel::Error,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void vulkan_cleanup_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            epochengine::vulkancontext::vulkan_cleanup(ctx);
        }
        catch (const std::exception& e)
        {
            epochengine::logger::get(kLogVulkan).logf(
                epochengine::logger::LogLevel::Error,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...)
        {
            epochengine::logger::get(kLogVulkan).log(
                epochengine::logger::LogLevel::Error,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool vulkan_process_adapter(
        std::shared_ptr<epochengine::core::Context> ctx,
        epochengine::core::CommandQueue& queue)
    {
        if (!ctx)
            return false;
        return epochengine::vulkancontext::vulkan_process(ctx, queue);
    }
}

namespace epochengine::core::detail
{
    void register_vulkan_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::Vulkan;
        ctx->backendName = "Vulkan";

        ctx->initialize = vulkan_initialize_adapter;
        ctx->cleanup = vulkan_cleanup_adapter;
        ctx->process = vulkan_process_adapter;
        ctx->present = epochengine::vulkancontext::vulkan_present;
        ctx->get_width = epochengine::vulkancontext::vulkan_get_width;
        ctx->get_height = epochengine::vulkancontext::vulkan_get_height;

        bind_default_input(ctx);

        ctx->draw_sprite = nullptr;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& a) { return default_add_atlas(a, ContextType::Vulkan); };

        AddContextForBackend(ContextType::Vulkan, std::move(ctx));
    }
}
#endif
