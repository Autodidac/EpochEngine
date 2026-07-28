/************************************************
 *  Epoch Engine capability profile foundation
 *
 *  SPDX-License-Identifier: LicenseRef-MIT-NoSell
 *
 *  Provided "AS IS", without warranty of any kind.
 *  See LICENSE for the complete terms.
 ***********************************************/
module;

#include "../src/epoch.common.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

export module capability.profile;

import perf.tier;
import platform.budgets;
import platform.capabilities;
import render.device;

export namespace epochengine::capability
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

    enum class Tier : std::uint8_t
    {
        headless = 0,
        portable_graphics = 1,
        explicit_api = 2,
        explicit_advanced = 3,
        hardware_ray_query = 4,
        ray_tracing_pipeline = 5
    };

    [[nodiscard]] constexpr const char* tier_code(const Tier tier) noexcept
    {
        switch (tier)
        {
        case Tier::headless: return "T0";
        case Tier::portable_graphics: return "T1";
        case Tier::explicit_api: return "T2";
        case Tier::explicit_advanced: return "T3";
        case Tier::hardware_ray_query: return "T4";
        case Tier::ray_tracing_pipeline: return "T5";
        }
        return "T0";
    }

    [[nodiscard]] constexpr const char* backend_code(
        const BackendFamily backend) noexcept
    {
        switch (backend)
        {
        case BackendFamily::cpu: return "CPU";
        case BackendFamily::software: return "CPU";
        case BackendFamily::opengles: return "GLES";
        case BackendFamily::opengl: return "GL";
        case BackendFamily::vulkan: return "VK";
        case BackendFamily::directx12: return "DX";
        }
        return "CPU";
    }

    using Status = RendererCapabilityStatus;

    enum class Feature : std::uint8_t
    {
        scalar_cpu,
        simd_cpu,
        deterministic_reference,
        headless,
        software_raster,
        presentation,
        raster_pipeline,
        textures,
        render_targets,
        instancing,
        compute,
        storage_buffers,
        image_load_store,
        indirect_draw,
        indirect_dispatch,
        atomic_operations,
        descriptor_indexing,
        bindless_resources,
        sparse_resources,
        explicit_queues,
        explicit_synchronization,
        async_compute,
        subgroup_operations,
        mesh_shaders,
        acceleration_structures,
        hardware_ray_query,
        ray_tracing_pipeline,
        temporal_reconstruction,
        procedural_generation,
        spatial_queries,
        navigation,
        resource_residency,
        audio_processing,
        offline_capture,
        count
    };

    static_assert(static_cast<std::uint8_t>(Feature::count) <= 64u);

    using FeatureMask = std::uint64_t;

    [[nodiscard]] constexpr FeatureMask feature_bit(const Feature feature) noexcept
    {
        return FeatureMask{ 1 } << static_cast<std::uint8_t>(feature);
    }

    template <typename... Features>
        requires ((std::same_as<Features, Feature>) && ...)
    [[nodiscard]] constexpr FeatureMask feature_mask(const Features... features) noexcept
    {
        return (FeatureMask{} | ... | feature_bit(features));
    }

    struct FeatureStates final
    {
        FeatureMask present{};
        FeatureMask partial{};
        FeatureMask deferred{};

        constexpr void set(const Feature feature, const Status status) noexcept
        {
            const FeatureMask bit = feature_bit(feature);
            present &= ~bit;
            partial &= ~bit;
            deferred &= ~bit;

            switch (status)
            {
            case Status::present:
                present |= bit;
                break;
            case Status::partial:
                partial |= bit;
                break;
            case Status::deferred:
                deferred |= bit;
                break;
            case Status::missing:
            default:
                break;
            }
        }

        [[nodiscard]] constexpr Status status(const Feature feature) const noexcept
        {
            const FeatureMask bit = feature_bit(feature);
            if ((present & bit) != 0u)
                return Status::present;
            if ((partial & bit) != 0u)
                return Status::partial;
            if ((deferred & bit) != 0u)
                return Status::deferred;
            return Status::missing;
        }

        [[nodiscard]] constexpr bool all_present(const FeatureMask required) const noexcept
        {
            return (present & required) == required;
        }

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return (present & partial) == 0u
                && (present & deferred) == 0u
                && (partial & deferred) == 0u;
        }
    };

    enum class Evidence : std::uint32_t
    {
        none = 0u,
        declared_contract = 1u << 0u,
        build_contract = 1u << 1u,
        pure_contract_test = 1u << 2u,
        runtime_probe = 1u << 3u,
        presentation_probe = 1u << 4u,
        benchmark = 1u << 5u,
        production_observation = 1u << 6u
    };

    using EvidenceMask = std::uint32_t;

    [[nodiscard]] constexpr EvidenceMask evidence_bit(const Evidence evidence) noexcept
    {
        return static_cast<EvidenceMask>(evidence);
    }

    template <typename... EvidenceValues>
        requires ((std::same_as<EvidenceValues, Evidence>) && ...)
    [[nodiscard]] constexpr EvidenceMask evidence_mask(const EvidenceValues... evidence) noexcept
    {
        return (EvidenceMask{} | ... | evidence_bit(evidence));
    }

    [[nodiscard]] constexpr bool has_evidence(
        const EvidenceMask available,
        const Evidence required) noexcept
    {
        return (available & evidence_bit(required)) != 0u;
    }

    [[nodiscard]] constexpr bool has_validation_evidence(
        const EvidenceMask available) noexcept
    {
        constexpr EvidenceMask validation =
            evidence_bit(Evidence::build_contract)
            | evidence_bit(Evidence::pure_contract_test)
            | evidence_bit(Evidence::runtime_probe)
            | evidence_bit(Evidence::presentation_probe)
            | evidence_bit(Evidence::benchmark)
            | evidence_bit(Evidence::production_observation);
        return (available & validation) != 0u;
    }

    enum class StabilityStatus : std::uint8_t
    {
        unproven,
        experimental,
        provisional,
        validated,
        production
    };

    enum class PowerClass : std::uint8_t
    {
        unrestricted,
        constrained,
        balanced,
        performance
    };

    struct DeviceLimits final
    {
        std::uint32_t max_sampled_images{};
        std::uint32_t max_samplers{};
        std::uint32_t max_storage_buffers{};
        std::uint32_t max_uniform_buffers{};
        std::uint32_t max_compute_workgroup_invocations{};
        std::uint32_t max_compute_shared_bytes{};
        std::uint32_t max_texture_dimension_2d{};
        std::uint32_t max_color_attachments{};
    };

    struct MemoryProfile final
    {
        std::uint64_t local_budget_bytes{};
        std::uint64_t shared_budget_bytes{};
        std::uint64_t upload_budget_bytes{};
        bool budget_is_measured{};
        bool supports_pressure_reporting{};
    };

    struct PowerProfile final
    {
        PowerClass power_class{ PowerClass::balanced };
        std::uint32_t sustained_power_milliwatts{};
        bool thermal_state_available{};
        bool battery_state_available{};
    };

    struct Profile final
    {
        BackendFamily backend{ BackendFamily::cpu };
        Tier tier{ Tier::headless };
        FeatureStates features{};
        DeviceLimits limits{};
        MemoryProfile memory{};
        PowerProfile power{};
        Budgets recommended_budgets{};
        perf::tier performance_tier{ perf::tier::desktop_60 };
        StabilityStatus stability{ StabilityStatus::unproven };
        Status status{ Status::missing };
        EvidenceMask evidence{};
    };

    [[nodiscard]] constexpr std::uint8_t status_rank(const Status status) noexcept
    {
        switch (status)
        {
        case Status::present:
            return 3u;
        case Status::partial:
            return 2u;
        case Status::deferred:
            return 1u;
        case Status::missing:
        default:
            return 0u;
        }
    }

    [[nodiscard]] constexpr Status weaker_status(const Status left, const Status right) noexcept
    {
        return status_rank(left) <= status_rank(right) ? left : right;
    }

    [[nodiscard]] constexpr bool valid(const Profile& profile) noexcept
    {
        if (!profile.features.valid())
            return false;

        if (profile.status == Status::present && !has_validation_evidence(profile.evidence))
            return false;

        if (profile.features.present != 0u && !has_validation_evidence(profile.evidence))
            return false;

        return profile.recommended_budgets.cpu_ms >= 0.0f
            && profile.recommended_budgets.gpu_ms >= 0.0f
            && profile.recommended_budgets.min_w <= profile.recommended_budgets.max_w
            && profile.recommended_budgets.min_h <= profile.recommended_budgets.max_h;
    }

    [[nodiscard]] constexpr bool selectable(const Profile& profile) noexcept
    {
        return valid(profile) && profile.status == Status::present;
    }

    namespace detail
    {
        constexpr void set_mask(
            FeatureStates& states,
            FeatureMask mask,
            const Status status) noexcept
        {
            for (std::uint8_t index = 0u;
                 index < static_cast<std::uint8_t>(Feature::count);
                 ++index)
            {
                const auto feature = static_cast<Feature>(index);
                if ((mask & feature_bit(feature)) != 0u)
                    states.set(feature, status);
            }
        }

        [[nodiscard]] constexpr FeatureMask portable_cpu_features() noexcept
        {
            return feature_mask(
                Feature::scalar_cpu,
                Feature::deterministic_reference,
                Feature::headless,
                Feature::temporal_reconstruction,
                Feature::procedural_generation,
                Feature::spatial_queries,
                Feature::navigation,
                Feature::resource_residency,
                Feature::audio_processing,
                Feature::offline_capture);
        }

        [[nodiscard]] constexpr FeatureMask portable_raster_features() noexcept
        {
            return feature_mask(
                Feature::presentation,
                Feature::raster_pipeline,
                Feature::textures,
                Feature::render_targets,
                Feature::instancing,
                Feature::indirect_draw,
                Feature::resource_residency);
        }

        [[nodiscard]] constexpr FeatureMask portable_compute_features() noexcept
        {
            return feature_mask(
                Feature::compute,
                Feature::storage_buffers,
                Feature::image_load_store,
                Feature::indirect_dispatch,
                Feature::atomic_operations);
        }

        [[nodiscard]] constexpr FeatureMask explicit_features() noexcept
        {
            return feature_mask(
                Feature::explicit_queues,
                Feature::explicit_synchronization,
                Feature::compute,
                Feature::storage_buffers,
                Feature::image_load_store,
                Feature::indirect_draw,
                Feature::indirect_dispatch,
                Feature::resource_residency);
        }

        [[nodiscard]] constexpr FeatureMask advanced_explicit_features() noexcept
        {
            return feature_mask(
                Feature::descriptor_indexing,
                Feature::bindless_resources,
                Feature::sparse_resources,
                Feature::async_compute,
                Feature::subgroup_operations,
                Feature::mesh_shaders);
        }

        [[nodiscard]] constexpr Profile base_profile(
            const BackendFamily backend,
            const Tier tier,
            const Status status,
            const EvidenceMask evidence) noexcept
        {
            Profile profile{};
            profile.backend = backend;
            profile.tier = tier;
            profile.status = status;
            profile.evidence = evidence;
            profile.stability = status == Status::present
                ? StabilityStatus::validated
                : (status == Status::partial
                    ? StabilityStatus::experimental
                    : StabilityStatus::unproven);
            return profile;
        }
    }

    [[nodiscard]] constexpr Profile cpu_reference_profile() noexcept
    {
        Profile profile = detail::base_profile(
            BackendFamily::cpu,
            Tier::headless,
            Status::present,
            evidence_mask(Evidence::build_contract, Evidence::pure_contract_test));
        detail::set_mask(profile.features, detail::portable_cpu_features(), Status::present);
        profile.features.set(Feature::simd_cpu, Status::partial);
        profile.features.set(Feature::software_raster, Status::partial);
        profile.memory.local_budget_bytes = 512ull * 1024ull * 1024ull;
        profile.memory.shared_budget_bytes = 512ull * 1024ull * 1024ull;
        profile.memory.upload_budget_bytes = 32ull * 1024ull * 1024ull;
        profile.memory.supports_pressure_reporting = true;
        profile.power.power_class = PowerClass::balanced;
        profile.performance_tier = perf::tier::desktop_60;
        return profile;
    }

    [[nodiscard]] constexpr Profile software_profile(
        const Status status = Status::deferred,
        const EvidenceMask evidence = evidence_bit(Evidence::declared_contract)) noexcept
    {
        Profile profile = cpu_reference_profile();
        profile.backend = BackendFamily::software;
        profile.status = status;
        profile.evidence = evidence;
        profile.stability = status == Status::present
            ? StabilityStatus::validated
            : StabilityStatus::experimental;
        profile.features.set(Feature::software_raster, status);
        profile.features.set(Feature::presentation, Status::deferred);
        return profile;
    }

    [[nodiscard]] constexpr Profile opengles_profile(
        const Tier tier = Tier::portable_graphics,
        const Status status = Status::deferred,
        const EvidenceMask evidence = evidence_bit(Evidence::declared_contract),
        const bool compute_supported = false) noexcept
    {
        Profile profile = detail::base_profile(
            BackendFamily::opengles,
            (tier >= Tier::portable_graphics && tier < Tier::explicit_api)
                ? tier
                : Tier::portable_graphics,
            status,
            evidence);
        detail::set_mask(profile.features, detail::portable_raster_features(), status);
        if (compute_supported)
            detail::set_mask(profile.features, detail::portable_compute_features(), status);
        profile.limits.max_sampled_images = 256u;
        profile.limits.max_samplers = 64u;
        profile.limits.max_storage_buffers = compute_supported ? 16u : 0u;
        profile.limits.max_uniform_buffers = 24u;
        profile.limits.max_texture_dimension_2d = 4096u;
        profile.limits.max_color_attachments = 4u;
        profile.memory.local_budget_bytes = 256ull * 1024ull * 1024ull;
        profile.memory.shared_budget_bytes = 512ull * 1024ull * 1024ull;
        profile.memory.upload_budget_bytes = 8ull * 1024ull * 1024ull;
        profile.memory.supports_pressure_reporting = true;
        profile.power.power_class = PowerClass::constrained;
        profile.power.thermal_state_available = true;
        profile.power.battery_state_available = true;
        profile.performance_tier = perf::tier::mobile_30;
        profile.recommended_budgets.cpu_ms = 10.0f;
        profile.recommended_budgets.gpu_ms = 20.0f;
        profile.recommended_budgets.vram_budget_bytes = profile.memory.local_budget_bytes;
        profile.recommended_budgets.upload_budget_bytes = profile.memory.upload_budget_bytes;
        profile.recommended_budgets.max_w = 1280u;
        profile.recommended_budgets.max_h = 720u;
        profile.recommended_budgets.max_lights = 32u;
        profile.recommended_budgets.shadow_cascades = 1u;
        return profile;
    }

    [[nodiscard]] constexpr Profile opengl_profile(
        const Tier tier = Tier::portable_graphics,
        const Status status = Status::partial,
        const EvidenceMask evidence = evidence_bit(Evidence::build_contract),
        const bool compute_supported = false) noexcept
    {
        Profile profile = detail::base_profile(
            BackendFamily::opengl,
            (tier >= Tier::portable_graphics && tier < Tier::explicit_api)
                ? tier
                : Tier::portable_graphics,
            status,
            evidence);
        detail::set_mask(profile.features, detail::portable_raster_features(), status);
        if (compute_supported)
            detail::set_mask(profile.features, detail::portable_compute_features(), status);
        profile.limits.max_sampled_images = 2048u;
        profile.limits.max_samplers = 1024u;
        profile.limits.max_storage_buffers = compute_supported ? 128u : 0u;
        profile.limits.max_uniform_buffers = 128u;
        profile.limits.max_texture_dimension_2d = 16384u;
        profile.limits.max_color_attachments = 8u;
        profile.memory.local_budget_bytes = 1024ull * 1024ull * 1024ull;
        profile.memory.upload_budget_bytes = 32ull * 1024ull * 1024ull;
        profile.power.power_class = PowerClass::balanced;
        profile.performance_tier = perf::tier::desktop_60;
        profile.recommended_budgets.vram_budget_bytes = profile.memory.local_budget_bytes;
        profile.recommended_budgets.upload_budget_bytes = profile.memory.upload_budget_bytes;
        return profile;
    }

    [[nodiscard]] constexpr Profile vulkan_profile(
        const Tier tier = Tier::explicit_api,
        const Status status = Status::partial,
        const EvidenceMask evidence = evidence_bit(Evidence::build_contract)) noexcept
    {
        Profile profile = detail::base_profile(
            BackendFamily::vulkan,
            tier < Tier::explicit_api ? Tier::explicit_api : tier,
            status,
            evidence);
        detail::set_mask(profile.features, detail::portable_raster_features(), status);
        detail::set_mask(profile.features, detail::explicit_features(), status);
        if (profile.tier >= Tier::explicit_advanced)
            detail::set_mask(profile.features, detail::advanced_explicit_features(), status);
        if (profile.tier >= Tier::hardware_ray_query)
        {
            detail::set_mask(
                profile.features,
                feature_mask(Feature::acceleration_structures, Feature::hardware_ray_query),
                status);
        }
        if (profile.tier >= Tier::ray_tracing_pipeline)
            profile.features.set(Feature::ray_tracing_pipeline, status);
        profile.limits.max_sampled_images = 8192u;
        profile.limits.max_samplers = 4096u;
        profile.limits.max_storage_buffers = 256u;
        profile.limits.max_uniform_buffers = 256u;
        profile.limits.max_texture_dimension_2d = 16384u;
        profile.limits.max_color_attachments = 8u;
        profile.memory.local_budget_bytes = 2048ull * 1024ull * 1024ull;
        profile.memory.upload_budget_bytes = 64ull * 1024ull * 1024ull;
        profile.power.power_class = PowerClass::performance;
        profile.performance_tier = perf::tier::editor_120;
        profile.recommended_budgets.vram_budget_bytes = profile.memory.local_budget_bytes;
        profile.recommended_budgets.upload_budget_bytes = profile.memory.upload_budget_bytes;
        return profile;
    }

    [[nodiscard]] constexpr Profile directx12_profile(
        const Tier tier = Tier::explicit_api,
        const Status status = Status::partial,
        const EvidenceMask evidence = evidence_bit(Evidence::build_contract)) noexcept
    {
        Profile profile = vulkan_profile(tier, status, evidence);
        profile.backend = BackendFamily::directx12;
        return profile;
    }

    [[nodiscard]] inline Profile adapt_platform_capabilities(
        const BackendFamily backend,
        const Tier tier,
        const Capabilities& capabilities,
        const Budgets& budgets,
        const perf::tier performance_tier,
        const Status status,
        const EvidenceMask evidence) noexcept
    {
        Profile profile = detail::base_profile(backend, tier, status, evidence);
        profile.recommended_budgets = budgets;
        profile.performance_tier = performance_tier;
        profile.limits.max_sampled_images = capabilities.max_sampled_images;
        profile.limits.max_samplers = capabilities.max_samplers;
        profile.limits.max_storage_buffers = capabilities.max_storage_buffers;
        profile.limits.max_uniform_buffers = capabilities.max_uniform_buffers;
        profile.memory.local_budget_bytes = budgets.vram_budget_bytes;
        profile.memory.upload_budget_bytes = budgets.upload_budget_bytes;
        profile.memory.supports_pressure_reporting = true;
        profile.power.thermal_state_available = true;

        const auto set_if = [&](const Feature feature, const bool supported) noexcept
        {
            profile.features.set(feature, supported ? status : Status::missing);
        };

        set_if(Feature::descriptor_indexing, capabilities.descriptor_indexing);
        set_if(Feature::bindless_resources, capabilities.bindless_textures);
        set_if(Feature::sparse_resources, capabilities.sparse_resources);
        set_if(Feature::indirect_draw, capabilities.indirect_draw);
        set_if(Feature::indirect_dispatch, capabilities.multi_draw_indirect);
        set_if(Feature::subgroup_operations, capabilities.subgroup_ops);
        set_if(Feature::async_compute, capabilities.async_compute);
        set_if(Feature::ray_tracing_pipeline, capabilities.ray_tracing);
        set_if(Feature::mesh_shaders, capabilities.mesh_shaders);
        return profile;
    }

    [[nodiscard]] constexpr BackendFamily backend_family_for(
        const RendererBackendKind backend) noexcept
    {
        switch (backend)
        {
        case RendererBackendKind::software:
            return BackendFamily::software;
        case RendererBackendKind::opengl:
        case RendererBackendKind::sdl3:
        case RendererBackendKind::sfml3:
        case RendererBackendKind::raylib3:
            return BackendFamily::opengl;
        case RendererBackendKind::vulkan:
            return BackendFamily::vulkan;
        case RendererBackendKind::directx:
            return BackendFamily::directx12;
        case RendererBackendKind::null:
        default:
            return BackendFamily::cpu;
        }
    }

    [[nodiscard]] constexpr Profile adapt_renderer_capabilities(
        const RendererCapabilities& capabilities,
        const RendererCapabilityReport& report,
        const Tier tier,
        const EvidenceMask evidence) noexcept
    {
        Profile profile = detail::base_profile(
            backend_family_for(capabilities.backend),
            tier,
            report.build_graph_proof,
            evidence);

        const Status descriptor_status = weaker_status(
            report.descriptor_contract,
            report.build_graph_proof);
        const auto set_descriptor = [&](const Feature feature, const bool supported) constexpr noexcept
        {
            profile.features.set(feature, supported ? descriptor_status : Status::missing);
        };

        set_descriptor(Feature::textures, capabilities.textures);
        set_descriptor(Feature::render_targets, capabilities.render_targets);
        set_descriptor(Feature::raster_pipeline, capabilities.pipelines);
        set_descriptor(Feature::resource_residency, capabilities.binding_sets);
        set_descriptor(Feature::indirect_draw, capabilities.command_lists);
        profile.features.set(
            Feature::presentation,
            capabilities.render_targets ? report.presentation_proof : Status::missing);
        return profile;
    }

    enum class Subsystem : std::uint8_t
    {
        rendering,
        spatial_queries,
        voxel_traversal,
        volumetrics,
        navigation,
        procedural_generation,
        physics,
        streaming,
        resource_residency,
        temporal_reconstruction,
        audio_processing,
        editor_picking,
        offline_capture
    };

    enum class DataRepresentation : std::uint8_t
    {
        analytic,
        triangles,
        voxels,
        signed_distance_field,
        sparse_tiles,
        clusters,
        acceleration_structure,
        audio_stream,
        count
    };

    using DataRepresentationMask = std::uint32_t;

    [[nodiscard]] constexpr DataRepresentationMask representation_bit(
        const DataRepresentation representation) noexcept
    {
        return DataRepresentationMask{ 1 } << static_cast<std::uint8_t>(representation);
    }

    template <typename... Representations>
        requires ((std::same_as<Representations, DataRepresentation>) && ...)
    [[nodiscard]] constexpr DataRepresentationMask representation_mask(
        const Representations... representations) noexcept
    {
        return (DataRepresentationMask{} | ... | representation_bit(representations));
    }

    enum class Determinism : std::uint8_t
    {
        unspecified,
        repeatable,
        reference
    };

    struct QualityRange final
    {
        std::uint8_t minimum{};
        std::uint8_t maximum{ 100u };
    };

    struct CostModel final
    {
        std::uint64_t working_memory_bytes{};
        std::uint64_t startup_memory_bytes{};
        std::uint32_t latency_microseconds{};
        std::uint32_t startup_microseconds{};
        std::uint32_t sustained_power_milliwatts{};
    };

    struct SubsystemProfile final
    {
        std::uint64_t implementation_id{};
        Subsystem subsystem{ Subsystem::rendering };
        Profile capability{};
        FeatureMask implementation_features{};
        DataRepresentationMask representations{};
        QualityRange quality{};
        Determinism determinism{ Determinism::unspecified };
        CostModel cost{};
        StabilityStatus stability{ StabilityStatus::unproven };
        Status status{ Status::missing };
        EvidenceMask evidence{};
        bool software_fallback{};
    };

    struct Requirement final
    {
        Subsystem subsystem{ Subsystem::rendering };
        Tier minimum_tier{ Tier::headless };
        FeatureMask required_features{};
        FeatureMask optional_features{};
        DataRepresentationMask accepted_representations{};
        std::uint8_t minimum_quality{};
        std::uint64_t maximum_memory_bytes{};
        std::uint32_t maximum_latency_microseconds{};
        std::uint32_t maximum_power_milliwatts{};
        StabilityStatus minimum_stability{ StabilityStatus::experimental };
        Determinism minimum_determinism{ Determinism::unspecified };
        BackendFamily preferred_backend{ BackendFamily::cpu };
        bool has_preferred_backend{};
        bool require_preferred_backend{};
    };

    enum class MatchFailure : std::uint8_t
    {
        none,
        wrong_subsystem,
        invalid_profile,
        unavailable,
        missing_evidence,
        wrong_backend,
        insufficient_tier,
        missing_feature,
        unsupported_representation,
        insufficient_quality,
        insufficient_determinism,
        insufficient_stability,
        memory_budget,
        latency_budget,
        power_budget,
        no_candidate
    };

    struct MatchResult final
    {
        bool matched{};
        MatchFailure failure{ MatchFailure::no_candidate };
    };

    [[nodiscard]] constexpr MatchResult match(
        const SubsystemProfile& candidate,
        const Requirement& requirement) noexcept
    {
        if (candidate.subsystem != requirement.subsystem)
            return { false, MatchFailure::wrong_subsystem };
        if (candidate.implementation_id == 0u || !valid(candidate.capability))
            return { false, MatchFailure::invalid_profile };
        if (candidate.status != Status::present || candidate.capability.status != Status::present)
            return { false, MatchFailure::unavailable };
        if (candidate.evidence == 0u || candidate.capability.evidence == 0u)
            return { false, MatchFailure::missing_evidence };
        if (requirement.require_preferred_backend
            && candidate.capability.backend != requirement.preferred_backend)
        {
            return { false, MatchFailure::wrong_backend };
        }
        if (candidate.capability.tier < requirement.minimum_tier)
            return { false, MatchFailure::insufficient_tier };

        const FeatureMask available =
            candidate.capability.features.present | candidate.implementation_features;
        if ((available & requirement.required_features) != requirement.required_features)
            return { false, MatchFailure::missing_feature };

        if (requirement.accepted_representations != 0u
            && (candidate.representations & requirement.accepted_representations) == 0u)
        {
            return { false, MatchFailure::unsupported_representation };
        }
        if (candidate.quality.maximum < requirement.minimum_quality)
            return { false, MatchFailure::insufficient_quality };
        if (candidate.determinism < requirement.minimum_determinism)
            return { false, MatchFailure::insufficient_determinism };
        if (candidate.stability < requirement.minimum_stability)
            return { false, MatchFailure::insufficient_stability };
        if (requirement.maximum_memory_bytes != 0u
            && candidate.cost.working_memory_bytes > requirement.maximum_memory_bytes)
        {
            return { false, MatchFailure::memory_budget };
        }
        if (candidate.capability.recommended_budgets.vram_budget_bytes != 0u
            && candidate.cost.working_memory_bytes
                > candidate.capability.recommended_budgets.vram_budget_bytes)
        {
            return { false, MatchFailure::memory_budget };
        }
        if (requirement.maximum_latency_microseconds != 0u
            && candidate.cost.latency_microseconds > requirement.maximum_latency_microseconds)
        {
            return { false, MatchFailure::latency_budget };
        }
        if (requirement.maximum_power_milliwatts != 0u
            && candidate.cost.sustained_power_milliwatts > requirement.maximum_power_milliwatts)
        {
            return { false, MatchFailure::power_budget };
        }
        return { true, MatchFailure::none };
    }

    namespace detail
    {
        [[nodiscard]] constexpr std::uint8_t stability_rank(
            const StabilityStatus status) noexcept
        {
            return static_cast<std::uint8_t>(status);
        }

        [[nodiscard]] constexpr bool better_candidate(
            const SubsystemProfile& candidate,
            const SubsystemProfile& current,
            const Requirement& requirement) noexcept
        {
            const bool candidate_preferred = requirement.has_preferred_backend
                && candidate.capability.backend == requirement.preferred_backend;
            const bool current_preferred = requirement.has_preferred_backend
                && current.capability.backend == requirement.preferred_backend;
            if (candidate_preferred != current_preferred)
                return candidate_preferred;

            const FeatureMask candidate_available =
                candidate.capability.features.present | candidate.implementation_features;
            const FeatureMask current_available =
                current.capability.features.present | current.implementation_features;
            const auto candidate_optional = std::popcount(
                candidate_available & requirement.optional_features);
            const auto current_optional = std::popcount(
                current_available & requirement.optional_features);
            if (candidate_optional != current_optional)
                return candidate_optional > current_optional;

            if (candidate.quality.maximum != current.quality.maximum)
                return candidate.quality.maximum > current.quality.maximum;
            if (candidate.stability != current.stability)
            {
                return stability_rank(candidate.stability)
                    > stability_rank(current.stability);
            }

            const auto candidate_evidence = std::popcount(candidate.evidence);
            const auto current_evidence = std::popcount(current.evidence);
            if (candidate_evidence != current_evidence)
                return candidate_evidence > current_evidence;
            if (candidate.cost.latency_microseconds != current.cost.latency_microseconds)
                return candidate.cost.latency_microseconds < current.cost.latency_microseconds;
            if (candidate.cost.working_memory_bytes != current.cost.working_memory_bytes)
                return candidate.cost.working_memory_bytes < current.cost.working_memory_bytes;
            if (candidate.cost.startup_microseconds != current.cost.startup_microseconds)
                return candidate.cost.startup_microseconds < current.cost.startup_microseconds;
            if (candidate.cost.sustained_power_milliwatts
                != current.cost.sustained_power_milliwatts)
            {
                return candidate.cost.sustained_power_milliwatts
                    < current.cost.sustained_power_milliwatts;
            }
            if (candidate.software_fallback != current.software_fallback)
                return !candidate.software_fallback;

            // Backend family is deliberately absent from ordering. Equivalent
            // providers resolve by their stable implementation identity.
            return candidate.implementation_id < current.implementation_id;
        }
    }

    struct SelectionResult final
    {
        bool matched{};
        std::size_t index{ (std::numeric_limits<std::size_t>::max)() };
        std::uint64_t implementation_id{};
        MatchFailure failure{ MatchFailure::no_candidate };
    };

    [[nodiscard]] constexpr SelectionResult select(
        const std::span<const SubsystemProfile> candidates,
        const Requirement& requirement) noexcept
    {
        SelectionResult result{};

        for (std::size_t index = 0u; index < candidates.size(); ++index)
        {
            const SubsystemProfile& candidate = candidates[index];
            if (!match(candidate, requirement).matched)
                continue;

            if (!result.matched
                || detail::better_candidate(candidate, candidates[result.index], requirement))
            {
                result.matched = true;
                result.index = index;
                result.implementation_id = candidate.implementation_id;
                result.failure = MatchFailure::none;
            }
        }
        return result;
    }

    inline constexpr std::size_t max_project_requirements = 16u;

    struct ProjectRequirements final
    {
        std::array<Requirement, max_project_requirements> requirements{};
        std::size_t count{};

        [[nodiscard]] constexpr bool add(const Requirement& requirement) noexcept
        {
            if (count >= requirements.size())
                return false;
            requirements[count++] = requirement;
            return true;
        }
    };

    [[nodiscard]] constexpr bool valid(const ProjectRequirements& project) noexcept
    {
        if (project.count == 0u || project.count > project.requirements.size())
            return false;

        for (std::size_t left = 0u; left < project.count; ++left)
        {
            for (std::size_t right = left + 1u; right < project.count; ++right)
            {
                if (project.requirements[left].subsystem
                    == project.requirements[right].subsystem)
                {
                    return false;
                }
            }
        }
        return true;
    }

    struct ProjectMatchResult final
    {
        std::array<SelectionResult, max_project_requirements> selections{};
        std::size_t count{};
        bool matched{};
    };

    [[nodiscard]] constexpr ProjectMatchResult match_project(
        const ProjectRequirements& project,
        const std::span<const SubsystemProfile> candidates) noexcept
    {
        ProjectMatchResult result{};
        if (!valid(project))
            return result;

        result.count = project.count;
        result.matched = true;
        for (std::size_t index = 0u; index < project.count; ++index)
        {
            result.selections[index] = select(candidates, project.requirements[index]);
            result.matched = result.matched && result.selections[index].matched;
        }
        return result;
    }

    enum class ContractCheck : std::uint32_t
    {
        cpu_only_selection = 1u << 0u,
        gles_baseline_selection = 1u << 1u,
        opengl_compute_selection = 1u << 2u,
        explicit_api_equivalence = 1u << 3u,
        missing_feature_fallback = 1u << 4u,
        partial_rejection = 1u << 5u,
        project_validation = 1u << 6u,
        per_subsystem_selection = 1u << 7u,
        deterministic_selection = 1u << 8u,
        no_capability_overclaim = 1u << 9u
    };

    struct ContractCheckReport final
    {
        std::uint32_t executed{};
        std::uint32_t failed{};

        constexpr void record(const ContractCheck check, const bool passed) noexcept
        {
            const auto bit = static_cast<std::uint32_t>(check);
            executed |= bit;
            if (!passed)
                failed |= bit;
        }

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return executed != 0u && failed == 0u;
        }
    };

    namespace detail
    {
        [[nodiscard]] constexpr SubsystemProfile implementation(
            const std::uint64_t id,
            const Subsystem subsystem,
            const Profile& capability,
            const FeatureMask implementation_features,
            const DataRepresentationMask representations,
            const std::uint8_t quality,
            const Determinism determinism,
            const CostModel cost,
            const bool fallback = false) noexcept
        {
            return SubsystemProfile{
                .implementation_id = id,
                .subsystem = subsystem,
                .capability = capability,
                .implementation_features = implementation_features,
                .representations = representations,
                .quality = { 0u, quality },
                .determinism = determinism,
                .cost = cost,
                .stability = StabilityStatus::validated,
                .status = Status::present,
                .evidence = evidence_mask(
                    Evidence::build_contract,
                    Evidence::pure_contract_test),
                .software_fallback = fallback
            };
        }
    }

    [[nodiscard]] constexpr ContractCheckReport run_contract_checks() noexcept
    {
        constexpr EvidenceMask proven = evidence_mask(
            Evidence::build_contract,
            Evidence::pure_contract_test,
            Evidence::runtime_probe);

        const Profile cpu = cpu_reference_profile();
        const Profile gles = opengles_profile(
            Tier::portable_graphics, Status::present, proven);
        const Profile gl_compute = opengl_profile(
            Tier::portable_graphics, Status::present, proven, true);
        const Profile vk = vulkan_profile(Tier::explicit_api, Status::present, proven);
        const Profile dx = directx12_profile(Tier::explicit_api, Status::present, proven);

        const auto cpu_spatial = detail::implementation(
            10u,
            Subsystem::spatial_queries,
            cpu,
            feature_mask(Feature::spatial_queries),
            representation_mask(DataRepresentation::analytic, DataRepresentation::triangles),
            70u,
            Determinism::reference,
            CostModel{ 8ull * 1024ull * 1024ull, 1ull * 1024ull * 1024ull, 900u, 20u, 4500u },
            true);
        const auto cpu_render = detail::implementation(
            11u,
            Subsystem::rendering,
            cpu,
            feature_mask(Feature::software_raster),
            representation_mask(DataRepresentation::triangles),
            40u,
            Determinism::reference,
            CostModel{ 64ull * 1024ull * 1024ull, 4ull * 1024ull * 1024ull, 8000u, 100u, 5500u },
            true);
        const auto gles_render = detail::implementation(
            20u,
            Subsystem::rendering,
            gles,
            feature_mask(Feature::raster_pipeline),
            representation_mask(DataRepresentation::triangles),
            60u,
            Determinism::repeatable,
            CostModel{ 96ull * 1024ull * 1024ull, 16ull * 1024ull * 1024ull, 3000u, 5000u, 4000u });
        const auto gl_compute_provider = detail::implementation(
            30u,
            Subsystem::procedural_generation,
            gl_compute,
            feature_mask(Feature::compute, Feature::procedural_generation),
            representation_mask(DataRepresentation::sparse_tiles),
            80u,
            Determinism::repeatable,
            CostModel{ 128ull * 1024ull * 1024ull, 24ull * 1024ull * 1024ull, 1200u, 9000u, 6500u });
        const auto vk_render = detail::implementation(
            400u,
            Subsystem::rendering,
            vk,
            feature_mask(Feature::raster_pipeline),
            representation_mask(DataRepresentation::triangles),
            90u,
            Determinism::repeatable,
            CostModel{ 192ull * 1024ull * 1024ull, 32ull * 1024ull * 1024ull, 700u, 12000u, 9000u });
        const auto dx_render = detail::implementation(
            300u,
            Subsystem::rendering,
            dx,
            feature_mask(Feature::raster_pipeline),
            representation_mask(DataRepresentation::triangles),
            90u,
            Determinism::repeatable,
            CostModel{ 192ull * 1024ull * 1024ull, 32ull * 1024ull * 1024ull, 700u, 12000u, 9000u });

        ContractCheckReport report{};

        {
            const std::array candidates{ cpu_spatial };
            Requirement requirement{};
            requirement.subsystem = Subsystem::spatial_queries;
            requirement.required_features = feature_mask(
                Feature::scalar_cpu,
                Feature::spatial_queries);
            requirement.minimum_determinism = Determinism::reference;
            report.record(
                ContractCheck::cpu_only_selection,
                select(candidates, requirement).implementation_id == cpu_spatial.implementation_id);
        }

        {
            const std::array candidates{ cpu_render, gles_render };
            Requirement requirement{};
            requirement.subsystem = Subsystem::rendering;
            requirement.required_features = feature_mask(
                Feature::presentation,
                Feature::raster_pipeline);
            requirement.preferred_backend = BackendFamily::opengles;
            requirement.has_preferred_backend = true;
            requirement.require_preferred_backend = true;
            report.record(
                ContractCheck::gles_baseline_selection,
                select(candidates, requirement).implementation_id == gles_render.implementation_id);
        }

        {
            const std::array candidates{ gl_compute_provider };
            Requirement requirement{};
            requirement.subsystem = Subsystem::procedural_generation;
            requirement.minimum_tier = Tier::portable_graphics;
            requirement.required_features = feature_mask(
                Feature::compute,
                Feature::storage_buffers,
                Feature::procedural_generation);
            report.record(
                ContractCheck::opengl_compute_selection,
                select(candidates, requirement).implementation_id
                    == gl_compute_provider.implementation_id);
        }

        {
            const std::array candidates{ vk_render, dx_render };
            Requirement requirement{};
            requirement.subsystem = Subsystem::rendering;
            requirement.minimum_tier = Tier::explicit_api;
            requirement.required_features = feature_mask(
                Feature::raster_pipeline,
                Feature::explicit_synchronization);
            const SelectionResult selected = select(candidates, requirement);
            report.record(
                ContractCheck::explicit_api_equivalence,
                selected.implementation_id == dx_render.implementation_id);
            report.record(
                ContractCheck::deterministic_selection,
                selected.implementation_id
                    == select(candidates, requirement).implementation_id);
        }

        {
            SubsystemProfile unsupported_gl = gl_compute_provider;
            unsupported_gl.implementation_id = 31u;
            unsupported_gl.subsystem = Subsystem::spatial_queries;
            unsupported_gl.capability.features.set(Feature::spatial_queries, Status::missing);
            unsupported_gl.implementation_features = 0u;
            const std::array candidates{ unsupported_gl, cpu_spatial };
            Requirement requirement{};
            requirement.subsystem = Subsystem::spatial_queries;
            requirement.required_features = feature_mask(Feature::spatial_queries);
            report.record(
                ContractCheck::missing_feature_fallback,
                select(candidates, requirement).implementation_id
                    == cpu_spatial.implementation_id);
        }

        {
            SubsystemProfile partial = gles_render;
            partial.capability.status = Status::partial;
            const std::array candidates{ partial };
            Requirement requirement{};
            requirement.subsystem = Subsystem::rendering;
            report.record(
                ContractCheck::partial_rejection,
                !select(candidates, requirement).matched);
        }

        {
            const std::array candidates{ cpu_spatial, gles_render };
            ProjectRequirements project{};
            Requirement rendering{};
            rendering.subsystem = Subsystem::rendering;
            rendering.required_features = feature_mask(Feature::raster_pipeline);
            Requirement spatial{};
            spatial.subsystem = Subsystem::spatial_queries;
            spatial.required_features = feature_mask(Feature::spatial_queries);
            const bool added = project.add(rendering) && project.add(spatial);
            const ProjectMatchResult matched = match_project(project, candidates);
            report.record(
                ContractCheck::project_validation,
                added && matched.matched && matched.count == 2u);
            report.record(
                ContractCheck::per_subsystem_selection,
                matched.matched
                    && matched.selections[0].implementation_id == gles_render.implementation_id
                    && matched.selections[1].implementation_id == cpu_spatial.implementation_id);
        }

        {
            const Profile unproven_gles = opengles_profile();
            const Profile unproven_vk = vulkan_profile();
            const Profile declaration_only_gles = opengles_profile(
                Tier::portable_graphics,
                Status::present,
                evidence_bit(Evidence::declared_contract));
            SubsystemProfile overclaim = vk_render;
            overclaim.capability = unproven_vk;
            const std::array candidates{ overclaim };
            Requirement requirement{};
            requirement.subsystem = Subsystem::rendering;
            report.record(
                ContractCheck::no_capability_overclaim,
                unproven_gles.status != Status::present
                    && unproven_vk.status != Status::present
                    && !selectable(declaration_only_gles)
                    && !select(candidates, requirement).matched);
        }

        return report;
    }

    static_assert(run_contract_checks().passed());
}
