# EpochEngine 0.90.38 — MSVC host and Windows RC build correction

Base: `EpochEngine-0.90.37-full-source.zip`. This is a local source version, **not** a published binary release.

## Corrected failures

1. `editor.application.cpp(23551,71)` and `(23555,90)` failed with C2039/C3861 because `epochengine::to_std` is not exported to the importer from `core.env`'s global module fragment. The EngCoder host now uses the already imported `epochengine::string` members directly to populate an owned STL string and the external backend path. No changes to model ownership or settings.
2. `WinResource.rc(10)` RC2135 and RC2255 during EpochEditor resource compilation: `icon.ico` lives under `Engine/resource`, not the EpochEditor MSBuild project directory. EpochEditor.vcxproj adds the resource directory to the `ResourceCompile` asset-search path without removing icon, title or VERSIONINFO entries.
3. Development version becomes **0.90.38** (including Windows Editor resource version); published Windows/Linux **0.90.35** and macOS **0.89.30** remain unchanged.

## Patch usage

Apply `EpochEngine-0.90.37-to-0.90.38.patch` at the root of the extracted original 0.90.37 source; `git apply --check` then `git apply` works without any `.git` directory. Or extract the complete 0.90.38 full source ZIP into a clean directory. Do not mix extracted files from unrelated builds.

## Verification boundary

Focused C++23 tests against actual Epoch string header compile under GCC and Clang; resource path resolves structurally and packaged icon is a valid ICO. The full native MSVC/rc.exe build and live Qwen runtime still require Windows verification.
