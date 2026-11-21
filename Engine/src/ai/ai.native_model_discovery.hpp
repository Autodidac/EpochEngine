// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include "ai.native_shared_inputs.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace epochengine::ai::native_discovery {
// Discovery preference only: never registers, admits or loads a model. The caller
// supplies its private edition/profile data directory, not the shared weights cache.
[[nodiscard]] inline std::filesystem::path read_library_root(const std::filesystem::path& file) noexcept try {
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size == 0 || size > 8192 || std::filesystem::is_symlink(file, ec) || ec) return {};
    std::ifstream in{file, std::ios::binary};
    std::string header, text, extra;
    if (!std::getline(in, header) || header != "EPOCH_MODEL_LIBRARY_1"
        || !std::getline(in, text) || std::getline(in, extra)
        || text.empty() || text.find('\0') != std::string::npos) return {};
    const auto root = std::filesystem::path{std::u8string{text.begin(), text.end()}};
    return root.is_absolute() ? root : std::filesystem::path{};
} catch (...) { return {}; }
template<class Replace>
[[nodiscard]] inline bool save_library_root(const std::filesystem::path& file,
    const std::filesystem::path& root, Replace&& replace) noexcept try {
    const auto utf8 = root.generic_u8string();
    const std::string text{utf8.begin(), utf8.end()};
    if (!root.is_absolute() || text.empty() || text.size() > 8000
        || text.find_first_of("\r\n") != std::string::npos || text.find('\0') != std::string::npos) return false;
    auto staged = file;
    staged += ".new";
    std::error_code ec;
    if (std::filesystem::is_symlink(file, ec) || std::filesystem::is_symlink(staged, ec)) return false;
    ec.clear();
    std::filesystem::create_directories(file.parent_path(), ec);
    if (ec) return false;
    {
        std::ofstream out{staged, std::ios::binary | std::ios::trunc};
        out << "EPOCH_MODEL_LIBRARY_1\n" << text << '\n';
        out.flush();
        if (!out) return false;
    }
    // Caller reuses the platform's atomic sibling replacement. Never remove or
    // truncate the previous preference to accommodate Windows rename behavior.
    const bool saved = read_library_root(staged) == root && replace(staged, file, ec);
    if (!saved) { std::error_code ignored; std::filesystem::remove(staged, ignored); }
    return saved;
} catch (...) { return false; }
struct ModelFileCandidate final {
    std::string id{};
    std::filesystem::path path{};
    std::uint64_t bytes{};
    std::uint32_t shard_count{1};
    bool complete{}; // File/header completeness only, not hash or backend admission.
    std::string status{};
};
struct ScanLimits final {
    std::size_t entries{20'000}, candidates{128};
    int depth{8};
    std::chrono::milliseconds elapsed{2'000};
};
struct ScanResult final {
    std::vector<ModelFileCandidate> models{};
    bool limited{}, cancelled{};
    std::string diagnostic{};
};
// Filesystem metadata and GGUF magic/version only. Explicit roots may resolve through a link; links to
// directories encountered below those roots are never recursively traversed.
[[nodiscard]] inline ScanResult scan(std::span<const std::filesystem::path> roots,
    std::stop_token stop = {}, ScanLimits limits = {}) {
    ScanResult result;
    const auto deadline = std::chrono::steady_clock::now() + limits.elapsed;
    std::size_t visited{};
    std::set<std::filesystem::path> seen;
    std::set<std::filesystem::path> scanned_roots;
    const auto interrupted = [&] {
        if (stop.stop_requested()) { result.cancelled = true; return true; }
        if (std::chrono::steady_clock::now() >= deadline) { result.limited = true; return true; }
        return false;
    };
    for (const auto& root : roots) {
        if (interrupted()) break;
        if (root.empty()) continue;
        std::error_code error;
        const auto resolved = std::filesystem::weakly_canonical(root, error);
        if (error || !std::filesystem::is_directory(resolved, error)) {
            result.diagnostic += "Folder unavailable: " + native_shared_inputs::utf8(root) + ". ";
            continue;
        }
        if (!scanned_roots.insert(resolved).second) continue;
        auto iterator = std::filesystem::recursive_directory_iterator(resolved,
            std::filesystem::directory_options::skip_permission_denied, error);
        const auto end = std::filesystem::recursive_directory_iterator{};
        for (; !error && iterator != end && !interrupted(); iterator.increment(error)) {
            if (visited >= limits.entries || result.models.size() >= limits.candidates) {
                result.limited = true;
                break;
            }
            ++visited;
            const auto& item = *iterator;
            std::error_code item_error;
            if (iterator.depth() >= limits.depth || item.is_symlink(item_error))
                iterator.disable_recursion_pending();
            item_error.clear();
            if (!item.is_regular_file(item_error) || item_error) continue;
            auto name = native_shared_inputs::utf8(item.path().filename());
            auto folded = name;
            std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) {
                return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
            });
            if (!folded.ends_with(".gguf") || folded.find("mmproj") != std::string::npos
                || folded.find("projector") != std::string::npos) continue;
            ModelFileCandidate candidate;
            candidate.path = item.path();
            // GGUF split convention: <name>-00001-of-00003.gguf.
            const auto split = folded.rfind("-of-");
            std::string prefix;
            if (split != std::string::npos && split >= 6u && folded[split - 6u] == '-'
                && split + 14u == folded.size()) {
                const auto index_text = std::string_view{folded}.substr(split - 5u, 5u);
                const auto count_text = std::string_view{folded}.substr(split + 4u, 5u);
                std::uint32_t index{}, count{};
                const auto a = std::from_chars(index_text.data(), index_text.data() + index_text.size(), index);
                const auto b = std::from_chars(count_text.data(), count_text.data() + count_text.size(), count);
                if (a.ec != std::errc{} || b.ec != std::errc{}
                    || a.ptr != index_text.data() + index_text.size()
                    || b.ptr != count_text.data() + count_text.size() || !index || index > count) continue;
                candidate.shard_count = count;
                prefix = name.substr(0, split - 5u);
                candidate.path = item.path().parent_path() / native_shared_inputs::path_from_utf8(prefix + "00001" + name.substr(split));
            }
            const auto identity = std::filesystem::weakly_canonical(candidate.path, item_error);
            if (item_error || !seen.insert(identity).second) continue;
            candidate.id = native_shared_inputs::utf8(candidate.path.stem());
            candidate.complete = candidate.shard_count <= 512u;
            if (candidate.complete) {
                for (std::uint32_t shard = 1; shard <= candidate.shard_count; ++shard) {
                    if (interrupted()) { candidate.complete = false; break; }
                    auto path = candidate.path;
                    if (!prefix.empty()) {
                        auto number = std::to_string(shard);
                        number.insert(0, 5u - number.size(), '0');
                        path = item.path().parent_path() / native_shared_inputs::path_from_utf8(prefix + number + name.substr(split));
                    }
                    const auto size = std::filesystem::file_size(path, item_error);
                    if (item_error || size < 24u || size > std::numeric_limits<std::uint64_t>::max() - candidate.bytes) {
                        candidate.complete = false; item_error.clear(); continue;
                    }
                    candidate.bytes += size;
                    std::array<unsigned char, 8> header{};
                    std::ifstream input(path, std::ios::binary);
                    input.read(reinterpret_cast<char*>(header.data()), header.size());
                    if (!input || header[0] != 'G' || header[1] != 'G' || header[2] != 'U' || header[3] != 'F'
                        || (header[4] != 2 && header[4] != 3) || header[5] || header[6] || header[7])
                        candidate.complete = false;
                }
            }
            candidate.status = candidate.complete ? "Files present; verify compatibility before registering"
                : result.cancelled || result.limited ? "File check interrupted; rescan required"
                : candidate.shard_count > 512u ? "Split count exceeds discovery limit"
                : "Missing, empty or invalid GGUF weights/shards; cannot load";
            if (folded.compare("model-skeleton.gguf") == 0) {
                candidate.complete = false;
                candidate.status = "Specialized Edge0 bundle; use candidate backend, not ordinary GGUF loading";
            } else if (folded.compare("ple-fp8.gguf") == 0) {
                candidate.complete = false;
                candidate.status = "Edge0 auxiliary PLE table; not a standalone model and cannot be registered separately";
            }
            result.models.push_back(std::move(candidate));
        }
        if (error) result.diagnostic += "Folder scan incomplete: " + error.message() + ". ";
        if (result.cancelled || result.limited) break;
    }
    std::sort(result.models.begin(), result.models.end(), [](const auto& a, const auto& b) {
        return a.path.native() < b.path.native();
    });
    return result;
}
}
