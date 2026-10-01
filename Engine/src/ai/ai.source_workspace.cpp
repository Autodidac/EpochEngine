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
    void Workspace::reset(std::string objective)
    {
        objective_ = std::move(objective);
        reviewed_.clear();
        searches_.clear();
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
        std::string out = "EPOCH_CUMULATIVE_SOURCE_MEMORY_V1\n";
        for (const auto& item : reviewed_)
        {
            std::string line = "REVIEWED " + item.path;
            if (item.first_line != 0u) line += " line=" + std::to_string(item.first_line);
            if (!item.query.empty()) line += " query=" + item.query;
            line += "\n";
            if (out.size() + line.size() > maximum_bytes) break;
            out += line;
        }
        out += "END_EPOCH_CUMULATIVE_SOURCE_MEMORY_V1\n";
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
        return workspace.merge("Engine/a.cpp")
            && !workspace.merge("Engine/a.cpp")
            && workspace.merge("Engine/b.cpp", 10u, "needle")
            && workspace.reviewed().size() == 2u
            && workspace.contains_path("Engine/b.cpp");
    }
}
