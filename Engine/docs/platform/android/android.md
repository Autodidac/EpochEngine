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

The October29 output goal is an ordinary project, not only a platform-library
test. The project must be editable and persistent in Epoch, produce an Android
package, and run without the desktop Editor. Start with a small software/GUI
application using only its declared dependencies. AI and terrain remain optional.

Keep the existing owners: `EpochPlatformEngine` owns application entry, native
lifecycle and input; the selected renderer owns graphics; EpochGui owns controls;
the project's build/export path assembles the application and assets. The current
Android platform/tier policy does not supply the missing Android host, package
pipeline or proven renderer. C++23 module toolchain support and packaged asset
resolution must be qualified together before claiming this output works.

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
- rotation, background/foreground and surface loss recover without duplicating
  application state or losing the saved project
- the startup report identifies the actual platform, adapter, graphics tier and
  selection reason, with unknown or unsupported capabilities stated explicitly
- docs say exactly what works and what is still missing
