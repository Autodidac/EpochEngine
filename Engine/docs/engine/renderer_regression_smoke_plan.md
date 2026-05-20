# Renderer Regression Smoke Plan

This plan defines the minimum smoke coverage for Epoch's active render backends.

## Core scenarios

| Backend | Minimum smoke checks |
| --- | --- |
| OpenGL | Startup, resize, atlas upload, present, shutdown |
| SDL | Startup, resize, texture upload, scene visibility, present-failure handling, shutdown |
| Raylib | Startup, dock/undock, logical vs framebuffer scaling, shutdown |
| SFML | Startup, GL context ownership, atlas upload, shutdown |
| Software | Safe-launch/debug GUI fallback, error-message visibility, capture/headless validation |
| Vulkan | Build-only or isolated validation until runtime support is finished |
| DirectX/D3D11 | Startup, resize, D3D11 device/swapchain/render-target ownership, GUI replay, scene visibility, present/shutdown |
| D3D12 | Planned Windows-native renderer track; no runtime smoke until device/context/swapchain/shader/resource support is promoted |

## Pass criteria

1. The backend starts without context-creation failures.
2. Resize events converge on the correct drawable size.
3. Atlas uploads succeed without duplicated or stale versions.
4. Present/shutdown paths fail quietly and locally when a backend loses its device/context.
5. No backend fix should break the other active backends.
6. Bounded smoke runs must exit without manual process termination after
   writing requested captures.

## Recommended cadence

- Run smoke coverage after backend startup/shutdown changes.
- Run smoke coverage after atlas or menu overlay changes.
- Treat Vulkan separately from the stable desktop backends until it graduates from experimental status.
- Treat DirectX/D3D11 as an active first-pass Windows smoke lane after v0.84.35.
  Treat D3D12 as planned until it is intentionally promoted beyond scaffolding.
- Treat Software as fallback/debug proof. It should continue to report clear
  safe-launch diagnostics, but it should not block Direct3D replacing it as the
  normal Windows product renderer once D3D is real.
- Hosted CI may use `epoch_ci_headless` as a no-window canary, but that target is not renderer proof. It confirms the public script-host surface and repo path probes while full renderer confidence still comes from Windows full-stack builds and local runtime smoke.

## Current Open Smoke Issue

- `v0.84.35`: `ConsoleApplication1` Debug/Release builds, HeadlessCI, CMake/MSVC
  configure/build/ctest, and WSL Clang configure/build/ctest passed. README
  proof now shows Raylib, SDL, SFML, Vulkan, OpenGL, and DirectX. Keep clean
  smoke shutdown, DirectX GUI flicker eye tests, and backend-by-backend maximize
  checks in the repro matrix before cutting wider releases.
