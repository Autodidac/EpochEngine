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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <include/engine.config.hpp>

export module core.commandline;

import context.type;
import epoch.version;
import core.logger;
import core.path;
import platform.engine;

inline constexpr int DEFAULT_WINDOW_WIDTH = 1277;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 1277;

namespace epochengine::core::cli
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
        [[nodiscard]] inline std::mutex& capture_mutex() noexcept
        {
            static std::mutex mutex{};
            return mutex;
        }

        [[nodiscard]] inline std::unordered_set<std::string>& captured_outputs()
        {
            static std::unordered_set<std::string> outputs{};
            return outputs;
        }

        [[nodiscard]] inline std::unordered_map<std::string, std::uint32_t>& capture_frame_counts()
        {
            static std::unordered_map<std::string, std::uint32_t> counts{};
            return counts;
        }

        [[nodiscard]] constexpr bool default_updater_shell_mode() noexcept
        {
#if defined(EPOCH_UPDATER_SHELL_BUILD) && (EPOCH_UPDATER_SHELL_BUILD == 1)
            return true;
#else
            return false;
#endif
        }

        [[nodiscard]] inline std::string_view default_updater_shell_backend() noexcept
        {
            return epochengine::platform::policy::updater_shell_backend_name();
        }

        [[nodiscard]] inline std::string standalone_only_window_message(const std::string_view action)
        {
            return std::string{ epochengine::platform::policy::current_runtime_policy().platform_key }
                + " platform policy keeps runtime windows standalone-only; "
                + std::string{ action };
        }

        enum class BackendSelection
        {
            Auto,
            OpenGL,
            SDL,
            SFML,
            RayLib,
            Vulkan,
            DirectX,
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

        [[nodiscard]] inline std::string sanitize_capture_token(const std::string_view value)
        {
            std::string token;
            token.reserve(value.size());

            bool previousWasDash = false;
            for (const unsigned char ch : value)
            {
                if (std::isalnum(ch) != 0)
                {
                    token.push_back(static_cast<char>(std::tolower(ch)));
                    previousWasDash = false;
                }
                else if (!previousWasDash)
                {
                    token.push_back('-');
                    previousWasDash = true;
                }
            }

            while (!token.empty() && token.front() == '-')
                token.erase(token.begin());
            while (!token.empty() && token.back() == '-')
                token.pop_back();

            return token.empty() ? std::string{ "capture" } : token;
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
            if (lowered == "directx" || lowered == "dx" || lowered == "d3d" || lowered == "d3d11") return BackendSelection::DirectX;
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
                return epochengine::platform::policy::supports_parented_multiwindow()
                    ? WindowMode::Parented
                    : WindowMode::Standalone;
            if (lowered == "standalone" || lowered == "top" || lowered == "top-level")
                return WindowMode::Standalone;
            return WindowMode::Auto;
        }

        [[nodiscard]] inline bool parse_frame_limit(const std::string_view value, double& out_fps)
        {
            const std::string lowered = to_lower(value);
            if (lowered == "unlimited" || lowered == "uncapped" || lowered == "off" || lowered == "0")
            {
                out_fps = 0.0;
                return true;
            }

            std::string text(value);
            char* end = nullptr;
            const double parsed = std::strtod(text.c_str(), &end);
            if (end == text.c_str() || parsed < 0.0)
                return false;

            out_fps = parsed;
            return true;
        }

        [[nodiscard]] constexpr bool default_parented_mode() noexcept
        {
            return epochengine::platform::policy::default_parented_multiwindow();
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
    export inline bool smoke_context_switch_requested = false;
    export inline bool editor_requested = false;
    export inline bool updater_shell_requested = false;
    export inline bool backend_selection_explicit = false;
    export inline bool frame_limit_explicit = false;
    export inline double frame_limit_fps = 0.0;
    export inline std::uint32_t capture_warmup_frames = 12;
    export std::string scene_name{};
    export std::string smoke_context_switch_backend{};
    export std::filesystem::path exe_path;

    export inline RuntimePath runtime_path = RuntimePath::Epoch;
    export inline WindowMode window_mode = WindowMode::Auto;
    export bool parented_mode = detail::default_parented_mode();

    export inline int raylib_window_count = 1;
    export inline int sdl_window_count = 1;
    export inline int sfml_window_count = 1;
    export inline int vulkan_window_count = 1;
    export inline int opengl_window_count = 1;
#if defined(_WIN32)
    export inline int directx_window_count = 1;
#else
    export inline int directx_window_count = 0;
#endif
    export inline int software_window_count = 0;

    export [[nodiscard]] inline std::filesystem::path capture_output_root()
    {
        return epochengine::core::path::capture_output_dir();
    }

    export [[nodiscard]] inline std::string capture_output_stem()
    {
        if (!scene_name.empty())
            return detail::sanitize_capture_token(scene_name);

        if (!exe_path.empty())
            return detail::sanitize_capture_token(exe_path.stem().string());

        return "capture";
    }

    export [[nodiscard]] inline std::filesystem::path reserve_capture_path(
        const std::string_view backend,
        const std::uintptr_t windowId = 0,
        const std::string_view extension = ".bmp")
    {
        if (!capture_requested)
            return {};

        const std::string stem = capture_output_stem();
        const std::string backendToken = detail::sanitize_capture_token(backend);
        const std::string key = stem + "|" + backendToken + "|" + std::to_string(windowId);

        std::lock_guard guard(detail::capture_mutex());
        const auto frameCount = ++detail::capture_frame_counts()[key];
        if (smoke_requested && frameCount < capture_warmup_frames)
            return {};

        if (!detail::captured_outputs().insert(key).second)
            return {};

        const auto root = capture_output_root();
        std::error_code ec;
        std::filesystem::create_directories(root, ec);

        std::string filename = stem + "-" + backendToken;
        if (windowId != 0)
            filename += "-" + std::to_string(windowId);
        filename += std::string(extension);
        return root / filename;
    }

    export struct ParseResult
    {
        bool version_requested = false;
        bool update_requested = false;
        bool force_update = false;
        bool editor_requested = false;
        bool editor_project_self_test_requested = false;
        bool editor_ai_gate_self_test_requested = false;
        bool engine_contract_self_test_requested = false;
        bool engine_validation_self_test_requested = false;
        RuntimePath runtime = RuntimePath::Epoch;
        std::string editor_project_self_test_id{};
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
#if defined(_WIN32)
        directx_window_count = 1;
#else
        directx_window_count = 0;
#endif
        software_window_count = 0;

        if (selected == BackendSelection::Auto)
            return true;

        raylib_window_count = 0;
        sdl_window_count = 0;
        sfml_window_count = 0;
        vulkan_window_count = 0;
        opengl_window_count = 0;
        directx_window_count = 0;
        software_window_count = 0;

        switch (selected)
        {
        case BackendSelection::OpenGL:  opengl_window_count = 1;  break;
        case BackendSelection::SDL:     sdl_window_count = 1;     break;
        case BackendSelection::SFML:    sfml_window_count = 1;    break;
        case BackendSelection::RayLib:  raylib_window_count = 1;  break;
        case BackendSelection::Vulkan:  vulkan_window_count = 1;  break;
        case BackendSelection::DirectX:
#if defined(_WIN32)
            directx_window_count = 1;
            break;
#else
            return false;
#endif
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
            std::string{ epochengine::GetEngineName() }
            + " v"
            + std::string{ epochengine::GetEngineVersion() });
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
        smoke_context_switch_requested = false;
        smoke_context_switch_backend.clear();
        editor_requested = false;
        updater_shell_requested = detail::default_updater_shell_mode();
        scene_name.clear();
        window_width_overridden = false;
        window_height_overridden = false;
        window_mode = WindowMode::Auto;
        parented_mode = detail::default_parented_mode();
        runtime_path = RuntimePath::Epoch;
        backend_selection_explicit = false;
        frame_limit_explicit = false;
        frame_limit_fps = 0.0;

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
                    "  --editor-project-self-test <id>\n"
                    "                             Materialize and build an editor project shell, then exit\n"
                    "  --editor-ai-gate-self-test\n"
                    "                             Run deterministic self-iteration helper gate checks, then exit\n"
                    "  --engine-contract-self-test\n"
                    "                             Run pure engine contract checks, then exit\n"
                    "  --menu                     Start the menu + games loop\n"
                    "  --runtime <epoch|legacy>   Select epoch-native or legacy parity runtime\n"
                    "  --epoch-native             Shortcut for --runtime epoch\n"
                    "  --legacy-runtime           Shortcut for --runtime legacy\n"
                    "  --window-mode <mode>       Select auto|parented|standalone\n"
                    "  --parented                 Shortcut for --window-mode parented\n"
                    "  --standalone               Shortcut for --window-mode standalone\n"
                    "  --renderer <backend|auto>  Select one backend; auto requests the backend grid\n"
                    "                             Backends: auto, opengl, directx/d3d11, vulkan, raylib, sdl, sfml, software\n"
                    "  --backend <backend|auto>   Alias for --renderer\n"
                    "  --frame-limit <fps|unlimited>\n"
                    "                             Use the engine core frame limiter; common values: 60, 120, unlimited\n"
                    "  --scene <name>             Optional scene hint for smoke tooling\n"
                    "  --capture                  Optional capture hint for smoke tooling\n"
                    "  --smoke                    Run bounded smoke flow where supported\n"
                    "  --smoke-switch-context <backend>\n"
                    "                             During editor smoke, request one toolbar-equivalent context switch\n"
                    "  --updater-shell            Start the bootstrap updater shell\n"
                    "  --update, -u               Check for a newer epochengine build\n"
                    "  --force                    Apply the available update immediately\n"
                    "  --editor-project-self-test <id>\n"
                    "                             Materialize, build, and child-smoke one project profile\n"
                    "  --editor-ai-gate-self-test\n"
                    "                             Validate the OS AI evidence/promotion gate\n"
                    "  --engine-contract-self-test\n"
                    "                             Validate Forest Factory, timeline, and scene snapshot contracts only\n"
                    "  --engine-validation-self-test\n"
                    "                             Run project, child-runtime, and AI gate validation lanes\n");
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
            else if (key == "--editor-project-self-test"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    result.editor_project_self_test_requested = true;
                    result.editor_project_self_test_id = std::string(parsed);
                }
            }
            else if (key == "--editor-ai-gate-self-test"sv)
            {
                result.editor_ai_gate_self_test_requested = true;
            }
            else if (key == "--engine-contract-self-test"sv)
            {
                result.engine_contract_self_test_requested = true;
            }
            else if (key == "--engine-validation-self-test"sv)
            {
                result.engine_validation_self_test_requested = true;
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
                    if (!epochengine::platform::policy::supports_parented_multiwindow()
                        && (detail::to_lower(parsed) == "parented"
                            || detail::to_lower(parsed) == "child"
                            || detail::to_lower(parsed) == "docked"))
                    {
                        detail::log_warn(detail::standalone_only_window_message(
                            "treating parented mode as standalone."));
                    }
                }
            }
            else if (key == "--parented"sv)
            {
                if (!epochengine::platform::policy::supports_parented_multiwindow())
                {
                    detail::log_warn(detail::standalone_only_window_message(
                        "ignoring --parented."));
                    window_mode = WindowMode::Standalone;
                    parented_mode = false;
                }
                else
                {
                    window_mode = WindowMode::Parented;
                    parented_mode = true;
                }
            }
            else if (key == "--standalone"sv)
            {
                window_mode = WindowMode::Standalone;
                parented_mode = false;
            }
            else if (key == "--renderer"sv || key == "--backend"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    if (!apply_backend_selection(parsed))
                    {
                        detail::log_error("Unknown renderer/backend selection: " + std::string(parsed));
                    }
                    else
                    {
                        backend_selection_explicit = true;
                    }
                }
            }
            else if (key == "--frame-limit"sv || key == "--fps-limit"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    double requested = 0.0;
                    if (detail::parse_frame_limit(parsed, requested))
                    {
                        frame_limit_explicit = true;
                        frame_limit_fps = requested;
                    }
                    else
                    {
                        detail::log_error("Invalid frame limit: " + std::string(parsed));
                    }
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
            else if (key == "--smoke-switch-context"sv)
            {
                const auto parsed = read_value(key);
                if (!parsed.empty())
                {
                    const auto selected = detail::parse_backend(parsed);
                    if (selected == detail::BackendSelection::Auto && detail::to_lower(parsed) != "auto")
                    {
                        detail::log_error("Unknown smoke context switch backend: " + std::string(parsed));
                    }
                    else
                    {
                        smoke_context_switch_requested = true;
                        smoke_context_switch_backend = detail::to_lower(parsed);
                        smoke_requested = true;
                        editor_requested = true;
                        result.editor_requested = true;
                    }
                }
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

            if (!backend_selection_explicit)
                (void)apply_backend_selection(detail::default_updater_shell_backend());

            detail::log_info(
                "Updater shell backend: "
                + std::string(detail::default_updater_shell_backend()));
        }

        return result;
    }
}
