# Forest Factory Package Contract

Forest Factory is Epoch's core procedural vegetation surface. It is the
production-facing name for the temporal graph / parametric L-system plant work
shown in the older Plant Lab prototype line.

## Ownership

- Engine core owns the lightweight deterministic contracts in
  `Engine/modules/forest.factory.ixx`.
- Package Registry owns package identity, provenance, activation mode, and
  security gates in `Engine/modules/package.registry.ixx`.
- The editor workspace row opens the current Forest Factory workbench surface,
  which runs through the scene-backed editor viewport with deterministic
  temporal-graph preview entities while keeping provenance, staged package
  evidence, and preview estimates documented.
- Package payload/source routing belongs in
  `https://github.com/Autodidac/EpochEngineExtensions`.
- The Package Manager stages project-visible manifests and deterministic seed
  profiles under `assets/packages/engine_forest_factory/`.
- Generated projects do not carry Forest Factory payload by default. They get
  project assets only after explicit package activation or visible main-scene
  use.

## Prototype Source

Reference/prototype source:

```text
https://github.com/Autodidac/Temporal_Parametric_Graph_Lindenmayer_System_Plant_Lab
```

The prototype repo is not cloned into EpochEngine mainline by default. Mainline
keeps the stable API shape and package gate. Implementation-heavy or
experimental payloads should move through `Autodidac/EpochEngineExtensions`
package work before source promotion.

## Current Core Shape

The first production contract exposes:

- presets: Cannabis, Tree, Bush, Fern
- preview modes: 2D and 3D
- edit stages: Stage, Structure, Branch, Foliage
- deterministic seed/profile data
- temporal controls: time, speed, duration, play, reverse
- branch controls matching the Plant Lab direction: node count, branch length,
  levels, children, angle, spread, twist, jitter, bend, outward bias, curve, sag
- output categories: preview skeleton, mesh LOD, impostor, voxel occupancy, and
  seed asset
- estimated preview stats for nodes, branches, leaves, vertices, and triangles
- a scene-backed editor prototype made from deterministic preview primitives so
  Forest Factory is visible in the same central 3D editor path as other
  workspaces before the production mesh/voxel renderer lands
- dedicated editor workspace access from the main toolbar instead of an Asset
  command-menu shortcut that spawns placeholder geometry

## Package Manager Behavior

Selecting `Forest Factory` in Package Manager should show the
`EpochEngineExtensions` package source and explain that the editor preview is
built in while generated project payloads remain opt-in. The staged manifest
keeps both `source_repo` and `reference_repo` so package payload ownership and
Plant Lab provenance do not get mixed together.

Installing the package stages:

- `assets/packages/engine_forest_factory.package.json`
- `assets/packages/engine_forest_factory/default.forest.json`

This is a visible project activation record, not a hidden import. It does not
download source, bind ports, create servers, or run package code automatically.

## Acceptance Gates

Before Forest Factory graduates beyond this contract:

- The current Forest Factory scene-backed prototype graduates into a dedicated
  3D preview with proper tabs, sliders, atlas controls, mature-stage playback,
  and production GUI controls.
- Package activation can materialize project-local generated assets without
  polluting software projects or minimal game clones.
- Generated outputs can feed mesh LOD, impostor, and voxel occupancy consumers.
- The voxel/pathing/tracing spine can consume Forest Factory occupancy without a
  renderer-specific dependency.
- Prototype code from the Plant Lab repo is reviewed through package provenance,
  build, test, and license evidence before any source promotion.
