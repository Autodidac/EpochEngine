# Epoch v0.82.41

## Highlights

- Repaired the Windows updater handoff so packaged and source updates explicitly replace the runtime executable instead of silently leaving the old binary in place.
- Kept the updater logging path sanitized so updater status stays readable in the commandline pane while the handoff runs.

## Notes

- This packaged updater release is `0.82.41`.
- The next `main` bump will move ahead again after the release so source-update checks keep a newer live target.
