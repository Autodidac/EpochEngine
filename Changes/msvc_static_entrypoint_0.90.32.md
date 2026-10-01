> **Superseded by v0.90.33:** this first repair over-suppressed the normal EpochEditor internal entry point. See `msvc_internal_entrypoint_and_dual_build_0.90.33.md`.

# v0.90.32 — MSVC static entry-point and relocatable ProjectLauncher fix

## Reported failure

Release x64 ProjectLauncher linked both the executable entry point and a legacy entry point from the static engine library:

```text
EpochEngine.lib(epoch.engine_legacy.obj) : error LNK2005: main already defined in epoch.main.obj
ProjectLauncher.exe : fatal error LNK1169: one or more multiply defined symbols found
```

## Cause

The CMake static-runtime lane already excluded legacy executable entry-point ownership, but the checked-in Visual Studio static-library path could still compile `epoch.engine_legacy.cpp` with `main` available. ProjectLauncher correctly compiled `Engine/src/epoch.main.cpp`, so the final link had two `main` definitions.

## Fix

- Wrap legacy entry-point ownership in `#if !defined(ENGINE_STATICLIB) && !defined(EPOCH_MAIN_IN_MAIN_CPP)`.
- Define the static-library/application split in the EpochEngine MSVC project for both Debug and Release.
- Preserve ProjectLauncher as the sole executable entry-point owner.
- Keep `Debug | x64` and `Release | x64` directly available in `Engine.sln`.
- Make the bundled ProjectLauncher resolve `EpochRepoRoot`, engine includes/libraries, vcpkg manifest/install paths, and project references relative to its extracted source-tree location rather than an old development worktree.

## Acceptance

Source inspection can prove the guard/project wiring. Final acceptance requires native MSVC builds of ProjectLauncher in both Debug x64 and Release x64 with no LNK2005/LNK1169, followed by a launch check. Do not record those native builds as passed until the operator runs them.
