# Epoch v0.82.10

## Summary

v0.82.10 is a manual docking interaction repair. It removes the temporary
modifier-key requirement and restores real left-drag pane docking through a
small pane drag strip, while keeping the parent-host shutdown and redock fixes
from the previous hotfixes.

## Runtime updates

- Replaced the temporary `Alt+Left Mouse` docking gesture with a dedicated drag
  strip at the top of each pane.
- Preserved the original dock-parent tracking and docked-only grid layout so
  panes can still undock and redock cleanly after becoming top-level windows.
- Kept the verified parent-host shutdown path so closing `EpochParent` still
  exits the whole session cleanly.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Ran live left-drag undock/redock interaction checks against SDL, Raylib,
  SFML, OpenGL, and Software panes.
- Confirmed each tested pane detached to a top-level window and then reattached
  to `EpochParent`.
- Closed the parent host and confirmed the process exited cleanly afterward.
