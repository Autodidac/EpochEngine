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

#include "../include/epoch.config.hpp"
#include "../src/epoch.common.hpp"
#include "../include/core.stl_types.hpp"

export module render.device;

export import render.camera;

export namespace epochengine
{
    enum class RendererBackendKind : u8
    {
        null,
        opengl,
        sdl3,
        sfml3,
        raylib3,
        vulkan,
        directx,
        software
    };

    enum class TextureFormat : u8
    {
        unknown,
        r8_unorm,
        r16_float,
        r32_uint,
        rgba8_unorm,
        bgra8_unorm,
        rgba16_float,
        rgba32_float,
        depth24_stencil8,
        depth32_float
    };
    [[nodiscard]] constexpr u32 texture_format_bytes_per_texel(
        TextureFormat format) noexcept
    {
        switch (format)
        {
        case TextureFormat::r8_unorm: return 1;
        case TextureFormat::r16_float: return 2;
        case TextureFormat::r32_uint:
        case TextureFormat::rgba8_unorm:
        case TextureFormat::bgra8_unorm:
        case TextureFormat::depth24_stencil8:
        case TextureFormat::depth32_float:
            return 4;
        case TextureFormat::rgba16_float: return 8;
        case TextureFormat::rgba32_float: return 16;
        case TextureFormat::unknown: break;
        }
        return 0;
    }


    enum class FilterMode : u8
    {
        nearest,
        linear
    };

    enum class AddressMode : u8
    {
        clamp_to_edge,
        repeat,
        mirrored_repeat
    };

    enum class ShaderStage : u8
    {
        vertex,
        fragment,
        compute
    };

    enum class PrimitiveTopology : u8
    {
        triangles,
        lines,
        points
    };

    enum class IndexFormat : u8
    {
        none,
        uint16,
        uint32
    };

    enum class VertexSemantic : u8
    {
        position,
        normal,
        tangent,
        texcoord0,
        texcoord1,
        color0,
        joints0,
        weights0
    };

    struct BufferDesc
    {
        u64 size_bytes = 0;
        bool gpu_only  = true;
        bool storage   = false;
        bool uniform   = false;
        bool indirect  = false;
        bool mapped    = false;
        const char* debug_name = nullptr;
    };

    struct TextureDesc
    {
        u32 width = 1, height = 1, mip_levels = 1;
        TextureFormat format = TextureFormat::rgba8_unorm;
        bool sampled = true;
        bool storage = false;
        bool render_target = false;
        bool depth_stencil = false;
        bool sparse = false;
        const char* debug_name = nullptr;
    };
    struct TextureUploadDesc
    {
        u32 mip_level = 0;
        u32 x = 0;
        u32 y = 0;
        u32 width = 0;
        u32 height = 0;
        u32 row_pitch_bytes = 0;
        TextureFormat format = TextureFormat::unknown;
        const void* data = nullptr;
        u64 size_bytes = 0;
    };

    // Upload implementations consume or copy the non-owning data span before
    // returning. Deferred backends must move bytes into backend-owned staging.

    [[nodiscard]] constexpr u64 minimum_texture_upload_bytes(
        const TextureUploadDesc& upload) noexcept
    {
        const u64 texelBytes = texture_format_bytes_per_texel(upload.format);
        if (texelBytes == 0 || upload.width == 0 || upload.height == 0)
            return 0;

        const u64 tightRowBytes = static_cast<u64>(upload.width) * texelBytes;
        const u64 rowPitch = upload.row_pitch_bytes == 0
            ? tightRowBytes
            : static_cast<u64>(upload.row_pitch_bytes);
        if (rowPitch < tightRowBytes)
            return 0;

        const u64 precedingRows = static_cast<u64>(upload.height - 1u);
        constexpr u64 maximum = ~u64{0};
        if (precedingRows != 0 && rowPitch > (maximum - tightRowBytes) / precedingRows)
            return 0;
        return precedingRows * rowPitch + tightRowBytes;
    }

    [[nodiscard]] constexpr bool valid(
        const TextureUploadDesc& upload) noexcept
    {
        const u64 minimumBytes = minimum_texture_upload_bytes(upload);
        return upload.data != nullptr
            && minimumBytes != 0
            && upload.size_bytes >= minimumBytes;
    }


    struct SamplerDesc
    {
        FilterMode min_filter = FilterMode::linear;
        FilterMode mag_filter = FilterMode::linear;
        AddressMode address_u = AddressMode::clamp_to_edge;
        AddressMode address_v = AddressMode::clamp_to_edge;
        AddressMode address_w = AddressMode::clamp_to_edge;
        float max_anisotropy = 1.0f;
        const char* debug_name = nullptr;
    };

    struct ShaderDesc
    {
        ShaderStage stage = ShaderStage::vertex;
        const void* bytecode = nullptr;
        u64 bytecode_size = 0;
        const char* entry_point = "main";
        const char* debug_name = nullptr;
    };

    struct PipelineDesc
    {
        PrimitiveTopology topology = PrimitiveTopology::triangles;
        bool depth_test = true;
        bool depth_write = true;
        bool alpha_blend = false;
        u32 color_attachment_count = 1;
        const char* debug_name = nullptr;
    };

    struct VertexAttributeDesc
    {
        VertexSemantic semantic = VertexSemantic::position;
        u32 location = 0;
        u32 offset_bytes = 0;
        u32 component_count = 3;
        TextureFormat component_format = TextureFormat::rgba32_float;
        bool normalized = false;
    };

    struct VertexLayoutDesc
    {
        u32 stride_bytes = 0;
        epochengine::small_vector<VertexAttributeDesc> attributes{};
    };

    enum class MaterialTextureSlot : u8
    {
        base_color,
        normal,
        roughness,
        metallic,
        emissive,
        opacity,
        render_surface
    };

    struct MaterialTextureSlotDesc
    {
        MaterialTextureSlot slot = MaterialTextureSlot::base_color;
        const char* name = nullptr;
        TextureFormat expected_format = TextureFormat::rgba8_unorm;
        bool required = false;
    };

    struct MaterialDesc
    {
        const char* name = nullptr;
        float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bool unlit = false;
        bool alpha_blend = false;
        epochengine::small_vector<MaterialTextureSlotDesc> texture_slots{};
        const char* debug_name = nullptr;
    };

    struct RenderTargetDesc
    {
        u32 width = 1;
        u32 height = 1;
        TextureFormat color_format = TextureFormat::rgba8_unorm;
        TextureFormat depth_format = TextureFormat::depth24_stencil8;
        bool has_depth = true;
        bool sampled_after_render = true;
        const char* debug_name = nullptr;
    };

    struct RenderPassDesc
    {
        struct ViewBinding
        {
            render_camera::ViewDescriptor view{};
            bool enabled{};
            bool require_clip_planes{};
        };

        bool clear_color = true;
        bool clear_depth = true;
        float clear[4] = { 0.07f, 0.09f, 0.12f, 1.0f };
        ViewBinding view_binding{};
        const char* debug_name = nullptr;
    };

    enum class RenderTextureUsage : u8
    {
        scene_preview,
        arcade_cabinet,
        ui_surface,
        capture,
        package_preview,
        portal,
        mirror
    };

    struct RenderTextureAssetDesc
    {
        u32 width = 512;
        u32 height = 512;
        TextureFormat color_format = TextureFormat::rgba8_unorm;
        TextureFormat depth_format = TextureFormat::depth24_stencil8;
        bool has_depth = true;
        bool sampled_after_render = true;
        RenderTextureUsage usage = RenderTextureUsage::scene_preview;
        const char* debug_name = nullptr;
    };

    struct RenderTextureBackendRequirements
    {
        bool color_attachment = true;
        bool depth_attachment = true;
        bool sampled_color = true;
        bool sampler = true;
        bool offscreen_target = true;
        bool presentable_surface = false;
    };

    struct RenderTextureAssetPlan
    {
        TextureDesc color_texture{};
        SamplerDesc sampler{};
        RenderTargetDesc render_target{};
        RenderPassDesc render_pass{};
        RenderTextureBackendRequirements backend_requirements{};
    };

    [[nodiscard]] constexpr RenderTextureAssetPlan make_render_texture_asset_plan(const RenderTextureAssetDesc& desc) noexcept
    {
        RenderTextureAssetPlan plan{};
        plan.color_texture.width = desc.width;
        plan.color_texture.height = desc.height;
        plan.color_texture.format = desc.color_format;
        plan.color_texture.sampled = desc.sampled_after_render;
        plan.color_texture.render_target = true;
        plan.color_texture.debug_name = desc.debug_name;

        plan.sampler.min_filter = FilterMode::linear;
        plan.sampler.mag_filter = FilterMode::linear;
        plan.sampler.address_u = AddressMode::clamp_to_edge;
        plan.sampler.address_v = AddressMode::clamp_to_edge;
        plan.sampler.address_w = AddressMode::clamp_to_edge;
        plan.sampler.debug_name = desc.debug_name;

        plan.render_target.width = desc.width;
        plan.render_target.height = desc.height;
        plan.render_target.color_format = desc.color_format;
        plan.render_target.depth_format = desc.depth_format;
        plan.render_target.has_depth = desc.has_depth;
        plan.render_target.sampled_after_render = desc.sampled_after_render;
        plan.render_target.debug_name = desc.debug_name;

        plan.render_pass.clear_color = true;
        plan.render_pass.clear_depth = desc.has_depth;
        plan.render_pass.debug_name = desc.debug_name;

        plan.backend_requirements.color_attachment = true;
        plan.backend_requirements.depth_attachment = desc.has_depth;
        plan.backend_requirements.sampled_color = desc.sampled_after_render;
        plan.backend_requirements.sampler = desc.sampled_after_render;
        plan.backend_requirements.offscreen_target = true;
        plan.backend_requirements.presentable_surface =
            desc.usage == RenderTextureUsage::arcade_cabinet
            || desc.usage == RenderTextureUsage::ui_surface
            || desc.usage == RenderTextureUsage::package_preview;

        return plan;
    }

    struct BackendBufferTag {};
    struct BackendTextureTag {};
    struct BackendSamplerTag {};
    struct BackendShaderTag {};
    struct BackendPipelineTag {};
    struct BackendMaterialTag {};
    struct BackendRenderTargetTag {};
    struct BackendBindingSetTag {};
    struct BackendCommandListTag {};
    struct BackendMeshTag {};
    struct BackendModelTag {};

    using BufferHandle  = Handle<BackendBufferTag, u32>;
    using TextureHandle = Handle<BackendTextureTag, u32>;
    using SamplerHandle = Handle<BackendSamplerTag, u32>;
    using ShaderHandle = Handle<BackendShaderTag, u32>;
    using PipelineHandle = Handle<BackendPipelineTag, u32>;
    using MaterialHandle = Handle<BackendMaterialTag, u32>;
    using RenderTargetHandle = Handle<BackendRenderTargetTag, u32>;
    using BindingSetHandle = Handle<BackendBindingSetTag, u32>;
    using CommandListHandle = Handle<BackendCommandListTag, u32>;
    using MeshHandle = Handle<BackendMeshTag, u32>;
    using ModelHandle = Handle<BackendModelTag, u32>;

    struct MeshDesc
    {
        BufferHandle vertex_buffer{};
        BufferHandle index_buffer{};
        MaterialHandle material{};
        VertexLayoutDesc vertex_layout{};
        u32 vertex_count = 0;
        u32 index_count = 0;
        IndexFormat index_format = IndexFormat::none;
        PrimitiveTopology topology = PrimitiveTopology::triangles;
        const char* debug_name = nullptr;
    };

    struct ModelMeshDesc
    {
        MeshHandle mesh{};
        MaterialHandle material{};
        const char* node_name = nullptr;
        float transform[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    };

    struct ModelDesc
    {
        const char* name = nullptr;
        const char* source_path = nullptr;
        bool static_mesh = true;
        epochengine::small_vector<ModelMeshDesc> meshes{};
        const char* debug_name = nullptr;
    };

    struct RenderTextureAssetHandles
    {
        TextureHandle color_texture{};
        SamplerHandle sampler{};
        RenderTargetHandle render_target{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return static_cast<bool>(color_texture) && static_cast<bool>(render_target);
        }

        [[nodiscard]] constexpr bool satisfies(const RenderTextureBackendRequirements& requirements) const noexcept
        {
            if ((requirements.color_attachment || requirements.sampled_color) && !color_texture)
                return false;
            if (requirements.sampler && !sampler)
                return false;
            if (requirements.offscreen_target && !render_target)
                return false;
            return true;
        }
    };

    struct MaterialTextureBinding
    {
        MaterialTextureSlot slot = MaterialTextureSlot::base_color;
        TextureHandle texture{};
        SamplerHandle sampler{};
    };

    struct CommandResourceBindings
    {
        epochengine::small_vector<BufferHandle> read_buffers{};
        epochengine::small_vector<TextureHandle> read_textures{};
        epochengine::small_vector<SamplerHandle> read_samplers{};
        epochengine::small_vector<MaterialHandle> read_materials{};
        epochengine::small_vector<MaterialTextureBinding> read_material_textures{};
        epochengine::small_vector<RenderTargetHandle> read_render_targets{};
        epochengine::small_vector<MeshHandle> read_meshes{};
        epochengine::small_vector<ModelHandle> read_models{};
        epochengine::small_vector<BufferHandle> write_buffers{};
        epochengine::small_vector<TextureHandle> write_textures{};
        epochengine::small_vector<MaterialHandle> write_materials{};
        epochengine::small_vector<RenderTargetHandle> write_render_targets{};
        epochengine::small_vector<MeshHandle> write_meshes{};
        epochengine::small_vector<ModelHandle> write_models{};

        [[nodiscard]] bool empty() const noexcept
        {
            return read_buffers.empty() &&
                   read_textures.empty() &&
                   read_samplers.empty() &&
                   read_materials.empty() &&
                   read_material_textures.empty() &&
                   read_render_targets.empty() &&
                   read_meshes.empty() &&
                   read_models.empty() &&
                   write_buffers.empty() &&
                   write_textures.empty() &&
                   write_materials.empty() &&
                   write_render_targets.empty() &&
                   write_meshes.empty() &&
                   write_models.empty();
        }
    };

    struct RendererCapabilities
    {
        RendererBackendKind backend = RendererBackendKind::null;
        bool buffers = false;
        bool textures = false;
        bool samplers = false;
        bool shaders = false;
        bool pipelines = false;
        bool materials = false;
        bool render_targets = false;
        bool command_lists = false;
        bool frame_graph = false;
        bool render_to_texture = false;
        bool sampled_render_targets = false;
        bool sampled_rtt_hook_ready = false;
        bool sampled_rtt_live_allocation_ready = false;
        bool sampled_rtt_presentation_proven = false;
        bool native_sampled_render_targets = false;
        bool binding_sets = false;
        bool mesh_resources = false;
        bool model_resources = false;
        bool model_import_ready = false;
        bool normal_mapping_ready = false;
        bool skybox_ready = false;
        bool instancing_ready = false;
        bool shadow_mapping_ready = false;
        bool deferred_gbuffer_ready = false;
    };

    enum class RendererCapabilityStatus : u8
    {
        missing,
        deferred,
        partial,
        present
    };

    struct RendererCapabilityReport
    {
        RendererBackendKind backend = RendererBackendKind::null;
        RendererCapabilityStatus descriptor_contract = RendererCapabilityStatus::missing;
        RendererCapabilityStatus build_graph_proof = RendererCapabilityStatus::missing;
        RendererCapabilityStatus hook_readiness = RendererCapabilityStatus::missing;
        RendererCapabilityStatus live_native_allocation = RendererCapabilityStatus::missing;
        RendererCapabilityStatus presentation_proof = RendererCapabilityStatus::missing;
        RendererCapabilityStatus sampled_render_targets = RendererCapabilityStatus::missing;
        RendererCapabilityStatus scene_sampled_surface = RendererCapabilityStatus::missing;
        RendererCapabilityStatus mesh_model_resources = RendererCapabilityStatus::missing;
    };

    [[nodiscard]] constexpr const char* renderer_capability_status_label(RendererCapabilityStatus status) noexcept
    {
        switch (status)
        {
        case RendererCapabilityStatus::present: return "Present";
        case RendererCapabilityStatus::partial: return "Partial";
        case RendererCapabilityStatus::deferred: return "Deferred";
        case RendererCapabilityStatus::missing:
        default: return "Missing";
        }
    }

    [[nodiscard]] constexpr bool renderer_capability_status_at_least(
        RendererCapabilityStatus status,
        RendererCapabilityStatus threshold) noexcept
    {
        auto rank = [](RendererCapabilityStatus value) constexpr noexcept -> u8
        {
            switch (value)
            {
            case RendererCapabilityStatus::present: return 3;
            case RendererCapabilityStatus::partial: return 2;
            case RendererCapabilityStatus::deferred: return 1;
            case RendererCapabilityStatus::missing:
            default: return 0;
            }
        };

        return rank(status) >= rank(threshold);
    }

    [[nodiscard]] constexpr RendererCapabilities renderer_capabilities_for(RendererBackendKind backend) noexcept
    {
        RendererCapabilities caps{};
        caps.backend = backend;

        switch (backend)
        {
        case RendererBackendKind::opengl:
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
            caps.binding_sets = true;
            break;
        case RendererBackendKind::raylib3:
            caps.buffers = true;
            caps.textures = true;
            caps.samplers = true;
            caps.shaders = true;
            caps.pipelines = true;
            caps.materials = true;
            caps.render_targets = true;
            caps.command_lists = true;
            caps.render_to_texture = true;
            caps.sampled_render_targets = true;
            caps.binding_sets = true;
            break;
        case RendererBackendKind::sdl3:
        case RendererBackendKind::sfml3:
            caps.textures = true;
            caps.samplers = true;
            caps.materials = true;
            caps.render_targets = true;
            caps.command_lists = true;
            caps.render_to_texture = true;
            caps.sampled_render_targets = true;
            caps.binding_sets = true;
            break;
        case RendererBackendKind::vulkan:
        case RendererBackendKind::directx:
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
            caps.binding_sets = true;
            break;
        default:
            break;
        }

        return caps;
    }

    [[nodiscard]] constexpr RendererCapabilityReport renderer_capability_report_for(RendererBackendKind backend) noexcept
    {
        RendererCapabilityReport report{};
        report.backend = backend;

        switch (backend)
        {
        case RendererBackendKind::opengl:
            report.descriptor_contract = RendererCapabilityStatus::present;
            report.build_graph_proof = RendererCapabilityStatus::present;
            report.hook_readiness = RendererCapabilityStatus::present;
            report.live_native_allocation = RendererCapabilityStatus::partial;
            report.presentation_proof = RendererCapabilityStatus::partial;
            report.sampled_render_targets = RendererCapabilityStatus::partial;
            report.scene_sampled_surface = RendererCapabilityStatus::partial;
            report.mesh_model_resources = RendererCapabilityStatus::partial;
            break;
        case RendererBackendKind::sdl3:
            report.descriptor_contract = RendererCapabilityStatus::present;
            report.build_graph_proof = RendererCapabilityStatus::present;
            report.hook_readiness = RendererCapabilityStatus::present;
            report.live_native_allocation = RendererCapabilityStatus::partial;
            report.presentation_proof = RendererCapabilityStatus::partial;
            report.sampled_render_targets = RendererCapabilityStatus::partial;
            report.scene_sampled_surface = RendererCapabilityStatus::partial;
            report.mesh_model_resources = RendererCapabilityStatus::partial;
            break;
        case RendererBackendKind::sfml3:
        case RendererBackendKind::raylib3:
            report.descriptor_contract = RendererCapabilityStatus::present;
            report.build_graph_proof = RendererCapabilityStatus::present;
            report.hook_readiness = RendererCapabilityStatus::partial;
            report.live_native_allocation = RendererCapabilityStatus::partial;
            report.presentation_proof = RendererCapabilityStatus::missing;
            report.sampled_render_targets = RendererCapabilityStatus::partial;
            report.scene_sampled_surface = RendererCapabilityStatus::partial;
            report.mesh_model_resources = RendererCapabilityStatus::partial;
            break;
        case RendererBackendKind::vulkan:
        case RendererBackendKind::directx:
            report.descriptor_contract = RendererCapabilityStatus::partial;
            report.build_graph_proof = RendererCapabilityStatus::partial;
            report.hook_readiness = RendererCapabilityStatus::missing;
            report.live_native_allocation = RendererCapabilityStatus::missing;
            report.presentation_proof = RendererCapabilityStatus::missing;
            report.sampled_render_targets = RendererCapabilityStatus::partial;
            report.scene_sampled_surface = RendererCapabilityStatus::partial;
            report.mesh_model_resources = RendererCapabilityStatus::partial;
            break;
        case RendererBackendKind::software:
            report.descriptor_contract = RendererCapabilityStatus::deferred;
            report.build_graph_proof = RendererCapabilityStatus::deferred;
            report.hook_readiness = RendererCapabilityStatus::deferred;
            report.live_native_allocation = RendererCapabilityStatus::deferred;
            report.presentation_proof = RendererCapabilityStatus::deferred;
            report.sampled_render_targets = RendererCapabilityStatus::deferred;
            report.scene_sampled_surface = RendererCapabilityStatus::partial;
            report.mesh_model_resources = RendererCapabilityStatus::deferred;
            break;
        case RendererBackendKind::null:
        default:
            break;
        }

        return report;
    }

    [[nodiscard]] constexpr bool renderer_supports_sampled_render_targets(const RendererCapabilities& caps) noexcept
    {
        return caps.textures &&
               caps.samplers &&
               caps.render_targets &&
               caps.render_to_texture &&
               caps.sampled_render_targets;
    }

    [[nodiscard]] constexpr bool renderer_supports_sampled_rtt_hooks(const RendererCapabilities& caps) noexcept
    {
        return renderer_supports_sampled_render_targets(caps) && caps.sampled_rtt_hook_ready;
    }

    [[nodiscard]] constexpr bool renderer_supports_live_sampled_rtt_allocation(const RendererCapabilities& caps) noexcept
    {
        return renderer_supports_sampled_render_targets(caps) && caps.sampled_rtt_live_allocation_ready;
    }

    [[nodiscard]] constexpr bool renderer_has_sampled_rtt_presentation_proof(const RendererCapabilities& caps) noexcept
    {
        return renderer_supports_sampled_render_targets(caps) && caps.sampled_rtt_presentation_proven;
    }

    [[nodiscard]] constexpr bool renderer_supports_native_sampled_render_targets(const RendererCapabilities& caps) noexcept
    {
        return renderer_supports_live_sampled_rtt_allocation(caps);
    }

    [[nodiscard]] constexpr bool renderer_supports_mesh_resources(const RendererCapabilities& caps) noexcept
    {
        return caps.buffers && caps.materials && caps.mesh_resources;
    }

    [[nodiscard]] constexpr bool renderer_supports_model_resources(const RendererCapabilities& caps) noexcept
    {
        return renderer_supports_mesh_resources(caps) && caps.model_resources;
    }

    struct ICommandContext
    {
        virtual ~ICommandContext() = default;
        virtual void begin(const char* label) = 0;
        virtual void end() = 0;
        virtual void debug_marker(const char* label) = 0;
        virtual void bind_binding_set(BindingSetHandle) {}
        virtual void bind_resources(const CommandResourceBindings&) {}
        virtual void barrier() = 0;
        virtual void begin_render_pass(RenderTargetHandle, const RenderPassDesc&) {}
        virtual void end_render_pass() {}
        virtual void draw_mesh(MeshHandle, MaterialHandle = {}) {}
        virtual void draw_model(ModelHandle) {}
    };

    struct ISwapchain
    {
        virtual ~ISwapchain() = default;
        virtual u32 width() const noexcept = 0;
        virtual u32 height() const noexcept = 0;
    };

    struct IRenderDevice
    {
        virtual ~IRenderDevice() = default;
        virtual std::string backend_name() const = 0;
        virtual RendererCapabilities capabilities() const noexcept { return {}; }

        virtual BufferHandle  create_buffer(const BufferDesc& desc) = 0;
        virtual TextureHandle create_texture(const TextureDesc& desc) = 0;
        virtual SamplerHandle create_sampler(const SamplerDesc&) { return {}; }
        virtual bool upload_texture(TextureHandle, const TextureUploadDesc&)
        {
            return false;
        }
        virtual bool texture_ready(TextureHandle) const noexcept
        {
            return false;
        }
        virtual ShaderHandle create_shader(const ShaderDesc&) { return {}; }
        virtual PipelineHandle create_pipeline(const PipelineDesc&) { return {}; }
        virtual MaterialHandle create_material(const MaterialDesc&) { return {}; }
        virtual RenderTargetHandle create_render_target(const RenderTargetDesc&) { return {}; }
        virtual BindingSetHandle create_binding_set(const CommandResourceBindings&) { return {}; }
        virtual MeshHandle create_mesh(const MeshDesc&) { return {}; }
        virtual ModelHandle create_model(const ModelDesc&) { return {}; }
        virtual RenderTextureAssetHandles create_render_texture_asset(const RenderTextureAssetDesc& desc)
        {
            const RenderTextureAssetPlan plan = make_render_texture_asset_plan(desc);
            RenderTextureAssetHandles handles{};
            handles.color_texture = create_texture(plan.color_texture);
            handles.sampler = create_sampler(plan.sampler);
            handles.render_target = create_render_target(plan.render_target);
            return handles;
        }

        virtual void destroy(BufferHandle) noexcept = 0;
        virtual void destroy(TextureHandle) noexcept = 0;
        virtual void destroy(SamplerHandle) noexcept {}
        virtual void destroy(ShaderHandle) noexcept {}
        virtual void destroy(PipelineHandle) noexcept {}
        virtual void destroy(MaterialHandle) noexcept {}
        virtual void destroy(RenderTargetHandle) noexcept {}
        virtual void destroy(BindingSetHandle) noexcept {}
        virtual void destroy(MeshHandle) noexcept {}
        virtual void destroy(ModelHandle) noexcept {}
        virtual void destroy(RenderTextureAssetHandles handles) noexcept
        {
            if (handles.render_target)
                destroy(handles.render_target);
            if (handles.sampler)
                destroy(handles.sampler);
            if (handles.color_texture)
                destroy(handles.color_texture);
        }

        virtual ICommandContext& acquire_graphics_context() = 0;
        virtual CommandListHandle begin_command_list(const char*) { return {}; }
        virtual void end_command_list(CommandListHandle) {}
        virtual void present(ISwapchain& sc) = 0;
    };
} // namespace epoch
