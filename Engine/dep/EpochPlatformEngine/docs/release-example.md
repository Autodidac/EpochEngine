# Release example

The release example is the runnable EpochPlatformEngine demonstration shipped with each binary release.

It intentionally does not create a renderer. EpochPlatformEngine owns process startup, native windows, events, context routing, parenting, and platform-driver threads; renderer backends attach to the published native handles.

## What it demonstrates

- automatic internal application entrypoint with no user-written `main`, `WinMain`, or `wWinMain`
- static and shared library application builds
- one primary native window
- one independent detached tools window
- one headless background context on its own driver host
- dedicated platform-driver thread routing
- per-context frame callbacks and live title updates
- independent window close handling
- automatic headless fallback

Close both visible windows to stop the worker and exit cleanly.

Use `--auto-exit` to close all contexts after eight seconds. The example also auto-exits after eight seconds when both visible contexts fall back to the headless driver.

## Release package layout

```text
EpochPlatformEngine-vX.Y.Z-<platform>-x64/
├── bin/
│   ├── EpochPlatformReleaseExampleStatic[.exe]
│   ├── EpochPlatformReleaseExampleShared[.exe]
│   └── EpochPlatformEngine shared runtime
├── sdk/
│   ├── include/
│   ├── lib/
│   └── CMake package files
├── source/
│   └── release_app.cpp
├── README.md
├── RELEASE_EXAMPLE.md
└── LICENSE
```

The static executable is the simplest standalone test. The shared executable must remain beside the included EpochPlatformEngine shared runtime.

## Build targets

CMake:

```bash
cmake --preset ninja-release
cmake --build --preset ninja-release --target EpochPlatformReleaseExampleStatic
cmake --build --preset ninja-release --target EpochPlatformReleaseExampleShared
```

Native Visual Studio:

```text
msvc/EpochPlatformEngine.sln
```

Build either `EpochPlatformReleaseExampleStatic` or `EpochPlatformReleaseExampleShared` in `Release|x64`.

Direct compiler builds:

```bash
./build.sh gcc Release
./build.sh clang Release
```

```bat
build.bat Release
```
