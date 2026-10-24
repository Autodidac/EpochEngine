// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
// Persistence contract for explicit Epoch-owned native resident model sets.
// This file is intentionally independent of RuntimeHost and the UI so both
// Save & Load Set and Load Saved Set use the same validated disk format.
#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace epochengine::ai::native_saved_set {
inline constexpr std::string_view header = "EPOCH_NATIVE_SET_1";
inline constexpr std::uintmax_t maximum_file_bytes = 1024u;

struct ReadResult final {
    std::vector<std::string> ids{};
    std::string error{};
    [[nodiscard]] explicit operator bool() const noexcept { return error.empty() && !ids.empty(); }
};

[[nodiscard]] inline bool valid_id(std::string_view id) noexcept {
    return !id.empty() && id.size() <= 160u &&
        std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        });
}

[[nodiscard]] inline bool valid_ids(std::span<const std::string> ids) noexcept {
    if (ids.empty() || ids.size() > 3u) return false;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (!valid_id(ids[i])) return false;
        for (std::size_t j = i + 1; j < ids.size(); ++j)
            if (ids[i] == ids[j]) return false;
    }
    return true;
}

[[nodiscard]] inline ReadResult read(const std::filesystem::path& path) noexcept {
    try {
        std::error_code ec{};
        const auto bytes = std::filesystem::file_size(path, ec);
        if (ec) return {{}, "No readable saved resident set. Use Save & Load Set once."};
        if (bytes == 0u || bytes > maximum_file_bytes)
            return {{}, "Saved resident set is empty or oversized. Save it again."};
        if (std::filesystem::is_symlink(path, ec) || ec)
            return {{}, "Saved resident set is not a permitted regular file."};

        // Binary mode with explicit CRLF handling works identically on Windows
        // and POSIX, and does not depend on C runtime text translation.
        std::ifstream in{path, std::ios::binary};
        if (!in) return {{}, "Saved resident set could not be opened."};
        std::string line{};
        auto next_line = [&]() -> bool {
            if (!std::getline(in, line)) return false;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        };
        if (!next_line() || line != header)
            return {{}, "Saved resident set has an invalid header. Save it again."};
        ReadResult result{};
        while (next_line()) {
            if (!valid_id(line) || result.ids.size() >= 3u ||
                std::find(result.ids.begin(), result.ids.end(), line) != result.ids.end())
                return {{}, "Saved resident set contains invalid, duplicate or too many model IDs."};
            result.ids.push_back(std::move(line));
        }
        if (in.bad()) return {{}, "I/O error while reading saved resident set."};
        if (result.ids.empty()) return {{}, "Saved resident set has no model IDs. Save it again."};
        return result;
    } catch (const std::exception& error) {
        return {{}, std::string{"Could not read saved resident set: "} + error.what()};
    } catch (...) {
        return {{}, "Could not read saved resident set."};
    }
}

// Empty return indicates success; otherwise the exact rejection reason.
[[nodiscard]] inline std::string save(const std::filesystem::path& path,
                                      std::span<const std::string> ids) noexcept {
    if (!valid_ids(ids)) return "Choose one to three distinct registered models.";
    try {
        std::filesystem::create_directories(path.parent_path());
        const auto staging = std::filesystem::path{path.string() + ".new"};
        {
            std::ofstream out{staging, std::ios::binary | std::ios::trunc};
            if (!out) return "Could not open staged resident-set file for writing.";
            out << header << '\n';
            for (const auto& id : ids) out << id << '\n';
            out.flush();
            if (!out) return "Could not finish writing staged resident-set file.";
        }
        std::error_code ec{};
        std::filesystem::rename(staging, path, ec);
        if (ec) {
            // Windows rename may refuse a destination already present.
            // Keep previous behavior for replacement of an existing set.
            std::error_code cleanup{};
            std::filesystem::remove(path, cleanup);
            ec.clear();
            std::filesystem::rename(staging, path, ec);
        }
        if (ec) return "Could not replace saved resident set: " + ec.message();
        return {};
    } catch (const std::exception& error) {
        return std::string{"Could not save resident set: "} + error.what();
    } catch (...) {
        return "Could not save resident set.";
    }
}
} // namespace epochengine::ai::native_saved_set
