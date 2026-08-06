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
#include <array>
#include <string>
#include <vector>

export module render.device_opengl_family;

import render.device;

export namespace epochengine
{
    struct OpenGLFamilyRenderTextureRecord
    {
        RenderTextureAssetDesc desc{};
        RenderTextureBackendRequirements backend_requirements{};
        u32 width = 0;
        u32 height = 0;
        u32 color_object = 0;
        u32 depth_object = 0;
        u32 framebuffer_object = 0;
        u32 sampler_object = 0;
        bool native_allocation_ready = false;
        bool active = false;

        [[nodiscard]] bool native_work_order_ready() const noexcept
        {
            return active
                && backend_requirements.color_attachment
                && backend_requirements.sampled_color
                && backend_requirements.sampler
                && backend_requirements.offscreen_target
                && backend_requirements.presentable_surface
                && color_object != 0u
                && sampler_object != 0u
                && framebuffer_object != 0u
                && (!backend_requirements.depth_attachment || depth_object != 0u);
        }
    };

    struct OpenGLFamilyNativeRenderTextureAllocation
    {
        u32 framebuffer_object = 0;
        u32 color_object = 0;
        u32 depth_object = 0;
        u32 sampler_object = 0;
        bool ready = false;
    };

    struct OpenGLFamilyNativeRenderTextureHooks
    {
        using AllocateFn = OpenGLFamilyNativeRenderTextureAllocation (*)(
            void* user,
            RendererBackendKind backend,
            const RenderTextureAssetDesc& desc,
            const RenderTextureBackendRequirements& requirements,
            u32 slot);
        using DestroyFn = void (*)(
            void* user,
            RendererBackendKind backend,
            const OpenGLFamilyRenderTextureRecord& record);
        using BeginPassFn = bool (*)(
            void* user,
            RendererBackendKind backend,
            const OpenGLFamilyRenderTextureRecord& record,
            const RenderPassDesc& pass);
        using EndPassFn = void (*)(
            void* user,
            RendererBackendKind backend,
            const OpenGLFamilyRenderTextureRecord& record);

        void* user = nullptr;
        AllocateFn allocate = nullptr;
        DestroyFn destroy = nullptr;
        BeginPassFn begin_pass = nullptr;
        EndPassFn end_pass = nullptr;

        [[nodiscard]] bool ready() const noexcept
        {
            return allocate != nullptr && destroy != nullptr && begin_pass != nullptr && end_pass != nullptr;
        }
    };

    struct OpenGLFamilyTextureRecord
    {
        TextureDesc desc{};
        u32 texture_object = 0;
        const void* native_context_key = nullptr;
        bool native_allocation_ready = false;
        bool active = false;

        [[nodiscard]] bool native_work_order_ready() const noexcept
        {
            return active && native_allocation_ready && texture_object != 0u;
        }
    };

    struct OpenGLFamilyNativeTextureAllocation
    {
        u32 texture_object = 0;
        bool ready = false;
        const void* context_key = nullptr;
    };

    struct OpenGLFamilyNativeTextureHooks
    {
        using AllocateFn = OpenGLFamilyNativeTextureAllocation (*)(
            void* user,
            RendererBackendKind backend,
            const TextureDesc& desc,
            u32 slot);
        using UploadFn = bool (*)(
            void* user,
            RendererBackendKind backend,
            const OpenGLFamilyTextureRecord& record,
            const TextureUploadDesc& upload);
        using DestroyFn = void (*)(
            void* user,
            RendererBackendKind backend,
            const OpenGLFamilyTextureRecord& record);

        void* user = nullptr;
        AllocateFn allocate = nullptr;
        UploadFn upload = nullptr;
        DestroyFn destroy = nullptr;

        [[nodiscard]] bool ready() const noexcept
        {
            return allocate != nullptr && upload != nullptr && destroy != nullptr;
        }
    };

    struct OpenGLFamilyBindingSetRecord
    {
        CommandResourceBindings bindings{};
        bool active = false;
    };

    struct OpenGLFamilyRenderPassRecord
    {
        RenderTargetHandle render_target{};
        RenderPassDesc desc{};
        bool open = false;
    };

    struct OpenGLFamilyMeshRecord
    {
        MeshDesc desc{};
        bool active = false;
    };

    struct OpenGLFamilyModelRecord
    {
        ModelDesc desc{};
        bool active = false;
    };

    class OpenGLFamilyCommandContext final : public ICommandContext
    {
    public:
        void set_render_textures(std::vector<OpenGLFamilyRenderTextureRecord>* records) noexcept
        {
            m_render_textures = records;
        }

        void set_native_render_texture_hooks(
            RendererBackendKind backend,
            const OpenGLFamilyNativeRenderTextureHooks* hooks) noexcept
        {
            m_backend = backend;
            m_native_hooks = hooks;
        }

        void begin(const char*) override
        {
            m_open = true;
        }

        void end() override
        {
            m_render_pass = {};
            m_open = false;
        }

        void debug_marker(const char*) override {}
        void bind_binding_set(BindingSetHandle binding_set) override { m_binding_set = binding_set; }
        void bind_resources(const CommandResourceBindings& bindings) override { m_bindings = bindings; }
        void barrier() override {}
        void draw_mesh(MeshHandle mesh, MaterialHandle material = {}) override
        {
            m_last_mesh = mesh;
            m_last_material = material;
        }

        void draw_model(ModelHandle model) override
        {
            m_last_model = model;
        }

        void begin_render_pass(RenderTargetHandle render_target, const RenderPassDesc& pass) override
        {
            OpenGLFamilyRenderTextureRecord* record = resolve(render_target);
            if (!record)
                return;

            m_render_pass.render_target = render_target;
            m_render_pass.desc = pass;
            m_render_pass.open = true;
            m_last_render_target = render_target;
            m_last_width = record->width;
            m_last_height = record->height;
            m_native_pass_bound = false;

            if (record->native_allocation_ready && m_native_hooks && m_native_hooks->begin_pass)
                m_native_pass_bound = m_native_hooks->begin_pass(m_native_hooks->user, m_backend, *record, pass);
        }

        void end_render_pass() override
        {
            OpenGLFamilyRenderTextureRecord* record = resolve(m_render_pass.render_target);
            if (record && m_native_pass_bound && m_native_hooks && m_native_hooks->end_pass)
                m_native_hooks->end_pass(m_native_hooks->user, m_backend, *record);

            m_native_pass_bound = false;
            m_render_pass.open = false;
        }

        [[nodiscard]] bool active() const noexcept { return m_open; }
        [[nodiscard]] bool render_pass_open() const noexcept { return m_render_pass.open; }
        [[nodiscard]] BindingSetHandle bound_binding_set() const noexcept { return m_binding_set; }
        [[nodiscard]] RenderTargetHandle last_render_target() const noexcept { return m_last_render_target; }
        [[nodiscard]] const CommandResourceBindings& bound_resources() const noexcept { return m_bindings; }
        [[nodiscard]] bool native_pass_bound() const noexcept { return m_native_pass_bound; }
        [[nodiscard]] u32 last_width() const noexcept { return m_last_width; }
        [[nodiscard]] u32 last_height() const noexcept { return m_last_height; }
        [[nodiscard]] MeshHandle last_mesh() const noexcept { return m_last_mesh; }
        [[nodiscard]] MaterialHandle last_material() const noexcept { return m_last_material; }
        [[nodiscard]] ModelHandle last_model() const noexcept { return m_last_model; }

    private:
        [[nodiscard]] OpenGLFamilyRenderTextureRecord* resolve(RenderTargetHandle render_target) noexcept
        {
            if (!m_render_textures || !render_target)
                return nullptr;

            const u32 index = render_target.value - 1u;
            if (index >= m_render_textures->size())
                return nullptr;

            OpenGLFamilyRenderTextureRecord& record = (*m_render_textures)[index];
            return record.active ? &record : nullptr;
        }

        std::vector<OpenGLFamilyRenderTextureRecord>* m_render_textures = nullptr;
        const OpenGLFamilyNativeRenderTextureHooks* m_native_hooks = nullptr;
        CommandResourceBindings m_bindings{};
        BindingSetHandle m_binding_set{};
        OpenGLFamilyRenderPassRecord m_render_pass{};
        RenderTargetHandle m_last_render_target{};
        MeshHandle m_last_mesh{};
        MaterialHandle m_last_material{};
        ModelHandle m_last_model{};
        u32 m_last_width = 0;
        u32 m_last_height = 0;
        RendererBackendKind m_backend = RendererBackendKind::opengl;
        bool m_native_pass_bound = false;
        bool m_open = false;
    };

    class OpenGLFamilyRenderDevice final : public IRenderDevice
    {
    public:
        explicit OpenGLFamilyRenderDevice(RendererBackendKind backend = RendererBackendKind::opengl) noexcept
            : m_backend(normalize_backend(backend))
        {
            m_context.set_render_textures(&m_render_textures);
            m_context.set_native_render_texture_hooks(m_backend, &m_native_hooks);
        }

        ~OpenGLFamilyRenderDevice() override
        {
            for (u32 index = 0; index < static_cast<u32>(m_textures.size()); ++index)
            {
                if (m_textures[index].active)
                    destroy(TextureHandle{index + 1u});
            }
            for (u32 index = 0; index < static_cast<u32>(m_render_textures.size()); ++index)
            {
                if (!m_render_textures[index].active)
                    continue;
                const u32 value = index + 1u;
                destroy(RenderTextureAssetHandles{
                    TextureHandle{value},
                    SamplerHandle{value},
                    RenderTargetHandle{value} });
            }
        }

        std::string backend_name() const override
        {
            switch (m_backend)
            {
            case RendererBackendKind::sdl3:
                return "sdl3-opengl";
            case RendererBackendKind::sfml3:
                return "sfml3-opengl";
            case RendererBackendKind::raylib3:
                return "raylib3-opengl";
            default:
                return "opengl";
            }
        }

        RendererCapabilities capabilities() const noexcept override
        {
            RendererCapabilities caps = renderer_capabilities_for(m_backend);
            caps.buffers = true;
            caps.textures = true;
            caps.samplers = true;
            caps.shaders = true;
            caps.pipelines = true;
            caps.materials = true;
            caps.render_targets = true;
            caps.command_lists = true;
            caps.frame_graph = true;
            caps.render_to_texture = true;
            caps.sampled_render_targets = true;
            caps.sampled_rtt_hook_ready = m_native_hooks.ready();
            caps.sampled_rtt_live_allocation_ready = false;
            caps.sampled_rtt_presentation_proven = false;
            caps.native_sampled_render_targets = caps.sampled_rtt_live_allocation_ready;
            caps.binding_sets = true;
            caps.mesh_resources = true;
            caps.model_resources = true;
            return caps;
        }

        BufferHandle create_buffer(const BufferDesc&) override { return BufferHandle{ allocate_slot(m_buffers) }; }
        TextureHandle create_texture(const TextureDesc& desc) override
        {
            if (desc.width == 0u || desc.height == 0u || desc.mip_levels == 0u
                || desc.mip_levels > 32u
                || texture_format_bytes_per_texel(desc.format) == 0u
                || desc.depth_stencil)
            {
                return {};
            }

            const u32 slot = allocate_texture_slot();
            OpenGLFamilyTextureRecord& record = m_textures[slot];
            record = {};
            record.desc = desc;
            record.active = true;
            record.desc.debug_name = nullptr;

            if (m_native_texture_hooks.allocate)
            {
                const OpenGLFamilyNativeTextureAllocation allocation =
                    m_native_texture_hooks.allocate(
                        m_native_texture_hooks.user,
                        m_backend,
                        desc,
                        slot + 1u);
                if (allocation.ready && allocation.texture_object != 0u)
                {
                    record.texture_object = allocation.texture_object;
                    record.native_context_key = allocation.context_key;
                    record.native_allocation_ready = true;
                }
            }

            return TextureHandle{slot + 1u};
        }

        bool upload_texture(
            TextureHandle handle,
            const TextureUploadDesc& upload) override
        {
            OpenGLFamilyTextureRecord* const record = resolve_texture_mutable(handle);
            if (!record || !record->native_work_order_ready()
                || !m_native_texture_hooks.upload || !epochengine::valid(upload)
                || upload.format != record->desc.format
                || upload.mip_level >= record->desc.mip_levels)
            {
                return false;
            }

            const u32 mipWidth = (std::max)(1u, record->desc.width >> upload.mip_level);
            const u32 mipHeight = (std::max)(1u, record->desc.height >> upload.mip_level);
            if (upload.width > mipWidth || upload.height > mipHeight
                || upload.x > mipWidth - upload.width
                || upload.y > mipHeight - upload.height)
            {
                return false;
            }

            return m_native_texture_hooks.upload(
                m_native_texture_hooks.user,
                m_backend,
                *record,
                upload);
        }

        bool texture_ready(TextureHandle handle) const noexcept override
        {
            const OpenGLFamilyTextureRecord* const record = resolve_texture(handle);
            return record && record->native_work_order_ready();
        }
        SamplerHandle create_sampler(const SamplerDesc&) override { return SamplerHandle{ allocate_slot(m_samplers) }; }
        ShaderHandle create_shader(const ShaderDesc&) override { return ShaderHandle{ allocate_slot(m_shaders) }; }
        PipelineHandle create_pipeline(const PipelineDesc&) override { return PipelineHandle{ allocate_slot(m_pipelines) }; }
        MaterialHandle create_material(const MaterialDesc&) override { return MaterialHandle{ allocate_slot(m_materials) }; }
        RenderTargetHandle create_render_target(const RenderTargetDesc&) override { return RenderTargetHandle{ allocate_slot(m_render_targets) }; }

        BindingSetHandle create_binding_set(const CommandResourceBindings& bindings) override
        {
            const u32 slot = allocate_binding_set_slot();
            OpenGLFamilyBindingSetRecord& record = m_binding_sets[slot];
            record.bindings = bindings;
            record.active = true;
            return BindingSetHandle{ slot + 1u };
        }

        MeshHandle create_mesh(const MeshDesc& desc) override
        {
            const u32 slot = allocate_mesh_slot();
            OpenGLFamilyMeshRecord& record = m_meshes[slot];
            record.desc = desc;
            record.active = true;
            return MeshHandle{ slot + 1u };
        }

        ModelHandle create_model(const ModelDesc& desc) override
        {
            const u32 slot = allocate_model_slot();
            OpenGLFamilyModelRecord& record = m_models[slot];
            record.desc = desc;
            record.active = true;
            return ModelHandle{ slot + 1u };
        }

        RenderTextureAssetHandles create_render_texture_asset(const RenderTextureAssetDesc& desc) override
        {
            const u32 slot = allocate_render_texture_slot();
            OpenGLFamilyRenderTextureRecord& record = m_render_textures[slot];
            const RenderTextureAssetPlan plan = make_render_texture_asset_plan(desc);
            record.desc = desc;
            record.backend_requirements = plan.backend_requirements;
            record.desc.debug_name = nullptr;
            record.width = desc.width == 0u ? 1u : desc.width;
            record.height = desc.height == 0u ? 1u : desc.height;
            record.color_object = slot + 1u;
            record.depth_object = desc.has_depth ? slot + 1u : 0u;
            record.framebuffer_object = slot + 1u;
            record.sampler_object = slot + 1u;
            record.native_allocation_ready = false;
            record.active = true;

            if (m_native_hooks.allocate)
            {
                const OpenGLFamilyNativeRenderTextureAllocation allocation =
                    m_native_hooks.allocate(
                        m_native_hooks.user,
                        m_backend,
                        desc,
                        record.backend_requirements,
                        slot + 1u);
                if (allocation.ready)
                {
                    record.color_object = allocation.color_object;
                    record.depth_object = allocation.depth_object;
                    record.framebuffer_object = allocation.framebuffer_object;
                    record.sampler_object = allocation.sampler_object;
                    record.native_allocation_ready = record.native_work_order_ready();
                }
            }

            const u32 handle_value = slot + 1u;
            return RenderTextureAssetHandles{
                TextureHandle{ handle_value },
                SamplerHandle{ handle_value },
                RenderTargetHandle{ handle_value } };
        }

        void destroy(BufferHandle handle) noexcept override { release_slot(m_buffers, handle.value); }
        void destroy(TextureHandle handle) noexcept override
        {
            if (!handle || handle.value > m_textures.size())
                return;

            OpenGLFamilyTextureRecord& record = m_textures[handle.value - 1u];
            if (!record.active)
                return;
            if (record.native_allocation_ready && m_native_texture_hooks.destroy)
                m_native_texture_hooks.destroy(m_native_texture_hooks.user, m_backend, record);
            record = {};
        }
        void destroy(SamplerHandle handle) noexcept override { release_slot(m_samplers, handle.value); }
        void destroy(ShaderHandle handle) noexcept override { release_slot(m_shaders, handle.value); }
        void destroy(PipelineHandle handle) noexcept override { release_slot(m_pipelines, handle.value); }
        void destroy(MaterialHandle handle) noexcept override { release_slot(m_materials, handle.value); }
        void destroy(RenderTargetHandle handle) noexcept override { release_slot(m_render_targets, handle.value); }
        void destroy(MeshHandle handle) noexcept override
        {
            if (!handle)
                return;

            const u32 index = handle.value - 1u;
            if (index < m_meshes.size())
                m_meshes[index] = {};
        }

        void destroy(ModelHandle handle) noexcept override
        {
            if (!handle)
                return;

            const u32 index = handle.value - 1u;
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
            if (index < m_render_textures.size())
            {
                const OpenGLFamilyRenderTextureRecord& record = m_render_textures[index];
                if (record.active && record.native_allocation_ready && m_native_hooks.destroy)
                    m_native_hooks.destroy(m_native_hooks.user, m_backend, record);

                m_render_textures[index] = {};
            }
        }

        ICommandContext& acquire_graphics_context() override { return m_context; }
        CommandListHandle begin_command_list(const char*) override { return CommandListHandle{ allocate_slot(m_command_lists) }; }
        void end_command_list(CommandListHandle) override {}
        void present(ISwapchain&) override {}

        [[nodiscard]] std::size_t render_texture_count() const noexcept { return m_render_textures.size(); }
        [[nodiscard]] std::size_t mesh_count() const noexcept { return m_meshes.size(); }
        [[nodiscard]] std::size_t model_count() const noexcept { return m_models.size(); }
        [[nodiscard]] RendererBackendKind backend() const noexcept { return m_backend; }
        [[nodiscard]] const OpenGLFamilyCommandContext& graphics_context() const noexcept { return m_context; }
        void set_native_texture_hooks(OpenGLFamilyNativeTextureHooks hooks) noexcept
        {
            m_native_texture_hooks = hooks;
        }

        [[nodiscard]] bool native_texture_hooks_ready() const noexcept
        {
            return m_native_texture_hooks.ready();
        }

        [[nodiscard]] const OpenGLFamilyTextureRecord* resolve_texture(
            TextureHandle handle) const noexcept
        {
            if (!handle || handle.value > m_textures.size())
                return nullptr;
            const OpenGLFamilyTextureRecord& record = m_textures[handle.value - 1u];
            return record.active ? &record : nullptr;
        }

        void set_native_render_texture_hooks(OpenGLFamilyNativeRenderTextureHooks hooks) noexcept
        {
            m_native_hooks = hooks;
            m_context.set_native_render_texture_hooks(m_backend, &m_native_hooks);
        }
        [[nodiscard]] const OpenGLFamilyRenderTextureRecord* resolve_render_texture(RenderTargetHandle handle) const noexcept
        {
            if (!handle)
                return nullptr;

            const u32 index = handle.value - 1u;
            if (index >= m_render_textures.size())
                return nullptr;

            const OpenGLFamilyRenderTextureRecord& record = m_render_textures[index];
            return record.active ? &record : nullptr;
        }

    private:
        struct SlotRecord
        {
            bool active = false;
        };

        [[nodiscard]] static constexpr RendererBackendKind normalize_backend(RendererBackendKind backend) noexcept
        {
            return (backend == RendererBackendKind::sdl3
                || backend == RendererBackendKind::sfml3
                || backend == RendererBackendKind::raylib3)
                ? backend
                : RendererBackendKind::opengl;
        }

        [[nodiscard]] static u32 allocate_slot(std::vector<SlotRecord>& records)
        {
            for (u32 i = 0; i < static_cast<u32>(records.size()); ++i)
            {
                if (!records[i].active)
                {
                    records[i].active = true;
                    return i + 1u;
                }
            }

            records.push_back(SlotRecord{ true });
            return static_cast<u32>(records.size());
        }

        static void release_slot(std::vector<SlotRecord>& records, u32 handle_value) noexcept
        {
            if (handle_value == 0u)
                return;

            const u32 index = handle_value - 1u;
            if (index < records.size())
                records[index] = {};
        }

        [[nodiscard]] OpenGLFamilyTextureRecord* resolve_texture_mutable(
            TextureHandle handle) noexcept
        {
            if (!handle || handle.value > m_textures.size())
                return nullptr;
            OpenGLFamilyTextureRecord& record = m_textures[handle.value - 1u];
            return record.active ? &record : nullptr;
        }

        [[nodiscard]] u32 allocate_texture_slot()
        {
            for (u32 index = 0; index < static_cast<u32>(m_textures.size()); ++index)
            {
                if (!m_textures[index].active)
                    return index;
            }

            m_textures.push_back({});
            return static_cast<u32>(m_textures.size() - 1u);
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

        RendererBackendKind m_backend = RendererBackendKind::opengl;
        OpenGLFamilyNativeRenderTextureHooks m_native_hooks{};
        OpenGLFamilyNativeTextureHooks m_native_texture_hooks{};
        OpenGLFamilyCommandContext m_context{};
        std::vector<OpenGLFamilyRenderTextureRecord> m_render_textures{};
        std::vector<OpenGLFamilyBindingSetRecord> m_binding_sets{};
        std::vector<OpenGLFamilyMeshRecord> m_meshes{};
        std::vector<OpenGLFamilyModelRecord> m_models{};
        std::vector<SlotRecord> m_buffers{};
        std::vector<OpenGLFamilyTextureRecord> m_textures{};
        std::vector<SlotRecord> m_samplers{};
        std::vector<SlotRecord> m_shaders{};
        std::vector<SlotRecord> m_pipelines{};
        std::vector<SlotRecord> m_materials{};
        std::vector<SlotRecord> m_render_targets{};
        std::vector<SlotRecord> m_command_lists{};
    };

    enum class OpenGLFamilyTextureContractFailure : u8
    {
        none,
        hook_readiness,
        logical_without_hooks,
        native_allocation,
        upload,
        bounds_rejection,
        destruction
    };

    [[nodiscard]] constexpr const char* opengl_family_texture_contract_failure_name(
        OpenGLFamilyTextureContractFailure failure) noexcept
    {
        switch (failure)
        {
        case OpenGLFamilyTextureContractFailure::none: return "pass";
        case OpenGLFamilyTextureContractFailure::hook_readiness: return "hook_readiness";
        case OpenGLFamilyTextureContractFailure::logical_without_hooks: return "logical_without_hooks";
        case OpenGLFamilyTextureContractFailure::native_allocation: return "native_allocation";
        case OpenGLFamilyTextureContractFailure::upload: return "upload";
        case OpenGLFamilyTextureContractFailure::bounds_rejection: return "bounds_rejection";
        case OpenGLFamilyTextureContractFailure::destruction: return "destruction";
        }
        return "unknown";
    }

    namespace detail
    {
        struct OpenGLFamilyTextureContractState final
        {
            RendererBackendKind backend{RendererBackendKind::null};
            u32 allocation_count{};
            u32 upload_count{};
            u32 destroy_count{};
            u32 width{};
            u32 height{};
            u64 uploaded_bytes{};
        };

        [[nodiscard]] inline OpenGLFamilyNativeTextureAllocation
            allocate_contract_texture(
                void* user,
                RendererBackendKind backend,
                const TextureDesc& desc,
                u32 slot)
        {
            auto* const state = static_cast<OpenGLFamilyTextureContractState*>(user);
            if (!state || desc.width == 0u || desc.height == 0u
                || texture_format_bytes_per_texel(desc.format) == 0u)
            {
                return {};
            }
            state->backend = backend;
            state->width = desc.width;
            state->height = desc.height;
            ++state->allocation_count;
            return OpenGLFamilyNativeTextureAllocation{5000u + slot, true};
        }

        [[nodiscard]] inline bool upload_contract_texture(
            void* user,
            RendererBackendKind backend,
            const OpenGLFamilyTextureRecord& record,
            const TextureUploadDesc& upload)
        {
            auto* const state = static_cast<OpenGLFamilyTextureContractState*>(user);
            if (!state || backend != state->backend
                || !record.native_work_order_ready()
                || !epochengine::valid(upload))
            {
                return false;
            }
            ++state->upload_count;
            state->uploaded_bytes += minimum_texture_upload_bytes(upload);
            return true;
        }

        inline void destroy_contract_texture(
            void* user,
            RendererBackendKind backend,
            const OpenGLFamilyTextureRecord& record)
        {
            auto* const state = static_cast<OpenGLFamilyTextureContractState*>(user);
            if (state && backend == state->backend && record.native_work_order_ready())
                ++state->destroy_count;
        }
    }

    [[nodiscard]] OpenGLFamilyTextureContractFailure
        opengl_family_texture_runtime_contract_failure()
    {
        constexpr std::array<RendererBackendKind, 4> backends{{
            RendererBackendKind::opengl,
            RendererBackendKind::sdl3,
            RendererBackendKind::sfml3,
            RendererBackendKind::raylib3
        }};
        std::array<u8, 16> pixels{};
        TextureDesc desc{};
        desc.width = 2;
        desc.height = 2;
        desc.mip_levels = 1;
        desc.format = TextureFormat::rgba8_unorm;
        desc.sampled = true;
        desc.debug_name = "OpenGLFamily.TextureContract";

        TextureUploadDesc upload{};
        upload.width = 2;
        upload.height = 2;
        upload.row_pitch_bytes = 8;
        upload.format = TextureFormat::rgba8_unorm;
        upload.data = pixels.data();
        upload.size_bytes = pixels.size();

        for (RendererBackendKind backend : backends)
        {
            OpenGLFamilyRenderDevice logicalDevice{backend};
            const TextureHandle logical = logicalDevice.create_texture(desc);
            if (!logical || logicalDevice.texture_ready(logical)
                || logicalDevice.upload_texture(logical, upload))
            {
                return OpenGLFamilyTextureContractFailure::logical_without_hooks;
            }
            logicalDevice.destroy(logical);
            if (logicalDevice.resolve_texture(logical))
                return OpenGLFamilyTextureContractFailure::logical_without_hooks;

            detail::OpenGLFamilyTextureContractState state{};
            OpenGLFamilyRenderDevice nativeDevice{backend};
            nativeDevice.set_native_texture_hooks(OpenGLFamilyNativeTextureHooks{
                .user = &state,
                .allocate = detail::allocate_contract_texture,
                .upload = detail::upload_contract_texture,
                .destroy = detail::destroy_contract_texture
            });
            if (!nativeDevice.native_texture_hooks_ready())
                return OpenGLFamilyTextureContractFailure::hook_readiness;

            const TextureHandle texture = nativeDevice.create_texture(desc);
            const OpenGLFamilyTextureRecord* const record =
                nativeDevice.resolve_texture(texture);
            if (!texture || !record || !record->native_work_order_ready()
                || record->texture_object == 0u || !nativeDevice.texture_ready(texture)
                || state.allocation_count != 1u || state.width != 2u || state.height != 2u
                || state.backend != backend)
            {
                return OpenGLFamilyTextureContractFailure::native_allocation;
            }

            if (!nativeDevice.upload_texture(texture, upload)
                || state.upload_count != 1u || state.uploaded_bytes != pixels.size())
            {
                return OpenGLFamilyTextureContractFailure::upload;
            }

            TextureUploadDesc outOfBounds = upload;
            outOfBounds.x = 1;
            outOfBounds.width = 2;
            if (nativeDevice.upload_texture(texture, outOfBounds)
                || state.upload_count != 1u)
            {
                return OpenGLFamilyTextureContractFailure::bounds_rejection;
            }

            nativeDevice.destroy(texture);
            if (state.destroy_count != 1u || nativeDevice.resolve_texture(texture)
                || nativeDevice.texture_ready(texture))
            {
                return OpenGLFamilyTextureContractFailure::destruction;
            }
        }

        return OpenGLFamilyTextureContractFailure::none;
    }

}
