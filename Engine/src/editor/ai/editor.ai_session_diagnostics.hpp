// EpochEngine — local, bounded AI iteration evidence. No third-party ZIP runtime.
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
// Epoch Editor: keep editor state and source changes within their owning workflows.
#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace epochengine::editor_ai_session_diagnostics
{
    namespace fs = std::filesystem;
    inline std::mutex evidence_mutex{};

    [[nodiscard]] inline std::string safe_line(std::string_view text, std::size_t limit = 1500)
    {
        std::string result;
        result.reserve((std::min)(text.size(), limit));
        for (char ch : text.substr(0, limit))
            result += ch == '\n' || ch == '\r' || ch == '\t' ? ' ' : ch;
        return result;
    }

    // Never materialize logs before the isolated workspace has been admitted.
    inline bool record(const fs::path& workspace, std::string_view phase,
        std::string_view transition, std::string_view summary = {}) noexcept
    {
        try
        {
            const fs::path logs = workspace / "logs";
            std::error_code ec;
            if (fs::is_symlink(workspace, ec) || ec
                || !fs::is_directory(logs, ec) || ec || fs::is_symlink(logs, ec)) return false;
            std::scoped_lock lock(evidence_mutex);
            std::ofstream out(logs / "ai_iteration_timeline.tsv", std::ios::app | std::ios::binary);
            if (!out) return false;
            const auto unixMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            out << unixMs << '\t' << safe_line(phase, 100) << '\t'
                << safe_line(transition, 80) << '\t' << safe_line(summary) << '\n';
            return out.good();
        }
        catch (...) { return false; }
    }

    namespace detail
    {
        inline void u16(std::ofstream& out, std::uint16_t value)
        {
            std::array<char, 2> bytes{char(value & 255u), char((value >> 8) & 255u)};
            out.write(bytes.data(), 2);
        }
        inline void u32(std::ofstream& out, std::uint32_t value)
        {
            for (unsigned shift = 0; shift < 32; shift += 8)
                out.put(static_cast<char>((value >> shift) & 255u));
        }
        struct Entry
        {
            fs::path file;
            std::string name;
            std::uint32_t size{};
            std::uint32_t crc{};
            std::uint32_t offset{};
        };
        // The log may continue growing while exported. Read precisely the size
        // admitted at collection time and compute CRC over the same bytes that
        // are actually written. A truncated or unreadable source aborts publish.
        [[nodiscard]] inline bool copy(std::ofstream& out, const fs::path& file,
            std::uint32_t size, std::uint32_t& resultCrc)
        {
            std::ifstream in(file, std::ios::binary);
            std::array<char, 32 * 1024> block{};
            if (!in) return false;
            std::uint32_t crc = 0xFFFFFFFFu;
            std::uint32_t remaining = size;
            while (remaining != 0)
            {
                const auto count = static_cast<std::streamsize>((std::min)(
                    static_cast<std::uint32_t>(block.size()), remaining));
                in.read(block.data(), count);
                if (in.gcount() != count) return false;
                for (std::streamsize i = 0; i < count; ++i)
                {
                    crc ^= static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);
                    for (unsigned bit = 0; bit < 8; ++bit)
                        crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
                }
                out.write(block.data(), count);
                if (!out) return false;
                remaining -= static_cast<std::uint32_t>(count);
            }
            resultCrc = ~crc;
            return true;
        }
        inline void local(std::ofstream& out, const Entry& e)
        {
            u32(out, 0x04034b50u); u16(out, 20); u16(out, 0); u16(out, 0);
            u16(out, 0); u16(out, 0); u32(out, e.crc); u32(out, e.size);
            u32(out, e.size); u16(out, static_cast<std::uint16_t>(e.name.size()));
            u16(out, 0); out.write(e.name.data(), e.name.size());
        }
        inline void central(std::ofstream& out, const Entry& e)
        {
            u32(out, 0x02014b50u); u16(out, 20); u16(out, 20);
            u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 0);
            u32(out, e.crc); u32(out, e.size); u32(out, e.size);
            u16(out, static_cast<std::uint16_t>(e.name.size()));
            u16(out, 0); u16(out, 0); u16(out, 0); u16(out, 0);
            u32(out, 0); u32(out, e.offset); out.write(e.name.data(), e.name.size());
        }
    }

    // Engine-wide diagnostics live next to the checkout, never in an
    // unmaterialized disposable candidate. All paths are host-selected.
    [[nodiscard]] inline fs::path fallback_engine_root() noexcept
    {
        std::error_code error{};
        return fs::current_path(error); // Only used until a verified source root is available.
    }

    [[nodiscard]] inline bool record_engine(const fs::path& engineRoot,
        std::string_view phase, std::string_view transition,
        std::string_view summary = {}) noexcept
    {
        try
        {
            std::error_code ec;
            if (!engineRoot.is_absolute() || fs::is_symlink(engineRoot, ec) || ec
                || !fs::is_directory(engineRoot, ec) || ec) return false;
            const auto logs = engineRoot / "build" / "logs";
            if (fs::weakly_canonical(logs, ec) != logs.lexically_normal() || ec) return false;
            const bool logsExist = fs::exists(logs, ec);
            if (ec) return false;
            if (logsExist && (fs::is_symlink(logs, ec) || ec)) return false;
            if (!logsExist && !fs::create_directories(logs, ec)) return false;
            if (ec || !fs::is_directory(logs, ec) || ec) return false;
            std::scoped_lock lock(evidence_mutex);
            std::ofstream out(logs / "epoch_engine_timeline.tsv", std::ios::binary | std::ios::app);
            if (!out) return false;
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            out << ms << '\t' << safe_line(phase, 100) << '\t'
                << safe_line(transition, 80) << '\t' << safe_line(summary) << '\n';
            return out.good();
        }
        catch (...) { return false; }
    }

    // The archive is published under engineRoot/build/logs, independent of whether
    // a candidate has been materialized. Workspace files are included only
    // when the caller has verified that the isolated copy completed.
    [[nodiscard]] inline std::optional<fs::path> bundle_engine(
        const fs::path& engineRoot, const fs::path& admittedWorkspace = {},
        const fs::path& runtimeLogs = {}) noexcept
    {
        try
        {
            std::error_code ec;
            if (!engineRoot.is_absolute() || fs::is_symlink(engineRoot, ec) || ec
                || !fs::is_directory(engineRoot, ec) || ec) return std::nullopt;
            const fs::path logs = engineRoot / "build" / "logs";
            if (fs::weakly_canonical(logs, ec) != logs.lexically_normal() || ec) return std::nullopt;
            const bool logsExist = fs::exists(logs, ec);
            if (ec) return std::nullopt;
            if (logsExist && (fs::is_symlink(logs, ec) || ec)) return std::nullopt;
            if (!logsExist && !fs::create_directories(logs, ec)) return std::nullopt;
            if (ec || !fs::is_directory(logs, ec) || ec) return std::nullopt;

            const bool hasCandidate = !admittedWorkspace.empty()
                && admittedWorkspace.is_absolute()
                && admittedWorkspace != engineRoot
                && !fs::is_symlink(admittedWorkspace, ec) && !ec
                && fs::is_directory(admittedWorkspace, ec) && !ec;
            ec.clear();
            const bool hasRuntimeLogs = !runtimeLogs.empty() && runtimeLogs.is_absolute()
                && runtimeLogs != logs && !fs::is_symlink(runtimeLogs, ec) && !ec
                && fs::is_directory(runtimeLogs, ec) && !ec;
            std::scoped_lock lock(evidence_mutex);
            {
                std::ofstream manifest(logs / "epoch_diagnostic_manifest.txt",
                    std::ios::binary | std::ios::trunc);
                if (manifest)
                {
                    manifest << "EPOCH_ENGINE_DIAGNOSTICS_V2\n";
                    manifest << "scope=engine_runtime_logs_and_admitted_candidate\n";
                    // This ZIP includes admitted candidate diagnostics, not its
                    // source files; never suggest the generated source is here.
                    manifest << "candidate_included=" << (hasCandidate ? "yes" : "no") << '\n';
                    manifest << "candidate_source_included=no\n";
                    // Diagnostics are not a candidate source export. Record
                    // its absolute location so the operator can find the
                    // actual files rather than trying a contract fixture path.
                    manifest << "candidate_workspace_path="
                        << (hasCandidate ? admittedWorkspace.generic_string() : "unavailable") << '\n';
                    const fs::path candidateExe = admittedWorkspace
                        / "x64" / "Release" / "EpochEditor.exe";
                    std::error_code executableCheck{};
                    const bool executablePresent = hasCandidate
                        && fs::is_regular_file(candidateExe, executableCheck)
                        && !executableCheck;
                    manifest << "candidate_release_executable_path="
                        << (executablePresent ? candidateExe.generic_string() : "unavailable") << '\n';
                    manifest << "candidate_source_location=on_disk_not_in_diagnostic_zip\n";
                    manifest << "candidate_diagnostics_included=" << (hasCandidate ? "yes" : "no") << '\n';
                    manifest << "candidate_missing_does_not_imply_success=yes\n";
                    manifest << "contains_live_source=no\n";
                    manifest << "runtime_logs_path="
                        << (hasRuntimeLogs ? runtimeLogs.generic_string() : "same_as_engine_or_unavailable") << '\n';
                    constexpr std::array names{
                        "epoch_engine_timeline.tsv", "epoch_resource_catalog.tsv", "ai_iteration_timeline.tsv",
                        "ai_source_release_build.output.log",
                        "ai_source_release_build.msbuild.log",
                        "ai_source_release_contract_test.output.log",
                        "ai_source_debug_build.output.log",
                        "ai_source_headless_build.output.log",
                        "candidate-visual-review.txt"};
                    for (const char* name : names)
                    {
                        std::error_code check{};
                        const bool enginePresent = fs::is_regular_file(logs / name, check) && !check;
                        bool candidatePresent = false;
                        if (hasCandidate)
                        {
                            check.clear();
                            candidatePresent = fs::is_regular_file(admittedWorkspace / "logs" / name, check) && !check;
                        }
                        manifest << name << '=' << (enginePresent || candidatePresent
                            ? "present" : "not_present") << '\n';
                    }
                }
            }
            std::vector<detail::Entry> entries;
            constexpr std::uintmax_t perFileLimit = 20u * 1024u * 1024u;
            constexpr std::uintmax_t aggregateLimit = 96u * 1024u * 1024u;
            std::uintmax_t total{};
            std::size_t visited{};
            const auto collect = [&](const fs::path& root, std::string prefix,
                const bool onlyDiagnostics) -> void
            {
                std::error_code walk{};
                if (fs::is_symlink(root, walk) || walk
                    || !fs::is_directory(root, walk) || walk) return;
                for (fs::recursive_directory_iterator it(root,
                    fs::directory_options::skip_permission_denied, walk), end;
                    !walk && it != end; it.increment(walk))
                {
                    if (++visited > 10'000u) break;
                    const auto& item = *it;
                    std::error_code itemError{};
                    if (item.is_symlink(itemError) || itemError)
                    { it.disable_recursion_pending(); continue; }
                    if (!item.is_regular_file(itemError) || itemError) continue;
                    auto relative = item.path().lexically_relative(root);
                    if (relative.empty()) continue;
                    const auto extension = item.path().extension().string();
                    if (extension == ".zip" || extension == ".pending") continue;
                    const bool diagnostic = extension == ".log" || extension == ".tlog"
                        || extension == ".binlog";
                    const bool auxiliary = extension == ".txt" || extension == ".jsonl"
                        || extension == ".png" || extension == ".tsv"
                        || extension == ".json" || extension == ".csv" || extension == ".xml";
                    if (!diagnostic && !auxiliary) continue;
                    if (onlyDiagnostics && !diagnostic)
                    {
                        bool inLogs = false;
                        for (const auto& component : relative)
                            inLogs = inLogs || component == "logs";
                        if (!inLogs) continue;
                    }
                    const auto size = item.file_size(itemError);
                    if (itemError || size > perFileLimit || total + size > aggregateLimit)
                        continue;
                    const auto name = prefix + relative.generic_string();
                    if (name.size() > 1024u || entries.size() >= 256u) break;
                    entries.push_back({item.path(), name, static_cast<std::uint32_t>(size)});
                    total += size;
                }
            };
            collect(logs, "engine/logs/", false);
            if (hasRuntimeLogs) collect(runtimeLogs, "runtime/logs/", false);
            if (hasCandidate) collect(admittedWorkspace, "session/", true);
            if (entries.empty()) return std::nullopt;
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
                { return a.name < b.name; });
            fs::path target;
            bool found = false;
            for (int i = 1; i <= 8; ++i)
            {
                std::ostringstream name;
                name << "epoch_diagnostics_" << std::setw(3) << std::setfill('0') << i << ".zip";
                const auto candidate = logs / name.str();
                ec.clear();
                if (!fs::exists(candidate, ec) && !ec)
                { target = candidate; found = true; break; }
            }
            if (!found) return std::nullopt;
            fs::path temporary = target;
            temporary += ".pending";
            ec.clear();
            if (fs::exists(temporary, ec) || ec) return std::nullopt;
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            if (!out) return std::nullopt;
            bool complete = true;
            for (auto& entry : entries)
            {
                const auto pos = out.tellp();
                if (pos < 0 || static_cast<std::uint64_t>(pos) > UINT32_MAX)
                { complete = false; break; }
                entry.offset = static_cast<std::uint32_t>(pos);
                detail::local(out, entry);
                if (!detail::copy(out, entry.file, entry.size, entry.crc))
                { complete = false; break; }
                const auto end = out.tellp();
                if (end < 0) { complete = false; break; }
                out.seekp(static_cast<std::streamoff>(entry.offset) + 14);
                detail::u32(out, entry.crc);
                out.seekp(end);
                if (!out) { complete = false; break; }
            }
            if (complete && out.good())
            {
                const auto position = out.tellp();
                if (position < 0 || static_cast<std::uint64_t>(position) > UINT32_MAX)
                    complete = false;
                else
                {
                    const auto centralStart = static_cast<std::uint32_t>(position);
                    for (const auto& entry : entries) detail::central(out, entry);
                    const auto end = out.tellp();
                    if (end < 0 || static_cast<std::uint64_t>(end) > UINT32_MAX)
                        complete = false;
                    else
                    {
                        const auto centralSize = static_cast<std::uint32_t>(end) - centralStart;
                        detail::u32(out, 0x06054b50u); detail::u16(out, 0); detail::u16(out, 0);
                        detail::u16(out, static_cast<std::uint16_t>(entries.size()));
                        detail::u16(out, static_cast<std::uint16_t>(entries.size()));
                        detail::u32(out, centralSize); detail::u32(out, centralStart);
                        detail::u16(out, 0);
                    }
                }
            }
            if (!complete) out.setstate(std::ios::badbit);
            out.flush();
            const bool succeeded = out.good();
            out.close();
            if (!succeeded) { fs::remove(temporary, ec); return std::nullopt; }
            ec.clear();
            fs::rename(temporary, target, ec);
            if (ec) { fs::remove(temporary, ec); return std::nullopt; }
            return target;
        }
        catch (...) { return std::nullopt; }
    }

    // Compatibility API for existing session-scoped tools. The editor now
    // exports to the engine-wide log directory via bundle_engine().
    [[nodiscard]] inline std::optional<fs::path> bundle(const fs::path& workspace) noexcept
    {
        return bundle_engine(workspace);
    }
}
