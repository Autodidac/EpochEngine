# Epoch v0.82.5

## Summary

v0.82.5 focuses on startup/runtime hygiene and public-facing project guidance.
The engine no longer spends normal launches appending ad hoc Vulkan frame traces
or per-frame SFML path messages, and the shared renderer slow-frame diagnostics
now warn in a controlled way instead of flooding logs during initialization.

## Runtime updates

- Disabled the unconditional `vulkan_runtime_diag.txt` write path for standard runs.
- Added startup grace + throttling to renderer slow-frame warnings.
- Removed the hot-loop SFML render-path info log from the frame path.

## Documentation updates

- Rewrote the top-level README around the active Epoch runtime tree.
- Expanded build guidance for Visual Studio, MSBuild, CMake presets, VS Code,
  shell scripts, and binary-folder launches.
- Updated the configuration and analysis docs to reflect the quieter runtime
  diagnostics behavior and the `v0.82.5` snapshot.
