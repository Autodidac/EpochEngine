# Runtime Operations Guide

## Prerequisites
- Start from the launcher entry point (`RunEngine`) so the updater and scripting hooks are available inside the runtime loop before you launch any contexts.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L39-L107】
- Provision a `ScriptScheduler` instance before enabling hot reload; the scheduler drives compilation tasks and executes the reloaded entry point.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L68-L107】【F:AlmondShell/modules/ascripting.system.ixx†L198-L235】
- Keep your script sources under `src/scripts/` so the loader can resolve `<name>.ascript.cpp` inputs and produce matching DLLs in the same directory.【F:AlmondShell/modules/ascripting.system.ixx†L103-L175】

## Update Flow
- The launcher’s first stage is an updater check that can clear stale artifacts, query the hosted manifest, and pull down a fresh build when a new version is available.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L39-L62】
- When no update is required the routine falls back to a clean console state and continues to engine start-up, keeping operator feedback consistent between refresh cycles.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L55-L61】
- Re-run the launcher with `--update --force` to apply the update immediately: this path downloads the matching source archive, expands it beside the executable, provisions the toolchain helpers, and rebuilds the binary so the recovered repository tree is available for inspection or local modifications.【F:AlmondShell/modules/aupdatesystem.ixx†L324-L392】【F:AlmondShell/modules/aupdatesystem.ixx†L394-L418】

## Hot-Reload Cycle
- The sample runtime demonstrates polling the script timestamp roughly every 200 ms and calling `load_or_reload_script` whenever the file changes, keeping edit–compile–run latency tight.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L84-L104】
- `load_or_reload_script` schedules a coroutine that compiles the updated source into a DLL, unloads any previously mapped handle, and executes the exported `run_script` symbol once the new library is resident.【F:AlmondShell/modules/ascripting.system.ixx†L103-L235】
- The scheduler waits for all tasks to finish and prunes completed nodes so your tooling observes a deterministic runtime state after each reload.【F:AlmondShell/modules/ascripting.system.ixx†L206-L235】

## Logging
- Pull a logger for a subsystem with `logger::get("<system>")`, then call `log` or `logf` on the returned `SystemLogger` to emit lines tagged with your system name and optional source metadata.【F:AlmondShell/modules/aengine.core.logger.ixx†L109-L235】【F:AlmondShell/modules/aengine.core.logger.ixx†L299-L315】
- Logs go to the console and to `Logs/<system>.log` (the path comes from the `LogConfig::root_dir`, which defaults to `logs/`, so update it if you prefer a `Logs/` directory).【F:AlmondShell/modules/aengine.core.logger.ixx†L53-L99】【F:AlmondShell/modules/aengine.core.logger.ixx†L125-L168】
- Severity levels are `INFO`, `WARN`, `ALMOND_ERROR`, and `OFF`; set the minimum level by configuring the logger hub with `logger::init(LogConfig{ .level = LogLevel::WARN })` (or by passing a different `LogConfig` when configuring systems).【F:AlmondShell/modules/aengine.core.logger.ixx†L35-L83】【F:AlmondShell/modules/aengine.core.logger.ixx†L254-L284】

## Multi-Context Troubleshooting
- Releasing the previous library handle (`FreeLibrary`/`dlclose`) before loading the replacement prevents Windows and POSIX backends from pinning stale code when multiple contexts request the same script in quick succession.【F:AlmondShell/modules/ascripting.system.ixx†L120-L175】
- Windows builds that embed alternate front ends (SDL, Raylib) may route through dedicated entry points; ensure headless overrides are disabled when you expect the shared `RunEngine` path to initialise every context.【F:AlmondShell/examples/ConsoleApplication1/main.cpp†L39-L107】【F:AlmondShell/include/aengineconfig.hpp†L26-L35】
- When juggling several renderers, confirm each required backend macro is enabled so the scheduler can safely execute per-context script logic after reloads.【F:AlmondShell/include/aengineconfig.hpp†L53-L70】

## Adjusting `aengineconfig.hpp`
- Toggle `ALMOND_SINGLE_PARENT` to switch between a single parent window with children and fully independent top-level windows during multi-context debugging.【F:AlmondShell/include/aengineconfig.hpp†L53-L55】
- Enable or disable specific context integrations (`ALMOND_USING_SDL`, `ALMOND_USING_RAYLIB`, etc.) to replicate your live configuration in development environments before testing hot reload interactions.【F:AlmondShell/include/aengineconfig.hpp†L57-L66】
- If you rely on Raylib, keep the provided Windows header guards and symbol remaps so the build remains stable across toolchains; they are required for successful reloads on Win32 and should only be altered with care.【F:AlmondShell/include/aengineconfig.hpp†L89-L197】

## Interpreting `ScriptLoadReport`
- `ScriptLoadReport::reset` clears stage flags and previous log entries whenever a reload begins, so inspect the latest run before triggering another iteration.【F:AlmondShell/modules/ascripting.system.ixx†L28-L63】
- A failed compilation sets `failed=true` and records the diagnostic message, while successful progression flips each stage flag (`scheduled`, `compiled`, `dllLoaded`, `executed`) so you can pinpoint where the pipeline stalled.【F:AlmondShell/modules/ascripting.system.ixx†L16-L235】
- Use `messages()` to capture the accumulated info/error entries for your editor UI or logging sink when presenting reload results to contributors.【F:AlmondShell/modules/ascripting.system.ixx†L49-L67】

## Handling Reload Failures
- Missing sources, compilation errors, loader failures, and absent entry symbols each emit a targeted error message. Surface these to the developer so they can correct the underlying issue without guessing the failing stage.【F:AlmondShell/modules/ascripting.system.ixx†L103-L235】
- Scheduling exceptions are caught at the outer entry point, logged, and flagged in the report to prevent silent background failures when the task graph cannot start the reload job.【F:AlmondShell/modules/ascripting.system.ixx†L222-L235】

## TaskGraph Stress Test
- Use `run_taskgraph_reload_stress_test` to drive repeated script reloads alongside a burst of coroutine tasks; the helper returns a report containing completion counts, queue depth highs, reload failures, and deadlock detection flags so you can capture runtime stability metrics in one call.【F:AlmondShell/modules/ascripting.system.ixx†L69-L318】
- The stress test sources its queue depth and completion count telemetry from the TaskGraph worker loop, which tracks enqueued work and completed nodes to give visibility into saturation and drain behavior during reload cycles.【F:AlmondShell/modules/aengine.taskgraph.dotsystem.ixx†L47-L232】

### Failure Signatures
- **Deadlock: stall timeout while tasks remain pending** → The stress test saw no new completions for longer than the configured `stallTimeout` while nodes were still outstanding, and reports the condition via `deadlockDetected` and `deadlockSignature`.【F:AlmondShell/modules/ascripting.system.ixx†L247-L315】
- **Deadlock: timeout before expected completion** → The run exceeded `maxDuration` before reaching the expected node count, usually indicating an unreleased dependency chain or a worker pool saturated by blocking work.【F:AlmondShell/modules/ascripting.system.ixx†L270-L318】
- **Reload failures > 0** → One or more iterations failed to compile/load/execute, typically from missing scripts, compiler errors, or loader issues; inspect the ScriptLoadReport entries for the failing iteration to isolate the stage.【F:AlmondShell/modules/ascripting.system.ixx†L16-L318】

### Troubleshooting Steps
- Reduce `tasksPerIteration` or add a small `taskDelay` to avoid monopolizing worker threads; this helps the reload task drain and keeps queue depth from staying pinned at capacity during hot reloads.【F:AlmondShell/modules/ascripting.system.ixx†L69-L267】
- If deadlocks persist, confirm each coroutine completes in a single resume and does not suspend or wait on external locks; TaskGraph nodes are marked finished once resumed, so blocking/suspending work can strand dependencies and stall completion counts.【F:AlmondShell/modules/aengine.taskgraph.dotsystem.ixx†L81-L218】
- Verify the script path and exported `run_script` symbol so reload iterations succeed; repeated loader failures can appear as deadlocks when the reload node never reaches the executed stage, even though the queue drains.【F:AlmondShell/modules/ascripting.system.ixx†L103-L235】
