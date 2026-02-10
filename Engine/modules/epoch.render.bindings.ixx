/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;


#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module epoch.render.bindings;

import epoch.platform.capabilities;
import epoch.render.device;

export namespace epoch
{
    struct TextureIdTag {};
    using TextureId = Handle<TextureIdTag, u32>;

    enum class BindingTier : u8 { bindless_indexed, bound_slots };

    struct BindingModel
    {
        BindingTier tier = BindingTier::bound_slots;
        u32 max_textures = 1024;
        u32 max_samplers = 1024;
    };

    [[nodiscard]] inline BindingModel choose_binding_model(const Capabilities& caps) noexcept
    {
        BindingModel m{};
        if (caps.bindless_textures && caps.descriptor_indexing)
        {
            m.tier = BindingTier::bindless_indexed;
            m.max_textures = caps.max_sampled_images;
            m.max_samplers = caps.max_samplers;
        }
        return m;
    }

    class BindingTable
    {
    public:
        explicit BindingTable(BindingModel model) : m_model(model) {}

        [[nodiscard]] TextureId register_texture(TextureHandle tex)
        {
            const u32 id = static_cast<u32>(m_textures.size()) + 1u;
            Entry e{};
            e.backend = tex;
            if (m_model.tier == BindingTier::bindless_indexed)
                e.bindless_index = id - 1u;
            m_textures.push_back(e);
            return TextureId{ id };
        }

        struct Resolved { TextureHandle backend{}; std::optional<u32> bindless_index{}; };

        [[nodiscard]] Resolved resolve(TextureId id) const
        {
            if (!id || id.value == 0 || id.value > m_textures.size())
                return {};
            const auto& e = m_textures[id.value - 1u];
            return Resolved{ e.backend, e.bindless_index };
        }

    private:
        struct Entry { TextureHandle backend{}; std::optional<u32> bindless_index{}; };
        BindingModel m_model{};
        std::vector<Entry> m_textures{};
    };
} // namespace epoch
