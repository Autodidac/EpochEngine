#include "../include/aengine.config.hpp"

#include <exception>
#include <iostream>
#include <string>

import aengine.cli;
import aengine.updater;
import core.env;
import runtime;

namespace
{
    constexpr const char* kGithubBase = "https://github.com/";
    constexpr const char* kGithubRawBase = "https://raw.githubusercontent.com/";
    constexpr const char* kOwner = "Autodidac/";
    constexpr const char* kRepo = "EpochEngine";
    constexpr const char* kBranch = "main/";

    [[nodiscard]] std::string make_version_url()
    {
        return std::string(kGithubRawBase) + kOwner + kRepo + "/" + kBranch + "/modules/aengine.version.ixx";
    }

    [[nodiscard]] std::string make_binary_url()
    {
        return std::string(kGithubBase) + kOwner + kRepo + "/releases/latest/download/ConsoleApplication1.exe";
    }
}

int main(int argc, char** argv)
{
    try
    {
        const auto cli_result = epochnamespace::core::cli::parse(argc, argv);

        if (epochnamespace::core::cli::smoke_requested)
            (void)epoch::core::env::set("DEMO_SMOKE", "1");

        const epochnamespace::updater::UpdateChannel channel{
            .version_url = make_version_url(),
            .binary_url = make_binary_url(),
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                epochnamespace::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            return 0;
        }

        runtime::LaunchOptions launch{};
        launch.editor_requested = cli_result.editor_requested;
        launch.path = (cli_result.runtime == epochnamespace::core::cli::RuntimePath::Legacy)
            ? runtime::Path::LegacyParity
            : runtime::Path::EpochNative;

        return runtime::run(launch);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[Fatal] " << ex.what() << '\n';
        return -1;
    }
}

