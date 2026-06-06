/************************************************
 *  Epoch Engine - SFML Renderer Device Module
 *
 *  SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ***********************************************/
module;

#include "../include/engine.config.hpp"
#include "../include/epoch.config.hpp"
#include "../include/epoch.common.hpp"
#include <memory>
#include <string>
#include <vector>

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
#define SFML_STATIC
#include <SFML/Graphics.hpp>
#endif

export module render.device_sfml;

import render.device;

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import sfml.state;
#endif

export namespace epoch
{
#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
    struct SfmlRenderTextureRecord
    {
        RenderTextureAssetDesc desc{};
        std::unique_ptr<sf::RenderTexture> target{};
        u32 width = 0;
        u32 height = 0;
        bool active = false;
    };

    struct SfmlBindingSetRecord
    {
        CommandResourceBindings bindings{};
        bool active = false;
    };

    class SfmlCommandContext final : public ICommandContext
    {
    public:
        void set_render_textures(std::vector<SfmlRenderTextureRecord>* records) noexcept
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
            SfmlRenderTextureRecord* const record = resolve(render_target);
            if (!record || !record->target)
                return;

            if (sf::RenderWindow* const window = active_window())
                window->setActive(false);

            record->target->setActive(true);
            m_render_pass_open = true;
            m_render_target = render_target;
            m_last_width = record->width;
            m_last_height = record->height;

            if (pass.clear_color)
            {
                record->target->clear(sf::Color{
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

            SfmlRenderTextureRecord* const record = resolve(m_render_target);
            if (record && record->target)
            {
                record->target->display();
                record->target->setActive(false);
            }

            if (sf::RenderWindow* const window = active_window())
                window->setActive(true);

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
        [[nodiscard]] static sf::RenderWindow* active_window() noexcept
        {
            return epochnamespace::sfmlcontext::state::s_sfmlstate.get_sfml_window();
        }

        [[nodiscard]] SfmlRenderTextureRecord* resolve(RenderTargetHandle render_target) noexcept
        {
            if (!m_render_textures || !render_target)
                return nullptr;

            const u32 index = render_target.value - 1u;
            if (index >= m_render_textures->size())
                return nullptr;

            SfmlRenderTextureRecord& record = (*m_render_textures)[index];
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

        std::vector<SfmlRenderTextureRecord>* m_render_textures = nullptr;
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

    class SfmlRenderDevice final : public IRenderDevice
    {
    public:
        SfmlRenderDevice()
        {
            m_context.set_render_textures(&m_render_textures);
        }

        std::string backend_name() const override { return "sfml3"; }

        RendererCapabilities capabilities() const noexcept override
        {
            RendererCapabilities caps = renderer_capabilities_for(RendererBackendKind::sfml3);
            caps.native_sampled_render_targets = true;
            caps.model_resources = true;
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
            SfmlBindingSetRecord& record = m_binding_sets[slot];
            record.bindings = bindings;
            record.active = true;
            return BindingSetHandle{ slot + 1u };
        }

        MeshHandle create_mesh(const MeshDesc&) override { return {}; }
        ModelHandle create_model(const ModelDesc&) override { return {}; }

        RenderTextureAssetHandles create_render_texture_asset(const RenderTextureAssetDesc& desc) override
        {
            if (!runtime_renderer_available())
                return {};

            const u32 width = desc.width == 0 ? 1u : desc.width;
            const u32 height = desc.height == 0 ? 1u : desc.height;

            auto target = std::make_unique<sf::RenderTexture>();
            if (!target->create(width, height))
                return {};

            const u32 slot = allocate_render_texture_slot();
            SfmlRenderTextureRecord& record = m_render_textures[slot];
            record.desc = desc;
            record.target = std::move(target);
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
        void destroy(MeshHandle) noexcept override {}
        void destroy(ModelHandle) noexcept override {}

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

            SfmlRenderTextureRecord& record = m_render_textures[index];
            if (!record.active)
                return;

            record = {};
        }

        ICommandContext& acquire_graphics_context() override { return m_context; }
        CommandListHandle begin_command_list(const char*) override { return {}; }
        void end_command_list(CommandListHandle) override {}
        void present(ISwapchain&) override {}

        [[nodiscard]] bool runtime_renderer_available() const noexcept
        {
            return epochnamespace::sfmlcontext::state::s_sfmlstate.get_sfml_window() != nullptr;
        }

        [[nodiscard]] const SfmlRenderTextureRecord* resolve_render_texture(RenderTargetHandle handle) const noexcept
        {
            if (!handle)
                return nullptr;

            const u32 index = handle.value - 1u;
            if (index >= m_render_textures.size())
                return nullptr;

            const SfmlRenderTextureRecord& record = m_render_textures[index];
            return record.active ? &record : nullptr;
        }

        [[nodiscard]] u32 render_texture_count() const noexcept
        {
            u32 count = 0;
            for (const SfmlRenderTextureRecord& record : m_render_textures)
            {
                if (record.active)
                    ++count;
            }
            return count;
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

        SfmlCommandContext m_context{};
        std::vector<SfmlRenderTextureRecord> m_render_textures{};
        std::vector<SfmlBindingSetRecord> m_binding_sets{};
    };
#endif
}
