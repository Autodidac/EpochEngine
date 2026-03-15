# Epoch v0.82.8

## Summary

v0.82.8 is a focused docking/shutdown hotfix. Closing the main parent host now
keeps backend panes docked in place while shutdown propagates, instead of
undocking them as part of the host close sequence.

## Runtime updates

- Removed the parent-close undock message from the Win32 multiplexer shutdown
  path so backend-owned child windows are closed in place.
- Preserved the verified parent shutdown behavior that marks tracked panes for
  close and lets the process exit cleanly when the `EpochParent` host closes.

## Verification

- Rebuilt `ConsoleApplication1` in `Release|x64`.
- Launched in `--parented --editor` mode from `x64/Release`.
- Verified that backend panes were still parented under the live `EpochParent`
  window at runtime.
- Closed the real parent window and confirmed the process exited cleanly.
