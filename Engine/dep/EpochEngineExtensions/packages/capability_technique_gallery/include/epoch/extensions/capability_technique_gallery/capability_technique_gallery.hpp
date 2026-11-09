// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace epoch::extensions::capability_technique_gallery
{
    enum class BackendFamily : std::uint8_t
    {
        cpu,
        software,
        opengles,
        opengl,
        vulkan,
        directx12
    };

    enum class CapabilityTier : std::uint8_t
    {
        t0_cpu,
        t1_gles,
        t1_gl,
        t2_vk,
        t2_dx,
        t3_vk,
        t3_dx,
        t4_vk,
        t4_dx,
        t5_vk,
        t5_dx
    };

    enum class CapabilityFeature : std::uint64_t
    {
        presentation = 1ULL << 0U,
        render_target = 1ULL << 1U,
        hdr_render_target = 1ULL << 2U,
        compute = 1ULL << 3U,
        storage_buffer = 1ULL << 4U,
        indirect_draw = 1ULL << 5U,
        tessellation = 1ULL << 6U,
        gpu_timing = 1ULL << 7U,
        descriptor_indexing = 1ULL << 8U,
        bindless_resources = 1ULL << 9U,
        ray_query = 1ULL << 10U,
        ray_pipeline = 1ULL << 11U
    };

    using FeatureMask = std::uint64_t;

    [[nodiscard]] constexpr FeatureMask feature_mask(CapabilityFeature feature) noexcept
    {
        return static_cast<FeatureMask>(feature);
    }

    [[nodiscard]] constexpr FeatureMask operator|(
        CapabilityFeature left,
        CapabilityFeature right) noexcept
    {
        return feature_mask(left) | feature_mask(right);
    }
    enum class TechniqueFamily : std::uint8_t
    {
        foundation,
        materials,
        lighting,
        geometry,
        simulation,
        post_process,
        diagnostics,
        spatial
    };

    enum class TechniqueStatus : std::uint8_t
    {
        reference_only,
        deferred,
        capability_gated
    };

    enum class EvidenceScope : std::uint8_t
    {
        reference_repository,
        epoch_architecture_plan
    };

    enum class StationDisposition : std::uint8_t
    {
        profile_eligible,
        fallback_only,
        deferred
    };

    struct ProviderRoute final
    {
        BackendFamily backend{};
        CapabilityTier minimumTier{};
        FeatureMask requiredFeatures{};
        std::string_view providerContract{};
    };

    struct TechniqueEntry final
    {
        std::string_view id{};
        std::string_view label{};
        TechniqueFamily family{};
        std::string_view owningContext{};
        TechniqueStatus status{};
        EvidenceScope evidenceScope{};
        std::string_view evidence{};
        std::string_view dependencies{};
        std::string_view fallback{};
        std::string_view sceneIntent{};
        std::span<const ProviderRoute> routes{};
    };

    struct CapabilityProfile final
    {
        BackendFamily backend{};
        CapabilityTier maximumTier{};
        FeatureMask features{};
        bool validated{};
    };

    struct Vec3 final
    {
        float x{};
        float y{};
        float z{};

        friend constexpr bool operator==(const Vec3&, const Vec3&) = default;
    };

    struct SceneStation final
    {
        std::size_t techniqueIndex{};
        Vec3 position{};
        StationDisposition disposition{StationDisposition::fallback_only};
        std::optional<std::size_t> selectedRoute{};
    };

    struct ScenePlan final
    {
        std::string_view id{"capability_technique_gallery"};
        std::vector<SceneStation> stations{};
        std::size_t profileEligibleCount{};
        std::size_t fallbackCount{};
        std::size_t deferredCount{};
    };

    struct ValidationReport final
    {
        bool valid{};
        std::size_t techniqueCount{};
        std::size_t referenceOnlyCount{};
        std::size_t deferredCount{};
        std::size_t capabilityGatedCount{};
        std::string_view firstError{};
    };

    [[nodiscard]] std::span<const TechniqueEntry> technique_catalog() noexcept;
    [[nodiscard]] const TechniqueEntry* find_technique(std::string_view id) noexcept;
    [[nodiscard]] std::optional<std::size_t> select_route(
        const TechniqueEntry& technique,
        std::span<const CapabilityProfile> profiles) noexcept;
    [[nodiscard]] ScenePlan make_scene_plan(std::span<const CapabilityProfile> profiles);
    [[nodiscard]] ValidationReport validate_catalog() noexcept;

    [[nodiscard]] bool profile_supports(
        const CapabilityProfile& profile,
        const ProviderRoute& route) noexcept;
    [[nodiscard]] std::string_view backend_label(BackendFamily backend) noexcept;
    [[nodiscard]] std::string_view tier_label(CapabilityTier tier) noexcept;
    [[nodiscard]] std::string_view family_label(TechniqueFamily family) noexcept;
    [[nodiscard]] std::string_view status_label(TechniqueStatus status) noexcept;
    [[nodiscard]] std::string_view disposition_label(StationDisposition disposition) noexcept;
}
