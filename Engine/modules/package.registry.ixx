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

#include <cstddef>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "../include/_epoch.stl_types.hpp"

export module package.registry;

import forest.factory;

export namespace epochengine::package_registry
{
    enum class PackageKind : std::uint8_t
    {
        RuntimeMini,
        CoreOptIn,
        NetworkRuntime,
        HeadlessServer,
        ModelAsset,
        ResearchPrototype,
        DownloadableSource
    };

    enum class ActivationMode : std::uint8_t
    {
        ProjectOptIn,
        MainSceneUse,
        HeadlessServerOptIn,
        ClientListenServerOptIn,
        ModelDownloadOptIn,
        ManualResearchImport,
        DownloadedOptIn
    };

    struct PackageDescriptor
    {
        std::string_view id{};
        std::string_view displayName{};
        std::string_view summary{};
        PackageKind kind{PackageKind::RuntimeMini};
        ActivationMode activation{ActivationMode::ProjectOptIn};
        bool shipsInCore{};
        bool includeInGeneratedProject{};
        bool requiresHumanBuildGate{true};
        bool serverOrListenerAllowed{};
        bool headlessCapable{};
        bool requiresExplicitNetworkApproval{};
        std::string_view externalSourceRepo{};
    };

    struct RegistryValidation
    {
        bool ok{};
        std::size_t packageCount{};
        std::size_t modelAssetCount{};
        std::size_t networkSensitiveCount{};
        std::size_t duplicateIdCount{};
    };
    enum class PayloadRole : std::uint8_t
    {
        CoreOnly,
        OptionalExtension,
        RequiredExtension
    };

    enum class PayloadRejectReason : std::uint8_t
    {
        None,
        UnknownPackage,
        MissingEvidence,
        PackageMismatch,
        RepositoryMismatch,
        MissingImmutableRevision,
        InvalidImmutableRevision,
        UnverifiedImmutableRevision,
        MissingContentHash,
        InvalidContentHash,
        UnverifiedContentHash,
        MissingLicenseEvidence,
        UnverifiedLicenseEvidence,
        MissingBuildTestEvidence,
        UnverifiedBuildTestEvidence,
        HumanApprovalRequired,
        ApprovalIdentityMismatch,
        NativeCodeNotAllowed,
        NativeCodeApprovalRequired
    };

    struct PackagePayloadRequirement final
    {
        std::string_view packageId{};
        PayloadRole role{PayloadRole::CoreOnly};
        std::string_view sourceRepo{};
        bool coreFallbackAvailable{};
        bool requiresBuildTestEvidence{};
        bool nativeCodeAllowed{};
    };

    struct PackagePayloadEvidence final
    {
        std::string_view packageId{};
        std::string_view sourceRepo{};
        std::string_view immutableRevision{};
        std::string_view contentHash{};
        std::string_view licenseEvidence{};
        std::string_view buildTestEvidence{};
        std::string_view approvedContentHash{};
        bool immutableRevisionVerified{};
        bool contentHashVerified{};
        bool licenseEvidenceVerified{};
        bool buildTestEvidenceVerified{};
        bool humanApproved{};
        bool containsNativeCode{};
        bool nativeCodeApproved{};
    };

    struct PayloadActivationDecision final
    {
        bool payloadAllowed{};
        bool coreFallbackAvailable{};
        PayloadRejectReason reason{PayloadRejectReason::None};
    };

    inline constexpr std::string_view kEpochEngineExtensionsRepo = "https://github.com/Autodidac/EpochEngineExtensions";
    inline constexpr std::string_view kForestFactoryReferenceRepo = epochengine::forest::kForestFactoryReferenceRepo;
    inline constexpr std::string_view kForestFactoryPackageSourceRepo = kEpochEngineExtensionsRepo;
    inline constexpr std::string_view kEngineArcadePackageId = "engine_arcade";
    inline constexpr std::string_view kEngineArcadeOptionalAssetRepo = kEpochEngineExtensionsRepo;
    inline constexpr std::string_view kEngineArcadeSceneId = "engine_arcade_scene";
    inline constexpr std::string_view kEngineArcadeDefaultSceneId = "snake";
    inline constexpr std::string_view kEngineArcadeSceneIds =
        "snake,tetris,pacman,frogger,sokoban,match3,sliding,minesweeper,2048,sandsim,cellular";
    inline constexpr std::string_view kEngineArcadeRenderAssetRole = "render_to_texture_arcade_cabinet";
    inline constexpr std::string_view kEngineArcadeRendererRequirements =
        "render_targets,sampled_render_targets,render_to_texture,materials,binding_sets,command_lists";
    inline constexpr std::string_view kEngineArcadeRenderTextureName = "engine_arcade.screen";
    inline constexpr std::uint32_t kEngineArcadeRenderTextureWidth = 512;
    inline constexpr std::uint32_t kEngineArcadeRenderTextureHeight = 512;
    inline constexpr std::string_view kEngineForestFactoryPackageId = epochengine::forest::kForestFactoryPackageId;
    inline constexpr std::string_view kEngineNetworkRuntimePackageId = "engine_network_runtime";
    inline constexpr std::string_view kEngineAuthoritativeServerPackageId = "engine_authoritative_dedicated_server";
    inline constexpr std::string_view kEngineListenServerPackageId = "engine_client_listen_server";
    inline constexpr std::string_view kNemotronNanoPackageId = "os_model_nemotron_3_nano_4b_bf16";
    inline constexpr std::string_view kQwenCoderPackageId = "os_model_qwen_27b";
    inline constexpr std::string_view kBonsaiImageTernaryPackageId = "os_model_bonsai_image_ternary_4b_mlx_2bit";
    inline constexpr std::string_view kBonsaiImageBinaryPackageId = "os_model_bonsai_image_binary_4b_mlx_1bit";
    inline constexpr std::string_view kFluxKleinImagePackageId = "os_model_flux_2_klein_4b";

    inline constexpr std::array<PackageDescriptor, 15> kKnownPackages{{
        {
            .id = kEngineArcadePackageId,
            .displayName = "Engine Arcade",
            .summary = "Kernel mini-runtime scenes exposed as opt-in project package/script assets for render-to-texture arcade cabinets and in-game terminals.",
            .kind = PackageKind::RuntimeMini,
            .activation = ActivationMode::ProjectOptIn,
            .shipsInCore = true,
        },
        {
            .id = kEngineForestFactoryPackageId,
            .displayName = "Forest Factory",
            .summary = "Core temporal graph/L-system vegetation lab. Editor preview is built in; project assets are emitted only after explicit activation.",
            .kind = PackageKind::CoreOptIn,
            .activation = ActivationMode::MainSceneUse,
            .shipsInCore = true,
            .externalSourceRepo = kForestFactoryPackageSourceRepo,
        },
        {
            .id = kEngineNetworkRuntimePackageId,
            .displayName = "Engine Network Runtime",
            .summary = "Shared client/session replication contracts; inert unless a network package is explicitly enabled.",
            .kind = PackageKind::NetworkRuntime,
            .activation = ActivationMode::ProjectOptIn,
            .shipsInCore = true,
        },
        {
            .id = kEngineAuthoritativeServerPackageId,
            .displayName = "Optional Authoritative Dedicated Server",
            .summary = "Optional headless authoritative server package for multiplayer projects that explicitly choose that model.",
            .kind = PackageKind::HeadlessServer,
            .activation = ActivationMode::HeadlessServerOptIn,
            .serverOrListenerAllowed = true,
            .headlessCapable = true,
            .requiresExplicitNetworkApproval = true,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = kEngineListenServerPackageId,
            .displayName = "Client Listen Server",
            .summary = "Client-hosted nondedicated or competitive-friendly session option for multiplayer projects that explicitly opt in.",
            .kind = PackageKind::NetworkRuntime,
            .activation = ActivationMode::ClientListenServerOptIn,
            .shipsInCore = true,
            .serverOrListenerAllowed = true,
            .requiresExplicitNetworkApproval = true,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = kNemotronNanoPackageId,
            .displayName = "NVIDIA Nemotron 3 Nano 4B BF16",
            .summary = "Fast OS coding/review model lane. Weights download on demand into cache/models and may be included in projects only by explicit package opt-in.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16",
        },
        {
            .id = kQwenCoderPackageId,
            .displayName = "Qwen 3.6 27B",
            .summary = "Heavy OS coding/planning model lane. Weights download on demand into cache/models and may be included in projects only by explicit package opt-in.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/Qwen/Qwen3.6-27B",
        },
        {
            .id = kBonsaiImageTernaryPackageId,
            .displayName = "Bonsai Image Ternary 4B",
            .summary = "Preferred local image generation/editing model lane for the best quality/footprint balance. Weights download on demand into cache/models.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/prism-ml/bonsai-image-ternary-4B-mlx-2bit",
        },
        {
            .id = kBonsaiImageBinaryPackageId,
            .displayName = "Bonsai Image Binary 4B",
            .summary = "Optional low-memory local image generation/editing model lane for the smallest local footprint. Weights download on demand into cache/models.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/prism-ml/bonsai-image-binary-4B-mlx-1bit",
        },
        {
            .id = kFluxKleinImagePackageId,
            .displayName = "FLUX.2 Klein 4B",
            .summary = "Optional higher-memory fallback image generation/editing model lane. Bonsai Ternary 4B remains the recommended local default.",
            .kind = PackageKind::ModelAsset,
            .activation = ActivationMode::ModelDownloadOptIn,
            .requiresHumanBuildGate = true,
            .externalSourceRepo = "https://huggingface.co/black-forest-labs/FLUX.2-klein-4B",
        },
        {
            .id = "research_voxel_planetoid",
            .displayName = "Research Voxel Planetoid",
            .summary = "Renderer-neutral voxel/chunk prototype snapshot staged for reviewed package import.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_voxel_authoring",
            .displayName = "Research Voxel Authoring",
            .summary = "External voxel authoring/toolkit prototype staged for API-boundary review.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_forest_temporal_graph",
            .displayName = "Research Forest Factory Temporal Graph",
            .summary = "Plant Lab source/prototype lineage used as Forest Factory reference material before production promotion.",
            .kind = PackageKind::ResearchPrototype,
            .activation = ActivationMode::ManualResearchImport,
            .externalSourceRepo = kForestFactoryReferenceRepo,
        },
        {
            .id = "research_planetary_terrain",
            .displayName = "Research Planetary Terrain",
            .summary = "NMS-like planetary and multi-terrain stacks remain package-gated.",
            .kind = PackageKind::DownloadableSource,
            .activation = ActivationMode::DownloadedOptIn,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
        {
            .id = "research_fft_ocean",
            .displayName = "Research FFT Ocean",
            .summary = "Ocean simulation package candidate; not part of the minimal engine clone.",
            .kind = PackageKind::DownloadableSource,
            .activation = ActivationMode::DownloadedOptIn,
            .externalSourceRepo = kEpochEngineExtensionsRepo,
        },
    }};

    [[nodiscard]] constexpr std::span<const PackageDescriptor> known_packages() noexcept
    {
        return {kKnownPackages.data(), kKnownPackages.size()};
    }

    [[nodiscard]] constexpr std::string_view package_kind_name(PackageKind kind) noexcept
    {
        switch (kind)
        {
        case PackageKind::RuntimeMini: return "Runtime mini";
        case PackageKind::CoreOptIn: return "Core opt-in";
        case PackageKind::NetworkRuntime: return "Network runtime";
        case PackageKind::HeadlessServer: return "Headless server";
        case PackageKind::ModelAsset: return "Model asset";
        case PackageKind::ResearchPrototype: return "Research prototype";
        case PackageKind::DownloadableSource: return "Downloadable source";
        }

        return "Unknown";
    }

    [[nodiscard]] constexpr std::string_view activation_mode_name(ActivationMode activation) noexcept
    {
        switch (activation)
        {
        case ActivationMode::ProjectOptIn: return "Project opt-in";
        case ActivationMode::MainSceneUse: return "Main-scene use";
        case ActivationMode::HeadlessServerOptIn: return "Headless server opt-in";
        case ActivationMode::ClientListenServerOptIn: return "Client listen-server opt-in";
        case ActivationMode::ModelDownloadOptIn: return "Model download opt-in";
        case ActivationMode::ManualResearchImport: return "Manual research import";
        case ActivationMode::DownloadedOptIn: return "Downloaded opt-in";
        }

        return "Unknown";
    }

    [[nodiscard]] constexpr const PackageDescriptor* find(std::string_view id) noexcept
    {
        for (const auto& package : kKnownPackages)
        {
            if (package.id == id)
            {
                return &package;
            }
        }

        return nullptr;
    }

    [[nodiscard]] constexpr bool is_network_sensitive(const PackageDescriptor& package) noexcept
    {
        return package.serverOrListenerAllowed ||
               package.requiresExplicitNetworkApproval ||
               package.activation == ActivationMode::HeadlessServerOptIn ||
               package.activation == ActivationMode::ClientListenServerOptIn;
    }

    [[nodiscard]] constexpr bool is_model_asset(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr && package->kind == PackageKind::ModelAsset;
    }

    [[nodiscard]] constexpr bool is_research_or_downloadable(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr &&
               (package->kind == PackageKind::ResearchPrototype ||
                package->kind == PackageKind::DownloadableSource);
    }

    [[nodiscard]] constexpr bool ships_in_core_without_default_project_payload(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr && package->shipsInCore && !package->includeInGeneratedProject;
    }

    [[nodiscard]] constexpr bool is_core_opt_in(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr && package->kind == PackageKind::CoreOptIn;
    }

    [[nodiscard]] constexpr bool must_use_human_build_gate(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package == nullptr || package->requiresHumanBuildGate;
    }

    [[nodiscard]] constexpr bool requires_explicit_network_approval(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr && package->requiresExplicitNetworkApproval;
    }

    [[nodiscard]] constexpr bool can_create_server_or_listener_after_approval(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr &&
               package->serverOrListenerAllowed &&
               package->requiresExplicitNetworkApproval;
    }

    [[nodiscard]] constexpr std::string_view external_source_repo(std::string_view id) noexcept
    {
        const auto* package = find(id);
        return package != nullptr ? package->externalSourceRepo : std::string_view{};
    }
    [[nodiscard]] constexpr PackagePayloadRequirement payload_requirement(
        std::string_view id) noexcept
    {
        const PackageDescriptor* package = find(id);
        if (package == nullptr)
        {
            return {};
        }

        if (id == kEngineArcadePackageId)
        {
            return {
                id,
                PayloadRole::OptionalExtension,
                kEngineArcadeOptionalAssetRepo,
                true,
                true,
                false
            };
        }

        if (id == kEngineForestFactoryPackageId)
        {
            return {
                id,
                PayloadRole::OptionalExtension,
                kForestFactoryPackageSourceRepo,
                true,
                true,
                true
            };
        }

        if (package->kind == PackageKind::ResearchPrototype ||
            package->kind == PackageKind::DownloadableSource)
        {
            return {
                id,
                PayloadRole::RequiredExtension,
                package->externalSourceRepo,
                false,
                true,
                true
            };
        }

        if (!package->externalSourceRepo.empty())
        {
            return {
                id,
                PayloadRole::RequiredExtension,
                package->externalSourceRepo,
                false,
                package->kind != PackageKind::ModelAsset,
                false
            };
        }

        return {id, PayloadRole::CoreOnly, {}, package->shipsInCore, false, false};
    }

    [[nodiscard]] constexpr bool hexadecimal_digest(
        std::string_view value,
        std::size_t digits) noexcept
    {
        if (value.size() != digits)
        {
            return false;
        }
        for (const char character : value)
        {
            const bool digit = character >= '0' && character <= '9';
            const bool lower = character >= 'a' && character <= 'f';
            const bool upper = character >= 'A' && character <= 'F';
            if (!digit && !lower && !upper)
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] constexpr bool immutable_revision_identity(
        std::string_view value) noexcept
    {
        return hexadecimal_digest(value, 40) || hexadecimal_digest(value, 64);
    }

    [[nodiscard]] constexpr bool content_hash_identity(
        std::string_view value) noexcept
    {
        constexpr std::string_view prefix = "sha256:";
        return value.starts_with(prefix) &&
               hexadecimal_digest(value.substr(prefix.size()), 64);
    }

    [[nodiscard]] constexpr PayloadActivationDecision evaluate_payload_activation(
        const PackagePayloadRequirement& requirement,
        const PackagePayloadEvidence& evidence) noexcept
    {
        if (requirement.packageId.empty())
        {
            return {false, false, PayloadRejectReason::UnknownPackage};
        }
        if (requirement.role == PayloadRole::CoreOnly)
        {
            return {true, requirement.coreFallbackAvailable, PayloadRejectReason::None};
        }
        if (evidence.packageId.empty())
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::MissingEvidence};
        }
        if (evidence.packageId != requirement.packageId)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::PackageMismatch};
        }
        if (evidence.sourceRepo != requirement.sourceRepo)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::RepositoryMismatch};
        }
        if (evidence.immutableRevision.empty())
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::MissingImmutableRevision};
        }
        if (!immutable_revision_identity(evidence.immutableRevision))
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::InvalidImmutableRevision};
        }
        if (!evidence.immutableRevisionVerified)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::UnverifiedImmutableRevision};
        }
        if (evidence.contentHash.empty())
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::MissingContentHash};
        }
        if (!content_hash_identity(evidence.contentHash))
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::InvalidContentHash};
        }
        if (!evidence.contentHashVerified)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::UnverifiedContentHash};
        }
        if (evidence.licenseEvidence.empty())
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::MissingLicenseEvidence};
        }
        if (!evidence.licenseEvidenceVerified)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::UnverifiedLicenseEvidence};
        }
        if (requirement.requiresBuildTestEvidence && evidence.buildTestEvidence.empty())
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::MissingBuildTestEvidence};
        }
        if (requirement.requiresBuildTestEvidence && !evidence.buildTestEvidenceVerified)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::UnverifiedBuildTestEvidence};
        }
        if (!evidence.humanApproved)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::HumanApprovalRequired};
        }
        if (evidence.approvedContentHash != evidence.contentHash)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::ApprovalIdentityMismatch};
        }
        if (evidence.containsNativeCode && !requirement.nativeCodeAllowed)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::NativeCodeNotAllowed};
        }
        if (evidence.containsNativeCode && !evidence.nativeCodeApproved)
        {
            return {false, requirement.coreFallbackAvailable, PayloadRejectReason::NativeCodeApprovalRequired};
        }
        return {true, requirement.coreFallbackAvailable, PayloadRejectReason::None};
    }


    [[nodiscard]] constexpr PayloadActivationDecision evaluate_payload_activation(
        std::string_view packageId,
        const PackagePayloadEvidence& evidence) noexcept
    {
        return evaluate_payload_activation(payload_requirement(packageId), evidence);
    }

    [[nodiscard]] constexpr std::string_view recommended_local_image_model_id() noexcept
    {
        return kBonsaiImageTernaryPackageId;
    }

    [[nodiscard]] constexpr std::string_view engine_arcade_default_scene_id() noexcept
    {
        return kEngineArcadeDefaultSceneId;
    }

    [[nodiscard]] constexpr std::string_view engine_arcade_scene_ids() noexcept
    {
        return kEngineArcadeSceneIds;
    }

    [[nodiscard]] constexpr std::string_view engine_arcade_render_asset_role() noexcept
    {
        return kEngineArcadeRenderAssetRole;
    }

    [[nodiscard]] constexpr std::string_view engine_arcade_renderer_requirements() noexcept
    {
        return kEngineArcadeRendererRequirements;
    }

    [[nodiscard]] constexpr std::string_view engine_arcade_render_texture_name() noexcept
    {
        return kEngineArcadeRenderTextureName;
    }

    [[nodiscard]] constexpr std::uint32_t engine_arcade_render_texture_width() noexcept
    {
        return kEngineArcadeRenderTextureWidth;
    }

    [[nodiscard]] constexpr std::uint32_t engine_arcade_render_texture_height() noexcept
    {
        return kEngineArcadeRenderTextureHeight;
    }

    [[nodiscard]] constexpr bool validate_descriptor(const PackageDescriptor& package) noexcept
    {
        if (package.id.empty() || package.displayName.empty() || package.summary.empty())
        {
            return false;
        }

        if (is_network_sensitive(package) &&
            (!package.requiresExplicitNetworkApproval || !package.requiresHumanBuildGate))
        {
            return false;
        }

        if (package.kind == PackageKind::ModelAsset &&
            (package.activation != ActivationMode::ModelDownloadOptIn ||
             package.externalSourceRepo.empty() ||
             !package.requiresHumanBuildGate))
        {
            return false;
        }

        if ((package.kind == PackageKind::ResearchPrototype ||
             package.kind == PackageKind::DownloadableSource) &&
            (package.externalSourceRepo.empty() || !package.requiresHumanBuildGate))
        {
            return false;
        }

        if (package.activation == ActivationMode::ManualResearchImport &&
            package.externalSourceRepo.empty())
        {
            return false;
        }

        return true;
    }

    [[nodiscard]] constexpr std::size_t duplicate_id_count() noexcept
    {
        std::size_t duplicates = 0;
        for (std::size_t left = 0; left < kKnownPackages.size(); ++left)
        {
            for (std::size_t right = left + 1; right < kKnownPackages.size(); ++right)
            {
                if (kKnownPackages[left].id == kKnownPackages[right].id)
                {
                    ++duplicates;
                }
            }
        }

        return duplicates;
    }

    [[nodiscard]] constexpr RegistryValidation validate_registry() noexcept
    {
        RegistryValidation result{};
        result.packageCount = kKnownPackages.size();
        result.duplicateIdCount = duplicate_id_count();
        result.ok = result.duplicateIdCount == 0;

        for (const auto& package : kKnownPackages)
        {
            result.ok = result.ok && validate_descriptor(package);

            if (package.kind == PackageKind::ModelAsset)
            {
                ++result.modelAssetCount;
            }

            if (is_network_sensitive(package))
            {
                ++result.networkSensitiveCount;
            }
        }

        result.ok = result.ok && is_model_asset(recommended_local_image_model_id());
        return result;
    }
}
