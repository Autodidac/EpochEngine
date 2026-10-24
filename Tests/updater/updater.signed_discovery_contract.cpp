// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
inline std::string PROJECT_RELEASE_SIGNING_ALGORITHM = "Ed25519";
inline std::string PROJECT_RELEASE_SIGNING_KEY_ID = "pinned-test-key";
inline std::string PROJECT_RELEASE_INTEGRITY_SCHEMA = "test-schema";
inline std::string EDITION_ID = "epoch.public.github.v1";
inline bool PUBLIC_DISTRIBUTION = true;
inline int crypto_calls = 0;
inline bool crypto_result = true;
void log_error(const std::string&) {}
void log_info(const std::string&) {}
bool verify_ed25519(const std::string&, const std::string&)
{ ++crypto_calls; return crypto_result; }
PRODUCTION_PARSER
PRODUCTION_ADMISSION
PRODUCTION_RELEASE_TYPES
inline bool PROJECT_PACKAGED_UPDATES_ENABLED = false;
inline unsigned download_calls = 0, temporary_calls = 0;
std::filesystem::path make_temp_download_path(const char*) { ++temporary_calls; return "unused.json"; }
std::string PROJECT_RELEASE_INTEGRITY_URL() { return "test-only"; }
bool download_file(const std::string&, const std::string&) { ++download_calls; return false; }
std::string read_text_file(const std::filesystem::path&) { return {}; }
std::string extract_version_string(const std::string& s) { return s; }
int compare_versions(const std::string&, const std::string&) { return 0; }
std::string PACKAGED_BINARY_ASSET_PREFIX() { return {}; }
std::string PACKAGED_BINARY_ASSET_SUFFIX() { return {}; }
bool starts_with_ascii(const std::string& s, const std::string& p) { return s.starts_with(p); }
bool ends_with_ascii(const std::string& s, const std::string& p) { return s.ends_with(p); }
bool PROJECT_RELEASE_DOWNLOAD_ALLOWED(const std::string&) { return false; }
namespace core::sha256 {
std::optional<std::string> from_hex(const std::string&) { return {}; }
std::string hex(const std::string& s) { return s; }
}
PRODUCTION_RESOLVER
CONFIG_POLICIES

std::string document(const std::string& edition)
{
    std::string payload = "{\"schema\":\"test-schema\"";
    if (!edition.empty()) payload += ",\"edition\":\"" + edition + "\"";
    payload += ",\"releases\":[]}";
    std::string escaped;
    for (const char c : payload) { if (c == '"' || c == '\\') escaped += '\\'; escaped += c; }
    return "{\"canonical_payload\":\"" + escaped
        + "\",\"signature\":{\"algorithm\":\"Ed25519\",\"key_id\":\"pinned-test-key\",\"value\":\"test\"}}";
}
int main()
{
    // Old v0.90.50 has no edition: Public rejects before attempting crypto.
    if (verified_release_integrity_payload(document("")) || crypto_calls) return 1;
    if (verified_release_integrity_payload(document("epoch.development.site.v1")) || crypto_calls) return 2;
    if (!verified_release_integrity_payload(document(EDITION_ID)) || crypto_calls != 1) return 3;
    crypto_result = false;
    if (verified_release_integrity_payload(document(EDITION_ID)) || crypto_calls != 2) return 4;
    PUBLIC_DISTRIBUTION = false; EDITION_ID = "epoch.development.site.v1"; crypto_result = true;
    if (verified_release_integrity_payload(document("")) || crypto_calls != 2) return 5;
    if (verified_release_integrity_payload(document("epoch.public.github.v1")) || crypto_calls != 2) return 6;
    if (!verified_release_integrity_payload(document(EDITION_ID)) || crypto_calls != 3) return 7;
    if (verified_release_integrity_payload("{}") || crypto_calls != 3) return 8;
    const auto disabled = resolve_packaged_release();
    if (disabled.found || disabled.discovery_ok || download_calls || temporary_calls) return 9;
    if (!development_policy::PROJECT_RELEASE_INTEGRITY_URL().empty()) return 10;
    if (public_policy::PROJECT_RELEASE_INTEGRITY_URL() !=
        "https://github.com/Autodidac/EpochEngine/releases/latest/download/epoch-release-integrity.json") return 11;
    PROJECT_PACKAGED_UPDATES_ENABLED = true;
    const auto public_result = resolve_packaged_release();
    if (public_result.found || download_calls != 1 || temporary_calls != 1) return 12;
    return 0;
}
