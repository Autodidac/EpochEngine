# Epoch Engine History And Release Archive

This file is the single long-form history surface for Epoch's release and
milestone work. It replaces the older split between:

- `post_v0_83_work_collated.md`
- `release_notes_0_83_0.md`
- `release_notes_archive.md`
- `release_notes_version_series_collated.md`

## Snapshot and status tags

- **Stable release**: a packaged release or milestone note that was intended as
  a shippable runtime snapshot.
- **Development line**: the active source tree after a release moved forward.
- **Work log**: a useful progress checkpoint from active source work, but not
  necessarily a clean standalone packaged release.
- **Known-bad / regression risk**: a note that explicitly carried instability,
  breakage, or unresolved follow-up at the time.

Current orientation:

- Latest published stable runtime line: `v0.83.86`
- Current development source line: `v0.83.88`

## What the engine has become so far

From the archived work in this file, a few themes are clear:

- Epoch has moved from a small launcher/updater-heavy shell toward a
  project-centric runtime and editor that can launch, build, script, inspect,
  and iterate from one engine-owned workflow.
- The rendering stack has expanded into a real multicontext desktop tool
  surface across Raylib, SDL3, SFML3, Vulkan, OpenGL, and Software, with the
  harder work focused on honest docking, undocking, redocking, startup, and
  shutdown behavior rather than fake placeholders.
- The updater story matured from a fragile one-off replacement path into a real
  bootstrap/runtime flow with managed rebuilds, source fallback, packaged
  assets, and cross-platform release alignment.
- The editor shell has steadily become more professional: clearer top menus,
  project and script workspaces, systems diagnostics, time controls, AI
  workspace plumbing, and cleaner documentation/build discipline.
- The repo itself has been moving toward cleaner public naming, stronger docs,
  and fewer ambiguous legacy surfaces, even when the underlying engine work is
  still ongoing.

## Reading guide

- For the latest condensed engine story, start with the summary above.
- For the `v0.83.x` era after the first updater-shell milestone, read the
  `Post-v0.83.0 Work Log` section first.
- For the long release-by-release history before that, continue into the
  archived release notes below.
- Use the tags in each section to quickly tell whether an entry describes a
  stable release, active development work, or a caution/regression period.

---

## Post-v0.83.0 Work Log

These notes were previously tracked in `post_v0_83_work_collated.md` and are
kept here because they describe the active engine/editor direction more directly
than the older updater-era release cadence.

### [Work Log | Development line] v0.83.59
- Restored SDL and SFML dock takeover ownership so the real backend child
  window remains the active pane after docking settles, while the placeholder
  host stays available only as the container.
- Unified Vulkan clear color with the shared context palette so Vulkan no
  longer drifts visually from the other editor backends.
- Pulled the uploaded `botface.html` into repo history, then restored the
  cleaner UTF-8 working copy on top so the worktree and GitHub stay aligned
  without preserving the broken encoded variant.

### [Work Log | Development line] v0.83.54
- Wired Win32 child-window wheel and text/key messages back into the editor GUI
  path so docked editor contexts can finally deliver wheel zoom and live AI
  chat input instead of relying on dead plumbing.
- Flattened the preview look-hit marker onto the grid plane and removed the
  floating camera-to-hit indicator so the center-look spot reads like an editor
  targeting aid instead of a hovering false collision.
- Replaced the placeholder asset/actions shell with real editor entity controls
  for adding meshes, lights, spawns, duplicating the current selection,
  deleting the current selection, and sending a live scene prompt into the AI
  panel.

### [Work Log | Development line] v0.83.53
- Added editor viewport drag-pan so held left mouse movement now slides the
  preview focus while wheel zoom and right-drag orbit remain active.
- Added a shared camera look-hit marker derived from the center view ray and
  projected it through the active preview backends so the current focus spot is
  visible in-editor.
- Tightened software preview invalidation and telemetry churn by tracking
  camera revisions directly and only re-emitting software framebuffer gauges
  when their values actually change.
- Captured a fresh 4K six-context editor proof from the asset-bearing local
  runtime for the README.

### [Work Log | Development line] v0.83.52
- Aligned the Windows resource move into `Engine/resource/` at the repo/build
  level, including the CMake resource path and the active Visual Studio
  shared-item surface.
- Refreshed the editor chrome toward a proper tool-style shell with `File`,
  `Edit`, `Asset`, `Window`, `Tools`, and `Help` plus mode/system tabs instead
  of fake project placeholders.
- Added mouse-wheel zoom for the editor preview camera and surfaced the live
  zoom value in the inspector/output panels.
- Captured and documented the refreshed multicontext editor run from the
  asset-bearing `x64/Debug` runtime so the README reflects the actual current
  local launch path.

### [Work Log | Development line] v0.83.51
- Kept the Windows multi-context host visible for docked SDL and SFML panes so
  backend child windows no longer disappear behind the editor host during local
  smoke runs.
- Improved Windows host/child handle matching in the multiplexer so resize,
  enqueue, cleanup, and dock layout logic follow the real live pane handle
  instead of only the original host HWND.
- Documented the asset-bearing output-folder launch path for multi-context
  smoke tests so local editor runs match the runtime asset layout used by the
  docked backends.

### [Work Log | Development line] v0.83.4
- Rolled `main` forward after the packaged `0.83.3` updater-shell release so
  the public binary can demonstrate the live fallback path into the newer
  source revision with the repaired runtime-version comparison.
- Fixed managed-vcpkg baseline rewriting inside updater sandboxes so the source
  fallback no longer walks up to the Epoch repo's own `HEAD` when it needs a
  vcpkg registry git revision.
- Made the source-update worker ignore any inherited `VCPKG_ROOT` and pin a
  worker-local managed toolchain instead, so updater rebuilds stay isolated
  from preexisting user vcpkg installs.

### [Work Log | Development line] v0.83.3
- Rolled `main` forward after the packaged `0.83.2` updater-shell hotfix
  release so the public binary can demonstrate the full release-to-source
  update path against a newer source revision.
- Trimmed the source-update manifest down to the graphics-only `SFML` and
  non-audio `raylib` feature set so updater-managed rebuilds no longer depend
  on the flaky `libogg`/`vorbis`/`openal` download chain.

### [Work Log | Development line] v0.83.2
- Fixed the updater worker's managed-vcpkg Git `HEAD` detection so the public
  shell can continue past managed tool bootstrap and reach the real
  restore/build/restart path during source fallback updates.

### [Work Log | Development line] v0.83.1
- Fixed the updater-shell source fallback so it always uses the
  updater-managed `vcpkg` toolchain instead of mutating any configured local
  `VCPKG_ROOT` checkout during the rebuild path.

---

# [Stable Milestone Release] Epoch 0.83.0 Release Notes

Epoch 0.83.0 marks the transition to the updater-shell bootstrap flow.

## Highlights

- Added a dedicated updater-shell entry build for `ConsoleApplication1.exe`.
- Simplified the launcher path so the bootstrap binary can focus on update,
  rebuild, handoff, and restart.
- Added visible version reporting in the launcher and editor surfaces.
- Cleaned up startup console output so update activity is easier to read.
- Improved GUI text wrapping and updater-shell copy so update instructions are
  clearer.

## Updater Shell

- The updater shell is now the intended binary entry point for users who need
  to move from a packaged build to a newer source snapshot.
- The shell can close its own window and continue the update in the console
  while the worker finishes.
- The update description now warns users not to interrupt the process once
  update starts.
- The release payload for the shell is intentionally slim: executable, required
  runtime DLLs, and the GUI font asset.

## Source Update Path

- Source-version parsing now reads the version macros in
  `aengine.version.ixx` correctly.
- The worker now falls back from packaged update checks to source update checks
  more reliably.
- Managed `vcpkg` handling was hardened so the worker keeps its own registry
  snapshot instead of escaping into an outer repository.
- Inherited `VCPKG_ROOT` values are ignored during worker execution so existing
  user installs are not repointed or rewritten.
- The worker prepares its own managed toolchain state before restore/build,
  keeping update behavior more deterministic.

## Runtime and Stability

- Windows unattended updater runs now avoid interactive debug/assert popups when
  no debugger is attached.
- The debug assert path only triggers `DebugBreak()` when a debugger is
  actually present.
- Updater validation now works better from short-path sandboxes, which better
  matches real release usage.

## User-Facing Outcome

The 0.83.x line is the first line intended to demonstrate a practical bootstrap
flow:

1. Start from the packaged updater shell.
2. Check the latest packaged release.
3. If packaged is current but `main` is newer, fall through to source update.
4. Restore dependencies, build the newer source snapshot, replace the runtime,
   and relaunch.

---

# [Stable Release] Epoch v0.83.85 Release Notes

## Highlights
- Restored the six-pane SDL3/SFML3 promoted-proxy contract on Windows so the
  visible proxy shell can detach as a real top-level host instead of remaining
  visually trapped inside the parented editor grid.
- Re-aligned the packaged runtime story across Windows and Linux: the normal
  packaged product path is the main runtime, while updater-shell mode is now an
  explicit bootstrap-only build.
- Refreshed the README proof set with current `v0.83.85` Windows and WSL/Linux
  captures, plus an explicit promoted-window undock screenshot.

## Verification
- Rebuilt `Engine.sln` in `Debug|x64` and `Release|x64` with MSVC.
- Verified [x64/Release/ConsoleApplication1.exe](../x64/Release/ConsoleApplication1.exe)
  reports `Epoch v0.83.85`.
- Rebuilt the Linux main-runtime path under WSL2 with clang/OpenGL/software and
  verified [Engine/Bin/Clang-Release/epoch](../Engine/Bin/Clang-Release/epoch)
  reports `Epoch v0.83.85`.

## Notes
- Clean human six-pane validation remains the final truth for the SDL/SFML
  promoted-window behavior when the synthetic harness diverges.
- The Linux proof image is captured from the asset-bearing WSL output rather
  than a bare source-tree launch so fonts and runtime assets stay aligned.

---

# AlmondShell v0.62.1 Release Notes

## Highlights
- Fixes a cosmetic regression in the updater where `curl` error output was glued to AlmondShell's diagnostics without a newline.
- Keeps the updater's failure handling defensive by trimming empty artifacts and surfacing the failure reason immediately.
- Synchronises the bundled version metadata and changelog so integrators can verify the expected runtime snapshot.

## Known Issues
- Automated renderer regression scenes remain under development; see `Engine/docs/engine/renderer_regression_smoke_plan.md` for the intended coverage.
- Crash reporting hooks have not landed yet, so crashes must be reproduced locally with a debugger attached.

## Roadmap Alignment
This release ticks off the Phase 5 documentation task to "Draft release notes summarising new features, known issues, and roadmap alignment" from `Changes/roadmap.md`.

---

# AlmondShell v0.62.7 Release Notes

## Highlights

- ðŸ› ï¸ **OpenGL renderer unblocked** â€“ The renderer now references the shared
  `openglcontext::OpenGL4State` when drawing quads and debug outlines, fixing the
  missing `s_openglstate` symbol that previously broke builds.

## Fixes & Improvements

- **OpenGL backend** â€“ Scoped all quad and debug-outline draws to the global
  renderer state so OpenGL builds can link successfully again.

## Documentation

- Updated the README snapshot, changelog, and engine analysis to record the
  OpenGL renderer fix and bumped version.

## Upgrade Notes

No manual action is requiredâ€”pull the release and rebuild to pick up the
OpenGL renderer fix and refreshed documentation.

---

# AlmondShell v0.70.0 Release Notes

## Highlights
- Fixes a segmentation fault when the Software backend spins up alongside Raylib on Linux by wiring the software renderer into the X11 multiplexer initialisation.
- Registers the software renderer before render threads launch so resize callbacks, atlas uploads, and framebuffer allocation succeed from the first frame.

## Known Issues
- The software renderer still renders to an in-memory framebuffer on Linux; presenting to a native window remains a future enhancement.
- Vulkan and DirectX backends are stubbed out on non-Windows platforms.

## Upgrade Notes
- Rebuild the project after pulling to pick up the Linux multiplexer changes and the new version number.

---

# AlmondShell v0.70.4 Release Notes

## Summary
- Unified the internal OpenGL renderer around the backend-managed GL state so quad and debug-outline helpers reuse the shader/VAO pipeline seeded by `opengl_initialize()` on Windows and Linux.
- Bumped the engine metadata and documentation to v0.70.4 so diagnostics and packagers report the refreshed runtime snapshot.

## Upgrade Notes
- Rebuild or reconfigure existing worktrees to pick up the new version string and ensure downstream packages include the renderer fix.
- If you ship platform-specific manifests, update them to reference v0.70.4 so launchers advertise the synced metadata.

---

# AlmondShell v0.71.0 Release Notes

## Summary
- Raised the engine toolchain to the C++23/module baseline and documented the migration so downstream packagers can rebuild with BMI-aware compilers and generators.
- Refreshed the README, configuration guide, and engine analysis to surface the v0.71.0 snapshot alongside the module-conversion guidance.

## Upgrade Notes
- Start from a clean build tree and enable module scanning (`CMAKE_CXX_SCAN_FOR_MODULES`/`CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP`) when configuring CMake to pick up the new baseline.
- Ensure your launcher manifests and packaged metadata reference v0.71.0 so diagnostics and update checks advertise the correct release.

---

# AlmondShell v0.72.5 Release Notes

## Summary
- Completed the module migration for the context/back-end stack, covering the multiplexer, window/render plumbing, and backend bindings for OpenGL, SDL3, Raylib, and the software renderer with `.ixx` partitions that expose importable surfaces.
- Documented the expanded module coverage and new import targets (`aengine.context`, `aengine.context.window`, `aengine.context.render`, plus backend-specific partitions) so downstream consumers can target contexts/backends directly from modules.

## Upgrade Notes
- Clear or regenerate your build tree to refresh BMI outputs and ensure the new context/back-end partitions are scanned during configuration.
- Update any downstream integrations to prefer the module imports over legacy headers when targeting contexts or backend renderers.

---

# AlmondShell v0.72.6 Release Notes

## Highlights
- Bumped the engine metadata so `aversion.hpp`, runtime banners, and documentation all advertise v0.72.6 consistently.
- Cleaned up the README snapshot, engine analysis brief, and configuration flag guide to remove stale references to earlier snapshots and better describe the current module/back-end layout.
- Recorded the documentation refresh in the changelog and release notes to keep downstream packagers aligned on the new version.

## Upgrade Notes
- Reconfigure builds after pulling to pick up the updated version helpers and ensure cached artifacts do not report the older revision.
- Regenerate documentation (Doxygen or static site) if you publish the reference so the refreshed snapshot metadata appears in the output.

---

# AlmondShell v0.72.7 Release Notes

- Added module partitions for the command queue, updater system, diagnostics, binding/handle helpers, and menu overlay so consumers can import the `aengine.*` surfaces directly without relying on legacy headers.
- Updated the README snapshot, module mapping guide, engine analysis, and configuration flag notes to document the expanded module coverage and keep metadata aligned with the v0.72.7 release tag.

---

# AlmondShell v0.72.8 Release Notes

- Synced version metadata across `aversion.hpp`, module partitions, and documentation so runtime banners, tools, and docs all advertise v0.72.8.
- Completed header-to-module conversions for atlas helpers and command-line plumbing, exposing `import aatlas.manager;`, `import aatlas.texture;`, and `import acommandline;` for mixed include/import builds.
- Clarified updater/configuration guidance so launcher manifests and release metadata pull their version strings from the runtime helpers and stay locked to v0.72.8.

---

# AlmondShell v0.72.9 Release Notes

- Bumped the engine version constants and legacy headers so runtime banners, tooling, and documentation agree on v0.72.9.
- Refreshed the README snapshot, engine analysis notes, and configuration guide changelog to advertise the v0.72.9 tag.
- Synced the changelog entry with the new release metadata for consistent snapshot reporting.

---

# Epoch v0.81.23 Release Notes

This is the last pre-rebrand snapshot before the public docs moved to the Epoch name.

- Bumped the engine version constants so runtime banners, tooling, and documentation agreed on v0.81.23.
- Refreshed the README snapshot, engine analysis notes, and configuration guide changelog to advertise the v0.81.23 tag.
- Synced the changelog entry with the release metadata for consistent snapshot reporting.
- Documented the C++23 module-first baseline so tooling and packagers could align module scanning, BMI generation, and compiler flags.

---

# Epoch v0.82.0 Release Notes

- Rebranded the public README, docs index, and release metadata from the previous public name to Epoch.
- Synchronized version helpers and the legacy compatibility version header to v0.82.0.
- Moved the standalone legacy archive into `Engine/legacy/` and replaced scattered older notes with a single legacy archive guide.
- Refreshed the build, runtime, configuration, and architecture docs so they describe the current repo layout and backend support more honestly.

---

# Epoch v0.82.10

## Summary

v0.82.10 is a manual docking interaction repair. It removes the temporary
modifier-key requirement and restores real left-drag pane docking through a
small pane drag strip, while keeping the parent-host shutdown and redock fixes
from the previous hotfixes.

## Runtime updates

- Replaced the temporary `Alt+Left Mouse` docking gesture with a dedicated drag
  strip at the top of each pane.
- Preserved the original dock-parent tracking and docked-only grid layout so
  panes can still undock and redock cleanly after becoming top-level windows.
- Kept the verified parent-host shutdown path so closing `EpochParent` still
  exits the whole session cleanly.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Ran live left-drag undock/redock interaction checks against SDL, Raylib,
  SFML, OpenGL, and Software panes.
- Confirmed each tested pane detached to a top-level window and then reattached
  to `EpochParent`.
- Closed the parent host and confirmed the process exited cleanly afterward.

---

# Epoch v0.82.11

## Summary

v0.82.11 hardens parent-host shutdown ownership for the parented multi-context
runtime. Docked panes now close with `EpochParent`, undocked panes survive as
independent top-level windows, and redocked panes become parent-owned again.

## Runtime updates

- Changed parent-host shutdown to target the authoritative live backend window
  for each context instead of stale placeholder hosts.
- Preserved undocked SDL, Raylib, and SFML panes when the parent host closes.
- Restored parent-owned shutdown semantics automatically once a pane is
  redocked.
- Fixed the last-window exit path so the process shuts down cleanly when the
  final surviving undocked pane closes.
- Added a live-window guard in the engine loop so stale backend bookkeeping does
  not strand the session after the last real native window disappears.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Rebuilt `StaticLib1` in `Debug|x64`.
- Ran live undock/parent-close/survivor-close verification against:
  - SDL (`SDL_app`)
  - Raylib (`GLFW30`)
  - SFML (`SFML_Window`)
- Confirmed:
  - undocked panes survive parent close
  - redocked panes close with the parent again
  - the process exits after the final surviving undocked pane closes

---

# Epoch v0.82.12

## Summary

v0.82.12 is a launcher/editor workflow pass. Projects and playable game
entries now live in the launcher, while the editor presents a more traditional
desktop-style top menu with scene preview controls and confirmation-gated
update actions.

## Runtime updates

- Split the old mixed command strip so the launcher owns project selection,
  game launching, and tool entry points.
- Reworked the editor shell into `File`, `Edit`, `Scene`, `Command`, and
  `Help` menus.
- Added editor scene preview switching between `Editor` and `None`.
- Added an in-editor confirmation modal before running the updater because the
  action can replace binaries and restart the current session.
- Kept launcher-driven project selection wired into editor mode and game
  selection wired into scene/runtime mode.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched the real binary from `x64/Debug`.
- Verified the new source flow compiles cleanly and the runtime stays healthy
  during launch with the refactored launcher/editor routing.

---

# Epoch v0.82.13

## Summary

v0.82.13 is a mixed-backend visual consistency pass. It aligns the active
backend base color with the darker Vulkan look, moves SFML ahead of Vulkan in
the default parented dock order, and fixes the SFML shared scene preview so it
stays clipped to the intended scene viewport.

## Runtime updates

- Changed the active backend base clear color table so the main backends share
  the same darker Vulkan-style baseline.
- Moved SFML ahead of Vulkan in the default parented grid ordering.
- Reworked SFML shared scene preview drawing to use a clipped sub-view so the
  preview no longer leaks into GUI space when the window is being manipulated.
- Kept the shared preview-grid geometry/path intact across the active backends.

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`.
- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched the real binary from `x64/Debug`.
- Verified the live parented top row now places Raylib, SDL, and SFML ahead of
  Vulkan.
- Checked the latest runtime logs for fresh mixed-backend bring-up errors after
  the change.

---

# Epoch v0.82.14

## Summary

v0.82.14 is a public-facing documentation pass. It updates the README and
active version surfaces so Epoch is described in terms that match the actual
engine: internal bootstrap, multi-context runtime, launcher/editor workflow,
cross-platform build freedom, module-first architecture, and built-in tooling
systems.

## Documentation updates

- Rewrote the top README description so it presents Epoch as a world-class,
  AI-enabled, C++23 modules-first engine instead of a thin runtime summary.
- Expanded the "What Epoch provides" section to call out:
  - internalized engine bootstrap and entry flexibility
  - multi-context, multi-backend orchestration
  - launcher/editor workflow
  - atlas-driven GUI and sprite systems
  - scripting, file-watch, and task-graph-backed iteration
  - telemetry, diagnostics, and updater plumbing
  - cross-platform, editor-agnostic build freedom
  - module-first public engine surface
- Added direct markdown links from the README into the active build/docs/project
  surfaces so the repository front page works as a real navigation hub.

## Verification

- Pulled the latest `main` before editing.
- Checked the active docs/build surfaces referenced by the README against the
  current repo layout.
- Refreshed the active version metadata to `v0.82.14`.

---

# Epoch v0.82.15

## Highlights

- Replaced the old filewatch-oriented editor scripting loop with an engine-owned
  `Run` action that compiles and executes scripts against a stable host API.
- Added reusable shared preview cameras with `Editor` and `FPS` modes,
  keyboard travel, and right-mouse look for the multicontext editor scene.
- Fixed updater version parsing so remote checks can read either plain semver
  text or the full `aengine.version.ixx` module without dumping banner/BOM
  garbage into the console.

## Active code surfaces

- [Engine/include/epoch.script_api.h](../Engine/include/epoch.script_api.h)
- [Engine/modules/ascripting.system.ixx](../Engine/modules/ascripting.system.ixx)
- [Engine/modules/aengine.scripting.compiler.ixx](../Engine/modules/aengine.scripting.compiler.ixx)
- [Engine/src/scripts/rotate_all_entities.ascript.cpp](../Engine/src/scripts/rotate_all_entities.ascript.cpp)
- [Engine/modules/epoch.render.preview_grid.ixx](../Engine/modules/epoch.render.preview_grid.ixx)
- [Engine/src/aengine.cpp](../Engine/src/aengine.cpp)
- [Engine/src/aeditor.cpp](../Engine/src/aeditor.cpp)
- [Engine/modules/aengine.updater.system.ixx](../Engine/modules/aengine.updater.system.ixx)

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`
- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Compiled and invoked the default `rotate_all_entities` script against the new
  host API
- Launched the debug editor/runtime from `x64/Debug`

---

# Epoch v0.82.16

## Highlights

- Fixed the Windows script compiler path so editor-run scripts launch `clang++`
  directly instead of routing through a fragile shell command string.
- This removes the `C:/Program` split failure when LLVM is installed under
  `Program Files`.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Launched the debug app from `x64/Debug`
- Confirmed the script source file remains ASCII-only

---

# Epoch v0.82.3 Release Notes

- Aligned the Vulkan preview palette with the OpenGL editor colors so active GPU backends present the same core scene look.
- Reduced Vulkan GUI flicker by reusing the last valid GUI batch when the producer misses a frame.
- Updated Vulkan GUI atlas sampling to match the OpenGL path more closely and reduce GUI edge bleeding.
- Refreshed the active version/docs surfaces to advertise the v0.82.3 Vulkan preview stabilization pass.

---

# Epoch v0.82.4 Release Notes

- Restored the Vulkan editor scene draw so the viewport now renders the editor grid instead of a flat clear pass.
- Kept the Vulkan editor preview aligned with the OpenGL editor look using the same dark palette and a stable fixed preview camera.
- Tightened the SFML shutdown path so GPU atlas cleanup runs before the context dies, reducing close-time activation failures in mixed backend sessions.

---

# Epoch v0.82.5

## Summary

v0.82.5 focuses on startup/runtime hygiene and public-facing project guidance.
The engine no longer spends normal launches appending ad hoc Vulkan frame traces
or per-frame SFML path messages, and the shared renderer slow-frame diagnostics
now warn in a controlled way instead of flooding logs during initialization.

## Runtime updates

- Disabled the unconditional `vulkan_runtime_diag.txt` write path for standard runs.
- Added startup grace + throttling to renderer slow-frame warnings.
- Removed the hot-loop SFML render-path info log from the frame path.

## Documentation updates

- Rewrote the top-level README around the active Epoch runtime tree.
- Expanded build guidance for Visual Studio, MSBuild, CMake presets, VS Code,
  shell scripts, and binary-folder launches.
- Updated the configuration and analysis docs to reflect the quieter runtime
  diagnostics behavior and the `v0.82.5` snapshot.

---

# Epoch v0.82.6

## Summary

v0.82.6 is a backend parity and parented-startup stability pass. The editor
scene preview is now shared across SDL and Software instead of falling back to
the placeholder viewport card, and the parented Raylib path keeps the original
WGL context/DC pairing captured during initialization.

## Backend updates

- Added the shared `epoch.render.preview_grid` scene-view rendering path to the
  SDL backend.
- Added the shared `epoch.render.preview_grid` scene-view rendering path to the
  Software backend.
- Updated the editor GUI viewport logic so those backends now own the scene
  viewport instead of drawing the old placeholder card over it.
- Preserved the Raylib GL context with its original captured device context
  instead of replacing it with a fresh `GetDC(...)` handle after startup.

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`.
- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched from `x64/Debug` and verified Software-only and SDL-only editor runs
  visually show the shared preview grid.
- Re-ran the parented mixed-context launch and confirmed the old Raylib
  `Failed to make raylib context current during process` shutdown line no longer
  appeared in the latest run logs.

---

# Epoch v0.82.7

## Summary

v0.82.7 is a shutdown and diagnostics discipline pass. Closing the main parent
window now propagates shutdown through the docked context panes and exits the
session cleanly, while backend confirmation messages are available again behind
config macros instead of living as hot-loop spam.

## Runtime updates

- Hardened the parent-host `WM_CLOSE` and `WM_DESTROY` flow so tracked child
  contexts are marked for shutdown before the parent window is destroyed.
- Updated the engine loop to honor the multiplexer running state immediately
  after the pump step, which prevents the console/process from hanging after
  the visible parent window is gone.
- Kept backend shutdown local to the affected backends instead of reintroducing
  broad early-close behavior in the shared multiplexer.

## Diagnostics updates

- Added the master `EPOCH_ENABLE_BACKEND_CONFIRMATION_LOGS` switch plus the
  matching context/upload/backend-specific confirmation flags in
  `aengine.config.hpp`.
- Restored one-shot confirmation messages for backend bring-up, uploads, and
  cleanup without bringing back the old per-frame log flood.

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`.
- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched from `x64/Debug`, closed the real `EpochParent` host window, and
  verified the process exited cleanly with no forced kill.

---

# Epoch v0.82.8

## Summary

v0.82.8 is a focused docking/shutdown hotfix. Closing the main parent host now
keeps backend panes docked in place while shutdown propagates, instead of
undocking them as part of the host close sequence.

## Runtime updates

- Removed the parent-close undock message from the Win32 multiplexer shutdown
  path so backend-owned child windows are closed in place.
- Preserved the verified parent shutdown behavior that marks tracked panes for
  close and lets the process exit cleanly when the `EpochParent` host closes.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Launched in `--parented --editor` mode from `x64/Release`.
- Verified that backend panes were still parented under the live `EpochParent`
  window at runtime.
- Closed the real parent window and confirmed the process exited cleanly.

---

# Epoch v0.82.9

## Summary

v0.82.9 is a docking interaction repair pass. It restores real undock/redock
behavior for parented panes by preserving each pane's original dock host and by
keeping the parent grid limited to panes that are actually still docked.

## Runtime updates

- Preserved the original dock parent on each dockable pane so future drags can
  redock a pane even after it has already been undocked once.
- Updated the parent grid arranger so only panes still parented under
  `EpochParent` are laid out, which stops undocked panes from snapping back or
  stealing space from docked panes.
- Kept the parent-host shutdown fix from `v0.82.8`, so closing the main host
  still exits the full session cleanly.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Ran live undock/redock interaction tests against SDL and Raylib panes.
- Ran a broader live undock/redock sweep against SFML, OpenGL, and Software.
- Confirmed all of those panes undocked to top-level windows, redocked back
  into `EpochParent`, and that closing the parent host still exited the process.

---

# Epoch v0.82.17

## Highlights

- Consolidated the older versioned release-note markdowns into a single archive
  file so the `Changes/` folder keeps one historical notes surface while new
  releases continue as individual version files.
- Removed the accidentally tracked Windows `.exp` script-build artifact from
  the repo and added ignore coverage for `.exp` files and the local
  `Temp Script Test/` workspace.

## Verification

- Confirmed the accidental `.exp` artifact was removed from Git tracking
- Added ignore rules so the same class of local script-build outputs stops
  reappearing
- Refreshed the active release/version surfaces to `v0.82.17`

---

# Epoch v0.82.18

## Highlights

- Fixed the remaining Windows editor script compiler spawn bug by using a
  direct executable launch for real LLVM paths instead of the PATH-search
  variant.
- This closes the last known `Program Files` path split that still produced
  `Files/LLVM/bin/clang++.exe` style failures during editor-run script builds.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Re-checked the compiler launch logic against the installed LLVM path
- Refreshed the active release/version surfaces to `v0.82.18`

---

# Epoch v0.82.19

## Highlights

- Fixed the self-update flow so it now targets the currently running executable
  instead of the old hardcoded `updater.exe` replacement path.
- Added explicit failure reporting for update handoff/install failures so the
  app no longer appears to update successfully when the download or replacement
  step did not complete.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Re-ran the update code path against the rebuilt app
- Refreshed the active release/version surfaces to `v0.82.19`

---

# Epoch v0.82.20

## Highlights

- Refreshed the active version, README, changelog, and current docs to
  `v0.82.20`.
- This bump exists specifically to give the self-update flow a fresh live
  target during manual updater validation.

## Verification

- Confirmed `main` was already current with `origin/main`
- Updated the active release/documentation surfaces to `v0.82.20`

---

# Epoch v0.82.21

## Highlights

- Rolled the active version and current docs forward to `v0.82.21`
  immediately after the `0.82.20` release tag.
- This gives the self-update flow a newer target than the packaged
  `0.82.20` release snapshot.

## Verification

- Confirmed the `0.82.20` tag exists on `origin`
- Updated the active version/documentation surfaces to `v0.82.21`

---

# Epoch v0.82.22 Release Notes

## Highlights

- Fixed the self-updater to target the shipped `main.zip` GitHub release asset.
- Updated the runtime replacement path so Windows installs from the packaged
  runtime directory instead of assuming GitHub serves a single replacement
  executable.
- Routed updater progress/status messages through Epoch logging so captured
  output no longer shows raw control-character glyphs at line endings.

## Technical Notes

- The built-in version check now points at
  `Engine/modules/aengine.version.ixx` on `main`.
- The release download target now points at
  `releases/latest/download/main.zip`.
- Windows update extraction now uses the platform archive tooling path instead
  of assuming a one-file executable swap.

---

# Epoch v0.82.23 Release Notes

## Highlights

- Rolled the active version and current documentation forward immediately after
  the fixed `0.82.22` package tag.
- Keeps `main` ahead of the packaged release so the updater has a new target to
  detect during follow-up validation.

---

# Epoch v0.82.24 Release Notes

## Highlights

- Fixed the updater to query the latest actual GitHub release instead of the
  `main` branch version file.
- Keeps version detection aligned with the downloadable `main.zip` release
  asset, which prevents `404` mismatches when `main` is ahead of the latest
  packaged release.

---

# Epoch v0.82.25 Release Notes

## Highlights

- Rolled the active version and documentation forward after the fixed
  `0.82.24` full release.
- Keeps the working tree ahead of the packaged updater snapshot while the
  updater now correctly follows the latest actual GitHub release.

---

# Epoch v0.82.26 Release Notes

## Highlights

- Restored visible console feedback for updater runs launched from the GUI.
- The updater now prints clear status lines even when no newer packaged release
  is available, instead of appearing to do nothing.

---

# Epoch v0.82.27 Release Notes

## Highlights

- Rolled the active version and documentation forward after the `0.82.26`
  updater-feedback release.
- Keeps the repo ahead of the currently packaged release while the updater now
  follows the latest real GitHub release and prints visible status in the GUI
  path.

---

# Epoch v0.82.28 Release Notes

## Highlights

- Cleaned up the editor update confirmation modal with separate actions for
  packaged runtime updates and source-snapshot downloads.
- Added a deliberate source-snapshot path for advanced testing when `main` is
  ahead of the latest packaged release.
- Source snapshot downloads now extract beside the runtime and do not replace or
  rebuild the running binary.

---

# Epoch v0.82.29 Release Notes

## Highlights

- Rolled the active version and documentation forward after the `0.82.28`
  update-flow release.
- Keeps the live repo ahead of the packaged runtime while the new update modal
  and source-snapshot path are available in the downloaded build.

---

# Epoch v0.82.30 Release Notes

## Highlights

- Restored the source-update path so it no longer stops at a downloaded source
  snapshot. It now restores manifest dependencies with `vcpkg`, rebuilds the
  runtime from the extracted tree, and replaces the running binary from that
  fresh build output.
- Fixed updater version comparison so local builds newer than the latest
  packaged release no longer attempt to downgrade themselves.
- Normalized updater console line output so captured update logs stop showing
  raw CR/LF glyphs at the end of status lines.

---

# Epoch v0.82.31 Release Notes

## Highlights

- Rolled the active version and documentation forward after the `0.82.30`
  updater rebuild release.
- Keeps the live repo ahead of the packaged runtime so the updater can detect a
  newer development target after the released `main.zip`.

---

# Epoch v0.82.32 Release Notes

## Highlights

- Fixed updater version checks to use unique temp files, eliminating the
  `remote_version.txt` file-in-use collision during packaged plus source probe
  runs.
- Kept source-snapshot update detection distinct from packaged-release
  detection, so the updater can report the two paths clearly.
- Cleaned the editor update confirmation modal so its content no longer bleeds
  into the title bar.

---

# Epoch v0.82.33 Release Notes

## Highlights

- Rolled the active version, docs, and README snapshot forward after the
  packaged `0.82.32` release.
- Keeps `main` ahead of the downloadable runtime so source-update checks can
  still see a newer live target.

---

# Epoch v0.82.34 Release Notes

## Highlights

- Removed the updater module's extra direct console output path.
- Updater status now relies on the engine logging path so the commandline view
  stops duplicating updater lines.

---

# Epoch v0.82.35 Release Notes

## Highlights

- Rolled the active version, docs, and README snapshot forward after the
  packaged `0.82.34` release.
- Keeps `main` ahead of the downloadable runtime so source-update checks can
  still see a newer live target.

---

# Epoch v0.82.36 Release Notes

## Highlights

- Restored a single visible updater status stream for the runtime update flow.
- Direct updater console output now only activates when the engine logger is not
  already configured to own console output.

---

# Epoch v0.82.37 Release Notes

## Highlights

- Rolled the active version, docs, and README snapshot forward after the
  packaged `0.82.36` release.
- Keeps `main` ahead of the downloadable runtime so source-update checks can
  still see a newer live target.

---

# Epoch v0.82.38

## Highlights

- Fixed the updater fallback so a confirmed update now rebuilds from source when `main` is newer and no newer packaged runtime exists.
- Removed the duplicate source-version probe so the updater stops printing the same source comparison twice before a source rebuild.
- Simplified updater status reporting to one direct console stream during update work, which keeps source/package update progress visible without the earlier doubled logic.

## Notes

- Packaged runtime releases still ship as `main.zip`.
- `main` will continue to move ahead after this release so source-update checks can detect newer source snapshots between packaged drops.

---

# Epoch v0.82.39

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.38` release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the post-release target version.

## Notes

- The packaged updater release is `0.82.38`.
- The active source target on `main` is now `0.82.39`.

---

# Epoch v0.82.40

## Highlights

- Routed updater status back through Epoch logging with sanitized updater messages so the commandline pane stops showing the raw `â™ªâ—™` newline artifacts.
- Kept the newer-source fallback intact so a confirmed update still rebuilds from source when no newer packaged runtime exists.
- Repackaged the runtime drop as a fresh `main.zip` release for updater testing.

## Notes

- Packaged runtime releases still ship as `main.zip`.
- `main` will move ahead again after this release so source-update checks keep a newer live target.

---

# Epoch v0.82.41

## Highlights

- Repaired the Windows updater handoff so packaged and source updates explicitly replace the runtime executable instead of silently leaving the old binary in place.
- Kept the updater logging path sanitized so updater status stays readable in the commandline pane while the handoff runs.

## Notes

- This packaged updater release is `0.82.41`.
- The next `main` bump will move ahead again after the release so source-update checks keep a newer live target.

---

# Epoch v0.82.42

## Highlights

- Repaired the Windows updater handoff batch by switching to proper batch variable quoting and explicit executable replacement.
- Added `epoch_update_handoff.log` in the runtime folder so failed packaged/source handoffs leave behind a concrete trail instead of silently stalling.

## Notes

- This packaged updater release is `0.82.42`.
- The next `main` bump will move ahead again after the release so source-update checks keep a newer live target.

---

# Epoch v0.82.43

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.42` release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.42`.
- The active source target on `main` is now `0.82.43`.

---

# Epoch v0.82.44

## Highlights

- Replaced the Windows updater `system()/cmd/start` flow with hidden process launches for `vcpkg`, `MSBuild`, and the handoff batches so source updates stop opening stray developer prompts.
- Added `epoch_source_update.log` next to `epoch_update_handoff.log` so failed source rebuilds and failed runtime replacement steps leave concrete diagnostics in the runtime folder.
- Fixed the updater's live source-version path to use `Engine/modules/aengine.version.ixx`, which keeps source update checks aligned with the actual repo version file.

## Notes

- This release is intended to repair both the source-update rebuild path and the final replacement handoff from a downloaded runtime.

---

# Epoch v0.82.45

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.44` updater repair release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.44`.
- The active source target on `main` is now `0.82.45`.

---

# Epoch v0.82.46

## Highlights

- Source updates continue to download the repository snapshot directly from `main`, not from GitHub releases.
- The source updater now does the restore/build sequence the safer way: restore manifest dependencies first, then retry the compile pass once before giving up.
- The runtime still leaves `epoch_source_update.log` and `epoch_update_handoff.log` behind for rebuild and handoff diagnostics.

## Notes

- This release is aimed specifically at the downloaded-runtime source update path, not the packaged binary update path.

---

# Epoch v0.82.47

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.46` updater repair release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.46`.
- The active source target on `main` is now `0.82.47`.

---

# Epoch v0.82.48

## Highlights

- Windows source updates now hand off to a detached worker process instead of trying to finish the download, restore, build, and replacement inline inside the running app.
- The worker keeps using the direct `main.zip` repository snapshot, restores manifest dependencies first, retries the MSBuild pass after restore, and waits long enough for the runtime handoff to complete cleanly.
- Source updates stay visible by default, and can be run silently on demand with `EPOCH_UPDATER_SILENT=1`.

## Notes

- Runtime-side diagnostics remain in `epoch_source_update.log` and `epoch_update_handoff.log`.
- The packaged updater asset is still published as `main.zip`.

---

# Epoch v0.82.49

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.48` detached-worker updater release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.48`.
- The active source target on `main` is now `0.82.49`.

---

# Epoch v0.82.50

## Highlights

- Fixed the Windows source updater handoff so the detached worker now resolves the real live executable path, waits for the runtime to exit, replaces it from the rebuilt `x64/Debug` output, and relaunches the updated app cleanly.
- Moved native `vcpkg` and `MSBuild` output into updater log files so restore/build activity no longer dumps raw junk into the live console.
- Replaced hardcoded machine-local sample-project dependency paths with repo-relative vcpkg include/library locations so source snapshot builds are portable across machines.

## Notes

- The packaged updater release is `0.82.50`.
- The active source target on `main` is now `0.82.50`.

---

# Epoch v0.82.51

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.50` updater release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.50`.
- The active source target on `main` is now `0.82.51`.

---

# Epoch v0.83.0 Complete Release Notes

## Summary

`v0.83.0` is the release-notes consolidation for the long updater and launcher
repair arc that ran from `v0.82.17` through the current tree. The headline
feature is the new updater shell: a dedicated bootstrap runtime that can ship
as a tiny Windows package, present one clear update action, hand control to the
console-driven updater, rebuild from source when `main` is newer than the last
packaged release, replace the local runtime, and relaunch into the updated
engine.

This document compresses the scattered point releases in that range into one
story that can be read as a single milestone instead of dozens of hotfix notes.
It also marks the first public binary release where the downloadable runtime is
the updater shell itself: a small `Release|x64` bootstrap build that can update
forward into the current full engine/editor.

## Headline Outcome

- Epoch now has a working release-to-source update path on Windows.
- Packaged releases can remain stable while `main` moves ahead for active
  development.
- The updater can detect when there is no newer packaged runtime and then fall
  through to the newer `main` source snapshot automatically.
- The updater shell gives new users a simple, obvious entry point into the
  engine/editor without exposing the full launcher surface first.

## What Changed Across v0.82.17 to Current

### 1. Release surfaces and version metadata were cleaned up

- The repo moved to a more consistent release-note structure, starting with the
  `v0.82.17` documentation cleanup and continuing through repeated version-surface
  refreshes after packaged releases.
- Active version metadata, README snapshots, docs, and release notes were kept
  aligned so the updater had a reliable newer source target to detect.
- Repo-relative paths replaced machine-local assumptions in the example project
  build surfaces, which was necessary for source snapshot rebuilds to work on
  other machines.

### 2. Packaged update detection was repaired

- The updater was moved onto the real GitHub release flow instead of stale
  executable URLs or mismatched source probes.
- Version detection was corrected so packaged runtime checks compare against the
  latest GitHub release, while source checks compare against
  `Engine/modules/aengine.version.ixx` on `main`.
- Temporary-file handling was hardened so sequential packaged/source checks do
  not trip over shared files or stale probe results.

### 3. Source fallback became a real rebuild path

- When `main` is newer than the latest packaged release, Epoch can now download
  the current source snapshot, restore dependencies with managed tooling, build
  `ConsoleApplication1`, and replace the local runtime from
  `<source-root>/x64/Release`.
- The Windows update flow was moved into detached worker/handoff stages so the
  rebuild and replacement can continue after the live runtime exits.
- `epoch_source_update.log` and `epoch_update_handoff.log` now leave behind a
  concrete trail for failed restore/build/replacement steps instead of silently
  stalling.
- Managed updater dependencies now include the ability to provision `vcpkg`
  and Git automatically for end users instead of assuming an existing dev
  machine setup.
- The updater also stages a patched `glad` overlay port so the source rebuild
  path survives current CMake policy behavior cleanly.

### 4. Handoff and replacement became reliable

- The updater now resolves the actual running executable path before replacement
  instead of relying on fragile startup-path assumptions.
- Hidden process launches replaced shell-heavy `system()` calls for restore,
  build, and handoff work.
- Runtime unlock waits and build retry behavior were strengthened so the final
  copy/restart step is less timing-sensitive.

### 5. Console and diagnostics readability improved

- Raw updater junk, duplicated console lines, and control-character artifacts
  were removed or redirected into log files.
- Command-line logging was collapsed into cleaner single-line summaries.
- Startup spam from backend confirmation, atlas/menu churn, and third-party
  initialization noise was reduced substantially.
- Missing-font failures in the GUI path were throttled so incomplete payloads do
  not spam the console every frame.

### 6. Launcher and editor presentation improved

- The launcher/editor split was clarified so project selection, game launching,
  and tool entry live in the launcher while the editor stays focused on editing.
- Visible version strings were restored in the launcher and editor about flows.
- GUI wrapping and description readability were improved so longer status text
  remains readable in the shell and launcher views.

### 7. The updater shell was introduced and stabilized

- Epoch now ships a dedicated updater-shell build with a minimal package shape:
  the runtime executable, required release DLLs, and font assets.
- The shell is software-only, intentionally avoiding the heavier mixed-backend
  path used by the full engine launcher.
- Pressing the update button no longer blocks inside the shell render loop. The
  shell window closes first, then the console-driven update work continues in
  process, which avoids the hang/close crash path seen in earlier iterations.
- The public binary can now boot straight into updater-shell mode by default,
  which makes the downloadable `main.zip` package a focused update/bootstrap
  surface instead of a full launcher drop.
- The live release flow has been demonstrated end to end:
  packaged release -> source fallback -> source rebuild -> runtime replacement
  -> automatic relaunch.

## Why The Updater Shell Matters

- It gives new users a cleaner first-run entry point than the full launcher.
- It demonstrates Epoch's self-update story with a minimal UI surface.
- It reduces the amount of runtime state that can interfere with binary
  replacement on Windows.
- It gives the project a stable bootstrap package that can point users at the
  current source even when there is no newer full release yet.

## Verification Snapshot

- Packaged updater releases now ship as `main.zip` assets containing only the
  updater-shell runtime, required DLLs, and font assets.
- Manual close of the updater shell exits cleanly.
- Live release builds have been tested in external sandboxes against newer
  source revisions on `main`.
- The public flow now succeeds with:
  packaged version detection,
  source fallback when appropriate,
  detached rebuild/handoff,
  binary replacement,
  and relaunch into the updated runtime.

## Known Follow-Up Areas

- The source rebuild path still shows a noisy first failed MSBuild attempt in
  some logs before the successful retry; the update still completes, but the log
  presentation can be cleaned further.
- The stripped-down updater shell is now stable enough to serve as the public
  bootstrap runtime, but the broader launcher/editor UX can still be made more
  modular and more polished.
- The long hotfix trail from `0.82.x` is now ready to roll into a cleaner
  `0.83.0` era with fewer repair releases and more feature-focused milestones.
