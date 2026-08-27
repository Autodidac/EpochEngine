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
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <include/scripting.epoch_api.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

export module scripting.system;

import scripting.compiler;
import epoch.cli;
import systems.task;
import taskgraph.dotsystem;
import core.logger;
import core.path;
import platform.filesystem;


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
        std::atomic<std::uint64_t> requestGeneration{ 0 };
        std::atomic<std::uint64_t> projectGeneration{ 0 };

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
            requestGeneration.store(0, std::memory_order_relaxed);
            projectGeneration.store(0, std::memory_order_relaxed);

            std::lock_guard<std::mutex> lock(messageMutex_);
            messages_.clear();
            compileResult_.reset();
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

        void record_compile_result(const compiler::CompileResult& result)
        {
            requestGeneration.store(
                result.evidence.request_generation, std::memory_order_relaxed);
            projectGeneration.store(
                result.evidence.project_generation, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(messageMutex_);
            compileResult_ = result;
        }

        [[nodiscard]] std::optional<compiler::CompileResult> compile_result() const
        {
            std::lock_guard<std::mutex> lock(messageMutex_);
            return compileResult_;
        }

    private:
        mutable std::mutex messageMutex_;
        std::vector<std::string> messages_;
        std::optional<compiler::CompileResult> compileResult_{};

        void copy_from(const ScriptLoadReport& other)
        {
            scheduled.store(other.scheduled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compiled.store(other.compiled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dllLoaded.store(other.dllLoaded.load(std::memory_order_relaxed), std::memory_order_relaxed);
            executed.store(other.executed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            failed.store(other.failed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            requestGeneration.store(other.requestGeneration.load(std::memory_order_relaxed), std::memory_order_relaxed);
            projectGeneration.store(other.projectGeneration.load(std::memory_order_relaxed), std::memory_order_relaxed);

            // Copy messages under lock. Take other's lock first.
            {
                std::scoped_lock lock{messageMutex_, other.messageMutex_};
                messages_ = other.messages_;
                compileResult_ = other.compileResult_;
            }
        }

        void move_from(ScriptLoadReport&& other)
        {
            scheduled.store(other.scheduled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            compiled.store(other.compiled.load(std::memory_order_relaxed), std::memory_order_relaxed);
            dllLoaded.store(other.dllLoaded.load(std::memory_order_relaxed), std::memory_order_relaxed);
            executed.store(other.executed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            failed.store(other.failed.load(std::memory_order_relaxed), std::memory_order_relaxed);
            requestGeneration.store(other.requestGeneration.load(std::memory_order_relaxed), std::memory_order_relaxed);
            projectGeneration.store(other.projectGeneration.load(std::memory_order_relaxed), std::memory_order_relaxed);

            {
                std::scoped_lock lock{messageMutex_, other.messageMutex_};
                messages_ = std::move(other.messages_);
                compileResult_ = std::move(other.compileResult_);
            }

            // Optional: leave other in a sane reset-ish state
            other.scheduled.store(false, std::memory_order_relaxed);
            other.compiled.store(false, std::memory_order_relaxed);
            other.dllLoaded.store(false, std::memory_order_relaxed);
            other.executed.store(false, std::memory_order_relaxed);
            other.failed.store(false, std::memory_order_relaxed);
            other.requestGeneration.store(0, std::memory_order_relaxed);
            other.projectGeneration.store(0, std::memory_order_relaxed);
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

    inline std::mutex scriptReloadMutex{};
    inline std::atomic<std::uint64_t> scriptCompileGeneration{0};

    [[nodiscard]] inline std::filesystem::path& loaded_library_path()
    {
        static std::filesystem::path value{};
        return value;
    }

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
            return compiler::default_script_output_path(source_path);
        }

        [[nodiscard]] inline std::filesystem::path script_workspace_root(
            const std::filesystem::path& source_path)
        {
            const auto repo = core::path::find_epoch_repo_root(source_path);
            return repo.empty() ? source_path.parent_path() : repo;
        }

        [[nodiscard]] inline std::filesystem::path load_image_path(
            const std::filesystem::path& published_path,
            std::uint64_t generation)
        {
            return published_path.parent_path()
                / (published_path.stem().string() + ".epoch_load_"
                    + std::to_string(generation)
                    + published_path.extension().string());
        }

        inline void remove_regular_file(
            const std::filesystem::path& path) noexcept
        {
            std::error_code error{};
            const auto status = std::filesystem::symlink_status(path, error);
            if (!error && std::filesystem::is_regular_file(status))
                std::filesystem::remove(path, error);
        }

        [[nodiscard]] inline bool stage_load_image(
            const compiler::CompileResult& compile,
            const std::filesystem::path& load_path,
            std::string& failure)
        {
            const auto temporary = std::filesystem::path{
                load_path.string() + ".copy"};
            const auto requireMissing = [&](const std::filesystem::path& path,
                                            std::string_view label)
            {
                std::error_code inspectionError{};
                const auto status = std::filesystem::symlink_status(
                    path, inspectionError);
                if (inspectionError == std::errc::no_such_file_or_directory
                    || (!inspectionError && !std::filesystem::exists(status)))
                {
                    return true;
                }
                failure = inspectionError
                    ? std::string(label) + " could not be inspected: "
                        + inspectionError.message()
                    : std::string(label) + " is already occupied";
                return false;
            };

            if (!requireMissing(temporary, "script load staging path")
                || !requireMissing(load_path, "script load image path"))
            {
                return false;
            }

            std::error_code error{};
            if (!std::filesystem::copy_file(
                    compile.evidence.output_path,
                    temporary,
                    std::filesystem::copy_options::none,
                    error))
            {
                failure = "could not stage the verified script load image: "
                    + error.message();
                return false;
            }

            const auto copied = compiler::inspect_file(
                temporary, compiler::kDefaultMaximumArtifactBytes);
            if (!copied
                || copied.evidence.size_bytes
                    != compile.evidence.published.size_bytes
                || !(copied.evidence.content
                    == compile.evidence.published.content))
            {
                remove_regular_file(temporary);
                failure =
                    "staged script load image did not match compile evidence";
                return false;
            }
            if (!requireMissing(load_path, "script load image path"))
            {
                remove_regular_file(temporary);
                return false;
            }
            if (!platform::filesystem::atomic_replace_same_filesystem(
                    temporary, load_path, error))
            {
                remove_regular_file(temporary);
                failure = "could not publish the script load image: "
                    + error.message();
                return false;
            }

            const auto published = compiler::inspect_file(
                load_path, compiler::kDefaultMaximumArtifactBytes);
            if (!published
                || published.evidence.size_bytes
                    != compile.evidence.published.size_bytes
                || !(published.evidence.content
                    == compile.evidence.published.content))
            {
                remove_regular_file(load_path);
                failure =
                    "published script load image did not match compile evidence";
                return false;
            }
            return true;
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
        (void)scheduler;
        try
        {
            const std::string scriptName = logicalScriptName.empty()
                ? detail::script_binary_stem(sourcePath)
                : std::string(logicalScriptName);
            const std::filesystem::path dllPath =
                detail::compiled_library_path(sourcePath);

            report.scheduled.store(true, std::memory_order_relaxed);
            report.log_info("Scheduling bounded script reload for '"
                + scriptName + "'.");

            std::unique_lock<std::mutex> reloadLock{
                scriptReloadMutex, std::try_to_lock};
            if (!reloadLock.owns_lock())
            {
                const std::string message =
                    "[script] Another compile/reload operation is already active.";
                logger::warn("Scripting", message);
                report.log_error(message);
                co_return;
            }

            const auto sourceProbe = compiler::inspect_file(
                sourcePath, compiler::kDefaultMaximumScriptBytes);
            if (!sourceProbe)
            {
                const std::string message = "[script] Source preflight failed: "
                    + sourcePath.string() + " (" + sourceProbe.message + ")";
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }

            const std::uint64_t generation =
                scriptCompileGeneration.fetch_add(
                    1u, std::memory_order_relaxed) + 1u;
            const compiler::CompileRequest request{
                .source_path = sourcePath,
                .output_path = dllPath,
                .workspace_root = detail::script_workspace_root(sourcePath),
                .request_generation = generation,
                .project_generation = 1u,
                .require_selected_script_name = true,
                .require_expected_source = true,
                .expected_source = sourceProbe.evidence};
            const compiler::CompileResult compile =
                compiler::compile_script(request);
            report.record_compile_result(compile);
            if (!compile)
            {
                const std::string message = "[script] Compilation refused or failed: "
                    + std::string(compiler::compile_status_name(compile.status))
                    + "/" + std::string(
                        compiler::compile_refusal_name(compile.refusal))
                    + " - " + compile.message;
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }

            report.compiled.store(true, std::memory_order_relaxed);
            report.log_info("Published verified script artifact '"
                + scriptName + "' at generation "
                + std::to_string(generation) + ".");

            const std::filesystem::path loadPath =
                detail::load_image_path(dllPath, generation);
            std::string stageFailure{};
            if (!detail::stage_load_image(compile, loadPath, stageFailure))
            {
                const std::string message = "[script] " + stageFailure;
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }

#ifdef EPOCH_MAIN_HEADLESS
            detail::remove_regular_file(loadPath);
            const std::string message =
                "[script] Dynamic script execution is unavailable in headless mode.";
            logger::warn("Scripting", message);
            report.log_error(message);
            co_return;
#else
#ifdef _WIN32
            HMODULE candidateLib = ::LoadLibraryW(loadPath.c_str());
            if (!candidateLib)
            {
                detail::remove_regular_file(loadPath);
                const std::string message =
                    "[script] LoadLibrary failed for verified load image: "
                    + loadPath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }
            auto entry = reinterpret_cast<run_script_fn>(
                ::GetProcAddress(candidateLib, "run_script"));
            if (!entry)
            {
                ::FreeLibrary(candidateLib);
                detail::remove_regular_file(loadPath);
                const std::string message =
                    "[script] Verified library has no run_script symbol: "
                    + loadPath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }
#else
            void* candidateLib = ::dlopen(loadPath.c_str(), RTLD_NOW);
            if (!candidateLib)
            {
                detail::remove_regular_file(loadPath);
                const char* native = ::dlerror();
                const std::string message =
                    "[script] dlopen failed for verified load image: "
                    + loadPath.string() + (native ? " - " + std::string(native) : "");
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }
            auto entry = reinterpret_cast<run_script_fn>(
                ::dlsym(candidateLib, "run_script"));
            if (!entry)
            {
                ::dlclose(candidateLib);
                detail::remove_regular_file(loadPath);
                const std::string message =
                    "[script] Verified library has no run_script symbol: "
                    + loadPath.string();
                logger::error("Scripting", message);
                report.log_error(message);
                co_return;
            }
#endif

            report.dllLoaded.store(true, std::memory_order_relaxed);
            try
            {
                report.log_info("Executing run_script for '" + scriptName + "'.");
                entry(host);
                report.executed.store(true, std::memory_order_relaxed);
            }
            catch (...)
            {
#ifdef _WIN32
                ::FreeLibrary(candidateLib);
#else
                ::dlclose(candidateLib);
#endif
                detail::remove_regular_file(loadPath);
                report.dllLoaded.store(false, std::memory_order_relaxed);
                throw;
            }

            const auto previousLib = lastLib;
            std::filesystem::path previousPath{};
            try
            {
                previousPath = loaded_library_path();
                loaded_library_path() = loadPath;
            }
            catch (...)
            {
#ifdef _WIN32
                ::FreeLibrary(candidateLib);
#else
                ::dlclose(candidateLib);
#endif
                detail::remove_regular_file(loadPath);
                report.dllLoaded.store(false, std::memory_order_relaxed);
                throw;
            }
            lastLib = candidateLib;
            if (previousLib)
            {
#ifdef _WIN32
                ::FreeLibrary(previousLib);
#else
                ::dlclose(previousLib);
#endif
                detail::remove_regular_file(previousPath);
            }
#endif
        }
        catch (const std::exception& exception)
        {
            const std::string message =
                std::string("[script] Exception during script load: ")
                + exception.what();
            logger::error("Scripting", message);
            report.log_error(message);
        }
        catch (...)
        {
            const std::string message =
                "[script] Unknown exception during script load";
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
            if (!scheduler.AddNode(std::move(node)).valid())
            {
                const std::string message =
                    "[script] Task graph rejected the script load request.";
                logger::error("Scripting", message);
                report.log_error(message);
                return false;
            }

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
            if (!scheduler.AddNode(std::move(node)).valid())
            {
                const std::string message =
                    "[script] Task graph rejected the script load request.";
                logger::error("Scripting", message);
                report.log_error(message);
                return false;
            }

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
            if (!scheduler.AddNode(std::move(reloadNode)).valid())
            {
                summary.deadlockDetected = true;
                summary.deadlockSignature =
                    "TaskGraph rejected a stress reload node.";
                break;
            }

            for (std::size_t taskIndex = 0; taskIndex < config.tasksPerIteration; ++taskIndex)
            {
                Task work = make_stress_task(stressCompleted, config.taskDelay);
                auto node = std::make_unique<taskgraph::Node>(std::move(work));
                node->Label = "stress-task:" + std::to_string(iteration) + ":" + std::to_string(taskIndex);
                if (!scheduler.AddNode(std::move(node)).valid())
                {
                    summary.deadlockDetected = true;
                    summary.deadlockSignature =
                        "TaskGraph rejected a stress work node.";
                    break;
                }
            }

            scheduler.Execute();
            if (summary.deadlockDetected)
                break;

            if (config.reloadInterval.count() > 0)
                std::this_thread::sleep_for(config.reloadInterval);
        }

        const auto deadline = std::chrono::steady_clock::now() + config.maxDuration;
        auto lastProgress = std::chrono::steady_clock::now();
        std::size_t lastCompleted = scheduler.CompletedCount();

        while (!summary.deadlockDetected
            && std::chrono::steady_clock::now() < deadline)
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
