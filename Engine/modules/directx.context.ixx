module;

#include <functional>
#include <memory>
#include <span>

export module directx.context;

import core.context;
import context.commandqueue;
import spritehandle;
import atlas.texture;

export namespace epochnamespace::directxcontext
{
    int directx_get_width();
    int directx_get_height();

    bool directx_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWnd = nullptr,
        unsigned int w = 400,
        unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr);

    bool directx_process(std::shared_ptr<core::Context> ctx, core::CommandQueue& queue);
    void directx_draw_sprite(
        SpriteHandle sprite,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float w,
        float h) noexcept;
    void directx_cleanup(std::shared_ptr<core::Context> ctx);
}
