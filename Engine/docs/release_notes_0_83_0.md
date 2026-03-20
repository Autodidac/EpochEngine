# Epoch 0.83.0 Release Notes

Epoch 0.83.0 marks the transition to the updater-shell bootstrap flow.

## Highlights

- Added a dedicated updater-shell entry build for `ConsoleApplication1.exe`.
- Simplified the launcher path so the bootstrap binary can focus on update, rebuild, handoff, and restart.
- Added visible version reporting in the launcher and editor surfaces.
- Cleaned up startup console output so update activity is easier to read.
- Improved GUI text wrapping and updater-shell copy so update instructions are clearer.

## Updater Shell

- The updater shell is now the intended binary entry point for users who need to move from a packaged build to a newer source snapshot.
- The shell can close its own window and continue the update in the console while the worker finishes.
- The update description now warns users not to interrupt the process once update starts.
- The release payload for the shell is intentionally slim: executable, required runtime DLLs, and the GUI font asset.

## Source Update Path

- Source-version parsing now reads the version macros in `aengine.version.ixx` correctly.
- The worker now falls back from packaged update checks to source update checks more reliably.
- Managed `vcpkg` handling was hardened so the worker keeps its own registry snapshot instead of escaping into an outer repository.
- Inherited `VCPKG_ROOT` values are ignored during worker execution so existing user installs are not repointed or rewritten.
- The worker prepares its own managed toolchain state before restore/build, keeping update behavior more deterministic.

## Runtime and Stability

- Windows unattended updater runs now avoid interactive debug/assert popups when no debugger is attached.
- The debug assert path only triggers `DebugBreak()` when a debugger is actually present.
- Updater validation now works better from short-path sandboxes, which better matches real release usage.

## User-Facing Outcome

The 0.83.x line is the first line intended to demonstrate a practical bootstrap flow:

1. Start from the packaged updater shell.
2. Check the latest packaged release.
3. If packaged is current but `main` is newer, fall through to source update.
4. Restore dependencies, build the newer source snapshot, replace the runtime, and relaunch.
