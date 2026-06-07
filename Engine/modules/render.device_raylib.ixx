/************************************************
 *  Epoch Engine - Raylib Renderer Device Module
 *
 *  SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ***********************************************/
module;

#include "../include/engine.config.hpp"
#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include <string>
#include <vector>

export module render.device_raylib;

import render.device;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import raylib.api;
import raylib.state;
#endif

export namespace epoch
{
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
    struct RaylibRenderTextureRecord
    {
        epochnamespace::raylib_api::RenderTexture2D target{};
        u32 width = 0;
        u32 height = 0;
        bool active = false;
    };

    struct RaylibBindingSetRecord
    {
        CommandResourceBindings bindings{};
        bool active = false;
    };

    class RaylibCommandContext final : public ICommandContext
    {
    public:
        void set_render_textures(std::vector<RaylibRenderTextureRecord>* records) noexcept
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
            RaylibRenderTextureRecord* record = resolve(render_target);
            if (!record)
                return;

            epochnamespace::raylib_api::begin_texture_mode(record->target);
            m_render_pass_open = true;

            if (pass.clear_color)
            {
                epochnamespace::raylib_api::clear_background(
                    epochnamespace::raylib_api::Color{
                        to_channel(pass.clear[0]),
                        to_channel(pass.clear[1]),
                        to_channel(pass.clear[2]),
                        to_channel(pass.clear[3]) });
            }
        }

        void end_render_pass() override
        {
            if (!m_render_pass_open)
                return;

            epochnamespace::raylib_api::end_texture_mode();
            m_render_pass_open = false;
        }

        void draw_model(ModelHandle model) override
        {
            if (model)
                epochnamespace::raylib_api::draw_model(static_cast<int>(model.value - 1));
        }

    private:
        [[nodiscard]] RaylibRenderTextureRecord* resolve(RenderTargetHandle render_target) noexcept
        {
            if (!m_render_textures || !render_target)
                return nullptr;

            const u32 index = render_target.value - 1u;
            if (index >= m_render_textures->size())
                return nullptr;

            RaylibRenderTextureRecord& record = (*m_render_textures)[index];
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

        std::vector<RaylibRenderTextureRecord>* m_render_textures = nullptr;
        CommandResourceBindings m_bindings{};
        BindingSetHandle m_binding_set{};
        bool m_render_pass_open = false;
    };

    class RaylibRenderDevice final : public IRenderDevice
    {
    public:
        RaylibRenderDevice()
        {
            m_context.set_render_textures(&m_render_textures);
        }

        std::string backend_name() const override { return "raylib"; }

        RendererCapabilities capabilities() const noexcept override
        {
            RendererCapabilities caps = renderer_capabilities_for(RendererBackendKind::raylib3);
            caps.native_sampled_render_targets = runtime_renderer_available();
            caps.model_resources = true;
            caps.model_import_ready = true;
            return caps;
        }

        BufferHandle create_buffer(const BufferDesc&) override { return {}; }
        TextureHandle create_texture(const TextureDesc&) override { return {}; }
        SamplerHandle create_sampler(const SamplerDesc&) override { return {}; }
        ShaderHandle create_shader(const ShaderDesc&) override { return {}; }
        PipelineHandle create_pipeline(const PipelineDesc&) override { return {}; }
        MaterialHandle create_material(const MaterialDesc&) override { return {}; }
        RenderTargetHandle create_render_target(const RenderTargetDesc&) override { return {}; }

        BindingSetHandle create_binding_set(const CommandResourceBindings& bindings) override
        {
            const u32 slot = allocate_binding_set_slot();
            RaylibBindingSetRecord& record = m_binding_sets[slot];
            record.bindings = bindings;
            record.active = true;
            return BindingSetHandle{ slot + 1u };
        }

        MeshHandle create_mesh(const MeshDesc&) override { return {}; }

        ModelHandle create_model(const ModelDesc& desc) override
        {
            const char* path = desc.source_path ? desc.source_path : desc.name;
            const int model_id = epochnamespace::raylib_api::load_model(path);
            if (model_id < 0)
                return {};

            return ModelHandle{ static_cast<u32>(model_id + 1) };
        }

        RenderTextureAssetHandles create_render_texture_asset(const RenderTextureAssetDesc& desc) override
        {
            if (!runtime_renderer_available())
                return {};

            const u32 width = desc.width == 0 ? 1u : desc.width;
            const u32 height = desc.height == 0 ? 1u : desc.height;
            const epochnamespace::raylib_api::RenderTexture2D target =
                epochnamespace::raylib_api::load_render_texture(static_cast<int>(width), static_cast<int>(height));

            if (target.id == 0 || target.texture.id == 0)
                return {};

            const u32 slot = allocate_render_texture_slot();
            RaylibRenderTextureRecord& record = m_render_textures[slot];
            record.target = target;
            record.width = width;
            record.height = height;
            record.active = true;

            const u32 handle_value = slot + 1u;
            return RenderTextureAssetHandles{
                TextureHandle{ handle_value },
                SamplerHandle{ handle_value },
                RenderTargetHandle{ handle_value } };
        }

        void destroy(BufferHandle) noexcept override {}
        void destroy(TextureHandle) noexcept override {}
        void destroy(SamplerHandle) noexcept override {}
        void destroy(ShaderHandle) noexcept override {}
        void destroy(PipelineHandle) noexcept override {}
        void destroy(MaterialHandle) noexcept override {}
        void destroy(RenderTargetHandle) noexcept override {}

        void destroy(BindingSetHandle binding_set) noexcept override
        {
            if (!binding_set)
                return;

            const u32 index = binding_set.value - 1u;
            if (index < m_binding_sets.size())
                m_binding_sets[index] = {};
        }

        void destroy(MeshHandle) noexcept override {}

        void destroy(ModelHandle model) noexcept override
        {
            if (model)
                epochnamespace::raylib_api::unload_model(static_cast<int>(model.value - 1));
        }

        void destroy(RenderTextureAssetHandles handles) noexcept override
        {
            if (!handles.render_target)
                return;

            const u32 index = handles.render_target.value - 1u;
            if (index >= m_render_textures.size())
                return;

            RaylibRenderTextureRecord& record = m_render_textures[index];
            if (!record.active)
                return;

            if (runtime_renderer_available())
                epochnamespace::raylib_api::unload_render_texture(record.target);

            record = {};
        }

        [[nodiscard]] const RaylibRenderTextureRecord* resolve_render_texture(RenderTargetHandle handle) const noexcept
        {
            if (!handle)
                return nullptr;

            const u32 index = handle.value - 1u;
            if (index >= m_render_textures.size())
                return nullptr;

            const RaylibRenderTextureRecord& record = m_render_textures[index];
            return record.active ? &record : nullptr;
        }

        [[nodiscard]] u32 render_texture_count() const noexcept
        {
            u32 count = 0;
            for (const RaylibRenderTextureRecord& record : m_render_textures)
            {
                if (record.active)
                    ++count;
            }
            return count;
        }

        ICommandContext& acquire_graphics_context() override { return m_context; }
        CommandListHandle begin_command_list(const char*) override { return {}; }
        void end_command_list(CommandListHandle) override {}
        void present(ISwapchain&) override {}

        [[nodiscard]] bool runtime_renderer_available() const noexcept
        {
            const auto& state = epochnamespace::raylibstate::s_raylibstate;
            return state.running && state.renderingActive;
        }

    private:
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

        RaylibCommandContext m_context{};
        std::vector<RaylibRenderTextureRecord> m_render_textures{};
        std::vector<RaylibBindingSetRecord> m_binding_sets{};
    };
#endif
}
