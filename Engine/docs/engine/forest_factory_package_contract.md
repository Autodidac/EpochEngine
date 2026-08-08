# Forest Factory Package Contract

Forest Factory is Epoch's standard-editor vegetation workflow. Plant Lab is the
dedicated launcher application for temporal graph / parametric L-system plant
authoring. Plant Lab produces reusable vegetation documents and assets; Forest
Factory browses/imports those outputs and places them into ordinary project
scenes.

## Ownership

- Engine core owns the lightweight deterministic contracts in
  `Engine/modules/forest.factory.ixx`.
- Package Registry owns package identity, provenance, activation mode, and
  security gates in `Engine/modules/package.registry.ixx`.
- `package.registry` validation is part of the non-GUI
  `--engine-contract-self-test` lane, so Forest Factory must remain a core
  opt-in package that ships in the engine but does not enter generated projects
  until main-scene use or package activation is visible.
- Plant Lab owns the dedicated scene-backed vegetation authoring application and
  deterministic temporal-graph preview.
- The standard editor retains the Forest Factory surface/tool for generated
  vegetation browsing, import, placement, package evidence, and project-visible
  activation.
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

GitHub currently reports no license for this repository and its source tree has
no `LICENSE` file. Its architecture and operator-provided behavior remain valid
reference requirements, but verbatim source promotion is blocked until the
repository gains explicit compatible licensing or a provenance record confirms
promotion rights.

The reviewed V6 delta that Epoch must reproduce behind its own contracts is:

- stable node/parent identity plus branch and leaf-cluster records;
- recursive side shoots and editable generation/depth/shoot budgets;
- dormant initial sapling state and overlapping trunk, branch, and leaf timing;
- per-organ birth/end ranges for deterministic forward/reverse evaluation;
- 3D, 2.5D, 2D plant, and 2D L-system pattern modes;
- graph regeneration separated from time-only mesh evaluation;
- indexed position/normal/color/UV mesh output, atlas regions, and leaf styles;
- deterministic seed/preset state plus diffable save/load and export boundaries.
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
- estimated voxel occupancy for trunk, branch, and foliage cells so future LOD,
  hit detection, navigation, lighting, and path-trace consumers can share the
  same deterministic vegetation descriptor
- a scene-backed editor prototype made from deterministic preview primitives so
  Plant Lab can author vegetation before the production mesh/voxel renderer lands
- a Forest Factory surface in the standard editor for importing and placing
  Plant Lab outputs into the same central 3D scene path as ordinary objects
- dedicated Plant Lab launcher access without replacing the standard-editor
  Forest Factory workflow

## Package Manager Behavior

Selecting `Forest Factory` in Package Manager should show the
`EpochEngineExtensions` package source and explain that the editor preview is
built in while generated project payloads remain opt-in. The staged manifest
keeps both `source_repo` and `reference_repo` so package payload ownership and
the external Plant Lab reference repository do not get mixed together.

Installing the package stages:

- `assets/packages/engine_forest_factory.package.json`
- `assets/packages/engine_forest_factory/default.forest.json`

This is a visible project activation record, not a hidden import. It does not
download source, bind ports, create servers, or run package code automatically.
The package descriptor must keep `Main-scene use` activation, the
`EpochEngineExtensions` source route, and a human build gate so the Package
Manager UI stays tied to the validated registry instead of local-only text.

## Acceptance Gates

Before Forest Factory graduates beyond this contract:

- Plant Lab graduates into a dedicated 3D authoring preview with proper tabs,
  sliders, atlas controls, mature-stage playback, and production GUI controls.
- Forest Factory imports and places Plant Lab outputs in the standard editor
  without duplicating the Plant Lab authoring application.
- Package activation can materialize project-local generated assets without
  polluting software projects or minimal game clones.
- Generated outputs can feed mesh LOD, impostor, and voxel occupancy consumers.
- The voxel/pathing/tracing spine can consume Forest Factory occupancy without a
  renderer-specific dependency.
- The preview graduates from primitive occupancy evidence to asset-grade branch,
  foliage, atlas, and growth playback rendering that visibly matches the Plant
  Lab direction.
- Prototype code from the Plant Lab repo is reviewed through package provenance,
  build, test, and license evidence before any source promotion.
