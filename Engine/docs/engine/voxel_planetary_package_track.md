# Voxel Planetary Package Track

Epoch's long-horizon world renderer is a voxel-first engine spine, not a
standalone terrain experiment. The target is a multi-informational voxel system
that can drive planetary-scale hybrid terrain, pathfinding, visibility,
lighting, asset generation, and distant-object LOD while still resolving to
classic terrain, mesh, model, material, and vegetation representations when
those are the fastest runtime form.

## Target Shape

- Multi-informational voxel cells should carry geometry, material, occupancy,
  lighting, navigation, biome, water/atmosphere, and generation metadata.
- Planetary terrain should combine voxel SDF/chunk data with classic terrain and
  mesh output rather than forcing every runtime surface to stay voxel-only.
- Voxel ray/path tracing should be treated as a shared information path for
  navigation, lighting, visibility, and generation, not as a renderer-only
  feature.
- Vegetation should converge through procedural plant/L-system generation plus
  SpeedTree-like LOD and impostor output tied back into the voxel field.
- Distant objects should use voxel-informed LOD and generated representations
  before the engine reaches for triangle-virtualization-style solutions.

## Prototype Inputs

Operator-provided prototypes are design references until they pass the research
import and package promotion gates.

- `Autodidac/AlmondVoxel`: operator-provided voxel prototype direction.
- `Autodidac/Temporal_Parametric_Graph_Lindenmayer_System_Plant_Lab`:
  operator-provided procedural plant/L-system direction.
- `C:\Users\iammi\source\cursorAIsource\vk_cp_cursor_nodoublefree.zip`:
  local Vulkan voxel/chunk prototype snapshot inspected on 2026-05-21.

The local zip snapshot has SHA-256:

```text
BCACE57C2ED881C0AE0D5B4B48F8757E80BE08135031CA29EAA3DE2CFE9B69F1
```

Observed prototype shape:

- Vulkan/XCB/Win32/Android platform shell.
- `voxel_terrain` SDF terrain generation.
- Chunk manager and LOD-keyed chunk ownership.
- Marching-style terrain extraction and seam-oriented tests.
- Debug overlay and simple app/runtime harness.

## Package Strategy

Do not import these prototypes directly into mainline engine source. The first
safe path is a package track:

1. Stage the prototype as research with provenance.
2. Create a local package manifest that records source, hash, license/provenance,
   build commands, runtime entry points, tests, and known limitations.
3. Keep package source in a special review branch, a separate repo, or a local
   package cache until the API boundary is clean.
4. Build and run the package through the same human-approved updater/package
   gate used for future downloadable source packages.
5. Promote only the smallest reusable engine interfaces into mainline Epoch:
   voxel data contracts, terrain chunk interfaces, generation jobs, renderer
   bridge points, and asset outputs.

## Acceptance Gates

Before any voxel package becomes tracked engine code:

- The package builds through CMake/MSBuild or the updater-style package build
  path without hardcoded developer paths.
- Tests prove deterministic chunk generation, stable LOD keys, seam behavior,
  and bounded memory behavior.
- The package does not auto-create servers, listeners, hidden model channels, or
  model-accessible bypass surfaces.
- Runtime proof shows the package can feed an editor-visible asset, preview, or
  generated scene without destabilizing current OpenGL/DirectX/multicontext
  editor behavior.
- The import adds one clear engine-owned API boundary instead of another
  competing terrain/render framework.

## Deferred Engine Work

- Define the core voxel cell, chunk, and LOD metadata contracts.
- Add package-manager support for local research packages with provenance and
  build/test evidence.
- Add a voxel terrain preview workspace once the GUI library has stable tabs,
  dropdowns, scrollable text, modals, and copy/paste.
- Connect EpochBot planning to package evidence only after the gate can reject
  low-evidence or hidden-reasoning model output.
