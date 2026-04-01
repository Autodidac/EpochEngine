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

#include "../include/_epoch.stl_types.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>        // FILE, fopen/fclose/fwrite/fflush
#include <functional>    // std::hash
#include <mutex>
#include <print>
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

namespace epoch::core::log
{
    namespace
    {
        std::mutex g_mtx;
        level g_min = level::info;
        bool g_console = true;
        bool g_debugger = false;
        std::FILE* g_file = nullptr;

        constexpr epoch::string_view lvl_text(level lvl) noexcept
        {
            switch (lvl)
            {
            case level::trace: return epoch::string_view{ "TRACE" };
            case level::info:  return epoch::string_view{ "INFO" };
            case level::warn:  return epoch::string_view{ "WARN" };
            case level::error: return epoch::string_view{ "ERROR" };
            case level::off:   return epoch::string_view{ "OFF" };
            }
            return epoch::string_view{ "UNKNOWN" };
        }

#if defined(_WIN32)
        static std::wstring utf8_to_wide(epoch::string_view s)
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

        static void sink_write(epoch::string_view line)
        {
            // Convert once; std::println wants std::string_view.
            const std::string_view sv = epoch::to_std(line);

            if (g_console)
                std::println("{}", sv);

            if (g_file)
            {
                std::fwrite(sv.data(), 1, sv.size(), g_file);
                std::fwrite("\n", 1, 1, g_file);
                std::fflush(g_file);
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

        static void append_sv(std::string& out, epoch::string_view v)
        {
            const auto sv = epoch::to_std(v);
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

    bool set_file(epoch::string_view utf8_path) noexcept
    {
        std::lock_guard lk(g_mtx);

        if (g_file)
        {
            std::fclose(g_file);
            g_file = nullptr;
        }

#if defined(_WIN32)
        const std::wstring wpath = utf8_to_wide(utf8_path);
        if (wpath.empty())
            return false;

        FILE* f = nullptr;
        if (_wfopen_s(&f, wpath.c_str(), L"ab") != 0)
            return false;

        g_file = f;
        return true;
#else
        // Use std::string_view boundary; do not force epoch::string constructors to accept views.
        const std::string_view sv = epoch::to_std(utf8_path);
        std::string p(sv);
        g_file = std::fopen(p.c_str(), "ab");
        return g_file != nullptr;
#endif
    }

    void close_file() noexcept
    {
        std::lock_guard lk(g_mtx);
        if (g_file)
        {
            std::fclose(g_file);
            g_file = nullptr;
        }
    }

    void write(level lvl, epoch::string_view tag, epoch::string_view msg)
    {
        std::lock_guard lk(g_mtx);
        if (!enabled(lvl)) return;

        const auto tid = static_cast<std::uint64_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id())
            );
        const std::string timestamp = epoch::core::time::system_time_string();

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

        sink_write(epoch::to_view(std::string_view{ line }));
    }

    void write_kv(level lvl,
        epoch::string_view tag,
        epoch::string_view msg,
        epoch::array_view<const kv> fields)
    {
        std::lock_guard lk(g_mtx);
        if (!enabled(lvl)) return;

        const auto tid = static_cast<std::uint64_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id())
            );
        const std::string timestamp = epoch::core::time::system_time_string();

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

        sink_write(epoch::to_view(std::string_view{ line }));
    }
}

// C ABI bridge for non-module TUs (e.g., App project).
extern "C" void core_log_write(std::uint32_t lvl, const char* tag_utf8, const char* msg_utf8)
{
    using epoch::core::log::level;

    auto clamp_level = [](std::uint32_t v) -> level
        {
            if (v > static_cast<std::uint32_t>(level::off))
                return level::off;
            return static_cast<level>(v);
        };

    const level L = clamp_level(lvl);

    const std::string_view tag_sv = tag_utf8 ? std::string_view{ tag_utf8 } : std::string_view{};
    const std::string_view msg_sv = msg_utf8 ? std::string_view{ msg_utf8 } : std::string_view{};

    epoch::core::log::write(L, epoch::to_view(tag_sv), epoch::to_view(msg_sv));
}
