# Event-Driven 3D Rendering

## Status

Epoch v0.90.21 introduced the first production renderer integration for event-driven scene reuse. This is not a separate lab renderer and does not replace the conventional path. The OpenGL editor scene now keeps a persistent color/depth cache and invalidates only the screen regions affected by real scene-object changes when that is safe. Unsupported or dense changes fall back to a full redraw.

## Runtime model

The editor scene already owns camera revisions and scene-geometry revisions. v0.90.21 separated the raw scene-content revision from the combined camera/geometry revision and records object-marker change frames when the marker population is stable.

For a localized object mutation:

1. the previous and current object markers are retained in a transient change frame;
2. conservative world-space bounds are projected through the current view/projection matrix;
3. previous and current projected bounds are unioned into dirty screen rectangles;
4. the persistent OpenGL scene color/depth target clears and redraws only those scissored regions;
5. all scene geometry is re-evaluated inside each dirty rectangle so newly exposed depth/occlusion is reconstructed correctly;
6. the complete cached scene is blitted to the real framebuffer before normal GUI composition.

Camera changes, grid/global scene changes, object population changes, sampled render surfaces, unknown invalidations, too many dirty rectangles, or dirty coverage at/above roughly 60% conservatively use a full-scene reconstruction. Lighting is classified separately: ambient/environment and directional changes are global, while point/spot changes may produce bounded previous/current influence damage.

## Correctness rules

- The old and new object positions are both invalidated so moved objects cannot leave stale pixels or depth behind.
- Partial redraw is based on persistent engine-owned color/depth state, never on assuming swapchain/default-framebuffer preservation.
- The renderer redraws complete scene content inside dirty rectangles rather than patching only the moving object's pixels.
- Dense invalidation automatically collapses to the conventional full redraw path.
- The software renderer now includes scene-content revision in its existing full-frame reuse decision so geometry mutations wake the renderer correctly.
- Vulkan keeps its existing geometry-revision buffer rebuild behavior in this pass; partial color/depth reuse is not claimed for Vulkan yet.

## Current limits

The first path targets the real OpenGL editor 3D scene. It does not yet provide partial color/depth reuse for Vulkan, DirectX, SDL, SFML, Raylib, or the software backend. Point/spot light influence damage is now represented in the renderer-neutral lighting core and consumed by OpenGL, but native partial-light reuse is not claimed for backends that still redraw the full frame. Transparency, temporal effects, shadow-map/volume dependency tracking, animation/skinning dirty ranges, and other nonlocal effects require additional invalidation policy before partial reuse is safe.

## Validation target

The useful measurement is end-to-end frame cost, not only dirty-region bookkeeping. Compare static/localized-change scenes against dense movement and camera-motion scenes, including the cost of change detection, projection, cache maintenance, redraw, and final presentation. The renderer must be allowed to lose to the dense conventional path; the automatic fallback exists for that reason.

## v0.90.23 live diagnostics and A/B controls

The production path now exposes its live decision state through `render.event_debug`. In the editor, open the Scene inspector's **World Settings** panel. The **Event-Driven Rendering** section reports the current path (`CACHED`, `PARTIAL`, `FULL`, or `CONVENTIONAL`), fallback reason, dirty-region count and coverage, last OpenGL preview render time, and cumulative path counts.

Three runtime controls are available there:

- **Event renderer**: enabled by default. Disable it to force the conventional OpenGL editor-scene path for direct A/B comparison.
- **Dirty-region overlay**: disabled by default. Enable it to draw the actual reconstructed region boundaries over the real 3D viewport. Partial redraw boundaries are yellow; full-redraw boundaries are red.
- **Vacated-region overlay**: disabled by default. Enable it to draw the previous object bounds in cyan, showing the part of the cached frame that had to be reconstructed specifically to erase stale color/depth after an object moved. Previous bounds are always invalidated; this switch changes visualization only.

Both overlays are diagnostic only and are never written into the persistent scene cache.

The Systems status dock also exposes the current event path, fallback reason, dirty-region count/coverage, and last render time. Camera changes, sampled surfaces, global/unknown invalidation, more than 12 dirty regions, or at least 60% dirty coverage remain conservative full-redraw fallbacks. Off-screen-only valid mutations now advance the cache scene revision without forcing a redundant redraw on the next frame. Dirty rectangle merging is transitive so coverage telemetry does not double-count overlapping/touching invalidations.


## v0.90.24 neuromorphic density bridge

The real OpenGL cached/partial path now owns a small persistent weighted
neuromorphic invalidation network. Each valid dirty rectangle produces a
visual-only change signal. Sparse changes remain eligible for regional
reconstruction; sustained/dense activity can conservatively promote the frame
to a full redraw. Queue/budget pressure also promotes to full.

This bridge cannot discard a dirty rectangle or hide a scene mutation. The
existing previous/current bounds, cache revision, camera/global invalidation,
12-region hard limit, and 60% coverage hard limit remain authoritative. The
neural path therefore changes only the cost-selection policy, not scene
correctness or gameplay truth.


## v0.90.27 vacated-region diagnostics

Localized movement now preserves a second diagnostic rectangle set for previous object bounds before those bounds are merged with current bounds into the authoritative dirty region set. The renderer still always reconstructs both old and new locations. The new **Vacated-region overlay** exposes only the old-location damage in cyan so removal work can be inspected independently from destination redraw work.


## v0.90.28 alternating A/B benchmark

World Settings now exposes **A/B render benchmark**. While enabled, eligible OpenGL editor-scene frames alternate deterministically:

- **SELECTIVE**: the normal event-driven path chooses cached, partial, or conservative full reconstruction from the real dirty-region/neural policy.
- **FULL BASELINE**: the same persistent scene cache is forcibly reconstructed over 100% of the viewport, then presented through the same final cache blit.

The full baseline intentionally uses the same cache target and presentation path rather than the older direct conventional renderer. That keeps the cache current for the next selective frame and removes target/presentation differences from the comparison. Each benchmark sample drains prior GPU work before timing and waits for scene GPU completion at the end, so the reported interval is a synchronized scene render measurement rather than asynchronous CPU submission time. Optional debug overlays are drawn outside the measured interval. Benchmark mode therefore trades throughput for cleaner comparison data and should be disabled during normal play/editing.

The panel reports sample count, last time, accumulated time, average time, average speedup, and percentage time saved for both lanes. **Reset render benchmark** clears both accumulators and restarts the A/B phase.

For a repeatable localized-motion test, select a Cube (or another movable scene object) and run the built-in **Oscillate Selected Entity** script. It attaches a transient X-axis oscillator at +/-2 world units and 0.5 Hz to the selected entity. Press **Play** so simulation time advances, then enable the A/B benchmark. The oscillator affects the live preview only; it does not write a transform history entry every frame or permanently rewrite the authored scene.

## v0.90.29 selection and lighting damage

The editor now maintains a selection set with a primary object. Shift+left-click toggles
objects in that set, dragging empty 3D space creates a marquee selection rectangle, and
dragging any selected object moves the complete selected set through one scene-document
transaction. Batch cube creation creates and selects four or eight cubes atomically for
repeatable renderer tests.

Lighting invalidation is now authored in `render.lighting`, not inferred from the visual
light marker. Ambient/environment or directional-light changes remain global damage because
they may affect every visible surface. Point and spot light changes publish conservative
bounded influence-volume damage for both the previous and current light state. Renderers can
consume the same `LightingInvalidationFrame`; the OpenGL event renderer projects those
volumes to dirty rectangles immediately. Full-frame backends remain correct by redrawing
the frame normally, while the shared lighting core no longer requires them to invent their
own light-damage semantics.


## v0.90.30 persistent telemetry and title FPS

Renderer diagnostics now distinguish two different rates that were previously easy to confuse. Native window titles report the real host/presentation FPS produced by the context loop. OpenGL windows additionally report measured **render FPS**, derived from the average scene-render time for the active CACHED, PARTIAL, FULL, or CONVENTIONAL path. In A/B benchmark mode the title reports both SELECTIVE and FULL BASELINE render-FPS rates while host FPS remains the actual presentation rate.

Cached, partial, full, and conventional timing accumulators now persist across event-renderer toggles. A/B accumulators likewise persist when benchmarking is disabled. Statistics are cleared only by the explicit reset controls, so disabling a system no longer destroys the evidence needed for comparison.


## v0.90.31 undo/create branch correctness

Scene object identities are unique among active objects, not permanently reserved by inactive temporal history slots. Undoing a creation or deleting an object keeps its old slot as a tombstone for exact semantic undo/redo, but a new history branch may create the same semantic identity in a fresh slot. This fixes single and batch cube creation after Undo without weakening active-scene duplicate-ID validation.


## Benchmark interpretation and test workflow

A cached frame can have extremely high **render throughput** while the native window still presents at 60/120/144 FPS because VSync, an editor frame limiter, normal simulation pacing, or A/B synchronization controls presentation. Therefore:

- use window-title **FPS** to judge the real context/presentation cadence;
- use path **render FPS** or average render microseconds to compare renderer work;
- do not infer cached-path cost from presentation FPS alone;
- keep statistics after disabling a system so one lane can be compared against earlier evidence;
- reset only when starting a deliberately new sample set.

For localized-motion benchmarking, attach the oscillator from **Properties -> Movement / Benchmark** to a selected movable object, press Play, then enable A/B benchmark mode. Multi-selection and +4/+8 cube creation can increase scene cost while one or more selected objects move. Dirty and vacated overlays are diagnostics and remain outside the timed scene interval.

## Cross-renderer lighting contract

`render.lighting` is the shared source of lighting damage semantics. Renderers must not decide light scope from editor gizmo pixels.

- ambient/environment and directional changes publish global damage;
- point/spot changes publish conservative previous/current influence volumes;
- OpenGL projects bounded influence volumes into its dirty-region system;
- full-frame backends may ignore the optimization while still honoring correctness by redrawing the frame;
- a future backend-specific partial cache should consume the same shared damage frame rather than invent a second light invalidation model.
