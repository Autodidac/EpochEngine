# Epoch v0.82.11

## Summary

v0.82.11 hardens parent-host shutdown ownership for the parented multi-context
runtime. Docked panes now close with `EpochParent`, undocked panes survive as
independent top-level windows, and redocked panes become parent-owned again.

## Runtime updates

- Changed parent-host shutdown to target the authoritative live backend window
  for each context instead of stale placeholder hosts.
- Preserved undocked SDL, Raylib, and SFML panes when the parent host closes.
- Restored parent-owned shutdown semantics automatically once a pane is
  redocked.
- Fixed the last-window exit path so the process shuts down cleanly when the
  final surviving undocked pane closes.
- Added a live-window guard in the engine loop so stale backend bookkeeping does
  not strand the session after the last real native window disappears.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Rebuilt `StaticLib1` in `Debug|x64`.
- Ran live undock/parent-close/survivor-close verification against:
  - SDL (`SDL_app`)
  - Raylib (`GLFW30`)
  - SFML (`SFML_Window`)
- Confirmed:
  - undocked panes survive parent close
  - redocked panes close with the parent again
  - the process exits after the final surviving undocked pane closes
