# Epoch Engine

Epoch is a C++23 engine for software, tools and games, with an optional Editor
and project Assistant. Reusable platform, GUI, audio and media libraries support
the Engine; projects select the services they need.

## Work in the Editor

- Author scenes and inspect objects in the 3D Preview workspace.
- Use separate **2D** and **UI** workspaces for 2D content and GUI documents.
- Create projects, save their authored data, and build and run through the
  existing project controls. Runtime options depend on the selected profile
  and renderer.
- Configure an optional inference provider or local model for chat and project
  assistance. Review proposed tool actions before applying them. Ordinary
  projects do not require an AI service or a loaded model.

Start with the [Editor and project workflows](Engine/docs/engine/workflows.md),
[GUI guide](Engine/docs/engine/gui.md) and
[documentation index](Engine/docs/README.md).
The [Assistant small-game acceptance prompt](Engine/docs/engine/workflows.md#assistant-test-prompt-for-a-complete-small-game)
exercises procedural worlds, custom trees, generated assets and play.

## Run a Windows release

Get a Windows x64 package from [Releases](https://github.com/Autodidac/EpochEngine/releases),
extract it, and run `EpochEditor.exe` beside its included `assets` directory.
Keep the package's dependency libraries and notices with the executable.
Release notes describe the checks completed for that exact package.

## Build from source

Use a supported C++23 toolchain with module support. Prepare the documented
native prerequisites and manifest-pinned dependencies first; see
[build tools](Engine/docs/build/tools.md) and
[build scripts](Engine/docs/build/scripts.md).

### Windows / Visual Studio 2022

Open `Engine.sln`, or use a Visual Studio Developer PowerShell from the repository
root:

```powershell
MSBuild.exe Engine.sln /t:EpochEditor /p:Configuration=Release /p:Platform=x64 /m:1
Set-Location Artifacts\Runtime\windows\public\x64\Release
.\EpochEditor.exe --editor --backend opengl
```

Use `Configuration=Debug` and the adjacent `Debug` directory for a Debug build.
Run from the output directory so runtime assets are available. The build-safe
CPU contract lane exits before renderer startup:

```powershell
.\EpochEditor.exe --engine-contract-self-test
```

Root CMake presets provide another build path. See
[presets](Engine/docs/build/presets.md); current output-layout details are in
[build scripts](Engine/docs/build/scripts.md).

### Linux / Clang

The full-engine module lane uses the pinned Clang toolchain. After installing
the documented native prerequisites, run from the repository root:

```bash
cd Engine
./build.sh --bootstrap-current-toolchain --tool-cache-root ../Artifacts/Tooling/linux clang Release
./run.sh clang Release -- --editor --backend opengl
```

The bootstrap option prepares verified toolchain inputs when needed. The GCC
lane is headless by default; its full module path is experimental. Consult the
exact release's Linux build, test and runtime evidence rather than assuming
parity from a Windows build.

Generated output belongs under `Artifacts/`: `Intermediate` holds compiler
state, `Runtime` holds runnable programs and assets, and `Reports` holds checks.

## Libraries and examples

| Project | Scope |
| --- | --- |
| [EpochGui](https://github.com/Autodidac/EpochGui) | Reusable GUI controls and layout |
| [EpochPlatformEngine](https://github.com/Autodidac/EpochPlatformEngine) | Application, platform and native-window foundation |
| [EpochAudioEngine](https://github.com/Autodidac/EpochAudioEngine) | Audio primitives and an offline WAV example |
| [EpochMediaEngine](https://github.com/Autodidac/EpochMediaEngine) | Media and timeline foundations |
| [EpochGui Demo](https://github.com/Autodidac/EpochGui_Demo) | Standalone GUI integration examples |

Browse `Engine/examples` for Engine entry points and integration examples.
Bundled revisions and Engine adaptations are recorded in
[dependencies.json](Engine/dep/dependencies.json); a standalone library release
does not automatically describe every Engine adapter.

Built-in mini-games use the Engine's shared game modules and `engine_arcade`
project integration. The complete playable 2D workflow and Arcade render-target
presentation remain under validation.

Forest Factory/Plant Lab, planetary terrain and FFT ocean are part of the
optional-package work. The [Extensions catalog](Engine/dep/EpochEngineExtensions/README.md)
contains both implemented source and planned descriptors. Package Manager must
show an available, reviewed source before installation; a catalog entry alone
does not provide a working feature. Ordinary projects do not include every
package automatically. Extraction and connected forest/terrain/ocean workflows
are still being completed.

## Status and license

Renderer support is feature-specific. Read the
[capability evidence](Engine/docs/engine/renderers.md) before choosing a backend;
an API name or successful compilation does not establish visual parity.
Optional asset-generation adapters, full Editor-as-project reproduction and
broader platform coverage still need their remaining integration checks.

Read [LICENSE](LICENSE), including its commercial-licensing requirements.
Dependencies, models and examples have their own licenses and notices.

## Public edition and contributing

This edition retains ordinary inference and project assistance. Engine
self-development automation and own-Engine-source AI tools are physically
excluded. Public source and updates use this GitHub repository without account
pairing; another edition is never an update fallback.

Run `python Tools/build/verify_public_source.py` before contributing a source
snapshot. `source-projection.json` records the admitted file inventory and hash
policy; the verifier documents intentional manifest refreshes. These source
checks are separate from build, runtime and release acceptance.
