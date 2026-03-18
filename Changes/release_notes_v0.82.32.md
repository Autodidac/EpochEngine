# Epoch v0.82.32 Release Notes

## Highlights

- Fixed updater version checks to use unique temp files, eliminating the
  `remote_version.txt` file-in-use collision during packaged plus source probe
  runs.
- Kept source-snapshot update detection distinct from packaged-release
  detection, so the updater can report the two paths clearly.
- Cleaned the editor update confirmation modal so its content no longer bleeds
  into the title bar.
