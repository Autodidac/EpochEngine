/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module ai.source_workspace;

namespace epochengine::ai::source_workspace
{
    std::vector<std::size_t> allocate_source_bytes(
        const std::vector<SourceDemand>& demands, std::size_t budget)
    {
        std::vector<std::size_t> allocated(demands.size());
        const auto fill = [&](auto eligible, auto target)
        {
            while (budget != 0u)
            {
                std::size_t pending{};
                for (std::size_t i = 0u; i < demands.size(); ++i)
                    if (eligible(i) && allocated[i] < target(i))
                        ++pending;
                if (pending == 0u) break;
                const auto share = (std::max)(std::size_t{1u}, budget / pending);
                for (std::size_t i = 0u; i < demands.size() && budget != 0u; ++i)
                {
                    if (!eligible(i) || allocated[i] >= target(i)) continue;
                    const auto amount = (std::min)({share, budget,
                        target(i) - allocated[i]});
                    allocated[i] += amount;
                    budget -= amount;
                }
            }
        };
        // Keep a useful minimum for every admitted path, without charging a
        // tiny header the same share as a large implementation file.
        for (std::size_t i = 0u; i < demands.size(); ++i)
        {
            const auto amount = (std::min)({demands[i].desired_bytes,
                std::size_t{1024u}, budget});
            allocated[i] = amount;
            budget -= amount;
        }
        const auto desired = [&](std::size_t i) { return demands[i].desired_bytes; };
        fill([&](std::size_t i) { return demands[i].current_request; }, [&](std::size_t i)
        {
            return (std::min)(demands[i].desired_bytes, demands[i].requested_bytes);
        });
        fill([&](std::size_t i) { return demands[i].mission_critical; }, [&](std::size_t i)
        {
            return (std::min)(demands[i].desired_bytes, demands[i].retained_bytes);
        });
        fill([&](std::size_t i) { return demands[i].desired_bytes <= 8192u; }, desired);
        fill([](std::size_t) { return true; }, [&](std::size_t i)
        {
            return (std::min)(demands[i].desired_bytes, demands[i].retained_bytes);
        });
        fill([&](std::size_t i) { return demands[i].current_request; }, desired);
        fill([](std::size_t) { return true; }, desired);
        return allocated;
    }

    WorkingSet select_working_set(const std::vector<std::string>& reviewed,
        const std::vector<std::string>& requested, std::string_view task)
    {
        WorkingSet result{};
        const auto filename = [](std::string_view path)
        {
            const auto slash = path.find_last_of("/\\");
            return path.substr(slash == std::string_view::npos ? 0u : slash + 1u);
        };
        const auto named = [&](const std::string& path)
        {
            if (task.find(path) != std::string_view::npos) return true;
            const auto name = filename(path);
            // A basename is useful only when it identifies one reviewed owner.
            return !name.empty() && task.find(name) != std::string_view::npos
                && std::ranges::count_if(reviewed, [&](const auto& other)
                    { return filename(other) == name; }) == 1;
        };
        const bool hasOwners = std::ranges::any_of(reviewed, named);
        const auto append = [&](const std::string& path)
        {
            if (std::ranges::find(reviewed, path) != reviewed.end()
                && std::ranges::find(result.resident_paths, path) == result.resident_paths.end())
                result.resident_paths.push_back(path);
        };
        for (const auto& path : requested) append(path);
        for (const auto& path : reviewed)
        {
            if (!hasOwners || named(path)
                || std::ranges::find(requested, path) != requested.end()) append(path);
            else result.archived_paths.push_back(path);
        }
        return result;
    }

    CompactedText compact_task_text(std::string_view text, std::size_t budget, bool recent_first)
    {
        CompactedText result{};
        if (text.size() <= budget) { result.text = text; return result; }
        constexpr std::string_view marker{"[Older records compacted; complete checkpoint/logs remain authoritative.]\n"};
        if (budget <= marker.size()) { result.protected_text_fits = false; return result; }
        // A handoff is indivisible: never lose END_NEXT_GENERATION or its tail
        // by using a UI preview/head substring as mission memory.
        std::size_t start = std::string_view::npos, end = std::string_view::npos;
        std::size_t protectedEnd{};
        for (std::size_t at{}; at < text.size();)
        {
            const auto newline = text.find('\n', at);
            const auto next = newline == std::string_view::npos ? text.size() : newline + 1u;
            auto line = text.substr(at, (newline == std::string_view::npos ? text.size() : newline) - at);
            if (line.ends_with('\r')) line.remove_suffix(1u);
            if (line == "NEXT_GENERATION") start = at;
            if (line == "END_NEXT_GENERATION" && start != std::string_view::npos)
            { end = at; protectedEnd = next; break; }
            at = next;
        }
        const auto handoff = start != std::string_view::npos && end != std::string_view::npos
            ? text.substr(start, protectedEnd - start) : std::string_view{};
        const auto handoffBytes = handoff.size() + (handoff.empty() ? 0u : 1u);
        if (handoffBytes >= budget - marker.size())
        { result.protected_text_fits = false; return result; }
        std::vector<std::string_view> records{};
        const auto task_headline = [](std::string_view line)
        {
            std::size_t digits{};
            while (digits < line.size() && line[digits] >= '0' && line[digits] <= '9') ++digits;
            return digits > 0u && digits + 1u < line.size()
                && (line[digits] == '.' || line[digits] == ')')
                && (line[digits + 1u] == ' ' || line[digits + 1u] == '\t');
        };
        for (std::size_t at{}; at < text.size();)
        {
            const auto newline = text.find('\n', at);
            auto next = newline == std::string_view::npos ? text.size() : newline + 1u;
            if (handoff.empty() || at < start || at >= protectedEnd)
            {
                // Wrapped task details belong to the same record as the task
                // title. Never keep a headline while dropping its dependency,
                // success criterion or warning from the following lines.
                if (task_headline(text.substr(at, next - at)))
                {
                    while (next < text.size() && (handoff.empty() || next != start))
                    {
                        const auto continuationNewline = text.find('\n', next);
                        const auto continuationEnd = continuationNewline == std::string_view::npos
                            ? text.size() : continuationNewline + 1u;
                        const auto continuation = text.substr(next, continuationEnd - next);
                        if (continuation == "\n" || continuation == "\r\n" || task_headline(continuation)) break;
                        next = continuationEnd;
                    }
                }
                records.push_back(text.substr(at, next - at));
            }
            at = next;
        }
        std::vector<bool> selected(records.size());
        auto available = budget - marker.size() - handoffBytes - 1u;
        const auto keep = [&](std::size_t i)
        {
            if (selected[i] || records[i].size() > available) return;
            // Duplicate observations add no knowledge to the next generation.
            for (std::size_t j{}; j < records.size(); ++j)
                if (selected[j] && records[j] == records[i]) return;
            selected[i] = true; available -= records[i].size();
        };
        // Preserve task headlines / trusted patch and failure records before
        // replaying read chatter. Remaining records retain original order.
        for (std::size_t i{}; i < records.size(); ++i)
            if (!records[i].empty() && (task_headline(records[i])
                || records[i].starts_with("PATCH") || records[i].starts_with("VALIDATION")
                || records[i].starts_with("BUILD") || records[i].starts_with("FAIL"))) keep(i);
        for (std::size_t count{}; count < records.size(); ++count)
            keep(recent_first ? records.size() - count - 1u : count);
        for (std::size_t i{}; i < records.size(); ++i)
            if (selected[i]) result.text += records[i]; else ++result.omitted_records;
        if (!result.text.empty() && !result.text.ends_with('\n')) result.text += '\n';
        result.text += marker;
        if (!handoff.empty()) { result.text += handoff; if (!result.text.ends_with('\n')) result.text += '\n'; }
        return result;
    }

    void Workspace::reset(std::string objective)
    {
        objective_ = std::move(objective);
        reviewed_.clear();
        searches_.clear();
        evidence_.clear();
        ranges_.clear();
        discovery_rounds_ = 0u;
        stagnant_rounds_ = 0u;
    }

    bool Workspace::merge(std::string path, std::size_t first_line, std::string query)
    {
        if (path.empty()) return false;
        const auto existing = std::ranges::find_if(reviewed_, [&](const ReviewedSlice& item)
        { return std::string_view{item.path}.compare(path) == 0; });
        if (existing == reviewed_.end())
        {
            reviewed_.push_back({std::move(path), first_line, std::move(query)});
            return true;
        }
        if (first_line != 0u || !query.empty())
        {
            const bool changed = existing->first_line != first_line || existing->query != query;
            existing->first_line = first_line;
            existing->query = std::move(query);
            return changed;
        }
        return false;
    }

    bool Workspace::remember_search(std::string signature)
    {
        if (signature.empty() || std::ranges::find(searches_, signature) != searches_.end())
            return false;
        searches_.push_back(std::move(signature));
        return true;
    }

    bool Workspace::contains_path(std::string_view path) const noexcept
    {
        return std::ranges::any_of(reviewed_, [path](const ReviewedSlice& item)
        { return std::string_view{item.path}.compare(path) == 0; });
    }

    bool Workspace::remember_evidence(std::string fingerprint)
    {
        // Bound history independently of model output and mission duration.
        if (fingerprint.empty() || fingerprint.size() > 2048u
            || evidence_.size() >= 4096u
            || std::ranges::find(evidence_, fingerprint) != evidence_.end())
            return false;
        evidence_.push_back(std::move(fingerprint));
        return true;
    }

    void Workspace::remember_range(VerifiedRange range)
    {
        if (!contains_path(range.path) || range.source_sha256.size() != 64u
            || range.byte_count == 0u
            || range.byte_offset > 8u * 1024u * 1024u
            || range.byte_count > 8u * 1024u * 1024u - range.byte_offset)
            return;
        std::erase_if(ranges_, [&](const VerifiedRange& prior)
        {
            return prior.path == range.path && prior.source_sha256 != range.source_sha256;
        });
        // Overlapping rereads are one verified union, not duplicate demand that
        // grows on every prompt and eventually crowds out the next useful read.
        for (;;)
        {
            const auto overlap = std::ranges::find_if(ranges_, [&](const VerifiedRange& prior)
            {
                return prior.path == range.path
                    && prior.byte_offset <= range.byte_offset + range.byte_count
                    && range.byte_offset <= prior.byte_offset + prior.byte_count;
            });
            if (overlap == ranges_.end()) break;
            const auto begin = (std::min)(range.byte_offset, overlap->byte_offset);
            const auto end = (std::max)(range.byte_offset + range.byte_count,
                overlap->byte_offset + overlap->byte_count);
            range.byte_offset = begin;
            range.byte_count = end - begin;
            ranges_.erase(overlap);
        }
        // A giant implementation can need many disjoint functions. The global
        // metadata ceiling bounds memory without discarding its fifth region.
        if (ranges_.size() >= 256u) ranges_.erase(ranges_.begin());
        ranges_.push_back(std::move(range));
    }

    const std::vector<VerifiedRange>& Workspace::ranges() const noexcept { return ranges_; }

    const std::vector<ReviewedSlice>& Workspace::reviewed() const noexcept { return reviewed_; }

    std::vector<std::string> Workspace::reviewed_paths() const
    {
        std::vector<std::string> paths{};
        paths.reserve(reviewed_.size());
        for (const auto& item : reviewed_) paths.push_back(item.path);
        return paths;
    }

    std::string Workspace::navigation_memory(std::size_t maximum_bytes) const
    {
        constexpr std::string_view begin{"EPOCH_CUMULATIVE_SOURCE_MEMORY_V1\n"};
        constexpr std::string_view end{"END_EPOCH_CUMULATIVE_SOURCE_MEMORY_V1\n"};
        if (maximum_bytes < begin.size() + end.size()) return {};
        std::string out{begin};
        for (const auto& item : reviewed_)
        {
            std::string line = "REVIEWED " + item.path;
            if (item.first_line != 0u) line += " line=" + std::to_string(item.first_line);
            if (!item.query.empty()) line += " query=" + item.query;
            line += "\n";
            if (out.size() + line.size() + end.size() > maximum_bytes) break;
            out += line;
        }
        for (auto item = ranges_.rbegin(); item != ranges_.rend(); ++item)
        {
            const auto line = "REMEMBERED_RANGE " + item->path
                + " byte_offset=" + std::to_string(item->byte_offset)
                + " byte_count=" + std::to_string(item->byte_count)
                + " source_sha256=" + item->source_sha256 + "\n";
            if (out.size() + line.size() + end.size() > maximum_bytes) break;
            out += line;
        }
        out += end;
        return out;
    }

    std::size_t Workspace::discovery_rounds() const noexcept { return discovery_rounds_; }
    void Workspace::note_discovery_round(bool added_evidence) noexcept
    {
        ++discovery_rounds_;
        stagnant_rounds_ = added_evidence ? 0u : stagnant_rounds_ + 1u;
    }
    std::size_t Workspace::stagnant_rounds() const noexcept { return stagnant_rounds_; }

    bool run_contract()
    {
        Workspace workspace{};
        workspace.reset("test");
        const bool navigation = workspace.merge("Engine/a.cpp")
            && !workspace.merge("Engine/a.cpp")
            && workspace.merge("Engine/b.cpp", 10u, "needle")
            && workspace.reviewed().size() == 2u
            && workspace.contains_path("Engine/b.cpp");
        const auto packed = allocate_source_bytes({{32768u, false},
            {2048u, false}, {32768u, true}}, 32768u);
        if (!navigation || packed.size() != 3u || packed[1] != 2048u
            || packed[0] != 1024u || packed[2] != 29696u
            || allocate_source_bytes({{1u, true}}, 0u) != std::vector<std::size_t>{0u}
            || !allocate_source_bytes({}, 1024u).empty())
            return false;
        // A large new implementation read must not evict the enum/palette bytes
        // the worker already needs. Pressure still stays inside the real budget.
        const auto retained = allocate_source_bytes({{36u * 1024u, false, 36u * 1024u},
            {256u * 1024u, true, 20u * 1024u}, {2048u, false, 2048u}}, 96u * 1024u);
        if (retained != std::vector<std::size_t>{36u * 1024u, 58u * 1024u, 2048u})
            return false;
        // Old broad source must not reduce a fresh function read to a handful
        // of lines. Three reads and one complete declaration keep useful room
        // even when retained content alone would consume the entire budget.
        const auto active = allocate_source_bytes({
            {256u * 1024u, false, 80u * 1024u},
            {256u * 1024u, true, 24u * 1024u, 24u * 1024u},
            {36u * 1024u, true, 4u * 1024u, 36u * 1024u}}, 96u * 1024u);
        if (active.size() != 3u || active[1] < 24u * 1024u
            || active[2] != 36u * 1024u
            || active[0] + active[1] + active[2] > 96u * 1024u)
            return false;
        std::vector<SourceDemand> smallHistory(12u, {8192u, false, 8192u});
        smallHistory.push_back({256u * 1024u, true, 0u, 16u * 1024u});
        const auto prioritized = allocate_source_bytes(smallHistory, 32u * 1024u);
        if (prioritized.back() < 16u * 1024u) return false;
        const auto pinned = allocate_source_bytes({
            {8192u, false, 8192u}, {36u * 1024u, false, 36u * 1024u, 0u, true},
            {128u * 1024u, true, 16u * 1024u, 8192u, true}}, 64u * 1024u);
        if (pinned[1] != 36u * 1024u || pinned[2] < 16u * 1024u) return false;
        const std::vector<std::string> owners{"Engine/modules/gui.engine.ixx",
            "Engine/src/epochgui/gui.engine.cpp", "Engine/src/editor/editor.application.cpp",
            "Engine/src/ai/ai.engine.cpp", "Engine/modules/core.context.ixx"};
        const auto working = select_working_set(owners, {owners[2]},
            "Add the palette in gui.engine.cpp and enum in gui.engine.ixx; keep the selector.");
        if (working.resident_paths != std::vector<std::string>{owners[2], owners[0], owners[1]}
            || working.archived_paths != std::vector<std::string>{owners[3], owners[4]}
            || select_working_set(owners, {}, "unlocated mission").resident_paths != owners
            || select_working_set({"Engine/a.cpp", "Engine/dep/a.cpp"}, {}, "a.cpp").resident_paths.size() != 2u)
            return false;
        const std::string handoff = "NEXT_GENERATION\r\nImplement the mapped palette; build the candidate.\r\nEND_NEXT_GENERATION\r\n";
        std::string verbose = "1. Preserve the enum mapping and saved values.\n\n";
        for (std::size_t i{}; i < 100u; ++i) verbose += "Old read observation " + std::to_string(i) + " already recorded.\n";
        verbose += handoff;
        const auto compacted = compact_task_text(verbose, 512u);
        if (!compacted.protected_text_fits || compacted.text.size() > 512u
            || compacted.omitted_records == 0u || compacted.text.find(handoff) == std::string::npos
            || !compacted.text.starts_with("1. Preserve")
            || compact_task_text(verbose, 16u).protected_text_fits
            || compact_task_text("short checkpoint", 512u).text != "short checkpoint") return false;
        const auto recent = compact_task_text(verbose.substr(0u, verbose.size() - handoff.size()), 512u, true);
        if (recent.text.size() > 512u || recent.text.find("observation 99") == std::string::npos) return false;
        const std::string wrappedTask = "1. Preserve saved preference values.\n"
            "   Append the enum; never renumber persisted entries.\n"
            "   Success requires the existing preferences to round-trip.\n";
        const auto wrapped = compact_task_text(wrappedTask + "\n" + verbose, 512u);
        if (!wrapped.protected_text_fits || wrapped.text.find(wrappedTask) == std::string::npos) return false;
        const std::vector<SourceDemand> pressure{{1024u, false, 8192u},
            {32768u, true, 32768u}, {16384u, false, 16384u}};
        for (const auto budget : {0u, 1u, 2048u, 8192u, 64u * 1024u})
        {
            const auto values = allocate_source_bytes(pressure, budget);
            std::size_t total{};
            for (std::size_t i{}; i < values.size(); ++i)
            {
                if (values[i] > pressure[i].desired_bytes) return false;
                total += values[i];
            }
            if (total > budget) return false;
        }
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 0u, 1024u});
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 2048u, 1024u});
        if (workspace.ranges().size() != 2u) return false;
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 0u, 4096u});
        if (workspace.ranges().size() != 1u || workspace.ranges()[0].byte_count != 4096u)
            return false;
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 1024u, 512u});
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 3072u, 2048u});
        if (workspace.ranges().size() != 1u || workspace.ranges()[0].byte_offset != 0u
            || workspace.ranges()[0].byte_count != 5120u) return false;
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'b'), 0u, 1024u});
        if (workspace.ranges().size() != 1u || workspace.ranges()[0].source_sha256 != std::string(64u, 'b'))
            return false;
        for (std::size_t index = 1u; index <= 8u; ++index)
            workspace.remember_range({"Engine/a.cpp", std::string(64u, 'b'), index * 2048u, 1024u});
        workspace.remember_range({"Engine/unreviewed.cpp", std::string(64u, 'b'), 0u, 1024u});
        if (workspace.ranges().size() != 9u) return false;
        workspace.remember_range({"Engine/b.cpp", std::string(64u, 'b'), 0u, 512u * 1024u});
        if (workspace.ranges().size() != 10u
            || workspace.ranges().back().byte_count != 512u * 1024u) return false;
        for (const auto limit : {0u, 64u, 256u, 1024u})
            if (workspace.navigation_memory(limit).size() > limit) return false;
        if (workspace.navigation_memory().find("REMEMBERED_RANGE") == std::string::npos)
            return false;
        workspace.reset("different objective");
        return workspace.ranges().empty();
    }
}
