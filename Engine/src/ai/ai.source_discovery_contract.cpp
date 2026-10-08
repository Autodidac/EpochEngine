/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
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
        if (error) return false;
        std::ofstream{root / "Engine/modules/gui.engine.ixx"}
            << "export module gui.engine;\nexport enum class ThemeVariant { Dark, FireEngineRed };\n";
        std::ofstream{root / "Engine/src/epochgui/gui.engine.cpp"}
            << "module gui.engine;\nvoid apply_theme(ThemeVariant theme) { (void)theme; }\n";
        std::ofstream{root / "Engine/src/epochgui/gui.empty.cpp"};

        source_index::RepositoryIndex index{};
        const auto built = index.build(root, "Engine/");
        const auto hits = index.search({source_index::SearchKind::identifier,
            "ThemeVariant", 8u});
        const std::string map = index.compact_map("add fire engine red theme", 16u * 1024u, 32u);
        const auto pathHits = index.search({source_index::SearchKind::path, "empty.cpp", 1u});

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
            && built && built.file_count == 3u && !hits.empty()
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
