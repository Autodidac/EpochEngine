/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module project.texture_library;

namespace epochengine::project_textures
{
    namespace
    {
        namespace fs = std::filesystem;

        inline constexpr std::string_view kArtifactExtension = ".epoch_texture";
        inline constexpr std::uint32_t kMaximumStoragePathBytes = 32'768;

        struct ReadResult final
        {
            LibraryCode code{LibraryCode::read_failure};
            asset::texture::CompiledTextureArtifact artifact{};
            std::vector<std::byte> bytes{};
        };

        [[nodiscard]] std::string hex_u64(std::uint64_t value)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result(16, '0');
            for (std::size_t index = 0; index < result.size(); ++index)
            {
                const std::size_t shift = (result.size() - index - 1u) * 4u;
                result[index] = digits[(value >> shift) & 0x0fu];
            }
            return result;
        }

        [[nodiscard]] fs::path texture_library_root(
            std::string_view projectRoot)
        {
            return fs::path{projectRoot} / "Library" / "Textures";
        }

        [[nodiscard]] fs::path asset_directory(
            std::string_view projectRoot,
            std::uint64_t assetKey)
        {
            return texture_library_root(projectRoot) / hex_u64(assetKey);
        }

        [[nodiscard]] fs::path artifact_path(
            std::string_view projectRoot,
            std::uint64_t assetKey,
            const asset::texture::ContentHash& artifactKey)
        {
            return asset_directory(projectRoot, assetKey)
                / (asset::texture::content_hash_hex(artifactKey)
                    + std::string{kArtifactExtension});
        }

        [[nodiscard]] bool same_identity(
            const asset::texture::CompiledTextureArtifactIdentity& left,
            const asset::texture::CompiledTextureArtifactIdentity& right) noexcept
        {
            return left.key.words == right.key.words
                && left.source_revision.content.words
                    == right.source_revision.content.words
                && left.source_revision.sequence == right.source_revision.sequence
                && left.profile == right.profile
                && left.width == right.width
                && left.height == right.height
                && left.mip_count == right.mip_count
                && left.estimated_artifact_bytes == right.estimated_artifact_bytes
                && left.compilation_required == right.compilation_required;
        }

        [[nodiscard]] bool same_artifact(
            const asset::texture::CompiledTextureArtifact& left,
            const asset::texture::CompiledTextureArtifact& right) noexcept
        {
            return left.status == right.status
                && same_identity(left.identity, right.identity)
                && left.payload_content.words == right.payload_content.words;
        }

        [[nodiscard]] ReadResult read_artifact(
            const fs::path& path,
            const TextureLibraryLimits& limits) noexcept
        {
            try
            {
                std::error_code error{};
                if (!fs::is_regular_file(path, error) || error)
                    return {LibraryCode::not_found};
                const std::uintmax_t fileBytes = fs::file_size(path, error);
                if (error || fileBytes == 0)
                    return {LibraryCode::read_failure};
                if (fileBytes > limits.maximum_serialized_bytes
                    || fileBytes > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::size_t>::max)()))
                {
                    return {LibraryCode::serialized_budget_exceeded};
                }

                std::vector<std::byte> bytes(
                    static_cast<std::size_t>(fileBytes));
                std::ifstream input(path, std::ios::binary);
                if (!input)
                    return {LibraryCode::read_failure};
                input.read(
                    reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                if (!input || input.gcount()
                    != static_cast<std::streamsize>(bytes.size()))
                {
                    return {LibraryCode::read_failure};
                }

                asset::texture::DeserializedTextureArtifact decoded =
                    asset::texture::deserialize_compiled_artifact(
                        bytes,
                        limits.read);
                if (!decoded)
                    return {LibraryCode::integrity_failure};
                return {
                    LibraryCode::ready,
                    std::move(decoded.artifact),
                    std::move(bytes)};
            }
            catch (...)
            {
                return {LibraryCode::allocation_failure};
            }
        }

        [[nodiscard]] LibraryCode write_bytes(
            const fs::path& path,
            const std::vector<std::byte>& bytes) noexcept
        {
            try
            {
                std::ofstream output(
                    path,
                    std::ios::binary | std::ios::trunc);
                if (!output)
                    return LibraryCode::write_failure;
                output.write(
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
                output.flush();
                if (!output)
                    return LibraryCode::write_failure;
                output.close();
                return output ? LibraryCode::ready : LibraryCode::write_failure;
            }
            catch (...)
            {
                return LibraryCode::write_failure;
            }
        }

        [[nodiscard]] bool artifact_file(const fs::path& path) noexcept
        {
            return path.extension().generic_string() == kArtifactExtension;
        }

        [[nodiscard]] std::uint64_t next_temporary_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1};
            std::uint64_t value = next.fetch_add(1, std::memory_order_relaxed);
            if (value == 0)
                value = next.fetch_add(1, std::memory_order_relaxed);
            return value;
        }

        [[nodiscard]] TextureArtifactLocator make_locator(
            LibraryCode code,
            std::string canonicalPath,
            const fs::path& storagePath,
            std::uint64_t projectKey,
            std::uint64_t assetKey,
            const asset::texture::CompiledTextureArtifact& artifact,
            std::uint64_t serializedBytes)
        {
            return {
                code,
                std::move(canonicalPath),
                storagePath.generic_string(),
                projectKey,
                assetKey,
                artifact.identity.key,
                artifact.identity.source_revision,
                artifact.identity.profile,
                serializedBytes};
        }
    }

    TextureArtifactLibrary::TextureArtifactLibrary(
        std::string projectId,
        std::string projectRoot,
        TextureLibraryLimits limits) noexcept
        : project_id_(std::move(projectId)), limits_(limits)
    {
        if (!limits_.valid()
            || projectRoot.empty()
            || projectRoot.size() > limits_.maximum_project_root_bytes)
        {
            return;
        }

        project_key_ = project_assets::stable_project_identity(
            project_id_, limits_.registry);
        if (project_key_ == 0)
            return;

        try
        {
            std::error_code error{};
            fs::path root{std::move(projectRoot)};
            if (root.is_relative())
                root = fs::absolute(root, error);
            if (error || root.empty())
            {
                project_key_ = 0;
                return;
            }
            root = root.lexically_normal();
            project_root_ = root.generic_string();
            if (project_root_.empty()
                || project_root_.size() > limits_.maximum_project_root_bytes)
            {
                project_root_.clear();
                project_key_ = 0;
            }
        }
        catch (...)
        {
            project_root_.clear();
            project_key_ = 0;
        }
    }

    bool TextureArtifactLibrary::valid() const noexcept
    {
        return limits_.valid() && project_key_ != 0 && !project_root_.empty();
    }

    std::string_view TextureArtifactLibrary::project_id() const noexcept
    {
        return project_id_;
    }

    std::string_view TextureArtifactLibrary::project_root() const noexcept
    {
        return project_root_;
    }

    std::uint64_t TextureArtifactLibrary::project_key() const noexcept
    {
        return project_key_;
    }

    const TextureLibraryLimits& TextureArtifactLibrary::limits() const noexcept
    {
        return limits_;
    }

    TextureLibraryMetrics TextureArtifactLibrary::metrics() const noexcept
    {
        return metrics_;
    }

    TextureArtifactLocator TextureArtifactLibrary::identify(
        std::string_view logicalPath,
        const asset::texture::CompiledTextureArtifact& artifact) noexcept
    {
        ++metrics_.identify_requests;
        if (!valid())
            return reject_locator(LibraryCode::invalid_library);
        if (!asset::texture::validate_compiled_artifact(artifact))
            return reject_locator(LibraryCode::invalid_artifact);

        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_locator(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::texture, *canonical);
            if (assetKey == 0)
                return reject_locator(LibraryCode::invalid_path);

            const asset::texture::SerializedTextureArtifact serialized =
                asset::texture::serialize_compiled_artifact(artifact);
            if (!serialized)
                return reject_locator(LibraryCode::invalid_artifact);
            if (serialized.bytes.size() > limits_.maximum_serialized_bytes)
            {
                return reject_locator(
                    LibraryCode::serialized_budget_exceeded);
            }

            const fs::path path = artifact_path(
                project_root_, assetKey, artifact.identity.key);
            if (path.generic_string().size() > kMaximumStoragePathBytes)
                return reject_locator(LibraryCode::invalid_path);
            return make_locator(
                LibraryCode::ready,
                *canonical,
                path,
                project_key_,
                assetKey,
                artifact,
                serialized.bytes.size());
        }
        catch (...)
        {
            return reject_locator(LibraryCode::allocation_failure);
        }
    }

    TextureArtifactLocator TextureArtifactLibrary::persist(
        std::string_view logicalPath,
        const asset::texture::CompiledTextureArtifact& artifact) noexcept
    {
        ++metrics_.persist_requests;
        if (!valid())
            return reject_locator(LibraryCode::invalid_library);
        if (!asset::texture::validate_compiled_artifact(artifact))
            return reject_locator(LibraryCode::invalid_artifact);

        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_locator(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::texture, *canonical);
            if (assetKey == 0)
                return reject_locator(LibraryCode::invalid_path);

            asset::texture::SerializedTextureArtifact serialized =
                asset::texture::serialize_compiled_artifact(artifact);
            if (!serialized)
                return reject_locator(LibraryCode::invalid_artifact);
            if (serialized.bytes.size() > limits_.maximum_serialized_bytes)
            {
                return reject_locator(
                    LibraryCode::serialized_budget_exceeded);
            }

            const fs::path directory = asset_directory(project_root_, assetKey);
            const fs::path destination = artifact_path(
                project_root_, assetKey, artifact.identity.key);
            if (destination.generic_string().size() > kMaximumStoragePathBytes)
                return reject_locator(LibraryCode::invalid_path);

            std::error_code error{};
            fs::create_directories(directory, error);
            if (error || !fs::is_directory(directory, error) || error)
                return reject_locator(LibraryCode::directory_failure);

            std::uint32_t artifactCount{};
            for (fs::directory_iterator iterator{directory, error}, end;
                !error && iterator != end;
                iterator.increment(error))
            {
                if (iterator->is_regular_file(error) && !error
                    && artifact_file(iterator->path()))
                {
                    ++artifactCount;
                }
            }
            if (error)
                return reject_locator(LibraryCode::directory_failure);

            if (fs::exists(destination, error) && !error)
            {
                ReadResult existing = read_artifact(destination, limits_);
                metrics_.bytes_read += existing.bytes.size();
                if (existing.code != LibraryCode::ready)
                    return reject_locator(existing.code);
                if (existing.bytes == serialized.bytes
                    && same_artifact(existing.artifact, artifact))
                {
                    ++metrics_.unchanged_writes;
                    return make_locator(
                        LibraryCode::unchanged,
                        *canonical,
                        destination,
                        project_key_,
                        assetKey,
                        artifact,
                        serialized.bytes.size());
                }
                return reject_locator(LibraryCode::identity_collision);
            }
            if (error)
                return reject_locator(LibraryCode::read_failure);
            if (artifactCount >= limits_.maximum_artifacts_per_asset)
                return reject_locator(LibraryCode::artifact_limit_exceeded);

            fs::path temporary = destination;
            temporary += ".pending." + hex_u64(next_temporary_id());
            const LibraryCode written = write_bytes(temporary, serialized.bytes);
            if (written != LibraryCode::ready)
            {
                fs::remove(temporary, error);
                return reject_locator(written);
            }

            ReadResult verified = read_artifact(temporary, limits_);
            metrics_.bytes_read += verified.bytes.size();
            if (verified.code != LibraryCode::ready
                || verified.bytes != serialized.bytes
                || !same_artifact(verified.artifact, artifact))
            {
                fs::remove(temporary, error);
                return reject_locator(LibraryCode::integrity_failure);
            }

            fs::rename(temporary, destination, error);
            if (error)
            {
                std::error_code raceError{};
                if (fs::exists(destination, raceError) && !raceError)
                {
                    ReadResult raced = read_artifact(destination, limits_);
                    metrics_.bytes_read += raced.bytes.size();
                    fs::remove(temporary, raceError);
                    if (raced.code == LibraryCode::ready
                        && raced.bytes == serialized.bytes
                        && same_artifact(raced.artifact, artifact))
                    {
                        ++metrics_.unchanged_writes;
                        return make_locator(
                            LibraryCode::unchanged,
                            *canonical,
                            destination,
                            project_key_,
                            assetKey,
                            artifact,
                            serialized.bytes.size());
                    }
                }
                fs::remove(temporary, raceError);
                return reject_locator(LibraryCode::write_failure);
            }

            ++metrics_.writes;
            metrics_.bytes_written += serialized.bytes.size();
            return make_locator(
                LibraryCode::ready,
                *canonical,
                destination,
                project_key_,
                assetKey,
                artifact,
                serialized.bytes.size());
        }
        catch (...)
        {
            return reject_locator(LibraryCode::allocation_failure);
        }
    }

    LoadedTextureArtifact TextureArtifactLibrary::load_exact(
        std::string_view logicalPath,
        const asset::texture::ContentHash& artifactKey) noexcept
    {
        ++metrics_.load_requests;
        if (!valid())
            return reject_load(LibraryCode::invalid_library);
        if (artifactKey.empty())
            return reject_load(LibraryCode::invalid_artifact);

        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_load(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::texture, *canonical);
            if (assetKey == 0)
                return reject_load(LibraryCode::invalid_path);
            const fs::path path = artifact_path(
                project_root_, assetKey, artifactKey);
            ReadResult read = read_artifact(path, limits_);
            metrics_.bytes_read += read.bytes.size();
            if (read.code != LibraryCode::ready)
                return reject_load(read.code);
            if (read.artifact.identity.key.words != artifactKey.words)
                return reject_load(LibraryCode::integrity_failure);

            ++metrics_.loads;
            TextureArtifactLocator locator = make_locator(
                LibraryCode::ready,
                *canonical,
                path,
                project_key_,
                assetKey,
                read.artifact,
                read.bytes.size());
            return {
                LibraryCode::ready,
                std::move(locator),
                std::move(read.artifact)};
        }
        catch (...)
        {
            return reject_load(LibraryCode::allocation_failure);
        }
    }

    LoadedTextureArtifact TextureArtifactLibrary::load_latest(
        std::string_view logicalPath,
        const asset::texture::TextureCompileProfile& profile) noexcept
    {
        ++metrics_.load_requests;
        if (!valid())
            return reject_load(LibraryCode::invalid_library);

        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_load(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::texture, *canonical);
            if (assetKey == 0)
                return reject_load(LibraryCode::invalid_path);

            const fs::path directory = asset_directory(project_root_, assetKey);
            std::error_code error{};
            if (!fs::is_directory(directory, error) || error)
                return reject_load(LibraryCode::not_found);

            std::vector<fs::path> candidates{};
            for (fs::directory_iterator iterator{directory, error}, end;
                !error && iterator != end;
                iterator.increment(error))
            {
                if (iterator->is_regular_file(error) && !error
                    && artifact_file(iterator->path()))
                {
                    candidates.push_back(iterator->path());
                    if (candidates.size() > limits_.maximum_artifacts_per_asset)
                    {
                        return reject_load(
                            LibraryCode::artifact_limit_exceeded);
                    }
                }
            }
            if (error)
                return reject_load(LibraryCode::directory_failure);
            std::sort(candidates.begin(), candidates.end());

            ReadResult selected{};
            fs::path selectedPath{};
            bool found{};
            for (const fs::path& candidate : candidates)
            {
                ReadResult read = read_artifact(candidate, limits_);
                metrics_.bytes_read += read.bytes.size();
                if (read.code != LibraryCode::ready)
                    return reject_load(read.code);
                const std::string expectedName =
                    asset::texture::content_hash_hex(read.artifact.identity.key)
                    + std::string{kArtifactExtension};
                if (candidate.filename().generic_string() != expectedName)
                    return reject_load(LibraryCode::integrity_failure);
                if (read.artifact.identity.profile != profile)
                    continue;

                if (!found
                    || read.artifact.identity.source_revision.sequence
                        > selected.artifact.identity.source_revision.sequence)
                {
                    selected = std::move(read);
                    selectedPath = candidate;
                    found = true;
                    continue;
                }
                if (read.artifact.identity.source_revision.sequence
                        == selected.artifact.identity.source_revision.sequence
                    && !same_artifact(read.artifact, selected.artifact))
                {
                    return reject_load(LibraryCode::ambiguous_artifact);
                }
            }
            if (!found)
                return reject_load(LibraryCode::not_found);

            ++metrics_.loads;
            TextureArtifactLocator locator = make_locator(
                LibraryCode::ready,
                *canonical,
                selectedPath,
                project_key_,
                assetKey,
                selected.artifact,
                selected.bytes.size());
            return {
                LibraryCode::ready,
                std::move(locator),
                std::move(selected.artifact)};
        }
        catch (...)
        {
            return reject_load(LibraryCode::allocation_failure);
        }
    }

    TextureArtifactLocator TextureArtifactLibrary::reject_locator(
        LibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }

    LoadedTextureArtifact TextureArtifactLibrary::reject_load(
        LibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {code};
    }
}
