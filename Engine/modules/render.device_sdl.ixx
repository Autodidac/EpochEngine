/************************************************
 *  Epoch Engine - SDL Renderer Device Module
 *
 *  SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ***********************************************/
module;

#include "../include/engine.config.hpp"
#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
#include <string>
#include <vector>

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
#include <SDL3/SDL.h>
#endif

export module render.device_sdl;

import render.device;

#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
import sdl.renderer;
import sdl.state;
#endif

export namespace epoch
{
#if defined(EPOCH_USING_SDL) && (EPOCH_USING_SDL == 1)
    struct SdlRenderTextureRecord
    {
        RenderTextureAssetDesc desc{};
        SDL_Texture* texture = nullptr;
        SDL_Texture* previous_target = nullptr;
        u32 width = 0;
        u32 height = 0;
        bool active = false;
    };

    struct SdlBindingSetRecord
    {
        CommandResourceBindings bindings{};
        bool active = false;
    };

    struct SdlSlotRecord
    {
        bool active = false;
    };

    struct SdlMeshRecord
    {
        MeshDesc desc{};
        bool active = false;
    };

    struct SdlModelRecord
    {
        ModelDesc desc{};
        bool active = false;
    };

    class SdlCommandContext final : public ICommandContext
    {
    public:
        void set_render_textures(std::vector<SdlRenderTextureRecord>* records) noexcept
        {
            m_render_textures = records;
        }

        void begin(const char*) override {}
        void end() override {}
        void debug_marker(const char*) override {}
        void bind_binding_set(BindingSetHandle binding_set) override { m_binding_set = binding_set; }
        void bind_resources(const CommandResourceBindings& bindings) override { m_bindings = bindings; }
        void barrier() override {}

        void begin_render_pass(RenderTargetHandle render_target, const RenderPassDesc& pass) override
        {
            SdlRenderTextureRecord* const record = resolve(render_target);
            SDL_Renderer* const renderer = active_renderer();
            if (!record || !renderer || epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted)
                return;

            record->previous_target = SDL_GetRenderTarget(renderer);
            if (!SDL_SetRenderTarget(renderer, record->texture))
            {
                epochnamespace::sdlcontext::check_sdl_error("SDL_SetRenderTarget");
                epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted = true;
                return;
            }

            m_render_pass_open = true;
            m_render_target = render_target;
            m_last_width = record->width;
            m_last_height = record->height;

            if (pass.clear_color)
            {
                if (!SDL_SetRenderDrawColor(
                        renderer,
                        to_channel(pass.clear[0]),
                        to_channel(pass.clear[1]),
                        to_channel(pass.clear[2]),
                        to_channel(pass.clear[3])))
                {
                    epochnamespace::sdlcontext::check_sdl_error("SDL_SetRenderDrawColor");
                    epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted = true;
                    return;
                }

                if (!SDL_RenderClear(renderer))
                {
                    epochnamespace::sdlcontext::check_sdl_error("SDL_RenderClear");
                    epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted = true;
                    return;
                }
            }
        }

        void end_render_pass() override
        {
            if (!m_render_pass_open)
                return;

            SDL_Renderer* const renderer = active_renderer();
            SdlRenderTextureRecord* const record = resolve(m_render_target);
            if (renderer && record)
            {
                if (!SDL_SetRenderTarget(renderer, record->previous_target))
                {
                    epochnamespace::sdlcontext::check_sdl_error("SDL_SetRenderTarget restore");
                    epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted = true;
                }
                record->previous_target = nullptr;
            }

            m_render_pass_open = false;
            m_render_target = {};
        }

        void draw_mesh(MeshHandle mesh, MaterialHandle material = {}) override
        {
            m_last_mesh = mesh;
            m_last_material = material;
        }

        void draw_model(ModelHandle model) override
        {
            m_last_model = model;
        }

        [[nodiscard]] bool render_pass_open() const noexcept { return m_render_pass_open; }
        [[nodiscard]] RenderTargetHandle last_render_target() const noexcept { return m_render_target; }
        [[nodiscard]] BindingSetHandle bound_binding_set() const noexcept { return m_binding_set; }
        [[nodiscard]] const CommandResourceBindings& bound_resources() const noexcept { return m_bindings; }
        [[nodiscard]] u32 last_width() const noexcept { return m_last_width; }
        [[nodiscard]] u32 last_height() const noexcept { return m_last_height; }
        [[nodiscard]] MeshHandle last_mesh() const noexcept { return m_last_mesh; }
        [[nodiscard]] MaterialHandle last_material() const noexcept { return m_last_material; }
        [[nodiscard]] ModelHandle last_model() const noexcept { return m_last_model; }

    private:
        [[nodiscard]] static SDL_Renderer* active_renderer() noexcept
        {
            return epochnamespace::sdlcontext::sdl_renderer.renderer;
        }

        [[nodiscard]] SdlRenderTextureRecord* resolve(RenderTargetHandle render_target) noexcept
        {
            if (!m_render_textures || !render_target)
                return nullptr;

            const u32 index = render_target.value - 1u;
            if (index >= m_render_textures->size())
                return nullptr;

            SdlRenderTextureRecord& record = (*m_render_textures)[index];
            return record.active ? &record : nullptr;
        }

        [[nodiscard]] static constexpr unsigned char to_channel(float value) noexcept
        {
            if (value <= 0.0f)
                return 0;
            if (value >= 1.0f)
                return 255;
            return static_cast<unsigned char>(value * 255.0f + 0.5f);
        }

        std::vector<SdlRenderTextureRecord>* m_render_textures = nullptr;
        CommandResourceBindings m_bindings{};
        BindingSetHandle m_binding_set{};
        RenderTargetHandle m_render_target{};
        MeshHandle m_last_mesh{};
        MaterialHandle m_last_material{};
        ModelHandle m_last_model{};
        u32 m_last_width = 0;
        u32 m_last_height = 0;
        bool m_render_pass_open = false;
    };

    class SdlRenderDevice final : public IRenderDevice
    {
    public:
        SdlRenderDevice()
        {
            m_context.set_render_textures(&m_render_textures);
        }

        std::string backend_name() const override { return "sdl3"; }

        RendererCapabilities capabilities() const noexcept override
        {
            RendererCapabilities caps = renderer_capabilities_for(RendererBackendKind::sdl3);
            caps.buffers = true;
            caps.sampled_rtt_hook_ready = true;
            caps.sampled_rtt_live_allocation_ready = runtime_renderer_available();
            caps.sampled_rtt_presentation_proven = false;
            caps.native_sampled_render_targets = caps.sampled_rtt_live_allocation_ready;
            caps.mesh_resources = true;
            caps.model_resources = true;
            return caps;
        }

        BufferHandle create_buffer(const BufferDesc&) override { return BufferHandle{ allocate_slot(m_buffers) }; }
        TextureHandle create_texture(const TextureDesc&) override { return TextureHandle{ allocate_slot(m_textures) }; }
        SamplerHandle create_sampler(const SamplerDesc&) override { return SamplerHandle{ allocate_slot(m_samplers) }; }
        ShaderHandle create_shader(const ShaderDesc&) override { return ShaderHandle{ allocate_slot(m_shaders) }; }
        PipelineHandle create_pipeline(const PipelineDesc&) override { return PipelineHandle{ allocate_slot(m_pipelines) }; }
        MaterialHandle create_material(const MaterialDesc&) override { return MaterialHandle{ allocate_slot(m_materials) }; }
        RenderTargetHandle create_render_target(const RenderTargetDesc&) override { return RenderTargetHandle{ allocate_slot(m_render_targets) }; }

        BindingSetHandle create_binding_set(const CommandResourceBindings& bindings) override
        {
            const u32 slot = allocate_binding_set_slot();
            SdlBindingSetRecord& record = m_binding_sets[slot];
            record.bindings = bindings;
            record.active = true;
            return BindingSetHandle{ slot + 1u };
        }

        MeshHandle create_mesh(const MeshDesc& desc) override
        {
            const u32 slot = allocate_mesh_slot();
            SdlMeshRecord& record = m_meshes[slot];
            record.desc = desc;
            record.active = true;
            return MeshHandle{ slot + 1u };
        }

        ModelHandle create_model(const ModelDesc& desc) override
        {
            const u32 slot = allocate_model_slot();
            SdlModelRecord& record = m_models[slot];
            record.desc = desc;
            record.active = true;
            return ModelHandle{ slot + 1u };
        }

        RenderTextureAssetHandles create_render_texture_asset(const RenderTextureAssetDesc& desc) override
        {
            SDL_Renderer* const renderer = epochnamespace::sdlcontext::sdl_renderer.renderer;
            if (!renderer || epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted)
                return {};

            const u32 width = desc.width == 0 ? 1u : desc.width;
            const u32 height = desc.height == 0 ? 1u : desc.height;

            SDL_Texture* const texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_TARGET,
                static_cast<int>(width),
                static_cast<int>(height));

            if (!texture)
            {
                epochnamespace::sdlcontext::check_sdl_error("SDL_CreateTexture render target");
                return {};
            }

            (void)SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);

            const u32 slot = allocate_render_texture_slot();
            SdlRenderTextureRecord& record = m_render_textures[slot];
            record.desc = desc;
            record.texture = texture;
            record.width = width;
            record.height = height;
            record.active = true;

            const u32 handle_value = slot + 1u;
            return RenderTextureAssetHandles{
                TextureHandle{ handle_value },
                SamplerHandle{ handle_value },
                RenderTargetHandle{ handle_value } };
        }

        void destroy(BufferHandle handle) noexcept override { release_slot(m_buffers, handle.value); }
        void destroy(TextureHandle handle) noexcept override { release_slot(m_textures, handle.value); }
        void destroy(SamplerHandle handle) noexcept override { release_slot(m_samplers, handle.value); }
        void destroy(ShaderHandle handle) noexcept override { release_slot(m_shaders, handle.value); }
        void destroy(PipelineHandle handle) noexcept override { release_slot(m_pipelines, handle.value); }
        void destroy(MaterialHandle handle) noexcept override { release_slot(m_materials, handle.value); }
        void destroy(RenderTargetHandle handle) noexcept override { release_slot(m_render_targets, handle.value); }
        void destroy(MeshHandle mesh) noexcept override
        {
            if (!mesh)
                return;

            const u32 index = mesh.value - 1u;
            if (index < m_meshes.size())
                m_meshes[index] = {};
        }

        void destroy(ModelHandle model) noexcept override
        {
            if (!model)
                return;

            const u32 index = model.value - 1u;
            if (index < m_models.size())
                m_models[index] = {};
        }

        void destroy(BindingSetHandle binding_set) noexcept override
        {
            if (!binding_set)
                return;

            const u32 index = binding_set.value - 1u;
            if (index < m_binding_sets.size())
                m_binding_sets[index] = {};
        }

        void destroy(RenderTextureAssetHandles handles) noexcept override
        {
            if (!handles.render_target)
                return;

            const u32 index = handles.render_target.value - 1u;
            if (index >= m_render_textures.size())
                return;

            SdlRenderTextureRecord& record = m_render_textures[index];
            if (!record.active)
                return;

            if (record.texture)
                SDL_DestroyTexture(record.texture);

            record = {};
        }

        ICommandContext& acquire_graphics_context() override { return m_context; }
        CommandListHandle begin_command_list(const char*) override { return {}; }
        void end_command_list(CommandListHandle) override {}
        void present(ISwapchain&) override {}

        [[nodiscard]] bool runtime_renderer_available() const noexcept
        {
            return epochnamespace::sdlcontext::sdl_renderer.renderer != nullptr &&
                   !epochnamespace::sdlcontext::state::get_sdl_state().renderFaulted;
        }

        [[nodiscard]] const SdlRenderTextureRecord* resolve_render_texture(RenderTargetHandle handle) const noexcept
        {
            if (!handle)
                return nullptr;

            const u32 index = handle.value - 1u;
            if (index >= m_render_textures.size())
                return nullptr;

            const SdlRenderTextureRecord& record = m_render_textures[index];
            return record.active ? &record : nullptr;
        }

        [[nodiscard]] u32 render_texture_count() const noexcept
        {
            u32 count = 0;
            for (const SdlRenderTextureRecord& record : m_render_textures)
            {
                if (record.active)
                    ++count;
            }
            return count;
        }

        [[nodiscard]] const SdlModelRecord* resolve_model(ModelHandle model) const noexcept
        {
            if (!model)
                return nullptr;

            const u32 index = model.value - 1u;
            if (index >= m_models.size())
                return nullptr;

            const SdlModelRecord& record = m_models[index];
            return record.active ? &record : nullptr;
        }

    private:
        [[nodiscard]] static u32 allocate_slot(std::vector<SdlSlotRecord>& records)
        {
            for (u32 i = 0; i < static_cast<u32>(records.size()); ++i)
            {
                if (!records[i].active)
                {
                    records[i].active = true;
                    return i + 1u;
                }
            }

            records.push_back(SdlSlotRecord{ true });
            return static_cast<u32>(records.size());
        }

        static void release_slot(std::vector<SdlSlotRecord>& records, u32 handle_value) noexcept
        {
            if (handle_value == 0u)
                return;

            const u32 index = handle_value - 1u;
            if (index < records.size())
                records[index] = {};
        }

        [[nodiscard]] u32 allocate_render_texture_slot()
        {
            for (u32 i = 0; i < static_cast<u32>(m_render_textures.size()); ++i)
            {
                if (!m_render_textures[i].active)
                    return i;
            }

            m_render_textures.push_back({});
            return static_cast<u32>(m_render_textures.size() - 1u);
        }

        [[nodiscard]] u32 allocate_binding_set_slot()
        {
            for (u32 i = 0; i < static_cast<u32>(m_binding_sets.size()); ++i)
            {
                if (!m_binding_sets[i].active)
                    return i;
            }

            m_binding_sets.push_back({});
            return static_cast<u32>(m_binding_sets.size() - 1u);
        }

        [[nodiscard]] u32 allocate_mesh_slot()
        {
            for (u32 i = 0; i < static_cast<u32>(m_meshes.size()); ++i)
            {
                if (!m_meshes[i].active)
                    return i;
            }

            m_meshes.push_back({});
            return static_cast<u32>(m_meshes.size() - 1u);
        }

        [[nodiscard]] u32 allocate_model_slot()
        {
            for (u32 i = 0; i < static_cast<u32>(m_models.size()); ++i)
            {
                if (!m_models[i].active)
                    return i;
            }

            m_models.push_back({});
            return static_cast<u32>(m_models.size() - 1u);
        }

        SdlCommandContext m_context{};
        std::vector<SdlRenderTextureRecord> m_render_textures{};
        std::vector<SdlBindingSetRecord> m_binding_sets{};
        std::vector<SdlMeshRecord> m_meshes{};
        std::vector<SdlModelRecord> m_models{};
        std::vector<SdlSlotRecord> m_buffers{};
        std::vector<SdlSlotRecord> m_textures{};
        std::vector<SdlSlotRecord> m_samplers{};
        std::vector<SdlSlotRecord> m_shaders{};
        std::vector<SdlSlotRecord> m_pipelines{};
        std::vector<SdlSlotRecord> m_materials{};
        std::vector<SdlSlotRecord> m_render_targets{};
    };
#endif
}
