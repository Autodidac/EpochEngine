# Epoch Roadmap

This file schedules only unfinished product work. Completed implementation and
verification history lives in `Changes/changelog.txt` and Git history. The
single active acceptance gate is `Changes/active_pass.md`; durable unresolved
operator intent is `Changes/mission_cache.md`.

## Product Mission

Make Epoch a reusable engine for software and games, with an optional capable
editor and a local-agent workflow that can improve, compile, compare and carry
forward its own sandbox source. Self-coding serves that product; neither model
autonomy nor a renderer demo is an alternative product goal.

Ship reusable non-editor software foundations (CLI, platform-window and GUI
applications) alongside a complete baseline 2D project loop. The editor and its
sandboxed AI workflow are development tools, not required runtime dependencies
of generated software. The 2D acceptance project must support:

- authoring and reopening one tile-based map;
- controlling, animating, and colliding one actor;
- deterministic camera, draw order, physics, and runtime restoration;
- at least one sound effect and one looping music or ambient bus;
- editor Play/Stop plus external Run and Build from the same project state;
- deletion of disposable caches followed by exact artifact regeneration;
- visible capability, performance, memory, build, and failure diagnostics.

Advanced 3D, networking, planetary simulation, persistent autonomous AI, and
high-tier effects cannot displace this product loop.

Before broad context expansion, refresh and prove the stable software/context
base against the current Engine. The September 5 operator authorization permits
updating `multicontext-base-stable` after that evidence; preserve the original
checkpoint in history and use this worktree rather than multiplying copies.
After the current major repairs are accepted, freeze that proven reusable base
before game-specific expansion. Later game work develops above it; refreshing
the frozen base requires a deliberate compatibility-tested decision.

## Ordered Delivery And Continuity

The operator's immediate target is **end of September 6, 2026
(America/New_York)**. This is a priority, not a claim that the batch is complete
or permission to bypass acceptance. The order below supersedes older dated
schedules. Mission IDs (P0–P4, EXT, RSH, SDK) are stable references, not competing
priority numbers.

| Order | Bounded result | Required exit evidence |
| --- | --- | --- |
| D0 / P0 | Repair actual Qwen failure handling, prompt/persistence continuity, measured 30-second pacing and task/UI truth | Closest production contracts plus exact current Debug/Release builds; full failure evidence survives into repair |
| D1 / P0 | Real model builds, embedded PID comparison and sandbox succession | Two accepted builds across Choose, Keep/Stop/close tests, enforced data/process boundary and no orphan/cross-project writes |
| D2 / P2 + EXT + P1 | Finish current EpochGui/editor and Extensions workflow inventory; prove generated CLI/native-window/GUI software projects | Readable/interactable/history-safe controls; truthful package states; Save/Load/Build/Run/Stop/Reopen/rebuild and dependency-closure matrix |
| D3 / P1 | Refresh then freeze the accepted reusable software/context base | Versioned compatibility contract and profile/backend tests; preserve original stable ref; no unfinished game-specific coupling |
| D4 / EXT | Integrate Sim/Space demos as add-on projects with separate engine libraries | Reviewed license/provenance, exact library dependencies, installation/load/build/run/stop/reopen evidence; planetary library named EpochSpaceEngine |
| D5 / SDK | Complete Doxygen, private SDK Reference in Engine About and matching website | Public API coverage and sample-consumer builds; exact-version artifacts; owner/admin success and every private-route denial/cache test |
| Next major product acceptance / P3 | Complete the playable 2D game loop | Full map/actor/input/animation/collision/audio/GUI/persistence/build/cache-regeneration acceptance, not merely a demo that opens |
| Continuing / P4 + later missions | Resources, context adapters, platform extraction and higher tiers | Explicit per-backend/profile capability, correctness and measured-cost evidence without breaking accepted lower tiers |

The **2D game remains a major goal**, not demoted to a demo, deleted, or blocked
on completing all future 3D/planetary work. Its shared GUI, authoring, input and
project acceptance can advance during D2; the full game-facing loop remains P3.
Integrating the supplied planetary project now is not open-ended planetary/
astronomical engine expansion, which remains a later mission.

Use independent agents for disjoint useful source/tests/docs, but serialize
heavy build/model/runtime work. Supporting GUI and minimal local research
retrieval may accompany P0. An unrelated subsystem cannot substitute for a
blocked P0 result. D3 precedes major game-specific changes; do not freeze an
unfinished source checkpoint merely to meet the date.

Each unchecked item means its **acceptance is open**, not that every component
needs rewriting. Reconcile with source, contracts and changelog first. At each
handoff record implemented/integrated/tested status, first failing evidence and
next concrete action in the active pass. Preserve older unfinished goals; move
proven completion to history with its mission ID/evidence instead of silently
dropping intent. Product behavior remains in its owning contract.

Release validation applies to the accepted release scope, not to completion of
all deferred missions. The authorized v0.90.1 batch must have honest feature
coverage and an exact reviewed publication set. SDK delivery is last in today's
batch, with its own private-access tests, not a reason to expose private material
through an otherwise public release.

## Current Release State

- Local source/Windows/Linux metadata is in **partial v0.90.1 preparation**, not
  an accepted release. Version scripts/receipt resolution/updater tests remain
  inconsistent; the active pass owns their exact repair and build evidence.
- Last recorded public Windows/Linux runtime/private-source authority is
  v0.89.34; macOS is v0.89.30. These are recorded authorities, not a fresh Site
  check. Keep independent EpochGui/Extensions versions independent.
- The next release may advance only one exact committed tree after Windows and
  managed-Clang Linux production evidence, immutable packages, receipts,
  sidecars, rollback checks, and one reviewed Site activation.
- September 6 release target: v0.90.1 as the new accepted base, not a public
  version advance before self-coding/native/security acceptance.
- Historical runtime/source objects remain immutable; stable-base advancement
  follows the explicit operator authorization and acceptance gate above.
  GitHub is not a release authority and currently rejects pushes
  because the account was suspended at the last recorded attempt; do not treat
  a historical remote failure as authority to change release policy.

## P0 — Self-Coding Candidate Lab

- [ ] Prove explicit model > remembered same-endpoint model > Nemotron 4B
  default selection, including unloaded/missing inventory and restart. Restore
  alone performs no inference. Make assistant versus Qwen 3.5+/3.8 coding roles
  visible, with no silent model eviction or weaker-worker substitution.
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

- [ ] Preserve repair evidence at maximum prompt size: exact edit source, first
  causal compiler/test error, full-evidence hash and failed proposal precede
  optional catalog/research. No silent loss of diagnostics on retries.
- [ ] Make validation and checkpoint persistence transactional: oversized logs,
  malformed receipts, disk/I/O/state-capacity failures, stale replies and restart
  leave one recoverable state; automatic retry does not consume invalid evidence.
- [ ] Enforce nonblocking 30-second cooldown and measured CPU/RAM admission
  before model/compiler/test/new-preview work. Show actual running/queued/idle
  activity and unsupported metrics; no new AI during Keep/Choose. Cancel/close
  retires global heavy ownership before another context starts.
- [ ] Separate routine sandbox automation from authority: Start authorizes the
  visible bounded local session, not every internal packet manually; exact paths,
  preimages, receipts and process policy remain checked. Keep/Choose changes only
  sandbox lineage; live-source promotion/access remains separate.

Exit gate: one operator-understandable session completes with truthful evidence,
two actual accepted builds across a choice, no blank/dead actions, no orphan
processes, and no live-source/project mutation. A successor plan alone cannot pass.

## P1 — Stable Software Base Before Context Expansion

Reuse `Epoch::SoftwareBase`, its CLI/Win32 lifecycle, and `context.admission`.
Their component/native-window foundation evidence is already in the changelog;
it does not complete generated templates, GUI/input or stable-branch acceptance.

- [ ] Audit Engine/runtime/editor dependencies and prove minimal CLI,
  platform-window and GUI software profiles against current source.
- [ ] Generate, build, run, stop, and rebuild those editor-free templates;
  document exact capability and artifact boundaries, then advance the stable
  base branch to the proven checkpoint without creating another full copy.
- [ ] Design the shared typed context contract around each backend's actual
  lifetime, threading, input, timing, presentation and ownership constraints.
- [ ] Plan incremental EpochPlatformEngine extraction with explicit consumers
  and compatibility tests; no blanket class replacement or duplicated platform code.
- [ ] Prove floating placement guards and readable guide labels for docked,
  floating, borrowed and separately supervised context windows before migration.

- [ ] Freeze a **compatibility contract**, not only a branch: public API/module
  identities, profile/dependency closure, build flags/toolchains, project/artifact
  schemas, context ownership/lifetime/input/timing interfaces, expected failure
  behavior and a per-backend test matrix. Record exact proven commit and artifacts.
  Later base updates need a deliberate compatibility-tested decision; Engine-
  specific expansions cannot silently change the frozen base.

Exit gate: current reusable software bases work without the editor, and every
context migration has an explicit backend-specific proof plan. The original
`ad6c416d930b348a61bc37ceb7d4522742be084a` remains available in history.

## P2 — Editor And 2D/UI Usability

- [ ] Prove centered Output/AI Chat defaults and proportional manual-divider
  persistence across main and secondary context resize, docking previews and
  save/reload. Reuse the existing docking system rather than replacing it.
- [ ] Finish Properties as a component-aware inspector with clear identity,
  layout/transform, appearance, interaction, state, resource, runtime, and
  diagnostic groups; unavailable fields explain why and never look editable.
- [ ] Complete 2D/UI over existing GUI and tile-map documents: dedicated GUI
  Editor owns widget/template construction; project 2D/UI owns tile authoring,
  GUI placement and integration. Both need discoverable controls, hierarchy,
  selection/manipulation, Properties, semantic Undo/Redo, preview and exact
  Save/Reload. Do not rebuild an already implemented compile/runtime path.
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
- [ ] Finish an opt-in generated-project AI campaign using project-owned
  source and Save/Build/Test/Run executors without inheriting Engine-development
  authority or model weights; reuse the existing project AI profile contracts.

World, World Outliner, World Settings, and the two command rows are not current
redesign targets unless an acceptance regression is found.

Exit gate: existing features outside World are discoverable, interactive,
reversible, persistent, and usable without reading internal hashes or subsystem
names.

### Finite Current GUI/Editor Acceptance Inventory

These named surfaces define D2's current feature completion; future controls
remain separate missions. Reuse implemented primitives and record remaining
integration versus missing implementation per row.

| Surface / owner | Acceptance required before calling this current workflow complete |
| --- | --- |
| EpochGui primitives / portable library | Standalone consumer build; focus, hit-test, sizing, text/selection/clipboard intent, scrolling, tabs, dropdowns, menus, modal and splitter contracts; no Engine/editor dependency |
| Engine GUI adapters / native hosts | Actual labels, clipping, DPI/zoom, input capture, drag locking and 50% held-window opacity; additive direct-tab slot guides with existing float/redock; no protected replay-order change |
| Editor layout / Properties | Centered default Output/AI Chat and preserved manual ratios; responsive list/body/footer; grouped actionable Properties with ownership reasons |
| Dedicated GUI Editor + project 2D/UI | Create/reopen reusable widget document; place/use it in a project; tile authoring, preview, document-local history and persistence remain distinct |
| Timeline / text / project assistant | Authoring before telemetry, real tracks/scrub/checkpoint/media states; real document edits/find/build feedback; no cross-document history or Engine/project AI crossover |

## EXT — Extensions And Generated Projects

- [ ] Treat Extensions as the catalog/repo of **project add-ons**, not an Engine
  plugin or installable monolith. EpochGui/Arcade stay built in and absent as
  installable package rows. Inventory every current row with its actual payload,
  activation, dependency, license and supported profile/version.
- [ ] Prove fetch/refresh, manifest/hash/dependency validation, explicit install/
  activation, cancellation/retry, offline/restart recovery and removal/deactivation
  behavior without overwriting project-owned edits. Descriptor-only/planned rows
  must not offer fake installation or claim a demo/library already works.
- [ ] Prove local Qwen/Nemotron exact-hash resumable installation in executable-
  local cache, visible progress/failure/recovery, independent selection and no
  inference/listener/weights in generated builds merely from install/discovery.
- [ ] Inventory all generated profiles and admitted demo projects. For EACH,
  record Create/Load/Save/Close/Reopen, dependency resolution, Build/Run/Stop/
  rebuild, startup data root and cleanup evidence. CLI/native-window/GUI profiles
  must omit editor-only systems and unrelated libraries/weights.
- [ ] Review the supplied EpochPlanet archive's paths, license, attribution,
  dependencies and build layout without running bundled scripts as instructions.
  Separate reusable **EpochSpaceEngine** library from its demo project.
- [ ] Expose EpochSimEngine and EpochSpaceEngine demos as separately loadable
  extension projects with separately versioned/pinned library dependencies.
  Compile/link libraries independently; prove project import/load/reopen/build/
  run/stop and dependency restoration without copying another Engine worktree.

Exit gate: each current catalog row is truthful, each supported generated project
has real lifecycle evidence, and both new demos have independent library edges.
Unimplemented optional payloads remain explicit, never represented as installed
functionality. Advanced new terrain/ocean/astronomy remains deferred.

## P3 — Playable 2D Acceptance Loop (Major Product Goal)

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

## P4 — Resources, Diagnostics, And Backend Truth

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

## RSH — Curated Model Research And Architecture Alignment

- [ ] Implement a small version-bound local research pack for Engine self-coding:
  current intent/plan, architecture/ownership, relevant API/contracts/examples
  and first-cause failure evidence. Qwen selects useful context without user
  named-file prompts; do not concatenate every planning/history document.
- [ ] Provide a distinct software/project/game-development pack for selected
  project APIs, templates, input/physics/audio/GUI/2D workflows and extension
  dependencies. It must not inherit Engine-private source, goals or receipts.
- [ ] Track provenance, content hash, version, topic, status (implemented versus
  target), permissions and freshness; preserve exact source/repair prompt budgets.
  Show retrieved references and omissions. This is retrieval, not self-training.
- [ ] Test stale/wrong-version references, maximum budgets, cross-project denial,
  cancellation/restart, no egress without session authority, and docs/web text
  that attempts to act as instructions. Optional online research remains scoped
  host work and cannot delay the local P0 loop.
- [ ] Apply the architecture alignment review in
  `Engine/docs/engine/capability_tier_architecture.md` to each major source tranche:
  data input/output/ownership, subsystem dependencies, optimization evidence,
  portable profiles, intense graphics budgets/fallbacks and compatibility.

Behavior/acceptance owner:
`Engine/docs/engine/ai_curated_research_contract.md`. The loader and automatic
packs are planned, not proven by the existence of this documentation. The same
review covers old and new goals without creating another parallel roadmap.

## SDK — Private SDK Reference And Delivery

- [ ] Audit Doxygen/public C++23 header/module inventory and create an explicit
  coverage report (documented, intentionally internal/excluded, or missing).
  HTML generation alone is not full SDK acceptance.
- [ ] Build editor-free CLI/native-window/GUI examples as external SDK consumers;
  include usable APIs, tutorials, build/debug guidance, profile/capability limits,
  versioned dependencies/licenses and exact file/package manifests.
- [ ] Provide readable SDK Reference inside Engine About only with current
  server-confirmed owner/admin SDK capability via existing source-update auth;
  do not invent another login or infer SDK access from generic source entitlement.
- [ ] Mirror the permission on website navigation AND every direct docs/asset/
  search/source-browser/archive route. Test anonymous/non-owner, expired/revoked
  session, already-open view, shared cache and direct URL cases against exact
  bytes. No private SDK content in public bundles, logs or caches.
- [ ] Evaluate a shared versioned public API/module metadata inventory for SDK
  generation and authorized model retrieval. Reuse one semantic source; keep
  read entitlement, model egress and execution authority distinct.

Exit gate and owning planned contract:
`Engine/docs/engine/sdk_reference_and_access_contract.md`. This is D5, the
**final same-day milestone after the integration batch**, not an implemented
access feature or a replacement for P0. SDK work is not blocked on all deferred
features or the entire later playable 2D milestone.

## Release Gate — Every Accepted Milestone

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
8. Qualify a stable inbound/outbound MCP adapter against the then-current
   supported client protocol. Do not rebuild the existing supervisor registry
   or enable a listener implicitly; this does not block the local HTTP P0 loop.

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
- Preserve all unfinished older goals. Completed acceptance moves to the
  changelog with its mission ID/evidence; enduring product contracts stay linked.
  No unverified implementation is erased from the queue as if complete.
- Use the cross-system architecture review to propose measured improvements,
  not speculative blanket rewrites; put the next bounded change in active_pass.
