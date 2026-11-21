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
#include <compare>
#include <cstdint>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>
#include "../../include/engine.config.hpp"

#if defined(__linux__)
#include <filesystem>
#include <unistd.h>
#endif

#if defined(_WIN32)
#include "platform.process_observation.hpp"
#include <dxgi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
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

        [[nodiscard]] bool add_counter(std::uint64_t& total, std::uint64_t value) noexcept
        {
            if (value > (std::numeric_limits<std::uint64_t>::max)() - total)
                return false;
            total += value;
            return true;
        }

#if defined(_WIN32)
        [[nodiscard]] std::uint64_t time_ticks(const FILETIME& time) noexcept
        {
            return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32u)
                | time.dwLowDateTime;
        }

        struct EpochCpuCounter final
        {
            std::uint64_t generation{}, ticks{};
            bool valid{};
        };

        struct EpochCpuFrame final
        {
            std::uint64_t sampled_at_ms{};
            DWORD logical_processors{};
            std::array<EpochCpuCounter, 1u + process_observation::registry.targets.size()> counters{};
        };

        [[nodiscard]] EpochCpuCounter process_cpu_counter(HANDLE process,
            std::uint64_t generation) noexcept
        {
            FILETIME created{}, exited{}, kernel{}, user{};
            EpochCpuCounter counter{generation};
            if (::GetProcessTimes(process, &created, &exited, &kernel, &user))
            {
                counter.ticks = time_ticks(kernel);
                counter.valid = add_counter(counter.ticks, time_ticks(user));
            }
            return counter;
        }

        [[nodiscard]] EpochCpuCounter job_cpu_counter(HANDLE job, std::uint64_t generation) noexcept
        {
            EpochCpuCounter counter{generation};
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
            if (::QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                    &accounting, static_cast<DWORD>(sizeof(accounting)), nullptr)
                && accounting.TotalUserTime.QuadPart >= 0 && accounting.TotalKernelTime.QuadPart >= 0)
            {
                // Even a short-lived compiler that has already exited contributes
                // CPU time while its owning job remains observable.
                counter.ticks = static_cast<std::uint64_t>(accounting.TotalKernelTime.QuadPart);
                counter.valid = add_counter(counter.ticks,
                    static_cast<std::uint64_t>(accounting.TotalUserTime.QuadPart));
            }
            return counter;
        }

        struct ObservedProcesses final
        {
            struct Member final { HANDLE handle{}; DWORD id{}; };
            static constexpr std::size_t limit = 128u;
            std::array<Member, limit> members{};
            std::size_t count{}, remaining_job_members{limit - 1u};

            ObservedProcesses() noexcept
            {
                members[count++] = {::GetCurrentProcess(), ::GetCurrentProcessId()};
            }
            ~ObservedProcesses()
            {
                for (std::size_t i = 1u; i < count; ++i)
                    (void)::CloseHandle(members[i].handle);
            }

            ObservedProcesses(const ObservedProcesses&) = delete;
            ObservedProcesses& operator=(const ObservedProcesses&) = delete;

            [[nodiscard]] bool retain(HANDLE process, DWORD id) noexcept
            {
                const bool known = std::any_of(members.begin(), members.begin() + count,
                    [id](const Member& member) { return member.id == id; });
                if (known || count == members.size())
                {
                    (void)::CloseHandle(process);
                    return known;
                }
                members[count++] = {process, id};
                return true;
            }

            [[nodiscard]] bool add_process(HANDLE process) noexcept
            {
                const DWORD liveness = ::WaitForSingleObject(process, 0u);
                if (liveness != WAIT_TIMEOUT) return liveness == WAIT_OBJECT_0;
                HANDLE observer{};
                const DWORD id = ::GetProcessId(process);
                return id != 0u && ::DuplicateHandle(::GetCurrentProcess(), process,
                    ::GetCurrentProcess(), &observer, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                    FALSE, 0u) && retain(observer, id);
            }

            [[nodiscard]] bool add_job(HANDLE job) noexcept
            {
                struct JobMembership final
                {
                    DWORD assigned{}, count{};
                    std::array<ULONG_PTR, limit> ids{};
                } membership{};
                static_assert(offsetof(JobMembership, ids)
                    == offsetof(JOBOBJECT_BASIC_PROCESS_ID_LIST, ProcessIdList));
                if (!::QueryInformationJobObject(job, JobObjectBasicProcessIdList,
                        &membership, static_cast<DWORD>(sizeof(membership)), nullptr)
                    || membership.count != membership.assigned || membership.count > remaining_job_members)
                    return false;
                remaining_job_members -= membership.count;
                bool complete{true};
                for (DWORD i = 0u; i < membership.count; ++i)
                {
                    const auto raw_id = membership.ids[i];
                    if (raw_id == 0u || raw_id > (std::numeric_limits<DWORD>::max)())
                    {
                        complete = false;
                        continue;
                    }
                    const auto id = static_cast<DWORD>(raw_id);
                    const auto member = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                        FALSE, id);
                    BOOL still_owned{};
                    if (!member || !::IsProcessInJob(member, job, &still_owned) || !still_owned)
                    {
                        if (member) (void)::CloseHandle(member);
                        complete = false;
                        continue;
                    }
                    // The ID was resolved while holding the Job lease, and may
                    // only contribute through this verified process handle.
                    if (!retain(member, id)) complete = false;
                }
                return complete;
            }
        };

        [[nodiscard]] bool observe_child_processes(ObservedProcesses& processes,
            EpochCpuFrame* cpu) noexcept
        {
            auto& registry = process_observation::registry;
            if (!::TryAcquireSRWLockShared(&registry.lock)) return false;
            bool complete = registry.overflow == 0u;
            for (std::size_t i = 0u; i < registry.targets.size(); ++i)
            {
                const auto& target = registry.targets[i];
                if (!target.handle) continue;
                const bool job = target.kind == process_observation::TargetKind::job;
                if (cpu)
                {
                    auto& counter = cpu->counters[i + 1u];
                    counter = job ? job_cpu_counter(target.handle, target.generation)
                        : process_cpu_counter(target.handle, target.generation);
                    if (!counter.valid) complete = false;
                }
                if (job)
                {
                    if (!processes.add_job(target.handle)) complete = false;
                }
                else
                {
                    (void)processes.add_process(target.handle);
                    // Direct process owners give us an exact root handle. No
                    // Job membership evidence is available for descendants.
                    complete = false;
                }
            }
            ::ReleaseSRWLockShared(&registry.lock);
            return complete;
        }

        void measure_epoch_cpu(EpochResourceSample& sample, const EpochCpuFrame& previous,
            const EpochCpuFrame& current) noexcept
        {
            if (current.sampled_at_ms <= previous.sampled_at_ms) return;
            const auto elapsed_ms = current.sampled_at_ms - previous.sampled_at_ms;
            if (elapsed_ms < 1'000u || elapsed_ms > 5'000u || current.logical_processors == 0u
                || current.logical_processors != previous.logical_processors) return;
            std::uint64_t elapsed_cpu{};
            bool valid{};
            for (std::size_t i = 0u; i < current.counters.size(); ++i)
            {
                const auto& before = previous.counters[i];
                const auto& after = current.counters[i];
                if (i != 0u && before.generation == 0u && after.generation == 0u) continue;
                if (!before.valid || !after.valid || before.generation != after.generation
                    || after.ticks < before.ticks)
                {
                    sample.coverage_complete = false;
                    continue;
                }
                if (!add_counter(elapsed_cpu, after.ticks - before.ticks)) return;
                valid = true;
            }
            if (!valid) return;
            const auto capacity = static_cast<double>(elapsed_ms) * 10'000.0 * current.logical_processors;
            sample.cpu_busy_fraction = (std::min)(static_cast<double>(elapsed_cpu) / capacity, 1.0);
            sample.cpu_valid = true;
        }

        [[nodiscard]] EpochResourceSample read_epoch_resources(EpochCpuFrame& previous,
            std::uint64_t began, ObservedProcesses& processes) noexcept
        {
            EpochResourceSample sample;
            EpochCpuFrame cpu{began, ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)};
            cpu.counters[0u] = process_cpu_counter(::GetCurrentProcess(), 0u);
            sample.coverage_complete = observe_child_processes(processes, &cpu) && cpu.counters[0u].valid;
            measure_epoch_cpu(sample, previous, cpu);
            previous = cpu;

            MEMORYSTATUSEX memory{};
            memory.dwLength = static_cast<DWORD>(sizeof(memory));
            if (::GlobalMemoryStatusEx(&memory)) sample.total_memory_bytes = memory.ullTotalPhys;
            bool any_working_set{};
            for (std::size_t i = 0u; i < processes.count; ++i)
            {
                const auto handle = processes.members[i].handle;
                if (i != 0u && ::WaitForSingleObject(handle, 0u) != WAIT_TIMEOUT)
                {
                    sample.coverage_complete = false;
                    continue;
                }
                ++sample.process_count;
                PROCESS_MEMORY_COUNTERS usage{};
                usage.cb = static_cast<DWORD>(sizeof(usage));
                if (::K32GetProcessMemoryInfo(handle, &usage, usage.cb)
                    && add_counter(sample.resident_memory_bytes, usage.WorkingSetSize))
                    any_working_set = true;
                else
                    sample.coverage_complete = false;
            }
            sample.memory_valid = any_working_set && sample.total_memory_bytes != 0u;
            return sample;
        }

        struct GpuInstance final
        {
            std::uint64_t luid{};
            std::uint32_t physical{}, engine{}, pid{};
            [[nodiscard]] constexpr auto operator<=>(const GpuInstance&) const noexcept = default;
        };

        [[nodiscard]] bool consume_prefix(std::wstring_view& text, std::wstring_view prefix) noexcept
        {
            if (!text.starts_with(prefix)) return false;
            text.remove_prefix(prefix.size());
            return true;
        }

        [[nodiscard]] bool consume_counter_number(std::wstring_view& text, std::uint32_t& value,
            std::uint32_t radix = 10u) noexcept
        {
            std::size_t consumed{};
            value = 0u;
            for (const auto character : text)
            {
                std::uint32_t digit{};
                if (character >= L'0' && character <= L'9') digit = static_cast<std::uint32_t>(character - L'0');
                else if (character >= L'a' && character <= L'f') digit = 10u + static_cast<std::uint32_t>(character - L'a');
                else if (character >= L'A' && character <= L'F') digit = 10u + static_cast<std::uint32_t>(character - L'A');
                else break;
                if (digit >= radix) break;
                if (value > ((std::numeric_limits<std::uint32_t>::max)() - digit) / radix) return false;
                value = value * radix + digit;
                ++consumed;
            }
            text.remove_prefix(consumed);
            return consumed != 0u;
        }

        [[nodiscard]] bool parse_gpu_instance(std::wstring_view text, GpuInstance& result,
            bool engine_counter) noexcept
        {
            std::uint32_t high{}, low{};
            if (!consume_prefix(text, L"pid_") || !consume_counter_number(text, result.pid) || result.pid == 0u
                || !consume_prefix(text, L"_luid_0x") || !consume_counter_number(text, high, 16u)
                || !consume_prefix(text, L"_0x") || !consume_counter_number(text, low, 16u)
                || !consume_prefix(text, L"_phys_") || !consume_counter_number(text, result.physical)) return false;
            result.luid = (static_cast<std::uint64_t>(high) << 32u) | low;
            if (result.luid == 0u) return false;
            if (engine_counter)
                return consume_prefix(text, L"_eng_") && consume_counter_number(text, result.engine)
                    && consume_prefix(text, L"_engtype_") && !text.empty();
            if (consume_prefix(text, L"#"))
            {
                std::uint32_t instance_index{};
                if (!consume_counter_number(text, instance_index)) return false;
            }
            return text.empty();
        }

        struct GpuAdapter final
        {
            std::uint64_t luid{}, capacity{};
            bool multi_node{};
            bool attributed{}, memory_attributed{};
        };

        struct GpuCounterRow final { GpuInstance instance{}; double value{}; std::size_t adapter{}; };

        // WDDM exposes one rate per PID and engine. Aliases share the same
        // physical identity, so only identical duplicate observations can be
        // collapsed without risking a stale or double-counted allocation.
        void deduplicate_gpu_rows(std::vector<GpuCounterRow>& rows, bool& partial)
        {
            std::ranges::sort(rows, {}, &GpuCounterRow::instance);
            std::size_t write{};
            for (std::size_t begin = 0u; begin < rows.size();)
            {
                std::size_t end = begin + 1u;
                bool consistent{true};
                while (end < rows.size() && rows[end].instance == rows[begin].instance)
                {
                    consistent = consistent && rows[end].value == rows[begin].value;
                    ++end;
                }
                if (consistent) rows[write++] = rows[begin];
                else partial = true;
                begin = end;
            }
            rows.resize(write);
        }

        void summarize_gpu_rows(GpuResourceSample& sample, std::vector<GpuCounterRow>& engine_rows,
            std::vector<GpuCounterRow>& memory_rows, std::array<GpuAdapter, 32u>& adapters)
        {
            deduplicate_gpu_rows(engine_rows, sample.activity_partial);
            deduplicate_gpu_rows(memory_rows, sample.memory_partial);
            double busiest{};
            for (std::size_t begin = 0u; begin < engine_rows.size();)
            {
                const auto& first = engine_rows[begin];
                const auto& id = first.instance;
                std::size_t next = begin;
                double busy{};
                do { busy += engine_rows[next++].value; }
                while (next < engine_rows.size()
                    && std::tie(id.luid, id.physical, id.engine) == std::tie(engine_rows[next].instance.luid,
                        engine_rows[next].instance.physical, engine_rows[next].instance.engine));
                busiest = (std::max)(busiest, busy);
                adapters[first.adapter].attributed = true;
                begin = next;
            }
            sample.busy_fraction = (std::min)(busiest * 0.01, 1.0);
            sample.busy_valid = !engine_rows.empty();
            for (const auto& row : memory_rows)
            {
                auto& adapter = adapters[row.adapter];
                // DXGI reports capacity by LUID. A linked adapter exposing
                // multiple physical nodes needs a separately proved denominator.
                if (adapter.multi_node || !add_counter(sample.dedicated_used_bytes,
                        static_cast<std::uint64_t>(row.value)))
                {
                    sample.memory_partial = true;
                    continue;
                }
                adapter.memory_attributed = adapter.attributed = true;
            }
            for (const auto& adapter : adapters)
            {
                if (adapter.attributed) ++sample.adapter_count;
                if (adapter.memory_attributed && !add_counter(sample.dedicated_total_bytes, adapter.capacity))
                    sample.memory_partial = true;
            }
            sample.memory_valid = sample.dedicated_total_bytes != 0u;
            if (sample.busy_valid || sample.memory_valid)
                sample.source = GpuTelemetrySource::windows_pdh_process;
        }

        struct GpuProcessIdentity final
        {
            std::uint32_t pid{};
            std::uint64_t created{};
            [[nodiscard]] constexpr bool operator==(const GpuProcessIdentity&) const noexcept = default;
        };

        struct GpuProcessFamily final
        {
            std::array<GpuProcessIdentity, ObservedProcesses::limit> identities{};
            std::size_t count{};
            bool complete{};

            [[nodiscard]] std::span<const GpuProcessIdentity> live() const noexcept
            { return std::span{identities}.first(count); }
        };

        [[nodiscard]] GpuProcessFamily identify_gpu_processes(const ObservedProcesses& processes,
            bool complete) noexcept
        {
            GpuProcessFamily family;
            for (std::size_t i = 0u; i < processes.count; ++i)
            {
                const auto member = processes.members[i];
                if (i != 0u && ::WaitForSingleObject(member.handle, 0u) != WAIT_TIMEOUT) continue;
                FILETIME created{}, exited{}, user{}, kernel{};
                if (::GetProcessTimes(member.handle, &created, &exited, &kernel, &user) && time_ticks(created) != 0u)
                    family.identities[family.count++] = {member.id, time_ticks(created)};
            }
            family.complete = complete && family.count == processes.count;
            return family;
        }

        struct NativeEpochGpu final
        {
            HMODULE pdh{::LoadLibraryExW(L"pdh.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)};
            HMODULE dxgi{::LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)};
            decltype(&::PdhOpenQueryW) open{};
            decltype(&::PdhCloseQuery) close{};
            decltype(&::PdhAddEnglishCounterW) add{};
            decltype(&::PdhCollectQueryData) collect{};
            decltype(&::PdhGetFormattedCounterArrayW) format{};
            decltype(&::CreateDXGIFactory1) create_factory{};
            PDH_HQUERY query{};
            PDH_HCOUNTER activity{}, memory{};
            IDXGIFactory1* factory{};
            std::array<GpuAdapter, 32u> adapters{};
            std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> buffer{};
            GpuProcessFamily previous{};
            std::uint64_t previous_at_ms{};

            NativeEpochGpu() noexcept
            {
                if (!pdh || !dxgi) return;
                open = std::bit_cast<decltype(open)>(::GetProcAddress(pdh, "PdhOpenQueryW"));
                close = std::bit_cast<decltype(close)>(::GetProcAddress(pdh, "PdhCloseQuery"));
                add = std::bit_cast<decltype(add)>(::GetProcAddress(pdh, "PdhAddEnglishCounterW"));
                collect = std::bit_cast<decltype(collect)>(::GetProcAddress(pdh, "PdhCollectQueryData"));
                format = std::bit_cast<decltype(format)>(::GetProcAddress(pdh, "PdhGetFormattedCounterArrayW"));
                create_factory = std::bit_cast<decltype(create_factory)>(::GetProcAddress(dxgi, "CreateDXGIFactory1"));
                if (!open || !close || !add || !collect || !format || !create_factory
                    || open(nullptr, 0u, &query) != ERROR_SUCCESS) return;
                if (add(query, LR"(\GPU Engine(*)\Utilization Percentage)", 0u, &activity) != ERROR_SUCCESS)
                    activity = nullptr;
                if (add(query, LR"(\GPU Process Memory(*)\Dedicated Usage)", 0u, &memory) != ERROR_SUCCESS)
                    memory = nullptr;
            }

            ~NativeEpochGpu()
            {
                if (factory) (void)factory->Release();
                if (query) (void)close(query);
                if (dxgi) (void)::FreeLibrary(dxgi);
                if (pdh) (void)::FreeLibrary(pdh);
            }
            NativeEpochGpu(const NativeEpochGpu&) = delete;
            NativeEpochGpu& operator=(const NativeEpochGpu&) = delete;

            [[nodiscard]] bool describe_hardware() noexcept
            {
                if (factory && factory->IsCurrent()) return adapters.front().luid != 0u;
                if (factory) (void)factory->Release();
                factory = nullptr;
                adapters = {};
                previous = {};
                previous_at_ms = 0u;
                if (FAILED(create_factory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))
                    || !factory) return false;
                std::size_t count{};
                for (UINT i = 0u; i < adapters.size(); ++i)
                {
                    IDXGIAdapter1* adapter{};
                    const auto result = factory->EnumAdapters1(i, &adapter);
                    if (result == DXGI_ERROR_NOT_FOUND) break;
                    if (FAILED(result) || !adapter) continue;
                    DXGI_ADAPTER_DESC1 descriptor{};
                    const auto described = adapter->GetDesc1(&descriptor);
                    (void)adapter->Release();
                    if (FAILED(described)
                        || (descriptor.Flags & (DXGI_ADAPTER_FLAG_SOFTWARE | DXGI_ADAPTER_FLAG_REMOTE)) != 0u)
                        continue;
                    const auto luid = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(
                        descriptor.AdapterLuid.HighPart)) << 32u) | descriptor.AdapterLuid.LowPart;
                    if (luid == 0u || descriptor.DedicatedVideoMemory > (1ull << 50u)) continue;
                    if (std::ranges::find(adapters, luid, &GpuAdapter::luid) != adapters.end())
                    {
                        adapters = {};
                        return false;
                    }
                    adapters[count++] = {luid, descriptor.DedicatedVideoMemory};
                }
                return count != 0u;
            }

            [[nodiscard]] bool collect_rows(PDH_HCOUNTER counter, bool engine_counter,
                std::span<const GpuProcessIdentity> stable, std::vector<GpuCounterRow>& rows,
                std::array<GpuAdapter, 32u>& current_adapters, bool& partial)
            {
                if (!counter) return false;
                constexpr DWORD maximum_counter_bytes = 2u * 1024u * 1024u;
                const DWORD type = PDH_FMT_NOCAP100 | PDH_FMT_NOSCALE
                    | (engine_counter ? PDH_FMT_DOUBLE : PDH_FMT_LARGE);
                for (unsigned attempt = 0u; attempt < 3u; ++attempt)
                {
                    DWORD bytes{}, count{};
                    if (format(counter, type, &bytes, &count, nullptr) != static_cast<PDH_STATUS>(PDH_MORE_DATA)
                        || bytes == 0u || bytes > maximum_counter_bytes) return false;
                    buffer.resize((bytes + sizeof(buffer.front()) - 1u) / sizeof(buffer.front()));
                    const auto allocated = bytes;
                    const auto status = format(counter, type, &bytes, &count, buffer.data());
                    if (status == static_cast<PDH_STATUS>(PDH_MORE_DATA)) continue;
                    if (status != ERROR_SUCCESS || bytes > allocated || count > bytes / sizeof(buffer.front()))
                        return false;
                    const auto first = reinterpret_cast<std::uintptr_t>(buffer.data());
                    const auto end = first + bytes;
                    for (DWORD i = 0u; i < count; ++i)
                    {
                        const auto& item = buffer[i];
                        const auto address = reinterpret_cast<std::uintptr_t>(item.szName);
                        if (address < first || address >= end || address % alignof(wchar_t) != 0u)
                        {
                            partial = true;
                            continue;
                        }
                        const auto available = (std::min)((end - address) / sizeof(wchar_t), std::size_t{160u});
                        const auto text_end = std::find(item.szName, item.szName + available, L'\0');
                        if (text_end == item.szName + available)
                        {
                            partial = true;
                            continue;
                        }
                        const std::wstring_view item_name{item.szName, text_end};
                        GpuInstance instance{};
                        if (!parse_gpu_instance(item_name, instance, engine_counter))
                        {
                            if (std::ranges::find(stable, instance.pid, &GpuProcessIdentity::pid) != stable.end()) partial = true;
                            continue;
                        }
                        auto adapter = std::ranges::find(current_adapters, instance.luid, &GpuAdapter::luid);
                        if (adapter != current_adapters.end() && instance.physical != 0u) adapter->multi_node = true;
                        if (std::ranges::find(stable, instance.pid, &GpuProcessIdentity::pid) == stable.end()) continue;
                        if (adapter == current_adapters.end() || (item.FmtValue.CStatus != PDH_CSTATUS_VALID_DATA
                                && item.FmtValue.CStatus != PDH_CSTATUS_NEW_DATA))
                        {
                            partial = true;
                            continue;
                        }
                        const auto value = engine_counter ? item.FmtValue.doubleValue
                            : static_cast<double>(item.FmtValue.largeValue);
                        if (!std::isfinite(value) || value < 0.0
                            || value > (engine_counter ? 100.0 : static_cast<double>(1ull << 50u)))
                        {
                            partial = true;
                            continue;
                        }
                        rows.push_back({instance, value, static_cast<std::size_t>(adapter - current_adapters.begin())});
                    }
                    return true;
                }
                return false;
            }

            [[nodiscard]] GpuResourceSample sample(const GpuProcessFamily& before, std::uint64_t began)
            {
                if ((!activity && !memory) || !describe_hardware()) return {};
                if (collect(query) != ERROR_SUCCESS)
                {
                    previous = {};
                    previous_at_ms = 0u;
                    return {};
                }
                // Reopen only owned processes after collection. A slow provider
                // call must hold no process or Job handle that delays retirement.
                GpuProcessFamily after{};
                {
                    ObservedProcesses processes;
                    const bool complete = observe_child_processes(processes, nullptr);
                    after = identify_gpu_processes(processes, complete);
                }
                const bool comparable = previous_at_ms != 0u && began >= previous_at_ms
                    && began - previous_at_ms >= 1'000u && began - previous_at_ms <= 5'000u;
                GpuProcessFamily stable{};
                if (comparable)
                    for (const auto& process : after.live())
                        if (std::ranges::find(previous.live(), process) != previous.live().end()
                            && std::ranges::find(before.live(), process) != before.live().end())
                            stable.identities[stable.count++] = process;
                stable.complete = before.complete && after.complete && previous.complete
                    && stable.count == before.count && stable.count == after.count
                    && stable.count == previous.count;
                previous = after;
                previous_at_ms = began;
                if (stable.count == 0u) return {};

                GpuResourceSample result;
                result.sampling_started_at_ms = began;
                auto current_adapters = adapters;
                std::vector<GpuCounterRow> engine_rows{}, memory_rows{};
                result.activity_partial = result.memory_partial = !stable.complete;
                if (!collect_rows(activity, true, stable.live(), engine_rows, current_adapters, result.activity_partial))
                    result.activity_partial = true;
                if (!collect_rows(memory, false, stable.live(), memory_rows, current_adapters, result.memory_partial))
                    result.memory_partial = true;
                summarize_gpu_rows(result, engine_rows, memory_rows, current_adapters);
                result.captured_at_ms = now_milliseconds();
                return result;
            }
        };
#endif

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
#endif

#if defined(_WIN32)
        struct GpuTelemetrySampler final
        {
            std::mutex lifecycle{}, publication{}, wake{};
            std::condition_variable changed{};
            std::thread worker{};
            std::atomic_bool cancelled{true};
            std::atomic_bool requested{};
            std::atomic_bool epoch_requested{};
            GpuResourceSample cached{};
            EpochResourceSample epoch_cached{};
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
#if EPOCH_HAS_NVML_TELEMETRY
                std::optional<NativeNvml> native;
#endif
                EpochCpuFrame previous_epoch_cpu{};
                std::optional<NativeEpochGpu> epoch_gpu;
                while (!sampler.cancelled.load(std::memory_order_acquire))
                {
                    if (sampler.epoch_requested.exchange(false, std::memory_order_relaxed))
                    {
                        const auto began = now_milliseconds();
                        EpochResourceSample sample;
                        GpuProcessFamily family;
                        {
                            ObservedProcesses processes;
                            sample = read_epoch_resources(previous_epoch_cpu, began, processes);
                            family = identify_gpu_processes(processes, sample.coverage_complete);
                        }
                        sample.captured_at_ms = now_milliseconds();
                        if (sample.captured_at_ms < began || sample.captured_at_ms - began > 5'000u)
                        {
                            sample = {};
                            previous_epoch_cpu = {};
                        }
                        {
                            // CPU/RAM publish independently of native GPU queries.
                            const std::lock_guard lock{sampler.publication};
                            if (!sampler.cancelled.load(std::memory_order_acquire))
                            {
                                const auto& previous_gpu = sampler.epoch_cached.gpu;
                                if (sample.captured_at_ms >= previous_gpu.captured_at_ms
                                    && sample.captured_at_ms - previous_gpu.captured_at_ms <= 5'000u)
                                    sample.gpu = previous_gpu;
                                sampler.epoch_cached = sample;
                            }
                        }
                        if (sampler.cancelled.load(std::memory_order_acquire)) break;
                        GpuResourceSample attributed{};
                        if (sample.captured_at_ms != 0u)
                        {
                            try
                            {
                                if (!epoch_gpu) epoch_gpu.emplace();
                                attributed = epoch_gpu->sample(family, now_milliseconds());
                            }
                            catch (...) { attributed = {}; }
                        }
                        if (attributed.captured_at_ms < attributed.sampling_started_at_ms
                            || attributed.captured_at_ms - attributed.sampling_started_at_ms > 5'000u)
                            attributed = {};
                        const std::lock_guard lock{sampler.publication};
                        if (!sampler.cancelled.load(std::memory_order_acquire))
                            sampler.epoch_cached.gpu = attributed;
                    }
#if EPOCH_HAS_NVML_TELEMETRY
                    if (sampler.requested.exchange(false, std::memory_order_relaxed))
                    {
                        if (!native) native.emplace();
                        const auto began = now_milliseconds();
                        auto sample = read_nvml_resources(native->api);
                        const auto finished = now_milliseconds();
                        sample.sampling_started_at_ms = began;
                        sample.captured_at_ms = finished;
                        if (finished < began || finished - began > 5'000u)
                            sample = {};
                        const std::lock_guard lock{sampler.publication};
                        if (!sampler.cancelled.load(std::memory_order_acquire))
                            sampler.cached = sample;
                    }
#endif
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
            CpuTimes result{time_ticks(kernel), time_ticks(idle), true};
            if (!add_counter(result.total, time_ticks(user)) || result.idle > result.total)
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
#if defined(_WIN32)
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
                sampler->epoch_cached = {};
            }
            sampler->requested.store(false, std::memory_order_relaxed);
            sampler->epoch_requested.store(false, std::memory_order_relaxed);
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
#if defined(_WIN32)
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

    GpuResourceSample sample_gpu_telemetry() noexcept
    {
#if EPOCH_HAS_NVML_TELEMETRY
        if (auto* const sampler = gpuSampler.load(std::memory_order_acquire);
            sampler && !sampler->cancelled.load(std::memory_order_acquire))
            sampler->requested.store(true, std::memory_order_relaxed);
#endif
        return cached_gpu_resources();
    }

#if defined(__linux__)
    namespace
    {
        struct LinuxProcessUsage final
        {
            pid_t pid{};
            std::uint64_t started{}, ticks{}, resident_pages{};
        };

        // Each task's children file is kernel-owned. Check the PPID from the
        // same stat record as its counters, since an enumerated PID can exit
        // and be reused before the read. The start tick identifies successive
        // incarnations across the two CPU measurements.
        [[nodiscard]] std::optional<LinuxProcessUsage> read_linux_process(pid_t pid, pid_t editor)
        {
            std::ifstream file{"/proc/" + std::to_string(pid) + "/stat"};
            std::array<char, 2048> data{};
            file.read(data.data(), data.size());
            if (file.bad() || file.gcount() == 0 || file.gcount() == static_cast<std::streamsize>(data.size())) return {};
            const std::string_view record{data.data(), static_cast<std::size_t>(file.gcount())};
            const auto name_end = record.rfind(')');
            if (name_end == record.npos) return {};
            std::istringstream fields{std::string{record.substr(name_end + 1)}};
            char state{};
            pid_t parent{};
            if (!(fields >> state >> parent) || state == 'Z' || state == 'X'
                || (pid != editor && parent != editor)) return {};
            // Fields 5..13 and 16..21 include signed values we do not use.
            std::string unused;
            for (int i = 5; i <= 13; ++i) if (!(fields >> unused)) return {};
            LinuxProcessUsage usage{pid};
            std::uint64_t system{};
            if (!(fields >> usage.ticks >> system) || !add_counter(usage.ticks, system)) return {};
            for (int i = 16; i <= 21; ++i) if (!(fields >> unused)) return {};
            std::int64_t pages{};
            if (!(fields >> usage.started >> unused >> pages) || pages < 0) return {};
            usage.resident_pages = static_cast<std::uint64_t>(pages);
            return usage;
        }

        struct LinuxEpochFrame final
        {
            std::array<LinuxProcessUsage, 33> processes{};
            std::size_t count{};
            CpuTimes host{};
        };

        [[nodiscard]] LinuxEpochFrame read_linux_epoch(ResourceSample& host)
        {
            LinuxEpochFrame frame{};
            const pid_t editor = ::getpid();
            const auto self = read_linux_process(editor, editor);
            if (!self) return frame;
            frame.processes[frame.count++] = *self;
            frame.host = read_resources(host);
            // Direct children may be launched by any Editor thread. Do not walk
            // the system process table or presume that grandchildren are owned.
            std::error_code error;
            auto task = std::filesystem::directory_iterator{"/proc/self/task", error};
            unsigned inspected{};
            for (unsigned n = 0; !error && task != std::filesystem::directory_iterator{}
                && n < 64 && inspected < frame.processes.size() - 1; ++n, task.increment(error))
            {
                std::ifstream children{task->path() / "children"};
                pid_t pid{};
                while (inspected < frame.processes.size() - 1 && children >> pid)
                {
                    ++inspected;
                    auto live = std::span{frame.processes}.first(frame.count);
                    if (pid <= 0 || std::ranges::find(live, pid, &LinuxProcessUsage::pid) != live.end())
                        continue;
                    if (const auto usage = read_linux_process(pid, editor))
                        frame.processes[frame.count++] = *usage;
                }
            }
            return frame;
        }

        [[nodiscard]] EpochResourceSample sample_linux_epoch()
        {
            static std::mutex mutex;
            static LinuxEpochFrame previous;
            static EpochResourceSample cached;
            const std::lock_guard lock{mutex};
            const auto now = now_milliseconds();
            if (cached.captured_at_ms && now >= cached.captured_at_ms && now - cached.captured_at_ms < 1000)
                return cached;
            ResourceSample host{};
            const auto current = read_linux_epoch(host);
            EpochResourceSample result{};
            result.captured_at_ms = now;
            result.process_count = static_cast<std::uint32_t>(current.count);
            result.total_memory_bytes = host.total_memory_bytes;
            const auto page_size = ::sysconf(_SC_PAGESIZE);
            std::uint64_t pages{}, ticks{};
            bool valid_pages = current.count && page_size > 0;
            bool valid_ticks = current.count && current.count == previous.count
                && current.host.valid && previous.host.valid && current.host.total > previous.host.total
                && now >= cached.captured_at_ms && now - cached.captured_at_ms <= 5000;
            auto prior = std::span{previous.processes}.first(previous.count);
            for (const auto& proc : std::span{current.processes}.first(current.count))
            {
                valid_pages = valid_pages && add_counter(pages, proc.resident_pages);
                const auto found = std::ranges::find(prior, proc.pid, &LinuxProcessUsage::pid);
                valid_ticks = valid_ticks && found != prior.end() && found->started == proc.started
                    && found->ticks <= proc.ticks && add_counter(ticks, proc.ticks - found->ticks);
            }
            if (valid_pages && host.total_memory_bytes
                && pages <= (std::numeric_limits<std::uint64_t>::max)() / static_cast<unsigned long>(page_size))
            {
                result.resident_memory_bytes = pages * static_cast<unsigned long>(page_size);
                result.memory_valid = result.resident_memory_bytes <= result.total_memory_bytes;
            }
            if (valid_ticks && ticks <= current.host.total - previous.host.total)
            {
                result.cpu_busy_fraction = static_cast<double>(ticks) / (current.host.total - previous.host.total);
                result.cpu_valid = true;
            }
            // Direct observed processes only. RSS can count shared pages once
            // per process; Linux GPU counters remain explicitly unavailable.
            previous = current;
            cached = result;
            return result;
        }
    }
#endif

    EpochResourceSample sample_epoch_resources() noexcept
    {
#if defined(_WIN32)
        auto* const sampler = gpuSampler.load(std::memory_order_acquire);
        if (sampler && !sampler->cancelled.load(std::memory_order_acquire))
        {
            sampler->epoch_requested.store(true, std::memory_order_relaxed);
            const std::lock_guard lock{sampler->publication};
            const auto now = now_milliseconds();
            if (!sampler->cancelled.load(std::memory_order_acquire)
                && now >= sampler->epoch_cached.captured_at_ms
                && now - sampler->epoch_cached.captured_at_ms <= 5'000u)
            {
                auto result = sampler->epoch_cached;
                if (now < result.gpu.captured_at_ms || now - result.gpu.captured_at_ms > 5'000u)
                    result.gpu = {};
                return result;
            }
        }
#elif defined(__linux__)
        try { return sample_linux_epoch(); } catch (...) { return {}; }
#endif
        return {};
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
