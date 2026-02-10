# AlmondShell v0.71.0 Release Notes

## Summary
- Raised the engine toolchain to the C++23/module baseline and documented the migration so downstream packagers can rebuild with BMI-aware compilers and generators.
- Refreshed the README, configuration guide, and engine analysis to surface the v0.71.0 snapshot alongside the module-conversion guidance.

## Upgrade Notes
- Start from a clean build tree and enable module scanning (`CMAKE_CXX_SCAN_FOR_MODULES`/`CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP`) when configuring CMake to pick up the new baseline.
- Ensure your launcher manifests and packaged metadata reference v0.71.0 so diagnostics and update checks advertise the correct release.
