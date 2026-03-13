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
| `Engine/legacy/` context stack | Archived | Historical compatibility/reference code, not the preferred active path. |

## Practical guidance

- Prefer module-backed active context surfaces under `Engine/modules/` and
  `Engine/src/`.
- Use `Engine/legacy/` for migration help or archaeology, not for new runtime work.
- Treat Vulkan and a few minor archival helpers as incomplete until their paths
  are explicitly finished and tested.