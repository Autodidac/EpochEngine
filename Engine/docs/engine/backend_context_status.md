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
| `software.*` | Active | Fallback/debug path. |
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
- Treat Vulkan and a few minor archival helpers as incomplete until their paths
  are explicitly finished and tested.
