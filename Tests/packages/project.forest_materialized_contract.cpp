// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

import core.sha256;
import forest.factory;
import project.asset_registry;
import project.forest_library;

namespace
{
    namespace fs = std::filesystem;
    namespace assets = epochengine::project_assets;
    std::vector<std::uint8_t> read(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
    void write(const fs::path& path, std::span<const std::uint8_t> bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    void note(const fs::path& path, std::string text)
    {
        write(path, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
    }
    assets::AssetContentHash key(std::span<const std::uint8_t> body)
    {
        const auto digest = epochengine::core::sha256::hash(body);
        assets::AssetContentHash result;
        for (std::size_t word = 0u; word != result.words.size(); ++word)
            for (std::size_t byte = 0u; byte != 8u; ++byte)
                result.words[word] = (result.words[word] << 8u) | digest.bytes[word * 8u + byte];
        return result;
    }
    std::string hex(assets::AssetContentHash key)
    {
        constexpr char digits[]{"0123456789abcdef"};
        std::string output;
        for (const auto word : key.words)
            for (int shift = 60; shift >= 0; shift -= 4)
                output += digits[(word >> shift) & 15u];
        return output;
    }
    void integer(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value)
    {
        for (unsigned index = 0u; index != 4u; ++index)
            bytes.at(at + index) = static_cast<std::uint8_t>(value >> (8u * index));
    }
    std::uint32_t integer(const std::vector<std::uint8_t>& bytes, std::size_t at)
    {
        std::uint32_t value{};
        for (unsigned index = 0u; index != 4u; ++index)
            value |= std::uint32_t{bytes.at(at + index)} << (8u * index);
        return value;
    }

    epochengine::project_forests::ForestLibraryCode selected_status(
        epochengine::project_forests::ForestAssetLibrary& owner, std::string_view path)
    {
        // Keep the large returned tree temporary in this call's bounded frame,
        // rather than reserving one copy for every negative case in main.
        return owner.load_selected_materialized(path).code;
    }
}

#define CHECK(condition) do { if (!(condition)) { note(base / (action.empty() ? "failure.txt" : std::string(action.substr(2u)) + "-failure.txt"), std::to_string(__LINE__)); return __LINE__; } } while (false)

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4) return 1;
    namespace forest = epochengine::forest;
    namespace library = epochengine::project_forests;
    const auto fixtures = fs::absolute(argv[1]);
    const auto base = fs::absolute(argv[2]);
    const std::string_view action = argc == 4 ? argv[3] : "";
    const bool nativeLocal = action == "--local-selected-producer" || action == "--local-selected-reopen";
    if (!action.empty())
    {
        try
        {
        constexpr std::string_view logical{"Assets/Forest/selected.epoch_forest"};
        const auto manifest = base / "project.epoch.json";
        library::ForestAssetLibrary owner{"forest-v1-golden", base};
        if (action == "--selected-producer" || action == "--local-selected-producer")
        {
            CHECK(!fs::exists(base));
            fs::create_directories(base);
            note(manifest, "{\"id\":\"forest-v1-golden\",\"project_format\":\"epoch-project-v1\"}\n");
            write(base / "Assets/Forest/default.epoch_forest", read(fixtures / "default.epoch_forest"));
            const auto source = owner.load_source("Assets/Forest/default.epoch_forest");
            CHECK(source);
            const auto legacy = owner.publish("Assets/Forest/default.epoch_forest", source.document);
            CHECK(legacy);
            CHECK(read(legacy.source.storage_path) == read(fixtures / "default.epoch_forest"));
            if (nativeLocal)
            {
                const auto repeated = owner.publish("Assets/Forest/default.epoch_forest", source.document);
                CHECK(repeated && repeated.code == library::ForestLibraryCode::unchanged
                    && repeated.artifact.artifact_key.words == legacy.artifact.artifact_key.words);
                CHECK(owner.load_exact("Assets/Forest/default.epoch_forest", legacy.artifact.artifact_key));
                write(base / "native-legacy-artifact.bin", read(legacy.artifact.storage_path));
                note(base / "native-legacy-key.txt", hex(legacy.artifact.artifact_key) + "\n");
                note(base / "windows-legacy-interoperability.txt",
                    read(legacy.artifact.storage_path) == read(fixtures / "default.epoch_forest_artifact")
                        ? "MATCH: native legacy artifact equals Windows fixture.\n"
                        : "FAIL: native legacy artifact differs from immutable Windows fixture; local selection proof does not waive this gate.\n");
            }
            else CHECK(read(legacy.artifact.storage_path) == read(fixtures / "default.epoch_forest_artifact"));
            CHECK(selected_status(owner, "Assets/Forest/default.epoch_forest")
                == library::ForestLibraryCode::not_found);
            const auto publication = owner.publish_materialized(logical, source.document);
            CHECK(publication);
            CHECK(selected_status(owner, logical) == library::ForestLibraryCode::not_found);
            // Publication alone never selects; only a verified missing leaf is absent.
            CHECK(owner.select_materialized(logical, publication.artifact) == library::ForestLibraryCode::ready);
            CHECK(owner.select_materialized(logical, publication.artifact) == library::ForestLibraryCode::unchanged);
            CHECK(selected_status(owner, logical) == library::ForestLibraryCode::ready);
            CHECK(owner.load_latest(logical).code == library::ForestLibraryCode::verified_key_required);
            note(base / "selected-producer.txt", "PASS: actual materialized publication and explicit replay-verified selection; v1 bytes unchanged.\n");
            return 0;
        }
        if (action != "--selected-reopen" && action != "--local-selected-reopen") return 1;
        CHECK(fs::is_regular_file(base / "selected-producer.txt"));
        if (nativeLocal)
        {
            const auto legacy = owner.load_latest("Assets/Forest/default.epoch_forest");
            CHECK(legacy && read(legacy.locator.storage_path) == read(base / "native-legacy-artifact.bin"));
        }
        const auto loaded = owner.load_selected_materialized(logical);
        CHECK(loaded); // A different OS process has no producer's in-memory locator.
        const auto reference = loaded.locator.storage_path.parent_path() / "selected.epoch_forest_reference";
        const auto originalReference = read(reference);
        const auto originalArtifact = read(loaded.locator.storage_path);
        const auto originalSource = read(base / logical);
        const auto originalManifest = read(manifest);
        CHECK(fs::remove(reference));
        CHECK(selected_status(owner, logical) == library::ForestLibraryCode::not_found);
        std::error_code missingError;
        const auto missingTarget = base / "missing-reference-target";
        fs::create_symlink(missingTarget, reference, missingError);
        CHECK(!missingError && selected_status(owner, logical) != library::ForestLibraryCode::not_found
            && selected_status(owner, logical) != library::ForestLibraryCode::ready);
        CHECK(fs::remove(reference));
        write(reference, originalReference);
        CHECK(read(base / "Assets/Forest/default.epoch_forest") == read(fixtures / "default.epoch_forest"));
        CHECK(owner.load_latest(logical).code == library::ForestLibraryCode::verified_key_required);
        const auto rehash = [](std::vector<std::uint8_t>& bytes) {
            const auto digest = epochengine::core::sha256::hash(std::span{bytes}.first(bytes.size() - 32u));
            std::copy(digest.bytes.begin(), digest.bytes.end(), bytes.end() - 32);
        };
        auto changedReference = originalReference;
        changedReference.at(changedReference.size() - 33u) ^= 1u;
        write(reference, changedReference);
        CHECK(selected_status(owner, logical) != library::ForestLibraryCode::ready);
        rehash(changedReference);
        write(reference, changedReference);
        CHECK(selected_status(owner, logical) != library::ForestLibraryCode::ready);
        write(reference, originalReference);
        auto changedOriginal = originalArtifact;
        changedOriginal.at(changedOriginal.size() / 2u) ^= 1u;
        write(loaded.locator.storage_path, changedOriginal);
        CHECK(selected_status(owner, logical) != library::ForestLibraryCode::ready);
        write(loaded.locator.storage_path, originalArtifact);

        // Alter a finite graph coordinate and recompute BOTH artifact and hint
        // hashes. Source replay still determines the original exact envelope.
        auto changedArtifact = originalArtifact;
        const auto sourceSize = integer(changedArtifact, 10u);
        const auto producerSizeAt = 14u + sourceSize;
        const auto graphSizeAt = producerSizeAt + 4u + integer(changedArtifact, producerSizeAt);
        const auto graphBegin = graphSizeAt + 4u;
        changedArtifact.at(graphBegin + 158u + 9u + 4u + 21u) ^= 1u;
        rehash(changedArtifact);
        const auto changedKey = key(std::span{changedArtifact}.first(changedArtifact.size() - 32u));
        const auto changedPath = loaded.locator.storage_path.parent_path() / (hex(changedKey) + ".epoch_forest_artifact");
        write(changedPath, changedArtifact);
        changedReference = originalReference;
        const auto keyAt = changedReference.size() - 32u - 8u - 32u;
        for (std::size_t word = 0u; word < 4u; ++word)
            for (std::size_t byte = 0u; byte < 8u; ++byte)
                changedReference.at(keyAt + word * 8u + byte) = static_cast<std::uint8_t>(changedKey.words[word] >> (byte * 8u));
        rehash(changedReference);
        write(reference, changedReference);
        CHECK(selected_status(owner, logical) != library::ForestLibraryCode::ready);
        write(reference, originalReference);
        CHECK(selected_status(owner, logical) == library::ForestLibraryCode::ready);

        write(reference, std::vector<std::uint8_t>(8193u, 0u));
        CHECK(selected_status(owner, logical) == library::ForestLibraryCode::artifact_too_large);
        write(reference, originalReference);
        note(manifest, "{\"id\":\"forest-v1-golden\",\"different_project_generation\":true}\n");
        CHECK(selected_status(owner, logical) != library::ForestLibraryCode::ready);
#if defined(_WIN32)
        // Windows refuses replacing an ordinary file held without delete sharing.
        // This reaches the actual atomic-commit failure after verified staging.
        std::ifstream held{reference, std::ios::binary};
        CHECK(held);
        CHECK(owner.select_materialized(logical, loaded.locator) == library::ForestLibraryCode::atomic_replace_failure);
        CHECK(read(reference) == originalReference && read(base / logical) == originalSource
            && read(loaded.locator.storage_path) == originalArtifact);
        held.close();
        note(base / "selected-atomic-failure-status.txt", "PASS: Windows locked-file replacement failure preserves prior reference/source/artifact.\n");
#else
        note(base / "selected-atomic-failure-status.txt", "SKIPPED: Windows locked-file atomic replacement failure is not portable; Linux rename permits open files.\n");
#endif
        write(manifest, originalManifest);
        CHECK(selected_status(owner, logical) == library::ForestLibraryCode::ready);
        const auto foreign = base / "foreign";
        fs::create_directories(foreign);
        fs::copy(base / "Assets", foreign / "Assets", fs::copy_options::recursive);
        fs::copy(base / "Library", foreign / "Library", fs::copy_options::recursive);
        write(foreign / "project.epoch.json", originalManifest);
        library::ForestAssetLibrary foreignOwner{"forest-v1-golden", foreign};
        CHECK(selected_status(foreignOwner, logical) != library::ForestLibraryCode::ready);
        CHECK(foreignOwner.select_materialized(logical, loaded.locator) != library::ForestLibraryCode::ready);

        const auto savedSource = base / "saved-source";
        fs::rename(base / logical, savedSource);
        std::error_code error;
        fs::create_symlink(savedSource, base / logical, error);
        CHECK(!error && selected_status(owner, logical) != library::ForestLibraryCode::ready);
        CHECK(fs::remove(base / logical));
        fs::rename(savedSource, base / logical);
        const auto extraLink = base / "artifact-hardlink";
        fs::create_hard_link(loaded.locator.storage_path, extraLink, error);
        if (error) note(base / "hardlink-error.txt", error.message());
        CHECK(!error && selected_status(owner, logical) != library::ForestLibraryCode::ready);
        CHECK(fs::remove(extraLink));
        const auto savedReference = base / "saved-reference";
        fs::rename(reference, savedReference);
        fs::create_symlink(savedReference, reference, error);
        CHECK(!error && selected_status(owner, logical) != library::ForestLibraryCode::ready);
        CHECK(owner.select_materialized(logical, loaded.locator) == library::ForestLibraryCode::invalid_path);
        CHECK(fs::remove(reference));
        fs::rename(savedReference, reference);
        const auto alias = base.parent_path() / (base.filename().string() + "-alias");
        fs::create_directory_symlink(base, alias, error);
        CHECK(!error);
        library::ForestAssetLibrary aliased{"forest-v1-golden", alias};
        CHECK(selected_status(aliased, logical) != library::ForestLibraryCode::ready);
        CHECK(fs::remove(alias));
        CHECK(selected_status(owner, logical) == library::ForestLibraryCode::ready);
        const auto directory = reference.parent_path();
        const auto savedDirectory = base / "saved-library-artifacts";
        fs::rename(directory, savedDirectory);
        fs::create_directory_symlink(savedDirectory, directory, error);
        CHECK(!error && selected_status(owner, logical) != library::ForestLibraryCode::not_found
            && selected_status(owner, logical) != library::ForestLibraryCode::ready);
        CHECK(fs::remove(directory));
        note(directory, "Not a directory.\n");
        CHECK(selected_status(owner, logical) != library::ForestLibraryCode::not_found
            && selected_status(owner, logical) != library::ForestLibraryCode::ready);
        CHECK(fs::remove(directory));
        fs::rename(savedDirectory, directory);
        CHECK(selected_status(owner, logical) == library::ForestLibraryCode::ready);

        auto source = owner.load_source(logical);
        CHECK(source && forest::apply_forest_profile_edit(source.document,
            forest::ForestProfileProperty::TargetHeightMeters, 9.25f));
        const auto newer = owner.publish_materialized(logical, source.document);
        CHECK(newer && newer.artifact.source_revision != loaded.locator.source_revision);
        CHECK(selected_status(owner, logical) != library::ForestLibraryCode::ready);
        CHECK(owner.select_materialized(logical, loaded.locator) == library::ForestLibraryCode::verified_key_required);
        CHECK(read(reference) == originalReference);
        write(base / logical, originalSource);
        CHECK(selected_status(owner, logical) == library::ForestLibraryCode::ready);
        CHECK(read(reference) == originalReference && read(loaded.locator.storage_path) == originalArtifact);
        for (const auto& entry : fs::directory_iterator(reference.parent_path()))
            CHECK(entry.path().filename().string().find(".pending.") == std::string::npos);
        note(base / "selected-reopen.txt", "PASS: separate-process source-replayed reopen; rehashed payload/hint, stale source, manifest/foreign root, links/nonordinary parents and limits rejected; verified missing reference leaf distinguished. See separate atomic-failure status.\n");
        return 0;
        }
        catch (...) { note(base / "selected-exception.txt", "Selected Forest contract exception.\n"); return 255; }
    }
    if (fs::exists(base)) return 2;
    fs::create_directories(base);
    try
    {
        constexpr std::string_view oldPath{"Assets/Forest/default.epoch_forest"};
        constexpr std::string_view newPath{"Assets/Forest/materialized.epoch_forest"};
        library::ForestAssetLibrary owner{"forest-v1-golden", base};
        const auto goldenSource = read(fixtures / "default.epoch_forest");
        write(base / oldPath, goldenSource);
        const auto historical = owner.load_source(oldPath);
        CHECK(historical);
        // Legacy edits have no temporal snapshot: never replace their time with zero.
        auto legacyUndo = historical.document;
        const auto legacyTime = legacyUndo.profile.temporal.timeSeconds;
        CHECK(forest::undo_forest_profile_edit(legacyUndo));
        CHECK(legacyUndo.profile.temporal.timeSeconds == legacyTime);
        for (const bool legacyProfile : {false, true})
        {
            auto duration = legacyProfile ? historical.document : forest::make_default_plant_lab_document();
            CHECK(forest::apply_forest_profile_edit(duration, forest::ForestProfileProperty::GrowthTimeSeconds, 20.0f));
            const auto original = duration.contentHash;
            CHECK(forest::apply_forest_profile_edit(duration, forest::ForestProfileProperty::GrowthDurationSeconds, 5.0f));
            const auto shrunk = duration.contentHash;
            CHECK(duration.profile.temporal.timeSeconds == 5.0f);
            CHECK(forest::undo_forest_profile_edit(duration));
            CHECK(duration.contentHash == original && duration.profile.temporal.timeSeconds == 20.0f);
            CHECK(forest::redo_forest_profile_edit(duration) && duration.contentHash == shrunk);
            const auto path = legacyProfile ? "Assets/Forest/duration-legacy.epoch_forest" : "Assets/Forest/duration.epoch_forest";
            CHECK(owner.publish(path, duration));
            auto reopenedDuration = owner.load_source(path);
            CHECK(reopenedDuration);
            CHECK(forest::undo_forest_profile_edit(reopenedDuration.document));
            CHECK(reopenedDuration.document.contentHash == original);
            CHECK(forest::redo_forest_profile_edit(reopenedDuration.document));
            CHECK(reopenedDuration.document.contentHash == shrunk);
            auto invalid = duration;
            invalid.journal.back().beforeDurationTimeSeconds = std::numeric_limits<float>::infinity();
            CHECK(owner.publish("Assets/Forest/invalid-duration.epoch_forest", invalid).code == library::ForestLibraryCode::invalid_source);
            invalid = duration;
            invalid.journal.back().afterDurationTimeSeconds = 6.0f;
            CHECK(owner.publish("Assets/Forest/invalid-duration.epoch_forest", invalid).code == library::ForestLibraryCode::invalid_source);
        }

        // Length changes the crown without changing the existing trunk. The
        // first edit captures the old scale; loading old source does not.
        auto plant = forest::make_default_plant_lab_document();
        const auto canopy = forest::build_morphology_graph(plant.profile);
        CHECK(canopy.terminals.size() > canopy.segments.size());
        CHECK(canopy.terminals.size() <= forest::kForestPreviewMaxLeaves);
        CHECK(canopy.content_hash == forest::build_morphology_graph(plant.profile).content_hash);
        CHECK(canopy.terminals[0].position.x != canopy.terminals[1].position.x
            || canopy.terminals[0].position.y != canopy.terminals[1].position.y
            || canopy.terminals[0].position.z != canopy.terminals[1].position.z);
        // The saved recipe regenerates the same rolled leaves after reopening.
        constexpr std::string_view rotatedPath{"Assets/Forest/rotated.epoch_forest"};
        const auto rolled = owner.publish(rotatedPath, plant);
        CHECK(rolled);
        library::ForestAssetLibrary cold{"forest-v1-golden", base};
        const auto savedRoll = cold.load_source(rotatedPath);
        CHECK(savedRoll);
        const auto regenerated = forest::build_morphology_graph(savedRoll.document.profile);
        CHECK(regenerated.content_hash == canopy.content_hash);
        const auto reopenedRoll = cold.load_exact(rotatedPath, rolled.artifact.artifact_key);
        CHECK(reopenedRoll && reopenedRoll.artifact.graph.content_hash == canopy.content_hash);
        for (std::size_t index = 0u; index != canopy.terminals.size(); ++index)
        {
            const auto& expected = canopy.terminals[index];
            const auto& actual = reopenedRoll.artifact.graph.terminals[index];
            CHECK(actual.outward.x == expected.outward.x && actual.outward.y == expected.outward.y
                && actual.outward.z == expected.outward.z);
            CHECK(actual.position.x == expected.position.x && actual.position.y == expected.position.y
                && actual.position.z == expected.position.z);
        }
        // A sealed descriptor from the old canopy compiler is rebuildable
        // source, not corrupt data and never current geometry.
        auto previousCompiler = read(rolled.artifact.storage_path);
        CHECK(previousCompiler.at(8u) == 3u && integer(previousCompiler, 10u) == 2u);
        integer(previousCompiler, 10u, 1u);
        const auto previousDigest = epochengine::core::sha256::hash(
            std::span{previousCompiler}.first(previousCompiler.size() - 32u));
        std::copy(previousDigest.bytes.begin(), previousDigest.bytes.end(), previousCompiler.end() - 32);
        const auto previousKey = key(std::span{previousCompiler}.first(previousCompiler.size() - 32u));
        const auto previousPath = rolled.artifact.storage_path.parent_path()
            / (hex(previousKey) + ".epoch_forest_artifact");
        write(previousPath, previousCompiler);
        CHECK(cold.load_exact(rotatedPath, previousKey).code == library::ForestLibraryCode::rebuild_required);
        CHECK(read(rolled.artifact.storage_path) != previousCompiler);
        CHECK(cold.load_source(rotatedPath).document.contentHash == plant.contentHash);
        CHECK(cold.publish(rotatedPath, savedRoll.document).code == library::ForestLibraryCode::unchanged);
        auto foliageEdit = plant;
        CHECK(forest::apply_forest_profile_edit(foliageEdit,
            forest::ForestProfileProperty::LeafSitesPerBranch, 9.0f));
        CHECK(forest::apply_forest_profile_edit(foliageEdit,
            forest::ForestProfileProperty::LeafSizeMeters, 0.32f));
        CHECK(forest::apply_forest_profile_edit(foliageEdit,
            forest::ForestProfileProperty::PreviewMode, 0.0f));
        const auto planar = forest::build_morphology_graph(foliageEdit.profile);
        // Cards lie in the XY plane, so their surface normal faces Z.
        for (const auto& leaf : planar.terminals)
            CHECK(leaf.position.z == 0.0f && leaf.outward.x == 0.0f
                && leaf.outward.y == 0.0f && leaf.outward.z == 1.0f);
        CHECK(owner.publish("Assets/Forest/canopy.epoch_forest", foliageEdit));
        auto canopyReopen = owner.load_source("Assets/Forest/canopy.epoch_forest");
        CHECK(canopyReopen && canopyReopen.document.profile.leafSitesPerBranch == 9u);
        CHECK(canopyReopen.document.profile.leafSizeMeters == 0.32f);
        CHECK(canopyReopen.document.profile.previewMode == forest::ForestPreviewMode::Mode2D);
        for (unsigned i=0; i<3; ++i) CHECK(forest::undo_forest_profile_edit(canopyReopen.document));
        CHECK(canopyReopen.document.contentHash == plant.contentHash);
        for (unsigned i=0; i<3; ++i) CHECK(forest::redo_forest_profile_edit(canopyReopen.document));
        CHECK(canopyReopen.document.contentHash == foliageEdit.contentHash);
        const auto initial = plant;
        const auto initialRecipe = forest::morphology_recipe(plant.profile);
        CHECK(plant.profile.branch.lengthReferenceMeters == 0.0f);
        CHECK(!forest::apply_forest_profile_edit(plant,
            forest::ForestProfileProperty::BranchLengthMeters, 0.92f));
        CHECK(plant.profile.branch.lengthReferenceMeters == 0.0f);
        CHECK(!forest::apply_forest_profile_edit(plant,
            forest::ForestProfileProperty::BranchLengthMeters,
            std::numeric_limits<float>::quiet_NaN()));
        for (unsigned press = 0; press != 8; ++press)
        {
            const auto previous = forest::morphology_recipe(plant.profile);
            CHECK(forest::apply_forest_profile_edit(plant,
                forest::ForestProfileProperty::BranchLengthMeters,
                plant.profile.branch.branchLengthMeters - 0.1f));
            const auto current = forest::morphology_recipe(plant.profile);
            CHECK(current.growth.root_length_meters == initialRecipe.growth.root_length_meters);
            CHECK(current.growth.segment_length_meters < previous.growth.segment_length_meters);
            CHECK(plant.profile.config.targetHeightMeters == initial.profile.config.targetHeightMeters);
        }
        CHECK(std::abs(plant.profile.branch.branchLengthMeters - 0.12f) < 0.000001f);
        const auto shortened = forest::compile_forest_asset(plant);
        const auto original = forest::compile_forest_asset(initial);
        CHECK(shortened.valid && original.valid && !shortened.sample.segments.empty());
        CHECK(shortened.sample.segments.front().end.y == original.sample.segments.front().end.y);
        CHECK(owner.publish("Assets/Forest/independent.epoch_forest", plant));
        CHECK(read(base / "Assets/Forest/independent.epoch_forest").at(8u) == 3u);
        auto reopened = owner.load_source("Assets/Forest/independent.epoch_forest");
        CHECK(reopened && reopened.document.contentHash == plant.contentHash);
        CHECK(forest::morphology_recipe(reopened.document.profile).growth.root_length_meters
            == initialRecipe.growth.root_length_meters);
        for (unsigned press = 0; press != 8; ++press)
            CHECK(forest::undo_forest_profile_edit(reopened.document));
        CHECK(reopened.document.profile.branch.lengthReferenceMeters == 0.0f);
        CHECK(reopened.document.contentHash == initial.contentHash);
        for (unsigned press = 0; press != 8; ++press)
            CHECK(forest::redo_forest_profile_edit(reopened.document));
        CHECK(reopened.document.contentHash == plant.contentHash);
        auto invalidReference = plant;
        invalidReference.profile.branch.lengthReferenceMeters = -1.0f;
        invalidReference.contentHash = forest::forest_profile_content_hash(invalidReference.profile);
        CHECK(!owner.publish("Assets/Forest/invalid-reference.epoch_forest", invalidReference));
        invalidReference = plant;
        invalidReference.journal.back().afterLengthReferenceMeters =
            std::numeric_limits<float>::infinity();
        CHECK(!owner.publish("Assets/Forest/invalid-history.epoch_forest", invalidReference));
        const auto shortenedHash = plant.contentHash;
        CHECK(forest::apply_forest_profile_edit(plant,
            forest::ForestProfileProperty::BranchAngleDegrees, 35.0f));
        CHECK(forest::undo_forest_profile_edit(plant));
        CHECK(plant.contentHash == shortenedHash);
        auto legacyEdit = historical.document;
        const auto legacyTrunk = forest::morphology_recipe(legacyEdit.profile).growth.root_length_meters;
        CHECK(forest::apply_forest_profile_edit(legacyEdit,
            forest::ForestProfileProperty::BranchLengthMeters,
            legacyEdit.profile.branch.branchLengthMeters * 0.5f));
        CHECK(forest::morphology_recipe(legacyEdit.profile).growth.root_length_meters == legacyTrunk);
        CHECK(forest::undo_forest_profile_edit(legacyEdit));
        CHECK(legacyEdit.contentHash == historical.document.contentHash);
        CHECK(legacyEdit.profile.branch.lengthReferenceMeters == 0.0f);
        auto document = historical.document;
        const auto legacy = owner.publish(oldPath, document);
        if (!legacy) note(base / "legacy-failure.txt", std::string{library::forest_library_code_name(legacy.code)});
        CHECK(legacy);
        CHECK(read(legacy.source.storage_path) == read(fixtures / "default.epoch_forest"));
        CHECK(read(legacy.artifact.storage_path) == read(fixtures / "default.epoch_forest_artifact"));
        CHECK(hex(legacy.artifact.artifact_key) == "d8911f7382f14a7194b51a53fab5b280f05bad69a510f9eae6dd5966f48c43a9");
        CHECK(owner.load_exact(oldPath, legacy.artifact.artifact_key));
        CHECK(owner.load_latest(oldPath));

        const auto published = owner.publish_materialized(newPath, document);
        CHECK(published);
        const auto bytes = read(published.artifact.storage_path);
        CHECK(bytes.size() > 32u && bytes.at(8u) == 2u && bytes.at(9u) == 0u);
        CHECK(key(std::span{bytes}.first(bytes.size() - 32u)).words == published.artifact.artifact_key.words);
        CHECK(owner.load_exact(newPath, published.artifact.artifact_key).code
            == library::ForestLibraryCode::verified_key_required);
        CHECK(owner.load_latest(newPath).code == library::ForestLibraryCode::verified_key_required);
        const auto loaded = owner.load_materialized_exact(newPath, published.artifact);
        CHECK(loaded);
        const auto expected = forest::compile_forest_asset(document);
        CHECK(loaded.artifact.genome.stableHash == expected.genome.stableHash);
        CHECK(loaded.artifact.sourceRevision == expected.sourceRevision);
        CHECK(loaded.artifact.sourceContentHash == expected.sourceContentHash);
        CHECK(loaded.artifact.graph.content_hash == expected.graph.content_hash);
        CHECK(loaded.artifact.graph.nodes.size() == expected.graph.nodes.size());
        CHECK(loaded.artifact.graph.segments.size() == expected.graph.segments.size());
        CHECK(loaded.artifact.graph.terminals.size() == expected.graph.terminals.size());
        CHECK(loaded.artifact.sample.segments.size() == expected.sample.segments.size());
        CHECK(loaded.artifact.sample.terminals.size() == expected.sample.terminals.size());
        const auto same = [](const auto& a, const auto& b) {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        };
        for (std::size_t index = 0u; index != expected.graph.nodes.size(); ++index)
        {
            const auto& actual = loaded.artifact.graph.nodes[index];
            const auto& wanted = expected.graph.nodes[index];
            CHECK(actual.id == wanted.id && actual.parent == wanted.parent && actual.role == wanted.role);
            CHECK(actual.position.x == wanted.position.x && actual.position.y == wanted.position.y
                && actual.position.z == wanted.position.z && actual.radius_meters == wanted.radius_meters);
            CHECK(same(actual.tangent, wanted.tangent) && actual.depth == wanted.depth);
            CHECK(actual.lifetime.birth_seconds == wanted.lifetime.birth_seconds
                && actual.lifetime.end_seconds == wanted.lifetime.end_seconds);
        }
        for (std::size_t index = 0u; index != expected.graph.segments.size(); ++index)
        {
            const auto& actual = loaded.artifact.graph.segments[index];
            const auto& wanted = expected.graph.segments[index];
            CHECK(actual.id == wanted.id && actual.parent_node == wanted.parent_node
                && actual.child_node == wanted.child_node && actual.depth == wanted.depth);
            CHECK(same(actual.start, wanted.start) && same(actual.end, wanted.end)
                && same(actual.tangent_start, wanted.tangent_start) && same(actual.tangent_end, wanted.tangent_end));
            CHECK(actual.radius_start_meters == wanted.radius_start_meters
                && actual.radius_end_meters == wanted.radius_end_meters);
            CHECK(actual.lifetime.birth_seconds == wanted.lifetime.birth_seconds
                && actual.lifetime.end_seconds == wanted.lifetime.end_seconds);
        }
        for (std::size_t index = 0u; index != expected.graph.terminals.size(); ++index)
        {
            const auto& actual = loaded.artifact.graph.terminals[index];
            const auto& wanted = expected.graph.terminals[index];
            CHECK(actual.id == wanted.id && actual.node == wanted.node && actual.kind == wanted.kind);
            CHECK(same(actual.position, wanted.position) && same(actual.outward, wanted.outward) && same(actual.up, wanted.up));
            CHECK(actual.scale_meters == wanted.scale_meters && actual.variant == wanted.variant);
            CHECK(actual.lifetime.birth_seconds == wanted.lifetime.birth_seconds
                && actual.lifetime.end_seconds == wanted.lifetime.end_seconds);
        }
        CHECK(loaded.artifact.sample.time_seconds == expected.sample.time_seconds);
        for (std::size_t index = 0u; index != expected.sample.segments.size(); ++index)
        {
            const auto& actual = loaded.artifact.sample.segments[index];
            const auto& wanted = expected.sample.segments[index];
            CHECK(actual.source == wanted.source && actual.progress == wanted.progress && actual.depth == wanted.depth);
            CHECK(actual.radius_start_meters == wanted.radius_start_meters
                && actual.radius_end_meters == wanted.radius_end_meters);
            CHECK(actual.start.x == wanted.start.x && actual.start.y == wanted.start.y && actual.start.z == wanted.start.z);
            CHECK(actual.end.x == wanted.end.x && actual.end.y == wanted.end.y && actual.end.z == wanted.end.z);
        }
        for (std::size_t index = 0u; index != expected.sample.terminals.size(); ++index)
        {
            const auto& actual = loaded.artifact.sample.terminals[index];
            const auto& wanted = expected.sample.terminals[index];
            CHECK(actual.source == wanted.source && actual.kind == wanted.kind);
            CHECK(same(actual.position, wanted.position) && actual.scale_meters == wanted.scale_meters
                && actual.progress == wanted.progress);
        }
        CHECK(loaded.artifact.preview.segmentCount == expected.preview.segmentCount);
        CHECK(loaded.artifact.preview.leafCount == expected.preview.leafCount);
        for (std::size_t index = 0u; index != expected.preview.segmentCount; ++index)
        {
            const auto& actual = loaded.artifact.preview.segments[index];
            const auto& wanted = expected.preview.segments[index];
            CHECK(same(actual.start, wanted.start) && same(actual.end, wanted.end)
                && actual.radius == wanted.radius && actual.depth == wanted.depth);
        }
        for (std::size_t index = 0u; index != expected.preview.leafCount; ++index)
        {
            const auto& actual = loaded.artifact.preview.leaves[index];
            const auto& wanted = expected.preview.leaves[index];
            CHECK(same(actual.position, wanted.position) && actual.size == wanted.size
                && actual.sourceSegment == wanted.sourceSegment);
        }
        CHECK(loaded.artifact.voxelLods.levels.size() == expected.voxelLods.levels.size());
        const auto sameChunk = [](const auto& a, const auto& b) {
            return a.cellsX == b.cellsX && a.cellsY == b.cellsY && a.cellsZ == b.cellsZ
                && a.cellSizeMeters == b.cellSizeMeters && a.lodLevel == b.lodLevel;
        };
        for (std::size_t index = 0u; index != expected.voxelLods.levels.size(); ++index)
        {
            const auto& actual = loaded.artifact.voxelLods.levels[index];
            const auto& wanted = expected.voxelLods.levels[index];
            CHECK(sameChunk(actual.chunk, wanted.chunk) && actual.bounds_min_meters == wanted.bounds_min_meters
                && actual.bounds_max_meters == wanted.bounds_max_meters);
            CHECK(actual.minimum_view_distance_meters == wanted.minimum_view_distance_meters
                && actual.estimated_active_cells == wanted.estimated_active_cells
                && actual.dense_equivalent_bytes == wanted.dense_equivalent_bytes);
        }
        CHECK(loaded.artifact.voxelLods.semantics == expected.voxelLods.semantics
            && loaded.artifact.voxelLods.source_content_hash == expected.voxelLods.source_content_hash);
        const auto& actualOccupancy = loaded.artifact.voxelOccupancy;
        const auto& wantedOccupancy = expected.voxelOccupancy;
        CHECK(sameChunk(actualOccupancy.chunk, wantedOccupancy.chunk)
            && actualOccupancy.semantics == wantedOccupancy.semantics);
        CHECK(actualOccupancy.boundsMinMeters == wantedOccupancy.boundsMinMeters
            && actualOccupancy.boundsMaxMeters == wantedOccupancy.boundsMaxMeters);
        CHECK(actualOccupancy.trunkCells == wantedOccupancy.trunkCells
            && actualOccupancy.branchCells == wantedOccupancy.branchCells
            && actualOccupancy.foliageCells == wantedOccupancy.foliageCells
            && actualOccupancy.denseBytes == wantedOccupancy.denseBytes);
        CHECK(loaded.artifact.voxelOccupancy.activeCells == expected.voxelOccupancy.activeCells);
        const auto repeated = owner.publish_materialized(newPath, document);
        CHECK(repeated && repeated.code == library::ForestLibraryCode::unchanged);
        CHECK(repeated.artifact.artifact_key.words == published.artifact.artifact_key.words);
        library::ForestAssetLibrary restarted{"forest-v1-golden", base};
        CHECK(restarted.load_materialized_exact(newPath, published.artifact));
        library::ForestAssetLibrary foreign{"forest-v1-golden", base / "foreign"};
        CHECK(!foreign.load_materialized_exact(newPath, published.artifact));
        auto wrong = published.artifact;
        ++wrong.source_revision;
        CHECK(!owner.load_materialized_exact(newPath, wrong));

        auto corrupted = bytes;
        corrupted.at(16u) ^= 1u;
        write(published.artifact.storage_path, corrupted);
        CHECK(!owner.load_materialized_exact(newPath, published.artifact));
        // Rehashing an altered file cannot satisfy the original approved key.
        const auto changedDigest = epochengine::core::sha256::hash(std::span{corrupted}.first(corrupted.size() - 32u));
        std::copy(changedDigest.bytes.begin(), changedDigest.bytes.end(), corrupted.end() - 32);
        write(published.artifact.storage_path, corrupted);
        CHECK(!owner.load_materialized_exact(newPath, published.artifact));
        write(published.artifact.storage_path, bytes);
        CHECK(owner.load_materialized_exact(newPath, published.artifact));

        // These deliberately supplied new keys test structural decoding, not
        // producer proof. A caller must never approve model-provided keys.
        const auto refuseWithFreshKey = [&](std::vector<std::uint8_t> changed) {
            const auto digest = epochengine::core::sha256::hash(std::span{changed}.first(changed.size() - 32u));
            std::copy(digest.bytes.begin(), digest.bytes.end(), changed.end() - 32);
            auto testKey = published.artifact;
            testKey.artifact_key = key(std::span{changed}.first(changed.size() - 32u));
            testKey.storage_path = published.artifact.storage_path.parent_path()
                / (hex(testKey.artifact_key) + ".epoch_forest_artifact");
            write(testKey.storage_path, changed);
            return !owner.load_materialized_exact(newPath, testKey);
        };
        const auto sourceSize = integer(bytes, 10u);
        const auto producerSizeAt = 14u + sourceSize;
        const auto producerBegin = producerSizeAt + 4u;
        CHECK(producerBegin < bytes.size());
        auto badProducer = bytes;
        badProducer.at(producerBegin) ^= 1u;
        CHECK(refuseWithFreshKey(std::move(badProducer)));
        auto badSource = bytes;
        badSource.at(14u + sourceSize - 1u) ^= 1u;
        CHECK(refuseWithFreshKey(std::move(badSource)));
        auto overBudget = bytes;
        const auto graphSizeAt = producerBegin + integer(bytes, producerSizeAt);
        integer(overBudget, graphSizeAt, 0xffffffffu);
        CHECK(refuseWithFreshKey(std::move(overBudget)));
        // Frozen v2 Recipe layout: document8 + revision40 + enums2 + seed8
        // + counts8 + growth floats68 + limits20 + duration4 =158 bytes.
        // Followed by graph hash8, truncated1, then the node count.
        const auto graphBegin = graphSizeAt + 4u;
        constexpr std::size_t recipeBytes = 158u;
        auto tooManyNodes = bytes;
        integer(tooManyNodes, graphBegin + recipeBytes + 9u, 0xffffffffu);
        CHECK(refuseWithFreshKey(std::move(tooManyNodes)));
        auto nonfiniteNode = bytes;
        integer(nonfiniteNode, graphBegin + recipeBytes + 9u + 4u + 21u, 0x7fc00000u);
        CHECK(refuseWithFreshKey(std::move(nonfiniteNode)));
        const auto sampleSizeAt = graphBegin + integer(bytes, graphSizeAt);
        auto tooManySamples = bytes;
        integer(tooManySamples, sampleSizeAt + 8u, 0xffffffffu);
        CHECK(refuseWithFreshKey(std::move(tooManySamples)));
        library::ForestLibraryLimits small;
        small.maximum_artifact_bytes = 512u;
        library::ForestAssetLibrary bounded{"forest-v1-golden", base, small};
        CHECK(!bounded.load_materialized_exact(newPath, published.artifact));
        CHECK(!bounded.publish_materialized("Assets/Forest/too-large.epoch_forest", document));
        CHECK(!fs::exists(base / "Assets/Forest/too-large.epoch_forest"));
        const auto mixed = owner.publish_materialized(oldPath, document);
        CHECK(mixed && owner.load_materialized_exact(oldPath, mixed.artifact));
        CHECK(owner.load_latest(oldPath).code == library::ForestLibraryCode::verified_key_required);
        CHECK(read(legacy.source.storage_path) == read(fixtures / "default.epoch_forest"));
        CHECK(read(legacy.artifact.storage_path) == read(fixtures / "default.epoch_forest_artifact"));
        CHECK(owner.load_exact(oldPath, legacy.artifact.artifact_key));
        note(base / "result.txt", "PASS: actual v1 golden bytes/key/replay unchanged; explicit v2 publication/exact-key roundtrip/restart; generic v2 refusal; foreign/stale keys, rehashed tamper and budgets rejected.\n");
    }
    catch (...) { note(base / "exception.txt", "Forest codec contract exception.\n"); return 255; }
    return 0;
}
