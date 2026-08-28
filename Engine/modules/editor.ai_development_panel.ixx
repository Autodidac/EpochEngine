/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module editor.ai_development_panel;

export namespace epochengine::editor_ai_development_panel
{
    enum class Domain : unsigned char
    {
        tooling,
        engine_source,
        project_source
    };

    enum class HostAction : unsigned char
    {
        none,
        materialize_source_workspace,
        compile_source_workspace,
        cancel_source_task,
        test_source_workspace,
        compile_source_release_workspace,
        test_source_release_workspace,
        compile_source_headless_workspace,
        test_source_headless_workspace,
        test_source_full_validation_workspace,
        request_model_source_proposal,
        execute_tool_harness
    };

    struct Input final
    {
        Domain domain{Domain::tooling};
        float available_width{320.0f};
        std::string workspace_id{};
        std::string source_snapshot_root{};
        std::string source_authority_kind{};
        std::string source_authority_version{};
        std::string source_authority_commit{};
        std::string source_authority_receipt_digest{};
        bool source_authority_verified{};
        std::string workspace_root{};
        std::string source_cache_root{};
        std::string curated_scope_digest{};
        std::vector<std::string> curated_source_paths{};
        std::string development_objective{};
        std::string architecture_evidence{};
        std::string tool_output_relative_path{};
        std::string latest_raw_model_reply{};
        std::string selected_model{};
        std::string selected_endpoint{};
        bool tool_source_ready{};
        bool execution_pending{};
    };

    struct RenderResult final
    {
        HostAction action{HostAction::none};
        std::string status{};
        std::string model_prompt{};
        std::string source_root{};
        std::string workspace_root{};
        std::vector<std::string> include_paths{};
        std::vector<std::string> source_paths{};
        std::vector<std::string> excluded_components{};
        std::vector<std::string> campaign_evidence{};
        std::uint32_t workspace_generation{};
    };

    class Panel final
    {
    public:
        Panel();
        ~Panel();

        Panel(const Panel&) = delete;
        Panel& operator=(const Panel&) = delete;
        Panel(Panel&&) noexcept;
        Panel& operator=(Panel&&) noexcept;

        [[nodiscard]] static bool run_contract();
        [[nodiscard]] RenderResult render(const Input& input);
        [[nodiscard]] RenderResult stage_latest_model_proposal(
            const Input& input);
        [[nodiscard]] RenderResult share_requested_source_context(
            const Input& input);
        [[nodiscard]] RenderResult reject_requested_source_context();
        [[nodiscard]] RenderResult complete_source_workspace(
            std::uint32_t generation,
            bool succeeded,
            std::string status,
            std::size_t file_count,
            std::uint64_t total_bytes);
        [[nodiscard]] RenderResult approve_and_run_staged_proposal(
            const Input& input);
        [[nodiscard]] RenderResult complete_source_build(
            std::uint32_t generation,
            bool succeeded,
            std::string status);
        [[nodiscard]] RenderResult complete_source_test(
            std::uint32_t generation,
            bool succeeded,
            std::string status);
        [[nodiscard]] RenderResult complete_source_release_build(
            std::uint32_t generation,
            bool succeeded,
            std::string status);
        [[nodiscard]] RenderResult complete_source_release_test(
            std::uint32_t generation,
            bool succeeded,
            std::string status);
        [[nodiscard]] RenderResult complete_source_headless_build(
            std::uint32_t generation,
            bool succeeded,
            std::string status);
        [[nodiscard]] RenderResult complete_source_headless_test(
            std::uint32_t generation,
            bool succeeded,
            std::string status);
        [[nodiscard]] RenderResult approve_source_full_validation();
        [[nodiscard]] RenderResult complete_source_full_validation(
            std::uint32_t generation,
            bool succeeded,
            std::string status);
        [[nodiscard]] RenderResult stage_verified_source_promotion(
            const Input& input);
        [[nodiscard]] RenderResult approve_and_promote_verified_source(
            const Input& input);
        [[nodiscard]] RenderResult discard_verified_source_candidate();
        [[nodiscard]] RenderResult cancel_staged_proposal();
        [[nodiscard]] bool has_pending_source_context() const;
        [[nodiscard]] bool has_staged_proposal() const;
        [[nodiscard]] bool has_source_full_validation_candidate() const;
        [[nodiscard]] bool has_verified_source_candidate() const;
        [[nodiscard]] bool has_staged_source_promotion() const;
        void complete_tool_harness(
            bool succeeded,
            std::string build_summary,
            std::string runtime_summary);
        void reset(std::string workspace_id);
        [[nodiscard]] std::string status() const;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> implementation_{};
    };
}