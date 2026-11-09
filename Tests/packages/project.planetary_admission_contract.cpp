// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stop_token>
#include <string>
#include <string_view>
#include "../../Engine/include/epoch.edition.hpp"

import package.source_install;
import platform.child_process;
import project.planetary;

namespace
{
    namespace fs = std::filesystem;
    std::string read(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }
    void write(const fs::path& path, std::string_view text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }
}

#define CHECK(condition) do { if (!(condition)) { write(base / "failure.txt", std::to_string(__LINE__)); return __LINE__; } } while (false)

int main(int argc, char** argv)
{
    if (argc != 4) return 1;
    namespace planet = epochengine::project_planetary;
    namespace source = epochengine::package_source;
    namespace process = epochengine::platform::child_process;
    std::error_code pathError;
    const auto manifest = fs::canonical(argv[1], pathError);
    if (pathError) return 3;
    const auto base = fs::absolute(argv[2]);
    const auto staleRuntime = fs::canonical(argv[3], pathError);
    if (pathError || !fs::is_directory(staleRuntime)) return 3;
    if (fs::exists(base)) return 2;
    fs::create_directories(base);
    try
    {
        const auto review = source::inspect_local_source(manifest);
        if (!review.review_ready()) write(base / "review-failure.txt", review.message);
        CHECK(review.review_ready());
        const auto staged = source::stage_local_source(review, review.content_hash, base / "cache");
        CHECK(staged.source_staged());
        const auto project = base / "project";
        const auto foreign = base / "other-project";
        const std::string projectText{"{\"id\":\"same-id-does-not-transfer-a-job\"}\n"};
        write(project / "project.epoch.json", projectText);
        write(foreign / "project.epoch.json", projectText);
        const auto attached = source::attach_project_source(review, staged, review.content_hash, project);
        CHECK(attached.source_attached());
        CHECK(source::attach_project_source(review, staged, review.content_hash, foreign).source_attached());
        const auto admitted = planet::capture_source_generation(project, attached.content_hash);
        CHECK(admitted && planet::source_generation_is_current(project, *admitted));
        // Identical package bytes AND identical manifest IDs do not authorize
        // completion into a different project root.
        CHECK(!planet::source_generation_is_current(foreign, *admitted));
        auto wrongHash = *admitted;
        wrongHash.content_hash = "sha256:" + std::string(64u, '0');
        CHECK(!planet::source_generation_is_current(project, wrongHash));
        write(project / "project.epoch.json", "{\"id\":\"replacement-project\"}\n");
        CHECK(!planet::source_generation_is_current(project, *admitted));
        write(project / "project.epoch.json", projectText);
        CHECK(planet::source_generation_is_current(project, *admitted));
        std::stop_source stopped; stopped.request_stop();
        CHECK(!planet::source_generation_is_current(project, *admitted, stopped.get_token()));

        planet::Settings settings;
        CHECK(planet::save_settings(project, settings));
        const auto recipe = read(project / planet::recipe_relative_path);
        const auto dependency = project / attached.relative_receipt_path.parent_path();
        const auto removed = base / "removed-package";
        fs::rename(dependency, removed);
        CHECK(!planet::source_generation_is_current(project, *admitted));
        planet::StreamView view{.camera={0.0, 2500.0, 0.0}, .request_id=1};
        const auto missing = planet::stream_step(project, settings, view);
        CHECK(!missing.operation.ok && !missing.snapshot && missing.operation.build_log.empty());
        CHECK(!fs::exists(project / "build/planet-stream"));
        CHECK(!process::active_in_group("epoch.project.planet"));
        fs::rename(removed, dependency);
        CHECK(planet::source_generation_is_current(project, *admitted));

        const auto sourceReceipt = dependency / "source.receipt.json";
        const auto receiptBytes = read(sourceReceipt);
        CHECK(fs::remove(sourceReceipt));
        CHECK(!planet::source_generation_is_current(project, *admitted));
        write(sourceReceipt, receiptBytes);
        const auto cmake = project / attached.relative_source_root / "CMakeLists.txt";
        const auto cmakeBytes = read(cmake);
        write(cmake, cmakeBytes + "\n# unapproved replacement\n");
        CHECK(!planet::source_generation_is_current(project, *admitted));
        CHECK(!planet::stream_step(project, settings, view).operation.ok);
        CHECK(!fs::exists(project / "build/planet-stream"));
        write(cmake, cmakeBytes);
        CHECK(planet::source_generation_is_current(project, *admitted));

        // A genuinely reapproved new version is valid for a NEW request only.
        const auto replacement = base / "replacement-source";
        for (const auto& file : review.files)
            write(replacement / file.relative_path, read(review.repository_root / file.relative_path));
        const auto replacementCmake = replacement / review.source_path / "CMakeLists.txt";
        write(replacementCmake, read(replacementCmake) + "\n# independently approved replacement\n");
        const auto newReview = source::inspect_local_source(replacement / review.source_path / "package.epoch-package.json");
        CHECK(newReview.review_ready() && newReview.content_hash != admitted->content_hash);
        const auto newStage = source::stage_local_source(newReview, newReview.content_hash, base / "cache");
        CHECK(newStage.source_staged());
        fs::rename(dependency, removed);
        const auto reattached = source::attach_project_source(newReview, newStage, newReview.content_hash, project);
        CHECK(reattached.source_attached());
        CHECK(!planet::source_generation_is_current(project, *admitted));
        const auto newAdmission = planet::capture_source_generation(project, reattached.content_hash);
        CHECK(newAdmission && planet::source_generation_is_current(project, *newAdmission));

        const auto unrelated = base / "unrelated";
        write(unrelated / "project.epoch.json", "{\"id\":\"unrelated\"}\n");
        CHECK(!planet::stream_step(unrelated, settings, view).operation.ok);
        CHECK(!fs::exists(unrelated / "build") && !fs::exists(unrelated / "cache"));

        const auto runtime = project / "Packages/.runtime/planetary_terrain"
            / (EPOCH_EDITION_PUBLIC ? "public" : "development") /
#if defined(NDEBUG)
            "Release";
#else
            "Debug";
#endif
        fs::create_directories(runtime);
        for (const auto& entry : fs::directory_iterator(staleRuntime))
            CHECK(fs::copy_file(entry.path(), runtime / entry.path().filename()));
        const auto runtimeReceipt = runtime / "provider.identity";
        auto stale = read(runtimeReceipt);
        const auto sourceAt = stale.find("source=");
        CHECK(sourceAt != std::string::npos);
        const auto end = stale.find('\n', sourceAt);
        stale.replace(sourceAt, end - sourceAt, "source=sha256:" + std::string(64u, '0'));
        write(runtimeReceipt, stale);
        const auto refused = planet::stream_step(project, settings, view);
        CHECK(!refused.operation.ok && !refused.snapshot && refused.operation.build_log.empty());
        CHECK(read(runtimeReceipt) == stale && read(project / planet::recipe_relative_path) == recipe);
        CHECK(!fs::exists(project / "build/planet-stream"));
        CHECK(!process::active_in_group("epoch.project.planet"));
        write(base / "result.txt", "PASS: completion binds project root, manifest and admitted source; removed, unapproved, reapproved, foreign, cancelled and stale-runtime states refuse without scene/settings writes or worker launch.\n");
    }
    catch (...) { write(base / "exception.txt", "Admission contract exception.\n"); return 255; }
    return 0;
}
