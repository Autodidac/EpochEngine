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
| `direct3d.*` / `d3d12.*` | Planned | Windows-native renderer replacement track for the old software-as-product fallback role. Not active until device/context/swapchain/shader/resource support is implemented and validated. |
| `noop.context` | Minimal | Headless placeholder. |
| `vulkan.*` | Experimental | Under active migration, not a stable default backend. |
| `opengl.renderer` | Review candidate | Looks more archival than central; keep under review. |
| Retired legacy context stack | Retired | Historical snapshots have been removed; keep migration work in active modules and documented feature maps instead. |

## Practical guidance

- Prefer module-backed active context surfaces under `Engine/modules/` and
  `Engine/src/`.
- Use the active modules plus `legacy_feature_map.md` for migration help, not
  deleted archive snapshots.
- On Windows, launch multi-context smoke runs from the asset-bearing output
  directory so docked backend panes do not drift away from the editor host's
  runtime assets.
- Keep multicontext fixes backend-specific until the common shell is proven:
  Raylib, SDL3, SFML3, OpenGL, Vulkan, and the software fallback each have
  different context ownership and shutdown rules.
- Current `v0.84.34` evidence: standalone OpenGL editor smoke exits cleanly, SDL
  and software scene visibility have been restored in local smoke captures, and
  a parented multicontext maximize/restore smoke completed without crashing.
  Raylib direct-child resize now uses immediate grid sizing while SDL/SFML proxy
  hosts keep asynchronous moves. Treat clean smoke shutdown and operator
  resize-convergence confirmation as open gates before promoting releases.
- Treat Vulkan and a few minor archival helpers as incomplete until their paths
  are explicitly finished and tested.
- Treat Direct3D/D3D12 as planned Windows-native renderer work, not as an active
  backend, until a build can create the device/swapchain, clear/present, own
  resources, and pass editor screenshot smoke.
