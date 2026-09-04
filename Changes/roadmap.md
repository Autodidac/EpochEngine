# Epoch Roadmap

This file schedules only unfinished product work. Completed implementation and
verification history lives in `Changes/changelog.txt` and Git history. The
single active acceptance gate is `Changes/active_pass.md`; durable unresolved
operator intent is `Changes/mission_cache.md`.

## Product Mission

Ship a complete, usable baseline 2D project loop while continuing to turn the
editor and its sandboxed AI development workflow into a reliable production
control plane. The acceptance project must support:

- authoring and reopening one tile-based map;
- controlling, animating, and colliding one actor;
- deterministic camera, draw order, physics, and runtime restoration;
- at least one sound effect and one looping music or ambient bus;
- editor Play/Stop plus external Run and Build from the same project state;
- deletion of disposable caches followed by exact artifact regeneration;
- visible capability, performance, memory, build, and failure diagnostics.

Advanced 3D, networking, planetary simulation, persistent autonomous AI, and
high-tier effects cannot displace this product loop.

## Current Release State

- Local source and Windows/Linux build authorities declare v0.89.35.
- Public Windows/Linux runtime and private-source discovery remain v0.89.34.
- Public macOS packaged authority remains v0.89.30.
- The next release may advance only one exact committed tree after Windows and
  managed-Clang Linux production evidence, immutable packages, receipts,
  sidecars, rollback checks, and one reviewed Site activation.
- Historical runtime/source objects and `multicontext-base-stable` remain
  immutable. GitHub is not a release authority and currently rejects pushes
  because the account is suspended.

## P0 — Self-Coding Candidate Lab

- [ ] Complete a real local-model run from an ordinary-language objective
  through model-selected context, plan, exact proposal, sandbox apply, all host
  validation, candidate PID/context preview, and Keep Current / Choose
  Candidate.
- [ ] Prove that the visible working indicator, elapsed time, Stop Session,
  retry states, diagnostics, and terminal result remain readable at normal,
  narrow, and high-zoom layouts.
- [ ] Prove selected source and request bytes cannot escape the reviewed
  12-path ceiling, cross candidate/project boundaries, overwrite live source,
  or enter project-assistant chat/session state.
- [ ] Enforce and test the candidate process's filesystem/network boundary.
  The current inherited OS identity plus Job Object is lifecycle supervision,
  not security confinement for arbitrary compiled candidate code.
- [ ] Prove transport, schema, proposal, build, and test failures retry only
  within their bounded budgets and preserve the last useful sandbox checkpoint.
- [ ] Prove Keep/Choose retires every losing child/worker, persists exactly one
  next sandbox parent, and resumes the saved mission without repeating completed
  steps.
- [ ] Decide the stable inbound/outbound MCP transport after the installed Codex
  `mcp-server` deprecation has a documented successor; no listener is enabled by
  default.
- [ ] Add an opt-in generated-project campaign profile using project-owned
  source, save/build/test/run executors without inheriting Engine-development
  authority or model weights.

Exit gate: one operator-understandable session completes with truthful evidence,
no blank/dead actions, no orphan processes, and no live-source/project mutation.

## P1 — Editor And 2D/UI Usability

- [ ] Finish Properties as a component-aware inspector with clear identity,
  layout/transform, appearance, interaction, state, resource, runtime, and
  diagnostic groups; unavailable fields explain why and never look editable.
- [ ] Complete the 2D/UI workspace over the existing GUI and tile-map documents:
  discoverable creation controls, hierarchy, canvas selection, direct
  manipulation, responsive property editing, semantic Undo/Redo, preview, and
  exact Save/Reload.
- [ ] Make Timeline user-facing: clear Sequence/Checkpoints/Media/Diagnostics
  sections, visible track/key selection and scrubbing, explanations for disabled
  transport, and no telemetry wall in the default view.
- [ ] Finish the code/script workspace with document tabs, syntax/diagnostics,
  find/replace, large-file behavior, build feedback, and document-local temporal
  Undo/Redo without implicit source writes.
- [ ] Eye-test tab insertion, native floating/redocking, guide/ghost targeting,
  drag opacity/locking, modal sizing, divider defaults, and input capture across
  the supported editor applications.
- [ ] Eye-test Package Manager responsive list/detail/footer sizing and model
  install progress at compact, 1080p, 2K, 4K, and high-zoom layouts.

World, World Outliner, World Settings, and the two command rows are not current
redesign targets unless an acceptance regression is found.

Exit gate: existing features outside World are discoverable, interactive,
reversible, persistent, and usable without reading internal hashes or subsystem
names.

## P2 — Playable 2D Acceptance Loop

- [ ] Eye-test tile-map layer/object authoring, selection, drag, staged
  properties, duplicate/delete, collision intent, save/reopen, and hierarchy.
- [ ] Eye-test Project Controls keyboard/controller editing, default restoration,
  dead zones, physical-device input, and isolation from editor-camera controls.
- [ ] Run repeated editor Play/Stop and external Run from the same accepted map,
  GUI, input, physics, animation, and audio state without leaked/duplicated
  bodies, sessions, devices, or renderer resources.
- [ ] Prove looping ambient/music and jump/landing cues through physical output,
  including stop/restart and unavailable-device behavior.
- [ ] Remove disposable project Library/cache output and prove source-authority
  regeneration without modifying authored source.
- [ ] Finish editor-free GUI runtime focus, text input, actions, tabs, image
  resources, and compiled-only game-build proof.
- [ ] Make generated/project output include only the selected systems and
  unavoidable runtime dependencies.

Exit gate: a new project can be authored, saved, closed, reopened, played,
stopped, built, run, and regenerated using documented commands and visible UI.

## P3 — Resources, Diagnostics, And Backend Truth

- [ ] Complete texture pointer feel, native save/reopen/assignment proof,
  dependency inspection, broader admitted formats, and measured residency/cost
  diagnostics while keeping authored pixels independent of physical storage.
- [ ] Expose measured native allocation, upload, atlas/residency pressure, frame
  timing, and budget failures without converting estimates into evidence.
- [ ] Prove Canvas2D scaling, alpha, minimized/restore, cache recreation, and
  repeated context replacement on every claimed presenting backend.
- [ ] Prove OpenGL context-bound texture hook ownership and cleanup-current
  behavior; prove Raylib deferred deletion and Vulkan repeated retirement.
- [ ] Gather approved Raylib/Vulkan orientation and non-OpenGL Canvas2D pixel
  evidence before changing any `Partial` capability to `Present`.
- [ ] Keep MSVC, managed-Clang CMake, generated-child, HeadlessCI, and package
  layouts aligned with the same source and capability contracts.

Exit gate: capability and renderer matrices contain no unsupported presentation
claim, and repeated resource replacement has bounded memory/handle behavior.

## P4 — Release And Distribution

- [ ] Run final serial Windows Debug/Release and managed-Clang Linux Release
  builds plus required contract, HeadlessCI, package-layout, RPATH/library,
  source-name, and diff checks on one clean commit.
- [ ] Complete native operator evidence for the exact packaged Windows build and
  the approved Linux OpenGL lane without substituting build proof for pixels.
- [ ] Stage minimal immutable Windows ZIP and Linux tar.gz artifacts with exact
  sizes, SHA-256, executable identity, checksum manifests, and exact-byte build
  receipts/sidecars.
- [ ] Prove previous v0.89.34 artifacts remain byte-identical rollback inputs.
- [ ] Hand one exact reviewed runtime/private-source/catalog/gallery set to the
  Site task; activate discovery only after every required platform report passes.
- [ ] Later, replace the Windows archive with one verified bootstrap executable
  and a minimal versioned runtime payload; keep developer output separate and do
  not retrofit immutable older releases.

Exit gate: public runtime, authenticated source discovery, signed admission,
cards, docs, checksums, and rollback state all identify the same accepted tree.

## Later Missions — After The Baseline 2D Loop

1. Portable GLES/OpenGL compute and accelerated procedural 2D work.
2. Shared typed material/model/effects authoring and post-processing.
3. Backend-native portal clipping, surfaces, traversal, audio, and physics.
4. Advanced extension-owned terrain, ocean, voxel, foliage, and package payloads.
5. Cross-platform, operator-started STT/TTS adapters for `ai.voice_session`.
6. T2 Vulkan/DirectX resource parity, then T3 GPU-driven features, T4 ray query,
   and T5 full ray-tracing pipelines.
7. Persistent regions, collaboration, networking, and planetary/astronomical
   packages after explicit product and security gates.

## Planning Rules

- Capability is selected per subsystem, never inferred from API name.
- Source documents and semantic history are authoritative; compiled artifacts
  are reproducible and physical caches are disposable.
- A feature and its settings, controls, diagnostics, persistence, tests, docs,
  and release claims move together.
- Project Save/Build/Run decisions flow through `project.lifecycle`.
- The latest real editor screenshots are layout authority; concepts guide only
  density and workflow, and defect captures are not acceptance evidence.
- Hosted/Site validation confirms faithful local proof; it is not where ordinary
  compiler or contract failures are first discovered.
- Completed tasks are removed from this roadmap and recorded in the changelog.
