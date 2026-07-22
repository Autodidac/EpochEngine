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
#include "../include/_epoch.stl_types.hpp"
#include "../src/cpp_feature_probe.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>    // std::hash
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

module core.log;

import core.time;

namespace epochengine::core::log
{
    namespace
    {
        std::mutex g_mtx;
        level g_min = level::info;
        bool g_console = true;
        bool g_debugger = false;
        std::ofstream g_file;

        constexpr epochengine::string_view lvl_text(level lvl) noexcept
        {
            switch (lvl)
            {
            case level::trace: return epochengine::string_view{ "TRACE" };
            case level::info:  return epochengine::string_view{ "INFO" };
            case level::warn:  return epochengine::string_view{ "WARN" };
            case level::error: return epochengine::string_view{ "ERROR" };
            case level::off:   return epochengine::string_view{ "OFF" };
            }
            return epochengine::string_view{ "UNKNOWN" };
        }

#if defined(_WIN32)
        static std::wstring utf8_to_wide(epochengine::string_view s)
        {
            if (s.empty() || s.data == nullptr) return {};
            const int wlen = ::MultiByteToWideChar(
                CP_UTF8, 0,
                s.data, static_cast<int>(s.size),
                nullptr, 0
            );
            if (wlen <= 0) return {};

            std::wstring w;
            w.resize(static_cast<std::size_t>(wlen));
            ::MultiByteToWideChar(
                CP_UTF8, 0,
                s.data, static_cast<int>(s.size),
                w.data(), wlen
            );
            return w;
        }
#endif

        static void write_console(std::string_view line) noexcept
        {
#if defined(_WIN32)
            const HANDLE output = ::GetStdHandle(STD_OUTPUT_HANDLE);
            if (!output || output == INVALID_HANDLE_VALUE)
                return;

            const auto write_bytes = [output](std::string_view bytes) noexcept
            {
                std::size_t offset = 0;
                while (offset < bytes.size())
                {
                    const std::size_t remaining = bytes.size() - offset;
                    const DWORD requested = static_cast<DWORD>((std::min)(
                        remaining,
                        static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
                    DWORD written = 0;
                    if (::WriteFile(output, bytes.data() + offset, requested, &written, nullptr) == FALSE
                        || written == 0)
                    {
                        return;
                    }
                    offset += written;
                }
            };

            write_bytes(line);
            write_bytes("\n");
#else
            std::cout.write(line.data(), static_cast<std::streamsize>(line.size()));
            std::cout.put('\n');
#endif
        }

        static void sink_write(epochengine::string_view line)
        {
            const std::string_view sv = epochengine::to_std(line);

            if (g_console)
                write_console(sv);

            if (g_file.is_open())
            {
                g_file.write(sv.data(), static_cast<std::streamsize>(sv.size()));
                g_file.put('\n');
                g_file.flush();
            }

#if defined(_WIN32)
            if (g_debugger)
            {
                // OutputDebugStringA expects NUL-terminated.
                std::string tmp(sv);
                tmp.push_back('\n');
                ::OutputDebugStringA(tmp.c_str());
            }
#endif
        }

        static bool enabled(level lvl) noexcept
        {
            return (g_min != level::off) &&
                (static_cast<unsigned>(lvl) >= static_cast<unsigned>(g_min));
        }

        static void append_u64(std::string& out, std::uint64_t v)
        {
            out.append(std::to_string(v));
        }

        static void append_sv(std::string& out, epochengine::string_view v)
        {
            const auto sv = epochengine::to_std(v);
            out.append(sv.data(), sv.size());
        }
    }

    void set_level(level min_level) noexcept
    {
        std::lock_guard lk(g_mtx);
        g_min = min_level;
    }

    level get_level() noexcept
    {
        std::lock_guard lk(g_mtx);
        return g_min;
    }

    void enable_console(bool on) noexcept
    {
        std::lock_guard lk(g_mtx);
        g_console = on;
    }

    void enable_debugger(bool on) noexcept
    {
        std::lock_guard lk(g_mtx);
#if defined(_WIN32)
        g_debugger = on;
#else
        (void)on;
        g_debugger = false;
#endif
    }

    bool set_file(epochengine::string_view utf8_path) noexcept
    {
        std::lock_guard lk(g_mtx);

        if (g_file.is_open())
            g_file.close();
        g_file.clear();

#if defined(_WIN32)
        const std::wstring wpath = utf8_to_wide(utf8_path);
        if (wpath.empty())
            return false;

        g_file.open(
            std::filesystem::path{ wpath },
            std::ios::binary | std::ios::app);
#else
        const std::string_view sv = epochengine::to_std(utf8_path);
        g_file.open(
            std::filesystem::path{ std::string{ sv } },
            std::ios::binary | std::ios::app);
#endif
        return g_file.is_open();
    }

    void close_file() noexcept
    {
        std::lock_guard lk(g_mtx);
        if (g_file.is_open())
            g_file.close();
        g_file.clear();
    }

    void write(level lvl, epochengine::string_view tag, epochengine::string_view msg)
    {
        std::lock_guard lk(g_mtx);
        if (!enabled(lvl)) return;

        const auto tid = static_cast<std::uint64_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id())
            );
        const std::string timestamp = epochengine::core::time::system_time_string();

        std::string line;
        line.reserve(tag.size + msg.size + 96);

        line.append(timestamp);
        line.append(" [");
        append_sv(line, lvl_text(lvl));
        line.append("] [");
        append_sv(line, tag);
        line.append("] [tid=");
        append_u64(line, tid);
        line.append("] - ");
        append_sv(line, msg);

        sink_write(epochengine::to_view(std::string_view{ line }));
    }

    void write_kv(level lvl,
        epochengine::string_view tag,
        epochengine::string_view msg,
        epochengine::array_view<const kv> fields)
    {
        std::lock_guard lk(g_mtx);
        if (!enabled(lvl)) return;

        const auto tid = static_cast<std::uint64_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id())
            );
        const std::string timestamp = epochengine::core::time::system_time_string();

        std::string line;
        line.reserve(tag.size + msg.size + fields.size * 16 + 128);

        line.append(timestamp);
        line.append(" [");
        append_sv(line, lvl_text(lvl));
        line.append("] [");
        append_sv(line, tag);
        line.append("] [tid=");
        append_u64(line, tid);
        line.append("] - ");
        append_sv(line, msg);

        for (std::size_t i = 0; i < fields.size; ++i)
        {
            const kv& field = fields.data[i];
            line.push_back(' ');
            append_sv(line, field.key);
            line.push_back('=');
            append_sv(line, field.value);
        }

        sink_write(epochengine::to_view(std::string_view{ line }));
    }
}

// C ABI bridge for non-module TUs (e.g., App project).
extern "C" void core_log_write(std::uint32_t lvl, const char* tag_utf8, const char* msg_utf8)
{
    using epochengine::core::log::level;

    auto clamp_level = [](std::uint32_t v) -> level
        {
            if (v > static_cast<std::uint32_t>(level::off))
                return level::off;
            return static_cast<level>(v);
        };

    const level L = clamp_level(lvl);

    const std::string_view tag_sv = tag_utf8 ? std::string_view{ tag_utf8 } : std::string_view{};
    const std::string_view msg_sv = msg_utf8 ? std::string_view{ msg_utf8 } : std::string_view{};

    epochengine::core::log::write(L, epochengine::to_view(tag_sv), epochengine::to_view(msg_sv));
}
