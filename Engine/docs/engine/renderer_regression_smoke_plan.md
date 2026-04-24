# Renderer Regression Smoke Plan

This plan defines the minimum smoke coverage for Epoch's active render backends.

## Core scenarios

| Backend | Minimum smoke checks |
| --- | --- |
| OpenGL | Startup, resize, atlas upload, present, shutdown |
| SDL | Startup, resize, texture upload, present-failure handling, shutdown |
| Raylib | Startup, dock/undock, logical vs framebuffer scaling, shutdown |
| SFML | Startup, GL context ownership, atlas upload, shutdown |
| Software | Resize, atlas rebuild, present path |
| Vulkan | Build-only or isolated validation until runtime support is finished |

## Pass criteria

1. The backend starts without context-creation failures.
2. Resize events converge on the correct drawable size.
3. Atlas uploads succeed without duplicated or stale versions.
4. Present/shutdown paths fail quietly and locally when a backend loses its device/context.
5. No backend fix should break the other active backends.

## Recommended cadence

- Run smoke coverage after backend startup/shutdown changes.
- Run smoke coverage after atlas or menu overlay changes.
- Treat Vulkan separately from the stable desktop backends until it graduates from experimental status.
- Hosted CI may use `epoch_ci_headless` as a no-window canary, but that target is not renderer proof. It confirms the public script-host surface and repo path probes while full renderer confidence still comes from Windows full-stack builds and local runtime smoke.
