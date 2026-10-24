# Android Bring-Up Plan

This doc records the current honest Android direction for Epoch.

## Current Truth

- Android is now the **first mobile priority**.
- macOS may still appear in build surfaces, but it is **not** the immediate
  next platform focus.
- The active repo does **not** yet have a validated Android runtime/package
  path.
- A stale filter or placeholder mention is not proof of working Android
  support.

## First Android Milestone

The first mobile milestone should stay deliberately narrow:

- one **single-context** runtime path
- one stable mobile renderer/backend path
- one input/touch ownership path
- one packaging/install story
- one asset/shader/script resolution path rooted from the installed runtime

That means Android should **not** begin by copying the desktop multicontext
story onto mobile.

## What Must Stay True

- no cwd-dependent asset lookups
- no mobile-only hacks that fork the asset resolver into a separate logic tree
- no fake "supported" claim until an actual packaged Android runtime boots
- no broad backend count requirement for the first milestone

## Recommended Order

1. keep the executable-root/runtime-root resolver as the shared contract
2. add one Android build/package path
3. prove one asset-bearing Android runtime boot
4. wire touch/input and lifecycle handling
5. validate one real render path on device/emulator
6. only then expand tooling/editor ambitions further

## Success Criteria

- Android package builds from the active source tree
- the packaged runtime can resolve assets, shaders, scripts, and logs without
  cwd assumptions
- one stable mobile backend path presents real content
- app lifecycle and touch/input behave predictably
- docs say exactly what works and what is still missing
