# Epoch systems consolidation — v0.90.24 through v0.90.32

This document is the compact map of the late-September integration series. Detailed behavior remains in the owning engine documents and per-release notes.

| Version | System | Durable result |
| --- | --- | --- |
| 0.90.24 | Neuromorphic mainline | Bounded weighted sparse graph, event-camera front end, timeline/task-graph adapters, and conservative render-invalidation pressure entered mainline. |
| 0.90.25 | Focus-owned input / AI protocol | Physical input fails closed outside the owning Epoch native context; internal authoring/tool/source packets are not rendered as user chat. |
| 0.90.26 | Vulkan module isolation | GLFW-only standalone Vulkan polling is isolated behind `EPOCH_VULKAN_STANDALONE`. |
| 0.90.27 | Vacated-region diagnostics | Previous object footprint can be inspected independently while remaining mandatory render damage. |
| 0.90.28 | A/B renderer benchmark | Deterministic SELECTIVE/FULL BASELINE alternation uses one persistent cache/present path and keeps independent timing accumulators; oscillator example supplies repeatable motion. |
| 0.90.29 | Editing / lighting / GUI | Shift multi-select, marquee selection, group transforms, batch cube creation, renderer-neutral bounded lighting invalidation, EpochGui shadows, multicontext top-row input fix, oscillator discoverability, Debug/Release x64 solution configs. |
| 0.90.30 | Telemetry / multicontext stability | Presentation FPS and measured render FPS are separated; statistics persist while disabled; docking guides/reopen behavior recover; detached AI Chat no longer owns a second AI runtime. |
| 0.90.31 | Temporal scene identity | Undo no longer reserves inactive object IDs and block subsequent creation. |
| 0.90.32 | MSVC static entry point | Reusable `EpochEngine.lib` no longer exports an application `main`/`wWinMain`; bundled ProjectLauncher paths are relocatable within the extracted source tree. |

## Renderer invariants

1. Previous and current object bounds are both damage.
2. Lighting damage is renderer-neutral: directional/global light changes are global; point/spot changes may be bounded.
3. Neuromorphic policy may promote work to full reconstruction but cannot suppress authoritative damage.
4. A/B FULL BASELINE reconstructs 100% through the same persistent cache/present path as SELECTIVE so comparison is apples-to-apples.
5. Presentation FPS and render throughput are different measurements and must remain labeled separately.
6. Diagnostic/statistical state is evidence and survives feature disable until explicit reset.

## Editor invariants

1. Selection is a set with one primary object; group transforms are one authored transaction.
2. Undo/redo history may retain inactive temporal slots, but only active identities participate in duplicate checks.
3. Physical keyboard/mouse input belongs to the focused native Epoch context.
4. Detached panes are projections of canonical editor state where ownership must remain singular; this is especially strict for AI/model/session lifetime.
5. Closed detached panes remain recoverable through their remembered dock and Window/command menu actions.

## Build invariants

1. `EpochEngine` is a reusable static library and therefore must not provide an application entry point.
2. Executables such as ProjectLauncher own exactly one entry point (`Engine/src/epoch.main.cpp` / generated project entry source as appropriate).
3. `Engine.sln` must expose both `Debug | x64` and `Release | x64` for the bundled projects.
4. Source packages must not depend on a Codex/worktree absolute path to compile the bundled ProjectLauncher.

## Current acceptance boundary

Source/contracts can prove ownership, registration, XML validity, and pure CPU behavior in this environment. Native MSVC link/runtime, GPU timing validity on the operator GPU, docking visuals, and multicontext Run-mode stability remain operator-side acceptance and must not be documented as proven until exercised there.
