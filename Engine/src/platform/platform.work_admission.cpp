// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch Platform: keep native code isolated behind platform-specific interfaces.
module;

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include "../../include/engine.config.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#if EPOCH_ENABLE_NVIDIA_TELEMETRY && defined(_WIN32) && __has_include(<nvml.h>)
#include <nvml.h>
#define EPOCH_HAS_NVML_TELEMETRY 1
#else
#define EPOCH_HAS_NVML_TELEMETRY 0
#endif

module platform.work_admission;

namespace epochengine::platform::work_admission
{
    namespace
    {
        struct CpuTimes final { std::uint64_t total{}, idle{}; bool valid{}; };

#if EPOCH_HAS_NVML_TELEMETRY
        struct NvmlFunctions final
        {
            decltype(&nvmlInit_v2) initialize{};
            decltype(&nvmlShutdown) shutdown{};
            decltype(&nvmlDeviceGetCount_v2) count{};
            decltype(&nvmlDeviceGetHandleByIndex_v2) device{};
            decltype(&nvmlDeviceGetUtilizationRates) utilization{};
            decltype(&nvmlDeviceGetMemoryInfo) memory{};
            decltype(&nvmlDeviceGetName) name{};
            decltype(&nvmlDeviceGetEncoderUtilization) encoder{};
            decltype(&nvmlDeviceGetDecoderUtilization) decoder{};
            bool initialized{};
        };
        [[nodiscard]] GpuResourceSample read_nvml_resources(const NvmlFunctions& api) noexcept
        {
            GpuResourceSample sample;
            if (!api.initialized || !api.count || !api.device) return sample;
            unsigned count{};
            if (api.count(&count) != NVML_SUCCESS || count == 0u || count > 128u) return sample;
            nvmlDevice_t device{};
            if (api.device(0u, &device) != NVML_SUCCESS || !device) return sample;
            sample.source = GpuTelemetrySource::nvidia_nvml;
            sample.adapter_count = count;
            if (api.name && api.name(device, sample.adapter_name.data(),
                static_cast<unsigned>(sample.adapter_name.size())) == NVML_SUCCESS)
            {
                sample.adapter_name.back() = '\0';
                for (auto& character : sample.adapter_name)
                    if (static_cast<unsigned char>(character) < 32u && character != '\0') character = ' ';
            }
            else sample.adapter_name.fill('\0');
            nvmlUtilization_t utilization{};
            if (api.utilization && api.utilization(device, &utilization) == NVML_SUCCESS)
            {
                if (utilization.gpu <= 100u)
                {
                    sample.busy_fraction = static_cast<double>(utilization.gpu) / 100.0;
                    sample.busy_valid = true;
                }
                if (utilization.memory <= 100u)
                {
                    sample.memory_activity_fraction = static_cast<double>(utilization.memory) / 100.0;
                    sample.memory_activity_valid = true;
                }
            }
            const auto video = [device](auto function, double& fraction,
                std::uint32_t& period, bool& valid) noexcept
            {
                unsigned utilization{}, microseconds{};
                if (function && function(device, &utilization, &microseconds) == NVML_SUCCESS
                    && utilization <= 100u)
                {
                    fraction = static_cast<double>(utilization) / 100.0;
                    period = microseconds;
                    valid = true;
                }
            };
            video(api.encoder, sample.encoder_busy_fraction,
                sample.encoder_sample_period_us, sample.encoder_valid);
            video(api.decoder, sample.decoder_busy_fraction,
                sample.decoder_sample_period_us, sample.decoder_valid);
            nvmlMemory_t memory{};
            if (api.memory && api.memory(device, &memory) == NVML_SUCCESS
                && memory.total > 0u && memory.total <= (1ull << 50u)
                && memory.used <= memory.total && memory.free <= memory.total)
            {
                sample.dedicated_total_bytes = memory.total;
                sample.dedicated_used_bytes = memory.used;
                sample.memory_valid = true;
            }
            return sample;
        }
        struct NativeNvml final
        {
            HMODULE library{};
            NvmlFunctions api{};
            NativeNvml() noexcept
            {
                // Driver-owned DCH location only; never load from the project,
                // current directory or a model-supplied search path.
                library = ::LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (!library) return;
                api.initialize = std::bit_cast<decltype(api.initialize)>(::GetProcAddress(library, "nvmlInit_v2"));
                api.shutdown = std::bit_cast<decltype(api.shutdown)>(::GetProcAddress(library, "nvmlShutdown"));
                api.count = std::bit_cast<decltype(api.count)>(::GetProcAddress(library, "nvmlDeviceGetCount_v2"));
                api.device = std::bit_cast<decltype(api.device)>(::GetProcAddress(library, "nvmlDeviceGetHandleByIndex_v2"));
                api.utilization = std::bit_cast<decltype(api.utilization)>(::GetProcAddress(library, "nvmlDeviceGetUtilizationRates"));
                api.memory = std::bit_cast<decltype(api.memory)>(::GetProcAddress(library, "nvmlDeviceGetMemoryInfo"));
                api.name = std::bit_cast<decltype(api.name)>(::GetProcAddress(library, "nvmlDeviceGetName"));
                api.encoder = std::bit_cast<decltype(api.encoder)>(::GetProcAddress(library, "nvmlDeviceGetEncoderUtilization"));
                api.decoder = std::bit_cast<decltype(api.decoder)>(::GetProcAddress(library, "nvmlDeviceGetDecoderUtilization"));
                api.initialized = api.initialize && api.shutdown && api.count && api.device
                    && api.initialize() == NVML_SUCCESS;
            }
            ~NativeNvml()
            {
                if (api.initialized) (void)api.shutdown();
                if (library) (void)::FreeLibrary(library);
            }
            NativeNvml(const NativeNvml&) = delete;
            NativeNvml& operator=(const NativeNvml&) = delete;
        };
        struct GpuTelemetrySampler final
        {
            std::mutex lifecycle{}, publication{}, wake{};
            std::condition_variable changed{};
            std::thread worker{};
            std::atomic_bool cancelled{true};
            GpuResourceSample cached{};
        };

        // Retained deliberately: an uninterruptible driver call must not make
        // static destruction join, detach, unload its DLL, or destroy its state.
        // Only explicit start allocates; status reads are allocation-free.
        std::atomic<GpuTelemetrySampler*> gpuSampler{};
        std::mutex gpuSamplerCreation{};

        void run_gpu_telemetry(GpuTelemetrySampler& sampler) noexcept
        {
            if (sampler.cancelled.load(std::memory_order_acquire)) return;
            try
            {
                NativeNvml native;
                while (!sampler.cancelled.load(std::memory_order_acquire))
                {
                    const auto began = now_milliseconds();
                    auto sample = read_nvml_resources(native.api);
                    const auto finished = now_milliseconds();
                    sample.sampling_started_at_ms = began;
                    sample.captured_at_ms = finished;
                    if (finished < began || finished - began > 5'000u)
                        sample = {};
                    {
                        const std::lock_guard lock{sampler.publication};
                        if (!sampler.cancelled.load(std::memory_order_acquire))
                            sampler.cached = sample;
                    }
                    // Cadence begins AFTER completion, including slow/failed
                    // queries; a stale-at-entry timestamp cannot cause a loop.
                    std::unique_lock lock{sampler.wake};
                    sampler.changed.wait_for(lock, std::chrono::seconds{1}, [&sampler]
                        { return sampler.cancelled.load(std::memory_order_acquire); });
                }
            } // Driver shutdown and DLL release also belong to this worker.
            catch (...)
            {
                sampler.cancelled.store(true, std::memory_order_release);
            }
        }
#endif

        [[nodiscard]] GpuResourceSample cached_gpu_resources() noexcept
        {
#if EPOCH_HAS_NVML_TELEMETRY
            auto* const sampler = gpuSampler.load(std::memory_order_acquire);
            if (!sampler || sampler->cancelled.load(std::memory_order_acquire))
                return {};
            const std::lock_guard lock{sampler->publication};
            const auto now = now_milliseconds();
            if (sampler->cancelled.load(std::memory_order_acquire)
                || now < sampler->cached.captured_at_ms
                || now - sampler->cached.captured_at_ms > 5'000u)
                return {};
            return sampler->cached;
#else
            return {};
#endif
        }

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

    bool start_gpu_telemetry() noexcept
    {
#if EPOCH_HAS_NVML_TELEMETRY
        try
        {
            GpuTelemetrySampler* sampler{};
            {
                const std::lock_guard lock{gpuSamplerCreation};
                sampler = gpuSampler.load(std::memory_order_acquire);
                if (!sampler)
                {
                    sampler = new GpuTelemetrySampler;
                    gpuSampler.store(sampler, std::memory_order_release);
                }
            }
            const std::lock_guard lock{sampler->lifecycle};
            if (sampler->worker.joinable())
            {
                if (::WaitForSingleObject(sampler->worker.native_handle(), 0u) != WAIT_OBJECT_0)
                    return !sampler->cancelled.load(std::memory_order_acquire);
                sampler->worker.join();
            }
            {
                const std::lock_guard publication{sampler->publication};
                sampler->cached = {};
            }
            sampler->cancelled.store(false, std::memory_order_release);
            try
            {
                sampler->worker = std::thread{[sampler] { run_gpu_telemetry(*sampler); }};
            }
            catch (...)
            {
                sampler->cancelled.store(true, std::memory_order_release);
                return false;
            }
            return true;
        }
        catch (...) { return false; }
#else
        return false;
#endif
    }

    bool stop_gpu_telemetry(std::uint32_t wait_budget_ms) noexcept
    {
#if EPOCH_HAS_NVML_TELEMETRY
        auto* const sampler = gpuSampler.load(std::memory_order_acquire);
        if (!sampler) return true;
        sampler->cancelled.store(true, std::memory_order_release);
        sampler->changed.notify_all();
        try
        {
            std::unique_lock lifecycle{sampler->lifecycle, std::try_to_lock};
            if (!lifecycle.owns_lock()) return false;
            if (!sampler->worker.joinable()) return true;
            // A completion flag can precede actual thread return. Require the
            // native thread to be signaled before the otherwise blocking join.
            if (::WaitForSingleObject(sampler->worker.native_handle(),
                    (std::min)(wait_budget_ms, 100u)) != WAIT_OBJECT_0)
                return false;
            sampler->worker.join();
            return true;
        }
        catch (...) { return false; }
#else
        (void)wait_budget_ms;
        return true;
#endif
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
            std::unique_lock lock{mutex};
            const auto now = now_milliseconds();
            if (sampled && now >= cached.captured_at_ms
                && now - cached.captured_at_ms < 1'000u)
            {
                auto result = cached;
                lock.unlock();
                result.gpu = cached_gpu_resources();
                return result;
            }
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
            lock.unlock();
            next.gpu = cached_gpu_resources();
            return next;
        }
        catch (...)
        {
            // Missing metrics never become a zero-load success observation.
            return { .captured_at_ms = now_milliseconds() };
        }
    }
}
