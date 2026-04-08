module;

#include <string>
#include <vector>

export module ai.dataset;

export namespace epoch::ai
{
    struct DatasetRecord
    {
        std::string prompt{};
        std::string answer{};
        std::string source{};
        std::string role{};
        std::vector<std::string> tags{};
    };
}
