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

#if defined(EPOCH_USING_VULKAN) && (EPOCH_USING_VULKAN == 1) && !defined(__linux__)
import engine.input;
import atlas.manager;
import core.logger;
import vulkan.context;

namespace
{
    constexpr std::string_view kLogVulkan = "Context.Vulkan";

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

    void vulkan_initialize_adapter()
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
            (void)epochnamespace::vulkancontext::vulkan_initialize(ctx, native, w, h, ctx->onResize);
        }
        catch (const std::exception& e)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogVulkan).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "init exception: {}",
                e.what());
        }
        catch (...)
        {
            ctx->init_failed = true;
            epochnamespace::logger::get(kLogVulkan).log(
                epochnamespace::logger::LogLevel::Error,
                "init unknown exception",
                std::source_location::current());
        }
    }

    void vulkan_cleanup_adapter()
    {
        auto ctx = epochnamespace::core::get_current_render_context();
        if (!ctx)
            return;

        try
        {
            epochnamespace::vulkancontext::vulkan_cleanup(ctx);
        }
        catch (const std::exception& e)
        {
            epochnamespace::logger::get(kLogVulkan).logf(
                epochnamespace::logger::LogLevel::Error,
                std::source_location::current(),
                "cleanup exception: {}",
                e.what());
        }
        catch (...)
        {
            epochnamespace::logger::get(kLogVulkan).log(
                epochnamespace::logger::LogLevel::Error,
                "cleanup unknown exception",
                std::source_location::current());
        }
    }

    bool vulkan_process_adapter(
        std::shared_ptr<epochnamespace::core::Context> ctx,
        epochnamespace::core::CommandQueue& queue)
    {
        if (!ctx)
            return false;
        return epochnamespace::vulkancontext::vulkan_process(ctx, queue);
    }
}

namespace epochnamespace::core::detail
{
    void register_vulkan_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::Vulkan;
        ctx->backendName = "Vulkan";

        ctx->initialize = vulkan_initialize_adapter;
        ctx->cleanup = vulkan_cleanup_adapter;
        ctx->process = vulkan_process_adapter;
        ctx->present = epochnamespace::vulkancontext::vulkan_present;
        ctx->get_width = epochnamespace::vulkancontext::vulkan_get_width;
        ctx->get_height = epochnamespace::vulkancontext::vulkan_get_height;

        bind_default_input(ctx);

        ctx->draw_sprite = nullptr;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const TextureAtlas& a) { return default_add_atlas(a, ContextType::Vulkan); };

        AddContextForBackend(ContextType::Vulkan, std::move(ctx));
    }
}
#endif
