# Neuromorphic Mainline Integration - v0.90.24

## Baselines

- Main/source baseline: current uploaded tree, already carrying the v0.90.23 OpenGL event-driven renderer and later source-discovery fixes.
- Neuromorphic source: `EpochEngine-0.90.16-neuromorphic-testable-branch.zip`.
- Renderer source: `EpochEngine-0.90.23-event-driven-renderer-debug-msvc-fix.zip`.

## What Codex had completed

The uploaded tree retained the v0.90.23 cached color/depth scene path, old/new object bounds, dirty-region reconstruction, fallback diagnostics, and the later AI source-discovery work. None of the neuromorphic runtime modules from the v0.90.16 branch were present in main.

## Mainline result

The reusable neuromorphic graph, event camera, and timeline/task-graph adapters are now mainline modules. The old experimental Neuro Lab UI and branch-only benchmark are intentionally excluded. A new `render.neuromorphic_invalidation` bridge is owned by each OpenGL state and receives real dirty-region activity from the production event renderer. It can promote a partial redraw to a full redraw when weighted activity becomes dense or bounded processing is exceeded; it cannot suppress required rendering.

This deliberately separates two jobs: scene-driven dirty-region reconstruction remains exact renderer bookkeeping, while the neuromorphic layer provides sparse weighted attention/pressure that can later also drive semantic simulation LOD. Pixel activity is not allowed to become authoritative gameplay state.

## Validation gate

`epoch_neuromorphic_contract` covers graph routing/weights/refractory/reset, event-camera behavior, timeline/task-graph adapters, and sparse/dense invalidation decisions. Full native OpenGL eye proof remains operator-owned.
