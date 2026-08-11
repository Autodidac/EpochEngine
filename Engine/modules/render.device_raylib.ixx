/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include "../include/engine.config.hpp"
#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

export module render.device_raylib;

import render.device;
import core.context;
import context.type;

#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
import raylib.api;
import raylib.state;
#if defined(_WIN32)
import raylib.context_win;
#else
import raylib.context_linux;
#endif
#endif

export namespace epochengine
{
#if defined(EPOCH_USING_RAYLIB) && (EPOCH_USING_RAYLIB == 1)
    struct RaylibRenderTextureRecord
    {
        epochengine::raylib_api::RenderTexture2D target{};
        u32 width = 0;
        u32 height = 0;
        bool active = false;
    };

    struct RaylibBindingSetRecord
    {
        CommandResourceBindings bindings{};
        bool active = false;
    };

    struct RaylibSlotRecord
    {
        bool active = false;
    };

    struct RaylibTextureRecord
    {
        TextureDesc desc{};
        epochengine::raylib_api::Texture2D texture{};
        bool active = false;
        bool pending_destroy = false;

        [[nodiscard]] bool ready() const noexcept
        {
            return active && texture.id != 0u;
        }
    };

    struct RaylibMeshRecord
    {
        MeshDesc desc{};
        bool active = false;
    };

    struct RaylibModelRecord
    {
        ModelDesc desc{};
        int live_model_id = -1;
        bool active = false;
    };

    class RaylibCommandContext final : public ICommandContext
    {
    public:
        void set_render_textures(std::vector<RaylibRenderTextureRecord>* records) noexcept
        {
            m_render_textures = records;
        }

        void set_models(std::vector<RaylibModelRecord>* records) noexcept
        {
            m_models = records;
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

            epochengine::raylib_api::begin_texture_mode(record->target);
            m_render_pass_open = true;

            if (pass.clear_color)
            {
                epochengine::raylib_api::clear_background(
                    epochengine::raylib_api::Color{
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

            epochengine::raylib_api::end_texture_mode();
            m_render_pass_open = false;
        }

        void draw_model(ModelHandle model) override
        {
            m_last_model = model;

            const RaylibModelRecord* const record = resolve(model);
            if (record && record->live_model_id >= 0)
                epochengine::raylib_api::draw_model(record->live_model_id);
        }

        [[nodiscard]] BindingSetHandle bound_binding_set() const noexcept { return m_binding_set; }
        [[nodiscard]] const CommandResourceBindings& bound_resources() const noexcept { return m_bindings; }
        [[nodiscard]] ModelHandle last_model() const noexcept { return m_last_model; }

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

        [[nodiscard]] const RaylibModelRecord* resolve(ModelHandle model) const noexcept
        {
            if (!m_models || !model)
                return nullptr;

            const u32 index = model.value - 1u;
            if (index >= m_models->size())
                return nullptr;

            const RaylibModelRecord& record = (*m_models)[index];
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
        std::vector<RaylibModelRecord>* m_models = nullptr;
        CommandResourceBindings m_bindings{};
        BindingSetHandle m_binding_set{};
        ModelHandle m_last_model{};
        bool m_render_pass_open = false;
    };

    class RaylibRenderDevice final : public IRenderDevice
    {
    public:
        RaylibRenderDevice()
        {
            m_context.set_render_textures(&m_render_textures);
            m_context.set_models(&m_models);
        }

        ~RaylibRenderDevice() override
        {
            flush_pending_texture_destroys();
            for (u32 index = 0; index < static_cast<u32>(m_textures.size()); ++index)
            {
                if (m_textures[index].active)
                    destroy(TextureHandle{index + 1u});
            }
        }

        std::string backend_name() const override { return "raylib"; }

        RendererCapabilities capabilities() const noexcept override
        {
            RendererCapabilities caps = renderer_capabilities_for(RendererBackendKind::raylib3);
            caps.sampled_rtt_hook_ready = true;
            caps.sampled_rtt_live_allocation_ready = runtime_renderer_available();
            caps.sampled_rtt_presentation_proven = false;
            caps.native_sampled_render_targets = caps.sampled_rtt_live_allocation_ready;
            caps.mesh_resources = true;
            caps.model_resources = true;
            caps.model_import_ready = true;
            return caps;
        }

        BufferHandle create_buffer(const BufferDesc&) override { return BufferHandle{ allocate_slot(m_buffers) }; }

        TextureHandle create_texture(const TextureDesc& desc) override
        {
            flush_pending_texture_destroys();
            constexpr u32 maximumDimension = (std::min)(
                static_cast<u32>((std::numeric_limits<int>::max)()),
                (std::numeric_limits<u32>::max)() / 4u);
            if (desc.width == 0u || desc.height == 0u
                || desc.width > maximumDimension || desc.height > maximumDimension
                || desc.mip_levels != 1u
                || desc.format != TextureFormat::rgba8_unorm
                || !desc.sampled || desc.storage || desc.render_target
                || desc.depth_stencil || desc.sparse)
            {
                return {};
            }

            const u32 slot = allocate_texture_slot();
            RaylibTextureRecord& record = m_textures[slot];
            record.desc = desc;
            record.texture = {};
            record.active = true;
            return TextureHandle{slot + 1u};
        }

        bool upload_texture(
            TextureHandle handle,
            const TextureUploadDesc& upload) override
        {
            flush_pending_texture_destroys();
            RaylibTextureRecord* const record = resolve_texture_mutable(handle);
            if (!record || !native_resource_context_available()
                || !epochengine::valid(upload)
                || upload.mip_level != 0u
                || upload.x != 0u || upload.y != 0u
                || upload.width != record->desc.width
                || upload.height != record->desc.height
                || upload.format != record->desc.format)
            {
                return false;
            }

            constexpr u32 bytesPerPixel = 4u;
            const u32 tightRowBytes = upload.width * bytesPerPixel;
            const u32 sourceRowBytes = upload.row_pitch_bytes == 0u
                ? tightRowBytes
                : upload.row_pitch_bytes;
            if (sourceRowBytes < tightRowBytes)
                return false;

            const auto* source = static_cast<const std::uint8_t*>(upload.data);
            std::vector<std::uint8_t> tightPixels{};
            const void* imagePixels = source;
            if (sourceRowBytes != tightRowBytes)
            {
                tightPixels.resize(
                    static_cast<std::size_t>(tightRowBytes) * upload.height);
                for (u32 row = 0; row < upload.height; ++row)
                {
                    std::memcpy(
                        tightPixels.data() + static_cast<std::size_t>(row) * tightRowBytes,
                        source + static_cast<std::size_t>(row) * sourceRowBytes,
                        tightRowBytes);
                }
                imagePixels = tightPixels.data();
            }

            epochengine::raylib_api::Image image{};
            image.data = const_cast<void*>(imagePixels);
            image.width = static_cast<int>(upload.width);
            image.height = static_cast<int>(upload.height);
            image.mipmaps = 1;
            image.format = epochengine::raylib_api::pixelformat_rgba8;
            const epochengine::raylib_api::Texture2D uploaded =
                epochengine::raylib_api::load_texture_from_image(image);
            if (uploaded.id == 0u)
                return false;

            const epochengine::raylib_api::Texture2D retired = record->texture;
            record->texture = uploaded;
            if (retired.id != 0u)
                epochengine::raylib_api::unload_texture(retired);
            return true;
        }

        bool texture_ready(TextureHandle handle) const noexcept override
        {
            const RaylibTextureRecord* const record = resolve_texture(handle);
            return record && record->ready() && native_resource_context_available();
        }

        SamplerHandle create_sampler(const SamplerDesc&) override { return SamplerHandle{ allocate_slot(m_samplers) }; }
        ShaderHandle create_shader(const ShaderDesc&) override { return ShaderHandle{ allocate_slot(m_shaders) }; }
        PipelineHandle create_pipeline(const PipelineDesc&) override { return PipelineHandle{ allocate_slot(m_pipelines) }; }
        MaterialHandle create_material(const MaterialDesc&) override { return MaterialHandle{ allocate_slot(m_materials) }; }
        RenderTargetHandle create_render_target(const RenderTargetDesc&) override { return RenderTargetHandle{ allocate_slot(m_render_targets) }; }

        BindingSetHandle create_binding_set(const CommandResourceBindings& bindings) override
        {
            const u32 slot = allocate_binding_set_slot();
            RaylibBindingSetRecord& record = m_binding_sets[slot];
            record.bindings = bindings;
            record.active = true;
            return BindingSetHandle{ slot + 1u };
        }

        MeshHandle create_mesh(const MeshDesc& desc) override
        {
            const u32 slot = allocate_mesh_slot();
            RaylibMeshRecord& record = m_meshes[slot];
            record.desc = desc;
            record.active = true;
            return MeshHandle{ slot + 1u };
        }

        ModelHandle create_model(const ModelDesc& desc) override
        {
            int liveModelId = -1;
            if (runtime_renderer_available() && desc.source_path && desc.source_path[0] != '\0')
                liveModelId = epochengine::raylib_api::load_model(desc.source_path);

            const u32 slot = allocate_model_slot();
            RaylibModelRecord& record = m_models[slot];
            record.desc = desc;
            record.live_model_id = liveModelId;
            record.active = true;
            return ModelHandle{ slot + 1u };
        }

        RenderTextureAssetHandles create_render_texture_asset(const RenderTextureAssetDesc& desc) override
        {
            if (!runtime_renderer_available())
                return {};

            const u32 width = desc.width == 0 ? 1u : desc.width;
            const u32 height = desc.height == 0 ? 1u : desc.height;
            const epochengine::raylib_api::RenderTexture2D target =
                epochengine::raylib_api::load_render_texture(static_cast<int>(width), static_cast<int>(height));

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

        void destroy(BufferHandle handle) noexcept override { release_slot(m_buffers, handle.value); }

        void destroy(TextureHandle handle) noexcept override
        {
            RaylibTextureRecord* const record = resolve_texture_mutable(handle);
            if (!record)
                return;

            if (record->texture.id != 0u)
            {
                if (!native_resource_context_available())
                {
                    record->pending_destroy = true;
                    return;
                }
                epochengine::raylib_api::unload_texture(record->texture);
            }
            *record = {};
        }

        void destroy(SamplerHandle handle) noexcept override { release_slot(m_samplers, handle.value); }
        void destroy(ShaderHandle handle) noexcept override { release_slot(m_shaders, handle.value); }
        void destroy(PipelineHandle handle) noexcept override { release_slot(m_pipelines, handle.value); }
        void destroy(MaterialHandle handle) noexcept override { release_slot(m_materials, handle.value); }
        void destroy(RenderTargetHandle handle) noexcept override { release_slot(m_render_targets, handle.value); }

        void destroy(BindingSetHandle binding_set) noexcept override
        {
            if (!binding_set)
                return;

            const u32 index = binding_set.value - 1u;
            if (index < m_binding_sets.size())
                m_binding_sets[index] = {};
        }

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
            if (index >= m_models.size())
                return;

            RaylibModelRecord& record = m_models[index];
            if (record.live_model_id >= 0)
                epochengine::raylib_api::unload_model(record.live_model_id);

            record = {};
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
                epochengine::raylib_api::unload_render_texture(record.target);

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

        [[nodiscard]] const RaylibModelRecord* resolve_model(ModelHandle model) const noexcept
        {
            if (!model)
                return nullptr;

            const u32 index = model.value - 1u;
            if (index >= m_models.size())
                return nullptr;

            const RaylibModelRecord& record = m_models[index];
            return record.active ? &record : nullptr;
        }

        ICommandContext& acquire_graphics_context() override { return m_context; }
        CommandListHandle begin_command_list(const char*) override { return {}; }
        void end_command_list(CommandListHandle) override {}
        void present(ISwapchain&) override {}

        [[nodiscard]] bool runtime_renderer_available() const noexcept
        {
            const auto& state = epochengine::raylibstate::s_raylibstate;
            return state.running && state.renderingActive;
        }

        [[nodiscard]] bool native_resource_context_available(
            const core::Context* expected = nullptr) const noexcept
        {
            const auto& state = epochengine::raylibstate::s_raylibstate;
            if (!state.renderingActive || !state.owner_ctx
                || !epochengine::raylib_api::is_window_ready()
                || (expected && expected != state.owner_ctx))
            {
                return false;
            }

            const auto current = core::get_current_render_context();
            if (!current || current.get() != state.owner_ctx
                || current->type != core::ContextType::RayLib)
            {
                return false;
            }

#if defined(_WIN32)
            return epochengine::raylibcontext::win::native_context_is_current();
#else
            return epochengine::raylibcontext::linux::native_context_is_current();
#endif
        }

        [[nodiscard]] bool native_presentation_context_available(
            const core::Context* expected) const noexcept
        {
            const auto& state = epochengine::raylibstate::s_raylibstate;
            return native_resource_context_available(expected)
                && state.running && state.frameActive && !state.frameInTextureMode;
        }

        [[nodiscard]] const RaylibTextureRecord* resolve_texture(
            TextureHandle handle) const noexcept
        {
            if (!handle || handle.value > m_textures.size())
                return nullptr;
            const RaylibTextureRecord& record = m_textures[handle.value - 1u];
            return record.active && !record.pending_destroy ? &record : nullptr;
        }

    private:
        void flush_pending_texture_destroys() noexcept
        {
            if (!native_resource_context_available())
                return;

            for (RaylibTextureRecord& record : m_textures)
            {
                if (!record.active || !record.pending_destroy)
                    continue;
                if (record.texture.id != 0u)
                    epochengine::raylib_api::unload_texture(record.texture);
                record = {};
            }
        }

        [[nodiscard]] static u32 allocate_slot(std::vector<RaylibSlotRecord>& records)
        {
            for (u32 i = 0; i < static_cast<u32>(records.size()); ++i)
            {
                if (!records[i].active)
                {
                    records[i].active = true;
                    return i + 1u;
                }
            }

            records.push_back(RaylibSlotRecord{ true });
            return static_cast<u32>(records.size());
        }

        static void release_slot(std::vector<RaylibSlotRecord>& records, u32 handle_value) noexcept
        {
            if (handle_value == 0u)
                return;

            const u32 index = handle_value - 1u;
            if (index < records.size())
                records[index] = {};
        }

        [[nodiscard]] u32 allocate_texture_slot()
        {
            for (u32 i = 0; i < static_cast<u32>(m_textures.size()); ++i)
            {
                if (!m_textures[i].active)
                    return i;
            }

            m_textures.push_back({});
            return static_cast<u32>(m_textures.size() - 1u);
        }

        [[nodiscard]] RaylibTextureRecord* resolve_texture_mutable(
            TextureHandle handle) noexcept
        {
            if (!handle || handle.value > m_textures.size())
                return nullptr;
            RaylibTextureRecord& record = m_textures[handle.value - 1u];
            return record.active && !record.pending_destroy ? &record : nullptr;
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
        std::vector<RaylibSlotRecord> m_buffers{};
        std::vector<RaylibTextureRecord> m_textures{};
        std::vector<RaylibSlotRecord> m_samplers{};
        std::vector<RaylibSlotRecord> m_shaders{};
        std::vector<RaylibSlotRecord> m_pipelines{};
        std::vector<RaylibSlotRecord> m_materials{};
        std::vector<RaylibSlotRecord> m_render_targets{};
        std::vector<RaylibRenderTextureRecord> m_render_textures{};
        std::vector<RaylibMeshRecord> m_meshes{};
        std::vector<RaylibModelRecord> m_models{};
        std::vector<RaylibBindingSetRecord> m_binding_sets{};
    };
#endif
}
