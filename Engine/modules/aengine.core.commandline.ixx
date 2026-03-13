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
module;

#include <include/aengine.config.hpp>

export module aengine.core.commandline;

import <algorithm>;
import <cctype>;
import <filesystem>;
import <iostream>;
import <string>;
import <string_view>;

import aengine.context.type;
import aengine.version;

inline constexpr int DEFAULT_WINDOW_WIDTH = 1277;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 1277;

export namespace epochnamespace::core::cli
{
    export enum class RuntimePath
    {
        Epoch,
        Legacy,
    };

    export enum class WindowMode
    {
        Auto,
        Parented,
        Standalone,
    };

    namespace detail
    {
        enum class BackendSelection
        {
            Auto,
            OpenGL,
            SDL,
            SFML,
            RayLib,
            Vulkan,
            Software,
        };

        [[nodiscard]] inline std::string to_lower(std::string_view value)
        {
            std::string lower(value.begin(), value.end());
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
            return lower;
        }

        [[nodiscard]] inline BackendSelection parse_backend(std::string_view value)
        {
            const std::string lowered = to_lower(value);

            if (lowered == "auto") return BackendSelection::Auto;
            if (lowered == "opengl" || lowered == "gl") return BackendSelection::OpenGL;
            if (lowered == "sdl") return BackendSelection::SDL;
            if (lowered == "sfml") return BackendSelection::SFML;
            if (lowered == "raylib" || lowered == "ray") return BackendSelection::RayLib;
            if (lowered == "vulkan" || lowered == "vk") return BackendSelection::Vulkan;
            if (lowered == "software" || lowered == "cpu") return BackendSelection::Software;

            return BackendSelection::Auto;
        }

        [[nodiscard]] inline RuntimePath parse_runtime(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "legacy" || lowered == "compat")
                return RuntimePath::Legacy;
            return RuntimePath::Epoch;
        }

        [[nodiscard]] inline WindowMode parse_window_mode(std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "parented" || lowered == "child" || lowered == "docked")
                return WindowMode::Parented;
            if (lowered == "standalone" || lowered == "top" || lowered == "top-level")
                return WindowMode::Standalone;
            return WindowMode::Auto;
        }

        [[nodiscard]] constexpr bool default_parented_mode() noexcept
        {
#if defined(ALMOND_SINGLE_PARENT) && (ALMOND_SINGLE_PARENT == 1)
            return true;
#else
            return false;
#endif
        }
    }

    inline int  window_width = DEFAULT_WINDOW_WIDTH;
    inline int  window_height = DEFAULT_WINDOW_HEIGHT;
    inline bool window_width_overridden = false;
    inline bool window_height_overridden = false;
    inline int  menu_columns = 4;
    inline bool trace_menu_button0_rect = true;
    inline bool trace_raylib_design_metrics = false;
    inline bool run_menu_loop = false;
    inline bool capture_requested = false;
    inline bool smoke_requested = false;
    inline std::string scene_name{};
    inline std::filesystem::path exe_path;

    inline RuntimePath runtime_path = RuntimePath::Epoch;
    inline WindowMode window_mode = WindowMode::Auto;
    inline bool parented_mode = detail::default_parented_mode();

    inline int raylib_window_count = 1;
    inline int sdl_window_count = 1;
    inline int sfml_window_count = 1;
    inline int vulkan_window_count = 1;
    inline int opengl_window_count = 1;
    inline int software_window_count = 1;

    struct ParseResult
    {
        bool update_requested = false;
        bool force_update = false;
        bool editor_requested = false;
        RuntimePath runtime = RuntimePath::Epoch;
    };

    [[nodiscard]] inline bool apply_backend_selection(std::string_view value)
    {
        using detail::BackendSelection;

        const BackendSelection selected = detail::parse_backend(value);
        if (selected == BackendSelection::Auto && detail::to_lower(value) != "auto")
            return false;

        raylib_window_count = 1;
        sdl_window_count = 1;
        sfml_window_count = 1;
        vulkan_window_count = 1;
        opengl_window_count = 1;
        software_window_count = 1;

        if (selected == BackendSelection::Auto)
            return true;

        raylib_window_count = 0;
        sdl_window_count = 0;
        sfml_window_count = 0;
        vulkan_window_count = 0;
        opengl_window_count = 0;
        software_window_count = 0;

        switch (selected)
        {
        case BackendSelection::OpenGL:
            opengl_window_count = 1;
            break;
        case BackendSelection::SDL:
            sdl_window_count = 1;
            break;
        case BackendSelection::SFML:
            sfml_window_count = 1;
            break;
        case BackendSelection::RayLib:
            raylib_window_count = 1;
            break;
        case BackendSelection::Vulkan:
            vulkan_window_count = 1;
            break;
        case BackendSelection::Software:
            software_window_count = 1;
            break;
        case BackendSelection::Auto:
        default:
            break;
        }

        return true;
    }

    inline void print_engine_info()
    {
        std::cout << epochnamespace::GetEngineName() << " v" << epochnamespace::GetEngineVersion() << '\n';
    }

    inline ParseResult parse(int argc, char* argv[])
    {
        using namespace std::string_view_literals;

        ParseResult result{};
        trace_menu_button0_rect = false;
        trace_raylib_design_metrics = false;
        run_menu_loop = false;
        capture_requested = false;
        smoke_requested = false;
        scene_name.clear();
        window_width_overridden = false;
        window_height_overridden = false;
        window_mode = WindowMode::Auto;
        parented_mode = detail::default_parented_mode();
        runtime_path = RuntimePath::Epoch;

        auto isBackendAutomatic = apply_backend_selection("auto");

        if (argc < 1)
        {
            std::cerr << "No command-line arguments provided.\n";
            return result;
        }

        exe_path = argv[0];
        std::cout << "Commandline for " << exe_path.filename().string() << ":\n";

        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg{ argv[i] };
            std::string_view key = arg;
            std::string_view value{};

            if (const auto eq = arg.find('='); eq != std::string_view::npos)
            {
                key = arg.substr(0, eq);
                value = arg.substr(eq + 1);
            }

            auto read_value = [&](std::string_view name) -> std::string_view
                {
                    if (!value.empty())
                        return value;

                    if (i + 1 < argc)
                        return std::string_view{ argv[++i] };

                    std::cerr << "Missing value for " << name << '\n';
                    return {};
                };

            if (key == "--help"sv || key == "-h"sv)
            {
                std::cout
                    << "  --help, -h                 Show this help message\n"
                    << "  --version, -v              Display the engine version\n"
                    << "  --width <value>            Set window width\n"
                    << "  --height <value>           Set window height\n"
                    << "  --menu-columns <n>         Cap the menu grid at n columns (default 4)\n"
                    << "  --trace-menu-button0       Log GUI bounds for menu button index 0\n"
                    << "  --trace-raylib-design      Log framebuffer vs design canvas dimensions\n"
                    << "  --editor                   Start the editor interface\n"
                    << "  --menu                     Start the menu + games loop\n"
                    << "  --runtime <epoch|legacy>   Select epoch-native or legacy parity runtime\n"
                    << "  --epoch-native             Shortcut for --runtime epoch\n"
                    << "  --legacy-runtime           Shortcut for --runtime legacy\n"
                    << "  --window-mode <mode>       Select auto|parented|standalone\n"
                    << "  --parented                 Shortcut for --window-mode parented\n"
                    << "  --standalone               Shortcut for --window-mode standalone\n"
                    << "  --renderer <backend>       Limit run to one backend\n"
                    << "  --backend <backend>        Alias for --renderer\n"
                    << "  --scene <name>             Optional scene hint for smoke tooling\n"
                    << "  --capture                  Optional capture hint for smoke tooling\n"
                    << "  --smoke                    Run bounded smoke flow where supported\n"
                    << "  --update, -u               Check for a newer epochengine build\n"
                    << "  --force                    Apply the available update immediately\n";
            }
            else if (key == "--version"sv || key == "-v"sv)
            {
                print_engine_info();
            }
            else if (key == "--width"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    window_width = std::stoi(std::string(parsed));
                    window_width_overridden = true;
                }
            }
            else if (key == "--height"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    window_height = std::stoi(std::string(parsed));
                    window_height_overridden = true;
                }
            }
            else if (key == "--menu-columns"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                    menu_columns = (std::max)(1, std::stoi(std::string(parsed)));
            }
            else if (key == "--trace-menu-button0"sv)
            {
                trace_menu_button0_rect = true;
            }
            else if (key == "--trace-raylib-design"sv)
            {
                trace_raylib_design_metrics = true;
            }
            else if (key == "--update"sv || key == "-u"sv)
            {
                result.update_requested = true;
            }
            else if (key == "--editor"sv)
            {
                result.editor_requested = true;
            }
            else if (key == "--menu"sv)
            {
                run_menu_loop = true;
            }
            else if (key == "--force"sv)
            {
                result.force_update = true;
            }
            else if (key == "--runtime"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    runtime_path = detail::parse_runtime(parsed);
                    result.runtime = runtime_path;
                }
            }
            else if (key == "--epoch-native"sv)
            {
                runtime_path = RuntimePath::Epoch;
                result.runtime = runtime_path;
            }
            else if (key == "--legacy-runtime"sv)
            {
                runtime_path = RuntimePath::Legacy;
                result.runtime = runtime_path;
            }
            else if (key == "--window-mode"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    window_mode = detail::parse_window_mode(parsed);
                    if (window_mode == WindowMode::Parented)
                        parented_mode = true;
                    else if (window_mode == WindowMode::Standalone)
                        parented_mode = false;
                }
            }
            else if (key == "--parented"sv)
            {
                window_mode = WindowMode::Parented;
                parented_mode = true;
            }
            else if (key == "--standalone"sv)
            {
                window_mode = WindowMode::Standalone;
                parented_mode = false;
            }
            else if (key == "--renderer"sv || key == "--backend"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty() && !apply_backend_selection(parsed))
                {
                    std::cerr << "Unknown renderer/backend selection: " << parsed << '\n';
                }
            }
            else if (key == "--scene"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                    scene_name = std::string(parsed);
            }
            else if (key == "--capture"sv)
            {
                capture_requested = true;
            }
            else if (key == "--smoke"sv)
            {
                smoke_requested = true;
            }
            else
            {
                std::cerr << "Unknown arg: " << arg << '\n';
            }
        }

        std::cout << '\n';

        if (result.force_update && !result.update_requested)
            std::cout << "[WARN] Ignoring --force without --update.\n";

        return result;
    }
}
