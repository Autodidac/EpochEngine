/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace epochengine::editor_ai_development_panel::documentation_guide
{
    inline constexpr std::size_t maximum_document_bytes = 16u * 1024u;
    inline constexpr std::size_t maximum_excerpt_bytes = 8u * 1024u;

    struct SourceRegion
    {
        std::string path, begin, end;
    };

    // Human-readable recipe rows are navigation hints. The host resolves both
    // literals against the current admitted source before making exact ranges.
    [[nodiscard]] inline std::vector<SourceRegion> source_regions(std::string_view recipe)
    {
        std::vector<SourceRegion> result;
        while (!recipe.empty() && result.size() < 24u)
        {
            const auto newline = recipe.find('\n');
            auto row = recipe.substr(0u, newline);
            recipe = newline == std::string_view::npos ? std::string_view{} : recipe.substr(newline + 1u);
            if (row.ends_with('\r')) row.remove_suffix(1u);
            if (!row.starts_with("SOURCE_REGION | ")) continue;
            row.remove_prefix(16u);
            const auto first = row.find(" | ");
            if (first == std::string_view::npos) continue;
            const auto second = row.find(" | ", first + 3u);
            if (second == std::string_view::npos) continue;
            SourceRegion region{std::string{row.substr(0u, first)},
                std::string{row.substr(first + 3u, second - first - 3u)},
                std::string{row.substr(second + 3u)}};
            const std::filesystem::path path{region.path};
            if (path.is_absolute() || path.has_root_name() || region.path.find('\\') != std::string::npos
                || std::any_of(path.begin(), path.end(), [](const auto& part) { return part == ".."; })
                || !region.path.starts_with("Engine/") || region.begin.empty() || region.end.empty()
                || region.begin.size() > 256u || region.end.size() > 256u) continue;
            result.push_back(std::move(region));
        }
        return result;
    }

    [[nodiscard]] inline std::string recipe_name(std::string_view objective)
    {
        std::string lowered{};
        lowered.reserve(objective.size());
        for (const unsigned char c : objective)
            lowered.push_back(static_cast<char>(std::tolower(c)));
        if (lowered.find("theme") != std::string::npos
            || lowered.find("palette") != std::string::npos
            || lowered.find("color scheme") != std::string::npos)
            return "THEME";
        if (lowered.find("self.iteration") != std::string::npos
            || lowered.find("self iteration") != std::string::npos
            || lowered.find("source discovery") != std::string::npos
            || lowered.find("agentic") != std::string::npos)
            return "SELF_ITERATION";
        if (lowered.find("qwen") != std::string::npos
            || lowered.find("model") != std::string::npos
            || lowered.find("engcoder") != std::string::npos
            || lowered.find("gpu offload") != std::string::npos)
            return "MODEL_RUNTIME";
        if (lowered.find("chat") != std::string::npos
            || lowered.find("composer") != std::string::npos)
            return "AI_CHAT";
        if (lowered.find("build") != std::string::npos
            || lowered.find("compile") != std::string::npos
            || lowered.find("compiler") != std::string::npos)
            return "BUILD";
        return {};
    }

    [[nodiscard]] inline std::string extract_recipe(
        std::string_view document, std::string_view recipe)
    {
        if (recipe.empty()) return {};
        const std::string begin = "<!-- EPOCH_RECIPE_" + std::string{recipe} + "_BEGIN -->";
        const std::string end = "<!-- EPOCH_RECIPE_" + std::string{recipe} + "_END -->";
        auto start = document.find(begin);
        if (start == std::string_view::npos) return {};
        start += begin.size();
        const auto stop = document.find(end, start);
        if (stop == std::string_view::npos || stop - start > maximum_excerpt_bytes)
            return {};
        return std::string{document.substr(start, stop - start)};
    }

    // Documentation is read only from a fixed, source-owned file; never from
    // a model-selected path. It provides navigation, not exact edit authority.
    [[nodiscard]] inline std::string load_recipe(
        const std::filesystem::path& admitted_root,
        std::string_view objective)
    {
        const auto name = recipe_name(objective);
        if (name.empty() || admitted_root.empty()) return {};
        std::error_code error{};
        const auto canonical_root = std::filesystem::canonical(admitted_root, error);
        if (error) return {};
        for (const auto& relative : {
            std::filesystem::path{"Engine/docs/engine/ai_task_recipes.md"},
            std::filesystem::path{"docs/engine/ai_task_recipes.md"}})
        {
            const auto candidate = canonical_root / relative;
            error.clear();
            if (std::filesystem::is_symlink(candidate, error) || error) continue;
            const auto canonical_file = std::filesystem::canonical(candidate, error);
            if (error) continue;
            const auto rel = canonical_file.lexically_relative(canonical_root);
            if (rel.empty() || rel.is_absolute()
                || *rel.begin() == std::filesystem::path{".."}) continue;
            if (!std::filesystem::is_regular_file(canonical_file, error) || error)
                continue;
            const auto size = std::filesystem::file_size(canonical_file, error);
            if (error || size > maximum_document_bytes) continue;
            std::ifstream stream{canonical_file, std::ios::binary};
            if (!stream) continue;
            const std::string document{
                std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
            const auto recipe = extract_recipe(document, name);
            if (!recipe.empty()) return recipe;
        }
        return {};
    }
}
