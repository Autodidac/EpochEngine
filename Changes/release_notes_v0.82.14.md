# Epoch v0.82.14

## Summary

v0.82.14 is a public-facing documentation pass. It updates the README and
active version surfaces so Epoch is described in terms that match the actual
engine: internal bootstrap, multi-context runtime, launcher/editor workflow,
cross-platform build freedom, module-first architecture, and built-in tooling
systems.

## Documentation updates

- Rewrote the top README description so it presents Epoch as a world-class,
  AI-enabled, C++23 modules-first engine instead of a thin runtime summary.
- Expanded the "What Epoch provides" section to call out:
  - internalized engine bootstrap and entry flexibility
  - multi-context, multi-backend orchestration
  - launcher/editor workflow
  - atlas-driven GUI and sprite systems
  - scripting, file-watch, and task-graph-backed iteration
  - telemetry, diagnostics, and updater plumbing
  - cross-platform, editor-agnostic build freedom
  - module-first public engine surface
- Added direct markdown links from the README into the active build/docs/project
  surfaces so the repository front page works as a real navigation hub.

## Verification

- Pulled the latest `main` before editing.
- Checked the active docs/build surfaces referenced by the README against the
  current repo layout.
- Refreshed the active version metadata to `v0.82.14`.
