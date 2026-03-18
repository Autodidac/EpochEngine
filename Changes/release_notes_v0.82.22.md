# Epoch v0.82.22 Release Notes

## Highlights

- Fixed the self-updater to target the shipped `main.zip` GitHub release asset.
- Updated the runtime replacement path so Windows installs from the packaged
  runtime directory instead of assuming GitHub serves a single replacement
  executable.
- Routed updater progress/status messages through Epoch logging so captured
  output no longer shows raw control-character glyphs at line endings.

## Technical Notes

- The built-in version check now points at
  `Engine/modules/aengine.version.ixx` on `main`.
- The release download target now points at
  `releases/latest/download/main.zip`.
- Windows update extraction now uses the platform archive tooling path instead
  of assuming a one-file executable swap.
