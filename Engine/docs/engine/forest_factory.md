# Forest Factory and Plant Lab

## November2 completion requirements — planned, not runtime acceptance

Plant Lab edits the individual plant; Forest Factory paints procedural regions
on a simplified shaded planet. Show highlighted region boundaries without the
foliage obscuring them. Existing saved region centre/radius, body identity and
body-local anchors can drive these overlays; do not add a second region manager.
The present painting toggle only adds controls over the ordinary scene.

Separate trunk length and branch length. The operator's eight **Length -**
presses were a shape reference, not an eight-metre change. Current recipe mapping
normalizes height using branch length, thereby rescaling the trunk as well.
The morphology owner already supports separate root and segment lengths. Repair
the profile mapping with saved-asset compatibility rather than changing every
existing plant's generated geometry silently.

Near trees need actual bark texture bindings. Existing mesh UVs/material slots
are flattened to colors by `project_compiled_forest_entities`; use the existing
project texture controller and persisted texture/material snapshot. Resolve bark
versus foliage atlas/submaterials and proper cap normals/UVs. Distant trees should
use bounded voxel representations through the existing voxel core; current LOD
plans and occupancy estimates alone do not render voxel LOD.

The same branching graph should also support a **lightning** consumer without
leaves. Reuse graph generation, but give lightning its own strike/decay and
material behavior, driven by the central time system. This effect is requested,
not implemented. Its acceptance includes deterministic seed/time replay,
pause/reverse, project persistence and visible output.

Next sequence and remaining cloud/water acceptance are retained in
`Changes/mission_cache.md`, November2 plant/forest completion gate. No tree
dimension default or saved plant was changed for this planning checkpoint.

## Approved regional forest workflow — November1, not implemented yet

Paint forest regions, rather than managing every generated tree in 3D Objects.
Show the planet and its named regions in the hierarchy. A region owns its seed,
species mix, density and biome rules; individual generated trees are disposable
instances, with deliberate conversion to editable objects only when requested.
Keep existing authored plant assets and saved placements readable during migration.

The painting view should show the planet's simple shape and highlighted regions,
without dense foliage hiding the selection. Support selecting an area, then
assigning or erasing a forest, like selecting an area in a strategy game. Plant
Lab still designs individual trees; Forest Factory composes and distributes them.
Green/wet areas are candidates, not a universal rule: biome, altitude, slope,
moisture and coast constraints determine where each composition can grow.

Region coordinates belong to the terrain body. Sampling, gravity alignment,
streaming and display must follow that body's current transform and terrain
revision. Moving a planet must move its forest. Scene/project history should own
region edits, including undo/redo and save/reopen; the renderer owns only generated
instance residency. Reuse the existing forest assets, placement descriptors,
planetary surface queries and scheduler. A list of ordinary tree scene objects
does not satisfy this target. The bounded implementation below remains prototype
evidence until this regional workflow and native acceptance are complete.

## November1 bounded planetary area command — native acceptance pending

Forest Factory now offers explicit **Paint Area** beneath the selected planet's
current scene camera, using the exact current Plant Lab asset. The individual
tree-design preview retains its own upright camera. **Paint Forest on Terrain**
switches to painting controls over the ordinary planet scene; returning to plant
design restores its independent camera. Plant Lab editing and terrain painting
are separate views, not a shared rolled camera or another complete editor.
Radius in metres,
density per square metre and seed produce a deterministic spherical-cap
candidate set, capped at64. The existing cancellable Editor scheduler prepares
one real rooted indexed plant mesh, reuses an exact immutable terrain artifact
or loads it on that worker, and submits one identity-bound batch surface query
through the admitted planetary provider. It does not launch one child per tree.
Underwater candidates are skipped. Current placement aligns to radial gravity;
no slope suitability or foliage/material acceptance is claimed.

Each accepted tree is an ordinary persistent ForestFactory scene object, sharing
the published mesh asset and carrying its region identity, source/recipe/body,
selected plant revision, distribution seed/radius/density and candidate index.
Scene format8 additionally records body-local root/up, admitted body centre and
mesh origin doubles; existing formats1–7 remain readable and unchanged when the
new fields are absent. The existing scene document, content hash and semantic
history own these fields. Paint is one creation transaction; **Erase Selected
Area** removes the selected tree's region in one transaction, with ordinary Scene
Undo/Redo. There is no region proxy or second scene manager. Source changes,
project changes, cancellation and invalid surface results refuse scene mutation.
Existing mesh cache/index/publication limits remain authoritative; excessive
areas are refused rather than raising renderer budgets.

CPU Debug/Release proofs cover deterministic cap distribution, genuine branch
and foliage geometry, batch provider admission and64samples in one process,
actual completion stale/cancel guards, grouped create/erase history, format8,
Save/Reopen, runtime metadata and legacy codec regression. These do not prove
moving-body presentation. One native area command created and saved five actual
tree instances, with their region metadata present in the saved format8 scene.
The operator rejected its mixed design/painting camera; the corrected view split
passes actual camera/UI CPU checks and still needs rebuilt native acceptance.
The existing runtime presentation applies body-local roots against the current managed body and
terrain transform; authoring anchors/history remain unchanged. Native saved
project Paint/Erase/Undo/Redo/reopen/Run acceptance stays open until explicitly
observed. Native Undo/Redo/reopen/Run remain open. Freehand strokes, mixed species composition, grass, vegetation
instancing/LOD/streaming and optional compiler extraction remain separate work.

## Composition and remaining work

The operator requires a working planetary simulation **with** a working tree
and forest system: author individual tree definitions in the existing Plant
Editor/Plant Lab, compose reusable procedural forest presets from those assets,
then paint forests onto terrain. Retain the inline editing workflow; painting
does not require opening another complete Plant Lab window. This connected
mission is open. Single-tree preview/Add acceptance does not complete it.

Current implementation provides `ForestAssetDocument` edits and Undo/Redo in
`forest.factory`, indexed tree mesh generation in `forest.preview_mesh`, and
canonical source/compiled publication in `project.forest_library`. The existing
`editor.application_scene_preview.inl` placement path publishes the plant and
mesh, then commits through the ordinary scene transaction/history owner.
The older native Add/save/reopen receipts remain bounded placement evidence.

Flat scenes retain their original placement path. Planetary placement now queries
the admitted terrain provider. The batch path resolves a bounded area in one
process; its results remain tied to the exact project, recipe, source and body.
This replaces the old flat-grid fallback for selected planets.

The existing `ForestPreset` enum selects individual plant morphology. It is
not a persisted forest composition referencing several authored tree assets.
The bounded area command is not a freehand brush or a vegetation streaming
system. Morphology voxel-LOD plans and occupancy summaries are
generation data, not accepted near/far forest streaming. Optional provider
extraction, physical foliage materials and cross-target generation compatibility
also retain their existing open gates.

The next acceptance step is native placement and area painting on the saved
planet, followed by Undo/Redo, close/reopen and Run. The runtime now projects
body-local roots against the current managed body using double precision;
changing the viewed anchor does not rewrite authored placements. Stale region
metadata keeps the authored pose and reports that repainting is needed. These
paths pass CPU tests; their visible combined behaviour still needs acceptance.

Subsequent forest presets must reference published tree identities and own
composition, density/distribution, seed and variation. A bounded painted stroke
must enter the existing document/history authority as one semantic operation,
support erase and deterministic replay, and retain placements across regeneration,
reopen and runtime. Terrain/body-local anchoring must survive planetary LOD
changes; vegetation cells, instancing/LOD and streaming must use existing resource
admission and task owners. Activate Forest and the selected terrain provider
through normal project package dependencies, retaining optional boundaries and
ordinary projects without either package. Manual/imported assets remain usable
without AI. Native acceptance requires painting an actual forest on the planetary
world, editing its source definitions/preset, Undo/Redo, save/reopen/runtime and
stable near/far traversal within declared limits; none is inferred from this audit.

## October27 smooth preview repair — local source

Forest's editor preview now uses its indexed branch mesh and oriented leaf
cards, replacing the capped preview made from branch pieces and box-shaped
leaf clusters. This display is not a voxelized forest. Optional voxel data
remains a separate output contract and does not choose the visible tree shape.
The in-editor controls edit the plant before explicit Add to Scene.

The restored `forest.preview_mesh` owner builds a content-addressed
`.epoch_mesh` through existing asset publication. Actual Debug/Release tests
cover dense trees, invalid input refusal, placement publication and cold
save/reopen. A native tree was visible while the operator edited branches.
Full original-demo fidelity, physical materials/textures and moving the sole
compiler owner into the optional extension remain open; no duplicate module
definition may be added during that extraction.

## October26 optional integration and generated textures

Forest Factory and Plant Lab retain their distinct authoring/placement roles, but their feature implementation is an optional add-on. An ordinary Engine consumer does not require forests, terrain, planets or AI. Terrain providers are interchangeable only through compatible declared capabilities; required dependencies, optional integrations and alternatives must remain distinct. Forest placement must work with an admitted surface provider without forcing a planetary or voxel stack. Preserve existing authoring documents and placements during extraction.

Engine-owned optional text-to-2D generation supplies tree/foliage textures as the first concrete image-generation use case before 3D model generation. Forest Factory consumes approved texture assets through the existing material/asset pipeline; it does not own a second model manager. Manual/imported textures work without models. Generation and its runtime validation are still unimplemented acceptance work.


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

The newer materialized v2 record can also store the complete compiled tree.
The library now has explicit `select_materialized` and
`load_selected_materialized` operations. Selection happens after publication;
failure leaves the previous selection, source and artifacts untouched. Its
8 KiB reference stays under the existing `Library/Forest` owner and binds the
project's real root, exact manifest bytes, source and compiled result.

That reference is a hint, not approval. A cold load rebuilds from saved source
and requires the exact same artifact bytes before returning it. This still
runs Forest's generation code. It does not make Forest Factory optional or change v1
loading, old keys, or the refusal to guess a latest v2 artifact. The Editor
still uses v1; v2 host adoption and automatic refresh are not implemented.
A manifest change requires explicit
reselection; the existing project owner must settle stable identity and save
ordering before UI adoption.

Only a missing reference file beneath verified ordinary project/source/Library
paths returns `not_found`. Broken links, invalid parents, corrupt files and
stale records remain errors; callers must not treat them as a missing tree.

The actual library passes Windows Debug/Release tests with separate producer
and reopen processes, altered/rehashed files, stale source, foreign roots,
links, size limits and a real failed atomic replacement. The prior v1 golden
bytes still match. Proof: `build/validation/forest-selected-reference-oct27/result.json`.

Linux Debug/Release also pass separate **native-local** producer/reopen tests;
the Windows locked-file test is explicitly skipped there. Windows artifact
interoperability still fails its unchanged golden test. The CRTs produce a
different cosine value, changing one tree tangent and its saved hash. Changing
all cosine calls to double precision would also change some existing Windows
results, so legacy math and golden files remain untouched. A versioned portable
generation/rebake policy is still needed; do not relax verification to hide this.
Proof and retained failure: `build/validation/forest-selected-reference-oct27/linux/result.json`.

## Forest Factory Placement Portal

Placement requires an open, selected project with a project root. A fresh
standard Editor may browse the portal, but placement cannot populate an
unbound workspace or implicitly turn it into a project.

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
oriented tapered eight-sided solids. Child roots overlap inside their parent
trunk. Internal junction disks are omitted; only the root base and exposed
terminal tips are capped. Exposed tips use two short taper stages beyond the
authored endpoint; authored nodes and leaf attachments remain unchanged. Cap
rims have separate planar normals and radial UVs.
This is overlapping branch geometry, not a smooth manifold Boolean union.
Invalid zero-length or zero-radius
projections are refused. The
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
