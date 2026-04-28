module;

#include <string>
#include <vector>

export module ai.train;

export namespace epoch::ai
{
    struct TrainingPaths
    {
        std::string workspace_root{};
        std::string local_capture_jsonl{};
        std::string mcp_capture_jsonl{};
        std::string checkpoint_root{};
        std::string model_root{};
        std::string cache_root{};
        std::string curated_dataset_root{};
        std::string eval_root{};
    };

    struct IterationPacket
    {
        std::string packet_name{};
        std::string task_prompt{};
        std::string assistant_hint{};
        std::string operator_notes{};
        std::string control_loop_stage{};
        std::string review_gate_state{};
        std::string review_gate_evidence{};
        std::string project_id{};
        std::string project_name{};
        std::string scene_id{};
        std::string project_root{};
        std::string scene_path{};
        std::string active_script{};
        std::string build_log_path{};
        std::string output_path{};
        std::string provider_summary{};
        std::string active_model{};
        std::string manifest_path{};
        std::string workspace_root{};
        std::string raw_capture_path{};
        std::string mcp_capture_path{};
        std::string checkpoint_root{};
        std::string model_root{};
        std::string cache_root{};
        std::string curated_dataset_root{};
        std::string eval_root{};
        std::vector<std::string> evidence_paths{};
    };
}
