# Epoch v0.82.42

## Highlights

- Repaired the Windows updater handoff batch by switching to proper batch variable quoting and explicit executable replacement.
- Added `epoch_update_handoff.log` in the runtime folder so failed packaged/source handoffs leave behind a concrete trail instead of silently stalling.

## Notes

- This packaged updater release is `0.82.42`.
- The next `main` bump will move ahead again after the release so source-update checks keep a newer live target.
