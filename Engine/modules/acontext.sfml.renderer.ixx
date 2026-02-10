module;

#include <include/aengine.config.hpp>

export module acontext.sfml.renderer;

#if defined(ALMOND_USING_SFML)

export namespace almondnamespace::sfmlcontext
{
    struct RendererContext
    {
        enum class RenderMode
        {
            SingleTexture,
            TextureAtlas
        };

        RenderMode mode = RenderMode::TextureAtlas;
    };

    inline RendererContext sfml_renderer{};
}

#endif
