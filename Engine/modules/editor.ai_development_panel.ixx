/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module editor.ai_development_panel;

export import ai.engine;
export import ai.source_patch_proposal;

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
        launch_source_candidate_preview,
        request_model_source_proposal,
        cancel_model_source_request,
        execute_tool_harness
    };

    enum class ModelTransport : unsigned char
    {
        local_inference,
        external_mcp
    };

    enum class ModelDispatchState : unsigned char
    {
        waiting_for_confirmation,
        queued_by_host,
        transport_started,
        rejected
    };

    enum class CandidateDecision : unsigned char
    {
        none,
        keep_current,
        choose_candidate,
        stop_lab
    };

    struct SourcePatchReviewBinding final
    {
        std::string admitted_response_sha256{};
        std::string curated_bundle_sha256{};
        std::string campaign_id{};
        std::string objective_id{};
        std::string operation_id{};
        std::uint64_t supervisor_generation{};
        std::string supervisor_state_sha256{};
        bool disposable_sandbox_stager_ready{};

        friend bool operator==(
            const SourcePatchReviewBinding&,
            const SourcePatchReviewBinding&) = default;
    };

    struct SourcePatchStagingRequest final
    {
        ai::source_patch_proposal::SealedProposal proposal{};
        SourcePatchReviewBinding binding{};
        bool sandbox_only{true};
        bool live_source_write_permitted{};
        bool promotion_permitted{};
        bool release_permitted{};

        friend bool operator==(
            const SourcePatchStagingRequest&,
            const SourcePatchStagingRequest&) = default;
    };

    struct SourcePatchReviewResult final
    {
        bool accepted{};
        std::string status{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return accepted;
        }
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
        ai::ModelTerminalFailure model_terminal_failure{ai::ModelTerminalFailure::none};
        std::string model_terminal_status{};
        std::string selected_model{};
        std::string selected_endpoint{};
        std::string selected_transport{};
        bool local_model_running{};
        bool local_model_queued{};
        bool local_model_cancelling{};
        std::uint64_t local_model_elapsed_ms{};
        std::string local_model_activity{};
        bool external_mcp_available{};
        std::string external_mcp_status{};
        std::uint64_t external_mcp_process_id{};
        std::uint64_t external_mcp_elapsed_ms{};
        std::string external_mcp_receipt_path{};
        bool external_mcp_running{};
        bool tool_source_ready{};
        bool execution_pending{};
        // The host retains cancelled child/process leases until retirement.
        // A new session must not replace their generation while they settle.
        bool session_retirement_pending{};
    };

    struct ModelActivityView final
    {
        bool visible{};
        bool can_cancel{};
        std::string label{};
        std::string detail{};
        std::string elapsed{};
        float animation_phase{};
    };

    [[nodiscard]] ModelActivityView describe_model_activity(const Input& input);

    struct RenderResult final
    {
        HostAction action{HostAction::none};
        ModelTransport model_transport{ModelTransport::local_inference};
        bool reveal_source_workspace{true};
        bool reveal_source_patch_workbench{};
        std::string status{};
        std::string model_prompt{};
        std::string source_root{};
        std::string workspace_root{};
        std::vector<std::string> include_paths{};
        std::vector<std::string> source_paths{};
        std::vector<std::string> excluded_components{};
        std::vector<std::string> campaign_evidence{};
        std::string source_patch_relative_path{};
        std::string source_patch_postimage_utf8{};
        std::vector<std::string> source_patch_evidence{};
        std::optional<SourcePatchStagingRequest> source_patch_staging{};
        std::uint32_t workspace_generation{};
        bool retire_candidate_preview{};
        CandidateDecision candidate_decision{CandidateDecision::none};
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
        [[nodiscard]] RenderResult begin_source_iteration(
            const Input& input,
            std::string objective);
        // Called once from the owning context update, even when AI Controls
        // is hidden. Advances automatic planning without drawing any GUI.
        [[nodiscard]] RenderResult advance_source_iteration(const Input& input);
        [[nodiscard]] bool has_reviewed_plan() const;
        [[nodiscard]] RenderResult approve_latest_plan();
        [[nodiscard]] bool sandbox_session_failed() const;
        [[nodiscard]] std::string session_status() const;
        [[nodiscard]] RenderResult select_candidate_preview(
            const Input& input, CandidateDecision decision);
        [[nodiscard]] RenderResult render(const Input& input);
        [[nodiscard]] RenderResult stage_latest_model_proposal(
            const Input& input);
        [[nodiscard]] RenderResult report_model_dispatch(
            ModelDispatchState state,
            std::string transport,
            std::string endpoint);
        [[nodiscard]] RenderResult share_requested_source_context(
            const Input& input);
        [[nodiscard]] RenderResult reject_requested_source_context();
        [[nodiscard]] SourcePatchReviewResult admit_source_patch_review(
            ai::source_patch_proposal::SealedProposal proposal,
            SourcePatchReviewBinding binding);
        [[nodiscard]] RenderResult reject_source_patch_review();
        [[nodiscard]] bool has_source_patch_review() const;
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
        [[nodiscard]] RenderResult complete_candidate_preview(
            std::uint32_t generation,
            bool succeeded,
            std::uint64_t platform_process_id,
            std::uint64_t platform_window_id,
            std::string status);
        [[nodiscard]] RenderResult stage_verified_source_promotion(
            const Input& input);
        [[nodiscard]] RenderResult approve_and_promote_verified_source(
            const Input& input);
        [[nodiscard]] RenderResult discard_verified_source_candidate();
        [[nodiscard]] RenderResult cancel_staged_proposal();
        [[nodiscard]] RenderResult cancel_active_campaign(std::string reason);
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
