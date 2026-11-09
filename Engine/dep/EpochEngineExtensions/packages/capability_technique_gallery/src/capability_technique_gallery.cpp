// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/capability_technique_gallery/capability_technique_gallery.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace epoch::extensions::capability_technique_gallery
{
    namespace
    {
        using enum BackendFamily;
        using enum CapabilityFeature;
        using enum CapabilityTier;

        constexpr auto kCpuRoutes = std::to_array<ProviderRoute>({
            {cpu, t0_cpu, 0U, "epoch.execution.cpu"},
            {software, t0_cpu, 0U, "epoch.execution.software"}
        });

        constexpr auto kPortableRasterRoutes = std::to_array<ProviderRoute>({
            {opengles, t1_gles, feature_mask(presentation), "epoch.render.portable"},
            {opengl, t1_gl, feature_mask(presentation), "epoch.render.portable"},
            {vulkan, t2_vk, feature_mask(presentation), "epoch.render.explicit"},
            {directx12, t2_dx, feature_mask(presentation), "epoch.render.explicit"}
        });

        constexpr auto kPortableRasterWithSoftwareRoutes = std::to_array<ProviderRoute>({
            {software, t0_cpu, 0U, "epoch.render.software"},
            {opengles, t1_gles, feature_mask(presentation), "epoch.render.portable"},
            {opengl, t1_gl, feature_mask(presentation), "epoch.render.portable"},
            {vulkan, t2_vk, feature_mask(presentation), "epoch.render.explicit"},
            {directx12, t2_dx, feature_mask(presentation), "epoch.render.explicit"}
        });

        constexpr auto kDesktopRasterRoutes = std::to_array<ProviderRoute>({
            {opengl, t1_gl, feature_mask(presentation), "epoch.render.desktop"},
            {vulkan, t2_vk, feature_mask(presentation), "epoch.render.explicit"},
            {directx12, t2_dx, feature_mask(presentation), "epoch.render.explicit"}
        });

        constexpr auto kRenderTargetRoutes = std::to_array<ProviderRoute>({
            {opengles, t1_gles, presentation | render_target, "epoch.render.frame_graph"},
            {opengl, t1_gl, presentation | render_target, "epoch.render.frame_graph"},
            {vulkan, t2_vk, presentation | render_target, "epoch.render.frame_graph"},
            {directx12, t2_dx, presentation | render_target, "epoch.render.frame_graph"}
        });

        constexpr auto kHdrRoutes = std::to_array<ProviderRoute>({
            {opengles, t1_gles, presentation | hdr_render_target, "epoch.render.frame_graph"},
            {opengl, t1_gl, presentation | hdr_render_target, "epoch.render.frame_graph"},
            {vulkan, t2_vk, presentation | hdr_render_target, "epoch.render.frame_graph"},
            {directx12, t2_dx, presentation | hdr_render_target, "epoch.render.frame_graph"}
        });
        constexpr auto kPortableComputeRoutes = std::to_array<ProviderRoute>({
            {opengles, t1_gles, compute | storage_buffer, "epoch.compute.portable"},
            {opengl, t1_gl, compute | storage_buffer, "epoch.compute.portable"},
            {vulkan, t2_vk, compute | storage_buffer, "epoch.compute.explicit"},
            {directx12, t2_dx, compute | storage_buffer, "epoch.compute.explicit"}
        });

        constexpr auto kIndirectRoutes = std::to_array<ProviderRoute>({
            {opengl, t1_gl, feature_mask(indirect_draw), "epoch.render.geometry"},
            {vulkan, t2_vk, feature_mask(indirect_draw), "epoch.render.geometry"},
            {directx12, t2_dx, feature_mask(indirect_draw), "epoch.render.geometry"}
        });

        constexpr auto kTessellationRoutes = std::to_array<ProviderRoute>({
            {opengl, t1_gl, feature_mask(tessellation), "epoch.render.terrain"},
            {vulkan, t2_vk, feature_mask(tessellation), "epoch.render.terrain"},
            {directx12, t2_dx, feature_mask(tessellation), "epoch.render.terrain"}
        });

        constexpr auto kGpuTimingRoutes = std::to_array<ProviderRoute>({
            {opengl, t1_gl, feature_mask(gpu_timing), "epoch.performance.evidence"},
            {vulkan, t2_vk, feature_mask(gpu_timing), "epoch.performance.evidence"},
            {directx12, t2_dx, feature_mask(gpu_timing), "epoch.performance.evidence"}
        });

        constexpr auto kBindlessRoutes = std::to_array<ProviderRoute>({
            {opengl, t1_gl, feature_mask(bindless_resources), "epoch.render.resource"},
            {vulkan, t3_vk, feature_mask(descriptor_indexing), "epoch.render.resource"},
            {directx12, t3_dx, feature_mask(descriptor_indexing), "epoch.render.resource"}
        });

        constexpr auto kSelectionRoutes = std::to_array<ProviderRoute>({
            {cpu, t0_cpu, 0U, "epoch.spatial.query"},
            {software, t0_cpu, 0U, "epoch.spatial.query"},
            {opengles, t1_gles, feature_mask(presentation), "epoch.editor.selection"},
            {opengl, t1_gl, feature_mask(presentation), "epoch.editor.selection"},
            {vulkan, t2_vk, feature_mask(presentation), "epoch.editor.selection"},
            {directx12, t2_dx, feature_mask(presentation), "epoch.editor.selection"}
        });

        constexpr auto kRayQueryRoutes = std::to_array<ProviderRoute>({
            {vulkan, t4_vk, feature_mask(ray_query), "epoch.spatial.ray_query"},
            {directx12, t4_dx, feature_mask(ray_query), "epoch.spatial.ray_query"}
        });

        constexpr auto kRayPipelineRoutes = std::to_array<ProviderRoute>({
            {vulkan, t5_vk, feature_mask(ray_pipeline), "epoch.render.ray_pipeline"},
            {directx12, t5_dx, feature_mask(ray_pipeline), "epoch.render.ray_pipeline"}
        });
        constexpr std::string_view kReferenceEvidence =
            "Implemented in the OpenGL technique laboratory; no EpochEngine runtime proof is claimed by this package.";
        constexpr std::string_view kDeferredEvidence =
            "Named by the reference catalog or Epoch capability plan, but deferred until an EpochEngine provider has build and runtime evidence.";
        constexpr std::string_view kGatedEvidence =
            "Staged behind explicit capability evidence; API name or extension presence alone is insufficient.";

        constexpr TechniqueEntry entry(
            std::string_view id,
            std::string_view label,
            TechniqueFamily family,
            std::string_view context,
            TechniqueStatus status,
            EvidenceScope evidenceScope,
            std::string_view evidence,
            std::string_view dependencies,
            std::string_view fallback,
            std::string_view sceneIntent,
            std::span<const ProviderRoute> routes) noexcept
        {
            return TechniqueEntry{
                id,
                label,
                family,
                context,
                status,
                evidenceScope,
                evidence,
                dependencies,
                fallback,
                sceneIntent,
                routes
            };
        }

        constexpr auto kCatalog = std::to_array<TechniqueEntry>({
            entry(
                "platform.desktop.gl45",
                "Native desktop OpenGL context",
                TechniqueFamily::foundation,
                "context.driver",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "native window; OpenGL loader; input; resize; frame pacing",
                "Use the selected Epoch context driver or T0-CPU headless execution.",
                "Context information pedestal and frame-pacing monitor.",
                kDesktopRasterRoutes),
            entry(
                "resource.explicit",
                "Explicit renderer resources",
                TechniqueFamily::foundation,
                "render.resource",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "logical mesh; texture; cubemap; material handles",
                "Use CPU-owned logical resources and backend-disposable caches.",
                "Resource lifetime wall with stable logical handles.",
                kPortableRasterWithSoftwareRoutes),
            entry(
                "target.hdr",
                "HDR multiple-render-target frame",
                TechniqueFamily::foundation,
                "render.frame_graph",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "floating-point color target; brightness target; sampleable depth",
                "Render directly to an LDR target with clamped lighting.",
                "High-range emissive ramp beside an LDR fallback ramp.",
                kHdrRoutes),
            entry(
                "target.rtt",
                "Render to texture",
                TechniqueFamily::foundation,
                "render.frame_graph",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "offscreen color target; depth target; final compose",
                "Render the primary view directly without secondary feeds.",
                "Security-camera, mirror, and Canvas2D monitor surfaces.",
                kRenderTargetRoutes),
            entry(
                "texture.srgb",
                "sRGB upload and linear-light shading",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "color-space metadata; texture decode; shader conversion",
                "Use linear RGBA data with an explicit quality warning.",
                "Color and data texture comparison cards.",
                kPortableRasterWithSoftwareRoutes),
            entry(
                "texture.mip.aniso",
                "Mipmaps and anisotropic filtering",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "mip chain; sampler policy; proven anisotropy limit",
                "Use bilinear mip filtering or nearest sampling under tight budgets.",
                "Angled material runway with sampler labels.",
                kPortableRasterRoutes),
            entry(
                "material.pbr",
                "Metallic roughness PBR",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "base color; normal; ORM channels; light and environment inputs",
                "Use Blinn-Phong or unlit material through the same logical material.",
                "Material spheres for dielectric, metal, rough, and smooth response.",
                kPortableRasterRoutes),
            entry(
                "material.blinn",
                "Blinn-Phong compatibility shading",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "normal; diffuse color; light list; gloss",
                "Use Lambert diffuse or unlit vertex color.",
                "Compatibility material sphere beside the PBR station.",
                kPortableRasterWithSoftwareRoutes),
            entry(
                "material.normal",
                "Tangent-space normal mapping",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "normal map; tangent basis; normal transform",
                "Use geometric normals and retain the material identity.",
                "Grazing-light wall showing mapped and geometric normals.",
                kPortableRasterRoutes),
            entry(
                "material.parallax",
                "Parallax occlusion mapping",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "height map; tangent basis; bounded ray-march samples",
                "Use normal mapping, then flat textured shading.",
                "Brick receiver panel with sample-budget labels.",
                kDesktopRasterRoutes),
            entry(
                "material.clearcoat",
                "Layered clearcoat",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "base lobe; coat lobe; environment response",
                "Fold coat energy into the base specular lobe.",
                "Painted metal and carbon-fiber comparison pieces.",
                kDesktopRasterRoutes),
            entry(
                "material.transmission",
                "Environment transmission and refraction",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "environment sample; thickness; absorption; refraction policy",
                "Use alpha blend or opaque dielectric material.",
                "Glass panel with explicit opaque fallback.",
                kDesktopRasterRoutes),
            entry(
                "material.alpha",
                "Alpha-tested material",
                TechniqueFamily::materials,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "opacity channel; cutoff; depth policy",
                "Use opaque geometry or sorted alpha blending.",
                "Cutout foliage card and opaque proxy.",
                kPortableRasterRoutes),
            entry(
                "light.directional",
                "Directional light",
                TechniqueFamily::lighting,
                "render.lighting",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "light direction; color; intensity; surface normals",
                "Use ambient hemispheric lighting or unlit shading.",
                "Sun-lit ground, cube, and normal indicator.",
                kPortableRasterWithSoftwareRoutes),
            entry(
                "light.point",
                "Finite-radius point lights",
                TechniqueFamily::lighting,
                "render.lighting",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "light list; attenuation; bounded per-view budget",
                "Use the nearest light or bake emissive contribution.",
                "Three locally lit props with visible radius markers.",
                kPortableRasterRoutes),
            entry(
                "light.spot",
                "Spot light",
                TechniqueFamily::lighting,
                "render.lighting",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "light direction; cone angles; attenuation",
                "Use a point light or emissive fixture.",
                "Cone-lit receiver and fixture proxy.",
                kPortableRasterRoutes),
            entry(
                "light.projector",
                "Projected texture spotlight",
                TechniqueFamily::lighting,
                "render.lighting",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "spot light; projector transform; cookie texture",
                "Use an untextured spot light.",
                "Cookie projection wall with plain-spot fallback.",
                kDesktopRasterRoutes),
            entry(
                "shadow.directional.soft",
                "Tiered directional soft shadows",
                TechniqueFamily::lighting,
                "render.shadow",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "directional depth map; bias; PCF or PCSS sampling",
                "Use hard PCF, blob shadows, or no dynamic shadow.",
                "Shadow receiver with T0 bounded and advanced filter cards.",
                kRenderTargetRoutes),
            entry(
                "environment.sky",
                "Cubemap sky and environment response",
                TechniqueFamily::lighting,
                "render.environment",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "cubemap; sky draw; reflection and transmission sampling",
                "Use a procedural gradient and constant ambient probe.",
                "Sky enclosure with reflective material samples.",
                kDesktopRasterRoutes),
            entry(
                "geometry.billboard",
                "Instanced vertex-expanded billboards",
                TechniqueFamily::geometry,
                "render.geometry",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "instance data; camera basis; alpha-tested material",
                "Use CPU-expanded quads or static crossed planes.",
                "Foliage strip with single and crossed-plane variants.",
                kPortableRasterRoutes),
            entry(
                "geometry.instancing",
                "Hardware instancing",
                TechniqueFamily::geometry,
                "render.geometry",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "shared mesh; shared material; instance transforms",
                "Issue bounded ordinary draws or merge static geometry.",
                "Repeated crate row with draw-count evidence.",
                kPortableRasterRoutes),
            entry(
                "geometry.indirect",
                "Indirect indexed drawing",
                TechniqueFamily::geometry,
                "render.geometry",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "indirect command buffer; draw parameters; capability proof",
                "Use ordinary instanced indexed drawing.",
                "GPU-driven submission station with visible instancing fallback.",
                kIndirectRoutes),
            entry(
                "geometry.tessellation",
                "Tessellated height-map terrain",
                TechniqueFamily::geometry,
                "render.terrain",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "patch topology; tessellation stages; height field",
                "Use a CPU-generated or vertex-displaced regular grid.",
                "Terrain patch with fixed-grid fallback.",
                kTessellationRoutes),
            entry(
                "geometry.pn",
                "PN-style curved tessellation",
                TechniqueFamily::geometry,
                "render.terrain",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "patch topology; control normals; tessellation stages",
                "Use the original mesh or pre-subdivided geometry.",
                "Curved patch beside its source control mesh.",
                kTessellationRoutes),
            entry(
                "simulation.object",
                "Transform animation",
                TechniqueFamily::simulation,
                "simulation.timeline",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "stable object identity; time address; transform evaluator",
                "Use a static authored transform.",
                "Reversible rotating display object.",
                kCpuRoutes),
            entry(
                "simulation.cloth",
                "Fixed-step Verlet cloth",
                TechniqueFamily::simulation,
                "simulation.physics",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "particle grid; constraints; fixed step; dynamic normals",
                "Use a static or skeletal cloth mesh.",
                "Pinned cloth patch with static fallback banner.",
                kCpuRoutes),
            entry(
                "simulation.water",
                "Screen-refractive water",
                TechniqueFamily::simulation,
                "simulation.water",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "water surface; scene color; depth; Fresnel; foam",
                "Use opaque animated water or a static translucent plane.",
                "Pool surface with explicit opaque fallback basin.",
                kRenderTargetRoutes),
            entry(
                "simulation.particles",
                "Multi-emitter CPU particles",
                TechniqueFamily::simulation,
                "simulation.particles",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "fixed-step emitter; deterministic seed; sprite batch",
                "Use a bounded CPU emitter or static effect card.",
                "Fire, mote, and mist emitter markers.",
                kCpuRoutes),
            entry(
                "post.hdr",
                "HDR and ACES tone mapping",
                TechniqueFamily::post_process,
                "render.post",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "HDR source; exposure; tone-map operator; final compose",
                "Use clamped LDR output with gamma correction.",
                "Exposure ramp and final-frame comparison monitor.",
                kHdrRoutes),
            entry(
                "post.bloom",
                "Separable Gaussian bloom",
                TechniqueFamily::post_process,
                "render.post",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "bright-pass target; horizontal and vertical blur; compose",
                "Disable bloom and retain emissive material color.",
                "Emissive objects with bloom-disabled comparison.",
                kRenderTargetRoutes),
            entry(
                "post.fxaa",
                "FXAA",
                TechniqueFamily::post_process,
                "render.post",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "sampleable final color; inverse dimensions; edge filter",
                "Use MSAA, native resolution, or no post antialiasing.",
                "High-contrast edge chart.",
                kRenderTargetRoutes),
            entry(
                "post.ssao",
                "Screen-space ambient occlusion",
                TechniqueFamily::post_process,
                "render.post",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "depth; normals; projection; sample kernel; blur",
                "Use material AO, baked AO, or no screen-space AO.",
                "Contact-occlusion corner with baked fallback.",
                kRenderTargetRoutes),
            entry(
                "post.fog",
                "Distance fog",
                TechniqueFamily::post_process,
                "render.environment",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "view distance; fog density or range; atmosphere color",
                "Use a constant background and distance-based LOD fade.",
                "Depth runway with fog range markers.",
                kPortableRasterRoutes),
            entry(
                "post.toon",
                "Material-routed toon and rim lighting",
                TechniqueFamily::post_process,
                "render.material",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "material route; quantized lighting; rim term",
                "Use standard lit or unlit material.",
                "Opt-in toon object beside a standard material.",
                kPortableRasterRoutes),
            entry(
                "debug.views",
                "Material and shadow debug views",
                TechniqueFamily::diagnostics,
                "editor.diagnostics",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "material channels; render-view policy; visible labels",
                "Show textual material values and CPU reference samples.",
                "Normal, UV, roughness, metallic, AO, and shadow cards.",
                kPortableRasterWithSoftwareRoutes),
            entry(
                "debug.gl",
                "OpenGL debug output",
                TechniqueFamily::diagnostics,
                "context.diagnostics",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "validated OpenGL debug callback; engine logging surface",
                "Use backend-neutral validation messages and visible error state.",
                "Context diagnostics panel; no hidden console requirement.",
                kDesktopRasterRoutes),
            entry(
                "debug.gpuquery",
                "GPU timer queries",
                TechniqueFamily::diagnostics,
                "performance.evidence",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "timestamp or elapsed query; nonblocking result readback",
                "Use CPU frame spans and mark GPU time unavailable.",
                "Per-station CPU/GPU evidence monitor.",
                kGpuTimingRoutes),
            entry(
                "shadow.point",
                "Point-light cubemap shadows",
                TechniqueFamily::lighting,
                "render.shadow",
                TechniqueStatus::reference_only,
                EvidenceScope::reference_repository,
                kReferenceEvidence,
                "point light; six depth views; cubemap depth sampling",
                "Use an unshadowed point light or static shadow proxy.",
                "Point light room with six-face diagnostic labels.",
                kRenderTargetRoutes),
            entry(
                "shadow.cascade",
                "Cascaded directional shadows",
                TechniqueFamily::lighting,
                "render.shadow",
                TechniqueStatus::deferred,
                EvidenceScope::reference_repository,
                kDeferredEvidence,
                "camera-stable splits; shadow atlas; render graph ownership",
                "Use one directional shadow map with bounded PCF.",
                "Deferred station showing the single-map fallback.",
                kRenderTargetRoutes),
            entry(
                "render.deferred",
                "Deferred G-buffer lighting",
                TechniqueFamily::post_process,
                "render.frame_graph",
                TechniqueStatus::deferred,
                EvidenceScope::reference_repository,
                kDeferredEvidence,
                "G-buffer layout; lighting pass; material encoding; bandwidth budget",
                "Use forward or forward-plus material rendering.",
                "Deferred station showing forward fallback ownership.",
                kRenderTargetRoutes),
            entry(
                "animation.skinning",
                "GPU skeletal animation",
                TechniqueFamily::simulation,
                "simulation.animation",
                TechniqueStatus::deferred,
                EvidenceScope::reference_repository,
                kDeferredEvidence,
                "skeleton; clips; skin weights; palette upload; temporal address",
                "Use CPU skinning or a static mesh pose.",
                "Static humanoid with future skeletal route label.",
                kPortableRasterRoutes),
            entry(
                "particles.compute",
                "Compute and SSBO particles",
                TechniqueFamily::simulation,
                "simulation.particles",
                TechniqueStatus::deferred,
                EvidenceScope::reference_repository,
                kDeferredEvidence,
                "compute dispatch; storage buffer; synchronization; deterministic seed",
                "Use the bounded CPU particle provider.",
                "CPU emitter beside a deferred compute provider station.",
                kPortableComputeRoutes),
            entry(
                "selection.outline",
                "Picking and selected-object outline",
                TechniqueFamily::diagnostics,
                "editor.selection",
                TechniqueStatus::deferred,
                EvidenceScope::reference_repository,
                kDeferredEvidence,
                "stable selection identity; spatial query; outline composition",
                "Use CPU ray/AABB picking and a visible inspector selection state.",
                "Selectable proxy with text-only fallback evidence.",
                kSelectionRoutes),
            entry(
                "bindless.texture",
                "Bindless texture residency",
                TechniqueFamily::foundation,
                "render.resource",
                TechniqueStatus::capability_gated,
                EvidenceScope::reference_repository,
                kGatedEvidence,
                "descriptor indexing or proven bindless extension; residency budget",
                "Use bounded descriptor tables, atlases, or ordinary texture bindings.",
                "Resource table showing bindless and bounded-binding plans.",
                kBindlessRoutes),
            entry(
                "spatial.ray_query",
                "Hardware inline ray queries",
                TechniqueFamily::spatial,
                "spatial.query",
                TechniqueStatus::capability_gated,
                EvidenceScope::epoch_architecture_plan,
                kGatedEvidence,
                "acceleration structures; inline ray query; managed lifecycle",
                "Use CPU rays, software BVH, or lower-tier compute traversal.",
                "T4 query station for picking, shadows, and visibility.",
                kRayQueryRoutes),
            entry(
                "render.ray_pipeline",
                "Full hardware ray-tracing pipeline",
                TechniqueFamily::spatial,
                "render.ray_pipeline",
                TechniqueStatus::deferred,
                EvidenceScope::epoch_architecture_plan,
                kDeferredEvidence,
                "RT pipeline; shader tables; acceleration structures; temporal resources",
                "Use raster, compute traversal, or CPU reference rendering.",
                "T5 pipeline station with raster and compute fallback labels.",
                kRayPipelineRoutes)
        });

        [[nodiscard]] constexpr bool tier_matches_backend(
            BackendFamily backend,
            CapabilityTier tier) noexcept
        {
            switch (backend)
            {
                case BackendFamily::cpu:
                case BackendFamily::software:
                    return tier == CapabilityTier::t0_cpu;
                case BackendFamily::opengles:
                    return tier == CapabilityTier::t1_gles;
                case BackendFamily::opengl:
                    return tier == CapabilityTier::t1_gl;
                case BackendFamily::vulkan:
                    return tier == CapabilityTier::t2_vk ||
                        tier == CapabilityTier::t3_vk ||
                        tier == CapabilityTier::t4_vk ||
                        tier == CapabilityTier::t5_vk;
                case BackendFamily::directx12:
                    return tier == CapabilityTier::t2_dx ||
                        tier == CapabilityTier::t3_dx ||
                        tier == CapabilityTier::t4_dx ||
                        tier == CapabilityTier::t5_dx;
            }
            return false;
        }

        [[nodiscard]] constexpr std::uint8_t tier_rank(CapabilityTier tier) noexcept
        {
            switch (tier)
            {
                case CapabilityTier::t0_cpu:
                    return 0U;
                case CapabilityTier::t1_gles:
                case CapabilityTier::t1_gl:
                    return 1U;
                case CapabilityTier::t2_vk:
                case CapabilityTier::t2_dx:
                    return 2U;
                case CapabilityTier::t3_vk:
                case CapabilityTier::t3_dx:
                    return 3U;
                case CapabilityTier::t4_vk:
                case CapabilityTier::t4_dx:
                    return 4U;
                case CapabilityTier::t5_vk:
                case CapabilityTier::t5_dx:
                    return 5U;
            }
            return 0U;
        }

        [[nodiscard]] constexpr std::size_t family_index(TechniqueFamily family) noexcept
        {
            return static_cast<std::size_t>(family);
        }
    }

    std::span<const TechniqueEntry> technique_catalog() noexcept
    {
        return kCatalog;
    }

    const TechniqueEntry* find_technique(std::string_view id) noexcept
    {
        const auto found = std::ranges::find(kCatalog, id, &TechniqueEntry::id);
        return found == kCatalog.end() ? nullptr : &*found;
    }

    bool profile_supports(
        const CapabilityProfile& profile,
        const ProviderRoute& route) noexcept
    {
        if (!profile.validated ||
            profile.backend != route.backend ||
            !tier_matches_backend(profile.backend, profile.maximumTier) ||
            !tier_matches_backend(route.backend, route.minimumTier))
        {
            return false;
        }

        const auto hasRequiredFeatures =
            (profile.features & route.requiredFeatures) == route.requiredFeatures;
        return tier_rank(profile.maximumTier) >= tier_rank(route.minimumTier) &&
            hasRequiredFeatures;
    }

    std::optional<std::size_t> select_route(
        const TechniqueEntry& technique,
        std::span<const CapabilityProfile> profiles) noexcept
    {
        for (std::size_t routeIndex = 0; routeIndex < technique.routes.size(); ++routeIndex)
        {
            for (const auto& profile : profiles)
            {
                if (profile_supports(profile, technique.routes[routeIndex]))
                {
                    return routeIndex;
                }
            }
        }

        return std::nullopt;
    }

    ScenePlan make_scene_plan(std::span<const CapabilityProfile> profiles)
    {
        ScenePlan plan;
        plan.stations.reserve(kCatalog.size());

        constexpr auto familyCount = family_index(TechniqueFamily::spatial) + 1U;
        std::array<std::size_t, familyCount> columns{};

        for (std::size_t index = 0; index < kCatalog.size(); ++index)
        {
            const auto& technique = kCatalog[index];
            const auto row = family_index(technique.family);
            const auto column = columns[row]++;
            const auto selectedRoute = select_route(technique, profiles);

            StationDisposition disposition{};
            if (technique.status == TechniqueStatus::deferred)
            {
                disposition = StationDisposition::deferred;
                ++plan.deferredCount;
            }
            else if (selectedRoute)
            {
                disposition = StationDisposition::profile_eligible;
                ++plan.profileEligibleCount;
            }
            else
            {
                disposition = StationDisposition::fallback_only;
                ++plan.fallbackCount;
            }

            plan.stations.push_back(SceneStation{
                index,
                Vec3{
                    (static_cast<float>(column % 8U) * 6.0F) - 21.0F,
                    0.0F,
                    static_cast<float>(row) * 10.0F +
                        static_cast<float>(column / 8U) * 4.0F
                },
                disposition,
                selectedRoute
            });
        }

        return plan;
    }

    ValidationReport validate_catalog() noexcept
    {
        ValidationReport report{};
        report.techniqueCount = kCatalog.size();

        std::array<bool, family_index(TechniqueFamily::spatial) + 1U> families{};

        for (std::size_t index = 0; index < kCatalog.size(); ++index)
        {
            const auto& technique = kCatalog[index];
            if (technique.id.empty() ||
                technique.label.empty() ||
                technique.owningContext.empty() ||
                technique.evidence.empty() ||
                technique.dependencies.empty() ||
                technique.fallback.empty() ||
                technique.sceneIntent.empty())
            {
                report.firstError = "catalog entry has an empty required field";
                return report;
            }

            if (technique.routes.empty())
            {
                report.firstError = "catalog entry has no provider route";
                return report;
            }

            for (const auto& route : technique.routes)
            {
                if (!tier_matches_backend(route.backend, route.minimumTier) ||
                    route.providerContract.empty())
                {
                    report.firstError = "catalog entry has an invalid provider route";
                    return report;
                }
            }

            const auto duplicate = std::find_if(
                kCatalog.begin() + static_cast<std::ptrdiff_t>(index + 1U),
                kCatalog.end(),
                [&technique](const TechniqueEntry& candidate) {
                    return candidate.id == technique.id;
                });
            if (duplicate != kCatalog.end())
            {
                report.firstError = "catalog contains duplicate technique ids";
                return report;
            }

            families[family_index(technique.family)] = true;
            switch (technique.status)
            {
                case TechniqueStatus::reference_only:
                    ++report.referenceOnlyCount;
                    if (technique.evidenceScope != EvidenceScope::reference_repository)
                    {
                        report.firstError = "reference-only entry lacks reference evidence scope";
                        return report;
                    }
                    break;
                case TechniqueStatus::deferred:
                    ++report.deferredCount;
                    break;
                case TechniqueStatus::capability_gated:
                    ++report.capabilityGatedCount;
                    break;
            }
        }

        if (!std::ranges::all_of(families, [](bool populated) { return populated; }))
        {
            report.firstError = "catalog does not cover every technique family";
            return report;
        }

        report.valid = true;
        return report;
    }

    std::string_view backend_label(BackendFamily backend) noexcept
    {
        switch (backend)
        {
            case BackendFamily::cpu: return "CPU";
            case BackendFamily::software: return "Software";
            case BackendFamily::opengles: return "OpenGL ES";
            case BackendFamily::opengl: return "OpenGL";
            case BackendFamily::vulkan: return "Vulkan";
            case BackendFamily::directx12: return "DirectX 12";
        }
        return "Unknown";
    }

    std::string_view tier_label(CapabilityTier tier) noexcept
    {
        switch (tier)
        {
            case CapabilityTier::t0_cpu: return "T0-CPU";
            case CapabilityTier::t1_gles: return "T1-GLES";
            case CapabilityTier::t1_gl: return "T1-GL";
            case CapabilityTier::t2_vk: return "T2-VK";
            case CapabilityTier::t2_dx: return "T2-DX";
            case CapabilityTier::t3_vk: return "T3-VK";
            case CapabilityTier::t3_dx: return "T3-DX";
            case CapabilityTier::t4_vk: return "T4-VK";
            case CapabilityTier::t4_dx: return "T4-DX";
            case CapabilityTier::t5_vk: return "T5-VK";
            case CapabilityTier::t5_dx: return "T5-DX";
        }
        return "Unknown";
    }

    std::string_view family_label(TechniqueFamily family) noexcept
    {
        switch (family)
        {
            case TechniqueFamily::foundation: return "Foundation";
            case TechniqueFamily::materials: return "Materials";
            case TechniqueFamily::lighting: return "Lighting";
            case TechniqueFamily::geometry: return "Geometry";
            case TechniqueFamily::simulation: return "Simulation";
            case TechniqueFamily::post_process: return "Post Process";
            case TechniqueFamily::diagnostics: return "Diagnostics";
            case TechniqueFamily::spatial: return "Spatial";
        }
        return "Unknown";
    }

    std::string_view status_label(TechniqueStatus status) noexcept
    {
        switch (status)
        {
            case TechniqueStatus::reference_only: return "Reference only";
            case TechniqueStatus::deferred: return "Deferred";
            case TechniqueStatus::capability_gated: return "Capability gated";
        }
        return "Unknown";
    }

    std::string_view disposition_label(StationDisposition disposition) noexcept
    {
        switch (disposition)
        {
            case StationDisposition::profile_eligible: return "Profile eligible";
            case StationDisposition::fallback_only: return "Fallback only";
            case StationDisposition::deferred: return "Deferred";
        }
        return "Unknown";
    }
}
