# Epoch v0.82.6

## Summary

v0.82.6 is a backend parity and parented-startup stability pass. The editor
scene preview is now shared across SDL and Software instead of falling back to
the placeholder viewport card, and the parented Raylib path keeps the original
WGL context/DC pairing captured during initialization.

## Backend updates

- Added the shared `epoch.render.preview_grid` scene-view rendering path to the
  SDL backend.
- Added the shared `epoch.render.preview_grid` scene-view rendering path to the
  Software backend.
- Updated the editor GUI viewport logic so those backends now own the scene
  viewport instead of drawing the old placeholder card over it.
- Preserved the Raylib GL context with its original captured device context
  instead of replacing it with a fresh `GetDC(...)` handle after startup.

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`.
- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched from `x64/Debug` and verified Software-only and SDL-only editor runs
  visually show the shared preview grid.
- Re-ran the parented mixed-context launch and confirmed the old Raylib
  `Failed to make raylib context current during process` shutdown line no longer
  appeared in the latest run logs.
