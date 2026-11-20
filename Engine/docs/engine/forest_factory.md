# Forest Factory and Plant Lab

## November2 completion requirements — planned, not runtime acceptance

Plant Lab edits the individual plant; Forest Factory paints procedural regions
on a simplified shaded planet. Show highlighted region boundaries without the
foliage obscuring them. Existing saved region centre/radius, body identity and
body-local anchors can drive these overlays; do not add a second region manager.
Forest Paint now uses the existing terrain preview and admitted region tint.

The operator's relief-globe reference sets the visual direction for Forest Paint.
Keep terrain relief and shorelines readable under the paint. Add a freehand
lasso that previews the region while drawing and shows the committed coverage
as a translucent, terrain-conforming layer with a clear edge. Keep far-side
coverage occluded and make a selected region distinguishable from other painted
areas. Display the saved footprint independently of tree visibility and density.
Keep the existing brush available for local strokes.

Use the existing viewport-to-body surface picking and placement scheduler. A
lasso needs a bounded, closed body-local footprint with explicit validation at
the limb and cube-face seams. Preview stays transient until commit. On commit,
use the ordinary region/history transaction and sample eligible terrain inside
the footprint for placement, with the same shoreline and spacing checks. Save
and regenerate from the footprint rather than visible trees or LOD vertices.
Native acceptance must show the boundary and fill from orbit and close up, then
exercise cancel, Undo/Redo, reopen and placement within the same outline.
Brush tint remains vertex-based and blends with the terrain mesh's existing
colors. Unpainted land and water keep their mesh colors. The OpenGL 3.3 Forest
Paint preview composites lasso fill and edge per fragment on the existing
terrain draw. Its terrain depth keeps the far side hidden. It admits the lasso
only when the view contains one
visible terrain mesh. Other backends retain Brush. A saved lasso is visible
independently of its trees. Hold the left mouse button to trace the visible
surface and release to close and queue placement. Escape cancels the gesture.
Use **Saved lasso** to select an area with or without trees. **Erase Selected
Area** removes its footprint and matching trees in one undoable operation.
Filling or adding species to a saved lasso remains a separate workflow.

Windows Debug and Release CPU checks cover polygon sampling, exact indexed
terrain picking, and a saved, undoable footprint with zero surviving trees.
The exact OpenGL shaders passed offline compilation and linked and rendered
through headless Mesa llvmpipe. The pixel test exercised fill, edge, color
separation and depth occlusion with two controlled terrain colors. The
interactive Editor gesture, full scene appearance and physical-driver
parity remain open.
Proof is retained under
`Artifacts/Reports/forest-lasso-geometry-nov01/`, `forest-terrain-ray/`,
`forest-area-nov01/`, `forest-area-completion-nov01/`,
`forest-paint-coverage-packing/` and `forest-coverage-opengl/`. The Mesa
receipt is under `forest-coverage-opengl/native-mesa-20261117/`.

Separate trunk length and branch length. The operator's eight **Length -**
presses were a shape reference, not an eight-metre change. Current recipe mapping
normalizes height using branch length, thereby rescaling the trunk as well.
The morphology owner already supports separate root and segment lengths. Repair
the profile mapping with saved-asset compatibility rather than changing every
existing plant's generated geometry silently.

Near-tree bark/foliage binding now has an existing atlas path (see Ownership);
verify physical material behavior, cap normals/UVs and native appearance rather
than rebuilding it. Distant voxel rendering remains open: LOD plans and occupancy
estimates do not render voxel trees.

Next sequence and remaining cloud/water acceptance are retained in
`Changes/mission_cache.md`, November2 plant/forest completion gate. No tree
dimension default or saved plant was changed for this planning checkpoint.

## Lightning scene object — November 16

Use **Asset → Add Lightning** or **3D Objects → + → Lightning**, then position
and scale it like a scene object. The normal scene save records its stable
identity and transform. The electrical morphology recipe uses that ID as its
seed. Each strike propagates and fades in 0.30 seconds and repeats every three
seconds. Timeline time zero is near the crest so the effect is visible when
added at the start. Play, pause, reverse or scrub to inspect it.

Editor and project play use the same electrical graph and bounded indexed mesh
projection. The shared scene mesh command draws the bolt with an unlit tint so
its brightness follows decay. Windows Debug and Release CPU checks exercised
scene save/reopen, runtime scene admission, editor marker publication, partial
propagation and repeatable time and seed. Native renderer appearance, planetary
placement and weather-driven triggering remain open.
[Scene and projection proof](../../../Artifacts/Reports/forest-bark-repeat-20261116/f89bc97473a01e0d/result.json)
and [editor publication proof](../../../Artifacts/Reports/preview-contracts-oct31/forest/result.json).

November 17 follow-up: current electrical morphology is already `spatial_3d`,
with radial tube cross-sections, not camera-facing cards. Angle-dependent
flattening/disappearance still needs a paused native camera-orbit check; periodic
pulse extinction alone does not diagnose an angle bug. The electrical graph now
alternates turn angles between successive branch generations, with seeded yaw
variation and enough lateral reach to visibly change direction. Cloud-to-cloud
strikes may stay entirely inside a cloud; increasingly horizontal strikes spread
within a flatter cloud-layer envelope while every bolt retains a volumetric tube
cross-section visible from all angles.

Lightning remains emissive/unlit; make its spatial form readable through jagged
volume, glow and eventual illumination of nearby clouds/surfaces rather than
making the bolt depend on ambient light. Cloud/surrounding illumination is a
separate unimplemented acceptance requirement.

Planned weather ownership: reuse existing storm cells with sparse emitter/zone
objects, deterministic seeds and spatial activation. Strikes recur within the
same sky region; bound active emitters and avoid a dense probe grid or iterating
every planetary emitter each frame. Weather activation, lighting interaction and
native visibility remain unresolved; the fixed three-second preview is not a
completed weather system.

The translation gizmo now anchors ordinary meshes at their transformed local
bottom center and Lightning at its authored emitter origin; planetary bodies
keep their center. This changes only the translation indicator and its extent,
not saved geometry, positions or rotation origins. Imported geometry keeps its
existing local coordinates. General per-model editable/authored pivot metadata,
rotation/scale-pivot controls and native visual acceptance remain open.

## Approved regional forest workflow — November1, not implemented yet

### Whole-planet forest generation — November 17 operator requirement

Planned, not accepted runtime behavior: one visible **Generate Planet** action
should coordinate terrain generation and a complete Forest Factory population
pass. Start from a saved default forest recipe and seed; incrementally cover
every surface region, including the far side, rather than only the camera view.
Use deterministic body-local sampling tiles and bounded batches of rays/surface
queries through the existing terrain provider and scheduler. Show coverage and
placement progress, permit cancellation, and keep editing responsive. Complete
coverage means each tile was evaluated, not that every tile receives trees.

Project candidate roots onto the current terrain revision, then apply biome,
land/paint mask, shoreline clearance, slope, elevation and spacing constraints.
Choose species and bounded size/rotation/shape variation from the recipe and its
seed, so the default becomes a coherent forest rather than arbitrary scattered
objects. Do not add a separate placement or terrain-query system. Progressive
preview may accumulate trees, but the saved forest revision must be coherent;
discard stale work if the terrain changes while queries are outstanding.

Regenerating the same planet re-evaluates and repositions its generated forest
through Forest Factory. Retain region intent, authored plant/material assets
and stable identities where applicable. A new planet/forest seed can deliberately
change the generated distribution and procedural variation; an ordinary surface
refresh must not silently redesign authored trees. Preserve manual placements
and exclusions as a distinct authored layer. Persist seed, recipe, coverage and
terrain revision so save/reopen or cancellation never duplicates completed work.
Publish the completed regeneration undoably, using existing project history.

Acceptance requires whole-body coverage, dry-land/coast/slope checks, repeatable
results independent of batch order, cancellation/resume, stale-revision rejection,
save/reopen, undo and preservation of manual edits. Visually verify one complete
planet, regenerate it with a different terrain seed, and confirm that trees move
onto suitable land and retain distance-based rendering. This extends the regional
workflow below; the current bounded area command does not satisfy it.

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

Freehand strokes and **Fill Painted Areas** keep new roots 24 metres apart.
On ocean terrain, the surface provider checks four points 24 metres around each
root and skips the root if any check reaches water. **Fill Painted Areas**
samples saved paint on the selected body in one undoable operation. In
perspective World view, nearby trees use their authored mesh. Trees below
24 projected pixels use a small silhouette mesh, and trees below 1.5 pixels
leave the render list. Selected trees retain their full mesh for editing.

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

Forest Factory/Plant Lab are intended optional Extensions; extraction is not complete (see Ownership). An ordinary Engine consumer should not require forests, terrain, planets or AI. Terrain providers are interchangeable only through compatible declared capabilities; required dependencies, optional integrations and alternatives must remain distinct. Forest placement must work with an admitted surface provider without forcing a planetary or voxel stack. Preserve existing authoring documents and placements during extraction.

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
Editable typed node UI, sparse voxel rasterization and physical material/native
acceptance remain unfinished; indexed preview mesh compilation exists.

Retained Plant Lab acceptance (November 17, from the August16 review):
branch-start and branch-length edits must regenerate the morphology while
retaining the configured target height. Configured height is authored intent,
not a requirement that final canopy bounds or the crown silhouette stay fixed.
Length edits must also preserve the independent trunk scale described above.
The current `forest.plant_lab_compiler` contract in
`Engine/src/epoch/implementation/epoch.contracts_engine.inl` checks branch-start
edit/compile/history behavior. `Tests/packages/project.forest_materialized_contract.cpp`
checks Length edits, retained configured height/root length/trunk endpoint and
saved-source reopen; `Tests/scene/forest.profile_geometry_contract.cpp` checks
actual branch geometry and retained trunk scale. These test sources were inspected
on November 17; no new execution is claimed here. Native control-to-preview edits
and the saved source/Library/reopen path still need visible acceptance of these
specific invariants. Preserve compatibility and migration requirements above.

## Project Source And Library Authority

`project.forest_library` publishes bounded binary `Assets/Forest/*.epoch_forest`
source (genome/profile, revision/content identity, semantic edit journal) into
immutable content-addressed `Library/Forest` artifacts (preview geometry, voxel
LOD/occupancy identity). Both carry SHA-256 and reject malformed, oversized,
mismatched or corrupt data. Verify/write the artifact before atomically replacing
source. Exact/latest reopen and old revisions survive restart; renderer resources
and preview atlases are excluded.

Materialized v2 can retain the complete compiled tree. `select_materialized` and
`load_selected_materialized` use an 8 KiB reference binding real project root,
exact manifest, source and compiled result. Failed selection preserves previous
selection/source/artifacts. The reference is a hint, never approval: cold loading
regenerates source and requires identical artifact bytes, so Forest generation
remains required. Editor adoption/automatic refresh are unfinished; v1 loading,
old keys and refusal to guess latest v2 stay unchanged. Manifest changes require
explicit reselection; settle stable identity/save ordering before UI adoption.
Only an absent reference under verified ordinary paths means `not_found`; broken
links, invalid parents, corruption and stale records remain errors.

Windows Debug/Release producer/reopen tests cover altered/rehashed files, stale
source, foreign roots, links, limits, failed atomic replacement and unchanged v1
golden bytes: `build/validation/forest-selected-reference-oct27/result.json`.
Linux native-local Debug/Release producer/reopen passes, skipping Windows locked-file
behavior. Cross-platform Windows golden interoperability still fails: differing
CRT cosine changes a tangent/hash. Double precision would alter existing Windows
results too. Keep legacy math/goldens; require versioned portable generation/rebake,
never relaxed integrity. Retained proof/failure:
`build/validation/forest-selected-reference-oct27/linux/result.json`.

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

Preview geometry uses oriented tapered eight-sided branch solids with overlapping
child roots, no internal junction disks, and capped root/exposed tips. Two short
tip taper stages preserve authored nodes/leaf attachments; cap rims have separate
planar normals/radial UVs. This is not a manifold Boolean union. Zero-length or
zero-radius projections are refused. Indexed topology/material slots, leaf-card
orientation, bounds and budgets have CPU contracts; native appearance and
physical material fidelity remain separate acceptance gates.

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

November 17 source inspection (not runtime proof):

| Layer | Current owner / persisted type | Downstream boundary |
|---|---|---|
| Shared morphology | `Engine/modules/authoring/authoring.morphology.ixx` | Bounded graph, temporal sampling and voxel planning; not a forest renderer. |
| Plant profile/document/compiler | `Engine/modules/forest/forest.factory.ixx` | Mainline compatibility/default-asset adapter and compiled forest data; intended optional feature extraction remains unfinished. |
| Mesh projection | `Engine/modules/forest/forest.preview_mesh.ixx` | Indexed branch/leaf geometry, UVs and logical material slots. |
| Source/Library | `Engine/modules/project/project.forest_library.ixx`, `Engine/src/project/project.forest_library.cpp` | `Assets/Forest/*.epoch_forest` → immutable `Library/Forest/*.epoch_forest_artifact`; v2 hint `selected.epoch_forest_reference` obeys validation described above. |
| Editor placement/materials | `Engine/src/editor/application/editor.application_scene_preview.inl` | `project_compiled_forest_entities` builds an `asset::mesh` snapshot; `prepare_plant_material_atlas` loads exact bark/foliage texture artifacts and `bind_plant_preview_material` binds the composed atlas. Geometry carries UVs plus tint, not colors alone. |
| Runtime consumption | `Engine/src/epoch/implementation/epoch.project_play_scene.inl` | Existing mesh/texture residency and shared scene draw path; renderer resources are disposable, not authoring authority. |

Plant Lab owns authoring; Forest Factory browses and places stable asset references.
Mainline retains shared morphology/temporal/voxel/asset validation contracts.
Heavy optional generators belong in `Engine/dep/EpochEngineExtensions`, but the
forest modules above still reside in mainline: do not describe extraction as done.
Package Registry owns activation/provenance/integrity/approval; generated projects
receive optional payload only after visible activation or main-scene use.

Evidence is scoped: `Tests/packages/project.forest_materialized_contract.cpp`,
`Tests/scene/forest.profile_geometry_contract.cpp` and the receipts above cover
specified CPU/data behavior. They do not prove every backend's textures, shadows,
voxel LOD, planetary placement or end-to-end GUI workflow. Regional and whole-planet missions remain open.

## Next Production Slices

1. Complete stable Plant Lab tree/configuration documents and semantic operations.
2. Finish typed node UI with stable node/pin/connection IDs, validation,
   serialization and migration.
3. Separate canonical documents/compiled artifacts from disposable atlas,
   sparse-page, GPU-buffer, descriptor, preview and renderer caches.
4. Add deterministic regeneration independent of time-only sampling.
5. Rasterize samples into `SparseVoxelField` LODs; complete mesh/material/impostor outputs.
6. Browse/place logical assets and edit scene/configuration overrides through
   Forest Factory without embedding Plant Lab.
7. Verify replay, bounds, forward/reverse equivalence, persistence and Build/Run.
