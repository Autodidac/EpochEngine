# Epoch v0.82.19

## Highlights

- Fixed the self-update flow so it now targets the currently running executable
  instead of the old hardcoded `updater.exe` replacement path.
- Added explicit failure reporting for update handoff/install failures so the
  app no longer appears to update successfully when the download or replacement
  step did not complete.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Re-ran the update code path against the rebuilt app
- Refreshed the active release/version surfaces to `v0.82.19`
