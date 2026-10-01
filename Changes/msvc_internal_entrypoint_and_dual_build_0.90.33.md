# v0.90.33 — internal editor entry point + Debug/Release combined MSVC build

## Operator evidence

The v0.90.32 Release rebuild proved that ProjectLauncher linked, but EpochEditor failed with `LNK2019` because the regular `EpochEngine.lib` no longer contributed the internal `main` expected by the thin `EpochEditor` executable project.

## Correct ownership

There are two intentional build modes:

1. **Epoch editor mode** — `EpochEditor` links the normal checked-in `EpochEngine` static library. The engine's internal legacy entry path owns `main`/`wWinMain`; `epoch.editor_entry.cpp` does not duplicate it.
2. **Generated child/static-runtime mode** — generated ProjectLauncher builds pass `EPOCH_MAIN_IN_MAIN_CPP=1` while building their runtime library and compile their own generated `source/epoch.main.cpp`. Only this mode suppresses the internal legacy entry path.

`ENGINE_STATICLIB` alone must therefore **not** suppress the internal editor entry point. It describes linkage form, not executable-entry ownership.

## Visual Studio workflow

`Engine.sln` contains the engine/editor development projects and no generated ProjectLauncher target. It exposes:

- `Debug | x64`
- `Release | x64`
- `Debug+Release | x64`

The combined configuration builds the normal Debug x64 solution first and then the normal Release x64 solution. It does not introduce a third binary ABI/configuration.

## Acceptance

On native MSVC:

- Debug x64 EpochEditor links with exactly one engine-owned entry point.
- Release x64 EpochEditor links with exactly one engine-owned entry point.
- `Debug+Release | x64` completes both normal builds.
- A generated ProjectLauncher still links independently with its generated `epoch.main.cpp` and without a duplicate engine entry point.
