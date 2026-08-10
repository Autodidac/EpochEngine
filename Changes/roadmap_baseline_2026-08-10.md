# Roadmap Baseline - 2026-08-10

## Purpose

This is a comparison checkpoint for the live `Changes/roadmap.md`. It records
where the engine stood before the next renderer and 2D-delivery expansion. The
roadmap remains the planning contract; this file is historical evidence.

## Source Anchor

- branch: `main`
- committed source: `d8c955e` (`v0.89.09`)
- local state: two commits ahead of `origin/main`
- active candidate: `v0.89.10`, not yet committed at this checkpoint
- packaged baseline: sealed `v0.89.06`
- remote synchronization: intentionally deferred while repositories are down

## Delivery Position

| Roadmap lane | Position on 2026-08-10 |
| --- | --- |
| Week 1 - capability and Tier-0 scene | Capability policy is build-proven. Default scene, project lifecycle, selection, and Focus still need one complete saved-editor-to-built-runtime acceptance proof. |
| Week 2 - texture document and residency | Temporal texture document, artifact, cache, residency, import, project publication, and exact-revision foundations exist. Full sparse authoring UI, settings, and cost controls remain open. |
| Week 3 - Canvas2D and sprite batch | Renderer-neutral planning, deterministic CPU raster, physical texture cache, OpenGL compositor, project texture publication, scene material identity, and native pixel comparison exist. Live authoring and broad backend presentation parity remain open. |
| Week 4 - tilemap and scene authoring | Contracts and editor surfaces are partial; a map cannot yet be authored end to end without source edits. |
| Week 5 - input and 2D physics | Manager and scheduling foundations exist; the playable actor/input/collision acceptance loop is not complete. |
| Week 6 - audio, animation, Run, and Build | Audio manager and project lifecycle foundations exist. Physical audio, sprite animation, and one-scene Play/Run/Build parity are not complete. |
| Week 7 - integration and portability | Generated profile production tests now cover all six current profiles locally. The acceptance game and complete cross-platform project loop remain open. |
| Week 8 - hardening and release readiness | Not entered. The release gate remains sealed. |

## Proven At This Checkpoint

- Debug and Release editor builds and engine contract self-tests pass for the
  active texture/material candidate.
- All six generated project profiles materialize, compile, and pass their child
  self-test on the Debug MSVC lane: Project Hub, Sandbox, Platformer, GUI Editor,
  Plant Lab, and Software Studio.
- Generated projects resolve the real repository root and link the same
  `StaticLib1` plus `EpochGui` boundary as the checked-in editor.
- Project texture import accepts bounded uncompressed BMP, TGA, and P6 PPM,
  publishes immutable artifacts, restores exact historical revisions, and
  binds scene materials through generation-checked leases.
- Scene snapshot format v3 preserves semantic texture-material identity and
  supports deterministic undo, redo, validation, and legacy snapshot reads.
- The Standard, GUI Editor, and Plant Lab applications remain separate
  applications over one shared editor spine.

## Open Acceptance Gates

1. Capture and inspect dedicated Standard, Assets, Systems, AI, GUI Editor, and
   Plant Lab surfaces from the production runtime without synthetic proof.
2. Finish the saved default scene loop: ground, camera, light, spawn, selection,
   Focus, Save, reopen, Play, Run, and Build all consume one document.
3. Complete live Assets authoring, sprite/material assignment, Canvas2D output,
   tilemap authoring, input, physics, audio, and animation for the 2D game.
4. Translate proven OpenGL techniques into backend-neutral contracts and add
   honest lower-tier fallbacks or explicit unsupported evidence per backend.
5. Re-prove Windows Debug/Release, CMake/Clang, headless, generated projects,
   cache regeneration, and bounded restart/resource behavior before release.

## Comparison Rule

Future comparisons should measure accepted workflows and evidence, not raw file
or line counts. A lane advances only when implementation, controls, persistence,
diagnostics, and the closest production test move together.
