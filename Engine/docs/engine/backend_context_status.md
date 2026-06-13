# Backend Context Status

This is the current high-level status of the context and renderer stack.

| Surface | Status | Notes |
| --- | --- | --- |
| `core.context` | Active | Shared context abstraction used everywhere. |
| `context.*` multiplexer/window/control/type/platform_utils | Active | Core windowing and command-routing layer. |
| `opengl.*` | Active | Primary GPU renderer path. |
| `sdl.*` | Active | Important desktop backend; Windows dock-host rendering is active and still being smoke-tested against GUI regressions. |
| `raylib.*` | Active | Active and feature-rich, especially for docked-window workflows. |
| `sfml.*` | Active | Supported, but still more delicate due to GL/context behavior and dock-host activation order. |
| `software.*` | Fallback | Safe-launch, debug/error-message GUI, capture diagnostics, and headless validation path. Do not treat it as the long-term Windows production renderer once Direct3D is promoted. |
| `directx.*` / D3D11 | Active first pass | Windows-native renderer slice with device/swapchain/render-target ownership, basic shader preview rendering, GUI replay, scene-preview gating, and v0.84.35 multicontext screenshot proof. |
| D3D12 | Planned | Future explicit Windows renderer track. Do not claim D3D12 support until a separate device/context/swapchain/shader/resource path is implemented and validated. |
| `noop.context` | Minimal | Headless placeholder. |
| `vulkan.*` | Experimental | Under active migration, not a stable default backend. |
| `opengl.renderer` | Review candidate | Looks more archival than central; keep under review. |
| Retired legacy context stack | Retired | Historical snapshots have been removed; keep migration work in active modules and documented feature maps instead. |

## Context Selection And Handoff Status

| Path | Status | Notes |
| --- | --- | --- |
| Editor toolbar combobox | Active first pass | Reflects the current backend and emits explicit `SwitchContext` requests. Explicit unavailable backends must fail closed instead of falling back to another backend. |
| Live backend focus/restore | Active first pass | The session loop can focus an already-live context of the selected backend and restore the captured editor snapshot. Snapshot capture/restore failure must log visibly. |
| New editor context request | Windows first pass | Windows can post a detached context request for a new editor context and later restore state when the target enters the session loop. Posted request, created window, session entry, snapshot restore, and present are separate evidence states. |
| Linux/WSL create-from-combobox | Unsupported today | The Linux detached-context request path currently returns false; WSL proof remains single-context OpenGL unless explicitly changed. |
| Mobile/console/headless create-from-combobox | Excluded by product policy | These targets may use portable `EpochGui` controls but should hide or reject native popout/context-create routes unless a product host implements them. |

## Practical guidance

- Prefer module-backed active context surfaces under `Engine/modules/` and
  `Engine/src/`.
- Use the active modules plus `legacy_feature_map.md` for migration help, not
  deleted archive snapshots.
- On Windows, launch multi-context smoke runs from the asset-bearing output
  directory so docked backend panes do not drift away from the editor host's
  runtime assets.
- Keep multicontext fixes backend-specific until the common shell is proven:
  Raylib, SDL3, SFML3, OpenGL, Vulkan, DirectX, and the software fallback each have
  different context ownership and shutdown rules.
- Name the source slice before editing backend context code. The common slices
  are host lifecycle, context bridge, device/resource allocation, preview
  rendering, GUI replay, upload/capture, capability reporting, and build
  metadata. Parallelize only across independent backend slices with disjoint
  files; keep the main integration pass responsible for final build proof.
- Do not use multicontext diagnostic grids to choose the editor's normal runtime
  backend. Background context scoring should measure comparable single-context
  editor sessions only; multicontext panes are useful for smoke/comparison but
  distort performance, memory, input, and upload contention.
- Current `v0.84.35` evidence: Windows six-context proof now uses Raylib, SDL,
  SFML, Vulkan, OpenGL, and DirectX. DirectX owns a real D3D11
  device/swapchain/render target, renders editor preview markers, replays the
  GUI batch, skips scene geometry when the editor has not published a valid
  scene viewport, clips preview geometry as whole primitives to avoid
  angle-dependent floating line artifacts, and passed README screenshot startup
  proof. Software remains fallback/debug/headless validation rather than the
  normal Windows product pane.
- DirectX is intentionally Windows-only. CMake/MSBuild must keep it disabled on
  Linux and WSL; Linux parity means the repo still builds and runs the
  non-DirectX lanes, not that D3D11 is available there.
- DirectX now keeps the public `directx.context` module interface while splitting
  real behavior across implementation units: `directx.context.cpp` for the
  exported bridge, `directx.state.cpp` for lifetime/resize/render-target state,
  `directx.device.cpp` for D3D11 device and shader setup, `directx.preview.cpp`
  for editor preview geometry, and `directx.gui.cpp` for GUI atlas/sprite replay.
- Shared editor preview projection now lives in `render.preview_grid`; backend
  preview renderers should call that spine for perspective vs Canvas2D
  orthographic selection instead of keeping backend-local projection branches.
- DirectX still needs the next real renderer-resource step: depth/stencil,
  resource lifetime, material/pipeline ownership, and deeper engine-facing
  renderer-resource APIs should move together instead of papering over the
  current first-pass renderer surface.
- Treat Vulkan and a few minor archival helpers as incomplete until their paths
  are explicitly finished and tested.
- Treat D3D12 as planned Windows-native renderer work, not as an active backend,
  until a build can create the device/swapchain, clear/present, own resources,
  and pass editor screenshot smoke.
