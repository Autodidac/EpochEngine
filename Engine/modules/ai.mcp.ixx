module;

#include <string>

export module ai.mcp;

export namespace epoch::ai
{
    struct McpCaptureRecord
    {
        std::string server{};
        std::string tool{};
        std::string prompt{};
        std::string normalized_output{};
        std::string source_path{};
    };
}
