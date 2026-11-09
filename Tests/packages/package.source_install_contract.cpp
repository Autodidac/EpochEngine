// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <chrono>
#include <compare>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <stdexcept>
#include <string_view>
#include <exception>
#include <string>
#include <vector>

import package.source_install;
import package.registry;
import platform.filesystem;

namespace
{
    namespace fs = std::filesystem;
    std::string read(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
    void write(const fs::path& path, const std::string& bytes)
    {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
}

#define CHECK(condition) do { if (!(condition)) { write(base / "failure-check.txt", std::to_string(__LINE__)); return __LINE__; } } while (false)

int main(int argc, char** argv)
{
    using namespace epochengine::package_source;
    const auto base = fs::current_path() / ("package-source-contract-"
        + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
    fs::create_directories(base);
    fs::path manifest;
    if (argc > 1) manifest = fs::absolute(argv[1]);
    else
    {
        const auto repository = base / "fixture";
        manifest = repository / "packages/planetary_terrain/package.epoch-package.json";
        write(repository / "LICENSE", "Test fixture license evidence; not a legal verification.\n");
        write(repository / "packages/_api/epoch_package_script_api.hpp", "// Source-only test API.\n");
        write(manifest.parent_path() / "CMakeLists.txt", "# Must never be configured by source staging.\n");
        write(manifest.parent_path() / "src/unit.cpp", "// Source-only fixture.\n");
        write(manifest.parent_path() / "scripts/unit.ascript.cpp", "// Must never be executed.\n");
        write(manifest, R"JSON({"id":"planetary_terrain","displayName":"Planet source","summary":"Source admission contract","manifestLicense":"LicenseRef-MIT-NoSell","kind":"code-extension","activation":"package-manager-opt-in","security":{"serverOrListenerAllowed":false,"requiresExplicitNetworkApproval":false,"autoRunAllowed":false,"includeByDefault":false},"source":{"repo":"https://github.com/Autodidac/EpochEngineExtensions","path":"packages/planetary_terrain"},"script":{"path":"scripts/unit.ascript.cpp","language":"C++23","api":"epoch_package_script_api","humanGated":true,"autoRunAllowed":false},"build":{"humanGated":true,"sourceFiles":["CMakeLists.txt","src/unit.cpp"]}})JSON");
    }
    // Provenance migration is local and exact. These tests never download,
    // configure, execute or approve a package without the existing hash gate.
    if (argc <= 1)
    {
        namespace registry = epochengine::package_registry;
        constexpr std::string_view legacy = "https://github.com/Autodidac/EpochEngineExtensions";
        const auto original = read(manifest);
        const auto legacyReview = inspect_local_source(manifest);
        CHECK(legacyReview.review_ready() && legacyReview.declared_repository == legacy);
        for (const auto& binding : registry::kSourceManifestBindings)
        {
            CHECK(binding.declared_repository == registry::kEpochEngineExtensionsRepo);
            CHECK(registry::source_manifest_repository_matches(binding, legacy));
            CHECK(registry::source_manifest_repository_matches(binding, registry::kEpochEngineExtensionsRepo));
        }
        CHECK(registry::external_source_repo("research_planetary_terrain") == registry::kEpochEngineExtensionsRepo);
        CHECK(registry::external_source_repo("research_fft_ocean") == registry::kEpochEngineExtensionsRepo);
        const auto with_repository = [&](std::string_view repository)
        {
            auto text = original;
            const auto offset = text.find(legacy);
            if (offset == std::string::npos) throw std::runtime_error("Fixture origin missing");
            text.replace(offset, legacy.size(), repository);
            return text;
        };
        for (const std::string_view foreign : {
            "https://github.com/Autodidac/EpochEngineExtensions.evil",
            "https://github.com/Autodidac/EpochEngineExtensions.git",
            "https://github.com/Other/EpochEngineExtensions",
            "https://epoch.adamrushford.chatgpt.site.evil/git/EpochEngineExtensions.git",
            "https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git?other=1"})
        {
            write(manifest, with_repository(foreign));
            CHECK(inspect_local_source(manifest).code == Code::invalid_manifest);
        }
        const auto canonicalManifest = with_repository(registry::kEpochEngineExtensionsRepo);
        write(manifest, canonicalManifest);
        const auto canonicalReview = inspect_local_source(manifest);
        CHECK(canonicalReview.review_ready());
        CHECK(canonicalReview.declared_repository == registry::kEpochEngineExtensionsRepo);
        CHECK(canonicalReview.content_hash != legacyReview.content_hash);
        write(manifest, original);
        const auto migrationCache = base / "origin migration cache";
        const auto oldStage = stage_local_source(legacyReview, legacyReview.content_hash, migrationCache);
        CHECK(oldStage.source_staged());
        const auto migrationProject = base / "origin migration project";
        write(migrationProject / "project.epoch.json", "{\"id\":\"retained\"}\n");
        write(migrationProject / "worlds/main.epoch", "retained scene and tree materials\n");
        write(migrationProject / "Assets/Worlds/Planet.epoch_planet", "retained planet and water settings\n");
        const auto oldAttachment = attach_project_source(legacyReview, oldStage,
            legacyReview.content_hash, migrationProject);
        CHECK(oldAttachment.source_attached());
        const auto oldReceipt = read(migrationProject / oldAttachment.relative_receipt_path);
        CHECK(load_project_attachment(migrationProject, "planetary_terrain").source_attached());
        write(manifest, canonicalManifest);
        const auto newStage = stage_local_source(canonicalReview, canonicalReview.content_hash, migrationCache);
        CHECK(newStage.source_staged());
        CHECK(update_project_source(canonicalReview, newStage, {}, migrationProject,
            legacyReview.content_hash).code == Code::approval_required);
        CHECK(load_project_attachment(migrationProject, "planetary_terrain").content_hash == legacyReview.content_hash);
        CHECK(update_project_source(canonicalReview, newStage, canonicalReview.content_hash, migrationProject,
            "sha256:stale").code == Code::source_changed);
        const auto migrated = update_project_source(canonicalReview, newStage, canonicalReview.content_hash,
            migrationProject, legacyReview.content_hash);
        CHECK(migrated.source_attached() && !migrated.retained_previous_source.empty());
        CHECK(load_project_attachment(migrationProject, "planetary_terrain").content_hash == canonicalReview.content_hash);
        const auto retained = inspect_local_source(migrated.retained_previous_source / "package.epoch-package.json");
        CHECK(retained.review_ready() && retained.content_hash == legacyReview.content_hash);
        CHECK(retained.declared_repository == legacy);
        auto previousRoot = migrated.retained_previous_source;
        for (const auto& part : fs::path{legacyReview.source_path})
        { (void)part; previousRoot = previousRoot.parent_path(); }
        previousRoot = previousRoot.parent_path();
        CHECK(read(previousRoot / "project.dependency.json") == oldReceipt);
        CHECK(read(migrationProject / "project.epoch.json") == "{\"id\":\"retained\"}\n");
        CHECK(read(migrationProject / "worlds/main.epoch") == "retained scene and tree materials\n");
        CHECK(read(migrationProject / "Assets/Worlds/Planet.epoch_planet") == "retained planet and water settings\n");
        CHECK(read(migrationProject / oldAttachment.relative_receipt_path) != oldReceipt);
        CHECK(verify_staged_source(canonicalReview, migrationCache).source_staged());
        CHECK(verify_staged_source(legacyReview, migrationCache).source_staged());
        write(manifest, original);
    }
    const auto inspection = inspect_local_source(manifest);
    CHECK(inspection.review_ready());
    CHECK(inspection.manifest_id == "planetary_terrain");
    CHECK(inspection.registry_package_id == "research_planetary_terrain");
    const auto cache = base / "cache with spaces";
    CHECK(stage_local_source(inspection, "sha256:unapproved", cache).code == Code::approval_required);
    CHECK(!fs::exists(cache));
    std::stop_source stop;
    stop.request_stop();
    CHECK(inspect_local_source(manifest, stop.get_token()).code == Code::cancelled);
    CHECK(stage_local_source(inspection, inspection.content_hash, cache, stop.get_token()).code == Code::cancelled);
    CHECK(!fs::exists(cache));
    const auto staged = stage_local_source(inspection, inspection.content_hash, cache);
    if (!staged.source_staged()) write(base / "failure.txt", staged.message);
    CHECK(staged.source_staged());
    CHECK(fs::is_regular_file(staged.receipt_path));
    CHECK(fs::is_regular_file(staged.source_root / "package.epoch-package.json"));
    CHECK(read(staged.receipt_path).find("\"activated\":false") != std::string::npos);
    CHECK(stage_local_source(inspection, inspection.content_hash, cache).code == Code::already_staged);
    CHECK(verify_staged_source(inspection, cache).source_staged());
    CHECK(inspect_cached_source(base / "missing cache", "research_planetary_terrain").inspection.code == Code::missing_file);
    CHECK(inspect_cached_source(cache, "unknown-package").inspection.code == Code::unknown_package);
    CHECK(inspect_cached_source(cache, "research_planetary_terrain", stop.get_token()).inspection.code == Code::cancelled);
    const auto recovered = inspect_cached_source(cache, "research_planetary_terrain");
    CHECK(recovered.review_ready());
    CHECK(recovered.inspection.content_hash == inspection.content_hash);
    CHECK(recovered.staged.source_root == staged.source_root);
    const auto freshCache = base / "fresh editor cache";
    CHECK(inspect_cached_source(freshCache, "research_planetary_terrain").inspection.code == Code::missing_file);
    const auto firstUse = inspect_available_source(freshCache, inspection.repository_root,
        "research_planetary_terrain");
    CHECK(firstUse.review_ready() && firstUse.inspection.content_hash == inspection.content_hash);
    CHECK(firstUse.staged.source_root != inspection.manifest_path.parent_path());
    CHECK(inspect_local_source(manifest).content_hash == inspection.content_hash);
    CHECK(!fs::exists(inspection.repository_root / "project.epoch.json"));
    CHECK(inspect_available_source(base / "cancelled discovery", inspection.repository_root,
        "research_planetary_terrain", stop.get_token()).inspection.code == Code::cancelled);
    CHECK(!fs::exists(base / "cancelled discovery"));
    CHECK(inspect_available_source(base / "unknown discovery", inspection.repository_root,
        "unknown-package").inspection.code == Code::unknown_package);
    CHECK(!fs::exists(base / "unknown discovery"));
    CHECK(!inspect_available_source(base / "missing source cache", base / "missing source",
        "research_planetary_terrain").review_ready());
    CHECK(!fs::exists(base / "missing source cache"));
    const auto recoveredProject = base / "recovered project";
    write(recoveredProject / "project.epoch.json", "{}\n");
    CHECK(attach_project_source(recovered.inspection, recovered.staged,
        recovered.inspection.content_hash, recoveredProject).source_attached());

    // The real installer resolves one optional source feature. These source-only
    // fixtures never configure CMake, run package scripts or render water.
    using Feature = epochengine::package_registry::SourceFeature;
    const auto feature = Feature::planetary_ocean;
    const auto dryRule = epochengine::package_registry::source_feature_dependencies("planetary_terrain",feature,false);
    const auto wetRule = epochengine::package_registry::source_feature_dependencies("research_planetary_terrain",feature,true);
    CHECK(dryRule.valid() && dryRule.required.empty());
    CHECK(wetRule.valid() && wetRule.required.size()==1u && wetRule.required.front().registry_id=="research_fft_ocean");
    CHECK(!epochengine::package_registry::source_feature_dependencies("fft_ocean",feature,true).valid());
    CHECK(!epochengine::package_registry::source_feature_dependencies("planetary_terrain",static_cast<Feature>(255),false).valid());
    const auto featureProject=base/"feature project", featureCache=base/"feature cache", fftRepository=base/"FFT fixture";
    write(featureProject/"project.epoch.json","{\"id\":\"feature-project\"}\n");
    write(featureProject/"worlds/main.epoch","retained scene\n");
    write(featureProject/"Assets/Worlds/Planet.epoch_planet","retained dry recipe\n");
    CHECK(attach_project_source(inspection,staged,inspection.content_hash,featureProject).source_attached());
    const auto parentBefore=load_project_attachment(featureProject,"planetary_terrain");
    const auto parentReceiptBefore=read(featureProject/parentBefore.relative_receipt_path);
    const auto featureManifestBefore=read(featureProject/"project.epoch.json");
    const auto dry=review_project_feature(featureProject,"planetary_terrain",feature,false,
        featureCache,base/"missing FFT repository");
    CHECK(dry.sources_admitted() && admit_project_feature(dry,{}).sources_admitted());
    CHECK(!fs::exists(featureCache) && !fs::exists(featureProject/"Packages/fft_ocean"));
    CHECK(!review_project_feature(featureProject,"planetary_terrain",feature,true,
        featureCache,base/"missing FFT repository").needs_approval());
    CHECK(!fs::exists(featureProject/"Packages/fft_ocean"));
    const auto fftManifest=fftRepository/"packages/fft_ocean/package.epoch-package.json";
    write(fftRepository/"LICENSE","Synthetic FFT fixture; not legal or algorithm evidence.\n");
    write(fftRepository/"packages/_api/epoch_package_script_api.hpp","// Inert test API.\n");
    write(fftManifest.parent_path()/"CMakeLists.txt","# Never configure this source-only fixture.\n");
    write(fftManifest.parent_path()/"src/unit.cpp","// Never execute.\n");
    write(fftManifest.parent_path()/"scripts/unit.ascript.cpp","// Never execute.\n");
    write(fftManifest,R"JSON({"id":"fft_ocean","displayName":"FFT source","summary":"Dependency admission fixture","manifestLicense":"LicenseRef-MIT-NoSell","kind":"code-extension","activation":"package-manager-opt-in","security":{"serverOrListenerAllowed":false,"requiresExplicitNetworkApproval":false,"autoRunAllowed":false,"includeByDefault":false},"source":{"repo":"https://github.com/Autodidac/EpochEngineExtensions","path":"packages/fft_ocean"},"script":{"path":"scripts/unit.ascript.cpp","language":"C++23","api":"epoch_package_script_api","humanGated":true,"autoRunAllowed":false},"build":{"humanGated":true,"sourceFiles":["CMakeLists.txt","src/unit.cpp"]}})JSON");
    const auto featureReview=review_project_feature(featureProject,"research_planetary_terrain",feature,true,featureCache,fftRepository);
    if(!featureReview.needs_approval())write(base/"feature-review-failure.txt",featureReview.message);
    CHECK(featureReview.needs_approval() && featureReview.dependency.review_ready());
    CHECK(featureReview.owner.content_hash==inspection.content_hash);
    CHECK(featureReview.dependency.inspection.manifest_id=="fft_ocean");
    CHECK(!fs::exists(featureProject/"Packages/fft_ocean"));
    CHECK(admit_project_feature(featureReview,{}).code==Code::approval_required);
    CHECK(admit_project_feature(featureReview,"sha256:other").code==Code::approval_required);
    CHECK(admit_project_feature(featureReview,featureReview.dependency.inspection.content_hash,stop.get_token()).code==Code::cancelled);
    CHECK(review_project_feature(featureProject,"planetary_terrain",feature,true,featureCache,fftRepository,stop.get_token()).code==Code::cancelled);
    CHECK(!fs::exists(featureProject/"Packages/fft_ocean"));
    {
        auto lease=epochengine::platform::filesystem::ExclusiveFileLease::acquire(featureProject/"Packages/.source-planetary_terrain.lock");
        CHECK(lease);
        CHECK(admit_project_feature(featureReview,featureReview.dependency.inspection.content_hash).code==Code::cache_collision);
    }
    {
        auto lease=epochengine::platform::filesystem::ExclusiveFileLease::acquire(featureProject/"Packages/.source-fft_ocean.lock");
        CHECK(lease);
        CHECK(admit_project_feature(featureReview,featureReview.dependency.inspection.content_hash).code==Code::cache_collision);
    }
    const auto parentUnit=featureProject/parentBefore.relative_source_root/"CMakeLists.txt";
    const auto parentUnitBefore=read(parentUnit);
    write(parentUnit,parentUnitBefore+"\n# Unreviewed parent change\n");
    CHECK(admit_project_feature(featureReview,featureReview.dependency.inspection.content_hash).code==Code::source_changed);
    CHECK(!fs::exists(featureProject/"Packages/fft_ocean"));
    write(parentUnit,parentUnitBefore);
    write(featureProject/"project.epoch.json","{\"id\":\"different project\"}\n");
    CHECK(admit_project_feature(featureReview,featureReview.dependency.inspection.content_hash).code==Code::source_changed);
    write(featureProject/"project.epoch.json",featureManifestBefore);
    const auto fftCachedUnit=featureReview.dependency.staged.source_root/"CMakeLists.txt";
    const auto fftUnitBefore=read(fftCachedUnit);
    write(fftCachedUnit,fftUnitBefore+"\n# Changed staged bytes\n");
    CHECK(admit_project_feature(featureReview,featureReview.dependency.inspection.content_hash).code==Code::source_changed);
    CHECK(!fs::exists(featureProject/"Packages/fft_ocean"));
    write(fftCachedUnit,fftUnitBefore);
    const auto featureAdmission=admit_project_feature(featureReview,featureReview.dependency.inspection.content_hash);
    if(!featureAdmission.sources_admitted())write(base/"feature-admit-failure.txt",featureAdmission.message);
    CHECK(featureAdmission.sources_admitted() && featureAdmission.dependency.manifest_id=="fft_ocean");
    CHECK(featureAdmission.owner.content_hash==parentBefore.content_hash);
    CHECK(read(featureProject/parentBefore.relative_receipt_path)==parentReceiptBefore);
    CHECK(read(featureProject/"project.epoch.json")==featureManifestBefore);
    CHECK(read(featureProject/"worlds/main.epoch")=="retained scene\n");
    CHECK(read(featureProject/"Assets/Worlds/Planet.epoch_planet")=="retained dry recipe\n");
    const auto reusedFeature=review_project_feature(featureProject,"planetary_terrain",feature,true,
        base/"absent cache",base/"absent repository");
    CHECK(reusedFeature.sources_admitted() && !reusedFeature.needs_approval());
    CHECK(admit_project_feature(reusedFeature,{}).sources_admitted());
    const auto installedFFT=featureProject/featureAdmission.dependency.relative_source_root/"CMakeLists.txt";
    write(installedFFT,"Retain corrupt existing source for explicit recovery.\n");
    CHECK(!review_project_feature(featureProject,"planetary_terrain",feature,true,featureCache,fftRepository).sources_admitted());
    CHECK(!admit_project_feature(reusedFeature,{}).sources_admitted());
    CHECK(read(installedFFT)=="Retain corrupt existing source for explicit recovery.\n");
    CHECK(review_project_feature(featureProject,"planetary_terrain",feature,false,
        base/"absent cache",base/"absent repository").sources_admitted());
    CHECK(read(featureProject/"worlds/main.epoch")=="retained scene\n");
    const auto tamperCache = base / "tamper cache";
    const auto tamperStage = stage_local_source(inspection, inspection.content_hash, tamperCache);
    CHECK(tamperStage.source_staged());
    const auto tamperFile = tamperStage.source_root / "CMakeLists.txt";
    write(tamperFile, read(tamperFile) + "\n# changed after cache publication\n");
    CHECK(!inspect_cached_source(tamperCache, "research_planetary_terrain").review_ready());
    const auto tamperedBytes = read(tamperFile);
    CHECK(!inspect_available_source(tamperCache, inspection.repository_root,
        "research_planetary_terrain").review_ready());
    CHECK(read(tamperFile) == tamperedBytes);
    const auto ambiguousCache = base / "ambiguous cache";
    CHECK(stage_local_source(inspection, inspection.content_hash, ambiguousCache).source_staged());
    const auto alternative = base / "alternative source";
    for (const auto& file : inspection.files)
        write(alternative / file.relative_path, read(inspection.repository_root / file.relative_path));
    const auto alternativeCmake = alternative / inspection.source_path / "CMakeLists.txt";
    write(alternativeCmake, read(alternativeCmake) + "\n# separate reviewed version\n");
    const auto secondReview = inspect_local_source(alternative / inspection.source_path / "package.epoch-package.json");
    CHECK(secondReview.review_ready() && secondReview.content_hash != inspection.content_hash);
    CHECK(stage_local_source(secondReview, secondReview.content_hash, ambiguousCache).source_staged());
    CHECK(inspect_cached_source(ambiguousCache, "research_planetary_terrain").inspection.code == Code::cache_collision);
    const auto exactAvailable = inspect_available_source(ambiguousCache, inspection.repository_root,
        "research_planetary_terrain");
    CHECK(exactAvailable.review_ready() && exactAvailable.inspection.content_hash == inspection.content_hash);
    CHECK(inspect_available_source(ambiguousCache, {},
        "research_planetary_terrain").inspection.code == Code::cache_collision);
    // A new explicit review checks the current durable source. Both immutable
    // snapshots and the already attached old project copy must stay unchanged.
    const auto firstReceipt = read(firstUse.staged.receipt_path);
    const auto recoveredAttachment = load_project_attachment(recoveredProject, "research_planetary_terrain");
    CHECK(recoveredAttachment.source_attached());
    const auto attachedReceipt = read(recoveredProject / recoveredAttachment.relative_receipt_path);
    const auto refreshed = inspect_available_source(freshCache, alternative, "research_planetary_terrain");
    CHECK(refreshed.review_ready() && refreshed.inspection.content_hash == secondReview.content_hash);
    CHECK(refreshed.staged.source_root != firstUse.staged.source_root);
    CHECK(read(firstUse.staged.receipt_path) == firstReceipt);
    CHECK(verify_staged_source(inspection, freshCache).source_staged());
    CHECK(verify_staged_source(secondReview, freshCache).source_staged());
    CHECK(read(recoveredProject / recoveredAttachment.relative_receipt_path) == attachedReceipt);
    CHECK(load_project_attachment(recoveredProject, "research_planetary_terrain").content_hash == inspection.content_hash);
    CHECK(inspect_available_source(freshCache, {}, "research_planetary_terrain").inspection.code == Code::cache_collision);
    CHECK(inspect_available_source(freshCache, base / "missing source", "research_planetary_terrain").inspection.code == Code::cache_collision);
    CHECK(inspect_available_source(freshCache, alternative, "research_planetary_terrain", stop.get_token()).inspection.code == Code::cancelled);
    const auto alternativeManifest = alternative / inspection.source_path / "package.epoch-package.json";
    const auto exactManifest = read(alternativeManifest);
    write(alternativeManifest, "Malformed refreshed manifest.\n");
    CHECK(!inspect_available_source(freshCache, alternative, "research_planetary_terrain").review_ready());
    CHECK(read(firstUse.staged.receipt_path) == firstReceipt);
    CHECK(read(recoveredProject / recoveredAttachment.relative_receipt_path) == attachedReceipt);
    write(alternativeManifest, exactManifest);
    // Explicit reviewed replacement is distinct from Add. Both approved
    // versions, original scene/settings and the old dependency remain intact.
    write(recoveredProject/"worlds/main.epoch","operator scene bytes\n");
    write(recoveredProject/"Assets/Worlds/Planet.epoch_planet","operator recipe bytes\n");
    const auto projectManifest=read(recoveredProject/"project.epoch.json");
    CHECK(!attach_project_source(secondReview,refreshed.staged,secondReview.content_hash,recoveredProject).source_attached());
    CHECK(update_project_source(secondReview,refreshed.staged,"not approved",recoveredProject,inspection.content_hash).code==Code::approval_required);
    CHECK(update_project_source(secondReview,refreshed.staged,secondReview.content_hash,recoveredProject,"sha256:stale").code==Code::source_changed);
    CHECK(update_project_source(secondReview,refreshed.staged,secondReview.content_hash,recoveredProject,inspection.content_hash,stop.get_token()).code==Code::cancelled);
    {
        auto lease=epochengine::platform::filesystem::ExclusiveFileLease::acquire(recoveredProject/"Packages/.source-planetary_terrain.lock");
        CHECK(lease);
        CHECK(update_project_source(secondReview,refreshed.staged,secondReview.content_hash,recoveredProject,inspection.content_hash).code==Code::cache_collision);
        CHECK(attach_project_source(inspection,staged,inspection.content_hash,recoveredProject).code==Code::cache_collision);
    }
    const auto updated=update_project_source(secondReview,refreshed.staged,secondReview.content_hash,recoveredProject,inspection.content_hash);
    if(!updated.source_attached())write(base/"update-failure.txt",updated.message);
    CHECK(updated.source_attached()&&!updated.retained_previous_source.empty());
    CHECK(load_project_attachment(recoveredProject,"planetary_terrain").content_hash==secondReview.content_hash);
    CHECK(inspect_local_source(updated.retained_previous_source/"package.epoch-package.json").content_hash==inspection.content_hash);
    CHECK(read(recoveredProject/"worlds/main.epoch")=="operator scene bytes\n");
    CHECK(read(recoveredProject/"Assets/Worlds/Planet.epoch_planet")=="operator recipe bytes\n");
    CHECK(read(recoveredProject/"project.epoch.json")==projectManifest);
    auto retained=updated.retained_previous_source;
    for(const auto& part:fs::path{inspection.source_path}){(void)part;retained=retained.parent_path();}
    retained=retained.parent_path();
    CHECK(read(retained/"project.dependency.json")==attachedReceipt);
    for(const auto& file:inspection.files)CHECK(read(retained/"source"/file.relative_path)==read(inspection.repository_root/file.relative_path));
    CHECK(verify_staged_source(inspection,freshCache).source_staged()&&verify_staged_source(secondReview,freshCache).source_staged());
    CHECK(update_project_source(secondReview,refreshed.staged,secondReview.content_hash,recoveredProject,inspection.content_hash).code==Code::source_changed);
    const auto project = base / "project with spaces";
    write(project / "project.epoch.json", "{}\n");
    CHECK(attach_project_source(inspection, staged, "unapproved", project).code == Code::approval_required);
    CHECK(!fs::exists(project / "Packages"));
    CHECK(attach_project_source(inspection, staged, inspection.content_hash, project, stop.get_token()).code == Code::cancelled);
    CHECK(!fs::exists(project / "Packages"));
    auto wrongStage = staged;
    wrongStage.content_hash = "sha256:wrong";
    CHECK(attach_project_source(inspection, wrongStage, inspection.content_hash, project).code == Code::source_changed);
    const auto attached = attach_project_source(inspection, staged, inspection.content_hash, project);
    if (!attached.source_attached()) write(base / "project-failure.txt", attached.message);
    CHECK(attached.source_attached());
    CHECK(attached.relative_source_root == fs::path{"Packages"} / inspection.manifest_id
        / "source" / inspection.source_path);
    CHECK(attached.relative_source_root.generic_string().find(inspection.content_hash.substr(7u)) == std::string::npos);
    CHECK(!attached.relative_source_root.is_absolute() && !attached.relative_receipt_path.is_absolute());
    CHECK(read(project / attached.relative_receipt_path).find("\"automatic_execution\":false") != std::string::npos);
    CHECK(read(project / attached.relative_receipt_path).find(inspection.content_hash) != std::string::npos);
    CHECK(read((project / attached.relative_receipt_path).parent_path() / "source.receipt.json")
        .find(inspection.content_hash) != std::string::npos);
    const auto projectSource = project / attached.relative_source_root;
    const auto localInspection = inspect_local_source(projectSource / "package.epoch-package.json");
    CHECK(localInspection.review_ready() && localInspection.content_hash == inspection.content_hash);
    for (const auto& file : inspection.files)
        CHECK(read(localInspection.repository_root / file.relative_path) == read(inspection.repository_root / file.relative_path));
    CHECK(verify_project_source(inspection, project).source_attached());
    CHECK(attach_project_source(inspection, staged, inspection.content_hash, project).code == Code::already_staged);
    CHECK(load_project_attachment(project, "research_planetary_terrain").source_attached());
    const auto replacementRepository = base / "replacement source";
    for (const auto& file : inspection.files)
        write(replacementRepository / file.relative_path, read(inspection.repository_root / file.relative_path));
    write(replacementRepository / inspection.source_path / "CMakeLists.txt", "# A different reviewed source version.\n");
    const auto replacement = inspect_local_source(replacementRepository / inspection.source_path / "package.epoch-package.json");
    CHECK(replacement.review_ready() && replacement.content_hash != inspection.content_hash);
    const auto replacementStage = stage_local_source(replacement, replacement.content_hash, base / "replacement cache");
    CHECK(replacementStage.source_staged());
    CHECK(attach_project_source(replacement, replacementStage, replacement.content_hash, project).code == Code::cache_collision);
    CHECK(load_project_attachment(project, "research_planetary_terrain").content_hash == inspection.content_hash);
    CHECK(load_project_attachment(project, "planetary_terrain").source_attached());
    CHECK(load_project_attachment(project, "../escape").code == Code::unknown_package);
    CHECK(load_project_attachment(project, "research_planetary_terrain", stop.get_token()).code == Code::cancelled);
    const auto dependencyReceipt = read(project / attached.relative_receipt_path);
    write(project / attached.relative_receipt_path, "foreign project receipt");
    CHECK(!load_project_attachment(project, "research_planetary_terrain").source_attached());
    CHECK(attach_project_source(inspection, staged, inspection.content_hash, project).code == Code::cache_collision);
    CHECK(read(project / attached.relative_receipt_path) == "foreign project receipt");
    write(project / attached.relative_receipt_path, dependencyReceipt);
    const auto projectCmake = read(projectSource / "CMakeLists.txt");
    write(projectSource / "CMakeLists.txt", projectCmake + "\n# unapproved project modification\n");
    CHECK(verify_project_source(inspection, project).code == Code::source_changed);
    CHECK(load_project_attachment(project, "research_planetary_terrain").code == Code::source_changed);
    write(projectSource / "CMakeLists.txt", projectCmake);
    write((project / attached.relative_receipt_path).parent_path() / "unexpected.cpp", "unapproved");
    CHECK(!load_project_attachment(project, "research_planetary_terrain").source_attached());
    fs::remove((project / attached.relative_receipt_path).parent_path() / "unexpected.cpp");
    CHECK(load_project_attachment(project, "research_planetary_terrain").source_attached());
    const auto copied = inspect_local_source(staged.source_root / "package.epoch-package.json");
    CHECK(copied.review_ready() && copied.content_hash == inspection.content_hash);
    CHECK(stage_local_source(copied, copied.content_hash, copied.repository_root / "cache").code == Code::unsafe_path);
#if !defined(_WIN32)
    // A mounted Linux volume may alias SOURCE and source. Never clean a
    // spelling that identifies the approved source itself.
    const auto caseSibling = copied.repository_root.parent_path() / "SOURCE";
    std::error_code identityError;
    const bool aliasesSource = fs::equivalent(caseSibling, copied.repository_root, identityError);
    if (!identityError && aliasesSource)
    {
        CHECK(stage_local_source(copied, copied.content_hash, caseSibling).code == Code::unsafe_path);
        CHECK(stage_local_source(copied, copied.content_hash, caseSibling / "nested cache").code == Code::unsafe_path);
        CHECK(!fs::exists(copied.repository_root / "nested cache"));
        CHECK(verify_staged_source(inspection, cache).source_staged());
    }
    else
    {
        CHECK(!fs::exists(caseSibling));
        CHECK(stage_local_source(copied, copied.content_hash, caseSibling).source_staged());
        CHECK(caseSibling.lexically_relative(base).begin()->string() != "..");
        identityError.clear();
        CHECK(!fs::equivalent(caseSibling, copied.repository_root, identityError) && !identityError);
        fs::remove_all(caseSibling);
    }
    write(base / "case-identity-proof.txt", aliasesSource ? "existing-alias-refused\n" : "distinct-sibling-staged\n");
#endif
    const auto cachedManifest = staged.source_root / "package.epoch-package.json";
    const auto originalManifest = read(cachedManifest);
    const auto cmake = staged.source_root / "CMakeLists.txt";
    const auto originalCmake = read(cmake);
    write(cmake, originalCmake + "\n# changed after review\n");
    CHECK(verify_staged_source(inspection, cache).code == Code::source_changed);
    CHECK(stage_local_source(copied, copied.content_hash, base / "second cache").code == Code::source_changed);
    CHECK(!fs::exists(base / "second cache"));
    write(cmake, originalCmake);
    const auto extra = staged.source_root / "unsigned.cmake";
    write(extra, "# Not in the reviewed source inventory.\n");
    CHECK(verify_staged_source(inspection, cache).code == Code::cache_collision);
    fs::remove(extra);
    const auto receipt = read(staged.receipt_path);
    write(staged.receipt_path, "foreign receipt must not be overwritten");
    CHECK(stage_local_source(inspection, inspection.content_hash, cache).code == Code::cache_collision);
    CHECK(read(staged.receipt_path) == "foreign receipt must not be overwritten");
    write(staged.receipt_path, receipt);
    CHECK(verify_staged_source(inspection, cache).source_staged());
    const auto field = originalManifest.find("\"CMakeLists.txt\"");
    CHECK(field != std::string::npos);
    for (const auto* replacement : {"\"../escape.cpp\"", "\"C:/escape.cpp\"",
        "\"CMakeLists.txt\",\"cmakelists.txt\"", "\"payload.dll\""})
    {
        auto modified = originalManifest;
        modified.replace(field, std::string{"\"CMakeLists.txt\""}.size(), replacement);
        write(cachedManifest, modified);
        CHECK(inspect_local_source(cachedManifest).code == Code::unsafe_path);
    }
    write(cachedManifest, originalManifest);
    auto server = originalManifest;
    const auto marker = server.find("\"serverOrListenerAllowed\"");
    CHECK(marker != std::string::npos);
    const auto value = server.find("false", marker);
    CHECK(value != std::string::npos);
    server.replace(value, 5u, "true");
    write(cachedManifest, server);
    CHECK(!inspect_local_source(cachedManifest).review_ready());
    write(cachedManifest, originalManifest);
    const auto foreign = base / "foreign";
    write(foreign / "protected.cpp", "untouched foreign source");
    std::error_code linkError;
    fs::create_directory_symlink(foreign, staged.source_root / "link", linkError);
    if (!linkError)
    {
        auto linked = originalManifest;
        linked.replace(field, std::string{"\"CMakeLists.txt\""}.size(), "\"link/protected.cpp\"");
        write(cachedManifest, linked);
        CHECK(inspect_local_source(cachedManifest).code == Code::unsafe_path);
        fs::remove(staged.source_root / "link");
        write(cachedManifest, originalManifest);
    }
    std::size_t longestPath{};
    for (const auto& file : inspection.files)
        longestPath = (std::max)(longestPath,
            (staged.receipt_path.parent_path() / "source" / file.relative_path).native().size());
    write(base / "proof.txt", std::string{"source_only=true\nsymlink_case="}
        + (linkError ? "unsupported\n" : "passed\n")
        + "longest_staged_path=" + std::to_string(longestPath) + "\n");
    CHECK(read(foreign / "protected.cpp") == "untouched foreign source");
    CHECK(verify_staged_source(inspection, cache).source_staged());
    const auto linkedProject = base / "linked project";
    write(linkedProject / "project.epoch.json", "{}\n");
    std::error_code projectLinkError;
    fs::create_directory_symlink(foreign, linkedProject / "Packages", projectLinkError);
    if (!projectLinkError)
    {
        CHECK(attach_project_source(inspection, staged, inspection.content_hash, linkedProject).code == Code::unsafe_path);
        CHECK(read(foreign / "protected.cpp") == "untouched foreign source");
        fs::remove(linkedProject / "Packages");
    }
    // Reopen uses the project copy even when cache/original review is invalid.
    // Only the synthetic fixture's original manifest is changed here.
    write(staged.receipt_path, "Cache no longer has a valid source receipt.");
    if (argc <= 1) write(manifest, "Original synthetic source is no longer valid.");
    CHECK(load_project_attachment(project, "research_planetary_terrain").source_attached());
    CHECK(read(project / attached.relative_receipt_path) == dependencyReceipt);
    return 0;
    }
    catch (const std::exception& error)
    {
        write(base / "exception.txt", error.what());
        return 222;
    }
}
