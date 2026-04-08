# Runtime Operations

This guide is intentionally high level. It documents the current expected
workflow instead of preserving the older citation-heavy snapshot docs.

## Startup model

- The runtime is expected to enter through the normal engine bootstrap so
  scripting, updater checks, and backend initialization share the same path.
- Example desktop wiring lives under `Engine/examples/ConsoleApplication1/`.
- Multi-context behavior depends heavily on the configuration macros documented
  in `aengineconfig_flags.md`.

## Scripting and reload workflow

- Script sources are expected to live in the script source tree used by the
  runtime/example launcher.
- Reload flow is task-graph driven and intended to surface compile/load/execute
  failures as explicit diagnostics instead of silent stalls.
- Treat hot reload as an active development feature, not a release guarantee:
  it is powerful, but it still benefits from targeted smoke coverage.

## Logging

- The engine logger is the preferred way to tag subsystem output.
- Keep backend-specific noise behind subsystem names so multi-context runs stay readable.
- Repeated identical startup/error lines should collapse instead of flooding the
  console; one tagged line per repeated condition is the goal, with backend
  detail still carried in the subsystem tag and source location.
- For release/version reporting, prefer the shared version helpers instead of
  duplicating hard-coded version strings.

## Backend operations

- OpenGL is the main GPU renderer today.
- SDL, Raylib, and SFML are active context integrations with different levels of stability.
- Software rendering remains valuable as a fallback and debugging path.
- Vulkan should still be treated as experimental unless you are actively
  finishing that path.

## Editor viewport controls

- Right-drag orbits the editor preview camera.
- Left-drag pans the preview focus across the grid plane.
- Mouse wheel zooms the editor preview camera in and out.
- The preview should show a visible look-hit marker where the center camera ray
  intersects the grid, which makes it easier to read the active focus spot
  during multi-context runs without a floating false-collision marker.

## Editor runtime surfaces

- The editor should expose real scene/entity actions from the live UI instead of
  placeholder logging buttons.
- The current asset/outliner path is expected to support adding meshes, lights,
  spawns, duplicating the current selection, and deleting the current
  selection.
- The AI chat panel depends on the same runtime input plumbing as the rest of
  the GUI. On Windows docked contexts, wheel/key/text messages must be routed
  from the child HWNDs into the GUI event queue or the chat/input surfaces will
  look present but behave dead.

## Project-system direction

- Editor-side `Run Game` is intended to become scene/game testing from the
  current project, not a permanent launcher for built-in sample games.
- Built-in sample games should gradually move behind project scripts or example
  project templates instead of remaining hard-wired editor destinations.
- The longer-term target is a Unity/Unreal-style project shell generated from a
  duplicated engine source/layout, so game projects can own scripts, assets,
  and runtime behavior without pretending they are editor internals.

## Troubleshooting checklist

- Verify the expected context/render macros are enabled.
- Launch Windows smoke tests from `x64/Debug/` or `x64/Release/` so the editor
  host and docked backend panes share the colocated runtime assets.
- Prefer engine-owned capture output over ad hoc desktop grabs when validating
  backend/editor rendering. The supported flow is documented in
  `smoke_capture_automation.md`.
- Keep Windows `.rc`, icon, and resource headers under `Engine/resource/` so
  MSVC and CMake stay aligned on the same resource root.
- Use fresh build directories when changing compilers, module scanning flags, or
  backend combinations.
- If reload behavior becomes inconsistent, check script diagnostics first, then
  task-graph saturation, then backend context health.
- When investigating backend issues, prefer backend-local fixes over broad
  multiplexer behavior changes unless you have proven the shared layer is at fault.

## Current editor/runtime seam

- `aeditor.cpp` still owns the live editor shell/UI state for projects, AI chat,
  and entity widgets.
- `aeditor.scene.cpp` already contains a separate editor-scene and command-bus
  implementation.
- The next honest editor step is to merge the live editor shell onto that scene
  layer instead of keeping a parallel local entity list in `aeditor.cpp`.
