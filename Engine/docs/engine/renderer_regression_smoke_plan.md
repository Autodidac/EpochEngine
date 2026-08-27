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
`renderer_feature_matrix.md`, regardless of successful compilation.

## Harness Direction

The existing `epoch_renderer_smoke` executable is a legacy process matrix. It
does not currently prove the canonical Canvas2D scene or pixel parity and is not
a release gate. Its eventual replacement must use the central
`platform.child_process` supervisor, exact argument vectors, bounded logs,
owned descendant termination, and the engine capture/evidence contracts. It
must not invoke shell strings or duplicate project/runtime lifecycle authority.
