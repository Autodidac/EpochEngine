/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

module ai.iteration_campaign;

namespace epochengine::ai::iteration_campaign
{
    bool run_contract()
    {
        namespace fs = std::filesystem;
        using namespace iteration_session;
        const auto token = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        const fs::path root = fs::temp_directory_path()
            / ("epoch_ai_campaign_" + std::to_string(token));
        const fs::path project = root / "Project";
        const fs::path cache = root / "cache/ai";
        std::error_code ec{};
        fs::create_directories(project / "Source", ec);
        std::ofstream{project / "Source/main.cpp", std::ios::binary}
            << "int epoch_campaign_contract() { return 31; }\n";

        const SourceAuthority authority{
            .target_kind = IterationTargetKind::project_source,
            .kind = SourceAuthorityKind::verified_project,
            .root = fs::weakly_canonical(project, ec),
            .project_id = "campaign_contract",
            .project_manifest_digest = std::string(64u, 'a'),
            .project_profile_digest = std::string(64u, 'b'),
            .verified = !ec};
        const CuratedInspection inspected = inspect_curated_files(
            authority, {"Source/main.cpp"});
        IterationSession session{};
        CampaignResult begun = inspected.accepted
            ? begin_campaign(CampaignConfiguration{
                .authority = authority,
                .objective = "Improve project behavior without touching Engine source.",
                .model_name = "Qwen3.8-27B",
                .curated_files = inspected.files,
                .policy = CandidatePolicy::manual_each_candidate,
                .budgets = default_budgets(IterationTargetKind::project_source),
                .created_at_unix_seconds = 2'000'000'000u}, session)
            : CampaignResult{};
        if (!begun
            || begun.report.session.source.target_kind
                != IterationTargetKind::project_source
            || begun.report.budgets.maximum_workspace_files != 2048u
            || begun.report.budgets.maximum_workspace_bytes
                != 256u * 1024u * 1024u)
        {
            fs::remove_all(root, ec);
            return false;
        }

        CampaignResult saved = save_report(cache, begun.report);
        CampaignResult loaded = saved ? load_report(saved.state_path) : CampaignResult{};
        if (!saved || !loaded || loaded.state_digest != saved.state_digest
            || loaded.report.session.objective != begun.report.session.objective
            || loaded.report.session.source != authority)
        {
            fs::remove_all(root, ec);
            return false;
        }

        std::ofstream{saved.state_path.string() + ".tmp.orphan", std::ios::binary}
            << "incomplete";
        if (!load_report(saved.state_path))
        {
            fs::remove_all(root, ec);
            return false;
        }

        CampaignResult counted = consume_budget(
            loaded.report, BudgetKind::model_call);
        CampaignResult resaved = counted
            ? save_report(cache, counted.report) : CampaignResult{};
        CampaignResult reloaded = resaved
            ? load_report(resaved.state_path) : CampaignResult{};
        if (!counted || !resaved || !reloaded
            || reloaded.report.counters.model_calls != 1u
            || reloaded.report.previous_state_digest != saved.state_digest)
        {
            fs::remove_all(root, ec);
            return false;
        }

        const fs::path tampered = root / "tampered.epochai";
        std::ifstream good{resaved.state_path, std::ios::binary};
        std::string bytes{
            std::istreambuf_iterator<char>{good},
            std::istreambuf_iterator<char>{}};
        if (!bytes.empty())
            bytes.back() = bytes.back() == 'x' ? 'y' : 'x';
        std::ofstream{tampered, std::ios::binary} << bytes;
        if (load_report(tampered))
        {
            fs::remove_all(root, ec);
            return false;
        }

        const auto old_identity = reloaded.report.session.identity;
        IterationSession resumed_session{};
        CampaignResult resumed = resume_campaign(
            reloaded.report,
            authority,
            inspected.files,
            2'000'000'100u,
            resumed_session);
        SourceAuthority drifted = authority;
        drifted.project_profile_digest = std::string(64u, 'c');
        IterationSession refused_session{};
        CampaignResult refused = resume_campaign(
            reloaded.report,
            drifted,
            inspected.files,
            2'000'000'100u,
            refused_session);
        CampaignReport exhausted = reloaded.report;
        exhausted.counters.model_calls = exhausted.budgets.maximum_model_calls;

        const auto engine_plan = validation_plan(IterationTargetKind::engine_source);
        const auto project_plan = validation_plan(IterationTargetKind::project_source);
        const bool ok = resumed
            && resumed.report.session.identity != old_identity
            && resumed.report.session.objective
                == reloaded.report.session.objective
            && !resumed.report.session.candidate_approved
            && resumed.report.session.validation.empty()
            && !refused
            && !consume_budget(exhausted, BudgetKind::model_call)
            && engine_plan.size() == 7u
            && engine_plan.front() == ValidationStep::debug_compiler
            && engine_plan.back() == ValidationStep::full_validation
            && project_plan.size() == 2u
            && project_plan.front() == ValidationStep::project_compiler
            && project_plan.back() == ValidationStep::project_contract;
        fs::remove_all(root, ec);
        return ok;
    }
}
