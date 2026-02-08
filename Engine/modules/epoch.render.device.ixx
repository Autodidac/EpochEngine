/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module epoch.render.device;

//import <string>;

export namespace epoch
{
    struct BufferDesc
    {
        u64 size_bytes = 0;
        bool gpu_only  = true;
        bool storage   = false;
        bool uniform   = false;
        bool indirect  = false;
        bool mapped    = false;
        const char* debug_name = nullptr;
    };

    struct TextureDesc
    {
        u32 width = 1, height = 1, mip_levels = 1;
        bool sampled = true;
        bool storage = false;
        bool render_target = false;
        bool sparse = false;
        const char* debug_name = nullptr;
    };

    struct BackendBufferTag {};
    struct BackendTextureTag {};
    using BufferHandle  = Handle<BackendBufferTag, u32>;
    using TextureHandle = Handle<BackendTextureTag, u32>;

    struct ICommandContext
    {
        virtual ~ICommandContext() = default;
        virtual void begin(const char* label) = 0;
        virtual void end() = 0;
        virtual void debug_marker(const char* label) = 0;
        virtual void barrier() = 0;
    };

    struct ISwapchain
    {
        virtual ~ISwapchain() = default;
        virtual u32 width() const noexcept = 0;
        virtual u32 height() const noexcept = 0;
    };

    struct IRenderDevice
    {
        virtual ~IRenderDevice() = default;
        virtual std::string backend_name() const = 0;

        virtual BufferHandle  create_buffer(const BufferDesc& desc) = 0;
        virtual TextureHandle create_texture(const TextureDesc& desc) = 0;

        virtual void destroy(BufferHandle) noexcept = 0;
        virtual void destroy(TextureHandle) noexcept = 0;

        virtual ICommandContext& acquire_graphics_context() = 0;
        virtual void present(ISwapchain& sc) = 0;
    };
} // namespace epoch
