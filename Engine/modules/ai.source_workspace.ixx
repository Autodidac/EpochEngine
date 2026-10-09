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
        std::size_t retained_bytes{};
        std::size_t requested_bytes{};
        bool mission_critical{};
    };

    // Reserve useful current reads before retained history can consume capacity.
    // Retain the remaining working set before widening. Never exceed the budget.
    [[nodiscard]] std::vector<std::size_t> allocate_source_bytes(
        const std::vector<SourceDemand>& demands, std::size_t budget);

    struct WorkingSet final
    {
        std::vector<std::string> resident_paths{};
        std::vector<std::string> archived_paths{};
    };

    // Compact only prompt residency, never source or cumulative provenance.
    // Explicit current reads and known owners named by the task remain active.
    [[nodiscard]] WorkingSet select_working_set(
        const std::vector<std::string>& reviewed,
        const std::vector<std::string>& requested, std::string_view task);

    struct CompactedText final
    {
        std::string text{};
        std::size_t omitted_records{};
        bool protected_text_fits{true};
    };
    // Whole-record compaction preserves the concrete handoff and task records;
    // no word counting, UTF-8 slicing or invented completion/summary evidence.
    [[nodiscard]] CompactedText compact_task_text(
        std::string_view text, std::size_t budget, bool recent_first = false);

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
