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
// EpochEditor desktop entry point.
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif
//#include "epoch.engine.hpp"
#include <chrono>
#include <filesystem>
#include <source_location>
#include <string_view>
#include <thread>
#include <vector>
#include "../../include/epoch.engine.hpp"
//#include "epoch.engine.hpp"

import epoch.engine; // import the module
import core.logger;

//void test_linkage() {
//    epochengine::opengl::s_state.shader = 42;  // Or just read a member
//}
inline auto sanity() {
    return epochengine::core::RunEngine();

//static void RunEngine() {
    // 🔄 **Cleanup Restart Script on Restart & Old Files on Update**
//#ifdef LEAVE_NO_FILES_ALWAYS_REDOWNLOAD
//#if defined(_WIN32)
//    system("del /F /Q replace_updater.bat >nul 2>&1");
//    system(("rmdir /s /q \"" + std::string(epochengine::updater::REPO.c_str()) + "-main\" >nul 2>&1").c_str());
//#else
//    system("rm -rf replace_updater");
//#endif
//#endif
//
//    if (epochengine::updater::check_for_updates(urls::version_url)) {
//        log.log(epochengine::logger::LogLevel::INFO,
//            "[Engine] New version available.",
//            std::source_location::current());
//        epochengine::updater::update_project(urls::version_url, urls::binary_url);
//    }
//    else {
//        // Clear console before showing "No updates available."
//#if defined(_WIN32)
//        system("cls");
//#else
//        system("clear");
//#endif
//        log.log(epochengine::logger::LogLevel::INFO,
//            "[Engine] No updates available.",
//            std::source_location::current());
//    }





    // Lets Begin
    //TaskScheduler scheduler;

    constexpr std::string_view kLogSystem = "Epoch.Editor";
    auto& log = epochengine::logger::get(kLogSystem);

    log.log(epochengine::logger::LogLevel::INFO,
        "[Engine] Starting up...",
        std::source_location::current());










    //Unleash C++ Scripting!
    /*
    std::string scriptName = "editor_launcher";

    if (!epochengine::scripting::load_or_reload_script(scriptName, scheduler)) {
        log.log(epochengine::logger::LogLevel::ERROR,
            "[Engine] Initial script load failed.",
            std::source_location::current());
    }

    auto lastCheck = std::filesystem::last_write_time("src/scripts/" + scriptName + ".ascript.cpp");
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::filesystem::last_write_time("src/scripts/" + scriptName + ".ascript.cpp");
        if (now != lastCheck) {
            log.log(epochengine::logger::LogLevel::INFO,
                "[Engine] Detected change in script source, recompiling.",
                std::source_location::current());
            epochengine::scripting::load_or_reload_script(scriptName, scheduler);
            lastCheck = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - start).count() > 10) break;
    }
*/
    log.log(epochengine::logger::LogLevel::INFO,
        "[Engine] Session ended.",
        std::source_location::current());
    //return 0;
}
