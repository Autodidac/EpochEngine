# Epoch Engine Analysis

## Snapshot

Epoch is now documented as a module-first engine with the active runtime living
under `Engine/modules/` and `Engine/src/`, while compatibility code has been
consolidated under `Engine/legacy/`.

Current public version: `v0.82.10`

## Architecture highlights

- **Multi-context runtime**: the shared context layer and multiplexer coordinate
  backend-owned windows, command queues, and render-thread work.
- **Backend stack**: OpenGL, SDL, Raylib, SFML, software, and noop/headless are
  all represented in the active engine tree; Vulkan remains experimental.
- **Atlas-driven UI/rendering**: atlas upload, sprite submission, and GUI layout
  are shared concerns across the active render paths.
- **Task graph + scripting**: reload and background work are funneled through
  task scheduling rather than ad hoc threaded entry points.
- **Compatibility archive**: older header/source snapshots are preserved in
  `Engine/legacy/` for migration and archaeology, not as the preferred
  implementation surface.

## Current strengths

- Strong module-first organization for the active engine.
- Broad backend coverage for desktop experimentation and tooling.
- Good separation between active code and archived compatibility material after
  moving the legacy tree under `Engine/`.
- Startup and shutdown behavior are getting more disciplined as backend-local
  diagnostics and hot-path logging are trimmed back out of the render loop.
- SDL and Software now share the same editor preview-grid geometry/palette path
  as the GPU editor previews, which tightens backend parity for scene-view
  smoke tests.
- Parent-window shutdown now behaves more like a real engine host lifecycle:
  docked children are marked for close and the session exits instead of leaving
  a dead console/process behind.
- The parented docking layout now stays intact during shutdown instead of
  undocking backend panes as the host window closes.
- Live pane management is back in a better place too: undocked windows keep the
  information they need to redock cleanly, and only still-docked panes are
  considered part of the parent grid layout.
- Manual pane management is now usable again without modifier keys: a dedicated
  drag strip provides left-drag docking while leaving the rest of the pane free
  for normal backend/editor interaction.

## Current cautions

- Vulkan is present but should still be treated as a migration/integration path,
  not the default renderer, even though the editor palette, editor grid preview,
  and GUI presentation have been brought closer to the OpenGL baseline.
- A few minor archival and compatibility surfaces still exist and should be
  changed carefully.
- Backend fixes are usually safest when applied locally to the affected backend
  instead of globally in the multiplexer.
- Raylib parent/docking work should continue to respect GLFW/raylib ownership of
  the native GL context instead of swapping in fresh Win32 DC handles after
  initialization.

## Recommended priorities

1. Keep renderer/backend smoke coverage improving, especially around startup,
   resize, docking, and shutdown behavior.
2. Continue reducing duplicated compatibility naming in public surfaces while
   preserving compile compatibility where needed.
3. Promote only the active, tested legacy pieces into the main engine surface;
   leave pure archive material under `Engine/legacy/`.
