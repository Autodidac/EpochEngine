///**************************************************************
// *   █████╗ ██╗     ███╗   ███╗   ███╗   ██╗    ██╗██████╗    *
// *  ██╔══██╗██║     ████╗ ████║ ██╔═══██╗████╗  ██║██╔══██╗   *
// *  ███████║██║     ██╔████╔██║ ██║   ██║██╔██╗ ██║██║  ██║   *
// *  ██╔══██║██║     ██║╚██╔╝██║ ██║   ██║██║╚██╗██║██║  ██║   *
// *  ██║  ██║███████╗██║ ╚═╝ ██║ ╚██████╔╝██║ ╚████║██████╔╝   *
// *  ╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝    *
// *                                                            *
// *   This file is part of the Almond Project.                 *
// *   AlmondShell - Modular C++ Framework                      *
// *                                                            *
// *   SPDX-License-Identifier: LicenseRef-MIT-NoSell           *
// *                                                            *
// *   Provided "AS IS", without warranty of any kind.          *
// *   Use permitted for Non-Commercial Purposes ONLY,          *
// *   without prior commercial licensing agreement.            *
// *                                                            *
// *   Redistribution Allowed with This Notice and              *
// *   LICENSE file. No obligation to disclose modifications.   *
// *                                                            *
// *   See LICENSE file for full terms.                         *
// *                                                            *
// **************************************************************/
//module;
//
//#include <include/aengine.config.hpp> // for ALMOND_USING Macros 
//
//#include <chrono>
//#include <filesystem>
//#include <fstream>
//#include <iomanip>
//#include <iostream>
//#include <optional>
//#include <sstream>
//#include <string>
//#include <string_view>
//#include <thread>
//#include <vector>
//
//#if defined(_WIN32)
//#    ifdef ALMOND_USING_WINMAIN
//#        include "../include/aframework.hpp"
//#    endif
//#    ifndef WIN32_LEAN_AND_MEAN
//#        define WIN32_LEAN_AND_MEAN
//#    endif
//#endif
//
//export module aengine.renderer.smoke.harness;
//
//namespace
//{
//    using Clock = std::chrono::steady_clock;
//
//    struct BackendScene
//    {
//        std::string name;
//        std::string renderer_arg;
//        std::string scene_arg;
//        std::string expected_output;
//        bool requires_dock_actions = false;
//        bool include_by_default = true;
//    };
//
//    struct ScheduledAction
//    {
//        enum class Type
//        {
//            Resize,
//            DockDetach,
//            DockAttach,
//            CaptureHint
//        };
//
//        Type type{};
//        int at_seconds = 0;
//        int width = 0;
//        int height = 0;
//        std::string note;
//    };
//
//    struct HarnessOptions
//    {
//        std::filesystem::path binary_path = "./Bin/GCC-Debug/cmakeapp1/cmakeapp1";
//        std::optional<std::string> backend_filter;
//        bool capture = false;
//        int duration_seconds = 60;
//    };
//
//    std::string format_timestamp()
//    {
//        const auto now = std::chrono::system_clock::now();
//        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
//        std::tm tm_time{};
//#if defined(_WIN32)
//        localtime_s(&tm_time, &in_time_t);
//#else
//        localtime_r(&in_time_t, &tm_time);
//#endif
//        std::ostringstream oss;
//        oss << std::put_time(&tm_time, "%Y%m%d-%H%M%S");
//        return oss.str();
//    }
//
//    std::vector<BackendScene> backend_definitions()
//    {
//        return {
//            {
//                "opengl",
//                "opengl",
//                "dockstress",
//                "Stable 4-pane layout, consistent GL viewport sizes, animated quad sampler with no shader recompilation errors.",
//                true,
//                true,
//            },
//            {
//                "sdl",
//                "sdl",
//                "menu_rainbow",
//                "Background hue cycling without hitching, resize events leave canvas centred, command queue drains to empty each frame.",
//                false,
//                true,
//            },
//            {
//                "raylib",
//                "raylib",
//                "fit_canvas",
//                "Render canvas letterboxed with GuiFitViewport scale, framebuffer + logical sizes reported, pointer focus stays aligned after resize/dock, context logs never report a lost binding.",
//                true,
//                true,
//            },
//            {
//                "raylib_nogl",
//                "raylib",
//                "fit_canvas",
//                "GUI letterboxes correctly, logs contain no \"Render context became unavailable\" warnings, viewport and input scaling remain stable.",
//                true,
//                false,
//            },
//            {
//                "software",
//                "software",
//                "cpu_quad",
//                "CPU renderer updates checkerboard quad without tearing, resize callback fires once per logical size, framebuffer buffer length matches width×height.",
//                false,
//                true,
//            },
//        };
//    }
//
//    std::vector<ScheduledAction> build_schedule(bool needs_dock_actions, bool capture_enabled)
//    {
//        std::vector<ScheduledAction> actions;
//        actions.push_back({ ScheduledAction::Type::Resize, 0, 640, 360, "Resize to <50% of baseline" });
//        actions.push_back({ ScheduledAction::Type::Resize, 20, 1920, 1080, "Resize to >150% of baseline" });
//        actions.push_back({ ScheduledAction::Type::Resize, 40, 1280, 720, "Return to baseline" });
//
//        if (needs_dock_actions)
//        {
//            actions.push_back({ ScheduledAction::Type::DockDetach, 1, 0, 0, "Detach docked window" });
//            actions.push_back({ ScheduledAction::Type::DockAttach, 3, 0, 0, "Reattach docked window" });
//        }
//
//        if (capture_enabled)
//        {
//            actions.push_back({ ScheduledAction::Type::CaptureHint, 1, 0, 0, "Capture after first resize" });
//            actions.push_back({ ScheduledAction::Type::CaptureHint, 21, 0, 0, "Capture after second resize" });
//            actions.push_back({ ScheduledAction::Type::CaptureHint, 41, 0, 0, "Capture after third resize" });
//        }
//
//        return actions;
//    }
//
//    HarnessOptions parse_args(int argc, char** argv)
//    {
//        HarnessOptions options{};
//        for (int i = 1; i < argc; ++i)
//        {
//            std::string_view arg{ argv[i] };
//            if (arg == "--binary" && i + 1 < argc)
//            {
//                options.binary_path = argv[++i];
//            }
//            else if (arg == "--backend" && i + 1 < argc)
//            {
//                options.backend_filter = std::string(argv[++i]);
//            }
//            else if (arg == "--capture")
//            {
//                options.capture = true;
//            }
//            else if (arg == "--duration" && i + 1 < argc)
//            {
//                options.duration_seconds = std::stoi(argv[++i]);
//            }
//            else if (arg == "--help" || arg == "-h")
//            {
//                std::cout
//                    << "Renderer smoke harness\n"
//                    << "  --binary <path>    Path to AlmondShell binary (default ./Bin/GCC-Debug/cmakeapp1/cmakeapp1)\n"
//                    << "  --backend <name>   Run a single backend (opengl|sdl|raylib|raylib_nogl|software)\n"
//                    << "  --capture          Enable capture scheduling + asset manifests\n"
//                    << "  --duration <sec>   Override scene duration (default 60)\n";
//            }
//        }
//        return options;
//    }
//
//    std::filesystem::path ensure_backend_log_dir(const std::string& backend)
//    {
//        std::filesystem::path root = "Logs";
//        root /= backend;
//        std::filesystem::create_directories(root);
//        return root;
//    }
//
//    std::filesystem::path build_log_path(const std::filesystem::path& dir, const std::string& stamp)
//    {
//        return dir / ("run-" + stamp + ".log");
//    }
//
//    std::filesystem::path build_manifest_path(const std::filesystem::path& dir, const std::string& stamp)
//    {
//        return dir / ("run-" + stamp + "-manifest.json");
//    }
//
//    std::filesystem::path build_capture_notes_path(const std::filesystem::path& dir, const std::string& stamp)
//    {
//        return dir / ("capture-" + stamp + "-notes.txt");
//    }
//
//#if defined(_WIN32)
//    std::wstring widen(const std::string& value)
//    {
//        if (value.empty()) return L"";
//        int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
//        std::wstring wide(static_cast<size_t>(len), L'\0');
//        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), len);
//        if (!wide.empty() && wide.back() == L'\0')
//        {
//            wide.pop_back();
//        }
//        return wide;
//    }
//
//    std::wstring widen_path(const std::filesystem::path& path)
//    {
//        return path.wstring();
//    }
//
//    std::wstring build_window_title(const std::string& backend)
//    {
//        std::wstring label = widen(backend);
//        if (backend == "opengl") label = L"OpenGL";
//        if (backend == "raylib") label = L"Raylib";
//        if (backend == "raylib_nogl") label = L"Raylib";
//        if (backend == "software") label = L"Software";
//        if (backend == "sdl") label = L"SDL";
//        return label + L" Dock 1";
//    }
//
//    HWND wait_for_window(const std::wstring& title, std::chrono::seconds timeout)
//    {
//        const auto deadline = Clock::now() + timeout;
//        while (Clock::now() < deadline)
//        {
//            if (HWND hwnd = FindWindowW(nullptr, title.c_str()))
//            {
//                return hwnd;
//            }
//            std::this_thread::sleep_for(std::chrono::milliseconds(100));
//        }
//        return nullptr;
//    }
//
//    void resize_window(HWND hwnd, int width, int height)
//    {
//        if (!hwnd) return;
//        RECT rect{ 0, 0, width, height };
//        DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
//        DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
//        AdjustWindowRectEx(&rect, style, FALSE, ex_style);
//        int win_w = rect.right - rect.left;
//        int win_h = rect.bottom - rect.top;
//        SetWindowPos(hwnd, nullptr, 0, 0, win_w, win_h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
//    }
//
//    void set_child_style(HWND hwnd, bool child)
//    {
//        if (!hwnd) return;
//        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
//        if (child)
//        {
//            style &= ~(WS_OVERLAPPEDWINDOW);
//            style |= (WS_CHILD | WS_VISIBLE);
//        }
//        else
//        {
//            style &= ~(WS_CHILD);
//            style |= (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
//        }
//        SetWindowLongPtrW(hwnd, GWL_STYLE, style);
//        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
//            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
//    }
//
//    bool launch_process(const std::filesystem::path& binary,
//        const std::string& args,
//        const std::filesystem::path& log_path,
//        PROCESS_INFORMATION& process_info)
//    {
//        std::wstring command_line = L"\"" + widen_path(binary) + L"\" " + widen(args);
//
//        HANDLE log_file = CreateFileW(
//            widen_path(log_path).c_str(),
//            GENERIC_WRITE,
//            FILE_SHARE_READ | FILE_SHARE_WRITE,
//            nullptr,
//            CREATE_ALWAYS,
//            FILE_ATTRIBUTE_NORMAL,
//            nullptr);
//
//        if (log_file == INVALID_HANDLE_VALUE)
//        {
//            std::cerr << "[Harness] Failed to open log file: " << log_path << "\n";
//            return false;
//        }
//
//        STARTUPINFOW startup_info{};
//        startup_info.cb = sizeof(startup_info);
//        startup_info.dwFlags = STARTF_USESTDHANDLES;
//        startup_info.hStdOutput = log_file;
//        startup_info.hStdError = log_file;
//        startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
//
//        PROCESS_INFORMATION pi{};
//        BOOL success = CreateProcessW(
//            nullptr,
//            command_line.data(),
//            nullptr,
//            nullptr,
//            TRUE,
//            CREATE_NO_WINDOW,
//            nullptr,
//            nullptr,
//            &startup_info,
//            &pi);
//
//        CloseHandle(log_file);
//
//        if (!success)
//        {
//            std::cerr << "[Harness] Failed to launch process for " << binary << "\n";
//            return false;
//        }
//
//        process_info = pi;
//        return true;
//    }
//#endif
//
//    void write_manifest(
//        const std::filesystem::path& manifest_path,
//        const BackendScene& backend,
//        const std::filesystem::path& binary,
//        const std::string& args,
//        const std::vector<ScheduledAction>& schedule,
//        bool capture)
//    {
//        std::ofstream manifest(manifest_path);
//        manifest << "{\n";
//        manifest << "  \"backend\": \"" << backend.name << "\",\n";
//        manifest << "  \"binary\": \"" << binary.generic_string() << "\",\n";
//        manifest << "  \"args\": \"" << args << "\",\n";
//        manifest << "  \"expected_output\": \"" << backend.expected_output << "\",\n";
//        manifest << "  \"capture_enabled\": " << (capture ? "true" : "false") << ",\n";
//        manifest << "  \"schedule\": [\n";
//        for (size_t i = 0; i < schedule.size(); ++i)
//        {
//            const auto& action = schedule[i];
//            manifest << "    {\n";
//            manifest << "      \"type\": \"";
//            switch (action.type)
//            {
//            case ScheduledAction::Type::Resize:
//                manifest << "resize";
//                break;
//            case ScheduledAction::Type::DockDetach:
//                manifest << "dock_detach";
//                break;
//            case ScheduledAction::Type::DockAttach:
//                manifest << "dock_attach";
//                break;
//            case ScheduledAction::Type::CaptureHint:
//                manifest << "capture";
//                break;
//            }
//            manifest << "\",\n";
//            manifest << "      \"at_seconds\": " << action.at_seconds << ",\n";
//            manifest << "      \"width\": " << action.width << ",\n";
//            manifest << "      \"height\": " << action.height << ",\n";
//            manifest << "      \"note\": \"" << action.note << "\"\n";
//            manifest << "    }";
//            if (i + 1 < schedule.size())
//            {
//                manifest << ",";
//            }
//            manifest << "\n";
//        }
//        manifest << "  ]\n";
//        manifest << "}\n";
//    }
//}
//
//int main(int argc, char** argv)
//{
//    const HarnessOptions options = parse_args(argc, argv);
//    const auto stamp = format_timestamp();
//    const auto definitions = backend_definitions();
//
//    for (const auto& backend : definitions)
//    {
//        if (options.backend_filter && *options.backend_filter != backend.name)
//        {
//            continue;
//        }
//        if (!backend.include_by_default && !options.backend_filter)
//        {
//            continue;
//        }
//
//        const auto log_dir = ensure_backend_log_dir(backend.name);
//        const auto log_path = build_log_path(log_dir, stamp);
//        const auto manifest_path = build_manifest_path(log_dir, stamp);
//        const auto capture_path = build_capture_notes_path(log_dir, stamp);
//
//        std::ostringstream args;
//        args << "--renderer=" << backend.renderer_arg
//             << " --scene=" << backend.scene_arg
//             << " --width 1280 --height 720";
//        if (options.capture)
//        {
//            args << " --capture";
//        }
//
//        const auto schedule = build_schedule(backend.requires_dock_actions, options.capture);
//
//        write_manifest(manifest_path, backend, options.binary_path, args.str(), schedule, options.capture);
//
//        if (options.capture)
//        {
//            std::ofstream capture_notes(capture_path);
//            capture_notes << "Capture schedule for backend " << backend.name << "\n";
//            capture_notes << "- OpenGL: RenderDoc capture on second frame after each resize.\n";
//            capture_notes << "- SDL: OBS capture after each resize, verify SDL renderer stats.\n";
//            capture_notes << "- Raylib: TakeScreenshot after each resize.\n";
//            capture_notes << "- Software: dump framebuffer PNG after each resize.\n";
//            capture_notes << "\n";
//            capture_notes << "Actions:\n";
//            for (const auto& action : schedule)
//            {
//                capture_notes << "  - t=" << action.at_seconds << "s: " << action.note << "\n";
//            }
//        }
//
//#if defined(_WIN32)
//        if (options.capture)
//        {
//            SetEnvironmentVariableW(L"ALMOND_CAPTURE_DIR", widen_path(log_dir).c_str());
//        }
//
//        PROCESS_INFORMATION pi{};
//        if (!launch_process(options.binary_path, args.str(), log_path, pi))
//        {
//            return 1;
//        }
//
//        const auto window_title = build_window_title(backend.name);
//        HWND hwnd = wait_for_window(window_title, std::chrono::seconds(10));
//        HWND parent = nullptr;
//        if (backend.requires_dock_actions)
//        {
//            parent = wait_for_window(L"Almond Docking", std::chrono::seconds(10));
//        }
//
//        const auto start = Clock::now();
//        size_t action_index = 0;
//
//        while (true)
//        {
//            DWORD wait_result = WaitForSingleObject(pi.hProcess, 0);
//            if (wait_result == WAIT_OBJECT_0)
//            {
//                break;
//            }
//
//            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start).count();
//            if (elapsed >= options.duration_seconds)
//            {
//                TerminateProcess(pi.hProcess, 0);
//                break;
//            }
//
//            while (action_index < schedule.size() && elapsed >= schedule[action_index].at_seconds)
//            {
//                const auto& action = schedule[action_index];
//                switch (action.type)
//                {
//                case ScheduledAction::Type::Resize:
//                    resize_window(hwnd, action.width, action.height);
//                    break;
//                case ScheduledAction::Type::DockDetach:
//                    if (hwnd)
//                    {
//                        SetParent(hwnd, nullptr);
//                        set_child_style(hwnd, false);
//                    }
//                    break;
//                case ScheduledAction::Type::DockAttach:
//                    if (hwnd && parent)
//                    {
//                        SetParent(hwnd, parent);
//                        set_child_style(hwnd, true);
//                    }
//                    break;
//                case ScheduledAction::Type::CaptureHint:
//                    break;
//                }
//                ++action_index;
//            }
//
//            std::this_thread::sleep_for(std::chrono::milliseconds(200));
//        }
//
//        CloseHandle(pi.hThread);
//        CloseHandle(pi.hProcess);
//#else
//        std::ostringstream command;
//        command << options.binary_path.string() << " " << args.str() << " > " << log_path.string() << " 2>&1";
//        std::cout << "[Harness] Non-Windows run: " << command.str() << "\n";
//        const int result = std::system(command.str().c_str());
//        if (result != 0)
//        {
//            std::cerr << "[Harness] Command failed with status " << result << "\n";
//        }
//#endif
//    }
//
//    return 0;
//}
