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
import core.log;
import runtime;

namespace
{
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

    void configure_windows_dpi_awareness()
    {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
        using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);
        using SetProcessDPIAwareFn = BOOL(WINAPI*)();

        if (HMODULE user32 = ::LoadLibraryW(L"user32.dll"))
        {
            if (auto setAwarenessContext =
                reinterpret_cast<SetProcessDpiAwarenessContextFn>(
                    ::GetProcAddress(user32, "SetProcessDpiAwarenessContext")))
            {
                if (setAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
                {
                    ::FreeLibrary(user32);
                    return;
                }
            }

            if (auto setDpiAware =
                reinterpret_cast<SetProcessDPIAwareFn>(
                    ::GetProcAddress(user32, "SetProcessDPIAware")))
            {
                if (setDpiAware())
                {
                    ::FreeLibrary(user32);
                    return;
                }
            }

            ::FreeLibrary(user32);
        }

        if (HMODULE shcore = ::LoadLibraryW(L"shcore.dll"))
        {
            if (auto setAwareness =
                reinterpret_cast<SetProcessDpiAwarenessFn>(
                    ::GetProcAddress(shcore, "SetProcessDpiAwareness")))
            {
                (void)setAwareness(2 /* PROCESS_PER_MONITOR_DPI_AWARE */);
            }
            ::FreeLibrary(shcore);
        }
    }
#endif

}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    configure_windows_dpi_awareness();
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
            .version_url = epochnamespace::updater::PROJECT_PACKAGED_VERSION_URL(),
            .binary_url = epochnamespace::updater::PROJECT_BINARY_URL(),
            .source_url = epochnamespace::updater::PROJECT_SOURCE_URL(),
            .source_version_url = epochnamespace::updater::PROJECT_SOURCE_VERSION_URL(),
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
        epoch::core::log::core_log_write(
            static_cast<std::uint32_t>(epoch::core::log::level::error),
            "Epoch.Fatal",
            ex.what());
        return -1;
    }
}
