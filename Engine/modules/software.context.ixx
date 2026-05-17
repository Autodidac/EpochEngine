module;

#include <functional>
#include <memory>
#include <span>

export module software.context;

import spritehandle;
import atlas.texture;
import core.context;
import context.commandqueue;

export namespace epochnamespace::anativecontext
{
    int get_width();
    int get_height();

    bool softrenderer_initialize(
        std::shared_ptr<core::Context> ctx,
        void* parentWnd = nullptr,
        unsigned int w = 400,
        unsigned int h = 300,
        std::function<void(int, int)> onResize = nullptr);

    void draw_sprite(
        SpriteHandle handle,
        std::span<const TextureAtlas* const> atlases,
        float x,
        float y,
        float width,
        float height) noexcept;

    bool softrenderer_process(core::Context& ctx, core::CommandQueue& queue);
    void softrenderer_cleanup(std::shared_ptr<core::Context>& ctx);
}
