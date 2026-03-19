# Epoch v0.82.48

## Highlights

- Windows source updates now hand off to a detached worker process instead of trying to finish the download, restore, build, and replacement inline inside the running app.
- The worker keeps using the direct `main.zip` repository snapshot, restores manifest dependencies first, retries the MSBuild pass after restore, and waits long enough for the runtime handoff to complete cleanly.
- Source updates stay visible by default, and can be run silently on demand with `EPOCH_UPDATER_SILENT=1`.

## Notes

- Runtime-side diagnostics remain in `epoch_source_update.log` and `epoch_update_handoff.log`.
- The packaged updater asset is still published as `main.zip`.
