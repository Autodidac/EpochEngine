/**************************************************************
 *   Epoch Engine - Modern Renderer Skeleton (2026)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include "../include/_epoch.stl_types.hpp"

export module epoch.render.graph;

import epoch.render.device;

export namespace epoch
{
    struct GraphResourceTag {};
    struct GraphPassTag {};
    using GraphResource = Handle<GraphResourceTag, u32>;
    using GraphPass     = Handle<GraphPassTag, u32>;

    enum class ResourceKind : u8 { buffer, texture };

    struct GraphBuffer { BufferDesc desc{}; BufferHandle backend{}; };
    struct GraphTexture{ TextureDesc desc{}; TextureHandle backend{}; };

    struct ResourceDecl { ResourceKind kind{}; epoch::string name{}; u32 index = 0; };

    struct PassDecl
    {
        epoch::string name{};
        epoch::small_vector<GraphResource> reads{};
        epoch::small_vector<GraphResource> writes{};
        epoch::function_ref<void(ICommandContext&)> execute{};
    };

    class GraphBuilder
    {
    public:
        [[nodiscard]] GraphResource create_buffer(epoch::string_view name, const BufferDesc& desc);
        [[nodiscard]] GraphResource create_texture(epoch::string_view name, const TextureDesc& desc);
        [[nodiscard]] GraphPass add_pass(epoch::string_view name,
                                         epoch::array_view<const GraphResource> reads,
                                         epoch::array_view<const GraphResource> writes,
                                         epoch::function_ref<void(ICommandContext&)> fn);

        struct CompiledGraph compile(IRenderDevice& dev) const;

    private:
        epoch::small_vector<ResourceDecl> m_resources{};
        epoch::small_vector<GraphBuffer>  m_buffers{};
        epoch::small_vector<GraphTexture> m_textures{};
        epoch::small_vector<PassDecl>     m_passes{};
    };

    struct CompiledGraph
    {
        epoch::small_vector<ResourceDecl> resources{};
        epoch::small_vector<GraphBuffer>  buffers{};
        epoch::small_vector<GraphTexture> textures{};
        epoch::small_vector<PassDecl>     passes{};

        void execute(IRenderDevice& dev);
        void destroy(IRenderDevice& dev) noexcept;
    };
} // namespace epoch
