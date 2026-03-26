/************************************************
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•—  â–ˆâ–ˆâ•—   *
 *  â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â•â•â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ•”â•â•â•  â–ˆâ–ˆâ•”â•â•â•â• â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘     â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘   *
 *  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘     â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘   *
 *  â•šâ•â•â•â•â•â•â•â•šâ•â•      â•šâ•â•â•â•â•â•  â•šâ•â•â•â•â•â•â•šâ•â•  â•šâ•â•   *
 ***********************************************/
module;

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <source_location>
#include <string>
#include <string_view>

#include <include/aengine.config.hpp>

export module aengine.core.commandline;

import aengine.context.type;
import aengine.version;
import aengine.core.logger;

inline constexpr int DEFAULT_WINDOW_WIDTH = 1277;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 1277;

namespace epochnamespace::core::cli
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
        [[nodiscard]] constexpr bool default_updater_shell_mode() noexcept
        {
#if defined(EPOCH_UPDATER_SHELL_BUILD) && (EPOCH_UPDATER_SHELL_BUILD == 1)
            return true;
#else
            return false;
#endif
        }

        [[nodiscard]] constexpr std::string_view default_updater_shell_backend() noexcept
        {
#if defined(_WIN32)
            return "software";
#elif defined(__linux__)
            return "opengl";
#else
            return "software";
#endif
        }

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

        inline void log_info(const std::string& message)
        {
            logger::get("CommandLine").log(
                logger::LogLevel::INFO,
                message,
                std::source_location::current());
        }

        inline void log_warn(const std::string& message)
        {
            logger::get("CommandLine").log(
                logger::LogLevel::WARN,
                message,
                std::source_location::current());
        }

        inline void log_error(const std::string& message)
        {
            logger::get("CommandLine").log(
                logger::LogLevel::Error,
                message,
                std::source_location::current());
        }

        [[nodiscard]] inline std::string to_lower(const std::string_view value)
        {
            std::string lower(value.begin(), value.end());
            std::transform(lower.begin(), lower.end(), lower.begin(), [](const unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
            return lower;
        }

        [[nodiscard]] inline BackendSelection parse_backend(const std::string_view value)
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

        [[nodiscard]] inline RuntimePath parse_runtime(const std::string_view value)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "legacy" || lowered == "compat")
                return RuntimePath::Legacy;
            return RuntimePath::Epoch;
        }

        [[nodiscard]] inline WindowMode parse_window_mode(const std::string_view value)
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
#if defined(EPOCH_SINGLE_PARENT) && (EPOCH_SINGLE_PARENT == 1)
            return true;
#else
            return false;
#endif
        }
    }

    export inline int  window_width = DEFAULT_WINDOW_WIDTH;
    export inline int  window_height = DEFAULT_WINDOW_HEIGHT;
    export inline bool window_width_overridden = false;
    export inline bool window_height_overridden = false;
    export inline int  menu_columns = 4;
    export inline bool trace_menu_button0_rect = true;
    export inline bool trace_raylib_design_metrics = false;
    export inline bool run_menu_loop = false;
    export inline bool capture_requested = false;
    export inline bool smoke_requested = false;
    export inline bool editor_requested = false;
    export inline bool updater_shell_requested = false;
    export inline std::string scene_name{};
    export inline std::filesystem::path exe_path;

    export inline RuntimePath runtime_path = RuntimePath::Epoch;
    export inline WindowMode window_mode = WindowMode::Auto;
    export inline bool parented_mode = detail::default_parented_mode();

    export inline int raylib_window_count = 1;
    export inline int sdl_window_count = 1;
    export inline int sfml_window_count = 1;
    export inline int vulkan_window_count = 1;
    export inline int opengl_window_count = 1;
    export inline int software_window_count = 1;

    export struct ParseResult
    {
        bool version_requested = false;
        bool update_requested = false;
        bool force_update = false;
        bool editor_requested = false;
        RuntimePath runtime = RuntimePath::Epoch;
    };

    export [[nodiscard]] inline bool apply_backend_selection(const std::string_view value)
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
        case BackendSelection::OpenGL:  opengl_window_count = 1;  break;
        case BackendSelection::SDL:     sdl_window_count = 1;     break;
        case BackendSelection::SFML:    sfml_window_count = 1;    break;
        case BackendSelection::RayLib:  raylib_window_count = 1;  break;
        case BackendSelection::Vulkan:  vulkan_window_count = 1;  break;
        case BackendSelection::Software: software_window_count = 1; break;
        case BackendSelection::Auto:
        default:
            break;
        }

        return true;
    }

    export inline void print_engine_info()
    {
        detail::log_info(
            std::string{ epochnamespace::GetEngineName() }
            + " v"
            + std::string{ epochnamespace::GetEngineVersion() });
    }

    export inline ParseResult parse(const int argc, char* argv[])
    {
        using namespace std::string_view_literals;

        ParseResult result{};
        trace_menu_button0_rect = false;
        trace_raylib_design_metrics = false;
        run_menu_loop = false;
        capture_requested = false;
        smoke_requested = false;
        editor_requested = false;
        updater_shell_requested = detail::default_updater_shell_mode();
        scene_name.clear();
        window_width_overridden = false;
        window_height_overridden = false;
        window_mode = WindowMode::Auto;
        parented_mode = detail::default_parented_mode();
        runtime_path = RuntimePath::Epoch;

        (void)apply_backend_selection("auto");

        if (updater_shell_requested)
        {
            run_menu_loop = true;
            menu_columns = 1;
            window_mode = WindowMode::Standalone;
            parented_mode = false;
            window_width = 960;
            window_height = 640;
            (void)apply_backend_selection(detail::default_updater_shell_backend());
        }

        if (argc < 1)
        {
            detail::log_error("No command-line arguments provided.");
            return result;
        }

        exe_path = argv[0];

        std::string rendered_command = exe_path.filename().string();
        if (argc <= 1)
        {
            rendered_command += " (no extra args)";
        }
        else
        {
            for (int i = 1; i < argc; ++i)
            {
                rendered_command.push_back(' ');

                const std::string_view arg{ argv[i] };
                const bool needs_quotes = arg.find_first_of(" \t") != std::string_view::npos;
                if (needs_quotes)
                    rendered_command.push_back('"');
                rendered_command.append(arg);
                if (needs_quotes)
                    rendered_command.push_back('"');
            }
        }

        detail::log_info("Command line: " + rendered_command);

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

            auto read_value = [&](const std::string_view name) -> std::string_view
                {
                    if (!value.empty())
                        return value;

                    if (i + 1 < argc)
                        return std::string_view{ argv[++i] };

                    detail::log_error("Missing value for " + std::string(name));
                    return {};
                };

            if (key == "--help"sv || key == "-h"sv)
            {
                detail::log_info(
                    "  --help, -h                 Show this help message\n"
                    "  --version, -v              Display the engine version\n"
                    "  --width <value>            Set window width\n"
                    "  --height <value>           Set window height\n"
                    "  --menu-columns <n>         Cap the menu grid at n columns (default 4)\n"
                    "  --trace-menu-button0       Log GUI bounds for menu button index 0\n"
                    "  --trace-raylib-design      Log framebuffer vs design canvas dimensions\n"
                    "  --editor                   Start the editor interface\n"
                    "  --menu                     Start the menu + games loop\n"
                    "  --runtime <epoch|legacy>   Select epoch-native or legacy parity runtime\n"
                    "  --epoch-native             Shortcut for --runtime epoch\n"
                    "  --legacy-runtime           Shortcut for --runtime legacy\n"
                    "  --window-mode <mode>       Select auto|parented|standalone\n"
                    "  --parented                 Shortcut for --window-mode parented\n"
                    "  --standalone               Shortcut for --window-mode standalone\n"
                    "  --renderer <backend>       Limit run to one backend\n"
                    "  --backend <backend>        Alias for --renderer\n"
                    "  --scene <name>             Optional scene hint for smoke tooling\n"
                    "  --capture                  Optional capture hint for smoke tooling\n"
                    "  --smoke                    Run bounded smoke flow where supported\n"
                    "  --updater-shell            Start the bootstrap updater shell\n"
                    "  --update, -u               Check for a newer epochengine build\n"
                    "  --force                    Apply the available update immediately\n");
            }
            else if (key == "--version"sv || key == "-v"sv)
            {
                result.version_requested = true;
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
                editor_requested = true;
            }
            else if (key == "--menu"sv)
            {
                run_menu_loop = true;
            }
            else if (key == "--force"sv)
            {
                result.force_update = true;
            }
            else if (key == "--updater-shell"sv)
            {
                updater_shell_requested = true;
                run_menu_loop = true;
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
                    detail::log_error("Unknown renderer/backend selection: " + std::string(parsed));
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
                detail::log_error("Unknown arg: " + std::string(arg));
            }
        }

        if (result.force_update && !result.update_requested)
            detail::log_warn("Ignoring --force without --update.");

        if (updater_shell_requested)
        {
            run_menu_loop = true;
            editor_requested = false;
            result.editor_requested = false;
            runtime_path = RuntimePath::Epoch;
            result.runtime = runtime_path;
            window_mode = WindowMode::Standalone;
            parented_mode = false;
            menu_columns = 1;

            if (!window_width_overridden)
                window_width = 960;
            if (!window_height_overridden)
                window_height = 640;

            (void)apply_backend_selection("software");
        }

        return result;
    }
}
