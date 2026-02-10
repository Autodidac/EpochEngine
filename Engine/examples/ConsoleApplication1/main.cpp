/**************************************************************
 *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
 *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
 *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
 *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
 *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
 *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
 *                                                            *
 *   This file is part of the Almond Project.                 *
 *   AlmondShell - Modular C++ Framework                      *
 *                                                            *
 *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
 *                                                            *
 *   Provided "AS IS", without warranty of any kind.          *
 *   Use permitted for non-commercial purposes only           *
 *   without prior commercial licensing agreement.            *
 *                                                            *
 *   Redistribution allowed with this notice.                 *
 *   No obligation to disclose modifications.                 *
 *   See LICENSE file for full terms.                         *
 **************************************************************/
//main.cpp - the console demonstration of Almond Shell / Almond Engine
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif
//#include "aengine.hpp"
#include <chrono>
#include <filesystem>
#include <source_location>
#include <string_view>
#include <thread>
#include <vector>
#include "../../include/aengine.hpp"
//#include "aengine.hpp"

import almondshell; // import the module
import aengine.core.logger;

#define RAYLIB_STATIC

//void test_linkage() {
//    almondshell::opengl::s_state.shader = 42;  // Or just read a member
//}
inline auto sanity() {
    return almondnamespace::core::RunEngine();

//static void RunEngine() {
    // 🔄 **Cleanup Restart Script on Restart & Old Files on Update**
//#ifdef LEAVE_NO_FILES_ALWAYS_REDOWNLOAD
//#if defined(_WIN32)
//    system("del /F /Q replace_updater.bat >nul 2>&1");
//    system(("rmdir /s /q \"" + std::string(almondnamespace::updater::REPO.c_str()) + "-main\" >nul 2>&1").c_str());
//#else
//    system("rm -rf replace_updater");
//#endif
//#endif
//
//    if (almondnamespace::updater::check_for_updates(urls::version_url)) {
//        std::cout << "[INFO] New version available!\n";
//        almondnamespace::updater::update_project(urls::version_url, urls::binary_url);
//    }
//    else {
//        // Clear console before showing "No updates available."
//#if defined(_WIN32)
//        system("cls");
//#else
//        system("clear");
//#endif
//        std::cout << "[INFO] No updates available.\n";
//    }





    // Lets Begin
    //TaskScheduler scheduler;

    constexpr std::string_view kLogSystem = "Example.ConsoleApp";
    auto& log = almondnamespace::logger::get(kLogSystem);

    log.log(almondnamespace::logger::LogLevel::INFO,
        "[Engine] Starting up...",
        std::source_location::current());










    //Unleash C++ Scripting!
    /*
    std::string scriptName = "editor_launcher";

    if (!almond::load_or_reload_script(scriptName, scheduler)) {
        std::cerr << "[Engine] Initial script load failed.\n";
    }

    auto lastCheck = std::filesystem::last_write_time("src/scripts/" + scriptName + ".ascript.cpp");
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::filesystem::last_write_time("src/scripts/" + scriptName + ".ascript.cpp");
        if (now != lastCheck) {
            std::cout << "\n[Engine] Detected change in script source, recompiling...\n";
            almond::load_or_reload_script(scriptName, scheduler);
            lastCheck = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - start).count() > 10) break;
    }
*/
    log.log(almondnamespace::logger::LogLevel::INFO,
        "[Engine] Session ended.",
        std::source_location::current());
    //return 0;
}
