# Build Scripts

The helper scripts live in `Engine/`. They are optional, but they are the
fastest way to get a repeatable local build.

## `build.sh`

Run from `Engine/`:

```bash
./build.sh [--no-vcpkg] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
```

What it does:

- Configures with `-S "$SCRIPT_DIR"` and an out-of-tree build under `Engine/Bin/`.
- Enables module scanning flags.
- Tries to discover `VCPKG_ROOT` automatically unless `--no-vcpkg` is used.
- Builds the target and generates API docs when Doxygen is available.

Examples:

```bash
cd Engine
./build.sh gcc Release
./build.sh --no-vcpkg clang Debug -- -DEPOCH_ENABLE_RAYLIB=OFF
```

## `run.sh`

```bash
cd Engine
./run.sh [gcc|clang] [Debug|Release] [-- <runtime args>]
```

The script launches the `epoch` runtime from the matching `Engine/Bin/...`
output directory.

## `install.sh`

```bash
cd Engine
./install.sh [gcc|clang] [Debug|Release]
```

Installs to `Engine/built/bin/<Compiler>-<Config>`.

## `clean.sh`

```bash
cd Engine
./clean.sh
```

Removes:

- `Engine/Bin/`
- `Engine/build/`
- `Engine/built/`
- top-level CMake cache/state under `Engine/`

## Related docs

- `build_presets.md`
- `runtime_operations.md`
- `smoke_capture_automation.md`
- `tools_list.md`

## Commit and test discipline

When a pass changes runtime/editor/backend behavior, the current working method is:

- sync with `origin/main` before starting if the local branch has drifted
- keep unrelated dirty files out of the commit instead of rolling them into a "cleanup" blob
- bump the source version in `Engine/modules/aengine.version.ixx`
- use versioned commit titles such as `v0.83.60 ...`
- rebuild `ConsoleApplication1` in both `Debug|x64` and `Release|x64`
- launch from the asset-bearing `x64/Debug/` or `x64/Release/` runtime, not from a source folder
- close live windows after validation so the next pass starts from a known state

If the pass touches Linux or WSL-facing behavior, run the matching WSL build path too instead of validating Windows only.
