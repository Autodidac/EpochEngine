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
import capability.profile;
import platform.budgets;
import project.asset_registry;
import project.texture_admission;
import project.texture_pipeline;
import project.texture_resources;
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

        [[nodiscard]] bool supported_source_extension(
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
            return extension == ".ppm"
                || extension == ".bmp"
                || extension == ".tga";
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

    ProjectTextureController::ProjectTextureController(
        std::string projectId,
        fs::path projectRoot) noexcept
        : project_root_(normalized_absolute(projectRoot)),
          assets_root_(project_root_.empty()
              ? fs::path{}
              : project_root_ / "Assets"),
          pipeline_(
              std::move(projectId),
              project_root_.generic_string())
    {
    }

    bool ProjectTextureController::valid() const noexcept
    {
        return !project_root_.empty()
            && !assets_root_.empty()
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
        return ControllerContractFailure::none;
    }
}
