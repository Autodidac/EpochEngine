# Renderer Regression And Evidence Plan

## Purpose

This plan defines the faithful runtime evidence lane for Epoch's seven baseline
Canvas2D providers: OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, and
Software. It does not promote a backend because it compiles, opens a window, or
survives a no-display probe.

The renderer-neutral scene, project artifacts, viewport policy, and T0-CPU
reference image are authoritative. Backend adapters own only physical upload,
sampling, composition, capture, and teardown.

## Evidence Layers

### Linux checks on GitHub

The Public workflow builds the complete Engine with the pinned Clang toolchain.
It runs the compiled component tests, not just the two top-level engine tests.
The separate `epoch_linux_<backend>_smoke` tests open the real Editor under Xvfb.
The matrix includes the compiled OpenGL, SDL, SFML, Raylib, Vulkan and Software
providers. OpenGL-based providers use Mesa CPU rendering; Vulkan selects Mesa
lavapipe explicitly. A missing CPU Vulkan driver fails rather than using a GPU.
Each test must report the requested provider ready, finish the test frames,
and close normally within 30 seconds. A clean process exit without those events
fails. Logs, executable hash, elapsed time and sampled process memory/CPU stay
under `build/validation/linux-graphics`.

Enable this test locally with `-DEPOCH_ENABLE_GRAPHICS_TESTS=ON`, build normally,
then run `ctest --test-dir <build-directory> -L graphics --output-on-failure`.
It is off by default; ordinary contract checks do not open
windows. The release packager repeats the smoke against the copied runtime and
checks its shared libraries and archive contents. Generated test logs are kept
outside the archive.

This checks real software rendering and shutdown. It does not check a physical
GPU, mouse/keyboard behavior or matching pixels across renderers. Those need the
native and image-comparison checks below. DirectX is a Windows backend, not a
missing Linux graphics test.

Run the cheapest faithful layer first:

1. C++23 module and contract build;
2. deterministic T0-CPU raster and artifact hash;
3. backend startup and bounded frame completion;
4. native capture and reference comparison;
5. resize/minimize/restore;
6. whole-editor context replacement and resource retirement;
7. saved-project Build and external Run;
8. bounded soak and operator eye evidence.

Headless CI is a canary, not renderer presentation proof. GUI/GPU runs remain
operator-approved and launch only from an asset-bearing output directory.

### OpenGL terrain washout capture

Launch the exact test Editor with a private showcase data root and set
`EPOCH_OPENGL_PREVIEW_SAMPLE_RADIUS=1.000841` in that process. This is the
camera distance from the planet center divided by nominal planet radius. The
recovered orbital pose uses `1.275792` in a separate launch. Select the intended
draw mode, hold the target pose and wait for terrain streaming to settle. The
probe needs an admitted density layer with an inner terrain radius. It takes
one frame per process within 0.0002 of the target ratio after a 750 ms dwell in
the same context and viewport. The camera must stay within 2 m and 0.25° of
the starting pose. Small field-of-view changes, clip-plane refits and geometry
republication do not restart the dwell. A gap of more than 250 ms between
observations restarts it.

Keep the executable hash, data-root receipt, terrain stream request and
`OpenGL.PreviewPass` log together. The log confirms flag parsing, reports the
first missing-layer or out-of-band observation, and records entry into the
target band. The captured frame has its own revision and measured ratio.
Compare the five points by their OpenGL
pixel coordinates. Each point records RGBA and depth after clear, plain opaque
meshes, textured meshes and sampled surfaces, cloud composition, and scene
helpers. Compare each sample to its clear value. A change at the cloud
checkpoint isolates cloud composition. A pale color or missing depth before
that checkpoint leads back to opaque scene data or drawing. Terrain and water
can share one mesh, so the opaque checkpoint cannot identify either one on its
own. The probe requires a single-sample depth target and records pixels without
altering pass order or presentation.

### Verdant cloud passage

Keep one admitted project, body and frozen weather time. Record one verified
Play window while the provider moves the camera from below the first deck to
above the full cloud shell. Keep the original recording and the saved
body-local camera checkpoints. Re-evaluate the shared density field at those
checkpoints and along the travelled axis. Confirm that the route crosses a
dense body and that the clear points are outside it in three dimensions.
Use fixed crops from the recording to compare perceived detail. State the
video-clock uncertainty when matching a frame to a saved checkpoint. A
stationary pass probe can identify the field and the composite, while motion
and coverage come from the continuous take and the camera receipts.

For a lighting-only change, replay both shader versions at the same density
inputs and compare accumulated alpha. Match native crops by body-local
altitude and camera direction. Record mean luminance, interior contrast, and
ground and orbit frames. Measure actual frame cost separately from shader
sample counts.

For a lateral shape change, record after the first density pass. Reuse the
same body, weather, starting direction and flight input. Align captures by
distance along the actual saved route and report altitude drift. Include
clear air at both ends and at least one dense, contiguous interval. Compare
the approach and exit silhouettes as well as the interior.

A lower-deck profile change needs separate coastal, ground, in-cloud and
orbital checks. A reduction in coastal bands is insufficient if the same
cloud loses opacity during passage. Apply the same screen region and
luminance filter to replay and native output. A synthetic skyline and
analytic occlusion can still reverse the result. If they do, capture
the scene-specific shader parameters, opaque color and depth, and resolve
output at the saved camera before using the replay to select a variant.

### SDL preview colors

The shared frustum clip interpolates optional per-vertex RGB at each clipped
edge. SDL submits those colors through its existing indexed fan. Software
keeps its position-only depth and wire paths. The focused SDL fixture checks
clipped and unclipped input, winding, flat colors and complete rejection.
On Android, check the baked shoreline on SDL OpenGL ES. The SDL triangle
color varies in screen space. A baked shoreline verifies gradients separately
from animated water and terrain streaming. The runtime control reports live
water availability. A disabled control can accompany a visible baked shore.

### Verdant shoreline draw evidence

For a close saved start, stream from the saved camera while the viewport
shows preparation. Do not draw the provisional cut with that camera.
The first visible close frame must contain the detailed terrain and its
fitted camera from one scene commit. Check the HUD, ordinary orbital and
LOD-disabled starts, and the pause, failure and timeout exits. Use the
same coastal pose and crop on both native backends for the horizon-strip
comparison. Verify an actual detailed commit so a blank frame cannot pass.

A retained patch can have an older wave clock and a different metric origin.
Replay the saved old and incoming hierarchies, including intermediate parents
and dry neighbors. The worker must use the incoming wave sample and an exact
provider lattice wherever the old packet lacks one. Check both endpoint
identities, refusal on stale source or budget and the settled cut before
counting any intermediate draw in the native test.

For a merge, enforce 2:1 on each positive-length shared edge, including
reversed cube-face charts. Finer leaves on the far side of an adjacent
tile and corner-only contacts do not restrict the merge. Measure within
each prepared batch and at its boundary separately. The next batch may
start after the last intermediate commits. Its first frame still waits
for the previous phase's display hold.

Rolling wet batches retain the approved Planetary Terrain and FFT Ocean file
inventories. Before and after preparing a batch, verify the exact saved project
and recipe, both local package receipts, the current inventory, and the SHA-256
of each approved file. Check a same-size edit with its timestamp restored, and
stop during the final verification. Either must discard the prepared batch and
preserve the previous terrain.

Set `EPOCH_TRACE_WATER_FRAMES=1` for the isolated project process when
running the retained shoreline route. OpenGL and Vulkan record the immutable
water packet at their solid draw commands. The log includes the terrain
request, mesh key, water appearance and shader range signatures, CPU sample,
live ripple phase, camera eye, and clip interval. A delayed geometry commit
keeps its own terrain request and sample while accepting the latest view.
The trace is off by default and records at most 2,048 draw packets per process.

For a staged shoreline transition, pair the `Engine.Planet` log's request,
frame count and wave sample with each intermediate draw packet. A native run
can settle successfully without taking this path. With the same trace
enabled, the `Shoreline input` and `Shoreline preparation` rows also show
whether a cut had verified wave input and how the worker disposed of it.
Check that the transition admitted intermediate cuts before using the
capture to judge them.

A long shoreline move can span several worker batches. Check each request's
preparation outcomes and the sequence of distinct backend draw artifacts at
its fixed wave time. Play holds each geometry for 50 ms after its shared
scene commit. The draw trace shows which phases the backend actually draws.
Confirm the final cut commits before the request advances and verify that
cancelled intermediate geometry does not publish after rollback.

Run both backends with the same verified source, saved pose, clock and frame
cap. The retained controller route caps rendering at 60 FPS and records the
window at 30 FPS. Compare changes in the packet with the visible shoreline.
The trace time records when the CPU issues or encodes the draw. Screen
recordings can corroborate continuity. Precise frame attribution requires a
capture tied to the same draw packet. Retain the linked executable, route and
capture receipts for that comparison.

### Retained native lifecycle acceptance details

November 17 reconciliation preserves these narrower historical gates; the
following are acceptance requirements, not new test or runtime pass claims.

- Vulkan: retain the v177 30-second native Release flicker scenario. Unchanged
  framebuffer size and transient `SUBOPTIMAL` must not recreate the swapchain or
  flash; real resize and `OUT_OF_DATE` must recover. Inspect recreation logs.
  See `Changes/v177-vulkan-swapchain-flicker.md` and the source-path fixture
  `Tests/ai/test_epoch_90177_vulkan_swapchain_flicker.py`; that fixture alone does
  not accept native presentation.
- Windows Vulkan Release `D952D2D80E9C` passed the ordinary Editor Play guide
  transition with the first-person starter and with a two-object scene whose
  camera mode stayed Editor. The grid returned after each Controls Return.
  [Same-camera captures](../../../Artifacts/Reports/windows-release-play-guides-1119/fixture-10eaeefd/play-result.json).
- OpenGL: make the retiring context current for its context-bound resource
  cleanup and verify that sibling-context resources survive. The scoped binding
  in `Engine/src/renderers/opengl/opengl.context_cleanup.cpp` is implementation
  evidence, not an approved native multi-context teardown result.
- Raylib: exercise off-owner and wrong-current deferred deletion, drain on the
  owning context/thread, and verify that cleanup never restores a deleted native
  context. `Engine/modules/raylib/raylib.context.ixx` guards restoration after
  native context destruction; that source guard does not prove the deferred
  deletion error paths or native repeated-switch acceptance.

Vulkan repeated native retirement/reopen is a separate open soak gate from the
30-second flicker scenario: record the exact Release image, cycle workload and
initial/per-cycle/final native memory and handle observations; repeatedly retire
and reopen/replace the context, verify resources retire without accumulating
across cycles, and confirm normal owned shutdown. Passing unchanged-size or
resize presentation alone does not close this lifecycle gate.

## Canonical Scene

Every backend consumes the same saved acceptance scene and immutable Canvas2D
resource closure. The diagnostic frame contains:

- asymmetric corner and axis markers that expose vertical/horizontal inversion;
- opaque, premultiplied-alpha, additive, and cutout samples;
- nearest and linear sampling regions using the same authenticated texture;
- non-square sprites with rotated/flipped UVs;
- overlapping layers with stable object IDs and explicit order;
- a sparse tilemap chunk boundary, animated tile, collision marker, and empty
  chunk;
- letterbox bars and one-pixel content-edge markers;
- a controllable animated actor and one deterministic audio cue in the complete
  project lane.

No backend may construct its own lookalike scene. Captures include the source
scene revision, compiled artifact revisions, backend epoch, viewport mapping,
and frame identity.

## Pixel Policy

- Nearest-sampled, opaque/cutout, orientation, bars, and stable-order regions
  compare exactly against T0-CPU after origin/stride normalization.
- Linear-sampled and alpha-composed regions use an explicit bounded per-channel
  tolerance and maximum mismatched-pixel ratio recorded in the manifest.
- A tolerance is never widened automatically after failure.
- Comparisons exclude editor chrome only through the shared content rectangle;
  arbitrary crops are invalid evidence.
- Missing captures, zero-sized captures, stale frame IDs, or a fallback image
  fail the scenario.

## Required Matrix

Each of the seven providers must complete:

| Scenario | Required evidence |
| --- | --- |
| Startup | Selected backend, native surface, first valid Canvas2D frame, no fallback |
| Resize | Landscape, portrait, integer-fit, fractional-fit, minimize, and restore mappings |
| Materials | Orientation, alpha, cutout, nearest/linear sampling, and stable overlap |
| Tilemap | Palette texture, visible chunks, empty chunks, animated tile, and collision output |
| Reopen | Delete disposable cache, reopen source/Library artifacts, reproduce frame identity |
| Replacement | Switch away and back without stale handles, duplicate windows, or background contexts |
| Shutdown | Retire Canvas2D resources, backend epoch, GUI route, and native surface once |
| Build/Run | Saved acceptance project builds and its standalone child renders the same scene |

## Replacement And Soak

Use the normal whole-editor replacement path, never a fake selector or detached
second editor. A bounded cycle visits every available provider and returns to
the starting provider. Record before/after:

- process private bytes and committed memory;
- native texture/render-target counts where available;
- Canvas2D residency records and backend epochs;
- context/window count;
- task/command queue depth;
- audio device and playback-session count;
- stale-handle, duplicate-release, and presentation failures.

Run at least three complete cycles for a candidate proof. Growth must converge
after warm-up; monotonic unbounded growth, a surviving retired context, or a
second physical audio device fails the run.

## Evidence Artifacts

Each scenario writes one bounded evidence directory containing:

- command/arguments and candidate executable SHA-256;
- backend/capability snapshot and project/artifact revisions;
- structured result manifest;
- engine and backend logs;
- native capture, T0-CPU reference, and visual diff;
- exact/tolerance comparison metrics;
- resize and lifecycle observations;
- terminal process status and elapsed time.

The evidence directory is local/generated output, never tracked source. README
screenshots are promoted separately only after operator approval.

## Pass Criteria

A backend passes the baseline only when:

1. the requested provider actually owns the presented frame;
2. orientation, alpha, sampling, resize, and ordering satisfy the pixel policy;
3. tilemap and texture artifacts match the saved project revision;
4. cache loss recreates disposable resources without changing visible meaning;
5. repeated replacement and shutdown leave no active retired resources;
6. Build and external Run consume the canonical saved project;
7. logs and manifests contain no hidden fallback or unobserved failure;
8. the operator accepts the candidate's visible result.

Until then the provider remains `Partial` in
`renderers.md`, regardless of successful compilation.

## Harness Direction

The existing `epoch_renderer_smoke` executable is a legacy process matrix. It
does not currently prove the canonical Canvas2D scene or pixel parity and is not
a release gate. Its eventual replacement must use the central
`platform.child_process` supervisor, exact argument vectors, bounded logs,
owned descendant termination, and the engine capture/evidence contracts. It
must not invoke shell strings or duplicate project/runtime lifecycle authority.
