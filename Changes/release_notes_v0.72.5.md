# AlmondShell v0.72.5 Release Notes

## Summary
- Completed the module migration for the context/back-end stack, covering the multiplexer, window/render plumbing, and backend bindings for OpenGL, SDL3, Raylib, and the software renderer with `.ixx` partitions that expose importable surfaces.
- Documented the expanded module coverage and new import targets (`aengine.context`, `aengine.context.window`, `aengine.context.render`, plus backend-specific partitions) so downstream consumers can target contexts/backends directly from modules.

## Upgrade Notes
- Clear or regenerate your build tree to refresh BMI outputs and ensure the new context/back-end partitions are scanned during configuration.
- Update any downstream integrations to prefer the module imports over legacy headers when targeting contexts or backend renderers.
