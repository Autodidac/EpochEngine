/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

module ai.source_discovery_contract;

import ai.source_index;
import ai.source_workspace;

namespace epochengine::ai::source_discovery_contract
{
    [[nodiscard]] bool run_contract()
    {
        const auto root = std::filesystem::temp_directory_path()
            / ("epoch_source_discovery_contract_"
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::error_code error{};
        std::filesystem::create_directories(root / "Engine/modules", error);
        std::filesystem::create_directories(root / "Engine/src/epochgui", error);
        std::filesystem::create_directories(root / "Engine/include/shared", error);
        std::filesystem::create_directories(root / "Engine/include/first", error);
        std::filesystem::create_directories(root / "Engine/include/second", error);
        std::filesystem::create_directories(root / "EngineSecret", error);
        if (error) return false;
        std::ofstream{root / "Engine/modules/gui.engine.ixx"}
            << "export module gui.engine;\nimport core.context;\n"
                "export enum class ThemeVariant { Dark, FireEngineRed };\n";
        std::ofstream{root / "Engine/src/epochgui/gui.engine.cpp"}
            << "module gui.engine;\nimport core.context;\nimport :widgets;\n"
                "# include \"gui.rules.hpp\"\n#include <shared.api.hpp>\n"
                "#include <collision.hpp>\n#include \"../../../EngineSecret/private.hpp\"\n"
                "void apply_theme(ThemeVariant theme) { (void)theme; }\n";
        std::ofstream{root / "Engine/modules/core.context.ixx"}
            << "export module core.context;\nexport struct Handle {};\n";
        std::ofstream{root / "Engine/modules/gui.engine_widgets.ixx"}
            << "export module gui.engine:widgets;\nimport gui.engine;\n";
        std::ofstream{root / "Engine/src/epochgui/gui.rules.hpp"}
            << "#pragma once\nstruct Rule {};\n";
        std::ofstream{root / "Engine/include/shared/shared.api.hpp"}
            << "#pragma once\nstruct SharedApi {};\n";
        std::ofstream{root / "Engine/include/first/collision.hpp"} << "struct First {};\n";
        std::ofstream{root / "Engine/include/second/collision.hpp"} << "struct Second {};\n";
        std::ofstream{root / "EngineSecret/private.hpp"} << "struct Private {};\n";
        std::ofstream{root / "Engine/src/epochgui/gui.client.cpp"}
            << "import gui.engine;\nvoid client() {}\n";
        std::ofstream{root / "Engine/src/epochgui/gui.comments.cpp"}
            << "/*\nexport module gui.engine;\nimport core.context;\nOnlyInComment\n*/\n"
                "const char* sample = R\"sample(\nimport gui.engine;\nThemeVariant\n)sample\";\n"
                "const char* literal = \"ThemeVariant\";\nint ThemeVariantSuffix;\n"
                "// continued \\\nimport gui.engine;\n";
        std::ofstream{root / "Engine/src/epochgui/gui.empty.cpp"};

        source_index::RepositoryIndex index{};
        const auto built = index.build(root, "Engine/");
        const auto hits = index.search({source_index::SearchKind::identifier,
            "ThemeVariant", 8u});
        const std::string map = index.compact_map("add fire engine red theme", 16u * 1024u, 32u);
        const auto pathHits = index.search({source_index::SearchKind::path, "empty.cpp", 1u});
        const auto has_path = [](const auto& results, std::string_view path)
        { return std::ranges::any_of(results, [&](const auto& hit) { return hit.relative_path == path; }); };
        const auto dependencies = index.search({source_index::SearchKind::imports,
            "Engine/src/epochgui/gui.engine.cpp", 64u});
        const auto importers = index.search({source_index::SearchKind::importers,
            "gui.engine", 64u});
        const auto related = index.related_sources("Engine/src/epochgui/gui.engine.cpp", 64u);
        const auto reverseRelated = index.related_sources("Engine/modules/gui.engine.ixx", 64u);
        const auto ownerMap = index.compact_map("apply_theme", 16u * 1024u, 32u);
        bool mapBounded = true;
        for (const std::size_t budget : {0u, 8u, 150u, 512u, 1024u})
        {
            const auto bounded = index.compact_map("apply_theme", budget, 32u);
            mapBounded = mapBounded && bounded.size() <= budget
                && (bounded.empty() || bounded.ends_with("END_EPOCH_SOURCE_REPOSITORY_MAP_V2\n"));
        }
        const bool relations = dependencies.size() == 4u
            && has_path(dependencies, "Engine/modules/core.context.ixx")
            && has_path(dependencies, "Engine/modules/gui.engine_widgets.ixx")
            && has_path(dependencies, "Engine/src/epochgui/gui.rules.hpp")
            && has_path(dependencies, "Engine/include/shared/shared.api.hpp")
            && importers.size() == 2u
            && has_path(importers, "Engine/src/epochgui/gui.client.cpp")
            && has_path(importers, "Engine/modules/gui.engine_widgets.ixx")
            && has_path(related, "Engine/modules/gui.engine.ixx")
            && has_path(reverseRelated, "Engine/src/epochgui/gui.engine.cpp")
            && !has_path(hits, "Engine/src/epochgui/gui.comments.cpp")
            && index.search({source_index::SearchKind::references, "OnlyInComment", 8u}).empty()
            && !index.search({source_index::SearchKind::text, "OnlyInComment", 8u}).empty()
            && index.search({source_index::SearchKind::importers, "gui.eng", 8u}).empty()
            && index.search({source_index::SearchKind::path, "gui", 0u}).empty()
            && index.related_sources("EngineSecret/private.hpp").empty()
            && index.related_sources("Engine/src/epochgui/gui.engine.cpp", 0u).empty()
            && ownerMap.find("PATH Engine/modules/gui.engine.ixx") != std::string::npos
            && ownerMap.find("PATH Engine/modules/core.context.ixx") != std::string::npos
            && ownerMap.find("EDGE Engine/src/epochgui/gui.engine.cpp -> Engine/modules/gui.engine.ixx") != std::string::npos
            && ownerMap.find("EngineSecret") == std::string::npos
            && map.find("PATH Engine/src/epochgui/gui.empty.cpp\n") != std::string::npos
            && mapBounded;
        // Rebuilding the index discards removed edges instead of retaining old
        // ownership as if it were source from the new checkout revision.
        std::ofstream{root / "Engine/src/epochgui/gui.client.cpp"} << "void client() {}\n";
        const bool refreshed = index.build(root, "Engine")
            && !has_path(index.search({source_index::SearchKind::importers, "gui.engine", 64u}),
                "Engine/src/epochgui/gui.client.cpp");

        source_workspace::Workspace workspace{};
        workspace.reset("add fire engine red theme");
        const bool first = workspace.merge("Engine/modules/gui.engine.ixx");
        const bool duplicate = workspace.merge("Engine/modules/gui.engine.ixx");
        const bool second = workspace.merge("Engine/src/epochgui/gui.engine.cpp", 2u, "apply_theme");
        workspace.note_discovery_round(first || second);
        workspace.note_discovery_round(false);
        workspace.note_discovery_round(false);

        const bool newEvidence = workspace.remember_evidence("path:2:sha256-A");
        const bool repeatedEvidence = workspace.remember_evidence("path:2:sha256-A");
        auto pending = workspace;
        const bool pendingEvidence = pending.remember_evidence("path:3:sha256-B");
        const bool rollbackPreserved = workspace.remember_evidence("path:3:sha256-B");
        workspace.reset("new objective");
        const bool resetEvidence = workspace.remember_evidence("path:2:sha256-A");

        std::filesystem::remove_all(root, error);
        return source_workspace::run_contract()
            && built && built.file_count == 11u && !hits.empty() && relations && refreshed
            && pathHits.size() == 1u && pathHits.front().line == 0u
            && pathHits.front().preview.empty()
            && map.find("PATH Engine/modules/gui.engine.ixx") != std::string::npos
            && map.find("ThemeVariant") != std::string::npos
            && first && !duplicate && second
            && pending.reviewed_paths().size() == 2u
            && pending.stagnant_rounds() == 2u
            && pending.navigation_memory().find("apply_theme") != std::string::npos
            && newEvidence && !repeatedEvidence && pendingEvidence
            && rollbackPreserved && resetEvidence
            && !workspace.remember_evidence(std::string(2049u, 'x'));
    }
}

#if defined(EPOCH_AI_SOURCE_DISCOVERY_CONTRACT_MAIN)
int main()
{
    return epochengine::ai::source_discovery_contract::run_contract() ? 0 : 1;
}
#endif
