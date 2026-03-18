# Epoch Release Notes Archive

This file consolidates older versioned release-note markdowns. New releases continue as individual files.

---

# AlmondShell v0.62.1 Release Notes

## Highlights
- Fixes a cosmetic regression in the updater where `curl` error output was glued to AlmondShell's diagnostics without a newline.
- Keeps the updater's failure handling defensive by trimming empty artifacts and surfacing the failure reason immediately.
- Synchronises the bundled version metadata and changelog so integrators can verify the expected runtime snapshot.

## Known Issues
- Automated renderer regression scenes remain under development; see `docs/renderer_regression_plan.md` for the intended coverage.
- Crash reporting hooks have not landed yet, so crashes must be reproduced locally with a debugger attached.

## Roadmap Alignment
This release ticks off the Phase 5 documentation task to "Draft release notes summarising new features, known issues, and roadmap alignment" from `Changes/roadmap.txt`.

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

