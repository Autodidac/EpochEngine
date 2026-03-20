/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/

#include "../include/aengine.config.hpp"

#include <exception>
#include <iostream>
#include <string>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  if defined(_DEBUG)
#    include <crtdbg.h>
#  endif
#endif

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
        return std::string("https://api.github.com/repos/") + kOwner + kRepo + "/releases/latest";
    }

    [[nodiscard]] std::string make_binary_url()
    {
        return std::string(kGithubBase) + kOwner + kRepo + "/releases/latest/download/main.zip";
    }

    [[nodiscard]] std::string make_source_url()
    {
        return std::string(kGithubBase) + kOwner + kRepo + "/archive/refs/heads/main.zip";
    }

    [[nodiscard]] std::string make_source_version_url()
    {
        return std::string(kGithubRawBase) + kOwner + kRepo + "/" + kBranch + "Engine/modules/aengine.version.ixx";
    }

#if defined(_WIN32)
    void configure_unattended_windows_error_mode()
    {
        ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

#if defined(_DEBUG)
        if (::IsDebuggerPresent() == FALSE)
        {
            _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        }
#endif
    }
#endif

}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    configure_unattended_windows_error_mode();
#endif
    try
    {
        const auto cli_result = epochnamespace::core::cli::parse(argc, argv);

        if (epochnamespace::core::cli::smoke_requested)
            (void)epoch::core::env::set("DEMO_SMOKE", "1");

        if (cli_result.version_requested && !cli_result.update_requested)
            return 0;

        const epochnamespace::updater::UpdateChannel channel{
            .version_url = make_version_url(),
            .binary_url = make_binary_url(),
            .source_url = make_source_url(),
            .source_version_url = make_source_version_url(),
        };

        if (cli_result.update_requested)
        {
            const auto update_result =
                epochnamespace::updater::run_update_command(channel, cli_result.force_update);

            if (update_result.force_required && !cli_result.force_update)
                return 2;

            if (cli_result.force_update
                && update_result.update_available
                && !update_result.update_performed)
            {
                return 1;
            }

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

