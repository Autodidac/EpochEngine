module;

#include "../include/_epoch.stl_types.hpp"

module epoch.render.graph;

namespace epoch
{
    GraphResource GraphBuilder::create_buffer(epoch::string_view name, const BufferDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_buffers.size());
        m_buffers.push_back(GraphBuffer{ desc, {} });

        ResourceDecl r{
            .kind = ResourceKind::buffer,
            .name = epoch::string{name},   // requires ctor from epoch::string_view
            .index = idx
        };
        m_resources.push_back(std::move(r));

        return GraphResource{ static_cast<u32>(m_resources.size() - 1) };
    }


    GraphResource GraphBuilder::create_texture(epoch::string_view name, const TextureDesc& desc)
    {
        const u32 idx = static_cast<u32>(m_textures.size());
        m_textures.push_back(GraphTexture{ desc, {} });
        m_resources.push_back(ResourceDecl{ ResourceKind::texture, epoch::string(name), idx });
        return GraphResource{ static_cast<u32>(m_resources.size()) };
    }

    GraphPass GraphBuilder::add_pass(epoch::string_view name,
                                     epoch::array_view<const GraphResource> reads,
                                     epoch::array_view<const GraphResource> writes,
                                     epoch::function_ref<void(ICommandContext&)> fn)
    {
        PassDecl p{};
        p.name = epoch::string(name);
        p.reads.assign(reads.begin(), reads.end());
        p.writes.assign(writes.begin(), writes.end());
        p.execute = std::move(fn);
        m_passes.push_back(std::move(p));
        return GraphPass{ static_cast<u32>(m_passes.size()) };
    }

    CompiledGraph GraphBuilder::compile(IRenderDevice& dev) const
    {
        CompiledGraph g{};
        g.resources = m_resources;
        g.buffers   = m_buffers;
        g.textures  = m_textures;
        g.passes    = m_passes;

        for (auto& b : g.buffers)  b.backend = dev.create_buffer(b.desc);
        for (auto& t : g.textures) t.backend = dev.create_texture(t.desc);
        return g;
    }

    void CompiledGraph::execute(IRenderDevice& dev)
    {
        auto& ctx = dev.acquire_graphics_context();
        ctx.begin("frame_graph");
        for (auto& p : passes)
        {
            ctx.debug_marker(p.name.c_str());
            if (p.execute) p.execute(ctx);
            ctx.barrier();
        }
        ctx.end();
    }

    void CompiledGraph::destroy(IRenderDevice& dev) noexcept
    {
        for (auto& b : buffers)  if (b.backend) dev.destroy(b.backend);
        for (auto& t : textures) if (t.backend) dev.destroy(t.backend);
    }
} // namespace epoch
