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

#include "../include/core.stl_types.hpp"

export module core.log;

export namespace epochengine::core::log
{
    enum class level : std::uint32_t
    {
        trace = 0,
        info = 1,
        warn = 2,
        error = 3,
        off = 4,
    };

    struct kv
    {
        epochengine::string_view key{};
        epochengine::string_view value{};
    };

    // Configuration (process-global, but explicit).
    void set_level(level min_level) noexcept;
    level get_level() noexcept;

    void enable_console(bool on) noexcept;   // stdout
    void enable_debugger(bool on) noexcept;  // OutputDebugString on Windows, no-op elsewhere
    bool set_file(epochengine::string_view utf8_path) noexcept; // append mode (UTF-8 path)
    void close_file() noexcept;

    // Log entry points.
    void write(level lvl, epochengine::string_view tag, epochengine::string_view msg);
    void write_kv(level lvl, epochengine::string_view tag, epochengine::string_view msg, epochengine::array_view<const kv> fields);

    inline void trace(epochengine::string_view tag, epochengine::string_view msg) { write(level::trace, tag, msg); }
    inline void info(epochengine::string_view tag, epochengine::string_view msg) { write(level::info, tag, msg); }
    inline void warn(epochengine::string_view tag, epochengine::string_view msg) { write(level::warn, tag, msg); }
    inline void error(epochengine::string_view tag, epochengine::string_view msg) { write(level::error, tag, msg); }

    // C ABI adapter for non-module translation units (App project, tools, etc.)
    // Implemented in src/core.log.cpp.
    extern "C" void core_log_write(std::uint32_t lvl, const char* tag_utf8, const char* msg_utf8);
}
