/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module editor.project_textures;

import asset.texture_artifact;
import asset.texture_import;
import authoring.texture;
import capability.profile;
import platform.budgets;
import project.asset_registry;
import project.texture_admission;
import project.texture_pipeline;
import project.texture_resources;
import project.texture_source;
import render.canvas2d;
import render.canvas2d_scene;
import scene.snapshot;

namespace epochengine::editor_project_textures
{
    namespace
    {
        namespace fs = std::filesystem;

        [[nodiscard]] fs::path normalized_absolute(
            const fs::path& source) noexcept
        {
            try
            {
                std::error_code error{};
                fs::path absolute = fs::absolute(source, error);
                if (error)
                    return {};
                fs::path canonical = fs::weakly_canonical(absolute, error);
                return error ? absolute.lexically_normal() : canonical;
            }
            catch (...)
            {
                return {};
            }
        }

        [[nodiscard]] bool path_component_equal(
            std::string left,
            std::string right) noexcept
        {
#if defined(_WIN32)
            const auto lower = [](std::string& value)
            {
                std::transform(
                    value.begin(),
                    value.end(),
                    value.begin(),
                    [](unsigned char character)
                    {
                        return static_cast<char>(std::tolower(character));
                    });
            };
            lower(left);
            lower(right);
#endif
            return left == right;
        }

        [[nodiscard]] bool path_is_within(
            const fs::path& candidate,
            const fs::path& root) noexcept
        {
            auto candidateIt = candidate.begin();
            auto rootIt = root.begin();
            for (; rootIt != root.end(); ++rootIt, ++candidateIt)
            {
                if (candidateIt == candidate.end()
                    || !path_component_equal(
                        candidateIt->string(),
                        rootIt->string()))
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] std::optional<std::string> logical_source_path(
            const fs::path& source,
            const fs::path& assetsRoot) noexcept
        {
            if (source.empty()
                || assetsRoot.empty()
                || !path_is_within(source, assetsRoot))
            {
                return std::nullopt;
            }
            std::error_code error{};
            const fs::path relative = fs::relative(source, assetsRoot, error);
            if (error || relative.empty())
                return std::nullopt;
            for (const fs::path& component : relative)
            {
                if (component == ".." || component == ".")
                    return std::nullopt;
            }
            return std::string{"Assets/"} + relative.generic_string();
        }

        [[nodiscard]] std::string lowercase_extension(
            const fs::path& source)
        {
            std::string extension = source.extension().string();
            std::transform(
                extension.begin(),
                extension.end(),
                extension.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return extension;
        }

        [[nodiscard]] bool supported_source_extension(
            const fs::path& source)
        {
            const std::string extension = lowercase_extension(source);
            return extension == ".ppm"
                || extension == ".bmp"
                || extension == ".tga";
        }

        [[nodiscard]] bool catalog_source_extension(
            const fs::path& source)
        {
            return supported_source_extension(source)
                || lowercase_extension(source) == ".epoch_texture";
        }

        [[nodiscard]] asset::texture::TextureCompileProfile
            imported_texture_profile() noexcept
        {
            asset::texture::TextureCompileProfile profile{};
            profile.format = asset::texture::ArtifactFormat::rgba8_unorm;
            profile.color_space = asset::texture::ColorSpace::linear;
            profile.mipmaps =
                asset::texture::MipmapPolicy::preserve_authored;
            return profile;
        }

        [[nodiscard]] asset::texture::TextureCompileProfile
            authored_texture_profile(
                const authoring::texture::TextureDocument& document) noexcept
        {
            asset::texture::TextureCompileProfile profile{};
            const auto& canvas = document.descriptor();
            profile.format =
                canvas.format
                    == authoring::texture::PixelFormat::rgba8_srgb
                ? asset::texture::ArtifactFormat::rgba8_srgb
                : asset::texture::ArtifactFormat::rgba8_unorm;
            profile.color_space = canvas.color_space;
            profile.mipmaps =
                asset::texture::MipmapPolicy::preserve_authored;
            return profile;
        }

        [[nodiscard]] std::uint64_t material_stable_key(
            canvas2d::LogicalTextureReference logical) noexcept
        {
            std::uint64_t value = logical.asset_key
                ^ std::rotl(logical.artifact_revision, 17);
            return value == 0u ? 1u : value;
        }

        [[nodiscard]] asset::texture::ContentHash content_hash(
            const scene::SceneTextureMaterialSnapshot& material) noexcept
        {
            return asset::texture::ContentHash{material.artifact_key};
        }

        [[nodiscard]] bool same_material_identity(
            const scene::SceneTextureMaterialSnapshot& left,
            const scene::SceneTextureMaterialSnapshot& right) noexcept
        {
            return left.logical_path == right.logical_path
                && left.artifact_key == right.artifact_key;
        }

        struct ContractRoot final
        {
            fs::path path{};

            ContractRoot() noexcept
            {
                static std::atomic<std::uint64_t> sequence{1u};
                std::error_code error{};
                path = fs::temp_directory_path(error);
                if (error)
                {
                    path.clear();
                    return;
                }
                path /= "epoch_editor_project_textures_contract_"
                    + std::to_string(sequence.fetch_add(1u));
                fs::create_directories(path / "Assets" / "Textures", error);
                if (error)
                    path.clear();
            }

            ~ContractRoot()
            {
                if (path.empty())
                    return;
                std::error_code error{};
                fs::remove_all(path, error);
            }
        };

        [[nodiscard]] bool write_contract_ppm(
            const fs::path& path,
            std::array<std::uint8_t, 6> pixels) noexcept
        {
            try
            {
                std::ofstream output(path, std::ios::binary);
                if (!output)
                    return false;
                output << "P6\n2 1\n255\n";
                output.write(
                    reinterpret_cast<const char*>(pixels.data()),
                    static_cast<std::streamsize>(pixels.size()));
                return static_cast<bool>(output);
            }
            catch (...)
            {
                return false;
            }
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
                .implementation_id = 0x45504f4354584354ull,
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
                .software_fallback = false
            };
        }
    }

    std::optional<TextureThumbnail> make_texture_thumbnail(
        const TexturePreview& preview,
        TextureThumbnailPolicy policy) noexcept
    {
        if (!preview || !policy.valid())
            return std::nullopt;

        try
        {
            std::uint32_t targetWidth = preview.width;
            std::uint32_t targetHeight = preview.height;
            if (targetWidth > policy.maximum_width
                || targetHeight > policy.maximum_height)
            {
                const std::uint64_t widthLimitedHeight =
                    static_cast<std::uint64_t>(preview.height)
                    * policy.maximum_width;
                const std::uint64_t heightLimitedWidth =
                    static_cast<std::uint64_t>(preview.width)
                    * policy.maximum_height;
                if (widthLimitedHeight
                    <= static_cast<std::uint64_t>(preview.width)
                        * policy.maximum_height)
                {
                    targetWidth = policy.maximum_width;
                    targetHeight = static_cast<std::uint32_t>((std::max)(
                        std::uint64_t{1u},
                        widthLimitedHeight / preview.width));
                }
                else
                {
                    targetHeight = policy.maximum_height;
                    targetWidth = static_cast<std::uint32_t>((std::max)(
                        std::uint64_t{1u},
                        heightLimitedWidth / preview.height));
                }
            }

            targetWidth = (std::min)(targetWidth, policy.maximum_width);
            targetHeight = (std::min)(targetHeight, policy.maximum_height);
            TextureThumbnail result{
                .logical_path = preview.logical_path,
                .artifact_key = preview.artifact_key,
                .width = targetWidth,
                .height = targetHeight
            };

            if (targetWidth == preview.width
                && targetHeight == preview.height)
            {
                result.rgba8 = preview.rgba8;
                return result;
            }

            const std::uint64_t outputBytes =
                static_cast<std::uint64_t>(targetWidth)
                * targetHeight
                * 4u;
            if (outputBytes > static_cast<std::uint64_t>(
                    (std::numeric_limits<std::size_t>::max)()))
            {
                return std::nullopt;
            }
            result.rgba8.resize(static_cast<std::size_t>(outputBytes));

            for (std::uint32_t targetY = 0u;
                 targetY < targetHeight;
                 ++targetY)
            {
                const std::uint32_t sourceYBegin =
                    static_cast<std::uint32_t>(
                        static_cast<std::uint64_t>(targetY)
                        * preview.height / targetHeight);
                const std::uint32_t sourceYEnd = (std::min)(
                    preview.height,
                    static_cast<std::uint32_t>(
                        (static_cast<std::uint64_t>(targetY + 1u)
                            * preview.height
                            + targetHeight - 1u)
                        / targetHeight));

                for (std::uint32_t targetX = 0u;
                     targetX < targetWidth;
                     ++targetX)
                {
                    const std::uint32_t sourceXBegin =
                        static_cast<std::uint32_t>(
                            static_cast<std::uint64_t>(targetX)
                            * preview.width / targetWidth);
                    const std::uint32_t sourceXEnd = (std::min)(
                        preview.width,
                        static_cast<std::uint32_t>(
                            (static_cast<std::uint64_t>(targetX + 1u)
                                * preview.width
                                + targetWidth - 1u)
                            / targetWidth));

                    std::array<std::uint64_t, 4> sums{};
                    std::uint64_t sampleCount{};
                    for (std::uint32_t sourceY = sourceYBegin;
                         sourceY < sourceYEnd;
                         ++sourceY)
                    {
                        for (std::uint32_t sourceX = sourceXBegin;
                             sourceX < sourceXEnd;
                             ++sourceX)
                        {
                            const std::size_t sourceOffset =
                                (static_cast<std::size_t>(sourceY)
                                    * preview.width
                                    + sourceX)
                                * 4u;
                            for (std::size_t channel = 0u;
                                 channel < sums.size();
                                 ++channel)
                            {
                                sums[channel] +=
                                    preview.rgba8[sourceOffset + channel];
                            }
                            ++sampleCount;
                        }
                    }

                    if (sampleCount == 0u)
                        return std::nullopt;
                    const std::size_t targetOffset =
                        (static_cast<std::size_t>(targetY)
                            * targetWidth
                            + targetX)
                        * 4u;
                    for (std::size_t channel = 0u;
                         channel < sums.size();
                         ++channel)
                    {
                        result.rgba8[targetOffset + channel] =
                            static_cast<std::uint8_t>(
                                (sums[channel] + sampleCount / 2u)
                                / sampleCount);
                    }
                }
            }

            return result
                ? std::optional<TextureThumbnail>{std::move(result)}
                : std::nullopt;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    ProjectTextureController::ProjectTextureController(
        std::string projectId,
        fs::path projectRoot) noexcept
        : project_root_(normalized_absolute(projectRoot)),
          assets_root_(project_root_.empty()
              ? fs::path{}
              : project_root_ / "Assets"),
          source_store_(projectId, project_root_),
          pipeline_(
              std::move(projectId),
              project_root_.generic_string())
    {
        if (valid())
            (void)refresh_catalog();
    }

    bool ProjectTextureController::valid() const noexcept
    {
        return !project_root_.empty()
            && !assets_root_.empty()
            && source_store_.valid()
            && pipeline_.valid();
    }

    std::string_view ProjectTextureController::project_id() const noexcept
    {
        return pipeline_.project_id();
    }

    const fs::path& ProjectTextureController::project_root() const noexcept
    {
        return project_root_;
    }

    std::span<const TextureCatalogEntry>
        ProjectTextureController::catalog() const noexcept
    {
        return catalog_;
    }

    std::string_view
        ProjectTextureController::selected_logical_path() const noexcept
    {
        return selected_logical_path_;
    }

    const TextureCatalogEntry*
        ProjectTextureController::selected() const noexcept
    {
        const auto found = std::find_if(
            catalog_.begin(),
            catalog_.end(),
            [&](const TextureCatalogEntry& entry)
            {
                return entry.logical_path == selected_logical_path_;
            });
        return found == catalog_.end() ? nullptr : &*found;
    }

    ControllerMetrics ProjectTextureController::metrics() const noexcept
    {
        ControllerMetrics result{};
        result.texture_count = static_cast<std::uint32_t>((std::min)(
            catalog_.size(),
            static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)())));
        result.has_selection = selected() != nullptr;
        const auto saturating_add = [](std::uint64_t left,
                                       std::uint64_t right) noexcept
        {
            const std::uint64_t maximum =
                (std::numeric_limits<std::uint64_t>::max)();
            return right > maximum - left ? maximum : left + right;
        };
        for (const TextureCatalogEntry& entry : catalog_)
        {
            result.source_bytes = saturating_add(
                result.source_bytes,
                entry.source_bytes);
            result.decoded_bytes = saturating_add(
                result.decoded_bytes,
                entry.decoded_bytes);
            result.maximum_width = (std::max)(
                result.maximum_width,
                entry.width);
            result.maximum_height = (std::max)(
                result.maximum_height,
                entry.height);
        }
        return result;
    }

    CatalogRefreshResult ProjectTextureController::refresh_catalog() noexcept
    {
        CatalogRefreshResult result{};
        if (!valid())
            return result;

        try
        {
            constexpr std::size_t kMaximumCatalogSources = 4'096u;
            const fs::path assets = normalized_absolute(assets_root_);
            if (assets.empty())
                return result;

            std::error_code error{};
            fs::create_directories(assets, error);
            if (error)
                return result;

            std::vector<fs::path> sources{};
            sources.reserve(64u);
            fs::recursive_directory_iterator iterator{
                assets,
                fs::directory_options::skip_permission_denied,
                error};
            const fs::recursive_directory_iterator end{};
            for (; !error && iterator != end; iterator.increment(error))
            {
                std::error_code entryError{};
                if (!iterator->is_regular_file(entryError)
                    || entryError
                    || !catalog_source_extension(iterator->path()))
                {
                    continue;
                }
                if (sources.size() >= kMaximumCatalogSources)
                {
                    ++result.skipped_sources;
                    continue;
                }
                sources.push_back(normalized_absolute(iterator->path()));
            }
            if (error)
                ++result.skipped_sources;
            std::sort(sources.begin(), sources.end());
            result.discovered_sources = static_cast<std::uint32_t>(
                sources.size());

            const std::vector<TextureCatalogEntry> before = catalog_;
            const std::string previousSelection = selected_logical_path_;
            std::vector<TextureCatalogEntry> refreshed{};
            refreshed.reserve(sources.size());

            for (const fs::path& source : sources)
            {
                const auto logicalPath = logical_source_path(source, assets);
                if (!logicalPath)
                {
                    ++result.skipped_sources;
                    continue;
                }

                const bool authored =
                    lowercase_extension(source) == ".epoch_texture";
                asset::texture::TextureCompileProfile profile =
                    imported_texture_profile();
                std::unique_ptr<authoring::texture::TextureDocument>
                    authoredDocument{};
                if (authored)
                {
                    auto loaded = source_store_.load(*logicalPath);
                    if (!loaded)
                    {
                        ++result.skipped_sources;
                        continue;
                    }
                    profile = authored_texture_profile(*loaded.document);
                    authoredDocument = std::move(loaded.document);
                }

                project_textures::TexturePipelineResult activated =
                    pipeline_.restore_latest(*logicalPath, profile);
                bool regenerated = false;
                if (!activated && authoredDocument)
                {
                    const asset::texture::CompiledTextureArtifact artifact =
                        authoredDocument->compile_artifact(profile);
                    if (artifact)
                    {
                        activated = pipeline_.publish(
                            *logicalPath,
                            artifact);
                        regenerated = static_cast<bool>(activated);
                    }
                }
                if (!activated)
                {
                    ++result.skipped_sources;
                    continue;
                }

                const std::array<canvas2d::LogicalTextureReference, 1>
                    logical{activated.logical};
                const project_textures::Canvas2DLeaseResult leased =
                    pipeline_.bind_canvas2d(logical);
                if (!leased
                    || leased.lease.bindings.textures.size() != 1u)
                {
                    ++result.skipped_sources;
                    continue;
                }
                const auto& view = leased.lease.bindings.textures.front();
                if (view.extent.empty())
                {
                    ++result.skipped_sources;
                    continue;
                }

                std::error_code sizeError{};
                const std::uintmax_t sourceBytes =
                    fs::file_size(source, sizeError);
                refreshed.push_back(TextureCatalogEntry{
                    .logical_path =
                        activated.locator.canonical_logical_path,
                    .source_path = source,
                    .artifact_key = activated.locator.artifact_key,
                    .logical = activated.logical,
                    .source_sequence =
                        activated.locator.source_revision.sequence,
                    .source_bytes = sizeError
                        ? 0u
                        : static_cast<std::uint64_t>(sourceBytes),
                    .decoded_bytes = leased.decoded_bytes,
                    .width = view.extent.width,
                    .height = view.extent.height
                });
                if (regenerated)
                    ++result.regenerated_artifacts;
                else
                    ++result.restored_artifacts;
            }

            std::sort(
                refreshed.begin(),
                refreshed.end(),
                [](const TextureCatalogEntry& left,
                   const TextureCatalogEntry& right)
                {
                    return left.logical_path < right.logical_path;
                });
            const auto sameCatalog = [&]() noexcept
            {
                if (before.size() != refreshed.size())
                    return false;
                for (std::size_t index = 0u;
                     index < before.size();
                     ++index)
                {
                    if (before[index].logical_path
                            != refreshed[index].logical_path
                        || before[index].artifact_key
                            != refreshed[index].artifact_key
                        || before[index].source_sequence
                            != refreshed[index].source_sequence)
                    {
                        return false;
                    }
                }
                return true;
            };

            const bool catalogUnchanged = sameCatalog();
            catalog_ = std::move(refreshed);
            const auto previous = std::find_if(
                catalog_.begin(),
                catalog_.end(),
                [&](const TextureCatalogEntry& entry)
                {
                    return entry.logical_path == previousSelection;
                });
            if (previous != catalog_.end())
                selected_logical_path_ = previous->logical_path;
            else if (!catalog_.empty())
                selected_logical_path_ = catalog_.front().logical_path;
            else
                selected_logical_path_.clear();

            result.code = catalogUnchanged
                ? ControllerCode::unchanged
                : ControllerCode::ready;
            return result;
        }
        catch (...)
        {
            result.code = ControllerCode::allocation_failure;
            return result;
        }
    }

    ControllerResult ProjectTextureController::import_source(
        const fs::path& sourcePath,
        const capability::SubsystemProfile& renderer,
        const Budgets& platformBudgets) noexcept
    {
        if (!valid())
            return reject(ControllerCode::invalid_controller);

        try
        {
            std::error_code error{};
            fs::create_directories(assets_root_, error);
            if (error)
                return reject(ControllerCode::invalid_controller);

            const fs::path source = normalized_absolute(sourcePath);
            const fs::path assets = normalized_absolute(assets_root_);
            const std::optional<std::string> logicalPath =
                logical_source_path(source, assets);
            if (!logicalPath)
                return reject(ControllerCode::source_outside_assets);
            if (!supported_source_extension(source))
                return reject(ControllerCode::unsupported_source);

            std::uint64_t sequence = 1u;
            if (const project_assets::AssetRecord* existing =
                    pipeline_.registry().find_by_path(*logicalPath))
            {
                if (existing->revision.sequence
                    == (std::numeric_limits<std::uint64_t>::max)())
                {
                    return reject(ControllerCode::import_rejected);
                }
                sequence = existing->revision.sequence + 1u;
            }

            asset::texture::TextureCompileProfile profile{};
            profile.format = asset::texture::ArtifactFormat::rgba8_unorm;
            profile.color_space = asset::texture::ColorSpace::linear;
            profile.mipmaps =
                asset::texture::MipmapPolicy::preserve_authored;
            const asset::texture::TextureImportResult imported =
                asset::texture::import_texture_file({
                    .source_path = source,
                    .source_sequence = sequence,
                    .profile = profile
                });
            if (!imported)
            {
                return reject(
                    ControllerCode::import_rejected,
                    imported.status);
            }

            const project_textures::TextureAdmissionDecision admission =
                project_textures::assess_texture_admission(
                    imported.artifact,
                    renderer,
                    {},
                    platformBudgets);
            if (!admission)
            {
                return reject(
                    ControllerCode::admission_rejected,
                    imported.status,
                    admission.reason);
            }

            const project_textures::TexturePipelineResult published =
                pipeline_.publish(*logicalPath, imported.artifact);
            if (!published)
            {
                return reject(
                    ControllerCode::publication_rejected,
                    imported.status,
                    admission.reason,
                    published.code);
            }

            TextureCatalogEntry entry{
                .logical_path =
                    published.locator.canonical_logical_path,
                .source_path = source,
                .artifact_key = imported.artifact.identity.key,
                .logical = published.logical,
                .source_sequence =
                    imported.artifact.identity.source_revision.sequence,
                .source_bytes = imported.source_bytes,
                .decoded_bytes = imported.decoded_bytes,
                .width = imported.width,
                .height = imported.height
            };
            const auto existing = std::find_if(
                catalog_.begin(),
                catalog_.end(),
                [&](const TextureCatalogEntry& candidate)
                {
                    return candidate.logical_path == entry.logical_path;
                });
            if (existing == catalog_.end())
                catalog_.push_back(entry);
            else
                *existing = entry;
            selected_logical_path_ = entry.logical_path;
            return {
                published.code == project_textures::PipelineCode::unchanged
                    ? ControllerCode::unchanged
                    : ControllerCode::ready,
                imported.status,
                admission.reason,
                published.code,
                std::move(entry)
            };
        }
        catch (...)
        {
            return reject(ControllerCode::allocation_failure);
        }
    }

    ControllerResult ProjectTextureController::create_editable(
        std::string_view logicalPath,
        authoring::texture::CanvasDescriptor descriptor,
        const capability::SubsystemProfile& renderer,
        const Budgets& platformBudgets) noexcept
    {
        if (!valid())
            return reject(ControllerCode::invalid_controller);
        if (editable_dirty_)
            return reject(ControllerCode::unsaved_changes);

        try
        {
            const auto canonical = project_assets::canonical_logical_path(
                logicalPath,
                source_store_.limits().registry);
            if (!canonical)
            {
                ControllerResult result =
                    reject(ControllerCode::source_rejected);
                result.source_code =
                    project_texture_sources::SourceCode::invalid_path;
                return result;
            }

            const std::uint64_t assetKey =
                project_assets::stable_asset_identity(
                    pipeline_.project_key(),
                    project_assets::AssetKind::texture,
                    *canonical);
            if (assetKey == 0u)
            {
                ControllerResult result =
                    reject(ControllerCode::source_rejected);
                result.source_code =
                    project_texture_sources::SourceCode::invalid_path;
                return result;
            }
            std::uint32_t documentIndex =
                static_cast<std::uint32_t>(assetKey);
            if (documentIndex
                == authoring::texture::DocumentHandle::invalid_index)
            {
                --documentIndex;
            }
            std::uint32_t generation =
                static_cast<std::uint32_t>(assetKey >> 32u);
            if (generation == 0u)
                generation = 1u;

            auto document =
                std::make_unique<authoring::texture::TextureDocument>(
                    authoring::texture::DocumentHandle{
                        documentIndex,
                        generation},
                    authoring::texture::BranchIdentity{
                        pipeline_.project_key(),
                        assetKey},
                    descriptor);
            if (!document->valid())
            {
                ControllerResult result =
                    reject(ControllerCode::edit_rejected);
                result.authoring_code =
                    authoring::texture::ResultCode::invalid_descriptor;
                return result;
            }

            const authoring::texture::MutationResult base =
                document->create_layer(
                    authoring::texture::LayerDescriptor{
                        .name = "Base Color"},
                    0u,
                    authoring::texture::TemporalPoint{
                        document->branch(),
                        1});
            if (!base)
            {
                ControllerResult result =
                    reject(ControllerCode::edit_rejected);
                result.authoring_code = base.code;
                return result;
            }

            editable_document_ = std::move(document);
            editable_logical_path_ = *canonical;
            editable_source_path_.clear();
            editable_selected_layer_ = base.layer;
            editable_dirty_ = true;
            ControllerResult result = publish_editable(
                renderer,
                platformBudgets,
                true);
            if (!result)
            {
                editable_document_.reset();
                editable_logical_path_.clear();
                editable_source_path_.clear();
                editable_selected_layer_ = {};
                editable_dirty_ = false;
            }
            return result;
        }
        catch (...)
        {
            return reject(ControllerCode::allocation_failure);
        }
    }

    ControllerResult ProjectTextureController::open_editable(
        std::string_view logicalPath,
        const capability::SubsystemProfile& renderer,
        const Budgets& platformBudgets) noexcept
    {
        if (!valid())
            return reject(ControllerCode::invalid_controller);
        if (editable_dirty_)
            return reject(ControllerCode::unsaved_changes);
        project_texture_sources::LoadedSource loaded =
            source_store_.load(logicalPath);
        if (!loaded)
        {
            ControllerResult result =
                reject(ControllerCode::source_rejected);
            result.source_code = loaded.code;
            return result;
        }

        editable_document_ = std::move(loaded.document);
        editable_logical_path_ =
            loaded.location.canonical_logical_path;
        editable_source_path_ = loaded.location.storage_path;
        editable_dirty_ = false;
        editable_selected_layer_ = {};
        refresh_editable_selection();
        return publish_editable(renderer, platformBudgets, false);
    }

    ControllerResult ProjectTextureController::save_editable(
        const capability::SubsystemProfile& renderer,
        const Budgets& platformBudgets) noexcept
    {
        return publish_editable(renderer, platformBudgets, true);
    }

    std::optional<EditableTextureState>
        ProjectTextureController::editable_state() const
    {
        if (!editable_document_ || !editable_document_->valid())
            return std::nullopt;
        EditableTextureState state{
            .logical_path = editable_logical_path_,
            .source_path = editable_source_path_,
            .revision = editable_document_->revision(),
            .canvas = editable_document_->descriptor(),
            .layers = editable_document_->layers(),
            .selected_layer = editable_selected_layer_,
            .metrics = editable_document_->metrics(),
            .dirty = editable_dirty_,
            .can_undo = editable_document_->can_undo(),
            .can_redo = editable_document_->can_redo()
        };
        return state ? std::optional<EditableTextureState>{
                std::move(state)}
            : std::nullopt;
    }

    std::optional<TexturePreview>
        ProjectTextureController::editable_preview() const noexcept
    {
        if (!editable_document_
            || !editable_document_->valid()
            || editable_logical_path_.empty())
        {
            return std::nullopt;
        }
        try
        {
            asset::texture::TextureCompileProfile profile{};
            const authoring::texture::CanvasDescriptor& canvas =
                editable_document_->descriptor();
            profile.format =
                canvas.format
                    == authoring::texture::PixelFormat::rgba8_srgb
                ? asset::texture::ArtifactFormat::rgba8_srgb
                : asset::texture::ArtifactFormat::rgba8_unorm;
            profile.color_space = canvas.color_space;
            profile.mipmaps =
                asset::texture::MipmapPolicy::preserve_authored;
            const asset::texture::CompiledTextureArtifact artifact =
                editable_document_->compile_artifact(profile);
            if (!artifact || artifact.mips.empty())
                return std::nullopt;
            const asset::texture::CompiledTextureMip& mip =
                artifact.mips.front();
            TexturePreview preview{
                .logical_path = editable_logical_path_,
                .artifact_key = artifact.identity.key,
                .width = mip.width,
                .height = mip.height
            };
            preview.rgba8.resize(mip.texels.size());
            std::transform(
                mip.texels.begin(),
                mip.texels.end(),
                preview.rgba8.begin(),
                [](std::byte value)
                {
                    return std::to_integer<std::uint8_t>(value);
                });
            return preview
                ? std::optional<TexturePreview>{std::move(preview)}
                : std::nullopt;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    TextureEditResult ProjectTextureController::select_editable_layer(
        authoring::texture::LayerHandle layer) noexcept
    {
        if (!editable_document_)
            return {};
        if (!editable_document_->layer(layer))
        {
            return {
                .code = ControllerCode::edit_rejected,
                .authoring_code =
                    authoring::texture::ResultCode::invalid_handle,
                .revision = editable_document_->revision(),
                .dirty = editable_dirty_
            };
        }
        editable_selected_layer_ = layer;
        return {
            .code = ControllerCode::unchanged,
            .authoring_code =
                authoring::texture::ResultCode::unchanged,
            .revision = editable_document_->revision(),
            .layer = layer,
            .dirty = editable_dirty_
        };
    }

    TextureEditResult ProjectTextureController::create_editable_layer(
        authoring::texture::LayerDescriptor descriptor) noexcept
    {
        if (!editable_document_)
            return {};
        const auto layers = editable_document_->layers();
        TextureEditResult result = edit_result(
            editable_document_->create_layer(
                std::move(descriptor),
                static_cast<std::uint32_t>(layers.size()),
                next_edit_time()));
        if (result && result.layer)
            editable_selected_layer_ = result.layer;
        return result;
    }

    TextureEditResult ProjectTextureController::create_editable_layer_mask(
        authoring::texture::LayerHandle owner,
        std::string name) noexcept
    {
        if (!editable_document_)
            return {};
        const auto ownerInfo = editable_document_->layer(owner);
        if (!ownerInfo
            || ownerInfo->descriptor.role
                != authoring::texture::LayerRole::content
            || ownerInfo->descriptor.mask.source
            || name.empty())
        {
            return {
                .code = ControllerCode::edit_rejected,
                .authoring_code =
                    authoring::texture::ResultCode::invalid_descriptor,
                .revision = editable_document_->revision(),
                .layer = owner,
                .dirty = editable_dirty_
            };
        }

        const auto layers = editable_document_->layers();
        TextureEditResult created = edit_result(
            editable_document_->create_layer(
                authoring::texture::LayerDescriptor{
                    .name = std::move(name),
                    .role = authoring::texture::LayerRole::mask
                },
                static_cast<std::uint32_t>(layers.size()),
                next_edit_time()));
        if (!created || !created.layer)
            return created;

        auto descriptor = ownerInfo->descriptor;
        descriptor.mask.source = created.layer;
        TextureEditResult attached = edit_result(
            editable_document_->set_layer_properties(
                owner,
                std::move(descriptor),
                next_edit_time()));
        if (!attached)
        {
            (void)edit_result(editable_document_->remove_layer(
                created.layer,
                next_edit_time()));
            editable_selected_layer_ = owner;
            return attached;
        }
        editable_selected_layer_ = created.layer;
        attached.layer = created.layer;
        return attached;
    }

    TextureEditResult ProjectTextureController::remove_editable_layer(
        authoring::texture::LayerHandle layer) noexcept
    {
        if (!editable_document_)
            return {};
        return edit_result(editable_document_->remove_layer(
            layer,
            next_edit_time()));
    }

    TextureEditResult ProjectTextureController::move_editable_layer(
        authoring::texture::LayerHandle layer,
        std::uint32_t insertionIndex) noexcept
    {
        if (!editable_document_)
            return {};
        return edit_result(editable_document_->move_layer(
            layer,
            insertionIndex,
            next_edit_time()));
    }

    TextureEditResult
        ProjectTextureController::set_editable_layer_properties(
            authoring::texture::LayerHandle layer,
            authoring::texture::LayerDescriptor descriptor) noexcept
    {
        if (!editable_document_)
            return {};
        return edit_result(
            editable_document_->set_layer_properties(
                layer,
                std::move(descriptor),
                next_edit_time()));
    }

    TextureEditResult ProjectTextureController::paint_editable(
        authoring::texture::StrokeDescriptor stroke) noexcept
    {
        if (!editable_document_)
            return {};
        stroke.target = editable_selected_layer_;
        return edit_result(editable_document_->apply_stroke(
            std::move(stroke),
            next_edit_time()));
    }

    TextureEditResult ProjectTextureController::undo_editable() noexcept
    {
        if (!editable_document_)
            return {};
        return edit_result(
            editable_document_->undo(next_edit_time()));
    }

    TextureEditResult ProjectTextureController::redo_editable() noexcept
    {
        if (!editable_document_)
            return {};
        return edit_result(
            editable_document_->redo(next_edit_time()));
    }

    ControllerResult ProjectTextureController::publish_editable(
        const capability::SubsystemProfile& renderer,
        const Budgets& platformBudgets,
        bool persistSource) noexcept
    {
        if (!valid())
            return reject(ControllerCode::invalid_controller);
        if (!editable_document_
            || !editable_document_->valid()
            || editable_logical_path_.empty())
        {
            return reject(ControllerCode::editable_not_open);
        }

        try
        {
            const auto serialized = editable_document_->serialize(
                source_store_.limits().maximum_source_bytes);
            if (!serialized)
            {
                ControllerResult result =
                    reject(ControllerCode::source_rejected);
                result.authoring_code = serialized.code;
                result.source_code =
                    project_texture_sources::SourceCode::malformed_source;
                return result;
            }

            project_texture_sources::SourceCode sourceCode =
                project_texture_sources::SourceCode::ready;
            if (persistSource)
            {
                const project_texture_sources::SourceLocation source =
                    source_store_.save(
                        editable_logical_path_,
                        *editable_document_);
                sourceCode = source.code;
                if (!source)
                {
                    ControllerResult result =
                        reject(ControllerCode::source_rejected);
                    result.source_code = source.code;
                    return result;
                }
                editable_source_path_ = source.storage_path;
            }

            asset::texture::TextureCompileProfile profile{};
            const auto& canvas = editable_document_->descriptor();
            profile.format =
                canvas.format
                    == authoring::texture::PixelFormat::rgba8_srgb
                ? asset::texture::ArtifactFormat::rgba8_srgb
                : asset::texture::ArtifactFormat::rgba8_unorm;
            profile.color_space = canvas.color_space;
            profile.mipmaps =
                asset::texture::MipmapPolicy::preserve_authored;
            const asset::texture::CompiledTextureArtifact artifact =
                editable_document_->compile_artifact(profile);
            if (!artifact)
            {
                ControllerResult result =
                    reject(ControllerCode::compilation_rejected);
                result.source_code = sourceCode;
                result.compilation_status = artifact.status;
                return result;
            }

            const project_textures::TextureAdmissionDecision admission =
                project_textures::assess_texture_admission(
                    artifact,
                    renderer,
                    {},
                    platformBudgets);
            if (!admission)
            {
                ControllerResult result = reject(
                    ControllerCode::admission_rejected,
                    asset::texture::TextureImportStatus::ready,
                    admission.reason);
                result.source_code = sourceCode;
                result.compilation_status = artifact.status;
                return result;
            }

            const project_textures::TexturePipelineResult published =
                pipeline_.publish(
                    editable_logical_path_,
                    artifact);
            if (!published)
            {
                ControllerResult result = reject(
                    ControllerCode::publication_rejected,
                    asset::texture::TextureImportStatus::ready,
                    admission.reason,
                    published.code);
                result.source_code = sourceCode;
                result.compilation_status = artifact.status;
                return result;
            }

            TextureCatalogEntry entry{
                .logical_path =
                    published.locator.canonical_logical_path,
                .source_path = editable_source_path_,
                .artifact_key = artifact.identity.key,
                .logical = published.logical,
                .source_sequence =
                    artifact.identity.source_revision.sequence,
                .source_bytes = serialized.bytes.size(),
                .decoded_bytes =
                    artifact.identity.estimated_artifact_bytes,
                .width = artifact.identity.width,
                .height = artifact.identity.height
            };
            const auto existing = std::find_if(
                catalog_.begin(),
                catalog_.end(),
                [&](const TextureCatalogEntry& candidate)
                {
                    return candidate.logical_path
                        == entry.logical_path;
                });
            if (existing == catalog_.end())
                catalog_.push_back(entry);
            else
                *existing = entry;
            selected_logical_path_ = entry.logical_path;
            if (persistSource)
                editable_dirty_ = false;

            ControllerResult result{
                published.code
                        == project_textures::PipelineCode::unchanged
                    ? ControllerCode::unchanged
                    : ControllerCode::ready,
                asset::texture::TextureImportStatus::ready,
                admission.reason,
                published.code,
                std::move(entry)
            };
            result.source_code = sourceCode;
            result.authoring_code =
                authoring::texture::ResultCode::success;
            result.compilation_status = artifact.status;
            return result;
        }
        catch (...)
        {
            return reject(ControllerCode::allocation_failure);
        }
    }

    TextureEditResult ProjectTextureController::edit_result(
        const authoring::texture::MutationResult& mutation) noexcept
    {
        if (!editable_document_)
            return {};
        if (mutation.code == authoring::texture::ResultCode::success)
            editable_dirty_ = true;
        refresh_editable_selection();
        return {
            .code = mutation
                ? (mutation.code
                        == authoring::texture::ResultCode::unchanged
                    ? ControllerCode::unchanged
                    : ControllerCode::ready)
                : ControllerCode::edit_rejected,
            .authoring_code = mutation.code,
            .revision = mutation.revision,
            .layer = mutation.layer,
            .affected_tiles = mutation.affected_tiles,
            .dirty = editable_dirty_
        };
    }

    void ProjectTextureController::refresh_editable_selection() noexcept
    {
        if (!editable_document_)
        {
            editable_selected_layer_ = {};
            return;
        }
        if (editable_document_->layer(editable_selected_layer_))
            return;
        try
        {
            const auto layers = editable_document_->layers();
            editable_selected_layer_ = layers.empty()
                ? authoring::texture::LayerHandle{}
                : layers.back().handle;
        }
        catch (...)
        {
            editable_selected_layer_ = {};
        }
    }

    authoring::texture::TemporalPoint
        ProjectTextureController::next_edit_time() const noexcept
    {
        if (!editable_document_)
            return {};
        const authoring::texture::TemporalPoint current =
            editable_document_->current_time();
        if (!current
            || current.tick
                == (std::numeric_limits<std::int64_t>::max)())
        {
            return {};
        }
        return {
            editable_document_->branch(),
            current.tick + 1
        };
    }
    ControllerResult ProjectTextureController::select(
        std::string_view logicalPath) noexcept
    {
        if (!valid())
            return reject(ControllerCode::invalid_controller);
        const auto found = std::find_if(
            catalog_.begin(),
            catalog_.end(),
            [&](const TextureCatalogEntry& entry)
            {
                return entry.logical_path == logicalPath;
            });
        if (found == catalog_.end())
            return reject(ControllerCode::selection_not_found);
        selected_logical_path_ = found->logical_path;
        return {
            ControllerCode::ready,
            asset::texture::TextureImportStatus::ready,
            project_textures::TextureAdmissionReason::none,
            project_textures::PipelineCode::ready,
            *found
        };
    }

    std::optional<TexturePreview> ProjectTextureController::preview(
        std::string_view logicalPath) noexcept
    {
        if (!valid() || logicalPath.empty())
            return std::nullopt;

        const auto found = std::find_if(
            catalog_.begin(),
            catalog_.end(),
            [&](const TextureCatalogEntry& entry)
            {
                return entry.logical_path == logicalPath;
            });
        if (found == catalog_.end())
            return std::nullopt;

        try
        {
            const std::array<canvas2d::LogicalTextureReference, 1>
                requested{found->logical};
            project_textures::Canvas2DLeaseResult leased =
                pipeline_.bind_canvas2d(requested);
            if (!leased
                || leased.lease.bindings.textures.size() != 1u)
            {
                return std::nullopt;
            }

            const auto& view = leased.lease.bindings.textures.front();
            if (view.extent.empty()
                || view.row_stride_pixels < view.extent.width)
            {
                return std::nullopt;
            }

            TexturePreview result{
                .logical_path = found->logical_path,
                .artifact_key = found->artifact_key,
                .width = view.extent.width,
                .height = view.extent.height
            };
            const std::uint64_t byteCount =
                static_cast<std::uint64_t>(result.width)
                * static_cast<std::uint64_t>(result.height)
                * 4u;
            if (byteCount > static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max()))
            {
                return std::nullopt;
            }
            result.rgba8.resize(static_cast<std::size_t>(byteCount));

            for (std::uint32_t y = 0u; y < result.height; ++y)
            {
                const std::size_t rowOffset =
                    static_cast<std::size_t>(y)
                    * view.row_stride_pixels;
                const std::size_t outputOffset =
                    static_cast<std::size_t>(y)
                    * result.width
                    * 4u;
                for (std::uint32_t x = 0u; x < result.width; ++x)
                {
                    const auto& pixel = view.pixels[rowOffset + x];
                    const std::size_t offset =
                        outputOffset + static_cast<std::size_t>(x) * 4u;
                    result.rgba8[offset + 0u] = pixel.r;
                    result.rgba8[offset + 1u] = pixel.g;
                    result.rgba8[offset + 2u] = pixel.b;
                    result.rgba8[offset + 3u] = pixel.a;
                }
            }
            return result ? std::optional<TexturePreview>{std::move(result)}
                          : std::nullopt;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<TexturePreview>
        ProjectTextureController::selected_preview() noexcept
    {
        const TextureCatalogEntry* entry = selected();
        return entry ? preview(entry->logical_path) : std::nullopt;
    }

    std::optional<scene::SceneTextureMaterialSnapshot>
        ProjectTextureController::selected_material() const noexcept
    {
        const TextureCatalogEntry* entry = selected();
        if (!entry)
            return std::nullopt;
        scene::SceneTextureMaterialSnapshot material{
            .logical_path = entry->logical_path,
            .artifact_key = entry->artifact_key.words,
            .stable_key = material_stable_key(entry->logical),
            .filter = scene::SceneTextureFilter::linear,
            .address_u = scene::SceneTextureAddress::clamp_to_edge,
            .address_v = scene::SceneTextureAddress::clamp_to_edge,
            .alpha = scene::SceneTextureAlphaMode::straight,
            .color_space = scene::SceneTextureColorSpace::linear,
            .alpha_cutoff = 0.5f
        };
        return material.valid()
            ? std::optional<scene::SceneTextureMaterialSnapshot>{
                std::move(material)}
            : std::nullopt;
    }

    SceneTextureLeaseResult
        ProjectTextureController::bind_scene_materials(
            std::span<const scene::SceneTextureMaterialSnapshot>
                materials) noexcept
    {
        SceneTextureLeaseResult result{};
        if (!valid())
            return result;
        if (materials.empty())
        {
            result.code = ControllerCode::ready;
            return result;
        }

        try
        {
            std::vector<canvas2d::LogicalTextureReference> logical{};
            logical.reserve(materials.size());
            result.materials.reserve(materials.size());
            for (const scene::SceneTextureMaterialSnapshot& material :
                 materials)
            {
                if (!material.valid())
                {
                    result.code = ControllerCode::exact_restore_rejected;
                    return result;
                }
                const auto duplicate = std::find_if(
                    result.materials.begin(),
                    result.materials.end(),
                    [&](const ResolvedTextureMaterial& candidate)
                    {
                        return same_material_identity(
                            candidate.material,
                            material);
                    });
                if (duplicate != result.materials.end())
                    continue;

                const project_textures::TexturePipelineResult restored =
                    pipeline_.restore_exact(
                        material.logical_path,
                        content_hash(material));
                result.pipeline_code = restored.code;
                result.resource_code = restored.resource_code;
                if (!restored)
                {
                    result.code =
                        ControllerCode::exact_restore_rejected;
                    return result;
                }
                result.materials.push_back({
                    .material = material,
                    .logical = restored.logical
                });
                logical.push_back(restored.logical);
            }

            project_textures::Canvas2DLeaseResult leased =
                pipeline_.bind_canvas2d(logical);
            result.pipeline_code = leased.code;
            result.resource_code = leased.resource_code;
            if (!leased)
            {
                result.code = ControllerCode::lease_rejected;
                result.materials.clear();
                return result;
            }
            result.lease = std::move(leased.lease);
            result.code = ControllerCode::ready;
            return result;
        }
        catch (...)
        {
            result = {};
            result.code = ControllerCode::allocation_failure;
            return result;
        }
    }

    const project_textures::ProjectTexturePipeline&
        ProjectTextureController::pipeline() const noexcept
    {
        return pipeline_;
    }

    ControllerResult ProjectTextureController::reject(
        ControllerCode code,
        asset::texture::TextureImportStatus importStatus,
        project_textures::TextureAdmissionReason admissionReason,
        project_textures::PipelineCode pipelineCode) const noexcept
    {
        return {
            code,
            importStatus,
            admissionReason,
            pipelineCode,
            std::nullopt
        };
    }

    ControllerContractFailure
        project_texture_controller_contract_failure() noexcept
    {
        ContractRoot root{};
        if (root.path.empty())
            return ControllerContractFailure::temporary_project;

        const fs::path source =
            root.path / "Assets" / "Textures" / "sample.ppm";
        if (!write_contract_ppm(
                source,
                {255u, 0u, 0u, 0u, 255u, 0u}))
        {
            return ControllerContractFailure::temporary_project;
        }

        const capability::SubsystemProfile renderer =
            contract_renderer();
        const Budgets budgets = renderer.capability.recommended_budgets;
        ProjectTextureController controller{
            "epoch.editor-project-textures.contract",
            root.path
        };

        const fs::path outside = root.path / "outside.ppm";
        if (!write_contract_ppm(
                outside,
                {0u, 0u, 0u, 0u, 0u, 0u})
            || controller.import_source(
                    outside,
                    renderer,
                    budgets).code
                != ControllerCode::source_outside_assets)
        {
            return ControllerContractFailure::outside_assets_rejection;
        }

        const ControllerResult first =
            controller.import_source(source, renderer, budgets);
        const auto firstMaterial = controller.selected_material();
        if (!first || !firstMaterial || !firstMaterial->valid())
            return ControllerContractFailure::import;
        const ControllerMetrics firstMetrics = controller.metrics();
        if (firstMetrics.texture_count != 1u
            || firstMetrics.source_bytes != first.entry->source_bytes
            || firstMetrics.decoded_bytes != first.entry->decoded_bytes
            || firstMetrics.maximum_width != first.entry->width
            || firstMetrics.maximum_height != first.entry->height
            || !firstMetrics.has_selection)
        {
            return ControllerContractFailure::metrics;
        }
        if (firstMaterial->logical_path
                != "Assets/Textures/sample.ppm"
            || firstMaterial->artifact_key
                != first.entry->artifact_key.words)
        {
            return ControllerContractFailure::material_identity;
        }

        const auto firstPreview =
            controller.preview(first.entry->logical_path);
        const auto thumbnail = firstPreview
            ? make_texture_thumbnail(
                *firstPreview,
                TextureThumbnailPolicy{
                    .maximum_width = 1u,
                    .maximum_height = 1u})
            : std::nullopt;
        if (!thumbnail
            || thumbnail->width != 1u
            || thumbnail->height != 1u
            || thumbnail->rgba8
                != std::vector<std::uint8_t>{
                    128u, 128u, 0u, 255u}
            || make_texture_thumbnail(
                *firstPreview,
                TextureThumbnailPolicy{
                    .maximum_width = 0u,
                    .maximum_height = 1u}))
        {
            return ControllerContractFailure::thumbnail_generation;
        }

        if (!write_contract_ppm(
                source,
                {0u, 0u, 255u, 255u, 255u, 0u}))
        {
            return ControllerContractFailure::temporary_project;
        }
        const ControllerResult second =
            controller.import_source(source, renderer, budgets);
        const auto secondMaterial = controller.selected_material();
        if (!second
            || !secondMaterial
            || second.entry->source_sequence
                <= first.entry->source_sequence
            || secondMaterial->artifact_key
                == firstMaterial->artifact_key)
        {
            return ControllerContractFailure::historical_revision;
        }
        const ControllerMetrics secondMetrics = controller.metrics();
        if (secondMetrics.texture_count != 1u
            || secondMetrics.source_bytes != second.entry->source_bytes
            || secondMetrics.decoded_bytes != second.entry->decoded_bytes
            || secondMetrics.maximum_width != second.entry->width
            || secondMetrics.maximum_height != second.entry->height
            || !secondMetrics.has_selection)
        {
            return ControllerContractFailure::metrics;
        }

        const std::array<scene::SceneTextureMaterialSnapshot, 2>
            revisions{*firstMaterial, *secondMaterial};
        SceneTextureLeaseResult lease =
            controller.bind_scene_materials(revisions);
        if (!lease
            || lease.materials.size() != 2u
            || lease.lease.bindings.textures.size() != 2u)
        {
            return ControllerContractFailure::lease;
        }

        ProjectTextureController restored{
            "epoch.editor-project-textures.contract",
            root.path
        };
        if (restored.catalog().size() != 1u
            || restored.selected() == nullptr
            || restored.selected()->artifact_key
                != second.entry->artifact_key
            || restored.selected()->source_sequence
                != second.entry->source_sequence)
        {
            return ControllerContractFailure::restart_catalog;
        }

        if (!write_contract_ppm(
                source,
                {24u, 48u, 72u, 96u, 120u, 144u}))
        {
            return ControllerContractFailure::temporary_project;
        }
        const ControllerResult third =
            restored.import_source(source, renderer, budgets);
        if (!third
            || third.entry->source_sequence
                <= second.entry->source_sequence
            || third.entry->artifact_key == second.entry->artifact_key)
        {
            return ControllerContractFailure::restart_reimport;
        }

        SceneTextureLeaseResult restoredLease =
            restored.bind_scene_materials(revisions);
        if (!restoredLease
            || restoredLease.materials.size() != 2u
            || restoredLease.lease.bindings.textures.size() != 2u)
        {
            return ControllerContractFailure::exact_restore;
        }

        scene::SceneTextureMaterialSnapshot missing = *firstMaterial;
        missing.artifact_key[0] ^= 0x1234u;
        if (restored.bind_scene_materials(
                std::span<const scene::SceneTextureMaterialSnapshot>{
                    &missing,
                    1u}).code
            != ControllerCode::exact_restore_rejected)
        {
            return ControllerContractFailure::stale_path;
        }

        authoring::texture::CanvasDescriptor canvas{};
        canvas.width = 8u;
        canvas.height = 8u;
        canvas.tile_extent = 4u;
        canvas.mip_count = 1u;
        canvas.format =
            authoring::texture::PixelFormat::rgba8_unorm;
        canvas.color_space = asset::texture::ColorSpace::linear;
        constexpr std::string_view authoredPath{
            "Assets/Textures/authored.epoch_texture"};
        const ControllerResult created = controller.create_editable(
            authoredPath,
            canvas,
            renderer,
            budgets);
        auto editable = controller.editable_state();
        if (!created
            || !editable
            || editable->dirty
            || editable->layers.size() != 1u
            || !fs::exists(editable->source_path))
        {
            return ControllerContractFailure::authoring_create;
        }

        const TextureEditResult highlight =
            controller.create_editable_layer({
                .name = "Highlight"
            });
        editable = controller.editable_state();
        if (!highlight
            || !editable
            || !editable->dirty
            || editable->layers.size() != 2u
            || editable->selected_layer != highlight.layer)
        {
            return ControllerContractFailure::authoring_edit;
        }
        authoring::texture::LayerDescriptor highlightProperties{};
        highlightProperties.name = "Highlight";
        highlightProperties.opacity = 48'000u;
        if (!controller.set_editable_layer_properties(
                highlight.layer,
                highlightProperties)
            || !controller.move_editable_layer(
                highlight.layer,
                0u))
        {
            return ControllerContractFailure::authoring_edit;
        }

        authoring::texture::StrokeDescriptor stroke{};
        stroke.color = {32u, 96u, 255u, 255u};
        stroke.radius_subpixels = 512u;
        stroke.samples.push_back({
            .x_subpixels = 3 * 256 + 128,
            .y_subpixels = 4 * 256 + 128
        });
        const TextureEditResult painted =
            controller.paint_editable(stroke);
        const auto paintedPreview = controller.editable_preview();
        constexpr std::size_t paintedAlpha =
            (4u * 8u + 3u) * 4u + 3u;
        if (!painted
            || !paintedPreview
            || paintedPreview->rgba8.size() <= paintedAlpha
            || paintedPreview->rgba8[paintedAlpha] == 0u)
        {
            return ControllerContractFailure::authoring_edit;
        }

        if (controller.open_editable(
                authoredPath,
                renderer,
                budgets).code != ControllerCode::unsaved_changes)
        {
            return ControllerContractFailure::authoring_unsaved_guard;
        }

        if (!controller.undo_editable())
            return ControllerContractFailure::authoring_undo_redo;
        const auto undonePreview = controller.editable_preview();
        if (!undonePreview
            || undonePreview->rgba8.size() <= paintedAlpha
            || undonePreview->rgba8[paintedAlpha] != 0u
            || !controller.redo_editable())
        {
            return ControllerContractFailure::authoring_undo_redo;
        }
        const auto redonePreview = controller.editable_preview();
        if (!redonePreview
            || redonePreview->rgba8.size() <= paintedAlpha
            || redonePreview->rgba8[paintedAlpha] == 0u)
        {
            return ControllerContractFailure::authoring_undo_redo;
        }

        const TextureEditResult mask = controller.create_editable_layer_mask(
            highlight.layer,
            "Highlight Mask");
        editable = controller.editable_state();
        if (!mask
            || !editable
            || editable->selected_layer != mask.layer
            || editable->layers.size() != 3u)
        {
            return ControllerContractFailure::authoring_layer_mask;
        }
        const auto contentLayer = std::ranges::find(
            editable->layers,
            highlight.layer,
            &authoring::texture::LayerInfo::handle);
        const auto maskLayer = std::ranges::find(
            editable->layers,
            mask.layer,
            &authoring::texture::LayerInfo::handle);
        if (contentLayer == editable->layers.end()
            || maskLayer == editable->layers.end()
            || contentLayer->descriptor.mask.source != mask.layer
            || maskLayer->descriptor.role
                != authoring::texture::LayerRole::mask)
        {
            return ControllerContractFailure::authoring_layer_mask;
        }

        authoring::texture::StrokeDescriptor maskStroke = stroke;
        maskStroke.target = {};
        maskStroke.color = {0u, 0u, 0u, 255u};
        maskStroke.channel_mask = 0x08u;
        if (!controller.paint_editable(maskStroke))
        {
            return ControllerContractFailure::authoring_layer_mask;
        }
        const auto maskedPreview = controller.editable_preview();
        if (!maskedPreview
            || maskedPreview->rgba8.size() <= paintedAlpha
            || maskedPreview->rgba8[paintedAlpha] != 0u)
        {
            return ControllerContractFailure::authoring_layer_mask;
        }

        const ControllerResult saved =
            controller.save_editable(renderer, budgets);
        editable = controller.editable_state();
        if (!saved
            || !editable
            || editable->dirty
            || editable->revision.sequence != saved.entry->source_sequence
            || saved.entry->logical_path != authoredPath)
        {
            return ControllerContractFailure::authoring_save_reopen;
        }
        const asset::texture::DocumentRevision savedRevision =
            editable->revision;
        const auto savedPreview = controller.editable_preview();

        std::error_code libraryError{};
        fs::remove_all(
            root.path / "Library" / "Textures",
            libraryError);
        if (libraryError)
        {
            return
                ControllerContractFailure::authoring_cache_regeneration;
        }

        ProjectTextureController reopened{
            "epoch.editor-project-textures.contract",
            root.path
        };
        const ControllerResult reopenedResult =
            reopened.open_editable(
                authoredPath,
                renderer,
                budgets);
        const auto reopenedState = reopened.editable_state();
        const auto reopenedPreview = reopened.editable_preview();
        if (!reopenedResult
            || !reopenedState
            || reopenedState->dirty
            || reopenedState->revision != savedRevision
            || reopenedState->layers.size() != 3u
            || std::ranges::find(
                    reopenedState->layers,
                    mask.layer,
                    &authoring::texture::LayerInfo::handle)
                == reopenedState->layers.end()
            || std::ranges::find(
                    reopenedState->layers,
                    highlight.layer,
                    &authoring::texture::LayerInfo::handle)
                == reopenedState->layers.end()
            || std::ranges::find(
                    reopenedState->layers,
                    highlight.layer,
                    &authoring::texture::LayerInfo::handle)
                    ->descriptor.mask.source != mask.layer
            || !savedPreview
            || !reopenedPreview
            || reopenedPreview->artifact_key != savedPreview->artifact_key
            || reopenedPreview->rgba8 != savedPreview->rgba8
            || reopened.catalog().size() != 1u)
        {
            return
                ControllerContractFailure::authoring_cache_regeneration;
        }
        return ControllerContractFailure::none;
    }
}
