# EpochEngine 0.90.37 — MSVC EngCoder backend compile repair

## Scope

Base: `EpochEngine-0.90.36-full-source.zip`. This is a local C++23 source fix only, not a published Epoch runtime release.

- Fix `core::env::get("ENGCODER_SERVER_EXECUTABLE")` assignment in `render_engcoder_backend_controls`: explicitly convert the engine-owned string to `std::string` using `epochengine::to_std()`.
- Fix the Windows `LOCALAPPDATA/EngCoder` detection path: explicitly convert the engine-owned string before constructing `std::filesystem::path`.
- Retain all prior self-iteration, compact log display, native MSVC logger overload, and Qwen Next Flash / EngCoder profile ownership repairs.
- Bump local development and Windows Editor resource versions to 0.90.37. Published Windows/Linux version 0.90.35 and macOS version 0.89.30 remain untouched.

## Applying

From the root of the extracted 0.90.36 **full source**, run:

```text
git apply --check /path/to/EpochEngine-0.90.36-to-0.90.37.patch
git apply /path/to/EpochEngine-0.90.36-to-0.90.37.patch
```

`git apply` works in a non-Git source directory. Do not apply this incremental patch directly to v0.90.35.

## Verification boundary

The input MSVC log reported C2679 at 23551 and C2440 at 23555, with dependent cascading C2737/C3536 and LNK1104. This release removes those two incompatible conversions. Focused C++23 tests run against the actual `core.stl_types.hpp` with GCC and Clang; there is no native Windows/MSVC build or Qwen model inference in this environment. A complete Debug/Release/ReleaseWithDebugInfo build remains required on the operator's Windows machine.
