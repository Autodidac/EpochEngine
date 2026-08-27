# Epoch - Creative Software And Game Engine

<p align="left">
  <img src="https://img.shields.io/badge/Current_Source_Development-v0.89.29-1F7A4C?style=for-the-badge" alt="Current development source v0.89.29" />
  <img src="https://img.shields.io/badge/Published_Runtime_Release-v0.89.28-2C6A8A?style=for-the-badge" alt="Published runtime release v0.89.28" />
</p>

EpochEngine development source is restricted. Public distribution is through
the [Epoch release site](https://epoch.adamrushford.chatgpt.site), which exposes
verified runtime packages and updater evidence without an anonymous source or
Git endpoint. EpochGui remains an independently published library.

<p align="center">
  <img src="Images/readme/epoch-engine-overview-v08435.png" alt="Epoch Engine overview card" />
</p>

<p align="left">
  <img src="https://img.shields.io/badge/Project--Centric_Runtime-1F6F78?style=for-the-badge" alt="Project-centric runtime" />
  <img src="https://img.shields.io/badge/Multicontext_Tooling-486B4A?style=for-the-badge" alt="Multicontext tooling" />
  <img src="https://img.shields.io/badge/AI--Assisted_Engine_Ops-5A4D86?style=for-the-badge" alt="AI-assisted engine operations" />
  <img src="https://img.shields.io/badge/C%2B%2B23_Scripting-8A5C2F?style=for-the-badge" alt="C++23 scripting" />
  <img src="https://img.shields.io/badge/Functional_Static_Programming-365D7D?style=for-the-badge" alt="Functional static programming" />
  <img src="https://img.shields.io/badge/AI--Software_Creation-6A4F8A?style=for-the-badge" alt="AI software creation" />
  <img src="https://img.shields.io/badge/AI--Game_Creation-6B7A32?style=for-the-badge" alt="AI game creation" />
  <img src="https://img.shields.io/badge/Universal_Build_Projects-6D5C36?style=for-the-badge" alt="Universal build projects" />
  <img src="https://img.shields.io/badge/Automatic_Entry_Point_Handling-8A473D?style=for-the-badge" alt="Automatic entry point handling" />
  <img src="https://img.shields.io/badge/Zero--Dependency_Core_Goal-34495E?style=for-the-badge" alt="Zero-dependency core goal" />
</p>

<p align="center">
  <strong>Worlds First</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/AI_Software_Engine-0E7490?style=for-the-badge" alt="AI software engine" />
  <img src="https://img.shields.io/badge/Fully_Featured_Vibe--Coded_Engine-A855F7?style=for-the-badge" alt="Fully featured vibe-coded engine" />
</p>

---

<p align="center">
  <img src="Images/readme/epoch-engine-story-v08431.png" alt="The story of Epoch Engine timeline" />
</p>

## What Epoch Is

For newcomers:

- Think of Epoch as one big workbench for making games and tools. Engines like
  Unity, Unreal, and Godot try to give you one main place to build things, and
  Epoch is aiming for that same kind of all-in-one home in its own way.
- The launcher is the front door. You pick a project, choose settings, check
  updates, and then open the editor.
- The editor is the work room. You can look at scenes, run the project, build
  scripts, check systems, and use AI tools there.
- The AI workspace has its own sub-workspaces. `Engine Development` is the separate
  guarded source-work sandbox, `Harness` runs editor tool scripts and captures
  before/after state, `Assistant` is for normal game-engine/project guidance,
  `Launcher` tracks project/build evidence, and `Evidence` reviews explicit
  tool traces and evals. Epoch runs an operator-selected model; it does not
  train an internal LLM.
- Epoch can also draw the same project in different ways. Those are called
  rendering backends, but you can think of them as different drawing engines
  under the hood.
- The downloadable builds are kept small on purpose. EpochEngine source and
  deeper engine work remain in the restricted development repository.

For engine/tooling developers:

- active engine code lives under `Engine/src/`, `Engine/modules/`,
  `Engine/include/`, `Engine/resource/`, and `Engine/ai/`
- local MSVC runtime outputs usually live under `x64/Debug/` and `x64/Release/`
- Windows multicontext validation should run from asset-bearing output folders,
  not from the repo root
- the repo is moving toward one active product backend at a time in normal use:
  editor -> OpenGL today, Windows-native product rendering -> DirectX/D3D11 as
  it matures, and software kept as fallback/debug/headless support

## Current Snapshot

- Active development source and the staged Windows/Linux runtime candidate are
  `v0.89.29`. The release Site's signed latest-release API is authoritative;
  published `v0.89.28`, v0.89.27, and immutable v0.89.06 packages remain the
  live and historical release evidence.
- The public updater consumes Epoch-owned runtime releases, checksums, and build
  evidence from the ChatGPT Site instead of GitHub. Anonymous source discovery,
  source archives, and EpochEngine smart-Git are disabled.
- EpochGui remains independently public and is kept identical to
  `Engine/dep/EpochGui`. The historical `multicontext-base-stable` branch remains
  fixed at `ad6c416d930b348a61bc37ceb7d4522742be084a` inside restricted development
  history.
- Epoch now has one forward plan:
  [the capability-tier architecture](Engine/docs/engine/capability_tier_architecture.md).
  It selects implementations per subsystem and operation from `T0-CPU` through
  proven GLES, OpenGL, Vulkan, DirectX, and future ray tiers.
- The immediate product target is a playable baseline 2D project that can be
  authored, saved, reopened, run, and built through the normal project workflow.
- Current source includes renderer-neutral math, bounded lighting, CPU ray
  queries, explicit request-driven temporal mapping/history, temporal texture
  documents, deterministic physics/audio managers and PCM mixing, optional
  renderer-independent SDL3 audio output, sparse voxel and water foundations,
  a canonical Tier-0 scene, and capability/evidence reporting.
- `render.canvas2d` now owns validated project policy, pixel-aware camera and
  viewport plans, generation-checked sprite identity, logical texture/material
  declarations, deterministic quad batching, tile descriptors, immutable frame
  submissions, final-compose plans, bounded diagnostics, contract tests, and a
  deterministic `T0-CPU` reference raster with texture/clip bindings, fixed-point
  coverage, sampling/blend policy, final composition, metrics, and image hashes.
  `render.canvas2d_presentation` now validates full-frame identity, derives
  content digests from actual pixel bytes, resolves CPU raster output through
  `render.texture.residency`, and dispatches an explicit native presentation
  packet without making physical state canonical. The cache resolves stable
  logical artifacts into bounded, generation-checked, disposable physical
  textures with reuse, eviction, pinning, upload budgets, backend reset,
  metrics, and cache-recreation proof. The compiled artifact schema, stable
  hashing, and integrity validator are runtime-owned and pass a standalone
  Clang contract with authoring and texture-editor features disabled; authoring
  imports that same canonical artifact boundary when producing source content.
  `project.texture_library` persists verified compiled bytes beneath each
  project Library, while bounded BMP, TGA, and P6 PPM import feeds one
  `project.texture_pipeline` registry/resource boundary. Snapshot format 3
  preserves semantic texture materials through atomic save/reopen and restores
  exact owned Canvas2D leases without persisting physical cache handles. The
  World Outliner Assets surface owns source discovery, import, Library restore,
  assignment, and diagnostics over that same controller.
  The primary OpenGL adapter compiles a scoped, viewport-confined final
  compositor with context-owned texture checks and core-profile-valid state
  restoration. An immutable per-context scene exchange maps committed editor
  entities into semantic Canvas2D sprites and presents them in the protected
  OpenGL scene slot. Approved capture evidence matches the T0-CPU reference
  across 1,178,872 pixels with zero outliers and zero channel error.
  All six generated profiles pass materialize, save/reopen, build, and child
  self-test, and GUI Editor passes the equivalent standalone external Run.
  One shared scene-raster session now feeds native Canvas2D adapters for OpenGL,
  SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, and Software. All seven adapters
  compile in Debug and Release and preserve renderer-owned presentation state.
  OpenGL retains exact live pixel evidence; the bounded Assets interaction
  automation and the six other native paths remain `Partial` until approved live
  capture, save/reopen, and repeated context-switch evidence passes.
- `authoring.tilemap` now provides stable semantic map editing and deterministic
  compiled artifacts; project Library publication/restore and Canvas2D visible
  chunk compilation are build-proven. The EpochGui palette/workspace now exposes
  generation-checked object and layer authoring with staged semantic properties,
  hierarchy actions, direct object drag, layer visibility/lock/collision/order,
  and exact source/Library/runtime restoration. The canonical map source,
  standalone restoration, and project input profile feed one
  deterministic fixed-step actor runtime with spawn, pause, reset, map collision,
  snapshots, and Canvas2D publication. A canonical sprite-animation source and
  compiled artifact now select idle/run/rise/fall frames deterministically, and
  actor events drive process-owned jump/landing cues without tying audio lifetime
  to a renderer context. Project now exposes generation-checked keyboard binding
  selectors that publish one matching source/artifact revision and feed the live
  runtime sampler independently of editor-camera input. Physical controller polling,
  imported clip decoding, authored audio events, and approved live interaction
  remain work. Palette collision now
  stages solid, one-way, both slope orientations, and bounded custom-box intent
  as one semantic operation; exact shape/filter data reaches Library artifacts,
  Canvas2D preview, and the deterministic actor solver.
- The launcher directly opens three application profiles after a prelaunch
  context choice: standard Editor, Plant Lab, and GUI Editor. They share engine
  services and one shell implementation but own separate source files, scene
  seeds, surfaces, camera policy, panes, and run/authoring permissions.
- Plant Lab is the separate custom-tree and forest-configuration authoring
  application. Forest Factory is the placement portal in the standard editor;
  its current default-asset adapter proves explicit scene placement, while
  project-library browsing and compiled Plant Lab asset import remain the next
  production slice. GUI Editor owns its canonical Canvas2D scene without legacy
  transform rewriting.
- One renderer owns logical active-editor authority at a time, independent of
  context creation order, parent-grid side, or dock state. Physical renderer
  contexts remain movable, while diagnostic contexts and application-compatible
  routed panes keep optional popout/redock.
- Engine Arcade now uses a validated cabinet silhouette with screen and control
  details instead of overlapping boxes. Its screen uses one shared attract
  pattern rendered into backend-owned sampled surfaces in OpenGL, SDL3, SFML3,
  Raylib3, Vulkan, DirectX, and Software. Every adapter now samples the shared
  front-facing cabinet plane instead of the reversed rear face. Rounded GUI
  controls are an opt-in EpochGui style in Editor Settings and remain off by default.
- Operator proof accepts SDL3, SFML3, DirectX, and Software solid orientation.
  Raylib and Vulkan solid correction, Vulkan replacement teardown, and every
  Arcade scene-surface path remain `Partial` until they receive visual and
  repeated-switch proof.
  Backend status is recorded in the
  [renderer feature matrix](Engine/docs/engine/renderer_feature_matrix.md), not
  inferred from API names or build success alone.

## In Action

The image below demonstrates Epoch's multicontext diagnostic shell. Normal
editor work owns one active backend; multicontext remains a comparison and
validation tool rather than the runtime selection model.

<p align="center">
  <img src="Images/readme/windows-multicontext-editor-v08709.png" alt="Epoch multicontext editor diagnostic" />
</p>

## What Epoch Provides Right Now

- A project-centric launcher/editor/runtime flow with project-owned scene data,
  generated project shells, single-context runs, and build-safe contract tests.
- A canonical Tier-0 default scene with camera, ground, directional light,
  spawn, starter object, ray-based selection, and working Focus behavior.
- Backend-neutral capability profiles, budgets, proof stages, and deterministic
  per-subsystem fallback selection; settings expose current evidence instead of
  unsupported feature switches.
- A shared renderer/resource spine used by OpenGL, DirectX, Vulkan, Raylib,
  SDL3, SFML3, software, and headless paths without making any API authoritative
  engine state.
- C++23 EpochGui controls for windows, tabs, selection, text, font/image/input,
  layouts, rounded rectangles, popups, panels, docking, and optional desktop
  floating-window hosts.
- Temporal world and authoring contracts that separate stable documents and
  semantic history from compiled artifacts and disposable physical caches.
- Temporal texture documents now compile deterministic owning RGBA8 mip
  artifacts, validate byte/content identity, and cross one renderer-neutral
  adapter into the bounded standalone residency cache without making GPU
  handles or physical placement canonical authoring state.
- Package and extension gates for optional terrain, voxel, ocean, networking,
  and technique-gallery work in
  [EpochEngineExtensions](https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git).

## Evidence

Current capability truth lives in the
[renderer feature matrix](Engine/docs/engine/renderer_feature_matrix.md).
Architecture and delivery state live in
[Engine/docs/README.md](Engine/docs/README.md) and `Changes/`. Images under
`Images/readme/` are retained as visual evidence archives; version-specific
release chronology belongs in [Changes/changelog.txt](Changes/changelog.txt).
## Quick Start

### Run the local Windows build

Launch from the binary directory so colocated assets resolve cleanly:

```powershell
Set-Location x64/Debug
.\EpochEditor.exe
```

Release build:

```powershell
Set-Location x64/Release
.\EpochEditor.exe
```

### Build with Visual Studio / MSBuild

Solution:

```text
Engine.sln
```

Typical configurations:

- `Debug | x64`
- `Release | x64`

Example app only:

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Engine.sln /t:ConsoleApplication1 /p:Configuration=Debug /p:Platform=x64 /m:1
```

Generated shell self-test:

```powershell
.\x64\Debug\EpochEditor.exe --editor-project-self-test sandbox
.\Projects\Sandbox\bin\windows\Debug\x64\Sandbox.exe --project-self-test
.\x64\Debug\EpochEditor.exe --editor-project-self-test projectlauncher
.\Projects\ProjectLauncher\bin\windows\Debug\x64\ProjectLauncher.exe --project-self-test
```

### Build with CMake

Windows MSVC:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

Linux:

```bash
cmake --preset ninja-clang-debug
cmake --build --preset ninja-clang-debug
```

Linux/GCC 16 uses the headless validation presets by default while the
full-engine GNU C++ module lane remains experimental. Use `ninja-gcc-debug`
for headless validation, use the current Clang toolchain for full Linux engine
builds, or explicitly opt into the experimental GCC module path with
`-DEPOCH_ALLOW_GCC_MODULE_ENGINE=ON`.

Hosted CI mirrors that split: required portable lanes build/test headless, and
the Linux Clang engine lane builds the real `epoch` target with OpenGL,
software renderer, and SFML enabled at build time without launching GUI windows.

### Run under WSL/Linux

- use an asset-bearing output such as `Engine/Bin/Clang-Release/`
- launch `./epoch` from the output directory
- updater-shell mode is explicit/bootstrap-only on Linux/WSL, not the default
  runtime identity

## Repository Layout

```text
Engine/    engine code, internal source grouped toward platform/render, examples, assets, and docs
Changes/   changelog, roadmap, release-note archives, current planning text
Images/    README and repo artwork
Tools/     local helper scripts and validation utilities
x64/       MSVC local outputs with colocated runtime assets
```

## Documentation

Documentation index: [Engine/docs/README.md](Engine/docs/README.md)

If you're new:

- [Engine/docs/build/cmake_presets_and_builds.md](Engine/docs/build/cmake_presets_and_builds.md)
- [Engine/docs/build/local_build_scripts_and_release_packaging.md](Engine/docs/build/local_build_scripts_and_release_packaging.md)
- [Engine/docs/engine/runtime_and_editor_workflows.md](Engine/docs/engine/runtime_and_editor_workflows.md)
- [Engine/docs/platform/linux/linux_wsl_build_setup.md](Engine/docs/platform/linux/linux_wsl_build_setup.md)

If you're digging into engine behavior:

- [Engine/docs/engine/capability_tier_architecture.md](Engine/docs/engine/capability_tier_architecture.md)`n- [Engine/docs/engine/temporal_engine_architecture.md](Engine/docs/engine/temporal_engine_architecture.md)`n- [Engine/docs/engine/temporal_authoring_platform.md](Engine/docs/engine/temporal_authoring_platform.md)
- [Engine/docs/engine/backend_context_status.md](Engine/docs/engine/backend_context_status.md)
- [Engine/docs/engine/backend_menu_overlay_status.md](Engine/docs/engine/backend_menu_overlay_status.md)
- [Engine/docs/engine/os_ai_tooling_and_evidence_policy.md](Engine/docs/engine/os_ai_tooling_and_evidence_policy.md)
- [Engine/docs/engine/smoke_capture_and_screenshot_workflow.md](Engine/docs/engine/smoke_capture_and_screenshot_workflow.md)

Project planning and release history:

- [Changes/roadmap.md](Changes/roadmap.md)
- [Changes/changelog.txt](Changes/changelog.txt)
- [Changes/engine_history_and_release_archive.md](Changes/engine_history_and_release_archive.md)

## Roadmap Direction

The next eight weeks are focused on one acceptance loop:

```text
capability/project profile
-> temporal textures and residency
-> Canvas2D compose and sprite batches
-> tilemap and scene authoring
-> input, deterministic 2D physics, audio, and animation
-> Play, Run, Build, save, reopen, and cache regeneration
```

The canonical architecture is
[Engine/docs/engine/capability_tier_architecture.md](Engine/docs/engine/capability_tier_architecture.md).
The bounded current gate is [Changes/active_pass.md](Changes/active_pass.md), and
[Changes/roadmap.md](Changes/roadmap.md) owns the delivery schedule.
## License

```text
LicenseRef-MIT-NoSell
```

Epoch is free for non-commercial use. For commercial use or any legal edge
case, read [LICENSE](LICENSE) directly and follow the full agreement text
instead of relying on the README summary.
