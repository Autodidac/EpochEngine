module;

#include <string>

export module ai.train;

export namespace epoch::ai
{
    struct TrainingPaths
    {
        std::string workspace_root{};
        std::string local_capture_jsonl{};
        std::string checkpoint_root{};
        std::string model_root{};
        std::string cache_root{};
        std::string curated_dataset_root{};
    };
}
