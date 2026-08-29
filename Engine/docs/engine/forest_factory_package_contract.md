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

The current document, compiler, and project-persistence slice is build-proven.
Editable typed node UI, sparse voxel rasterization, production mesh compilation,
and physical texture/material outputs remain unfinished.

## Project Source And Library Authority

`project.forest_library` owns the first canonical Plant Lab asset path:
`Assets/Forest/*.epoch_forest`. The bounded binary source preserves stable
genome identity, profile state, revision/content identity, and the complete
semantic edit journal. Each publication compiles that exact source into a
content-addressed immutable `Library/Forest` descriptor containing the admitted
preview geometry, voxel LOD, and occupancy identity. Both formats carry SHA-256
integrity and fail closed on malformed, oversized, mismatched, or corrupted
content.

The Library writes and verifies an immutable artifact before atomically
replacing the canonical source. Exact and latest reopen remain available across
process restarts; old artifact revisions stay addressable. Preview atlases and
renderer resources are not serialized into either format.

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

Placed preview solids now keep their internal authored offsets while one merged
vertical bound is aligned to the primary Ground support surface through
`scene.surface_alignment`. Plant Lab preview projection uses the same group
rule. This removes independent per-path center-Y guesses without claiming
physical settling, terrain collision, arbitrary imported-mesh bounds, or a GUI
eye test.

The production preview no longer reduces every authored segment to a capped
box. Trunks and branches use their real endpoints and sampled radii to form
oriented tapered eight-sided solids. Trunks close only at the root; branches
close only at their terminal end, preventing stacked internal caps at connected
nodes. Invalid zero-length or zero-radius projections are refused. The
renderer-neutral indexed mesh compiler separately proves deterministic
topology, logical materials, leaf-card orientation, bounds, budgets, and cap
evidence; production indexed-mesh consumption and native pixel acceptance
remain later gates.

## Tiered Terrain Extension Boundary

The first non-descriptor-only source in the local EpochEngineExtensions subtree
is `demonstrations/tiered_terrain`. It imports the authoritative
`terrain.foundation` and `render.math` modules and emits one deterministic,
bounded local `Heightfield`; it does not replace Engine terrain identity,
surface queries, mesh plans, limits, or validation.

The generator accepts 5..129 samples per axis, at most 16,641 total samples,
2..32 terraces, bounded spacing/height/world span, an explicit seed, origin,
material slot, and collision-query intent. It preserves a zero-height border,
an exact peak sample, discrete terrace levels, stable bounds, repeatability,
changed-seed divergence, and core-validator rejection. These are data and
contract properties only.

`source_available_local` does not mean public or automatically active. There is
no Site payload, native renderer, project-scene adoption, forest integration,
planetary terrain, streaming LOD, collision solver, generated asset bundle,
server/listener, download, or automatic execution claim. Publication requires
an immutable revision, archive checksum, license evidence, independent build
evidence, and explicit local admission. The other seven Extensions entries
remain descriptor-only.

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
6. Make Forest Factory browse those assets, place scene references, and edit
   placement/configuration overrides without opening Plant Lab internally.
7. Add replay, boundedness, forward/reverse equivalence, placement persistence,
   and Build/Run tests.

Stable authoring documents and semantic operations are canonical. Atlases,
sparse pages, GPU buffers, descriptors, previews, and renderer caches remain
disposable.
