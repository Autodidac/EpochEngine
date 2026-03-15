# Epoch v0.82.12

## Summary

v0.82.12 is a launcher/editor workflow pass. Projects and playable game
entries now live in the launcher, while the editor presents a more traditional
desktop-style top menu with scene preview controls and confirmation-gated
update actions.

## Runtime updates

- Split the old mixed command strip so the launcher owns project selection,
  game launching, and tool entry points.
- Reworked the editor shell into `File`, `Edit`, `Scene`, `Command`, and
  `Help` menus.
- Added editor scene preview switching between `Editor` and `None`.
- Added an in-editor confirmation modal before running the updater because the
  action can replace binaries and restart the current session.
- Kept launcher-driven project selection wired into editor mode and game
  selection wired into scene/runtime mode.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched the real binary from `x64/Debug`.
- Verified the new source flow compiles cleanly and the runtime stays healthy
  during launch with the refactored launcher/editor routing.
