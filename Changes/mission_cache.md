# Mission Cache

This file preserves durable unresolved operator intent and safety boundaries. It
is not a completion log or schedule. Completed work is removed from here and
recorded in `Changes/changelog.txt`; current ordering belongs in
`Changes/roadmap.md`; the one active gate belongs in `Changes/active_pass.md`.

## Engine Self-Coding

- Accept ordinary-language objectives like a normal agentic coding assistant.
  Do not require the operator to name an internal system, source path, symbol,
  error code, or protocol token.
- Let the selected model choose a coherent source slice from a host-provided
  path catalog. Twelve paths is the normal ceiling; bounded expansion and one or
  two useful retries should recover from incomplete context, transport, schema,
  proposal, build, or test failures when safety still holds.
- Never answer a feasible request with a content-free refusal merely because the
  first source guess or formatting attempt failed. Show the concrete terminal
  reason only after the relevant retry/expansion budget is exhausted.
- Show that the model is still working: active phase, animated progress, elapsed
  time, cancellation, retries, and next action must remain visible and readable.
- Keep every model-authored byte inside a disposable Candidate Lab sandbox.
  Live Engine source, active projects, unrelated sandboxes, Git, releases, Site
  state, listeners, approvals, and host validation evidence stay outside model
  authority.
- Preserve a numbered mission plan and continuity. After each successful build,
  launch the candidate editor as a separate supervised PID/context in the parent
  grid, present current versus candidate evidence, and expose Keep Current,
  Choose Candidate, and Stop Lab.
- Keep/Choose must retire the losing child, retain exactly one sandbox parent,
  and continue with the next unfinished plan step. Automatic replacement of live
  source remains a future, separately reviewed policy.
- Support both an operator-selected local OpenAI-compatible model and bounded
  external MCP tooling. Prefer authenticated local stdio/process transport; no
  inbound network listener is enabled implicitly.
- Local/project/Engine campaigns, prompts, reviewed source, candidate output,
  receipts, chat, and goals must never overwrite or leak into one another.
- Model output does not certify builds, tests, screenshots, security, or release
  readiness. The host owns every compiler/test/process receipt and exact digest.
- Disposable directories and PID supervision are not OS security isolation.
  Candidate execution needs a proved filesystem/network boundary before the
  product can claim arbitrary generated code cannot affect source or projects.

## Editor Control Plane

- Make AI Controls understandable without technical hashes. Default view shows
  goal, phase, progress, model/connection, reviewed scope, candidate state,
  current-versus-candidate result, and one useful next action. Technical evidence
  remains available through progressive disclosure.
- Keep Project Assistant and Engine Self-Coding visibly separate. Neither may
  delete, replace, resume, or display the other's goal or transcript.
- Properties must be a component-aware inspector, not a flat telemetry dump.
  Group identity, layout/transform, appearance, interaction, state, resources,
  runtime, and diagnostics; disabled values explain their ownership or blocker.
- The 2D/UI workspace must expose real GUI-overlay and tile-map authoring with
  creation controls, hierarchy, direct manipulation, semantic history,
  Properties, preview, and persistence. The separate GUI Editor owns reusable
  document/template construction; the standard editor owns placement and
  project integration.
- Timeline must explain its purpose and disabled states, keep user authoring
  ahead of diagnostics, and expose real sequence, key, checkpoint, media, scrub,
  and transport behavior without a wall of internal counters.
- Code/script editing is a first-class workflow: stable documents/tabs,
  selectable text, find/replace, syntax and diagnostics, bounded large-file
  behavior, source-safe Save/Revert, build feedback, and document-local temporal
  Undo/Redo.
- Global Undo/Redo resolves the visible document's typed temporal branch. World,
  GUI, tile map, Plant Lab, text/code, Properties edits, Timeline, and future
  authoring surfaces must not mutate one another's histories.
- Preserve the existing docking/floating system. Additive tab insertion,
  guides/ghosting, native float/redock, opacity, drag locking, input capture,
  responsive modal sizing, and remembered dividers must behave like a modern
  desktop editor across normal and high-DPI layouts.
- World, World Outliner, World Settings, and the two command rows are acceptable
  unless testing finds a regression. Prioritize unfinished interface and AI
  workflows elsewhere.

## Software Base And Platform Architecture

- Epoch produces software as well as games. Audit the whole dependency and
  project structure for reusable CLI, native platform-window, and GUI software
  templates: the selected Engine capabilities without the editor, editor host,
  development caches, or model weights as runtime dependencies.
- The operator confirmed on September 5 that `multicontext-base-stable` is the
  basic software/context baseline and explicitly requested that it be updated
  to match the current Engine. Preserve its original `ad6c416d930b348a61bc37ceb7d4522742be084a`
  checkpoint in history; advance the branch only to a reviewed, build-tested
  editor-free baseline. A branch does not require another full worktree/copy.
- Establish that current stable base and prove CLI/platform-window template
  generation, build, run, exit, and rapid rebuild before broad context feature
  expansion. Keep incomplete self-coding work out of stable-base claims.
- Gradually form EpochPlatformEngine as the reusable platform/context/window/
  input foundation with clear Engine/runtime/editor consumers. Review boundaries
  before extraction, migrate incrementally, and avoid duplicate implementations,
  a wholesale rename, or breaking existing generated projects.
- Design one typed public context contract over backend-owned implementations.
  Inventory each context's real needs before changing the fragile multicontext
  system: native/process ownership, borrowed versus owned windows, thread and
  current-context affinity, resource lifetime, input/focus, timing, presentation,
  suspend/restore, replacement, and shutdown. Do not assume backend interchangeability.
- Improve floating placements and guide/ghost controls across all supported
  host kinds. Preserve the accepted colors while making labels readable and
  responsive; distinguish docking, direct-tab insertion, resizing, dragging,
  input capture, and external-PID context attachment.
- The operator's tiered OpenGL example may inform design by read-only study.
  Do not copy its source, API assumptions, frame ordering, or tutorial material
  into Epoch. Production behavior must be proved against Epoch's own contracts.

## Playable 2D Product

- Deliver the full map → actor → animation → collision → audio → GUI →
  save/reopen → Play/Stop → Run → Build loop from one project-owned state.
- Prove keyboard and controller editing, dead zones, default restoration,
  physical-device sampling, and isolation from editor camera/input.
- Prove looping ambient/music and jump/landing cues over the real process-owned
  audio boundary, including missing-device and repeated stop/start behavior.
- Prove disposable Library/cache deletion regenerates exact runtime artifacts
  from authoritative source without corrupt fallback or source mutation.
- Generated games include only selected systems and unavoidable dependencies;
  editor-only docking, floating hosts, unused backends, tests, caches, logs, and
  model weights do not ship.

## GUI, Assets, And Temporal Authoring

- Keep EpochGui built into every non-CLI Epoch application. It is not an
  installable package. Reusable controls belong in EpochGui; engine adapters own
  native hosts, backend input/drawing, project state, and evidence.
- Finish editor-free compiled GUI runtime behavior for focus, text input,
  actions, tabs, image resources, and project-game restoration.
- Extend the current texture/asset path with native pointer/save/reopen proof,
  dependency inspection, admitted formats, measured residency/cost diagnostics,
  and bounded large catalogs without making physical handles authoring state.
- Persistent authoring and Undo/Redo use typed temporal documents/branches.
  Compiled artifacts and renderer residency remain reproducible/disposable.
- Keep screenshot evidence additive. Latest real editor captures define current
  layout; concepts are design inspiration and defect captures are not release
  acceptance evidence.

## Renderer And Contexts

- Every editor/renderer context consumes the same scene revision, camera/input
  contracts, GUI draw model, selection, authoring changes, pacing policy, and
  evidence interfaces. Backend-specific behavior remains real and reported
  truthfully.
- OpenGL remains the reference portable presentation lane. Prove context-bound
  texture ownership and cleanup; prove Raylib deferred deletion and Vulkan
  repeated retirement before changing their evidence state.
- Canvas2D needs approved scaling/alpha/minimize/restore/cache/replacement proof
  on each backend claimed as presenting. A build, descriptor, or safe refusal is
  not pixel evidence.
- Do not change protected frame order, queue drain, overlay replay, subpass,
  depth, or present behavior without a bounded renderer mission and proof.

## Extensions And Packages

- EpochEngineExtensions is the public source/catalog authority for optional
  project add-ons, not an Engine plugin and not a package row itself. Extensions
  extend generated projects; EpochEngine ships as a complete linked engine.
- EpochGui and Engine Arcade are built in and do not appear as installable
  package rows.
- Heavy terrain, ocean, voxel, foliage, portal, imported prototype, and
  game-specific stacks remain separately admitted extension payloads. Do not
  claim descriptor-only entries are installed implementations.
- Terrain work must converge on the extension-owned terrain system while core
  keeps only stable descriptors, collision/query contracts, and safe fallback.
- Local Qwen/Nemotron packages remain explicit, resumable, exact-hash,
  executable-local installs. Selection does not auto-start inference, a server,
  a listener, or copy weights into projects/builds.
- Anything that can bind a port, host, listen, execute downloaded native code,
  or expose a control surface requires separate visible human approval.

## Release And Site

- Source, Windows/Linux/macOS packaged authorities, exact commit/tree, build
  receipts, executable hashes, archive hashes, manifests, signatures, cards,
  catalog rows, and docs must never describe conflicting releases.
- Require faithful local Windows MSVC and managed-Clang Linux production proof
  before Site admission. Native interaction/pixels remain separate evidence.
- The Site task stores protected source/runtime objects and signed admission;
  it does not compile Epoch and must not activate partial platform evidence.
- September 5 authorization: publish the next release through the Site-owning
  task only after its build, runtime, visual, and safety checks pass. This is not
  authorization to publish current partial evidence or promote AI sandbox source.
- Preserve immutable older runtime/source/package objects for rollback while
  current public cards show only the accepted current release.
- Every meaningful known-good source tranche gets one rollbackable commit.
  Avoid release churn: package and publish only after the bounded source,
  contracts, platform builds, artifacts, sidecars, and operator evidence agree.
- GitHub is not the Epoch release authority. Its current account-suspension 403
  must be reported, not bypassed.
- Remove temporary grants, secrets, plaintext/ciphertext staging, duplicate
  archives, build clones, and verification debris after publication, while
  preserving canonical release inputs, rollback objects, and one exact recovery
  checkpoint.

## Deferred Product Missions

- Cross-platform capture plus operator-started local STT/TTS adapters for voice.
- Backend-native portals, traversal, audio, and physics policy.
- Broader material/model/effect authoring and portable compute.
- T2 explicit-resource parity, T3 GPU-driven features, T4 ray query, and T5 full
  ray-tracing pipelines.
- Persistent regions, collaboration, networking, planetary/astronomical worlds,
  and autonomous live-source replacement only after separate product/security
  designs and explicit gates.

## Delivery Discipline

- Preserve unrelated dirty work. The reviewed `ai.mcp_supervisor_*` source is
  now part of the Engine's bounded inbound supervisor contract; do not confuse
  that protocol surface with permission to enable a listener or transport.
- Implement production source first, run the closest faithful build/contracts,
  record only remaining work here, and move proven completion into the changelog.
- Never use placeholders, fake UI, model self-attestation, dead controls, hidden
  training, duplicate wrappers, or capability claims as progress.
- Keep docs synchronized with source and tests. A completed mission must not
  remain in this cache, the roadmap, or AGENTS.md as pending work.
