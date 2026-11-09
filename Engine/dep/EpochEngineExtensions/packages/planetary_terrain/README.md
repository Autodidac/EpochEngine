# Planetary Terrain

<!-- SPDX-License-Identifier: LicenseRef-MIT-NoSell -->
<!-- Copyright (c) 2026 Adam Rushford -->

This optional package contains a renderer-neutral indexed cube-sphere meshing
core and the existing small biome/tile sampler. It adds no mandatory Engine
startup service, model load, renderer, viewer loop or worker pool.

`EpochEngineExtensions::PlanetaryCore` exposes `import planet.terrain;` and the
`epoch::extensions::planetary_terrain::core` namespace. This is an independently
consumable C++23 **module facade over one upstream ABI**, not a completed
conversion of the upstream implementation into module units. The unchanged
`world.cpp`, `terrain.cpp` and three headers remain the only algorithm owners.
The original `epoch_extension_planetary_terrain` sampler target/header stays
compatible and separate.

The selected EpochPlanet0.2.3 owners implement metric body-local indexed tile
meshes, parent-triangle geomorph deltas, bounded tile/LOD planning and stop-token
cancellation. Positions and tile anchors use doubles until the vertex anchor is
subtracted. A renderer computes `body_center + tile_anchor - camera_position` in
doubles; uploading absolute planetary positions as floats breaks this contract.

The upstream subset is under `upstream/EpochPlanet-0.2.3`; exact source hashes and
exclusions are in [provenance](upstream/provenance.json). These newly-written
replacement owners retain their [upstream MIT notice](upstream/EpochPlanet-0.2.3/LICENSE)
and [third-party/provenance notes](upstream/EpochPlanet-0.2.3/THIRD_PARTY.md), despite
the enclosing package's separate Epoch terms. No grant is inferred for the
excluded legacy archive, derivative voxel mesher or its interface. Those files
are absent. No third-party binary or dependency source is bundled.

Configure this package directory with a C++23 module-capable CMake toolchain,
build `epoch_extension_planetary_core_contract` and run CTest. The independent
module consumer checks actual mesh indices, bounds, metric/double anchoring,
cancellation and invalid input. No Engine or graphics dependency is required.
Recorded build results belong to the owning acceptance receipt; source presence
alone proves neither a successful module build nor native rendering.

The package script still describes a human-approved install/build/preview plan.
The Engine's source-package installer, project activation and native mesh
presentation remain separate unfinished gates. A CPU mesh or a `livePreview`
manifest flag is not an actual planetary scene inside the Editor.

`import planet.coverage;` now generates actual bounded CPU meshes for the
requested progression:

| Setting | Geometry produced |
| --- | --- |
| Classic | One flat grid with sampled terrain heights |
| Tiled | A configurable grid of adjoining terrain patches, up to16x16 |
| Hemisphere | The northern half of one curved body, with an open equator |
| Planet | Six joined cube-sphere faces covering one body |
| Universe | Separate body identities and double-precision origins for the supplied world |

The caller chooses a seed, body, sample density, tile size and memory/patch
budget. Admission rejects an oversized request before generating meshes.
Cancellation returns no partial result. Shared planar edges use the same global
samples and normal stencil. Existing planetary algorithms remain single-owned in
the reviewed upstream subset. There is no new worker pool or renderer.

`epoch_extension_planetary_coverage_contract` checks all five forms, adjoining
grid edges, metric spacing, indices/normals/bounds, body identities, admission,
cancellation and invalid inputs. This is finite mesh generation, **not** streamed
world residency, collisions, editable terrain, project persistence or a completed
Editor preview. Those integrations still require their own acceptance.

An optional offline producer uses Engine's existing mesh-artifact codec. Configure
with `EPOCH_PLANET_BUILD_MESH_EXPORT=ON` and an explicit `EPOCH_ENGINE_SOURCE_ROOT`,
then build `epoch_planet_mesh_export`. Run it with a coverage name, a new output
directory and an optional unsigned seed. It writes `terrain.epoch_mesh` and
`terrain.preview.json`; existing directories are refused. No Editor, renderer,
model or network process starts.

This artifact is a normalized overview. Planet mode uses radius10 plus its
sampled terrain relief, matching the streamed patch scale. Other modes fit
within10 preview units of their origin.
It is clearly marked `preview_only`; it is not a metric world save. Its recipe
retains the generation settings, meter-to-preview scale and double-precision
body/tile origins. Conversion subtracts the preview origin in doubles before
casting local vertices to floats. The standalone core remains independent of
Engine because this consumer is off by default.

`import planet.streaming;` adds camera-driven terrain detail and a bounded set
of loaded patches. Moving closer requests smaller patches. Moving away releases
unneeded detail. The existing `plan_terrain`, `build_tile` and `balance_cut`
algorithms still own those decisions and the geometry.

The caller owns `streaming::State` and its job queue. Call `update` with metric
camera coordinates, viewport height, vertical field of view and memory limits.
`next_work` reserves retained and scratch memory for each independent patch.
`maximum_in_flight` defaults to1 for existing callers and explicitly admits up to4
claims. Copy the body and Work on the owner, call `build(body, work, stop)` on an
existing worker, then return each result through `complete` on the owner. The
`build(state, work)` convenience overload remains available for serialized callers.
Retire execution and discard result buffers before `abandon` releases failed or
cancelled claims. Every outstanding identity remains reserved across view changes;
old or cancelled jobs cannot replace a
newer camera plan. `draw_cut` returns a complete, balanced set of resident
patches; a parent stays visible until all four replacement children are ready.
The first body becomes visible only after its six root patches are available.

Memory admission covers mesh storage, every admitted meshing job and bounded planning
metadata. Oversized plans are refused before replacing the current terrain.
This module creates no threads, scheduler, renderer, model or network service.
The optional supervised producer reuses Engine's maintained Scheduler/TaskGraph,
with clamp(hardware/3,1,4) workers building distinct admitted patches. Completion,
cache writes and snapshot publication stay on its owner. Errors cancel and retire
all outstanding jobs before returning. This is CPU production parallelism; it does
not add smooth LOD transitions or a live cross-process worker-count protocol.
Its current planner budgets all six supplied bodies before selecting the
requested body, so `LodSettings::max_leaves` must be at least36. Six roots per
other body consume30 of that budget. This is a single selected-body stream;
ocean geometry, collisions, editing, vegetation and a streamed universe are
separate integrations.

The optional producer also accepts these supervised step commands:

```text
epoch_planet_mesh_export --stream-step new-output cache seed cells body cx cy cz height fov max-level max-leaves bytes max-new request-id
epoch_planet_mesh_export --stream-step-preview new-output cache seed cells body cx cy cz height fov max-level max-leaves bytes max-new request-id
```

The first form uses metric world camera coordinates. The second uses body-local
overview coordinates where the body's radius is10. The producer performs the
double-precision conversion itself. `fov` is in radians. `max-new` permits1â€“32
new patches in this step. The artifact codec's vertex/index limits also apply.

The reusable cache holds patch geometry and the previous LOD plan, preserving
detail hysteresis between steps. It is tied to one seed, cell count and body.
Each step checks that identity, validates cached meshes, removes obsolete owned
patches and recovers interrupted patch writes. Unknown files and redirected
paths are refused. Repeated steps reuse cached geometry without compilation.
The caller should coalesce camera changes and retire its completed output
folders after consuming them; output folders are immutable and never replaced.

A completed step writes `terrain.stream` last. This small ordered receipt starts
with `epoch.planet.stream.v1`, then reports `request_id`, `snapshot_ready`,
`built`, `reused`, `resident`, `pending`, `draw`, `min_level`, `max_level`,
`resident_bytes`, `maximum_bytes`, `meters_to_preview_units`, `mesh_file`,
`content_key`, `vertices` and `indices` as `name=value` lines. A ready snapshot
also contains the validated `terrain.epoch_mesh` and `terrain.preview.json`.
Its scale stays at10 divided by the body radius while detail changes.

The independent streaming consumer checks real far/near detail transitions,
complete coverage, balanced edges, memory limits, eviction, cancellation and
stale job rejection. The producer contract checks repeated native steps, cached
reuse, mesh bytes, corrupt/foreign cache refusal and interrupted-write recovery.
These tests prove the provider and its offline transport. Editor camera updates,
project saves and runtime presentation require separate Engine acceptance.

`import planet.exploration;` exposes the original camera and travel algorithms
without starting the original viewer. The caller owns the `Simulation` value,
supplies input and advances one bounded tick. It can select a destination,
enter orbit, land on a solid body, cruise, walk on the sampled surface, or fly
with optional gravity and horizon leveling. The existing six-body world includes
the moon SELENE orbiting VERDANT. The original double-precision camera keeps its
forward/up frame while its carrier moves.

`exploration::advance` checks cancellation before the tick and rejects invalid
time/input without changing the session. The module does not export or invoke
the original `FixedClock`; it creates no time loop, scheduler, thread, renderer
or window. `SessionState` can be validated and restored in memory. Project-file
saving must use the host's existing persistence and admission rules; the original
viewer's separate save-file implementation is not included.

Build and run `epoch_extension_planetary_exploration_contract` for the independent
CPU checks. These cover real orbital time, parent-relative moon motion, camera
frame interpolation, cruise/landing/walking/gravity, state restore and cancelled
or invalid ticks. Editor controls, optional project runtime wiring, moving-body
presentation and save/reopen remain separate integration work. This foundation
does not make the current normalized terrain overview the full original demo.

The optional `EPOCH_PLANET_BUILD_RUNTIME_CONTROLLER=ON` build produces
`epoch_extension_planetary_runtime` against an explicit `EPOCH_ENGINE_SOURCE_ROOT`.
This is a linked project controller, not a second application loop. Its startup
bridge calls the Engine's normal project/package admission before registering a
controller for that exact project, source hash, edition and seed.

Each controller keeps the original simulation and one bounded fixed-step
accumulator. The host supplies movement, look and commands after its GUI input
checks. The controller returns a double-precision camera, orbital time and body
positions. The admitted binding carries the authored Planet/Universe choice.
Planet builds and emits only the selected body, with selection, gravity,
collision and automatic carrier changes confined to that body. Universe retains
all six coarse terrain meshes, including SELENE. The host owns presentation and
replacing an active body's mesh with streamed detail. No process is launched for
each frame.

The session codec records the original camera, motion, mode, assists, target,
orbital time and partial tick. It excludes the absolute project path so a complete
project can move. Malformed or mismatched project/world/source/seed data is
refused without changing the previous session. Planet uses the scoped v4
session envelope; Universe retains its v1-v3 codecs. A session cannot cross
between those scopes. Older attached providers require explicit source review
and update before Engine can run a selected Planet. The host still owns atomic
project-file saving. Run `epoch_planet_runtime_controller_contract` for the
standalone CPU checks; its host-admission port is a test fixture. Actual Engine
startup, input controls, visible moons and session save/reopen need their own
integration acceptance.

## Optional cloud-density transport

The offline exporter accepts `--cloud-density-v1` only when explicitly selected by
its host. This writes a v5 planetary snapshot containing a bounded analytic cloud
density recipe instead of a cloud-lobe mesh. `--cloud-layer-v1` retains the v4
mesh transport for existing consumers. The source build needs the Engine's
`asset.cloud_volume` module and v5 snapshot codec prepared for 0.90.185; it cannot
build this route against the older 0.90.183 compatibility baseline.

`planet.clouds` owns the metric recipe, and the host owns rendering, depth,
lighting and resource admission. The recipe retains deterministic seed, wind and
simulation time. It does not provide editable voxel storage, weather simulation
or permission to remove terrain hidden by clouds. The optional runtime controller
returns the same density description; it does not create a renderer or worker pool.

The CPU transport test exports and reads both v4 and v5 snapshots through the
actual producer and codec. Native OpenGL appearance, occlusion, performance,
resize and other renderer support remain separate Engine acceptance gates. A
saved project needs explicit reviewed package-source update and regeneration to
receive this transport; existing scenes and package pins are not silently changed.
