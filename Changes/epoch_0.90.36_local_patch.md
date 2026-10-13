# EpochEngine 0.90.36 local source patch (2026-10-11)

## Base and payload

- Base: operator-provided `epochengine0.90.35.zip` **not** a GitHub commit or a runtime binary release.
- Output: `EpochEngine-0.90.36-full-source.zip` and `EpochEngine-0.90.35-to-0.90.36.patch`.
- Both outputs preserve all source, assets, local `addons/EngCoder` material and existing project scaffolding from the base; omit any `.git/` directory.
- Contains the previously prepared source-iteration exact-edit recovery, compact logging changes, MSVC `std::string_view` comparison fix, and EngCoder Qwen Flash Next external host/profile owner fixes.
- The `Engine/src/build/build.cpp_feature_probe.hpp` source-owned header remains present.

## Version and limits

- The development and compiled Editor resource version is **0.90.36**.
- The signed/published Windows/Linux 0.90.35 and macOS 0.89.30 packaged-version constants are intentionally unchanged; this patch is not a new public release.
- This does not bundle EngCoder's complete Qwen Flash server or its model weights. Epoch connects to the separately installed backend without overriding that backend's home, runtime and cache paths.
- Debug/Release Windows builds, actual Qwen loading, and successful self-iteration candidate edits are **not** verified here.

## Applying the patch

Extract the **original** `epochengine0.90.35.zip` into a fresh working directory and from its root run:

```bash
git apply --check /path/to/EpochEngine-0.90.35-to-0.90.36.patch
git apply /path/to/EpochEngine-0.90.35-to-0.90.36.patch
```

`git apply` can be used in a folder without a `.git` repository. Alternatively, extract the full-source ZIP into a fresh folder. Do **not** apply the patch to the previously modified source ZIPs.
