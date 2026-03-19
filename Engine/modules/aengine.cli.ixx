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
export module aengine.cli;

import aengine.core.commandline;

export namespace epochnamespace::core::cli
{
    export using ::epochnamespace::core::cli::RuntimePath;
    export using ::epochnamespace::core::cli::WindowMode;

    export using ::epochnamespace::core::cli::ParseResult;
    export using ::epochnamespace::core::cli::parse;

    export using ::epochnamespace::core::cli::window_width;
    export using ::epochnamespace::core::cli::window_height;
    export using ::epochnamespace::core::cli::window_width_overridden;
    export using ::epochnamespace::core::cli::window_height_overridden;
    export using ::epochnamespace::core::cli::menu_columns;
    export using ::epochnamespace::core::cli::trace_menu_button0_rect;
    export using ::epochnamespace::core::cli::trace_raylib_design_metrics;
    export using ::epochnamespace::core::cli::run_menu_loop;
    export using ::epochnamespace::core::cli::capture_requested;
    export using ::epochnamespace::core::cli::smoke_requested;
    export using ::epochnamespace::core::cli::updater_shell_requested;
    export using ::epochnamespace::core::cli::scene_name;
    export using ::epochnamespace::core::cli::exe_path;

    export using ::epochnamespace::core::cli::runtime_path;
    export using ::epochnamespace::core::cli::window_mode;
    export using ::epochnamespace::core::cli::parented_mode;

    export using ::epochnamespace::core::cli::raylib_window_count;
    export using ::epochnamespace::core::cli::sdl_window_count;
    export using ::epochnamespace::core::cli::sfml_window_count;
    export using ::epochnamespace::core::cli::vulkan_window_count;
    export using ::epochnamespace::core::cli::opengl_window_count;
    export using ::epochnamespace::core::cli::software_window_count;

    export using ::epochnamespace::core::cli::print_engine_info;
}
