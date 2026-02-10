module;

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

export module ascripting.system;

import aengine.scripting.compiler;
import aengine.systems;
import aengine.taskgraph.dotsystem;

import <algorithm>;
import <atomic>;
import <chrono>;
import <filesystem>;
import <iostream>;
import <mutex>;
import <string>;
import <thread>;
import <utility>;
import <vector>;

export namespace almondnamespace::scripting
{
    using ScriptScheduler = taskgraph::TaskGraph;

    struct ScriptLoadReport
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
#ifndef ALMOND_MAIN_HEADLESS
    inline HMODULE lastLib = nullptr;
#else
    inline void* lastLib = nullptr;
#endif
#else
    inline void* lastLib = nullptr;
#endif

    using run_script_fn = void(*)(ScriptScheduler&);

    inline Task do_load_script(const std::string& scriptName, ScriptScheduler& scheduler, ScriptLoadReport& report)
    {
        try
        {
            const std::filesystem::path sourcePath = std::filesystem::path("src/scripts") / (scriptName + ".ascript.cpp");
#ifdef _WIN32
            const std::filesystem::path dllPath = std::filesystem::path("src/scripts") / (scriptName + ".dll");
#else
            const std::filesystem::path dllPath = std::filesystem::path("src/scripts") / ("lib" + scriptName + ".so");
#endif

            report.scheduled.store(true, std::memory_order_relaxed);
            report.log_info("Scheduling script reload for '" + scriptName + "'.");

            if (!std::filesystem::exists(sourcePath))
            {
                const std::string message = "[script] Source file missing: " + sourcePath.string();
                std::cerr << message << "\n";
                report.log_error(message);
                co_return;
            }

            if (lastLib)
            {
#ifdef _WIN32
#ifndef ALMOND_MAIN_HEADLESS
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
                std::cerr << message << "\n";
                report.log_error(message);
                co_return;
            }

            report.compiled.store(true, std::memory_order_relaxed);
            report.log_info("Compiled script '" + scriptName + "' to DLL.");

            if (!std::filesystem::exists(dllPath))
            {
                const std::string message = "[script] Expected output missing after compilation: " + dllPath.string();
                std::cerr << message << "\n";
                report.log_error(message);
                co_return;
            }

#ifdef _WIN32
#ifndef ALMOND_MAIN_HEADLESS
            lastLib = LoadLibraryA(dllPath.string().c_str());
            if (!lastLib)
            {
                const std::string message = "[script] LoadLibrary failed: " + dllPath.string();
                std::cerr << message << "\n";
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
                std::cerr << message << "\n";
                report.log_error(message);
                co_return;
            }
            auto entry = reinterpret_cast<run_script_fn>(dlsym(lastLib, "run_script"));
#endif

            report.dllLoaded.store(true, std::memory_order_relaxed);

#ifndef ALMOND_MAIN_HEADLESS
            if (!entry)
            {
                const std::string message = "[script] Missing run_script symbol in: " + dllPath.string();
                std::cerr << message << "\n";
                report.log_error(message);
                co_return;
            }

            report.log_info("Executing run_script for '" + scriptName + "'.");
            entry(scheduler);
            report.executed.store(true, std::memory_order_relaxed);
#endif
        }
        catch (const std::exception& e)
        {
            const std::string message = std::string("[script] Exception during script load: ") + e.what();
            std::cerr << message << "\n";
            report.log_error(message);
        }
        catch (...)
        {
            const std::string message = "[script] Unknown exception during script load";
            std::cerr << message << "\n";
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

    export bool load_or_reload_script(const std::string& scriptName, ScriptScheduler& scheduler, ScriptLoadReport* reportPtr = nullptr)
    {
        ScriptLoadReport fallbackReport;
        ScriptLoadReport& report = reportPtr ? *reportPtr : fallbackReport;
        report.reset();

        try
        {
            Task t = do_load_script(scriptName, scheduler, report);

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
            std::cerr << message << "\n";
            report.log_error(message);
            return false;
        }
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

            Task reloadTask = do_load_script(config.scriptName, scheduler, report);
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

        std::cout << "[StressTest] Completed=" << summary.completedNodes
            << " Total=" << summary.totalNodes
            << " MaxQueueDepth=" << summary.maxQueueDepth
            << " ReloadFailures=" << summary.reloadFailures;

        if (summary.deadlockDetected)
            std::cout << " Deadlock=" << summary.deadlockSignature;

        std::cout << "\n";

        return summary;
    }
}
