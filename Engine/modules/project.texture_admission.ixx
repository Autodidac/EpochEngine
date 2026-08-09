/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

export module project.texture_admission;

import asset.texture_artifact;
import capability.profile;
import platform.budgets;

export namespace epochengine::project_textures
{
    enum class TextureAdmissionStatus : std::uint8_t
    {
        rejected,
        experimental,
        admitted
    };

    enum class TextureAdmissionReason : std::uint8_t
    {
        none,
        invalid_policy,
        invalid_platform_budgets,
        invalid_artifact,
        unsupported_format,
        unsupported_color_space,
        unsupported_mip_count,
        missing_capability_evidence,
        unsupported_sampled_image_representation,
        renderer_requirement_rejected,
        experimental_not_allowed,
        invalid_effective_limits,
        dimension_limit_exceeded,
        resident_budget_exceeded,
        upload_budget_exceeded
    };

    enum class TextureProjectPolicyMode : std::uint8_t
    {
        strict,
        allow_experimental
    };

    enum class TextureExecutionRoute : std::uint8_t
    {
        cpu_reference_canvas2d
    };

    [[nodiscard]] constexpr std::string_view texture_admission_status_name(
        const TextureAdmissionStatus status) noexcept
    {
        switch (status)
        {
        case TextureAdmissionStatus::admitted: return "admitted";
        case TextureAdmissionStatus::experimental: return "experimental";
        case TextureAdmissionStatus::rejected: return "rejected";
        }
        return "rejected";
    }

    [[nodiscard]] constexpr std::string_view texture_admission_reason_name(
        const TextureAdmissionReason reason) noexcept
    {
        switch (reason)
        {
        case TextureAdmissionReason::none: return "none";
        case TextureAdmissionReason::invalid_policy: return "invalid_policy";
        case TextureAdmissionReason::invalid_platform_budgets:
            return "invalid_platform_budgets";
        case TextureAdmissionReason::invalid_artifact: return "invalid_artifact";
        case TextureAdmissionReason::unsupported_format: return "unsupported_format";
        case TextureAdmissionReason::unsupported_color_space:
            return "unsupported_color_space";
        case TextureAdmissionReason::unsupported_mip_count:
            return "unsupported_mip_count";
        case TextureAdmissionReason::missing_capability_evidence:
            return "missing_capability_evidence";
        case TextureAdmissionReason::unsupported_sampled_image_representation:
            return "unsupported_sampled_image_representation";
        case TextureAdmissionReason::renderer_requirement_rejected:
            return "renderer_requirement_rejected";
        case TextureAdmissionReason::experimental_not_allowed:
            return "experimental_not_allowed";
        case TextureAdmissionReason::invalid_effective_limits:
            return "invalid_effective_limits";
        case TextureAdmissionReason::dimension_limit_exceeded:
            return "dimension_limit_exceeded";
        case TextureAdmissionReason::resident_budget_exceeded:
            return "resident_budget_exceeded";
        case TextureAdmissionReason::upload_budget_exceeded:
            return "upload_budget_exceeded";
        }
        return "unknown";
    }

    struct TextureAdmissionLimits final
    {
        std::uint32_t maximum_dimension{32'768};
        std::uint8_t maximum_mip_count{1};
        std::uint32_t maximum_sampled_textures{1'024};
        std::uint64_t maximum_single_texture_bytes{
            256ull * 1024ull * 1024ull};
        std::uint64_t maximum_resident_texture_bytes{
            512ull * 1024ull * 1024ull};
        std::uint64_t maximum_upload_bytes{32ull * 1024ull * 1024ull};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_dimension != 0u
                && maximum_mip_count != 0u
                && maximum_sampled_textures != 0u
                && maximum_single_texture_bytes != 0u
                && maximum_resident_texture_bytes != 0u
                && maximum_upload_bytes != 0u
                && maximum_single_texture_bytes
                    <= maximum_resident_texture_bytes;
        }
    };

    struct TextureAdmissionPolicy final
    {
        TextureProjectPolicyMode mode{
            TextureProjectPolicyMode::allow_experimental};
        capability::Requirement renderer_requirement{
            capability::portable_rendering_requirement()};
        TextureAdmissionLimits limits{};
        bool allow_software_fallback{true};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return limits.valid()
                && renderer_requirement.subsystem
                    == capability::Subsystem::rendering;
        }
    };

    struct EffectiveTextureAdmissionLimits final
    {
        std::uint32_t maximum_dimension{};
        std::uint8_t maximum_mip_count{};
        std::uint32_t maximum_sampled_textures{};
        std::uint64_t maximum_single_texture_bytes{};
        std::uint64_t maximum_resident_texture_bytes{};
        std::uint64_t maximum_upload_bytes{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_dimension != 0u
                && maximum_mip_count != 0u
                && maximum_sampled_textures != 0u
                && maximum_single_texture_bytes != 0u
                && maximum_resident_texture_bytes != 0u
                && maximum_upload_bytes != 0u
                && maximum_single_texture_bytes
                    <= maximum_resident_texture_bytes;
        }
    };

    struct TextureAdmissionDecision final
    {
        TextureAdmissionStatus status{TextureAdmissionStatus::rejected};
        TextureAdmissionReason reason{TextureAdmissionReason::invalid_policy};
        TextureExecutionRoute route{
            TextureExecutionRoute::cpu_reference_canvas2d};
        EffectiveTextureAdmissionLimits effective_limits{};
        capability::RequirementAdmission renderer_admission{};
        std::uint64_t artifact_bytes{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return status != TextureAdmissionStatus::rejected
                && reason == TextureAdmissionReason::none
                && effective_limits.valid();
        }
    };

    namespace detail
    {
        template <typename Value>
        [[nodiscard]] constexpr Value minimum_positive(
            const Value first,
            const Value second) noexcept
        {
            if (first == 0 || second == 0)
                return Value{};
            return (std::min)(first, second);
        }

        [[nodiscard]] constexpr bool valid_platform_budgets(
            const Budgets& budgets) noexcept
        {
            return budgets.cpu_ms >= 0.0f
                && budgets.gpu_ms >= 0.0f
                && budgets.vram_budget_bytes != 0u
                && budgets.upload_budget_bytes != 0u
                && budgets.min_w != 0u
                && budgets.min_h != 0u
                && budgets.min_w <= budgets.max_w
                && budgets.min_h <= budgets.max_h;
        }

        [[nodiscard]] constexpr std::uint64_t available_texture_memory(
            const capability::MemoryProfile& memory) noexcept
        {
            if (memory.local_budget_bytes != 0u)
                return memory.local_budget_bytes;
            return memory.shared_budget_bytes;
        }

        [[nodiscard]] constexpr EffectiveTextureAdmissionLimits effective_limits(
            const TextureAdmissionPolicy& policy,
            const capability::SubsystemProfile& renderer,
            const Budgets& platform_budgets) noexcept
        {
            constexpr std::uint8_t routeMaximumMipCount = 1u;

            EffectiveTextureAdmissionLimits limits{};
            limits.maximum_dimension = minimum_positive(
                policy.limits.maximum_dimension,
                renderer.capability.limits.max_texture_dimension_2d);
            limits.maximum_mip_count = minimum_positive(
                policy.limits.maximum_mip_count,
                routeMaximumMipCount);
            limits.maximum_sampled_textures = minimum_positive(
                policy.limits.maximum_sampled_textures,
                renderer.capability.limits.max_sampled_images);

            limits.maximum_resident_texture_bytes = minimum_positive(
                policy.limits.maximum_resident_texture_bytes,
                platform_budgets.vram_budget_bytes);
            limits.maximum_resident_texture_bytes = minimum_positive(
                limits.maximum_resident_texture_bytes,
                renderer.capability.recommended_budgets.vram_budget_bytes);
            limits.maximum_resident_texture_bytes = minimum_positive(
                limits.maximum_resident_texture_bytes,
                available_texture_memory(renderer.capability.memory));

            limits.maximum_single_texture_bytes = minimum_positive(
                policy.limits.maximum_single_texture_bytes,
                limits.maximum_resident_texture_bytes);

            limits.maximum_upload_bytes = minimum_positive(
                policy.limits.maximum_upload_bytes,
                platform_budgets.upload_budget_bytes);
            limits.maximum_upload_bytes = minimum_positive(
                limits.maximum_upload_bytes,
                renderer.capability.recommended_budgets.upload_budget_bytes);
            limits.maximum_upload_bytes = minimum_positive(
                limits.maximum_upload_bytes,
                renderer.capability.memory.upload_budget_bytes);
            return limits;
        }

        [[nodiscard]] constexpr TextureAdmissionReason capability_reason(
            const capability::MatchFailure failure) noexcept
        {
            switch (failure)
            {
            case capability::MatchFailure::missing_evidence:
            case capability::MatchFailure::invalid_profile:
                return TextureAdmissionReason::missing_capability_evidence;
            case capability::MatchFailure::unsupported_representation:
                return TextureAdmissionReason::unsupported_sampled_image_representation;
            case capability::MatchFailure::memory_budget:
                return TextureAdmissionReason::resident_budget_exceeded;
            case capability::MatchFailure::none:
            case capability::MatchFailure::wrong_subsystem:
            case capability::MatchFailure::unavailable:
            case capability::MatchFailure::wrong_backend:
            case capability::MatchFailure::insufficient_tier:
            case capability::MatchFailure::missing_feature:
            case capability::MatchFailure::insufficient_quality:
            case capability::MatchFailure::insufficient_determinism:
            case capability::MatchFailure::insufficient_stability:
            case capability::MatchFailure::latency_budget:
            case capability::MatchFailure::power_budget:
            case capability::MatchFailure::no_candidate:
                return TextureAdmissionReason::renderer_requirement_rejected;
            }
            return TextureAdmissionReason::renderer_requirement_rejected;
        }

        [[nodiscard]] constexpr bool artifact_shape_available(
            const asset::texture::CompiledTextureArtifact& artifact) noexcept
        {
            return artifact.status
                    == asset::texture::ArtifactCompilationStatus::ready
                && static_cast<bool>(artifact.identity)
                && !artifact.identity.compilation_required
                && !artifact.payload_content.empty()
                && artifact.identity.source_revision.sequence != 0u
                && artifact.identity.profile.compiler_schema_version != 0u
                && artifact.identity.estimated_artifact_bytes != 0u
                && artifact.mips.size() == artifact.identity.mip_count;
        }

        [[nodiscard]] inline asset::texture::CompiledTextureArtifact
            contract_artifact(
                const std::uint32_t width = 2u,
                const std::uint32_t height = 2u,
                const std::uint8_t mip_count = 1u)
        {
            using namespace asset::texture;

            DocumentRevision source{};
            source.content.words = {1u, 2u, 3u, 4u};
            source.sequence = 1u;

            TextureCompileProfile profile{};
            profile.format = ArtifactFormat::rgba8_unorm;
            profile.color_space = ColorSpace::linear;
            profile.mipmaps = MipmapPolicy::preserve_authored;

            CompiledTextureArtifact artifact{};
            artifact.status = ArtifactCompilationStatus::ready;
            artifact.identity = build_compiled_texture_artifact_identity(
                source, profile, width, height, mip_count);
            artifact.identity.compilation_required = false;

            std::uint32_t mipWidth = width;
            std::uint32_t mipHeight = height;
            for (std::uint8_t mipIndex = 0u;
                 mipIndex < mip_count;
                 ++mipIndex)
            {
                CompiledTextureMip mip{};
                mip.width = mipWidth;
                mip.height = mipHeight;
                mip.row_pitch_bytes = mipWidth * 4u;
                mip.texels.resize(
                    static_cast<std::size_t>(mip.row_pitch_bytes) * mipHeight);
                for (std::size_t index = 0; index < mip.texels.size(); ++index)
                {
                    mip.texels[index] = std::byte{
                        static_cast<unsigned char>((index + mipIndex) & 0xffu)};
                }
                mip.content = compiled_texture_mip_content(mip);
                artifact.mips.push_back(std::move(mip));
                mipWidth = (std::max)(1u, mipWidth / 2u);
                mipHeight = (std::max)(1u, mipHeight / 2u);
            }
            artifact.payload_content = compiled_texture_payload_content(
                artifact.identity, artifact.mips);
            return artifact;
        }

        [[nodiscard]] constexpr capability::SubsystemProfile
            contract_renderer() noexcept
        {
            constexpr capability::EvidenceMask evidence =
                capability::evidence_mask(
                    capability::Evidence::build_contract,
                    capability::Evidence::pure_contract_test);
            const capability::Profile profile = capability::opengl_profile(
                capability::Tier::portable_graphics,
                capability::Status::present,
                evidence);
            return capability::SubsystemProfile{
                .implementation_id = 0x45504f4354455801ull,
                .subsystem = capability::Subsystem::rendering,
                .capability = profile,
                .implementation_features = 0u,
                .representations = capability::representation_mask(
                    capability::DataRepresentation::sampled_image),
                .quality = {0u, 80u},
                .determinism = capability::Determinism::repeatable,
                .cost = {},
                .stability = capability::StabilityStatus::validated,
                .status = capability::Status::present,
                .evidence = evidence,
                .software_fallback = false};
        }
    }

    [[nodiscard]] inline TextureAdmissionDecision assess_texture_admission(
        const asset::texture::CompiledTextureArtifact& artifact,
        const capability::SubsystemProfile& renderer,
        const TextureAdmissionPolicy& policy = {},
        const Budgets& platform_budgets = {}) noexcept
    {
        TextureAdmissionDecision decision{};

        if (!policy.valid())
        {
            decision.reason = TextureAdmissionReason::invalid_policy;
            return decision;
        }
        if (!detail::valid_platform_budgets(platform_budgets))
        {
            decision.reason = TextureAdmissionReason::invalid_platform_budgets;
            return decision;
        }
        if (!detail::artifact_shape_available(artifact))
        {
            decision.reason = TextureAdmissionReason::invalid_artifact;
            return decision;
        }
        if (artifact.identity.profile.format
            != asset::texture::ArtifactFormat::rgba8_unorm)
        {
            decision.reason = TextureAdmissionReason::unsupported_format;
            return decision;
        }
        if (artifact.identity.profile.color_space
            != asset::texture::ColorSpace::linear)
        {
            decision.reason = TextureAdmissionReason::unsupported_color_space;
            return decision;
        }
        if (artifact.identity.mip_count != 1u)
        {
            decision.reason = TextureAdmissionReason::unsupported_mip_count;
            return decision;
        }
        if (!asset::texture::validate_compiled_artifact(artifact))
        {
            decision.reason = TextureAdmissionReason::invalid_artifact;
            return decision;
        }

        if (!capability::has_validation_evidence(renderer.evidence)
            || !capability::has_validation_evidence(
                renderer.capability.evidence))
        {
            decision.reason = TextureAdmissionReason::missing_capability_evidence;
            return decision;
        }

        capability::Requirement textureRequirement =
            policy.renderer_requirement;
        textureRequirement.accepted_representations =
            capability::representation_mask(
                capability::DataRepresentation::sampled_image);
        decision.renderer_admission = capability::assess_requirement(
            renderer,
            textureRequirement,
            true,
            policy.allow_software_fallback);
        if (decision.renderer_admission.status
            == capability::AdmissionStatus::rejected)
        {
            decision.reason = detail::capability_reason(
                decision.renderer_admission.failure);
            return decision;
        }
        if (decision.renderer_admission.status
                == capability::AdmissionStatus::experimental
            && policy.mode == TextureProjectPolicyMode::strict)
        {
            decision.reason = TextureAdmissionReason::experimental_not_allowed;
            return decision;
        }

        decision.effective_limits = detail::effective_limits(
            policy, renderer, platform_budgets);
        if (!decision.effective_limits.valid())
        {
            decision.reason = TextureAdmissionReason::invalid_effective_limits;
            return decision;
        }

        const auto& identity = artifact.identity;
        decision.artifact_bytes = identity.estimated_artifact_bytes;
        if (identity.width > decision.effective_limits.maximum_dimension
            || identity.height > decision.effective_limits.maximum_dimension)
        {
            decision.reason = TextureAdmissionReason::dimension_limit_exceeded;
            return decision;
        }
        if (identity.mip_count
            > decision.effective_limits.maximum_mip_count)
        {
            decision.reason = TextureAdmissionReason::unsupported_mip_count;
            return decision;
        }
        if (decision.artifact_bytes
                > decision.effective_limits.maximum_single_texture_bytes
            || decision.artifact_bytes
                > decision.effective_limits.maximum_resident_texture_bytes)
        {
            decision.reason = TextureAdmissionReason::resident_budget_exceeded;
            return decision;
        }
        if (decision.artifact_bytes
            > decision.effective_limits.maximum_upload_bytes)
        {
            decision.reason = TextureAdmissionReason::upload_budget_exceeded;
            return decision;
        }

        decision.status = decision.renderer_admission.status
                == capability::AdmissionStatus::experimental
            ? TextureAdmissionStatus::experimental
            : TextureAdmissionStatus::admitted;
        decision.reason = TextureAdmissionReason::none;
        return decision;
    }

    enum class TextureAdmissionContractFailure : std::uint8_t
    {
        none,
        admitted_baseline,
        unsupported_format,
        unsupported_color_space,
        unsupported_mip_count,
        dimension_limit,
        resident_budget,
        upload_budget,
        invalid_artifact,
        missing_evidence,
        unsupported_representation,
        experimental_admission,
        strict_rejection
    };

    [[nodiscard]] constexpr std::string_view
        texture_admission_contract_failure_name(
            const TextureAdmissionContractFailure failure) noexcept
    {
        switch (failure)
        {
        case TextureAdmissionContractFailure::none: return "pass";
        case TextureAdmissionContractFailure::admitted_baseline:
            return "admitted_baseline";
        case TextureAdmissionContractFailure::unsupported_format:
            return "unsupported_format";
        case TextureAdmissionContractFailure::unsupported_color_space:
            return "unsupported_color_space";
        case TextureAdmissionContractFailure::unsupported_mip_count:
            return "unsupported_mip_count";
        case TextureAdmissionContractFailure::dimension_limit:
            return "dimension_limit";
        case TextureAdmissionContractFailure::resident_budget:
            return "resident_budget";
        case TextureAdmissionContractFailure::upload_budget:
            return "upload_budget";
        case TextureAdmissionContractFailure::invalid_artifact:
            return "invalid_artifact";
        case TextureAdmissionContractFailure::missing_evidence:
            return "missing_evidence";
        case TextureAdmissionContractFailure::unsupported_representation:
            return "unsupported_representation";
        case TextureAdmissionContractFailure::experimental_admission:
            return "experimental_admission";
        case TextureAdmissionContractFailure::strict_rejection:
            return "strict_rejection";
        }
        return "unknown";
    }

    [[nodiscard]] inline TextureAdmissionContractFailure
        project_texture_admission_contract_failure()
    {
        using Failure = TextureAdmissionContractFailure;

        const auto renderer = detail::contract_renderer();
        const auto artifact = detail::contract_artifact();
        const TextureAdmissionDecision admitted = assess_texture_admission(
            artifact, renderer);
        if (admitted.status != TextureAdmissionStatus::admitted
            || admitted.reason != TextureAdmissionReason::none
            || admitted.artifact_bytes != 16u
            || !admitted.effective_limits.valid())
        {
            return Failure::admitted_baseline;
        }

        auto unsupportedFormat = artifact;
        unsupportedFormat.identity.profile.format =
            asset::texture::ArtifactFormat::bc7_rgba;
        if (assess_texture_admission(unsupportedFormat, renderer).reason
            != TextureAdmissionReason::unsupported_format)
        {
            return Failure::unsupported_format;
        }

        auto unsupportedColor = artifact;
        unsupportedColor.identity.profile.color_space =
            asset::texture::ColorSpace::srgb;
        if (assess_texture_admission(unsupportedColor, renderer).reason
            != TextureAdmissionReason::unsupported_color_space)
        {
            return Failure::unsupported_color_space;
        }

        const auto mipmapped = detail::contract_artifact(4u, 4u, 2u);
        if (assess_texture_admission(mipmapped, renderer).reason
            != TextureAdmissionReason::unsupported_mip_count)
        {
            return Failure::unsupported_mip_count;
        }

        TextureAdmissionPolicy dimensionPolicy{};
        dimensionPolicy.limits.maximum_dimension = 4u;
        const auto oversized = detail::contract_artifact(8u, 8u);
        if (assess_texture_admission(
                oversized, renderer, dimensionPolicy).reason
            != TextureAdmissionReason::dimension_limit_exceeded)
        {
            return Failure::dimension_limit;
        }

        TextureAdmissionPolicy residentPolicy{};
        residentPolicy.limits.maximum_single_texture_bytes = 8u;
        if (assess_texture_admission(
                artifact, renderer, residentPolicy).reason
            != TextureAdmissionReason::resident_budget_exceeded)
        {
            return Failure::resident_budget;
        }

        TextureAdmissionPolicy uploadPolicy{};
        uploadPolicy.limits.maximum_upload_bytes = 8u;
        if (assess_texture_admission(
                artifact, renderer, uploadPolicy).reason
            != TextureAdmissionReason::upload_budget_exceeded)
        {
            return Failure::upload_budget;
        }

        auto corrupted = artifact;
        corrupted.payload_content.words[0] ^= 1u;
        if (assess_texture_admission(corrupted, renderer).reason
            != TextureAdmissionReason::invalid_artifact)
        {
            return Failure::invalid_artifact;
        }

        auto unevidenced = renderer;
        unevidenced.evidence = capability::evidence_bit(
            capability::Evidence::declared_contract);
        unevidenced.capability.evidence = unevidenced.evidence;
        if (assess_texture_admission(artifact, unevidenced).reason
            != TextureAdmissionReason::missing_capability_evidence)
        {
            return Failure::missing_evidence;
        }

        auto wrongRepresentation = renderer;
        wrongRepresentation.representations = capability::representation_mask(
            capability::DataRepresentation::triangles);
        if (assess_texture_admission(artifact, wrongRepresentation).reason
            != TextureAdmissionReason::unsupported_sampled_image_representation)
        {
            return Failure::unsupported_representation;
        }

        auto experimentalRenderer = renderer;
        experimentalRenderer.status = capability::Status::partial;
        experimentalRenderer.capability.status = capability::Status::partial;
        experimentalRenderer.stability =
            capability::StabilityStatus::experimental;
        experimentalRenderer.capability.stability =
            capability::StabilityStatus::experimental;
        const TextureAdmissionDecision experimental = assess_texture_admission(
            artifact, experimentalRenderer);
        if (experimental.status != TextureAdmissionStatus::experimental
            || experimental.reason != TextureAdmissionReason::none)
        {
            return Failure::experimental_admission;
        }

        TextureAdmissionPolicy strictPolicy{};
        strictPolicy.mode = TextureProjectPolicyMode::strict;
        const TextureAdmissionDecision strict = assess_texture_admission(
            artifact, experimentalRenderer, strictPolicy);
        if (strict.status != TextureAdmissionStatus::rejected
            || strict.reason
                != TextureAdmissionReason::experimental_not_allowed)
        {
            return Failure::strict_rejection;
        }

        return Failure::none;
    }
}
