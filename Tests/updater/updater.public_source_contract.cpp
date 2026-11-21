// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "updater.runtime_identity.hpp"
PRODUCTION_SHA
PRODUCTION_DISTRIBUTION
PRODUCTION_POLICIES
namespace core = epochengine::core;
namespace fs = std::filesystem;
bool PUBLIC_DISTRIBUTION = true;
bool PROJECT_PACKAGED_UPDATES_ENABLED = public_policy::PROJECT_PACKAGED_UPDATES_ENABLED;
std::string EDITION_ID = "epoch.public.github.v1";
constexpr auto GITHUB_BASE = "https://github.com/";
constexpr auto GITHUB_API_BASE = "https://api.github.com/repos/";
constexpr auto PROJECT_RELEASE_SIGNING_ALGORITHM = "Ed25519";
constexpr auto PROJECT_RELEASE_SIGNING_KEY_ID = "test-pinned-key";
constexpr auto PROJECT_RELEASE_INTEGRITY_SCHEMA = "test-schema";
bool crypto_ok = true, payload_ok = true;
int metadata_calls{}, payload_calls{}, crypto_calls{};
std::string metadata, runtime_metadata, payload = "exact source archive bytes";
std::string tag = "0.90.185";
std::vector<std::string> requested_metadata;
std::vector<std::pair<std::string, std::string>> hosted_metadata;
std::string source_module(int minor, int revision)
{
    return "// Version example 0.98.03\r\n#  define EPOCH_VERSION_MAJOR_VALUE 0\r\n"
        "#  define EPOCH_VERSION_MINOR_VALUE " + std::to_string(minor)
        + "\r\n#  define EPOCH_VERSION_REVISION_VALUE " + std::to_string(revision) + "\r\n";
}
std::string announcement = source_module(90, 185);
fs::path fixture_root;
void log_error(const std::string&) {}
void log_info(const std::string&) {}
bool verify_ed25519(const std::string&, const std::string& signature)
{ ++crypto_calls; return crypto_ok && signature == "accepted"; }
std::string PROJECT_RELEASE_INTEGRITY_URL() { return public_policy::PROJECT_RELEASE_INTEGRITY_URL(); }
std::string PROJECT_SOURCE_URL() { return public_policy::PROJECT_SOURCE_URL(); }
std::string PROJECT_ACTION_RUNS_API_URL() { return public_policy::PROJECT_ACTION_RUNS_API_URL(); }
std::string SOURCE_VERSION_FILE_NAME() { return "Engine/modules/epoch.version.ixx"; }
std::string PACKAGED_BINARY_ASSET_PREFIX() { return "epoch_win10_x64_v"; }
std::string PACKAGED_BINARY_ASSET_SUFFIX() { return ".zip"; }
bool PROJECT_RELEASE_DOWNLOAD_ALLOWED(std::string_view url)
{ return PUBLIC_DISTRIBUTION ? epochengine::updater::distribution::release_download_allowed(url)
    : epochengine::updater::development_distribution::release_download_allowed(url); }
bool PROJECT_SOURCE_EDITION_MATCHES(std::string_view marker)
{ return PUBLIC_DISTRIBUTION ? epochengine::updater::distribution::marker_matches(marker)
    : epochengine::updater::development_distribution::marker_matches(marker); }
void write(const fs::path& path, const std::string& bytes)
{ fs::create_directories(path.parent_path()); std::ofstream(path, std::ios::binary) << bytes; }
std::string read_text_file(const fs::path& path)
{ std::ifstream input(path, std::ios::binary); return {std::istreambuf_iterator<char>{input}, {}}; }
fs::path make_temp_download_path(const char*) { return fixture_root / "metadata.json"; }
bool download_file(const std::string& url, const std::string& path)
{
    ++metadata_calls;
    requested_metadata.push_back(url);
    for (const auto& [hosted_url, response] : hosted_metadata)
        if (url == hosted_url) { write(path, response); return true; }
    if (url == PROJECT_SOURCE_URL()) { write(path, announcement); return true; }
    if (url == PROJECT_RELEASE_INTEGRITY_URL()) { write(path, runtime_metadata); return true; }
    if (url != "https://github.com/Autodidac/EpochEngine/releases/download/v" + tag + "/epoch-release-integrity.json")
        return false;
    write(path, metadata);
    return true;
}
PRODUCTION_VERSION
PRODUCTION_PARSER
PRODUCTION_ADMISSION
PRODUCTION_TYPES
namespace runtime_identity = epochengine::updater::runtime_identity;
PRODUCTION_PACKAGE
PRODUCTION_SOURCE
PRODUCTION_HOSTED_BUILD
PRODUCTION_EDITION
PRODUCTION_ROOT
PRODUCTION_ROOT_ADMISSION
namespace system_detail {
using ::ResolvedSourceArchive;
using ::resolve_public_source_archive;
using ::log_error;
bool verify_sha256(const fs::path& path, const std::string& expected)
{ return core::sha256::hex(core::sha256::hash(read_text_file(path))) == expected; }
}
bool download_update_file_atomic(const std::string&, const fs::path& path)
{ ++payload_calls; if (!payload_ok) return false; write(path, payload); return true; }
PRODUCTION_DOWNLOAD
PRODUCTION_WINDOWS
std::string quote(const std::string& value)
{
    std::string result = "\"";
    for (char ch : value) { if (ch == '\\' || ch == '"') result += '\\'; result += ch; }
    return result + '"';
}
std::string document(const std::string& body, const std::string& edition = "epoch.public.github.v1")
{
    const auto canonical = "{\"schema\":\"test-schema\",\"edition\":" + quote(edition)
        + ",\"releases\":" + body + "}";
    return "{\"canonical_payload\":" + quote(canonical)
        + ",\"signature\":{\"algorithm\":\"Ed25519\",\"key_id\":\"test-pinned-key\",\"value\":\"accepted\"}}";
}
std::string release(const std::string& version, const std::string& assets)
{ return "{\"version\":" + quote(version) + ",\"tag\":" + quote("v" + version) + ",\"assets\":[" + assets + "]}"; }
std::string asset(const std::string& url, const std::string& digest, const std::string& name = "epoch_source_v0.90.185.zip")
{ return "{\"name\":" + quote(name) + ",\"url\":" + quote(url) + ",\"sha256\":" + quote(digest) + "}"; }
void source_tree(const fs::path& root, const std::string& version = source_module(90, 185))
{
    write(root / "Engine.sln", "fixture"); write(root / "Engine/vcpkg.json", "{}");
    write(root / "Engine/include/epoch.edition.hpp", "#define EPOCH_SOURCE_EDITION_PUBLIC 1\n");
    write(root / SOURCE_VERSION_FILE_NAME(), version); write(root / "source-projection.json", "{}");
}
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "Check failed: %s\n", #condition); return 1; } } while (false)
int hosted_source_build_matrix()
{
    const std::string version = "0.91.14";
    const std::string exact(40, 'a'), unrelated(40, 'b'), annotation(40, 'c');
    const std::string api = std::string{GITHUB_API_BASE} + "Autodidac/EpochEngine";
    const auto workflow = PROJECT_ACTION_RUNS_API_URL();
    CHECK(workflow == api + "/actions/workflows/epoch-public.yml/runs");
    const auto ref = api + "/git/ref/tags/v" + version;
    const auto annotation_url = api + "/git/tags/" + annotation;
    const auto runs = workflow + "?head_sha=" + exact + "&per_page=5";
    const auto jobs = [&](int id) { return api + "/actions/runs/" + std::to_string(id) + "/jobs?per_page=10"; };
    const auto ref_body = [&](const std::string& type, const std::string& sha) {
        return "{\"ref\":\"refs/tags/v" + version + "\",\"object\":{\"type\":" + quote(type)
            + ",\"sha\":" + quote(sha) + "}}";
    };
    const auto run = [&](int id, const std::string& sha, const std::string& status = "completed") {
        return "{\"id\":" + std::to_string(id) + ",\"run_number\":" + std::to_string(id)
            + ",\"head_sha\":" + quote(sha)
            + ",\"status\":" + quote(status) + ",\"jobs_url\":\"https://example.com/jobs\"}";
    };
    const auto job = [&](const std::string& name, const std::string& sha,
        const std::string& status = "completed", const std::string& conclusion = "success") {
        return "{\"name\":" + quote(name) + ",\"head_sha\":" + quote(sha)
            + ",\"status\":" + quote(status) + ",\"conclusion\":" + quote(conclusion) + "}";
    };
    const auto run_list = [](const std::string& rows) { return "{\"workflow_runs\":[" + rows + "]}"; };
    const auto job_list = [](const std::string& rows) { return "{\"jobs\":[" + rows + "]}"; };
    const std::string windows = "windows-msvc";
    const std::string linux = "linux-clang-engine";

    requested_metadata.clear();
    CHECK(!check_platform_build_status(api + "/actions/runs", windows, version).ok);
    CHECK(!check_platform_build_status(workflow, windows, version + "/../main").ok);
    CHECK(requested_metadata.empty());

    hosted_metadata = {{ref, ref_body("commit", exact)}, {runs, run_list(run(90, unrelated))},
        {jobs(90), job_list(job(windows, unrelated))}};
    CHECK(!check_platform_build_status(workflow, windows, version).ok);
    CHECK(requested_metadata.size() == 2 && requested_metadata.back() == runs);

    hosted_metadata = {{ref, ref_body("tag", annotation)},
        {annotation_url, "{\"sha\":" + quote(annotation) + ",\"object\":{\"type\":\"commit\",\"sha\":"
            + quote(exact) + "}}"}, {runs, run_list(run(91, unrelated) + "," + run(85, exact))},
        {jobs(85), job_list(job(windows, exact) + "," + job(linux, exact))}};
    requested_metadata.clear();
    CHECK(check_platform_build_status(workflow, windows, version).ok);
    CHECK(requested_metadata.size() == 4 && requested_metadata.back() == jobs(85));
    CHECK(check_platform_build_status(workflow, linux, version).ok);

    for (const auto& misleading : {job("old-windows-msvc", exact), job(windows, unrelated),
                                  job(windows, exact, "completed", "failure")})
    {
        requested_metadata.clear();
        hosted_metadata = {{ref, ref_body("commit", exact)},
            {runs, run_list(run(84, exact) + "," + run(85, exact))},
            {jobs(85), job_list(misleading)}, {jobs(84), job_list(job(windows, exact))}};
        CHECK(!check_platform_build_status(workflow, windows, version).ok);
        CHECK(requested_metadata.size() == 3 && requested_metadata.back() == jobs(85));
    }
    hosted_metadata = {{ref, ref_body("commit", exact)},
        {runs, run_list(run(85, exact, "queued") + "," + run(84, exact))},
        {jobs(85), job_list("")}, {jobs(84), job_list(job(windows, exact))}};
    const auto waiting = check_platform_build_status(workflow, windows, version);
    CHECK(waiting.pending && !waiting.ok);

    PUBLIC_DISTRIBUTION = false;
    const auto site_runs = development_policy::PROJECT_ACTION_RUNS_API_URL();
    const auto site_jobs = site_runs + "/101/jobs";
    hosted_metadata = {{site_runs, run_list("{\"jobs_url\":" + quote(site_jobs) + "}")},
        {site_jobs, job_list("{\"name\":\"Site windows-msvc\",\"status\":\"completed\",\"conclusion\":\"success\"}")}};
    requested_metadata.clear();
    CHECK(check_platform_build_status(site_runs, windows, version).ok);
    CHECK(requested_metadata.size() == 2 && requested_metadata.front() == site_runs);
    PUBLIC_DISTRIBUTION = true;
    hosted_metadata.clear();
    return 0;
}

int release_route_matrix(const fs::path& fixtures)
{
    const std::string current = "0.91.13";
    const std::string newer = "0.91.14";
    const auto release_url = [](const std::string& version, const std::string& name) {
        return "https://github.com/Autodidac/EpochEngine/releases/download/v" + version + "/" + name;
    };
    const auto runtime_asset = [&](const std::string& version, const std::string& name) {
        const auto bytes = read_text_file(fixtures / ("public-runtime-" + version) / "runtime.zip");
        return asset(release_url(version, name), core::sha256::hex(core::sha256::hash(bytes)), name);
    };
    const auto current_runtime = release(current, runtime_asset(current, "epoch_win10_x64_v" + current + ".zip"));
    const auto newer_runtime = release(newer, runtime_asset(newer, "epoch_win10_x64_v" + newer + ".zip"));
    const auto newer_foreign_platform = release(newer, runtime_asset(newer, "epoch_linux_x64_v" + newer + ".tar.gz"));
    const auto source_name = "epoch_source_v" + newer + ".zip";
    const auto source_url = release_url(newer, source_name);
    payload = read_text_file(fixtures / ("public-source-" + newer + ".zip"));
    CHECK(payload.starts_with("PK"));
    const auto source_digest = core::sha256::hex(core::sha256::hash(payload));
    const auto newer_source = release(newer, asset(source_url, source_digest, source_name));

    announcement = source_module(91, 14);
    tag = newer;
    metadata = document("[" + newer_source + "]");
    requested_metadata.clear();
    const auto source = resolve_public_source_archive();
    CHECK(source.found && source.version == newer && source.url == source_url);
    CHECK(source.expected_sha256 == source_digest && requested_metadata.size() == 2);
    CHECK(requested_metadata.front() == PROJECT_SOURCE_URL());
    CHECK(requested_metadata.back() == release_url(newer, "epoch-release-integrity.json"));
    const auto destination = fixture_root / "newer-source-readback.zip";
    ResolvedSourceArchive result;
    CHECK(download_source_archive_with_fallbacks(PROJECT_SOURCE_URL(), destination, result));
    CHECK(read_text_file(destination) == payload);

    for (const auto& release_list : {current_runtime + "," + newer_runtime,
        newer_runtime + "," + current_runtime})
    {
        requested_metadata.clear();
        runtime_metadata = document("[" + release_list + "]");
        const auto packaged = resolve_packaged_release();
        CHECK(packaged.discovery_ok && packaged.found && packaged.latest_version == newer);
        CHECK(packaged.remote_version == newer && packaged.binary_name == "epoch_win10_x64_v" + newer + ".zip");
        CHECK(packaged.binary_url == release_url(newer, packaged.binary_name));
        CHECK(packaged.expected_sha256 == core::sha256::hex(core::sha256::hash(
            read_text_file(fixtures / ("public-runtime-" + newer) / "runtime.zip"))));
        CHECK(requested_metadata.size() == 1 && requested_metadata.front() == PROJECT_RELEASE_INTEGRITY_URL());
        const VersionCheckResult source_status{true, current, newer, compare_versions(current, newer) < 0};
        const VersionCheckResult packaged_status{true, current, packaged.remote_version,
            compare_versions(current, packaged.remote_version) < 0};
        CHECK(packaged_status.update_available && !prefer_source_release(source_status, packaged_status));
    }
    runtime_metadata = document("[" + current_runtime + "," + newer_foreign_platform + "," + newer_source + "]");
    const auto current_only = resolve_packaged_release();
    CHECK(current_only.found && current_only.remote_version == current && current_only.latest_version == newer);
    CHECK(prefer_source_release({true, current, newer, true}, {true, current, current, false}));
    runtime_metadata = document("[" + newer_source + "]");
    const auto source_only = resolve_packaged_release();
    CHECK(source_only.discovery_ok && !source_only.found && source_only.latest_version == newer);
    runtime_metadata = document("[" + newer_runtime + "]", "epoch.development.site.v1");
    CHECK(!resolve_packaged_release().discovery_ok);
    metadata = document("[" + newer_source + "]", "epoch.development.site.v1");
    CHECK(!resolve_public_source_archive().found);

    for (const auto& version : {current, newer})
    {
        const auto& other_version = version == current ? newer : current;
        PUBLIC_DISTRIBUTION = true;
        EDITION_ID = "epoch.public.github.v1";
        CHECK(runtime_payload_matches_edition(fixtures / ("public-runtime-" + version) / "extracted", version));
        CHECK(!runtime_payload_matches_edition(fixtures / ("development-runtime-" + version) / "extracted", version));
        CHECK(source_snapshot_matches(fixtures / ("public-source-" + version), version));
        CHECK(!source_snapshot_matches(fixtures / ("public-source-" + version), other_version));
        CHECK(!source_snapshot_matches(fixtures / ("public-source-" + version), ""));
        CHECK(!source_snapshot_matches(fixtures / ("development-source-" + version), version));
        PUBLIC_DISTRIBUTION = false;
        EDITION_ID = "epoch.development.site.v1";
        CHECK(runtime_payload_matches_edition(fixtures / ("development-runtime-" + version) / "extracted", version));
        CHECK(!runtime_payload_matches_edition(fixtures / ("public-runtime-" + version) / "extracted", version));
        CHECK(!runtime_payload_matches_edition(fixtures / ("development-runtime-" + version) / "extracted", other_version));
        CHECK(source_snapshot_matches(fixtures / ("development-source-" + version), version));
        CHECK(!source_snapshot_matches(fixtures / ("development-source-" + version), other_version));
        CHECK(!source_snapshot_matches(fixtures / ("development-source-" + version), ""));
        CHECK(!source_snapshot_matches(fixtures / ("public-source-" + version), version));
    }
    const auto owner_build = fixture_root / "owner-runtime-build";
    source_tree(owner_build, source_module(91, 14));
    write(owner_build / "Engine/include/epoch.edition.hpp",
        "#define EPOCH_SOURCE_EDITION_PUBLIC 0\n#define EPOCH_SOURCE_SELF_CODING 0\n");
    CHECK(!source_snapshot_matches(owner_build, newer));
    PROJECT_PACKAGED_UPDATES_ENABLED = development_policy::PROJECT_PACKAGED_UPDATES_ENABLED;
    requested_metadata.clear();
    CHECK(!resolve_packaged_release().found && !resolve_public_source_archive().found && requested_metadata.empty());
    return 0;
}
int main(int argc, char** argv) try
{
    if (argc != 2) return 1;
    fixture_root = fs::path(argv[1]) / "fixtures";
    const std::string url = "https://github.com/Autodidac/EpochEngine/releases/download/v0.90.185/epoch_source_v0.90.185.zip";
    const auto digest = core::sha256::hex(core::sha256::hash(payload));
    const auto good_asset = asset(url, digest);
    const auto good_document = document("[" + release("0.90.185", good_asset) + "]");
    metadata = good_document;
    const auto selected = resolve_public_source_archive();
    CHECK(selected.found && selected.url == url && selected.expected_sha256 == digest);
    CHECK(public_policy::PROJECT_SOURCE_URL() == public_policy::PROJECT_SOURCE_VERSION_URL());
    CHECK(requested_metadata.size() == 2 && requested_metadata[0] == PROJECT_SOURCE_URL());
    CHECK(requested_metadata[1] == "https://github.com/Autodidac/EpochEngine/releases/download/v0.90.185/epoch-release-integrity.json");
    CHECK(std::ranges::find(requested_metadata, PROJECT_RELEASE_INTEGRITY_URL()) == requested_metadata.end());
    CHECK(public_policy::PROJECT_SOURCE_CHECKSUM_URL().empty());
    CHECK(development_policy::PROJECT_SOURCE_URL().empty());
    CHECK(!public_policy::PROJECT_SOURCE_CHANNEL_ALLOWED("", ""));
    CHECK(!development_policy::PROJECT_SOURCE_CHANNEL_ALLOWED(public_policy::PROJECT_SOURCE_URL(), ""));
    CHECK(development_policy::PROJECT_SOURCE_CHANNEL_ALLOWED("", ""));
    const auto default_announcement = announcement;
    for (const auto& invalid : {std::string{}, std::string{"0.90.185"}, default_announcement
        + "#define EPOCH_VERSION_REVISION_VALUE 186\n"})
    {
        announcement = invalid;
        const auto requests_before = requested_metadata.size();
        CHECK(!resolve_public_source_archive().found && requested_metadata.size() == requests_before + 1);
    }
    announcement = source_module(90, 186);
    const auto requests_before = requested_metadata.size();
    CHECK(!resolve_public_source_archive().found && requested_metadata.size() == requests_before + 2);
    CHECK(requested_metadata.back().ends_with("/v0.90.186/epoch-release-integrity.json"));
    tag = "0.90.186";
    CHECK(!resolve_public_source_archive().found); // Signed manifest identifies a different version.
    tag = "0.90.185";
    announcement = default_announcement;
    for (const auto& bad_asset : {asset(url + "?redirect=1", digest), asset("https://example.com/source.zip", digest),
        asset("https://epoch.adamrushford.chatgpt.site/downloads/releases/source.zip", digest), asset(url, "bad"),
        asset(url, digest, "epoch-source-main.zip"), asset(url, digest, "epoch_source_v0.90.184.zip")})
    {
        metadata = document("[" + release("0.90.185", bad_asset) + "]");
        CHECK(!resolve_public_source_archive().found);
    }
    metadata = document("[" + release("0.90.185", good_asset + "," + good_asset) + "]");
    CHECK(!resolve_public_source_archive().found);
    metadata = document("[" + release("0.90.185", good_asset) + "," + release("0.90.186", "") + "]");
    CHECK(!resolve_public_source_archive().found); // Do not silently choose the older source.
    const std::string next_url = "https://github.com/Autodidac/EpochEngine/releases/download/v0.91.02/epoch_source_v0.91.02.zip";
    const auto next_asset = asset(next_url, digest, "epoch_source_v0.91.02.zip");
    metadata = document("[" + release("0.90.185", good_asset) + "," + release("0.91.02", next_asset) + "]");
    announcement = source_module(91, 2);
    tag = "0.91.02";
    const auto next = resolve_public_source_archive();
    CHECK(next.found && next.version == "0.91.02" && next.url == next_url);
    metadata = document("[" + release("0.91.2", next_asset) + "]");
    CHECK(!resolve_public_source_archive().found);
    tag = "0.90.185";
    announcement = default_announcement;
    metadata = document("[" + release("0.90.185", good_asset) + "]", "epoch.development.site.v1");
    const int before_foreign = crypto_calls;
    CHECK(!resolve_public_source_archive().found && crypto_calls == before_foreign);
    metadata = good_document; crypto_ok = false;
    CHECK(!resolve_public_source_archive().found);
    crypto_ok = true;
    ResolvedSourceArchive result;
    const auto destination = fixture_root / "source.zip";
    CHECK(download_source_archive_with_fallbacks(PROJECT_SOURCE_URL(), destination, result));
    CHECK(result.found && read_text_file(destination) == payload && payload_calls == 1);
    const auto original = payload; payload += "corruption";
    CHECK(!download_source_archive_with_fallbacks(PROJECT_SOURCE_URL(), destination, result));
    CHECK(!result.found && !fs::exists(destination));
    payload = original; metadata = "{}";
    const int before_payload = payload_calls;
    CHECK(!download_source_archive_with_fallbacks(PROJECT_SOURCE_URL(), destination, result));
    CHECK(payload_calls == before_payload);
    metadata = good_document; PUBLIC_DISTRIBUTION = false;
    const int before_private = metadata_calls;
    CHECK(!download_source_archive_with_fallbacks(PROJECT_SOURCE_URL(), destination, result));
    CHECK(metadata_calls == before_private);
    PUBLIC_DISTRIBUTION = true;
    CHECK(!download_source_archive_with_fallbacks(url, destination, result));
    const auto flat = fixture_root / "flat"; source_tree(flat);
    CHECK(source_snapshot_root(flat) == flat && source_snapshot_matches(flat, "0.90.185"));
    CHECK(!source_snapshot_matches(flat, "0.90.186"));
    const auto next_tree = fixture_root / "next";
    source_tree(next_tree, "// Example 0.90.185\n#define EPOCH_VERSION_MAJOR_VALUE 0\n"
        "#define EPOCH_VERSION_MINOR_VALUE 91\n#define EPOCH_VERSION_REVISION_VALUE 2\n");
    CHECK(source_snapshot_matches(next_tree, "0.91.02"));
    CHECK(!source_snapshot_matches(next_tree, "0.91.2"));
    const auto wrapped = fixture_root / "wrapped";
    fs::create_directories(wrapped / ".irrelevant"); source_tree(wrapped / "EpochEngine-tag");
    CHECK(source_snapshot_root(wrapped) == wrapped / "EpochEngine-tag");
    source_tree(wrapped / "other"); CHECK(source_snapshot_root(wrapped).empty());
    fs::create_directories(flat / "Engine/dep/EpochSelfCoding");
    CHECK(!source_snapshot_matches(flat, "0.90.185"));
    write(fixture_root.parent_path() / "admission.ps1", std::string{source_snapshot_admission_script()});
    write(fixture_root.parent_path() / "digest.ps1", std::string{source_archive_digest_script()});
    CHECK(hosted_source_build_matrix() == 0);
    return release_route_matrix(fixture_root.parent_path() / "release-fixtures");
}
catch (...) { return 250; }
