#include "engine.config.hpp"
#include "epoch/core/cpp_feature_probe.hpp"
#include "epoch.runtime_bridge.hpp"
#include "epoch.script_api.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <type_traits>

extern "C" void core_log_write(std::uint32_t lvl, const char* tag_utf8, const char* msg_utf8);

namespace
{
    constexpr std::uint32_t kLogInfo = 1u;
    constexpr std::uint32_t kLogError = 3u;
    constexpr const char* kLogTag = "Epoch.HeadlessCI";

    struct SmokeState
    {
        bool logged = false;
        bool queuedModel = false;
    };

    void LogInfo(std::string_view message)
    {
        const std::string text{ message };
        core_log_write(kLogInfo, kLogTag, text.c_str());
    }

    void LogError(std::string_view message)
    {
        const std::string text{ message };
        core_log_write(kLogError, kLogTag, text.c_str());
    }

    void LogInfoPair(std::string_view label, std::string_view value)
    {
        std::string text;
        text.reserve(label.size() + value.size() + 2u);
        text.append(label);
        text.append(": ");
        text.append(value);
        LogInfo(text);
    }

    void LogInfoPair(std::string_view label, int value)
    {
        LogInfoPair(label, std::to_string(value));
    }

    void LogErrorPair(std::string_view label, std::string_view value)
    {
        std::string text;
        text.reserve(label.size() + value.size() + 2u);
        text.append(label);
        text.append(": ");
        text.append(value);
        LogError(text);
    }

    void SmokeLog(void* userData, const char* message)
    {
        auto* state = static_cast<SmokeState*>(userData);
        if (state)
        {
            state->logged = true;
        }
        LogInfo(message ? message : "(null)");
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

        std::string text;
        text.reserve(std::strlen(debugName) + std::strlen(path) + 24u);
        text.append("queued model '");
        text.append(debugName);
        text.append("' from ");
        text.append(path);
        LogInfo(text);
        return 1;
    }

    bool IsRepoRoot(const std::filesystem::path& candidate)
    {
        return std::filesystem::exists(candidate / "Engine" / "ai" / "control" / "continuous_build_loop.json")
            && std::filesystem::exists(candidate / "Changes" / "roadmap.md");
    }

    std::filesystem::path ResolveRepoRoot(std::filesystem::path start)
    {
        std::error_code ec;
        if (start.empty())
            start = std::filesystem::current_path(ec);

        if (std::filesystem::is_regular_file(start, ec))
            start = start.parent_path();

        for (auto candidate = std::filesystem::absolute(start, ec);
            !candidate.empty();
            candidate = candidate.parent_path())
        {
            if (IsRepoRoot(candidate))
                return candidate;

            if (candidate == candidate.root_path())
                break;
        }

        return start;
    }

    bool OptionalPathProbe(const std::filesystem::path& repoRoot)
    {
        const auto engineDir = repoRoot / "Engine";
        const auto assetsDir = engineDir / "assets";
        const auto demoDir = assetsDir / "demo";

        const std::string repoRootText = repoRoot.string();
        LogInfoPair("repo probe", repoRootText);
        LogInfoPair("Engine present", std::filesystem::exists(engineDir) ? 1 : 0);
        LogInfoPair("Engine/assets present", std::filesystem::exists(assetsDir) ? 1 : 0);
        LogInfoPair("demo assets present", std::filesystem::exists(demoDir) ? 1 : 0);

        // Hosted CI must stay asset-light, so this is an informational probe.
        return true;
    }

    bool RequiredControlContractProbe(const std::filesystem::path& repoRoot)
    {
        const auto contractPath = repoRoot / "Engine" / "ai" / "control" / "continuous_build_loop.json";
        std::ifstream in(contractPath);
        if (!in)
        {
            const std::string contractText = contractPath.string();
            LogErrorPair("missing AI control contract", contractText);
            return false;
        }

        const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const bool hasPlanner = content.find("\"stage\": \"planner\"") != std::string::npos;
        const bool hasBuilder = content.find("\"stage\": \"builder\"") != std::string::npos;
        const bool hasVerifier = content.find("\"stage\": \"verifier\"") != std::string::npos;
        const bool hasGate = content.find("\"stage\": \"gate\"") != std::string::npos;
        const bool hasNoBlindWriteThrough =
            content.find("Never allow blind repo write-through") != std::string::npos;

        const std::string contractText = contractPath.string();
        LogInfoPair("AI control contract", contractText);
        LogInfoPair("AI control stages present",
            (hasPlanner && hasBuilder && hasVerifier && hasGate) ? 1 : 0);
        LogInfoPair("AI control gate policy present",
            hasNoBlindWriteThrough ? 1 : 0);

        return hasPlanner && hasBuilder && hasVerifier && hasGate && hasNoBlindWriteThrough;
    }
}

int main(int argc, char** argv)
{
    static_assert(std::is_standard_layout_v<EpochScriptHost>,
        "EpochScriptHost must stay ABI-simple for generated project scripts.");
    static_assert(std::is_same_v<decltype(&epochnamespace::core::bridge::run_legacy_runtime),
        int (*)(bool)>,
        "The runtime bridge signature must remain stable for lightweight callers.");
    static_assert(epoch::core::has_expected == (EPOCH_HAS_EXPECTED != 0),
        "Feature probe macros and constexpr values must agree.");

    SmokeState state{};
    EpochScriptHost host{};
    host.user_data = &state;
    host.log = &SmokeLog;
    host.queue_model_load = &SmokeQueueModelLoad;
    host.project_model_asset = "Engine/assets/demo/minisponza/mini_sponza_v2.gltf";

    host.log(host.user_data, "headless compile/run smoke");
    const int queued = host.queue_model_load(host.user_data, "MiniSponza",
        host.project_model_asset);

    const auto repoRoot = ResolveRepoRoot((argc > 1 && argv && argv[1])
        ? std::filesystem::path(argv[1])
        : std::filesystem::current_path());
    OptionalPathProbe(repoRoot);
    const bool controlContractReady = RequiredControlContractProbe(repoRoot);

    if (!state.logged || !state.queuedModel || queued != 1 || !controlContractReady)
    {
        LogError("script host smoke failed");
        return 1;
    }

    LogInfo("headless smoke passed without graphics dependencies");
    return 0;
}
