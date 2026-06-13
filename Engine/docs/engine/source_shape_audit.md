# Source Shape Audit

This document is the current source-organization checkpoint for Epoch's C++23
engine line. It exists so future cleanup can be done in small verified batches
instead of broad renames that break CMake, MSBuild filters, module imports, or
working runtime behavior.

Epoch is treated as a professional production codebase even while features are
still landing in phases. A first-pass implementation must be real behavior with
clear ownership, honest limits, build/test proof, and a documented next gate. It
must not be placeholder code, dead scaffolding, fake UI, or a speculative
rewrite disguised as progress.

## Current truth

- CMake and the Visual Studio solution both remain active build surfaces.
  Source moves must update filesystem paths, CMake source lists/module file
  sets, `.vcxproj`, `.vcxitems`, and `.filters` together.
- Public configuration still has multiple compatibility-facing entry points:
  `Engine/include/engine.config.hpp`, `Engine/include/epoch.config.hpp`,
  legacy C headers, and bridge headers. Treat these as audited public surfaces,
  not random duplicates to delete in one pass.
- Backend bridge headers and source slices are intentionally mixed while the
  renderer backends converge. OpenGL and DirectX now have split implementation
  files; Vulkan/Raylib/SDL/SFML still carry older module/source shapes that must
  be normalized one backend at a time.
- Perf/time/platform capability policy is live but still being integrated into
  the central spine. `perf.tier`, `perf.select`, `platform.capabilities`, and
  runtime support-tier docs should drive backend feature selection instead of
  scattered ad hoc flags.
- Some names remain compatibility-stable even if they are not final public
  naming. Do not rename module names, project targets, or generated project ids
  without a build-tested migration.

## Cleanup gates

- Every rename must compile through MSBuild Debug x64 and the active CMake
  preset before being marked done.
- Every new source/config/header surface must have a named owner and a reason to
  exist. Compatibility shims are acceptable, but they must stay thin and point to
  the canonical implementation.
- Header cleanup should prefer one canonical owner and thin compatibility
  shims. Deleting a shim is only allowed after all includes/imports are proven
  gone.
- Shared engine/editor sources must use the established platform/config header
  path instead of hand-rolling local Win32 macro blocks. If a translation unit
  needs Win32 APIs, include the audited wrapper such as `framework.hpp` or the
  backend-local platform owner; do not scatter fresh `WIN32_LEAN_AND_MEAN`,
  `NOMINMAX`, and `<windows.h>` blocks through unrelated code.
- New backend files should follow the current split shape: context bridge,
  state/lifetime, device/setup, preview rendering, GUI replay, and upload/capture
  bridges where applicable.
- Cleanup passes should increase implementation clarity, not merely shuffle
  metadata. A useful pass leaves moved or split code in an owned folder,
  repaired includes/imports, synchronized CMake/MSVC/filter entries, and at
  least one focused build proof. Use helper agents for include/project inventory
  only when that lets the main pass land more source, not as a substitute for
  the source move itself.
- Source organization should support the editor domains directly: scene/game,
  assets, project/build, systems/perf, AI sandbox, package manager, scripting,
  and backend/runtime.
- Voxel/pathing/tracing contracts and deterministic Forest Factory descriptors
  are core engine primitives. Heavy planetary terrain, multi-terrain authoring,
  FFT ocean, external prototype demos, and game-specific world stacks stay
  package-managed until their API boundary, provenance, build/test path, and
  project opt-in behavior are proven.
- Generated project output under repo-root `Projects/` remains evidence unless
  a template or fixture is explicitly promoted into tracked source.

## GUI library shape

Epoch GUI is an engine-internal library layer, not an editor-only pile of
controls. The public surface starts in `Engine/modules/engine.gui.ixx`; the
current implementation lives in `Engine/src/engine.gui.cpp`; editor workspaces
such as scene, assets, project, systems, and AI sandbox consume that API instead
of drawing ad hoc sprite controls directly.

The intended split is:

- primitive widgets: labels, buttons, tab bars, select boxes, text inputs,
  scroll areas, image/runtime-surface views, and future checkboxes/sliders
- layout and docking: windows, splitters, scroll extents, modal/scrim layers,
  context menus, focus routing, and future popout hosts
- theme and rendering: palette ownership, atlas/runtime-surface use, glyph
  clipping, deferred GUI replay, and backend-specific present safety
- editor composition: `editor.cpp` chooses which workbench/domain is visible,
  but does not own generic widget behavior

New GUI features should first become reusable primitives in the GUI layer, then
be wired into the editor. Console Dock content remains a compact evidence/log
strip; it should not become the home for central project, systems, AI, package,
or scripting controls. The detailed contract lives in
`Engine/docs/engine/gui_library_architecture.md`.

The GUI split now has two layers. Portable layout/state controllers belong in
`Engine/include/epoch/gui`, `Engine/src/gui`, and `Engine/lib/EpochGui`.
Rendering, input, font/theme, clipping, and deferred/top-layer replay stay in
`engine.gui`. Native detached windows and context route lifecycles stay in the
context host/session code. Do not hide native host behavior inside `EpochGui`,
and do not hide generic widget behavior inside `editor.cpp`.

## Open organization risks

- Legacy broad module names such as `framework`, `applicationmodule`, and
  `epochengine` still exist and should be wrapped or split only when touched by
  a verified feature pass.
- The renderer feature matrix and perf tier policy need one shared capability
  table so GUI/settings surfaces can present honest backend support instead of
  backend-local guesses.
- The scripting/editor text path needs a real code/text editor view; until then
  script files should stay discoverable through project/assets surfaces without
  moving core mini-runtime code out of the engine.
- The package-manager surface should start with local runtime-mini packages and
  later support downloadable source packages through the updater-style human
  build/run gate.
