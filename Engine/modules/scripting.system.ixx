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

#include <coroutine>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <source_location>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <include/epoch.script_api.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

export module scripting.system;

import scripting.compiler;
import engine.cli;
import engine.systems;
import taskgraph.dotsystem;
import core.logger;
import core.path;


namespace epochengine::scripting
{
    using ScriptScheduler = taskgraph::TaskGraph;

    export struct ScriptLoadReport
    {
        std::atomic<bool> scheduled{ false };
        std::atomic<bool> compiled{ false };
        std::atomic<bool> dllLoaded{ false };
        std::atomic<bool> executed{ false };
        std::atomic<bool> failed{ false };

        ScriptLoadReport() = default;
        ~ScriptLoadReport() = default;

        // ---- Make this type vector-friendly (move/copy) ----
        ScriptLoadReport(const ScriptLoadReport& other)
        {
            copy_from(other);
        }

        ScriptLoadReport& operator=(const ScriptLoadReport& other)
        {
            if (this != &other) copy_from(other);
            return *this;
        }

        ScriptLoadReport(ScriptLoadReport&& other) noexcept
        {
            move_from(std::move(other));
        }

        ScriptLoadReport& operator=(ScriptLoadReport&& other) noexcept
        {
            if (this != &other) move_from(std::move(other));
            return *this;
        }

        void reset()
        {
            scheduled.store(false, std::memory_order_relaxed);
            compiled.store(false, std::memory_order_relaxed);
            dllLoaded.store(false, std::memory_order_relaxed);
            executed.store(false, std::memory_order_relaxed);
            failed.store(false, std::memory_order_relaxed);

            std::lock_guard<std::mutex> lock(messageMutex_);
            messages_.clear();
        }

        void log_info(const std::string& message)
        {
            std::lock_guard<std::mutex> lock(messageMutex_);
            messages_.push_back(message);
        }

        void log_error(const std::string& message)
        {
            failed.store(true, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(messageMutex_);
            messages_.push_back(message);
        }

        bool succeeded() const
        {
            return scheduled.load(std::memory_order_relaxed)
                && compiled.load(std::memory_order_relaxed)
                && dllLoaded.load(std::memory_order_relaxed)
                && executed.load(std::memory_order_relaxed)
                && !failed.load(std::memory_order_relaxed);
        }

        std::vector<std::string> messages() const
        {
            std::lock_guard<std::mutex> lock(messageMutex_);
            return messages_;
        }

    private:
        mutable std::mutex messageMutex_;
        std::vector<std::string> messages_;

        void copy_from(const ScriptLoadReport& other)
        {
            scheduled.store(other.scheduled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compiled.store(other.compiled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dllLoaded.store(other.dllLoaded.load(std::memory_order_relaxed), std::memory_order_relaxed);
            executed.store(other.executed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            failed.store(other.failed.load(std::memory_order_relaxed), std::memory_order_relaxed);

            // Copy messages under lock. Take other's lock first.
            {
                std::lock_guard<std::mutex> lockOther(other.messageMutex_);
                std::lock_guard<std::mutex> lockThis(messageMutex_);
                messages_ = other.messages_;
            }
        }

        void move_from(ScriptLoadReport&& other)
        {
            scheduled.store(other.scheduled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compiled.store(other.compiled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dllLoaded.store(other.dllLoaded.load(std::memory_order_relaxed), std::memory_order_relaxed);
            executed.store(other.executed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            failed.store(other.failed.load(std::memory_order_relaxed), std::memory_order_relaxed);

            {
                std::lock_guard<std::mutex> lockOther(other.messageMutex_);
                std::lock_guard<std::mutex> lockThis(messageMutex_);
                messages_ = std::move(other.messages_);
            }

            // Optional: leave other in a sane reset-ish state
            other.scheduled.store(false, std::memory_order_relaxed);
            other.compiled.store(false, std::memory_order_relaxed);
            other.dllLoaded.store(false, std::memory_order_relaxed);
            other.executed.store(false, std::memory_order_relaxed);
            other.failed.store(false, std::memory_order_relaxed);
        }
    };

    struct TaskGraphStressConfig
    {
        std::string scriptName;
        std::size_t workerCount{ 4 };
        std::size_t reloadIterations{ 25 };
        std::size_t tasksPerIteration{ 200 };
        std::chrono::milliseconds taskDelay{ 0 };
        std::chrono::milliseconds reloadInterval{ 0 };
        std::chrono::milliseconds pollInterval{ 25 };
        std::chrono::milliseconds stallTimeout{ 2000 };
        std::chrono::seconds maxDuration{ 30 };
    };

    struct TaskGraphStressReport
    {
        bool deadlockDetected{ false };
        std::size_t totalNodes{ 0 };
        std::size_t completedNodes{ 0 };
        std::size_t maxQueueDepth{ 0 };
        std::size_t reloadAttempts{ 0 };
        std::size_t reloadFailures{ 0 };
        std::string deadlockSignature;
    };

    // Keep this inside the module with a single definition across TUs.
    // In a module interface, inline variables are the safe pattern.
#ifdef _WIN32
#ifndef EPOCH_MAIN_HEADLESS
    inline HMODULE lastLib = nullptr;
#else
    inline void* lastLib = nullptr;
#endif
#else
    inline void* lastLib = nullptr;
#endif

    using run_script_fn = void(*)(EpochScriptHost*);

    namespace detail
    {
        [[nodiscard]] inline std::filesystem::path resolve_scripts_root()
        {
            namespace fs = std::filesystem;
            std::error_code ec;

            const fs::path exeDir = epochengine::core::cli::exe_path.empty()
                ? epochengine::core::path::executable_dir()
                : fs::absolute(epochengine::core::cli::exe_path, ec).parent_path();
            const fs::path runtimeRoot = epochengine::core::path::runtime_root_dir();
            const fs::path repoRoot = epochengine::core::path::find_epoch_repo_root(exeDir);

            const std::vector<fs::path> candidates{
                exeDir / "src" / "scripts",
                exeDir / "Engine" / "src" / "scripts",
                runtimeRoot / "src" / "scripts",
                runtimeRoot / "Engine" / "src" / "scripts",
                repoRoot / "Engine" / "src" / "scripts",
                exeDir / ".." / ".." / "Engine" / "src" / "scripts",
                exeDir / ".." / ".." / ".." / "Engine" / "src" / "scripts"
            };

            for (const auto& candidate : candidates)
            {
                if (candidate.empty())
                    continue;

                const fs::path absolute = fs::absolute(candidate, ec).lexically_normal();
                if (fs::exists(absolute))
                    return absolute;
            }

            return fs::absolute(exeDir / ".." / ".." / "Engine" / "src" / "scripts", ec).lexically_normal();
        }

        [[nodiscard]] inline std::string script_binary_stem(const std::filesystem::path& source_path)
        {
            const std::string filename = source_path.filename().string();
            constexpr std::string_view suffix = ".ascript.cpp";
            if (std::string_view(filename).ends_with(suffix))
                return filename.substr(0, filename.size() - suffix.size());
            return source_path.stem().string();
        }

        [[nodiscard]] inline std::filesystem::path compiled_library_path(const std::filesystem::path& source_path)
        {
            const std::string stem = script_binary_stem(source_path);
#ifdef _WIN32
            return source_path.parent_path() / (stem + ".dll");
#else
            return source_path.parent_path() / ("lib" + stem + ".so");
#endif
        }
    }

    inline Task do_load_script(
        const std::filesystem::path& sourcePath,
        std::string_view logicalScriptName,
        ScriptScheduler& scheduler,
        EpochScriptHost* host,
        ScriptLoadReport& report);

    inline Task do_load_script(const std::string& scriptName, ScriptScheduler& scheduler, EpochScriptHost* host, ScriptLoadReport& report)
    {
        return do_load_script(
            detail::resolve_scripts_root() / (scriptName + ".ascript.cpp"),
            scriptName,
            scheduler,
            host,
            report);
    }

    inline Task do_load_script(
        const std::filesystem::path& sourcePath,
        std::string_view logicalScriptName,
        ScriptScheduler& scheduler,
        EpochScriptHost* host,
        ScriptLoadReport& report)
    {
        try
        {
            const std::string scriptName = logicalScriptName.empty()
                ? detail::script_binary_stem(sourcePath)
                : std::string(logicalScriptName);
            const std::filesystem::path dllPath = detail::compiled_library_path(sourcePath);

            report.scheduled.store(true, std::memory_order_relaxed);
            report.log_info("Scheduling script reload for '" + scriptName + "'.");

            if (!std::filesystem::exists(sourcePath))
            {
                const std::string message = "[script] Source file missing: " + sourcePath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }

            if (lastLib)
            {
#ifdef _WIN32
#ifndef EPOCH_MAIN_HEADLESS
                FreeLibrary(lastLib);
#endif
#else
                dlclose(lastLib);
#endif
                lastLib = nullptr;
            }

            if (!compiler::compile_script_to_dll(sourcePath, dllPath))
            {
                const std::string message = "[script] Compilation failed: " + sourcePath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }

            report.compiled.store(true, std::memory_order_relaxed);
            report.log_info("Compiled script '" + scriptName + "' to DLL.");

            if (!std::filesystem::exists(dllPath))
            {
                const std::string message = "[script] Expected output missing after compilation: " + dllPath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }

#ifdef _WIN32
#ifndef EPOCH_MAIN_HEADLESS
            lastLib = LoadLibraryA(dllPath.string().c_str());
            if (!lastLib)
            {
                const std::string message = "[script] LoadLibrary failed: " + dllPath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }
            auto entry = reinterpret_cast<run_script_fn>(GetProcAddress(lastLib, "run_script"));
#endif
#else
            lastLib = dlopen(dllPath.string().c_str(), RTLD_NOW);
            if (!lastLib)
            {
                const std::string message = "[script] dlopen failed: " + dllPath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }
            auto entry = reinterpret_cast<run_script_fn>(dlsym(lastLib, "run_script"));
#endif

            report.dllLoaded.store(true, std::memory_order_relaxed);

#ifndef EPOCH_MAIN_HEADLESS
            if (!entry)
            {
                const std::string message = "[script] Missing run_script symbol in: " + dllPath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }

            report.log_info("Executing run_script for '" + scriptName + "'.");
            entry(host);
            report.executed.store(true, std::memory_order_relaxed);
#endif
        }
        catch (const std::exception& e)
        {
            const std::string message = std::string("[script] Exception during script load: ") + e.what();
            logger::error("Scripting", message);
            report.log_error(message);
        }
        catch (...)
        {
            const std::string message = "[script] Unknown exception during script load";
            logger::error("Scripting", message);
            report.log_error(message);
        }

        co_return;
    }

    inline Task make_stress_task(std::atomic<std::size_t>& completed, std::chrono::milliseconds delay)
    {
        if (delay.count() > 0)
            std::this_thread::sleep_for(delay);

        completed.fetch_add(1, std::memory_order_relaxed);
        co_return;
    }

    export bool load_or_reload_script(const std::string& scriptName, ScriptScheduler& scheduler, EpochScriptHost* host, ScriptLoadReport* reportPtr = nullptr)
    {
        ScriptLoadReport fallbackReport;
        ScriptLoadReport& report = reportPtr ? *reportPtr : fallbackReport;
        report.reset();

        try
        {
            Task t = do_load_script(scriptName, scheduler, host, report);

            auto node = std::make_unique<taskgraph::Node>(std::move(t));
            node->Label = "script:" + scriptName;
            scheduler.AddNode(std::move(node));

            scheduler.Execute();
            scheduler.WaitAll();
            scheduler.PruneFinished();

            return report.succeeded();
        }
        catch (const std::exception& e)
        {
            const std::string message = std::string("[script] Scheduling exception: ") + e.what();
            logger::error("Scripting", message);
            report.log_error(message);
            return false;
        }
    }

    export bool load_or_reload_script(
        const std::filesystem::path& sourcePath,
        std::string_view logicalScriptName,
        ScriptScheduler& scheduler,
        EpochScriptHost* host,
        ScriptLoadReport* reportPtr = nullptr)
    {
        ScriptLoadReport fallbackReport;
        ScriptLoadReport& report = reportPtr ? *reportPtr : fallbackReport;
        report.reset();

        try
        {
            Task t = do_load_script(sourcePath, logicalScriptName, scheduler, host, report);

            auto node = std::make_unique<taskgraph::Node>(std::move(t));
            const std::string nodeLabel = logicalScriptName.empty()
                ? detail::script_binary_stem(sourcePath)
                : std::string(logicalScriptName);
            node->Label = "script:" + nodeLabel;
            scheduler.AddNode(std::move(node));

            scheduler.Execute();
            scheduler.WaitAll();
            scheduler.PruneFinished();

            return report.succeeded();
        }
        catch (const std::exception& e)
        {
            const std::string message = std::string("[script] Scheduling exception: ") + e.what();
            logger::error("Scripting", message);
            report.log_error(message);
            return false;
        }
    }

    export bool load_or_reload_script(const std::string& scriptName, ScriptScheduler& scheduler, ScriptLoadReport* reportPtr = nullptr)
    {
        return load_or_reload_script(scriptName, scheduler, nullptr, reportPtr);
    }

    export bool load_or_reload_script(const std::string& scriptName, EpochScriptHost* host, ScriptLoadReport* reportPtr = nullptr)
    {
        const std::size_t workerCount = (std::max)(std::size_t{ 1 }, std::thread::hardware_concurrency() > 0
            ? static_cast<std::size_t>(std::thread::hardware_concurrency())
            : std::size_t{ 4 });
        taskgraph::TaskGraph scheduler(workerCount);
        return load_or_reload_script(scriptName, scheduler, host, reportPtr);
    }

    export bool load_or_reload_script(
        const std::filesystem::path& sourcePath,
        std::string_view logicalScriptName,
        EpochScriptHost* host,
        ScriptLoadReport* reportPtr = nullptr)
    {
        const std::size_t workerCount = (std::max)(std::size_t{ 1 }, std::thread::hardware_concurrency() > 0
            ? static_cast<std::size_t>(std::thread::hardware_concurrency())
            : std::size_t{ 4 });
        taskgraph::TaskGraph scheduler(workerCount);
        return load_or_reload_script(sourcePath, logicalScriptName, scheduler, host, reportPtr);
    }

    export TaskGraphStressReport run_taskgraph_reload_stress_test(const TaskGraphStressConfig& config)
    {
        TaskGraphStressReport summary;
        summary.reloadAttempts = config.reloadIterations;
        summary.totalNodes = config.reloadIterations + (config.reloadIterations * config.tasksPerIteration);

        taskgraph::TaskGraph scheduler(config.workerCount);
        std::atomic<std::size_t> stressCompleted{ 0 };

        std::vector<ScriptLoadReport> reloadReports;
        reloadReports.reserve(config.reloadIterations);

        for (std::size_t iteration = 0; iteration < config.reloadIterations; ++iteration)
        {
            reloadReports.emplace_back();
            auto& report = reloadReports.back();
            report.reset();

            Task reloadTask = do_load_script(config.scriptName, scheduler, nullptr, report);
            auto reloadNode = std::make_unique<taskgraph::Node>(std::move(reloadTask));
            reloadNode->Label = "stress-reload:" + config.scriptName + "#" + std::to_string(iteration);
            scheduler.AddNode(std::move(reloadNode));

            for (std::size_t taskIndex = 0; taskIndex < config.tasksPerIteration; ++taskIndex)
            {
                Task work = make_stress_task(stressCompleted, config.taskDelay);
                auto node = std::make_unique<taskgraph::Node>(std::move(work));
                node->Label = "stress-task:" + std::to_string(iteration) + ":" + std::to_string(taskIndex);
                scheduler.AddNode(std::move(node));
            }

            scheduler.Execute();

            if (config.reloadInterval.count() > 0)
                std::this_thread::sleep_for(config.reloadInterval);
        }

        const auto deadline = std::chrono::steady_clock::now() + config.maxDuration;
        auto lastProgress = std::chrono::steady_clock::now();
        std::size_t lastCompleted = scheduler.CompletedCount();

        while (std::chrono::steady_clock::now() < deadline)
        {
            const std::size_t completed = scheduler.CompletedCount();
            const std::size_t queueDepth = scheduler.QueueDepth();
            summary.maxQueueDepth = (std::max)(summary.maxQueueDepth, queueDepth);

            if (completed != lastCompleted)
            {
                lastCompleted = completed;
                lastProgress = std::chrono::steady_clock::now();
            }

            if (completed >= summary.totalNodes)
                break;

            if (std::chrono::steady_clock::now() - lastProgress > config.stallTimeout)
            {
                summary.deadlockDetected = true;
                summary.deadlockSignature = "TaskGraph stall timeout while tasks remain pending.";
                break;
            }

            std::this_thread::sleep_for(config.pollInterval);
        }

        summary.completedNodes = scheduler.CompletedCount();

        for (const auto& report : reloadReports)
        {
            if (!report.succeeded())
                summary.reloadFailures += 1;
        }

        if (!summary.deadlockDetected && summary.completedNodes < summary.totalNodes)
        {
            summary.deadlockDetected = true;
            summary.deadlockSignature = "TaskGraph timed out before reaching expected completion count.";
        }

        if (summary.deadlockDetected)
        {
            logger::warnf_loc(
                "Scripting.Stress",
                std::source_location::current(),
                "Completed={} Total={} MaxQueueDepth={} ReloadFailures={} Deadlock={}",
                summary.completedNodes,
                summary.totalNodes,
                summary.maxQueueDepth,
                summary.reloadFailures,
                summary.deadlockSignature);
        }
        else
        {
            logger::infof_loc(
                "Scripting.Stress",
                std::source_location::current(),
                "Completed={} Total={} MaxQueueDepth={} ReloadFailures={}",
                summary.completedNodes,
                summary.totalNodes,
                summary.maxQueueDepth,
                summary.reloadFailures);
        }

        return summary;
    }
}
