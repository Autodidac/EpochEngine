module;

#include <string>

export module ai.eval;

export namespace epoch::ai
{
    struct EvalCase
    {
        std::string name{};
        std::string prompt{};
        std::string expected_contains{};
        std::string project_id{};
        std::string scene_id{};
    };
}
