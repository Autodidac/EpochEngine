// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/capability_technique_gallery/capability_technique_gallery.hpp"

#include <array>
#include <stdexcept>

using namespace epoch::extensions::capability_technique_gallery;

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

int main()
{
    const auto validation = validate_catalog();
    require(validation.valid, "capability technique catalog validation failed");
    require(validation.techniqueCount == 46U, "capability technique catalog coverage changed");
    require(validation.referenceOnlyCount == 38U, "reference technique evidence count changed");
    require(validation.deferredCount == 6U, "deferred technique count changed");
    require(validation.capabilityGatedCount == 2U, "capability-gated technique count changed");

    const auto* pbr = find_technique("material.pbr");
    const auto* deferred = find_technique("render.deferred");
    const auto* rayQuery = find_technique("spatial.ray_query");
    const auto* rayPipeline = find_technique("render.ray_pipeline");
    require(pbr != nullptr && deferred != nullptr && rayQuery != nullptr && rayPipeline != nullptr,
        "required capability technique is missing");

    constexpr auto presentationFeatures =
        feature_mask(CapabilityFeature::presentation) |
        feature_mask(CapabilityFeature::render_target) |
        feature_mask(CapabilityFeature::hdr_render_target) |
        feature_mask(CapabilityFeature::indirect_draw) |
        feature_mask(CapabilityFeature::tessellation) |
        feature_mask(CapabilityFeature::gpu_timing);
    constexpr auto computeFeatures =
        presentationFeatures |
        feature_mask(CapabilityFeature::compute) |
        feature_mask(CapabilityFeature::storage_buffer);
    constexpr std::array desktopProfiles{
        CapabilityProfile{BackendFamily::cpu, CapabilityTier::t0_cpu, 0U, true},
        CapabilityProfile{BackendFamily::opengl, CapabilityTier::t1_gl, computeFeatures, true},
        CapabilityProfile{
            BackendFamily::vulkan,
            CapabilityTier::t4_vk,
            presentationFeatures | feature_mask(CapabilityFeature::ray_query),
            true}
    };
    constexpr std::array reversedDesktopProfiles{
        CapabilityProfile{
            BackendFamily::vulkan,
            CapabilityTier::t4_vk,
            presentationFeatures | feature_mask(CapabilityFeature::ray_query),
            true},
        CapabilityProfile{BackendFamily::opengl, CapabilityTier::t1_gl, computeFeatures, true},
        CapabilityProfile{BackendFamily::cpu, CapabilityTier::t0_cpu, 0U, true}
    };
    constexpr std::array cpuOnlyProfiles{
        CapabilityProfile{BackendFamily::cpu, CapabilityTier::t0_cpu, 0U, true}
    };
    constexpr std::array presentationOnlyProfiles{
        CapabilityProfile{
            BackendFamily::opengl,
            CapabilityTier::t1_gl,
            feature_mask(CapabilityFeature::presentation),
            true}
    };
    constexpr std::array unvalidatedProfiles{
        CapabilityProfile{
            BackendFamily::vulkan,
            CapabilityTier::t5_vk,
            feature_mask(CapabilityFeature::ray_pipeline),
            false}
    };
    const auto pbrRoute = select_route(*pbr, desktopProfiles);
    const auto pbrRouteReversed = select_route(*pbr, reversedDesktopProfiles);
    require(pbrRoute.has_value(), "desktop profile did not select a PBR route");
    require(pbrRoute == pbrRouteReversed, "route selection depends on profile order");
    require(!select_route(*pbr, cpuOnlyProfiles).has_value(),
        "CPU-only profile incorrectly selected a raster PBR route");
    require(!select_route(*rayPipeline, unvalidatedProfiles).has_value(),
        "unvalidated profile incorrectly selected a T5 route");

    const auto* computeParticles = find_technique("particles.compute");
    require(computeParticles != nullptr, "compute particle station is missing");
    require(!select_route(*computeParticles, presentationOnlyProfiles).has_value(),
        "T1 presentation was incorrectly treated as proven T1 compute");
    require(select_route(*computeParticles, desktopProfiles).has_value(),
        "feature-proven T1 compute route was not selected");
    const auto rayQueryRoute = select_route(*rayQuery, desktopProfiles);
    require(rayQueryRoute.has_value(), "validated T4 Vulkan profile did not select ray-query route");
    require(rayQuery->routes[*rayQueryRoute].minimumTier == CapabilityTier::t4_vk,
        "ray-query route selected the wrong minimum tier");

    const auto plan = make_scene_plan(desktopProfiles);
    require(plan.stations.size() == validation.techniqueCount,
        "gallery scene did not create one station per technique");
    require(plan.profileEligibleCount + plan.fallbackCount + plan.deferredCount == plan.stations.size(),
        "gallery scene disposition counts do not cover every station");
    require(plan.deferredCount == validation.deferredCount,
        "gallery scene activated a deferred technique");

    const auto cpuPlan = make_scene_plan(cpuOnlyProfiles);
    require(cpuPlan.profileEligibleCount > 0U,
        "CPU-only gallery should retain CPU technique stations");
    require(cpuPlan.fallbackCount > 0U,
        "CPU-only gallery should expose raster fallbacks");

    for (std::size_t index = 0; index < plan.stations.size(); ++index)
    {
        require(plan.stations[index].techniqueIndex == index,
            "gallery scene station order is not deterministic");
        require(plan.stations[index].position == make_scene_plan(desktopProfiles).stations[index].position,
            "gallery scene station placement is not deterministic");
    }

    require(deferred->status == TechniqueStatus::deferred,
        "deferred renderer path was incorrectly promoted");
    return 0;
}
