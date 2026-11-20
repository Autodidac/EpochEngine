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

module project.gui_library;

import platform.filesystem;
import authoring.gui_document;
import authoring.gui_compiler;

namespace epochengine::project_gui
{
    namespace
    {
        namespace fs = std::filesystem;

        inline constexpr std::string_view artifactExtension = ".epoch_gui";
        inline constexpr std::uint32_t maximumStoragePathBytes = 32'768u;

        struct ReadResult final
        {
            LibraryCode code{LibraryCode::read_failure};
            asset::gui::CompiledGuiArtifact artifact{};
            std::vector<std::byte> bytes{};
        };

        [[nodiscard]] std::string hex_u64(std::uint64_t value)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result(16u, '0');
            for (std::size_t index = 0u; index < result.size(); ++index)
            {
                const std::size_t shift = (result.size() - index - 1u) * 4u;
                result[index] = digits[(value >> shift) & 0x0fu];
            }
            return result;
        }

        [[nodiscard]] std::string hash_hex(
            const asset::gui::ContentHash& hash)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result{};
            result.reserve(hash.bytes.size() * 2u);
            for (const auto byte : hash.bytes)
            {
                result.push_back(digits[byte >> 4u]);
                result.push_back(digits[byte & 0x0fu]);
            }
            return result;
        }

        [[nodiscard]] fs::path library_root(std::string_view projectRoot)
        {
            return fs::path{projectRoot} / "Library" / "Gui";
        }

        [[nodiscard]] fs::path asset_directory(
            std::string_view projectRoot,
            std::uint64_t assetKey)
        {
            return library_root(projectRoot) / hex_u64(assetKey);
        }

        [[nodiscard]] fs::path artifact_path(
            std::string_view projectRoot,
            std::uint64_t assetKey,
            const asset::gui::ContentHash& key)
        {
            return asset_directory(projectRoot, assetKey)
                / (hash_hex(key) + std::string{artifactExtension});
        }

        [[nodiscard]] bool artifact_file(const fs::path& path) noexcept
        {
            return path.extension().generic_string() == artifactExtension;
        }

        [[nodiscard]] ReadResult read_artifact(
            const fs::path& path,
            const LibraryLimits& limits) noexcept
        {
            try
            {
                std::error_code error{};
                if (!fs::is_regular_file(path, error) || error)
                    return {LibraryCode::not_found};
                const std::uintmax_t fileBytes = fs::file_size(path, error);
                if (error || fileBytes == 0u)
                    return {LibraryCode::read_failure};
                if (fileBytes > limits.maximum_serialized_bytes
                    || fileBytes > static_cast<std::uintmax_t>(
                        (std::numeric_limits<std::size_t>::max)()))
                {
                    return {LibraryCode::serialized_budget_exceeded};
                }

                std::vector<std::byte> bytes(
                    static_cast<std::size_t>(fileBytes));
                std::ifstream input{path, std::ios::binary};
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

                auto decoded = asset::gui::deserialize(
                    bytes, limits.artifact);
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

        [[nodiscard]] std::uint64_t next_temporary_id() noexcept
        {
            static std::atomic<std::uint64_t> next{1u};
            std::uint64_t value = next.fetch_add(1u, std::memory_order_relaxed);
            if (value == 0u)
                value = next.fetch_add(1u, std::memory_order_relaxed);
            return value;
        }

        [[nodiscard]] ArtifactLocator make_locator(
            LibraryCode code,
            std::string canonicalPath,
            const fs::path& storagePath,
            std::uint64_t projectKey,
            std::uint64_t assetKey,
            const asset::gui::CompiledGuiArtifact& artifact,
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
                serializedBytes};
        }

        [[nodiscard]] bool newer(
            const asset::gui::CompiledGuiArtifact& candidate,
            const asset::gui::CompiledGuiArtifact& current) noexcept
        {
            if (candidate.identity.source_revision.sequence
                != current.identity.source_revision.sequence)
            {
                return candidate.identity.source_revision.sequence
                    > current.identity.source_revision.sequence;
            }
            return hash_hex(candidate.identity.key)
                > hash_hex(current.identity.key);
        }
    }

    ArtifactLibrary::ArtifactLibrary(
        std::string projectId,
        std::string projectRoot,
        LibraryLimits limits) noexcept
        : project_id_(std::move(projectId)), limits_(limits)
    {
        if (!limits_.valid() || projectRoot.empty()
            || projectRoot.size() > limits_.maximum_project_root_bytes)
        {
            return;
        }
        project_key_ = project_assets::stable_project_identity(
            project_id_, limits_.registry);
        if (project_key_ == 0u)
            return;
        try
        {
            std::error_code error{};
            fs::path root{std::move(projectRoot)};
            if (root.is_relative())
                root = fs::absolute(root, error);
            if (error || root.empty())
            {
                project_key_ = 0u;
                return;
            }
            project_root_ = root.lexically_normal().generic_string();
            if (project_root_.empty()
                || project_root_.size() > limits_.maximum_project_root_bytes)
            {
                project_root_.clear();
                project_key_ = 0u;
            }
        }
        catch (...)
        {
            project_root_.clear();
            project_key_ = 0u;
        }
    }

    bool ArtifactLibrary::valid() const noexcept
    {
        return limits_.valid() && project_key_ != 0u && !project_root_.empty();
    }

    std::string_view ArtifactLibrary::project_id() const noexcept
    {
        return project_id_;
    }

    std::string_view ArtifactLibrary::project_root() const noexcept
    {
        return project_root_;
    }

    std::uint64_t ArtifactLibrary::project_key() const noexcept
    {
        return project_key_;
    }

    const LibraryLimits& ArtifactLibrary::limits() const noexcept
    {
        return limits_;
    }

    LibraryMetrics ArtifactLibrary::metrics() const noexcept
    {
        return metrics_;
    }

    ArtifactLocator ArtifactLibrary::identify(
        std::string_view logicalPath,
        const asset::gui::CompiledGuiArtifact& artifact) noexcept
    {
        ++metrics_.identify_requests;
        if (!valid())
            return reject_locator(LibraryCode::invalid_library);
        if (!asset::gui::validate(artifact, limits_.artifact))
            return reject_locator(LibraryCode::invalid_artifact);
        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath, limits_.registry);
            if (!canonical)
                return reject_locator(LibraryCode::invalid_path);
            const std::uint64_t assetKey = project_assets::stable_asset_identity(
                project_key_, project_assets::AssetKind::gui_document,
                *canonical);
            const auto serialized = asset::gui::serialize(
                artifact, limits_.artifact);
            if (assetKey == 0u || !serialized)
                return reject_locator(LibraryCode::invalid_artifact);
            if (serialized.bytes.size() > limits_.maximum_serialized_bytes)
                return reject_locator(
                    LibraryCode::serialized_budget_exceeded);
            const fs::path path = artifact_path(
                project_root_, assetKey, artifact.identity.key);
            if (path.generic_string().size() > maximumStoragePathBytes)
                return reject_locator(LibraryCode::invalid_path);
            return make_locator(
                LibraryCode::ready, *canonical, path, project_key_, assetKey,
                artifact, serialized.bytes.size());
        }
        catch (...)
        {
            return reject_locator(LibraryCode::allocation_failure);
        }
    }

    ArtifactLocator ArtifactLibrary::persist(
        std::string_view logicalPath,
        const asset::gui::CompiledGuiArtifact& artifact) noexcept
    {
        ++metrics_.persist_requests;
        ArtifactLocator locator = identify(logicalPath, artifact);
        if (!locator)
            return locator;
        try
        {
            auto serialized = asset::gui::serialize(
                artifact, limits_.artifact);
            if (!serialized)
                return reject_locator(LibraryCode::invalid_artifact);
            const fs::path destination{locator.storage_path};
            const fs::path directory = destination.parent_path();
            std::error_code error{};
            fs::create_directories(directory, error);
            if (error || !fs::is_directory(directory, error) || error)
                return reject_locator(LibraryCode::directory_failure);

            std::uint32_t artifactCount{};
            for (fs::directory_iterator iterator{directory, error}, end;
                 !error && iterator != end; iterator.increment(error))
            {
                if (iterator->is_regular_file(error) && !error
                    && artifact_file(iterator->path()))
                {
                    ++artifactCount;
                }
            }
            if (error)
                return reject_locator(LibraryCode::directory_failure);

            const auto read_existing = [&]() -> ArtifactLocator
            {
                ReadResult existing = read_artifact(destination, limits_);
                metrics_.bytes_read += existing.bytes.size();
                if (existing.code != LibraryCode::ready)
                    return reject_locator(existing.code);
                if (existing.bytes == serialized.bytes
                    && existing.artifact.identity == artifact.identity)
                {
                    ++metrics_.unchanged_writes;
                    locator.code = LibraryCode::unchanged;
                    return locator;
                }
                return reject_locator(LibraryCode::identity_collision);
            };
            if (fs::exists(destination, error) && !error)
                return read_existing();
            if (error)
                return reject_locator(LibraryCode::read_failure);
            if (artifactCount >= limits_.maximum_artifacts_per_asset)
                return reject_locator(
                    LibraryCode::artifact_limit_exceeded);

            // Exclusive creation leaves another process's staging file untouched.
            fs::path temporary;
            constexpr unsigned maximumStagingAttempts = 8u;
            for (unsigned attempt = 0u; attempt < maximumStagingAttempts; ++attempt)
            {
                temporary = directory / (".pending." + hex_u64(next_temporary_id()));
                if (platform::filesystem::exclusive_create_and_write(temporary, serialized.bytes, error))
                    break;
                if (error != std::errc::file_exists || attempt + 1u == maximumStagingAttempts)
                    return reject_locator(LibraryCode::write_failure);
            }
            ReadResult verified = read_artifact(temporary, limits_);
            metrics_.bytes_read += verified.bytes.size();
            if (verified.code != LibraryCode::ready
                || verified.bytes != serialized.bytes
                || verified.artifact.identity != artifact.identity)
            {
                fs::remove(temporary, error);
                return reject_locator(LibraryCode::integrity_failure);
            }

            if (!platform::filesystem::atomic_publish_new_same_filesystem(temporary, destination, error))
            {
                std::error_code cleanupError{};
                fs::remove(temporary, cleanupError);
                if (error == std::errc::file_exists)
                    return read_existing();
                return reject_locator(LibraryCode::write_failure);
            }
            ++metrics_.writes;
            metrics_.bytes_written += serialized.bytes.size();
            return locator;
        }
        catch (...)
        {
            return reject_locator(LibraryCode::allocation_failure);
        }
    }

    LoadedArtifact ArtifactLibrary::load_exact(
        std::string_view logicalPath,
        const asset::gui::ContentHash& artifactKey) noexcept
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
                project_key_, project_assets::AssetKind::gui_document,
                *canonical);
            if (assetKey == 0u)
                return reject_load(LibraryCode::invalid_path);
            const fs::path path = artifact_path(
                project_root_, assetKey, artifactKey);
            ReadResult read = read_artifact(path, limits_);
            metrics_.bytes_read += read.bytes.size();
            if (read.code != LibraryCode::ready)
                return reject_load(read.code);
            if (read.artifact.identity.key != artifactKey)
                return reject_load(LibraryCode::integrity_failure);
            ArtifactLocator locator = make_locator(
                LibraryCode::ready, *canonical, path, project_key_, assetKey,
                read.artifact, read.bytes.size());
            ++metrics_.loads;
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

    LoadedArtifact ArtifactLibrary::load_latest(
        std::string_view logicalPath) noexcept
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
                project_key_, project_assets::AssetKind::gui_document,
                *canonical);
            if (assetKey == 0u)
                return reject_load(LibraryCode::invalid_path);
            const fs::path directory = asset_directory(project_root_, assetKey);
            std::error_code error{};
            if (!fs::is_directory(directory, error) || error)
                return reject_load(LibraryCode::not_found);

            bool found{};
            ReadResult selected{};
            fs::path selectedPath{};
            for (fs::directory_iterator iterator{directory, error}, end;
                 !error && iterator != end; iterator.increment(error))
            {
                if (!iterator->is_regular_file(error) || error
                    || !artifact_file(iterator->path()))
                {
                    continue;
                }
                ReadResult candidate = read_artifact(
                    iterator->path(), limits_);
                metrics_.bytes_read += candidate.bytes.size();
                if (candidate.code != LibraryCode::ready)
                    continue;
                if (!found || newer(candidate.artifact, selected.artifact))
                {
                    selected = std::move(candidate);
                    selectedPath = iterator->path();
                    found = true;
                }
            }
            if (error)
                return reject_load(LibraryCode::directory_failure);
            if (!found)
                return reject_load(LibraryCode::not_found);

            ArtifactLocator locator = make_locator(
                LibraryCode::ready, *canonical, selectedPath, project_key_,
                assetKey, selected.artifact, selected.bytes.size());
            ++metrics_.loads;
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

    LoadedArtifact ArtifactLibrary::load_or_compile(
        std::string_view logicalPath) noexcept
    {
        auto loaded = load_latest(logicalPath);
        if (loaded.code == LibraryCode::not_found
            && logicalPath == canonical_source_path)
        {
            return compile_source(logicalPath);
        }
        return loaded;
    }

    LoadedArtifact ArtifactLibrary::compile_source(
        std::string_view logicalPath) noexcept
    {
        if (!valid())
            return reject_load(LibraryCode::invalid_library);
        if (logicalPath != canonical_source_path)
            return reject_load(LibraryCode::invalid_path);

        try
        {
            constexpr std::uintmax_t maximumSourceBytes = 64ull * 1024ull * 1024ull;
            std::error_code error{};
            const auto root = fs::canonical(fs::path{project_root_}, error);
            if (error)
                return reject_load(LibraryCode::invalid_path);
            const auto source = root / fs::path{logicalPath};
            const auto status = fs::symlink_status(source, error);
            if (status.type() == fs::file_type::not_found
                && (!error || error == std::errc::no_such_file_or_directory))
            {
                return reject_load(LibraryCode::not_found);
            }
            if (error || !fs::is_regular_file(status)
                || fs::canonical(source, error) != source || error)
            {
                return reject_load(LibraryCode::invalid_path);
            }

            const auto byteCount = fs::file_size(source, error);
            if (error || byteCount == 0u)
                return reject_load(LibraryCode::read_failure);
            if (byteCount > maximumSourceBytes)
                return reject_load(LibraryCode::serialized_budget_exceeded);
            std::vector<std::byte> bytes(static_cast<std::size_t>(byteCount));
            std::ifstream input{source, std::ios::binary};
            if (!input.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()))
                || input.peek() != std::char_traits<char>::eof() || input.bad())
            {
                return reject_load(LibraryCode::read_failure);
            }

            const auto decoded = authoring::gui::deserialize_gui_document(bytes);
            if (!decoded)
                return reject_load(LibraryCode::invalid_artifact);
            const auto compiled = authoring::gui::compile_gui_document(*decoded.snapshot);
            if (!compiled)
                return reject_load(LibraryCode::invalid_artifact);
            const auto candidate = identify(logicalPath, compiled.artifact);
            if (!candidate)
                return reject_load(candidate.code);

            const auto destination = artifact_path(root.generic_string(), candidate.asset_key,
                compiled.artifact.identity.key);
            if (fs::weakly_canonical(destination, error) != destination || error
                || fs::is_symlink(fs::symlink_status(destination, error))
                || (error && error != std::errc::no_such_file_or_directory))
                return reject_load(LibraryCode::invalid_path);

            const auto published = persist(logicalPath, compiled.artifact);
            return published ? load_exact(logicalPath, compiled.artifact.identity.key)
                : reject_load(published.code);
        }
        catch (...)
        {
            return reject_load(LibraryCode::allocation_failure);
        }
    }

    ArtifactLocator ArtifactLibrary::reject_locator(
        LibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }

    LoadedArtifact ArtifactLibrary::reject_load(LibraryCode code) noexcept
    {
        ++metrics_.rejected_operations;
        return {.code = code};
    }
}
