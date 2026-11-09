// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>
PRODUCTION_SHA
PRODUCTION_DISTRIBUTION
PRODUCTION_POLICIES
namespace core = epochengine::core;
namespace fs = std::filesystem;
bool PUBLIC_DISTRIBUTION = true;
std::string EDITION_ID = "epoch.public.github.v1";
constexpr auto GITHUB_BASE = "https://github.com/";
constexpr auto PROJECT_RELEASE_SIGNING_ALGORITHM = "Ed25519";
constexpr auto PROJECT_RELEASE_SIGNING_KEY_ID = "test-pinned-key";
constexpr auto PROJECT_RELEASE_INTEGRITY_SCHEMA = "test-schema";
bool crypto_ok = true, payload_ok = true;
int metadata_calls{}, payload_calls{}, crypto_calls{};
std::string metadata, payload = "exact source archive bytes";
fs::path fixture_root;
void log_error(const std::string&) {}
void log_info(const std::string&) {}
bool verify_ed25519(const std::string&, const std::string& signature)
{ ++crypto_calls; return crypto_ok && signature == "accepted"; }
std::string PROJECT_RELEASE_INTEGRITY_URL() { return public_policy::PROJECT_RELEASE_INTEGRITY_URL(); }
std::string PROJECT_SOURCE_URL() { return public_policy::PROJECT_SOURCE_URL(); }
std::string SOURCE_VERSION_FILE_NAME() { return "Engine/modules/epoch.version.ixx"; }
bool PROJECT_RELEASE_DOWNLOAD_ALLOWED(std::string_view url)
{ return epochengine::updater::distribution::release_download_allowed(url); }
bool PROJECT_SOURCE_EDITION_MATCHES(std::string_view marker)
{ return epochengine::updater::distribution::marker_matches(marker); }
void write(const fs::path& path, const std::string& bytes)
{ fs::create_directories(path.parent_path()); std::ofstream(path, std::ios::binary) << bytes; }
std::string read_text_file(const fs::path& path)
{ std::ifstream input(path, std::ios::binary); return {std::istreambuf_iterator<char>{input}, {}}; }
fs::path make_temp_download_path(const char*) { return fixture_root / "metadata.json"; }
bool download_file(const std::string& url, const std::string& path)
{ ++metadata_calls; if (url != PROJECT_RELEASE_INTEGRITY_URL()) return false; write(path, metadata); return true; }
PRODUCTION_VERSION
PRODUCTION_PARSER
PRODUCTION_ADMISSION
PRODUCTION_TYPES
PRODUCTION_SOURCE
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
void source_tree(const fs::path& root, const std::string& version = "0.90.185")
{
    write(root / "Engine.sln", "fixture"); write(root / "Engine/vcpkg.json", "{}");
    write(root / "Engine/include/epoch.edition.hpp", "#define EPOCH_SOURCE_EDITION_PUBLIC 1\n");
    write(root / SOURCE_VERSION_FILE_NAME(), version); write(root / "source-projection.json", "{}");
}
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
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
    CHECK(public_policy::PROJECT_SOURCE_URL() == public_policy::PROJECT_RELEASE_INTEGRITY_URL());
    CHECK(public_policy::PROJECT_SOURCE_CHECKSUM_URL().empty());
    CHECK(development_policy::PROJECT_SOURCE_URL().empty());
    CHECK(!public_policy::PROJECT_SOURCE_CHANNEL_ALLOWED("", ""));
    CHECK(!development_policy::PROJECT_SOURCE_CHANNEL_ALLOWED(public_policy::PROJECT_SOURCE_URL(), ""));
    CHECK(development_policy::PROJECT_SOURCE_CHANNEL_ALLOWED("", ""));
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
    const auto next = resolve_public_source_archive();
    CHECK(next.found && next.version == "0.91.02" && next.url == next_url);
    metadata = document("[" + release("0.91.2", next_asset) + "]");
    CHECK(!resolve_public_source_archive().found);
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
    return 0;
}
catch (...) { return 250; }
