# Epoch v0.82.7

## Summary

v0.82.7 is a shutdown and diagnostics discipline pass. Closing the main parent
window now propagates shutdown through the docked context panes and exits the
session cleanly, while backend confirmation messages are available again behind
config macros instead of living as hot-loop spam.

## Runtime updates

- Hardened the parent-host `WM_CLOSE` and `WM_DESTROY` flow so tracked child
  contexts are marked for shutdown before the parent window is destroyed.
- Updated the engine loop to honor the multiplexer running state immediately
  after the pump step, which prevents the console/process from hanging after
  the visible parent window is gone.
- Kept backend shutdown local to the affected backends instead of reintroducing
  broad early-close behavior in the shared multiplexer.

## Diagnostics updates

- Added the master `EPOCH_ENABLE_BACKEND_CONFIRMATION_LOGS` switch plus the
  matching context/upload/backend-specific confirmation flags in
  `aengine.config.hpp`.
- Restored one-shot confirmation messages for backend bring-up, uploads, and
  cleanup without bringing back the old per-frame log flood.

## Verification

- Rebuilt `StaticLib1` in `Debug|x64`.
- Rebuilt `ConsoleApplication1` in `Debug|x64`.
- Launched from `x64/Debug`, closed the real `EpochParent` host window, and
  verified the process exited cleanly with no forced kill.
