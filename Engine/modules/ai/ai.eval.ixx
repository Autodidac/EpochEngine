// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
module;

#include <string>

export module ai.eval;

export namespace epochengine::ai
{
    struct EvalCase
    {
        std::string name{};
        std::string prompt{};
        std::string expected_contains{};
        std::string project_id{};
        std::string scene_id{};
    };

    struct HelperReviewGateResult
    {
        bool accepted = false;
        int evidence_score = 0;
        std::string state{};
        std::string reason{};
    };
}
