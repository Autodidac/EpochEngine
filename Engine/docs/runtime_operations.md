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
  during multi-context runs.

## Troubleshooting checklist

- Verify the expected context/render macros are enabled.
- Launch Windows smoke tests from `x64/Debug/` or `x64/Release/` so the editor
  host and docked backend panes share the colocated runtime assets.
- Keep Windows `.rc`, icon, and resource headers under `Engine/resource/` so
  MSVC and CMake stay aligned on the same resource root.
- Use fresh build directories when changing compilers, module scanning flags, or
  backend combinations.
- If reload behavior becomes inconsistent, check script diagnostics first, then
  task-graph saturation, then backend context health.
- When investigating backend issues, prefer backend-local fixes over broad
  multiplexer behavior changes unless you have proven the shared layer is at fault.
