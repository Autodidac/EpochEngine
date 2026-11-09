// Epoch Engine - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
// Immutable native inputs only. This binding grants no shared mutable state
// and no process/GPU ownership; the native manager owns the separate OS lease.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace epochengine::ai::native_shared_inputs {
inline constexpr auto cache_variable = "EPOCH_AI_SHARED_MODEL_CACHE_ROOT";
inline constexpr auto directories_variable = "EPOCH_AI_SHARED_RUNTIME_DLL_DIRS";
inline constexpr auto disabled_variable = "EPOCH_AI_SHARED_NATIVE_INPUTS_DISABLED";

[[nodiscard]] inline std::optional<std::string> environment(const char* name) {
#ifdef _WIN32
    char* bytes = nullptr; std::size_t size{};
    if (_dupenv_s(&bytes, &size, name) != 0) throw std::runtime_error("Cannot read native input binding");
    if (!bytes) return std::nullopt;
    std::string value{bytes}; std::free(bytes); return value;
#else
    if (const auto* value = std::getenv(name)) return std::string{value};
    return std::nullopt;
#endif
}
[[nodiscard]] inline std::string utf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
[[nodiscard]] inline std::filesystem::path path_from_utf8(std::string_view bytes) {
    return std::filesystem::path{std::u8string{
        reinterpret_cast<const char8_t*>(bytes.data()), bytes.size()}};
}
[[nodiscard]] inline bool same_path(const std::filesystem::path& a, const std::filesystem::path& b) {
    auto left = a.native(), right = b.native();
#ifdef _WIN32
    const auto lower = [](auto c) { return static_cast<decltype(c)>(c >= L'A' && c <= L'Z' ? c + (L'a' - L'A') : c); };
    std::transform(left.begin(), left.end(), left.begin(), lower);
    std::transform(right.begin(), right.end(), right.begin(), lower);
#endif
    return left == right;
}
// Canonical paths refuse redirected ancestors, relative paths and missing
// inputs. Revalidate at use as well; this is admission, not OS confinement.
[[nodiscard]] inline std::filesystem::path validate_path(
    const std::filesystem::path& path, bool directory, bool allow_missing_directory = false) {
    const auto text = utf8(path);
    if (text.empty() || text.size() > 4096u || text.find('\0') != text.npos
        || text.find_first_of("\r\n;") != text.npos || !path.is_absolute()
        || path == path.root_path()) throw std::runtime_error("Invalid shared native input path");
    for (const auto& part : path) if (part == "..")
        throw std::runtime_error("Parent traversal in shared native input path");
    std::error_code error;
    const auto normalized = path.lexically_normal();
    if (directory && allow_missing_directory && !std::filesystem::exists(path, error) && !error) {
        auto ancestor = path.parent_path();
        while (!ancestor.empty() && !std::filesystem::exists(ancestor, error) && !error)
            ancestor = ancestor.parent_path();
        if (error || ancestor.empty()) throw std::runtime_error("Shared native input ancestor is unavailable");
        if (ancestor != ancestor.root_path()) (void)validate_path(ancestor, true);
        const auto resolved = std::filesystem::weakly_canonical(path, error);
        if (error || !same_path(normalized, resolved))
            throw std::runtime_error("Shared native input ancestor is redirected");
        return resolved;
    }
    const auto canonical = std::filesystem::canonical(path, error);
    if (error || !same_path(normalized, canonical))
        throw std::runtime_error("Shared native input is missing or redirected");
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || (directory ? !std::filesystem::is_directory(status)
                           : !std::filesystem::is_regular_file(status)))
        throw std::runtime_error("Shared native input has an invalid file type");
    if (!directory && (std::filesystem::file_size(path, error) == 0u || error))
        throw std::runtime_error("Shared native executable is empty or unreadable");
    return canonical;
}
[[nodiscard]] inline std::vector<std::filesystem::path> parse_directories(std::string_view value) {
    std::vector<std::filesystem::path> result;
    if (value.empty()) return result;
    std::size_t first{};
    while (first < value.size()) {
        const auto end = value.find(';', first);
        const auto part = value.substr(first, end == value.npos ? value.size() - first : end - first);
        if (part.empty() || result.size() >= 8u) throw std::runtime_error("Invalid native SDK directory binding");
        auto path = validate_path(path_from_utf8(part), true);
        if (std::ranges::none_of(result, [&](const auto& existing) { return same_path(existing, path); }))
            result.push_back(std::move(path));
        if (end == value.npos) break;
        first = end + 1u;
        if (first == value.size()) throw std::runtime_error("Empty native SDK directory binding");
    }
    return result;
}
[[nodiscard]] inline std::string encode_directories(const std::vector<std::filesystem::path>& paths) {
    if (paths.size() > 8u) throw std::runtime_error("Too many native SDK directories");
    std::string result;
    for (const auto& path : paths) {
        const auto admitted = validate_path(path, true);
        if (!result.empty()) result += ';';
        result += utf8(admitted);
    }
    return result;
}
[[nodiscard]] inline std::vector<std::pair<std::string, std::string>> runtime_search_environment(
    const std::vector<std::filesystem::path>& directories) {
    std::vector<std::pair<std::string, std::string>> result;
#ifdef _WIN32
    auto search = encode_directories(directories);
    const auto system_root = environment("SystemRoot");
    if (!system_root) throw std::runtime_error("Native SDK system directory is unavailable");
    const auto system = validate_path(path_from_utf8(*system_root) / "System32", true);
    if (!search.empty()) search += ';';
    search += utf8(system);
    result.emplace_back("PATH", std::move(search));
#else
    std::string search;
    for (const auto& directory : directories) {
        const auto path = validate_path(directory, true);
        const auto text = utf8(path);
        if (text.find(':') != text.npos) throw std::runtime_error("Invalid native library directory");
        if (!search.empty()) search += ':';
        search += text;
    }
    result.emplace_back("LD_LIBRARY_PATH", std::move(search));
#endif
    return result;
}
struct Binding final {
    bool bound{};
    bool disabled{};
    std::filesystem::path weights_root, executable;
    std::vector<std::filesystem::path> dependency_directories;
};
[[nodiscard]] inline Binding from_environment() {
    Binding binding;
    const auto cache = environment(cache_variable);
    const auto disabled = environment(disabled_variable);
    const auto directories = environment(directories_variable);
    if (!cache) {
        if (disabled || directories) throw std::runtime_error("Incomplete shared native input binding");
        return binding;
    }
    binding.bound = true;
    if (disabled) {
        if (*disabled != "0" && *disabled != "1") throw std::runtime_error("Invalid shared native disabled binding");
        binding.disabled = *disabled == "1";
    }
    binding.weights_root = validate_path(path_from_utf8(*cache), true, binding.disabled);
    // A captured unavailable SDK is deliberately empty: catalog/model-free work
    // remains possible, but the loader may not discover/copy another runtime.
    if (const auto server = environment("EPOCH_AI_QWEN_FLASH_SERVER"); server && !server->empty())
        binding.executable = validate_path(path_from_utf8(*server), false);
    if (directories) binding.dependency_directories = parse_directories(*directories);
    return binding;
}
} // namespace epochengine::ai::native_shared_inputs
