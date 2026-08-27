module;

#include <string>

export module ai.session;

export namespace epochengine::ai
{
    struct EvidencePaths
    {
        std::string workspace_root{};
        std::string model_exchange_jsonl{};
        std::string tool_trace_jsonl{};
        std::string session_root{};
        std::string model_root{};
        std::string cache_root{};
        std::string review_fixture_root{};
        std::string eval_root{};
    };

}