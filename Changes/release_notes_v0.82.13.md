# Epoch v0.82.13

## Summary

v0.82.13 is a mixed-backend visual consistency pass. It aligns the active
backend base color with the darker Vulkan look, moves SFML ahead of Vulkan in
the default parented dock order, and fixes the SFML shared scene preview so it
stays clipped to the intended scene viewport.

## Runtime updates

- Changed the active backend base clear color table so the main backends share
  the same darker Vulkan-style baseline.
- Moved SFML ahead of Vulkan in the default parented grid ordering.
- Reworked SFML shared scene preview drawing to use a clipped sub-view so the
  preview no longer leaks into GUI space when the window is being manipulated.
- Kept the shared preview-grid geometry/path intact across the active backends.

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`.
- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched the real binary from `x64/Debug`.
- Verified the live parented top row now places Raylib, SDL, and SFML ahead of
  Vulkan.
- Checked the latest runtime logs for fresh mixed-backend bring-up errors after
  the change.
