/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstdint>
#include <string>
#include <string_view>

export module project.texture_library;

import asset.texture_artifact;
import project.asset_registry;

export namespace epochengine::project_textures
{
    enum class LibraryCode : std::uint8_t
    {
        ready,
        unchanged,
        invalid_library,
        invalid_project,
        invalid_path,
        invalid_artifact,
        source_revision_mismatch,
        serialized_budget_exceeded,
        artifact_limit_exceeded,
        directory_failure,
        write_failure,
        read_failure,
        integrity_failure,
        identity_collision,
        ambiguous_artifact,
        not_found,
        allocation_failure
    };

    [[nodiscard]] constexpr std::string_view library_code_name(
        LibraryCode code) noexcept
    {
        switch (code)
        {
        case LibraryCode::ready: return "ready";
        case LibraryCode::unchanged: return "unchanged";
        case LibraryCode::invalid_library: return "invalid_library";
        case LibraryCode::invalid_project: return "invalid_project";
        case LibraryCode::invalid_path: return "invalid_path";
        case LibraryCode::invalid_artifact: return "invalid_artifact";
        case LibraryCode::source_revision_mismatch:
            return "source_revision_mismatch";
        case LibraryCode::serialized_budget_exceeded:
            return "serialized_budget_exceeded";
        case LibraryCode::artifact_limit_exceeded:
            return "artifact_limit_exceeded";
        case LibraryCode::directory_failure: return "directory_failure";
        case LibraryCode::write_failure: return "write_failure";
        case LibraryCode::read_failure: return "read_failure";
        case LibraryCode::integrity_failure: return "integrity_failure";
        case LibraryCode::identity_collision: return "identity_collision";
        case LibraryCode::ambiguous_artifact: return "ambiguous_artifact";
        case LibraryCode::not_found: return "not_found";
        case LibraryCode::allocation_failure: return "allocation_failure";
        }
        return "unknown";
    }

    struct TextureLibraryLimits final
    {
        std::uint32_t maximum_artifacts_per_asset{64};
        std::uint64_t maximum_serialized_bytes{512ull * 1024ull * 1024ull};
        std::uint32_t maximum_project_root_bytes{4'096};
        project_assets::RegistryLimits registry{};
        asset::texture::ArtifactReadLimits read{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return maximum_artifacts_per_asset != 0
                && maximum_serialized_bytes != 0
                && maximum_project_root_bytes != 0
                && registry.valid()
                && read.maximum_serialized_bytes != 0
                && read.maximum_payload_bytes != 0
                && read.maximum_dimension != 0
                && read.maximum_mip_count != 0
                && maximum_serialized_bytes <= read.maximum_serialized_bytes;
        }
    };

    struct TextureLibraryMetrics final
    {
        std::uint64_t identify_requests{};
        std::uint64_t persist_requests{};
        std::uint64_t writes{};
        std::uint64_t unchanged_writes{};
        std::uint64_t load_requests{};
        std::uint64_t loads{};
        std::uint64_t bytes_written{};
        std::uint64_t bytes_read{};
        std::uint64_t rejected_operations{};
    };

    struct TextureArtifactLocator final
    {
        LibraryCode code{LibraryCode::invalid_library};
        std::string canonical_logical_path{};
        std::string storage_path{};
        std::uint64_t project_key{};
        std::uint64_t asset_key{};
        asset::texture::ContentHash artifact_key{};
        asset::texture::DocumentRevision source_revision{};
        asset::texture::TextureCompileProfile profile{};
        std::uint64_t serialized_bytes{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return (code == LibraryCode::ready
                    || code == LibraryCode::unchanged)
                && !canonical_logical_path.empty()
                && !storage_path.empty()
                && project_key != 0
                && asset_key != 0
                && !artifact_key.empty()
                && !source_revision.content.empty()
                && source_revision.sequence != 0
                && serialized_bytes != 0;
        }
    };

    struct LoadedTextureArtifact final
    {
        LibraryCode code{LibraryCode::invalid_library};
        TextureArtifactLocator locator{};
        asset::texture::CompiledTextureArtifact artifact{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return code == LibraryCode::ready
                && static_cast<bool>(locator)
                && static_cast<bool>(artifact);
        }
    };

    class TextureArtifactLibrary final
    {
    public:
        TextureArtifactLibrary(
            std::string projectId,
            std::string projectRoot,
            TextureLibraryLimits limits = {}) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::string_view project_id() const noexcept;
        [[nodiscard]] std::string_view project_root() const noexcept;
        [[nodiscard]] std::uint64_t project_key() const noexcept;
        [[nodiscard]] const TextureLibraryLimits& limits() const noexcept;
        [[nodiscard]] TextureLibraryMetrics metrics() const noexcept;

        [[nodiscard]] TextureArtifactLocator identify(
            std::string_view logicalPath,
            const asset::texture::CompiledTextureArtifact& artifact) noexcept;

        [[nodiscard]] TextureArtifactLocator persist(
            std::string_view logicalPath,
            const asset::texture::CompiledTextureArtifact& artifact) noexcept;

        [[nodiscard]] LoadedTextureArtifact load_exact(
            std::string_view logicalPath,
            const asset::texture::ContentHash& artifactKey) noexcept;

        [[nodiscard]] LoadedTextureArtifact load_latest(
            std::string_view logicalPath,
            const asset::texture::TextureCompileProfile& profile) noexcept;

    private:
        [[nodiscard]] TextureArtifactLocator reject_locator(
            LibraryCode code) noexcept;
        [[nodiscard]] LoadedTextureArtifact reject_load(
            LibraryCode code) noexcept;

        std::string project_id_{};
        std::string project_root_{};
        TextureLibraryLimits limits_{};
        std::uint64_t project_key_{};
        TextureLibraryMetrics metrics_{};
    };
}
