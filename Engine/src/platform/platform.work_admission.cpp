// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

module platform.work_admission;

namespace epochengine::platform::work_admission
{
    namespace
    {
        struct CpuTimes final { std::uint64_t total{}, idle{}; bool valid{}; };

        [[nodiscard]] bool add_counter(std::uint64_t& total, std::uint64_t value) noexcept
        {
            if (value > (std::numeric_limits<std::uint64_t>::max)() - total)
                return false;
            total += value;
            return true;
        }

        [[nodiscard]] CpuTimes read_resources(ResourceSample& sample)
        {
#if defined(_WIN32)
            MEMORYSTATUSEX memory{};
            memory.dwLength = sizeof(memory);
            if (::GlobalMemoryStatusEx(&memory))
            {
                sample.total_memory_bytes = memory.ullTotalPhys;
                sample.available_memory_bytes = memory.ullAvailPhys;
                sample.memory_valid = memory.ullTotalPhys != 0u
                    && memory.ullAvailPhys <= memory.ullTotalPhys;
            }
            FILETIME idle{}, kernel{}, user{};
            if (!::GetSystemTimes(&idle, &kernel, &user)) return {};
            const auto counter = [](const FILETIME& time) noexcept
            {
                return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32u)
                    | time.dwLowDateTime;
            };
            CpuTimes result{counter(kernel), counter(idle), true};
            if (!add_counter(result.total, counter(user)) || result.idle > result.total)
                return {};
            return result;
#elif defined(__linux__)
            std::ifstream memory{"/proc/meminfo"};
            std::string key{}, unit{};
            std::uint64_t value{};
            bool total_found{}, available_found{};
            // /proc is a host-owned bounded observation, not a model-supplied path.
            for (unsigned line = 0u; line < 128u && memory >> key >> value >> unit; ++line)
            {
                if (unit != "kB" || value > (std::numeric_limits<std::uint64_t>::max)() / 1024u)
                    continue;
                if (key == "MemTotal:")
                {
                    sample.total_memory_bytes = value * 1024u;
                    total_found = true;
                }
                else if (key == "MemAvailable:")
                {
                    sample.available_memory_bytes = value * 1024u;
                    available_found = true;
                }
                if (total_found && available_found) break;
            }
            sample.memory_valid = total_found && available_found
                && sample.total_memory_bytes != 0u
                && sample.available_memory_bytes <= sample.total_memory_bytes;

            std::ifstream cpu{"/proc/stat"};
            std::array<char, 2'048u> line{};
            if (!cpu.getline(line.data(), static_cast<std::streamsize>(line.size()))) return {};
            std::istringstream fields{line.data()};
            std::array<std::uint64_t, 8u> counters{};
            if (!(fields >> key) || key != "cpu") return {};
            for (auto& count : counters)
                if (!(fields >> count)) return {};
            CpuTimes result{};
            for (const auto count : counters)
                if (!add_counter(result.total, count)) return {};
            result.idle = counters[3u];
            if (!add_counter(result.idle, counters[4u]) || result.idle > result.total)
                return {};
            result.valid = true;
            return result;
#else
            (void)sample;
            return {};
#endif
        }
    }

    std::uint64_t now_milliseconds() noexcept
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0u;
    }

    ResourceSample sample_host_resources() noexcept
    {
        // One short sample per second for the process, including multiple UI
        // contexts. No sleeping, child probes or GPU context initialization.
        static std::mutex mutex{};
        static ResourceSample cached{};
        static CpuTimes previous{};
        static bool sampled{};
        try
        {
            const std::lock_guard lock{mutex};
            const auto now = now_milliseconds();
            if (sampled && now >= cached.captured_at_ms
                && now - cached.captured_at_ms < 1'000u)
                return cached;
            ResourceSample next{};
            next.captured_at_ms = now;
            const auto current = read_resources(next);
            if (sampled && now > cached.captured_at_ms
                && now - cached.captured_at_ms <= 5'000u
                && current.valid && previous.valid
                && current.total > previous.total && current.idle >= previous.idle)
            {
                const auto total = current.total - previous.total;
                const auto idle = current.idle - previous.idle;
                if (idle <= total)
                {
                    next.cpu_busy_fraction = static_cast<double>(total - idle)
                        / static_cast<double>(total);
                    next.cpu_valid = true;
                }
            }
            previous = current;
            cached = next;
            sampled = true;
            return cached;
        }
        catch (...)
        {
            // Missing metrics never become a zero-load success observation.
            return { .captured_at_ms = now_milliseconds() };
        }
    }
}
