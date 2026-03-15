# Epoch v0.82.9

## Summary

v0.82.9 is a docking interaction repair pass. It restores real undock/redock
behavior for parented panes by preserving each pane's original dock host and by
keeping the parent grid limited to panes that are actually still docked.

## Runtime updates

- Preserved the original dock parent on each dockable pane so future drags can
  redock a pane even after it has already been undocked once.
- Updated the parent grid arranger so only panes still parented under
  `EpochParent` are laid out, which stops undocked panes from snapping back or
  stealing space from docked panes.
- Kept the parent-host shutdown fix from `v0.82.8`, so closing the main host
  still exits the full session cleanly.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Ran live undock/redock interaction tests against SDL and Raylib panes.
- Ran a broader live undock/redock sweep against SFML, OpenGL, and Software.
- Confirmed all of those panes undocked to top-level windows, redocked back
  into `EpochParent`, and that closing the parent host still exited the process.
