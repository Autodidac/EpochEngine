# Epoch v0.82.46

## Highlights

- Source updates continue to download the repository snapshot directly from `main`, not from GitHub releases.
- The source updater now does the restore/build sequence the safer way: restore manifest dependencies first, then retry the compile pass once before giving up.
- The runtime still leaves `epoch_source_update.log` and `epoch_update_handoff.log` behind for rebuild and handoff diagnostics.

## Notes

- This release is aimed specifically at the downloaded-runtime source update path, not the packaged binary update path.
