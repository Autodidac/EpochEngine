/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

export module ai.source_workspace;

export namespace epochengine::ai::source_workspace
{
    struct ReviewedSlice final
    {
        std::string path{};
        std::size_t first_line{};
        std::string query{};
    };

    class Workspace final
    {
    public:
        void reset(std::string objective = {});
        [[nodiscard]] bool merge(std::string path, std::size_t first_line = 0u,
            std::string query = {});
        [[nodiscard]] bool remember_search(std::string signature);
        // Only host-verified path/range/content fingerprints count as progress.
        [[nodiscard]] bool remember_evidence(std::string fingerprint);
        [[nodiscard]] bool contains_path(std::string_view path) const noexcept;
        [[nodiscard]] const std::vector<ReviewedSlice>& reviewed() const noexcept;
        [[nodiscard]] std::vector<std::string> reviewed_paths() const;
        [[nodiscard]] std::string navigation_memory(std::size_t maximum_bytes = 16u * 1024u) const;
        [[nodiscard]] std::size_t discovery_rounds() const noexcept;
        void note_discovery_round(bool added_evidence) noexcept;
        [[nodiscard]] std::size_t stagnant_rounds() const noexcept;

    private:
        std::string objective_{};
        std::vector<ReviewedSlice> reviewed_{};
        std::vector<std::string> searches_{};
        std::vector<std::string> evidence_{};
        std::size_t discovery_rounds_{};
        std::size_t stagnant_rounds_{};
    };

    [[nodiscard]] bool run_contract();
}
