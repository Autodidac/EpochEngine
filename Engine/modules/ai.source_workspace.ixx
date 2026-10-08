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

    // Exact ranges are navigation memory only. A loader must re-read the file
    // and verify its whole-file hash before making these bytes resident again.
    struct VerifiedRange final
    {
        std::string path{};
        std::string source_sha256{};
        std::size_t byte_offset{};
        std::size_t byte_count{};
    };

    struct SourceDemand final
    {
        std::size_t desired_bytes{};
        bool current_request{};
    };

    // Small declarations stay complete; current requests get surplus before
    // historical windows. Allocation never exceeds either demand or budget.
    [[nodiscard]] std::vector<std::size_t> allocate_source_bytes(
        const std::vector<SourceDemand>& demands, std::size_t budget);

    class Workspace final
    {
    public:
        void reset(std::string objective = {});
        [[nodiscard]] bool merge(std::string path, std::size_t first_line = 0u,
            std::string query = {});
        [[nodiscard]] bool remember_search(std::string signature);
        // Only host-verified path/range/content fingerprints count as progress.
        [[nodiscard]] bool remember_evidence(std::string fingerprint);
        void remember_range(VerifiedRange range);
        [[nodiscard]] const std::vector<VerifiedRange>& ranges() const noexcept;
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
        std::vector<VerifiedRange> ranges_{};
        std::size_t discovery_rounds_{};
        std::size_t stagnant_rounds_{};
    };

    [[nodiscard]] bool run_contract();
}
