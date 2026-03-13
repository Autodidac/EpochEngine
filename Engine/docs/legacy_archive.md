# Legacy Archive

The old standalone `legacy/` tree has been moved into `Engine/legacy/` so the
active runtime and the compatibility/archive material now live under one engine
root.

## What lives here

- `Engine/legacy/include/` - compatibility headers and older public surfaces.
- `Engine/legacy/src/` - older runtime, GUI, scripting, and platform snapshots.

## How to use it

- Use it for migration help, archaeology, or extracting a specific still-useful
  compatibility shim.
- Do not treat it as the preferred source of truth for active engine behavior.
- Promote pieces back into the main engine only when they are actively needed,
  understood, and tested.

## Current caution

Most of the important runtime systems already live in `Engine/modules/` and
`Engine/src/`. Only a few minor archived or experimental surfaces remain in a
partially finished state, so changes here should be deliberate rather than broad.