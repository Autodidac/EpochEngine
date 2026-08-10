/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <filesystem>
#include <string_view>

export module asset.texture_import;

import asset.texture_artifact;

export namespace epochengine::asset::texture
{
    enum class TextureImportStatus : std::uint8_t
    {
        ready,
        invalid_request,
        source_not_found,
        source_too_large,
        unsupported_format,
        malformed_source,
        dimensions_exceeded,
        payload_exceeded,
        allocation_failure,
        artifact_failure
    };

    [[nodiscard]] constexpr std::string_view texture_import_status_name(
        TextureImportStatus status) noexcept
    {
        switch (status)
        {
        case TextureImportStatus::ready: return "ready";
        case TextureImportStatus::invalid_request: return "invalid_request";
        case TextureImportStatus::source_not_found: return "source_not_found";
        case TextureImportStatus::source_too_large: return "source_too_large";
        case TextureImportStatus::unsupported_format: return "unsupported_format";
        case TextureImportStatus::malformed_source: return "malformed_source";
        case TextureImportStatus::dimensions_exceeded: return "dimensions_exceeded";
        case TextureImportStatus::payload_exceeded: return "payload_exceeded";
        case TextureImportStatus::allocation_failure: return "allocation_failure";
        case TextureImportStatus::artifact_failure: return "artifact_failure";
        }
        return "unknown";
    }

    struct TextureImportLimits final
    {
        std::uint64_t maximum_source_bytes{64ull * 1024ull * 1024ull};
        std::uint64_t maximum_decoded_bytes{256ull * 1024ull * 1024ull};
        std::uint32_t maximum_dimension{16'384};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_source_bytes != 0u
                && maximum_decoded_bytes != 0u
                && maximum_dimension != 0u;
        }
    };

    struct TextureImportRequest final
    {
        std::filesystem::path source_path{};
        std::uint64_t source_sequence{1u};
        TextureCompileProfile profile{};
        TextureImportLimits limits{};
    };

    struct TextureImportResult final
    {
        TextureImportStatus status{TextureImportStatus::invalid_request};
        CompiledTextureArtifact artifact{};
        std::uint64_t source_bytes{};
        std::uint64_t decoded_bytes{};
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return status == TextureImportStatus::ready
                && static_cast<bool>(artifact);
        }
    };

    [[nodiscard]] TextureImportResult import_texture_file(
        const TextureImportRequest& request) noexcept;

    enum class TextureImportContractFailure : std::uint8_t
    {
        none,
        temporary_source,
        ppm_import,
        deterministic_identity,
        malformed_rejection,
        oversized_rejection
    };

    [[nodiscard]] constexpr std::string_view
        texture_import_contract_failure_name(
            TextureImportContractFailure failure) noexcept
    {
        switch (failure)
        {
        case TextureImportContractFailure::none: return "pass";
        case TextureImportContractFailure::temporary_source:
            return "temporary_source";
        case TextureImportContractFailure::ppm_import: return "ppm_import";
        case TextureImportContractFailure::deterministic_identity:
            return "deterministic_identity";
        case TextureImportContractFailure::malformed_rejection:
            return "malformed_rejection";
        case TextureImportContractFailure::oversized_rejection:
            return "oversized_rejection";
        }
        return "unknown";
    }

    [[nodiscard]] TextureImportContractFailure
        texture_import_contract_failure() noexcept;
}
