module;

#include <include/aengine.config.hpp>

export module acontext.vulkan.context:renderer;

#if defined(ALMOND_USING_VULKAN)

import <cstdint>;

export namespace almondnamespace::vulkanrenderer
{
    struct RendererContext
    {
        enum class RenderMode
        {
            SingleTexture,
            TextureAtlas
        };

        RenderMode mode = RenderMode::TextureAtlas;
        bool enable_sprite_draws = true;
    };

    struct VulkanConfig
    {
        std::uint32_t max_frames_in_flight = 2;
        bool enable_validation_layers =
#if defined(NDEBUG)
            false;
#else
            true;
#endif
        bool prefer_mailbox_present = true;
        bool enable_sampler_anisotropy = true;
    };

    inline RendererContext vulkan_renderer{};
    inline VulkanConfig vulkan_config{};
}

#endif
