# Epoch v0.82.38

## Highlights

- Fixed the updater fallback so a confirmed update now rebuilds from source when `main` is newer and no newer packaged runtime exists.
- Removed the duplicate source-version probe so the updater stops printing the same source comparison twice before a source rebuild.
- Simplified updater status reporting to one direct console stream during update work, which keeps source/package update progress visible without the earlier doubled logic.

## Notes

- Packaged runtime releases still ship as `main.zip`.
- `main` will continue to move ahead after this release so source-update checks can detect newer source snapshots between packaged drops.
