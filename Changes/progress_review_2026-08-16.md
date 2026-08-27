# Progress Review - 2026-08-16

## Evidence Anchor

- branch: `main`, nine commits ahead of `origin/main`
- committed head: `3e54eff`
- active source candidate: `v0.89.25`
- sealed packaged baseline: `v0.89.06`
- previous comparison: `Changes/roadmap_baseline_2026-08-10.md`
- Debug and Release `ConsoleApplication1` builds pass
- Debug and Release `--engine-contract-self-test` pass
- source naming validation passes for 485 first-party files

The numbers below describe repository movement, not product completion. Accepted
workflows and evidence remain the delivery measure.

| Range | Tracked committed delta |
| --- | ---: |
| Since 2026-08-09 | 197 files, +45,092 / -2,105 lines |
| Since 2026-07-16 | 510 files, +110,213 / -14,818 lines |
| Current tracked worktree beyond HEAD | 111 files, +44,345 / -9,039 lines |

Current untracked modules are excluded from those line totals.

## Week-Over-Week Movement

| Critical lane | August 10 baseline | August 16 position | Remaining acceptance |
| --- | --- | --- | --- |
| Capability and Tier-0 scene | Typed capability policy; complete saved scene loop open | Renderer-neutral capability/budget admission, shared ray selection/Focus, semantic scene command gateway, monitor/tier host sizing, and one canonical editor projection are present | Operator proof that Save/reopen/Play/Run/Build consume the same default scene |
| Texture authoring | Source/artifact/import/material foundations | Continuous versioned brush path, opacity/hardness/channels, non-destructive transforms/filters, sparse masks, schema migration, exact preview, and thumbnail atlas are build-proven | Native pointer/layout proof, broader formats, compression/color conversion, complete cost controls |
| Canvas2D | OpenGL and reference foundations | Shared runtime/presenter lifecycle, bounded replacement, all seven baseline adapters, OpenGL/reference pixel parity, limit mapping, and generated GUI external Run are build-proven | Non-OpenGL native pixel/switch evidence, minimized/restore and memory proof, sRGB/compressed/mip execution |
| Tilemap | Partial contracts and editor surface | Temporal source, sparse chunks, artifact, Library restore, authoring workspace, collision/object output, generated-child regeneration, and compiled-only runtime restore are build-proven | Operator authoring proof, live Play/Stop, external Run, explicit cache-regeneration proof |
| Input and physics | Managers existed; playable loop absent | Persistent project input, controller snapshots, deterministic 2D solver, one-way/slope collision, actor runtime, and a 187-frame generated-child composition are build-proven | Physical-device interaction and repeated Play/Stop leak proof |
| Audio and animation | Foundations only | Deterministic PCM mixer, optional process-owned SDL3 device, project WAV/profile pipeline, sprite animation, cue routing, composed gameplay runtime, and 64-session soak contracts are present | Ear proof, physical output proof, and repeated editor Play/Stop proof |
| Project integration | Six profiles could materialize and self-test | Versioned project manifests, static runtime Build, exact source/Library/cache separation, generated child acceptance, GUI runtime, runtime cost admission, and explicit project open/close are present | One operator-approved new-project-to-built-game workflow and native resource metrics |
| Editor shell | Separate applications, early tools | Canonical project sessions, GUI semantic history/delete/save/reload, shared timeline controls, independently routed authoring tools, one filtered Output pane, native floating hosts, docking guides/ghosts, readable closable tabs, and exact move selection are present | Native docking/multi-monitor eye proof, layout persistence, richer per-tool content, arbitrary compatible stack creation |

## Month-Over-Month Movement

The month began with Raylib native-surface repair and the `v0.88.69`
multicontext baseline. Since then Epoch has gained:

- the temporal engine and authoring architecture as the canonical ownership plan;
- capability-tier admission, platform budgets, Tier-0 reference systems, shared
  lighting/ray/physics/audio spines, and renderer evidence rules;
- backend orientation/retirement repairs and cross-backend Canvas2D adapters;
- source naming and ownership normalization across engine, editor, modules, and
  renderer folders;
- separate Standard Editor, Plant Lab, and GUI Editor applications over one
  shared shell;
- canonical scene, GUI, texture, tilemap, input, audio, and animation documents
  with source/Library/cache separation;
- deterministic project runtime composition spanning input, physics, sprites,
  GUI, Canvas2D, and audio;
- bounded external OS-AI discovery, explicit per-session model approval,
  proposal-only authoring, guarded source-development contracts, and voice
  session policy without hidden servers;
- a real systems/task-graph workspace, reusable EpochGui graph/text/asset
  controls, project file-dialog admission, timeline controls, and current
  tool-window docking.

## Garbage Collection And Resource Boundaries

Routed panes use deterministic reclamation rather than unbounded retained state:

1. close/redock marks the routed context for retirement;
2. the manager removes it from the live window set;
3. finished render threads are joined;
4. owner and render command queues are cleared;
5. native GL context, device context, and HWND ownership are released;
6. editor, chat, Canvas2D, preview-grid, and look state are erased;
7. full editor shutdown also clears route/redock maps, detached projection, and
   passive-context scoring.

The workspace cache inventory contains only the active 73-byte local AI
selection/runtime configuration and an empty Release cache directory. These are
operator/runtime state, not stale update/package payloads, so this pass did not
delete them. Source, project documents, Library artifacts, and operator files
are never garbage-collected as cache.

## Maturity Assessment

- Architecture and deterministic contracts: strong foundation.
- Editor and project integration: coherent prototype, rapidly becoming usable.
- Native interaction and cross-backend visual evidence: partial and still the
  main gap between build proof and production confidence.
- Playable 2D objective: core systems are composed; the decisive remaining work
  is one polished authored project proven through Save, reopen, Play, Run,
  Build, cache regeneration, and repeated resource teardown.
- Release readiness: intentionally not open. Finish interaction proof and the
  acceptance project before hardening or packaging.

## August 20 Follow-Up

The current source candidate adds a stricter and smaller OS-AI lane:

- context requests are exact, ordered, bounded records that reject unknown fields,
  duplicate counts, duplicate paths, traversal, cross-domain paths, and trailing
  data;
- every proposed source path is context-first: existing files require exact
  content evidence, while new files require explicit absent-file evidence and an
  exact operator objective path;
- Review Paths is request-only and transmits no source bytes. Share Requested
  Context is a distinct consent step that revalidates the objective and canonical
  root, reads only the displayed UTF-8 paths within 184 KiB, and sends exact
  counted contents or absent-path evidence only to the displayed selected
  endpoint. The grounded second pass rejects generic or invented edits;
- proposal review and sandbox execution remain attached to the relevant AI Chat
  response. Duplicate lifecycle controls and the obsolete hidden iteration-packet
  and internal-dataset path were removed;
- the sandbox compiler future has one completion owner, and AI scene/GUI goals
  apply one bounded semantic command per approved milestone;
- the Plant Lab compiler now maps branch-start and branch-length authoring into a
  target-height-normalized morphology recipe, so a semantic branch-start edit
  changes the compiled graph while preserving the authored target scale.

Build-safe Debug and Release editor builds and engine contracts are the evidence
for this follow-up. No GUI runtime eye test was run in this pass. The live
automation gap remains substantial: test-runner, static-analysis, sanitizer,
architecture-review, visual-harness, and frontier-review host adapters are not
connected; voice capture/STT/TTS adapters are also absent. Native multi-context
interaction, renderer pixel parity, persistent docking behavior, and a complete
Plant Lab source/Library/reopen path still require operator-visible proof.
