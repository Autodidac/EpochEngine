# Epoch v0.82.44

## Highlights

- Replaced the Windows updater `system()/cmd/start` flow with hidden process launches for `vcpkg`, `MSBuild`, and the handoff batches so source updates stop opening stray developer prompts.
- Added `epoch_source_update.log` next to `epoch_update_handoff.log` so failed source rebuilds and failed runtime replacement steps leave concrete diagnostics in the runtime folder.
- Fixed the updater's live source-version path to use `Engine/modules/aengine.version.ixx`, which keeps source update checks aligned with the actual repo version file.

## Notes

- This release is intended to repair both the source-update rebuild path and the final replacement handoff from a downloaded runtime.
