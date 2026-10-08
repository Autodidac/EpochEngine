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
        const auto fill = [&](auto eligible)
        {
            while (budget != 0u)
            {
                std::size_t pending{};
                for (std::size_t i = 0u; i < demands.size(); ++i)
                    if (eligible(i) && allocated[i] < demands[i].desired_bytes)
                        ++pending;
                if (pending == 0u) break;
                const auto share = (std::max)(std::size_t{1u}, budget / pending);
                for (std::size_t i = 0u; i < demands.size() && budget != 0u; ++i)
                {
                    if (!eligible(i)) continue;
                    const auto amount = (std::min)({share, budget,
                        demands[i].desired_bytes - allocated[i]});
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
        fill([&](std::size_t i) { return demands[i].desired_bytes <= 8192u; });
        fill([&](std::size_t i) { return demands[i].current_request; });
        fill([](std::size_t) { return true; });
        return allocated;
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
            || range.byte_count == 0u || range.byte_count > 128u * 1024u
            || range.byte_offset > 8u * 1024u * 1024u
            || range.byte_count > 8u * 1024u * 1024u - range.byte_offset)
            return;
        std::erase_if(ranges_, [&](const VerifiedRange& prior)
        {
            if (prior.path != range.path) return false;
            if (prior.source_sha256 != range.source_sha256) return true;
            return prior.byte_offset >= range.byte_offset
                && prior.byte_offset + prior.byte_count
                    <= range.byte_offset + range.byte_count;
        });
        // Most recent ranges win, but the declaration and implementation can
        // remain distinct. This metadata never retains a second source copy.
        if (std::ranges::count(ranges_, range.path, &VerifiedRange::path) >= 4)
        {
            const auto oldest = std::ranges::find(ranges_, range.path, &VerifiedRange::path);
            ranges_.erase(oldest);
        }
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
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 0u, 1024u});
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 2048u, 1024u});
        if (workspace.ranges().size() != 2u) return false;
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'a'), 0u, 4096u});
        if (workspace.ranges().size() != 1u || workspace.ranges()[0].byte_count != 4096u)
            return false;
        workspace.remember_range({"Engine/a.cpp", std::string(64u, 'b'), 0u, 1024u});
        if (workspace.ranges().size() != 1u || workspace.ranges()[0].source_sha256 != std::string(64u, 'b'))
            return false;
        for (std::size_t index = 1u; index <= 8u; ++index)
            workspace.remember_range({"Engine/a.cpp", std::string(64u, 'b'), index * 2048u, 1024u});
        workspace.remember_range({"Engine/unreviewed.cpp", std::string(64u, 'b'), 0u, 1024u});
        if (workspace.ranges().size() != 4u) return false;
        for (const auto limit : {0u, 64u, 256u, 1024u})
            if (workspace.navigation_memory(limit).size() > limit) return false;
        if (workspace.navigation_memory().find("REMEMBERED_RANGE") == std::string::npos)
            return false;
        workspace.reset("different objective");
        return workspace.ranges().empty();
    }
}
