// Standalone C++23 regression for the real epochengine::string ↔ STL boundary.
// Build: g++ -std=c++23 -Wall -Wextra -Werror -pedantic Tests/ai/engcoder_host_environment_bridge_test.cpp -o engcoder_host_bridge_test
#include "../../Engine/include/core.stl_types.hpp"
#include <cassert>
#include <filesystem>
#include <string>

int main()
{
    const epochengine::optional<epochengine::string> configured{
        epochengine::string{"engcoder-server"}};
    std::string server_executable;
    if (configured && !configured->empty())
        server_executable = std::string{epochengine::to_std(*configured)};
    assert(server_executable == "engcoder-server");

    const epochengine::optional<epochengine::string> local{
        epochengine::string{"local-profile-root"}};
    if (local && !local->empty())
    {
        const auto existing = std::filesystem::path{
            std::string{epochengine::to_std(*local)}} / "EngCoder";
        const auto externalServer = existing / "bin/EngCoderServer.exe";
        assert(existing.filename().string() == "EngCoder");
        assert(externalServer.filename().string() == "EngCoderServer.exe");
    }
}
