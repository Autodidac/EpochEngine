#include "aengine.config.hpp"
#include "epoch.runtime_bridge.hpp"
#include "epoch.script_api.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <type_traits>

namespace
{
    struct SmokeState
    {
        bool logged = false;
        bool queuedModel = false;
    };

    void SmokeLog(void* userData, const char* message)
    {
        auto* state = static_cast<SmokeState*>(userData);
        if (state)
        {
            state->logged = true;
        }
        std::cout << "[epoch-ci] " << (message ? message : "(null)") << '\n';
    }

    int SmokeQueueModelLoad(void* userData, const char* debugName, const char* path)
    {
        auto* state = static_cast<SmokeState*>(userData);
        if (state)
        {
            state->queuedModel = true;
        }

        if (!debugName || !path || std::strlen(debugName) == 0 || std::strlen(path) == 0)
        {
            return 0;
        }

        std::cout << "[epoch-ci] queued model '" << debugName << "' from " << path << '\n';
        return 1;
    }

    bool OptionalPathProbe(const std::filesystem::path& repoRoot)
    {
        const auto engineDir = repoRoot / "Engine";
        const auto assetsDir = engineDir / "assets";
        const auto demoDir = assetsDir / "demo";

        std::cout << "[epoch-ci] repo probe: " << repoRoot.string() << '\n';
        std::cout << "[epoch-ci] Engine present: " << std::filesystem::exists(engineDir) << '\n';
        std::cout << "[epoch-ci] Engine/assets present: " << std::filesystem::exists(assetsDir) << '\n';
        std::cout << "[epoch-ci] demo assets present: " << std::filesystem::exists(demoDir) << '\n';

        // Hosted CI must stay asset-light, so this is an informational probe.
        return true;
    }
}

int main(int argc, char** argv)
{
    static_assert(std::is_standard_layout_v<EpochScriptHost>,
        "EpochScriptHost must stay ABI-simple for generated project scripts.");
    static_assert(std::is_same_v<decltype(&epochnamespace::core::bridge::run_legacy_runtime),
        int (*)(bool)>,
        "The runtime bridge signature must remain stable for lightweight callers.");

    SmokeState state{};
    EpochScriptHost host{};
    host.user_data = &state;
    host.log = &SmokeLog;
    host.queue_model_load = &SmokeQueueModelLoad;
    host.project_model_asset = "Engine/assets/demo/minisponza/mini_sponza_v2.gltf";

    host.log(host.user_data, "headless compile/run smoke");
    const int queued = host.queue_model_load(host.user_data, "MiniSponza",
        host.project_model_asset);

    const auto repoRoot = (argc > 1 && argv && argv[1])
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path();
    OptionalPathProbe(repoRoot);

    if (!state.logged || !state.queuedModel || queued != 1)
    {
        std::cerr << "[epoch-ci] script host smoke failed\n";
        return 1;
    }

    std::cout << "[epoch-ci] headless smoke passed without graphics dependencies\n";
    return 0;
}
