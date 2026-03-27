# Context Audit

This is the current high-level status of the context and renderer stack.

| Surface | Status | Notes |
| --- | --- | --- |
| `aengine.core.context` | Active | Shared context abstraction used everywhere. |
| `aengine.context.*` multiplexer/window/control/type | Active | Core windowing and command-routing layer. |
| `acontext.opengl.*` | Active | Primary GPU renderer path. |
| `acontext.sdl.*` | Active | Important desktop backend, still worth continued stabilization. |
| `acontext.raylib.*` | Active | Active and feature-rich, especially for docked-window workflows. |
| `acontext.sfml.*` | Active | Supported, but generally more delicate due to GL/context behavior. |
| `acontext.softrenderer.*` | Active | Fallback/debug path. |
| `acontext.noop.context` | Minimal | Headless placeholder. |
| `acontext.vulkan.*` | Experimental | Under active migration, not a stable default backend. |
| `acontext.opengl.renderer.ixx` | Review candidate | Looks more archival than central; keep under review. |
| Retired legacy context stack | Retired | Historical snapshots have been removed; keep migration work in active modules and documented feature maps instead. |

## Practical guidance

- Prefer module-backed active context surfaces under `Engine/modules/` and
  `Engine/src/`.
- Use the active modules plus `legacy_feature_map.md` for migration help, not deleted archive snapshots.
- Treat Vulkan and a few minor archival helpers as incomplete until their paths
  are explicitly finished and tested.
