# Forest Factory Portal And Plant Lab Contract

Plant Lab and Forest Factory are separate editor experiences.

- **Plant Lab** is a dedicated launcher editor for authoring custom trees,
  reusable tree assets, growth behavior, and forest configurations.
- **Forest Factory** is a portal/editor surface inside the standard Epoch
  application. It browses Plant Lab outputs and places or configures them in the
  real project scene.

They may share deterministic morphology, temporal, voxel, asset, and GUI
contracts, but they do not share a central editor surface and one never replaces
the other.

Plant Lab preview geometry carries `PlantLabPreview` authoring identity. Only an
explicit Forest Factory placement operation converts an accepted asset preview
into persistent `ForestFactory` project-scene entities.

## Provenance

The Plant Lab integration source is:

`https://github.com/Autodidac/Temporal_Parametric_Graph_Lindenmayer_System_Plant_Lab`

The operator has confirmed this is their own code, created for Epoch
integration, and has explicitly authorized its use. Record source revision
`40a3db7` when adapting behavior. There is no third-party license blocker.

The prototype remains an integration source rather than a runtime dependency.
Epoch owns production limits, tests, identity, serialization, compiled assets,
and backend-neutral preview/runtime contracts.

## Plant Lab Authoring

Plant Lab owns:

- custom tree and branching-form documents;
- reusable species/preset assets;
- forest configurations such as composition, distribution, density, scale,
  seed, age/time, and LOD intent;
- temporal graph and parametric L-system editing;
- 3D, 2.5D, 2D, and pattern previews;
- mesh, foliage/material, impostor, voxel, and seed output requests;
- save/load, undo/redo, deterministic regeneration, and export.

`authoring.morphology` is its first shared C++23 generation foundation. It
provides generation-checked node/segment/terminal IDs, bounded deterministic
construction, per-organ birth/end ranges, forward/reverse sampling, content
identity, and multi-level voxel LOD planning.

The underlying morphology contract also supports vascular, respiratory,
electrical, coral, and generic branching structures. Those capabilities can
serve future authoring tools without turning Forest Factory into a general
morphology editor.

The current slice is build-proven. Editable typed node UI, compiled morphology
artifacts, sparse voxel rasterization, production mesh compilation, and physical
texture/material outputs remain unfinished.

## Forest Factory Placement Portal

Forest Factory owns:

- browsing Plant Lab tree assets and forest configurations;
- previews sufficient to identify an authored asset;
- placement of individual trees or configured forest groups;
- transforms, bounds, distribution regions, density, seed, age/time, and LOD
  overrides that belong to the project scene;
- package activation and project-visible asset references;
- selection, Focus, save, Build, Run, and scene persistence of placed results.

Forest Factory consumes authored/compiled outputs through stable logical asset
identity. It must not mutate Plant Lab authoring documents through placement
widgets, regenerate a different canonical tree behind the user's back, or embed
Plant Lab's complete editor inside the main application.

The existing `forest.factory` profile/preview path is a compatibility and
default-asset adapter while the authored asset pipeline is completed. Shared
morphology use beneath that adapter does not merge editor ownership.

## Ownership

- Plant Lab owns generation and forest-configuration authoring.
- Forest Factory owns standard-editor browsing and placement.
- Engine mainline owns shared document, morphology, temporal, voxel, compiled
  asset, validation, and project integration contracts.
- EpochEngineExtensions owns heavy optional generators and generated payloads.
- Package Registry owns activation, provenance, integrity, and approval gates.
- Generated projects receive optional payload only after visible activation or
  main-scene use.

## Next Production Slices

1. Define Plant Lab tree-asset and forest-configuration documents with stable
   identity and semantic operations.
2. Build the shared typed node editor with stable node/pin/connection identity,
   validation, serialization, and migration.
3. Separate editable documents, compiled morphology/forest artifacts, and
   physical preview/voxel/texture caches.
4. Add deterministic regeneration independent from time-only sampling.
5. Rasterize selected samples into `SparseVoxelField` LODs and compile indexed
   mesh/material/impostor outputs.
6. Save Plant Lab outputs to the asset registry and project library.
7. Make Forest Factory browse those assets, place scene references, and edit
   placement/configuration overrides without opening Plant Lab internally.
8. Add replay, boundedness, forward/reverse equivalence, placement persistence,
   and Build/Run tests.

Stable authoring documents and semantic operations are canonical. Atlases,
sparse pages, GPU buffers, descriptors, previews, and renderer caches remain
disposable.