- The v0.89.35 acceptance repair extends direct tab insertion through native
  routed-pane drags. A floating pane remains a distinct top-level window until
  deliberate button release, renders exact target-strip insertion slots in the
  parent editor, publishes the selected route/region/index to the Win32 host,
  and redocks at that exact tab position. The held native window uses 50%
  opacity while moving, and native/title/splitter/floating-resize grabs capture
  editor input so scene selection and camera navigation cannot leak through.
- Global Undo/Redo now resolves one typed authoring-history controller over
  actual World, project-GUI, Plant Lab, and code-document journals. The visible
  document wins when it can perform the transition; otherwise the most recently
  changed compatible branch is selected. Plans bind generation, revision,
  cursor, and controller revision before the owning journal executes, so stale
  or cross-document transitions fail rather than mutating another surface.
- Package Manager sizing now follows the editor viewport instead of remaining
  a fixed small dialog. Its list and detail regions consume the available body
  budget while the progress/actions footer stays reachable. Layout preference
  schema 5 migrates only untouched legacy 68/32 bottom columns and 24% bottom
  dock defaults to the current 55/45 columns and 28% dock, preserving operator-
  customized divider positions.
- The v0.89.35 acceptance repair extends direct tab insertion through native
  routed-pane drags. A floating pane remains a distinct top-level window until
  deliberate button release, renders exact target-strip insertion slots in the
  parent editor, publishes the selected route/region/index to the Win32 host,
  and redocks at that exact tab position. The held native window uses 50%
  opacity while moving, and native/title/splitter/floating-resize grabs capture
  editor input so scene selection and camera navigation cannot leak through.
- Global Undo/Redo now resolves one typed authoring-history controller over
  actual World, project-GUI, Plant Lab, and code-document journals. The visible
  document wins when it can perform the transition; otherwise the most recently
  changed compatible branch is selected. Plans bind generation, revision,
  cursor, and controller revision before the owning journal executes, so stale
  or cross-document transitions fail rather than mutating another surface.
- Package Manager sizing now follows the editor viewport instead of remaining
  a fixed small dialog. Its list and detail regions consume the available body
  budget while the progress/actions footer stays reachable. Layout preference
  schema 5 migrates only untouched legacy 68/32 bottom columns and 24% bottom
  dock defaults to the current 55/45 columns and 28% dock, preserving operator-
  customized divider positions.
- Direct Visual Studio-style tab insertion is now additive to the existing
  docking system. Exact tab rectangles feed EpochGui's variable-width slot
  planner; same-group reorder and compatible cross-group moves preserve active
  tabs, close behavior, keyboard order, remembered groups, snapshots, and
  layout preferences. A tab-strip hover owns the marker/ghost first; otherwise
  the existing outer guides, native popout, redock, and no-guide float path run
  unchanged.
- Dark Factory validation failures now enter the real bounded repair cycle.
  Exact failed build/test receipts are distinguished from malformed or forged
  receipts, bounded diagnostics are digest-bound into durable evidence, resume
  preserves repair readiness without replay, and the orchestrator receives one
  `passed=false` transition requiring a fresh proposal. No Git, upload, release,
  listener, or hidden mutation authority is added.
- The code workspace now owns stable document handles and tab order, bounded
  UTF-8 find/replace, memory revert, revision-bound diagnostics, deterministic
  session state, and stale-evidence refusal. Session persistence never embeds
  unsaved source or diagnostics and never writes source implicitly.
- The local v0.89.35 hotfix repairs the exact Package Manager layout failure
  captured after v0.89.34. It restores the content-column origin after the
  right-aligned Refresh action, measures list/detail/footer regions from one
  modal body budget, anchors the progress/actions footer, and keeps model
  verification visible without presenting a fake cancellable state.
- Package discovery now matches the repository's original ownership model.
  EpochGui is linked into every non-CLI Epoch application and is never an
  installable row. EpochEngineExtensions is the public add-on catalog/source
  authority, not an engine plugin or a package itself. Engine Arcade remains
  built in. Project add-ons enter only as individually admitted rows; Site v72
  publishes nine such descriptors/model entries and no container-library row.
- Qwen3.8 27B and Nemotron 3 Nano 4B BF16 now have real explicit, resumable,
  exact-hash Package Manager transfers into immutable executable-local
  `cache/models/<package>/versions/<revision>` snapshots. Publication is
  receipt-bound and atomic, corrupt staging is recoverable, and selection does
  not activate inference, start a listener, or write model bytes into projects.
- The obsolete native engine-plugin loader, ABI header, module, source,
  configuration switch, generated-project switch, and aggregate test hook are
  removed. Optional add-ons compile into generated projects or run as explicit
  child tools; EpochEngine itself remains a complete linked engine.
- Public source discovery and Windows/Linux packaged runtime are v0.89.34 from
  exact Engine commit `5a5c5f26af8e96317efd92f8a104187606db39e4`.
  Local v0.89.35 remains unpublished. Fresh Windows Debug/Release editor builds
  and both build-safe aggregate contracts pass. Managed Clang 22.1.8 now links
  the full Linux Release engine after the embedded CMake target registers the
  EpochGui docking implementation; the module-owned direct-tab CTest and Linux
  aggregate engine contract pass. Standalone EpochGui and
  EpochEngineExtensions also pass their focused suites. Operator eye testing,
  final immutable Windows/Linux packages/receipts, and explicit Site activation
  remain open. macOS remains v0.89.30.
- The published v0.89.34 visual-authoring repairs remain the product baseline:
  nested dock clipping, exact legacy root-only GUI migration, truthful
  no-source Timeline presentation, shared surface-relative placement, and
  tapered Forest morphology. The latest real Epoch Editor screenshots remain
  default-layout authority; earlier concepts remain density inspiration only.
- EpochEngineExtensions terrain remains a separate package boundary. The first
  truthful terrain payload is a deterministic bounded local-heightfield
  generator over `terrain.foundation`; planetary terrain, native rendering,
  streaming LOD, and automatic execution remain unclaimed until implemented
  and separately admitted.
- The v0.89.33 hard-gate checkpoint restores saved GUI projections, blocks
  background editor input while any modal is active, routes global editor
  commands to their owning document, replaces the hardcoded Package Manager
  list with the bounded Epoch Site catalog, and removes Engine Arcade from the
  engine-package workflow. EpochGui v0.89.30 owns the reusable modal arbiter;
  EpochEngineExtensions has an exact descriptor-only companion subtree with no
  installable or automatically executable payload claim. The curated Site LLM
  lane has exact no-weights descriptors for Qwen3.8 27B and Nemotron 3 Nano 4B
  BF16 only; both retain local admission and explicit operator approval.
- The explicitly authorized v0.89.33 release pass is complete on Site v55 from
  exact Engine commit `a392fd03a2ba5e6a5ce4742ff62e719f1a0c3f16`.
  Windows and Linux packaged authorities, source discovery, validation
  receipts, checksums, and the signed release envelope all resolve to v0.89.33.
  macOS remains v0.89.30 and no macOS runtime claim is made.
- The post-release receipt-writer repair hashes the exact UTF-8 receipt bytes
  it writes, including the final LF, and derives the sidecar from that same byte
  sequence. Windows PowerShell 5.1 and modern PowerShell self-tests pass. This
  repair is unreleased and does not mutate the published v0.89.33 artifacts.
- The unreleased Project/Input repair now publishes canonical input source and
  compiled artifact through one matched-pair boundary. It validates and stages
  both members, rechecks the source preimage, commits authored source ahead of
  the disposable artifact, and reports the permitted source-ahead partial state
  for regeneration. Corrupt or conflicting Library artifacts cannot outrank a
  valid authored source. Focused build-safe contracts prove fresh-store keyboard
  evaluation, persisted controller-axis dead-zone behavior, artifact-ahead
  repair, and exact Restore Defaults reopen. No GUI or physical device launched.
- The August 28 v0.89.32 churn checkpoint is source/contract evidence, not a
  runtime release or GUI eye-test claim. Canonical generated-game input and
  project editing landed at `38d541b2`, `408c496f`, `bbda8b7b`, and
  `de937045`; deterministic local-build admission receipts landed at
  `148fffa0` with the documented host contract at `eadfea94`.
- The guarded project/self-iteration control plane now has committed checkpoints
  for the durable campaign queue (`445bc3f0`), MCP scheduler
  (`9ea6deaa`), project session admission (`8ab6cc5c`), canonical restore
  (`7711648d`), supervisor control (`c101507d`), curated context bundles
  (`c8e1fdb8`), deterministic source proposals (`2daec382`), and the guarded
  source patch stager (`a39ea309`). The operational editor campaign surface is
  `94c7357c`; sealed patch review is `60ce0032`. These contracts preserve
  explicit operator authority, replay/stale refusal, and project/source
  boundaries; they do not grant Git, upload, promotion, or release authority.
- Editor-shell source checkpoints in the same line are the World Outliner
  hierarchy (`6deaf036`), responsive non-clipping workspace tabs
  (`4cedf62b`), and repaired project source workbench (`73c86889`). Debug and
  Release build-safe contract evidence exists for the admitted churn, but no
  exact-build GUI eye-test is claimed here.
- Renderer frame-pacing authority is centralized at `1417a8e4`: all registered
  contexts consume the same capability/policy contract and the desktop target
  is 120 Hz where supported. SDL capture/loading/first-present alignment is
  committed at `c937fa2f`; a distinct minimize/restore slice is still in
  progress and remains uncommitted and unclaimed.
- Standalone EpochGui `v0.89.30` is public on Epoch Site v55 from hosted
  commit `b97167423373b9a7af3f821dcf91d8a71613dbf2` (tree
  `07c90cea87d8b2942175b62ef93444b38f040803`). An independent live clone,
  Release build, and 11/11 tests passed; v0.89.29 and v0.89.28 remain
  available.
- ParticleEngine PR-009 is a separate local-only package checkpoint at
  `98d10d41c3e2d534e7023014a79333ba121b362b` (tree
  `4658b09c3c633017ddf68c435b4e737043a3311d`). Its
  `EpochParticleEngine-v1.0.0-source.zip` is 415,520 bytes with SHA-256
  `180950e3a8f0b2d29d4902ed014f8de1332874c46f85a295dcd0733fef07b502`.
  It has no remote push, tag, upload, or Site publication; Linux/macOS native
  proof and human eye testing remain absent. EpochEngine adoption is package
  gated.
- `ai.mcp_supervisor_adapter` is implemented in five local files but remains
  unregistered, unbuilt, and unpublished pending fresh approval for shared
  build metadata. `source_iteration_worker` remains an audit/in-progress lane
  with no commit and is blocked on missing prerequisites; the
  `disposable_sandbox` design is still pending. Those five untracked files
  were excluded from exact commit `a392fd03a2ba5e6a5ce4742ff62e719f1a0c3f16`;
  public EpochEngine source discovery and packaged runtime remain v0.89.33.
- The local v0.89.32 source candidate advances both source-version authorities,
  source-build Windows metadata, updater discovery fixtures, validation receipts,
  and AI campaign evidence together while every packaged-version authority and
  published runtime remains v0.89.30. The AI development panel now consumes the
  real updater source authority instead of carrying a release literal. The
  D3D11/Vulkan readback evidence checkpoint at `102bb9135ed6bf452ac227d14fa56b03d14f76b7`
  is included in this source line; publication remains a separate human-owned pass.
- Engine Development now binds explicitly shared source to a typed non-GUI
  self-iteration pipeline. Verified checkout/cache authority, current curated
  hashes, strict project profiles, durable campaign restoration, guarded local
  or external MCP transport, manual proposal review, one-receipt disposable
  patch application, bounded repairs, and candidate-bound Debug/Release/
  Headless/full-validation evidence all retain exact generation and digest
  bindings. Ambiguous host curation stops at `selection_required` with zero
  bytes sent. The final local-build aggregate is deterministic Site-readable
  admission JSON with upload and release authority explicitly false. Live
  promotion and Site publication remain separate human-owned transactions.
- The v0.89.31 source-only updater checkpoint is now the active private-source
  authority on Epoch Site v44 from exact Engine commit
  `7dd0be88c53338614fb702583ce2985ecb5c149e`. Source identity and the
  updater-parsed compatibility module advance together while Windows, Linux,
  and macOS packaged-version authorities intentionally remain v0.89.30. The
  build-safe discovery contract proves source 31 is newer than packaged 30,
  packaged 30 is newer than the prior 29 authority, and discovery still uses the Site.
  Fresh Debug and Release builds/contracts pass; source-build Windows metadata
  reports 0.89.31. Site v44 activated exact committed Windows ZIP and Linux
  tar.gz siblings only after independent encryption, live-edge hash, and offline
  decrypt verification. Packaged runtime and signed integrity remain v0.89.30,
  preserving the intended source-only update test and all prior rollback rows.
- The updater-equivalent managed Clang 22.1.8 Release lane builds the full Linux
  engine in 1325 steps with source delivery enabled. The 24,731,808-byte
  executable has SHA-256 `ef6e880195c7fd596685bba57642eaacd21c9bc9270ac3e79200374c9086854e`,
  reports v0.89.31, and passes the aggregate build-safe engine contract without
  opening a renderer or GUI.
  All 33 no-display Linux CTests pass.
- The local v0.89.30 editor/library identity baseline is source- and build-proven.
  `ConsoleApplication1` is now `EpochEditor`; `StaticLib1` is now `EpochEngine`.
  Solution/project paths, updater source target, generated-project links,
  runtime paths, tools, ignores, and docs are aligned. The sole old library
  string is an intentional generated-child migration input. Template-only
  `.vcxproj.user`, `fake()` anchor, framework/PCH files, and `.codex/.gitkeep`
  are removed.
- Source naming (495 files), stale-name audit, and whitespace checks pass.
  Debug and Release `EpochEditor` builds pass and both build-safe
  `--engine-contract-self-test` lanes exit 0 without launching a GUI. Exact
  artifact sizes and SHA-256 evidence are preserved in `Changes/mission_cache.md`
  and the v0.89.30 changelog.
- The main workspace strip now uses explicit full-label widths and a 34-pixel
  height. EpochGui now also owns responsive tab-strip planning and an engine
  adapter-backed overflow selector, so narrow workspace and reviewed-source
  strips retain the active route and keep every hidden route selectable instead
  of clipping Assets, Systems, or later files outside the window. The reusable
  planner also owns deterministic previous/next/first/last traversal with
  disabled-route skipping and explicit wrapping; the main strip opts into
  Ctrl+Tab/Ctrl+Shift+Tab without changing menu, run, or dock ownership. The
  combined Code / AI Development label remains whole either on the strip or in
  its explicit overflow selector.
- Direct `EpochGui.TextControl` CTest passes for full-width, constrained-active,
  overflow-only, and deterministic keyboard-navigation layouts. Fresh MSVC
  Debug and Release `EpochEditor` builds
  pass their build-safe aggregate contracts without a GUI launch. The rebuilt
  executables are 30,265,344 bytes / SHA-256
  `7238606367b635289bebb404ae968fcff848500201f808b49b25bd9fcc1886a8`
  and 9,596,416 bytes / SHA-256
  `ef50e4836ccb555ad95b233d8b6c5f052248e109c1ee51b097ffec4f46f90903`.
  Exact-build eye evidence remains an operator gate.
- The binary-first v0.89.30 runtime is published from exact commit
  `5d6fcf982d9d8e062d0dc919502444bb5cf3458d`. The Windows ZIP is 29,736,248
  bytes with SHA-256
  `fb222ac7ae0ed21ce4f231c30e942db82f0a6226c8f7c569016eb985af70580a`;
  it contains exactly one root `EpochEditor.exe` and no legacy target, Git,
  log, or cache paths. The Linux tar.gz is 31,119,632 bytes with SHA-256
  `bc5cedb8e59614d8dc38327a1657e4fcd63f5cd0b82dccc576365f70b6e60435`
  under one package prefix with the same debris checks. Both staged packages
  report v0.89.30 and pass the build-safe aggregate engine contract. The Linux
  release is build/contract-proven only because native renderer smoke was
  explicitly skipped. Site v43 independently re-downloaded and verified both
  immutable packages before making v0.89.30 the signed latest runtime.
- The identity baseline is checkpointed at exact local commit
  `2d0f3c9a1df7a7ca52eb0419b01840ece80483cc`. Published v0.89.30 now carries
  `EpochEditor`, satisfying the binary-first compatibility prerequisite. Activate
  only a later exact v0.89.31 source checkpoint to test the renamed rebuild lane,
  and keep each Site release/checkpoint visible until explicit cleanup.
- Continue substantial concept-guided parity cycles
  across shared EpochGui shell primitives, editor hierarchy/inspector/timeline/
  task/evidence composition, engine-backed data, and every supported renderer
  context. Local Debug/Release/headless admission precedes each intermittent
  Site release; cinematic assets or unsupported features are never fabricated.
  Third-party visual/audio content may enter a commercial package only from a
  professional source with an explicit compatible commercial-use license,
  retained origin/version/license evidence, and deterministic package admission;
  generated concept art remains design reference only.
- EpochEngine source distribution remains a restricted development boundary for
  `v0.89.33`; Site source discovery and packaged runtime are both `v0.89.33`.
  Smart Update is binary-first; authorized encrypted source is its
  missing/failed-package fallback and remains available explicitly for a local
  rebuild or project-cache extraction. First native enrollment uses explicit
  browser approval; later access can use a short server challenge signed by a
  persistent OS-protected ECDSA P-256 key. Archive key wrapping remains a fresh
  per-operation P-256 ECDH/HKDF exchange. Signed private metadata, AES-256-GCM
  archive authentication, hidden restricted temporary storage, and cleanup
  after handoff remain mandatory. Anonymous engine Git/source archives stay
  unavailable, no static source credential ships in Epoch or browser code, and
  a binary-only compile policy can remove all source lanes. EpochGui remains
  independently public.
- Historical Site v43 runtime authority came from exact Site commit
  `1b7ef1fce6a9bc1fdd00dc7b7655494b99e3546b`; its private-source artifact/device
  state retains the compatible v34 contract at environment revision 7.
  `/admin` remains owner-only through direct Sign in
  with ChatGPT. Remembered devices authorize until explicitly revoked; per-update
  browser approval remains disabled and unavailable to the shipped v0.89.29
  client. Current source checkpoint
  `3b874ad87e3525703b870ca22134b5004dfb5b67` has two active same-commit
  platform siblings at that checkpoint. Windows artifact
  `epoch-engine-v0.89.30-windows-x64-3b874ad87e35` is an exact 899-file ZIP:
  plaintext is 60,240,968 bytes with SHA-256
  `2cf6d0d937a52e14639f35ad5ffcf4669031959283cbd60706c797020b48ca19`;
  ciphertext is 60,240,984 bytes with SHA-256
  `a40d3969a6ad8da798cf78a9480cbd4ed1924449e288991e5917384888bce751`.
  Linux retained the exact same-commit tar.gz sibling. Signed manifests used
  platform-correct formats, lowercase hexadecimal SHA-256 fields, and the legacy
  literal-backslash-n AAD descriptor required by v0.89.29. Packaged runtime
  v0.89.30 and signed integrity were live; v0.89.29 and five older source
  artifacts remain available for rollback. No bearer, private key, DEK, identity, device
  code, or private response is recorded here.
- Historical Site v31 came from exact Site commit `46be28d97a76d51c6b35d44fa7cd4a63f6b2c51e`
  at environment revision 7. `/admin` is an owner-only CMS using direct Sign in
  with ChatGPT;
  native device codes are never CMS credentials. Extended native starts advertise
  `epoch-source-device-enroll/v1` only after every enrollment field validates;
  legacy starts remain unchanged. Persistent ECDSA P-256 challenge/token routes
  reject unknown devices, malformed requests, and challenge replay. The owner CMS
  exposes device revocation and anonymous aggregate download counts without
  storing identity, IP, token, or user-code telemetry. Public runtime
  discovery/integrity and the source sentinel are signed/published `v0.89.29`;
  public EpochEngine Git/source routes remain 404. Active private artifact
  `epoch-engine-v0.89.29-90805e528f3e` comes only from committed checkpoint
  `90805e528f3e462fdd47aa9394620922e83db268`: ciphertext is 59,807,212
  bytes with SHA-256 `30d035b99a9e6607491b78c44e45322729a66a1fe242a78abae81c03e37480aa`;
  authenticated plaintext is 59,807,196 bytes with SHA-256
  `bf2f411d95a49804a6d75799f31869e88ae229baf46578c43d3116db97bfd379`.
  Published Windows v0.89.29 is 29,731,299 bytes with SHA-256
  `65fdf23ea3567253122fca8de8ea172925c27e7614325d5208f4ed7ac92fdc46`;
  Linux is 31,114,697 bytes with SHA-256
  `c6895b1b717d4bab2969ca9e3dba712da4e23e020bc4834877137e42bd5ec6b3`.
  Earlier v0.89.29, v0.89.28, and v0.89.27 artifacts remain inactive for
  rollback. No bearer,
  private key, DEK, identity, device code, or private response is recorded here.
- `updater.system` and `updater.source_access` now expose lean declarations-only module interfaces. Crypto/network work, updater orchestration, and the generated Windows worker live in separate implementation units. Full Windows Debug and Release editor targets build; their build-safe contracts pass, including `updater.private_source_crypto_policy`; the opt-in binary-only Debug build and contract also pass before restoring the normal source-enabled Debug output. No GUI was launched.
- The editor main-surface row now uses an isolated compact workbench
  presentation with explicit full-label widths, one connected inactive strip,
  bounded hover/press feedback, a two-pixel active indicator, and a 34-pixel
  row height. Assets and Systems no longer depend on an exact-fit measurement
  that clipped their final characters. Other document/tool tabs retain their
  existing presentation. Windows Debug/Release builds and build-safe contracts
  pass without launching the GUI; exact-build eye evidence remains open.

- Hosted SDL creation now matches the renderer thread''s DPI-awareness context
  to the selected dock parent before SDL creates its native child, applies child
  window ownership before `SetParent`, and reports the exact Win32 error if
  parenting still fails. SDL atlas sampling now uses literal pixel coordinates,
  corrects SDL3 viewport-status handling, and establishes nearest filtering and
  alpha blending when each texture is created. The loading transition waits for
  the exact top-layer batch generation to be replayed by the active renderer. The
  Linux SDL host now processes pending atlas uploads and replays base GUI, scene,
  refreshed GUI, and top-layer batches in the same protected order as OpenGL and
  the managed SDL adapter.
- A later focused Debug SDL transition exposed a native access violation in SDL
  3.4.10 Windows IME teardown. Matching-PDB symbolization resolved the fault to
  `IME_Quit` during `SDL_QuitSubSystem(SDL_INIT_VIDEO)`. Epoch now serializes
  both managed and Linux SDL frames/video teardown against process-wide audio
  and gamepad subsystem initialization and shutdown. The build-safe
  `context.sdl_runtime_lifecycle_serialization` contract proves recursive
  renderer-thread entry, competing-thread exclusion, and admission after
  release. Linux SDL teardown also clears cached textures before renderer/video
  destruction and nulls every renderer alias; upload failures now publish a
  renderer fault instead of escaping the `noexcept` draw boundary. Debug and
  Release editor builds and build-safe engine contracts pass. An authorized
  local SDL rerun now closes from the native window without a crash or retained
  process.
- Windows SDL now separates dock/window size from framebuffer size, derives a
  high-DPI logical presentation from the SDL output and display scale, and
  applies that canvas through SDL3 logical presentation without changing the
  protected frame, queue-drain, scene, GUI-replay, top-layer, or present order.
  Both sampled Win32 cursor positions and queued host mouse events cross the same
  framebuffer-to-logical mapping. The build-safe
  `context.sdl_high_dpi_logical_presentation` contract covers native and 150%
  scale dimensions plus positive and negative coordinate mapping. Authorized
  150% Windows eye testing proves the SDL launcher, replay-backed 450 ms loading
  surface, standard editor, Systems workspace, live resize, World Script
  Browser, large source editor, wheel input, 120 FPS title, and clean close.
  Matched Windows OpenGL reruns with and without an explicit 120 FPS override
  report swap interval 0, sustain 120-121 measured frames per second with
  approximately 0.02 ms `SwapBuffers`, show 120 FPS in the native title, retain
  the current launcher/editor GUI through the transition, and close cleanly.
  This closes the observed OpenGL 60 FPS frame-policy/telemetry discrepancy for
  the current Debug candidate; broader OpenGL renderer evidence remains scoped
  by the renderer matrix.

- EpochGui dock-guide layout now emits both physical context targets in addition
  to four logical tab targets and the optional float target. Ordinary layouts
  place contexts on the upper guide row; detached full contexts center them in
  the parent destination previews, so hit selection and placement ghosts share
  the same bounded layout. Fresh Debug and Release editor builds and build-safe
  engine contracts pass.

- Goal Save no longer launches model work from inside the reusable console
  control callback. The action stages one request, dispatches it after the
  completed editor frame, and routed chats retain heap-stable ownership while
  contexts are inserted or removed. AI configuration remains synchronized,
  endpoint scans commit atomically, and in-flight requests retain client
  ownership.

- AI authoring and engine self-iteration now have reciprocal workspace routing.
  Engine Development opens only the isolated AI Development source surface;
  Authoring restores the last World or GUI Canvas surface, and `/plan` plus
  persistent-goal requests queue that same return when submitted from another
  workspace. Curated source sharing preserves the remembered authoring surface
  before opening reviewed files. A fresh Debug editor build and build-safe
  engine contract pass without launching the GUI.

- Guarded source iteration now treats a request for one source-proven defect in
  a named subsystem as a bounded diagnostic objective instead of accepting the
  first insufficient-evidence reply as final. The host permits exactly one
  evidence-preserving diagnostic recheck, then stops safely if the reviewed bytes
  still prove no repair. Every newly shared context clears stale packet/recheck
  state, and the workspace-completion message distinguishes the complete local
  disposable build copy from the small reviewed context sent to the selected
  model. The two reviewed OpenGL context sources now carry ASCII license banners
  and accurate source identity instead of persisted mojibake and stale `.ixx`
  labels. Root-level generated logs and the old capture were removed without
  touching executable-local model state or unrelated dirty work. A fresh Debug
  editor build and build-safe engine contract pass without launching the GUI.

- Epoch-owned release discovery, build admission, and runtime artifacts resolve
  through `https://epoch.adamrushford.chatgpt.site`; no automatic GitHub
  fallback remains for Epoch-owned content. Public EpochEngine Git and source
  archive routes are disabled. Authorized engine source is available only
  through the authenticated, encrypted device-flow boundary. EpochGui remains
  public at `/git/EpochGui.git` and bundled `Engine/dep/EpochGui` must remain
  byte-identical to it. EpochEngineExtensions remains fail-closed until reviewed
  hosted content is admitted. Runtime and source payloads retain their separate
  signed/hash-verified evidence contracts.
- The historical `multicontext-base-stable` branch is preserved at exact commit
  `ad6c416d930b348a61bc37ceb7d4522742be084a`; it is branch history, not the
  current release. `v0.89.06` assets remain immutable release history. The exact
  `v0.89.27` candidate now builds in Windows x64 Release and managed Clang
  22.1.8 Linux Release, passes the Windows build-safe engine contract and all
  33 Linux Release CTests, and its standalone EpochGui tree builds and passes
  all 10 tests. Final publication remains gated on staged-archive checksums and
  live endpoint verification; no GUI was launched for this proof.

# Active Pass

## Gate

Prepare the distinct v0.89.32 source-only checkpoint so a published v0.89.30
editor can exercise authenticated source discovery and acquisition.
Preserve the accepted editor work around explicit project sessions,
launcher-owned authoring applications, semantic GUI deletion, and a
process-owned physical controller boundary. The operator accepted the v0.89.23
tile-collision eye test; the earlier GUI, Assets, Scripts, Systems, camera,
workspace, and project-input surfaces remain protected checkpoints.

The bounded gate opens or switches projects through an explicitly selected
`project.epoch.json` manifest without rewriting it, removes generated-output
project discovery from the normal Project surface, and refuses project creation
before overwriting an existing manifest. Scene and GUI Delete, deselection, and
Undo/Redo share canonical document identity; GUI Canvas Save/Reload use the one
project GUI source; and timeline scrub/play/step controls operate on the shared
temporal spine. Tool windows now use independent left/right/bottom-left/
Global editor actions now resolve through the typed, data-only
`editor.workspace_commands` surface catalog. World, GUI Canvas, and Plant Lab
history remain document-local; unsupported surface actions are disabled with an
explicit reason and cannot silently mutate World. Captured revisions fail
closed, and AI Development has no command representation for approval,
promotion, release, Git, or unrestricted execution.

bottom-right tab routes,
visible guide targets and placement ghosts, exact Window-menu recovery, and
optional context-backed native floats that restore their remembered group from
native close or titlebar drag. Main document tabs and scene views remain outside
the tool-dock model. Closable tool tabs activate on press and drag the exact
pressed route; EpochGui derives readable tab width from font metrics while
reserving close affordances, destination stacks activate the moved tool, and
source stacks resolve a live fallback immediately. EpochGui also scopes reusable
control state by GUI host, owning window, and stable control identity, so
separate pane scrollbars cannot share drag/offset state or escape their viewport
clip.
Reusable console panes now own their transcript, input row, command button,
persistent task strip, and optional response, pinned, and footer action rows
inside one clipped layout. AI Chat therefore keeps one input surface at every
dock size, and any valid completed authoring plan containing exactly one
allowlisted call is staged once with Apply/Discard attached immediately below
the response before canonical scene or GUI mutation. `/plan` requests one
bounded proposal. `/goal <objective>` starts a persistent objective; the same
strip exposes Play/Pause, Edit, and Delete, and bare `/goal` resumes it. A
running goal stages its next separately approved milestone automatically only
after the prior milestone advances the canonical document revision. No-op or
failed plans pause at the current milestone instead of claiming completion.
When a goal needs an unapproved discovered model, consent stays attached to the
waiting AI Chat request and existing AI Controls; it must not dim the editor
behind a detached or unavailable modal, resend the request, or advance the goal
until the operator confirms the model.
Transcript rows carry semantic user, assistant, system, and error roles through
EpochGui; reusable text surfaces reserve descender/raster padding and draw a
contrasting focused caret without editor-specific text rendering. Native hosts
filter editing/control codepoints out of printable text delivery, so Backspace is
one erase command instead of an erase plus inserted control character. Canvas
widgets retain one stable drag owner until release. Camera helpers remain
outliner-selectable but do not own oversized viewport hit volumes; bare MMB pans
while Alt+LMB orbits and Alt+RMB dollies. Timeline playback anchors at synchronization
and advances by elapsed deltas rather than absolute engine uptime. AI request
workers publish through generation-checked shared state, and local API calls are
timeout-bounded. Goals fail closed above 24 milestones, one semantic command per
milestone, or eight bounded object consequences for that command. Scene and GUI
create calls are idempotent minimum-count requests, and the active goal retains
all attempted semantic signatures so reworded duplicate milestones cannot append
more spawns, cubes, or widgets.
Guarded engine-development source work is evidence-first. Before a model sees
source work, the host removes conversational filler, weights explicit subsystem
owners, ranks existing files under the selected read-only source root from the
approved objective, and displays at most six candidates without reading or
transmitting source bytes. An exact canonical objective path wins over vocabulary
ranking. The first exact primary file may use the 184 KiB source-evidence ceiling;
additional related candidates remain within the 128 KiB aggregate budget. Share
Engine-source curation includes embedded EpochGui source, module, public header,
and test roots. Text-input, edit-box, caret, and Backspace vocabulary maps to
`text_control` ownership so GUI input defects do not fall through to unrelated
engine metadata.
Curated Context is a separate explicit action: it revalidates the unchanged
objective, endpoint, canonical paths, and byte bounds, then sends only bounded
evidence derived from those reviewed UTF-8 files to the displayed selected
endpoint. Files at or below 48 KiB provide complete counted contents; larger
files provide one UTF-8-safe 16 KiB objective-centered excerpt. The action opens
the reviewed files in the large source workspace, where Project Scripts and 3D
Scene restore the normal authoring surfaces and live source remains read-only
unless the operator explicitly saves a manual edit. A model cannot request,
discover, invent, or expand paths; obsolete context-request output is rejected
and insufficient context stops the pass.
Source inference accepts only a line-boundary
`EPOCH_SOURCE_PATCH_PROPOSAL_V1` exact-block packet or exactly
`EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1`; framed or raw prose is rejected.
Exact search blocks must be unique in reviewed evidence, and the trusted host
reconstructs and hashes the complete postimage before staging. Generic logger,
singleton, entry-point rewrite, placeholder, stub, duplicate-wrapper, unrelated
cleanup, invented symbol/service/include, model-authored build/test claims,
no-op replacements, textual `.ixx` includes, destructive whole-file shrinkage,
and edits that remove license, module, or namespace ownership fail before
staging. Review Proposal, Approve Sandbox, Cancel Proposal, and Dismiss remain
attached to the relevant reply; AI Controls owns objective entry and read-only
path/model/endpoint/evidence inspection. A changed objective invalidates earlier
source evidence. Approved edits execute only in the disposable source sandbox
and cannot promote themselves into live source. Direct local source inference
runs below normal priority, on half the available logical CPUs, with matching
batch threads and `--gpu-layers 0`; ordinary AI chat retains its configured GPU
acceleration.
Direct llama.cpp source transport now prefers a structured patch envelope over
an earlier insufficient marker and extracts only the exact line-framed
header-through-terminator packet; surrounding transcript or model chatter never
enters the strict codec. Each completed source generation is ingested exactly
once, and a rejected packet receives at most two automatic host-diagnosed
correction attempts over the same reviewed evidence. The proposal prompt closes
its grammar example correctly, uses the first exact host-reviewed evidence path
instead of an invented placeholder, and, when the first two quoted objective
literals prove one unique replacement in that reviewed file or excerpt, shows
those exact bytes with correct no-newline flags. Ambiguous search blocks still
fail closed. An operator-approved live Qwen3.8 run reached the explicit Approve
Sandbox review gate at 120 FPS while live source remained read-only.

EpochGui owns one translucent-orange docking model for local tabs, native routed
panes, and physical renderer-context destinations. Routed pane popouts keep
their guides in context-local coordinates. Detached full renderer contexts
project no competing local guide set: EpochGui centers the left/right guides in
the actual parent destination previews, and the engine parent host paints the
same hovered preview as the placement ghost. Drawing, hit testing, and release
selection therefore share one layout without changing backend lifecycle or
frame order. The lower pair targets ordinary Bottom Left and Bottom Right tab
stacks; the upper side pair preserves and grid-docks a physical context on the
chosen side. Output and AI Chat initially occupy the two bottom stacks, but both
remain ordinary movable panes and either stack may hold any compatible route or
collapse when empty. Standard Editor, Plant Lab, and GUI Editor persist pane
visibility, four-region placement, active tabs, the bottom split ratio, theme,
and rounded-control preference in bounded versioned per-user state. The restored
theme table includes true Light and named Classic Launcher, Midnight Blue, Ember
Forge, Forest Terminal, and Aurora Steel palettes; rounded controls default on.
Routed-window collection retires the context, joins finished render threads,
clears command queues, releases native GL/DC/HWND ownership, erases editor/chat/
preview state, and resets shared projection/scoreboard state at shutdown.
On Windows, managed SDL close requests enter manager retirement instead of
destroying a backend child from the host thread. SDL accepts window events only
for its owned window ID and releases its native child on the renderer thread
before the manager destroys the surrogate host. Native multicontext shutdown
remains unclaimed until operator eye-testing.
Plant
Lab remains a
dedicated launcher editor rather than a generated project. SDL3 gamepads publish
renderer-neutral, generation-checked snapshots exactly once per engine frame;
Project Controls shows provider and connection evidence. Mapping those snapshots
into authored project actions remains a separate follow-up from physical polling
itself. Desktop hosts now sample the usable monitor work area and capability
tier before creation, center one stable launcher/editor host, and use distinct
4K, 2K, 1080p, and compact client policies without a mode-change resize jump.
Low-resource mobile/deck tiers retain proportional host scaling while renderer
budgets own internal workload scaling. Windows selects the monitor under the
launch cursor; Linux consumes the EWMH work area when available. Explicit
dimensions, generated game windows, and the sealed updater shell remain
authoritative exceptions.

Debug and Release source builds plus build-safe contract proof are required.
Native selector interaction and layout remain unclaimed until the operator
eye-tests this candidate. Package and tag only the exact reviewed source and
staged Windows/Linux artifacts whose checksums are published and independently
verified; do not convert build evidence into native interaction evidence.
## Release Baseline

`v0.89.33` is the current development source and published binary-first
Windows/Linux runtime from exact source commit
`a392fd03a2ba5e6a5ce4742ff62e719f1a0c3f16`. The Windows archive is
30,605,050 bytes with SHA-256
`865ec229cb2b867198c137d16c06ca9f6959bece02f5be3dd0e53b4814ed763d`.
The Linux archive is 32,343,875 bytes with SHA-256
`89b0ef6c40a948a91cd5a015a8e8649550f24e38a443fd91a1f2938f60f177fa`.
Site v55 independently verified the immutable packages and exact-file
validation receipts, then made v0.89.33 the signed latest runtime and activated
the paired private-source artifacts. Published v0.89.30, v0.89.29, historical
v0.89.28, v0.89.27, and immutable v0.89.06 remain available for rollback. The independent historical
`multicontext-base-stable` branch remains fixed at
`ad6c416d930b348a61bc37ceb7d4522742be084a`. EpochGui remains a public Site
repository while EpochEngine development source
and the historical branch are restricted. The private EpochEngine tree still
carries the exact reviewed EpochGui subtree.
Preserve these accepted source contracts:

- normal editor operation owns one logical active backend; physical contexts
  remain movable and multicontext remains diagnostic;
- whole-editor context replacement preserves editor state and retires the old
  backend instead of opening another editor;
- Engine Arcade owns the sampled render-to-texture graph proof and procedural
  fallback;
- renderer capability reporting separates descriptors, graph build, native
  allocation, presentation, benchmark, and production evidence;
- reusable GUI state belongs in EpochGui while native hosts and backend drawing
  remain engine adapter responsibilities.

## Current Source Truth

The working tree contains these current or in-progress foundations:

- `capability.profile` extends existing capability, budget, `perf.tier`, and
  render-device vocabulary with backend-neutral tiers, feature/evidence states,
  per-subsystem profiles, project requirements, deterministic selection, and
  build-safe no-overclaim checks;
- `platform.budgets` now owns the single performance-tier recommendation
  function consumed by runtime and capability adapters. Project profiles carry
  typed renderer requirements and explicit experimental/software-fallback
  policy; recommended budgets remain distinct from unknown measured cost;
- generated manifests persist `capability_profile`. Missing legacy fields map
  to the portable default without rewriting project files, while duplicate,
  wrong-type, malformed, unknown, and profile-mismatched values fail closed;
- Project, System Info, Settings, and the status dock distinguish active-editor
  admission from selected project-run admission instead of certifying one
  backend with another backend's evidence;
- `render.math` owns shared renderer-neutral vectors and linear color;
- `render.lighting` owns generation-checked light identity, bounded registries,
  immutable frames, environment state, metrics, and reference raster lighting;
- editor and project preview map scene lights through the shared lighting frame
  while retaining truthful reference-solid output until native shading lands;
- `render.ray` owns validated CPU AABB, sphere, triangle, scene, and voxel-DDA
  queries; editor selection uses it and Focus changes the preview camera;
- `physics.manager` owns stable bodies, bounded deterministic commands,
  fixed-step commit boundaries, snapshots, restoration, and metrics;
- `physics.solver2d` composes that manager into a deterministic AABB/circle
  baseline with static, kinematic, and dynamic bodies; solid, upward one-way,
  rising-right, and falling-right static surfaces; layer/mask filtering; stable
  contacts; bounded static maps; pause/reset; snapshots; and replay proof;
- `audio.manager` owns clips, sources, buses, listener/spatial state, temporal
  scheduling, mix plans, and metrics. `audio.mixer` resolves bounded decoded PCM
  into deterministic stereo frames, `audio.device` owns the physical boundary,
  and `audio.playback_runtime` keeps one optional SDL3 device alive across
  renderer replacement while project scenes own generation-checked sessions.
  `project.audio_profile` now owns content-addressed WAV import, canonical bus
  and cue semantics, volume/mute/loop/autoplay controls, jump/land event
  bindings, exact source/artifact validation, atomic project-local source and
  derived Library publication, and compiled-only restore. Project Audio edits
  that same profile from Project and Assets. A present but invalid source fails
  closed instead of falling back to an older artifact;
- `voxel.storage` and `water.system` own deterministic sparse/reference state
  without claiming native rendering;
- `scene.tier0` and terrain foundations establish reusable default-scene data;
- `authoring.document` owns shared generation-checked document identity,
  deterministic content revisions, and bounded history policy;
- `authoring.gui_document` owns stable widget hierarchy, typed content,
  layout/style/interaction meaning, semantic operations, deterministic revision,
  bounded undo/redo, snapshots, validation, and metrics. Its bounded binary codec
  covers the complete descriptor graph, verifies SHA-256 integrity, rejects
  malformed/over-budget input, and reconstructs the exact revision;
- `authoring.gui_document` also owns the shared Blank Canvas, Desktop App,
  Dashboard, Mobile App, and Game HUD factories. Editor templates and generated
  project defaults consume that one semantic builder rather than duplicating
  widget layouts in application code;
- GUI Editor restores and atomically publishes canonical
  `<project>/Assets/Gui/main.epochgui` source. Scene entities are its current
  preview projection; Save Project, Build, and Run require verified GUI and
  scene publication. Save compiles the accepted document revision into a
  validated immutable artifact under `Library/Gui`; ProjectPlayScene restores
  that exact logical source through the project GUI runtime and EpochGui
  adapter. Widget activations and normalized sliders may enter the compiled
  project input profile only through exact canonical semantics. Unknown actions
  and invalid event/value-kind combinations fail closed, `editor.return` is the
  sole host command, focused GUI controls suppress physical gameplay sampling,
  and admitted impulses are consumed once on the next deterministic project
  frame. Arbitrary application/script command dispatch remains unfinished;
- GUI Editor is a projectless launcher application over the same canonical GUI
  document used by the Standard Editor. The dedicated application owns widget
  creation, full Canvas/Runtime Preview/Component Graph/Styles authoring, native
  `.epochgui` Open/Save, and verified built-in/user templates; the embedded GUI
  Canvas exposes project placement, hierarchy, transform, history, and deletion
  without duplicating the authoring implementation;
- GUI Canvas Save/Reload and semantic Undo/Redo/Delete operate on that canonical
  document. Reload refuses to discard unsaved revisions; scene projection is
  reconciled after each accepted operation;
- World Edit owns scene-document Undo/Redo, while global history and Delete
  resolve the active document and defer whenever a reusable GUI control owns
  keyboard input. Empty scene clicks, Escape, and explicit Deselect clear stable
  selection rather than silently selecting the first projected object;
- Open/Switch Project admits an explicitly selected existing manifest without
  rewriting it. The Project surface no longer offers a generated-profile picker,
  Close releases project-bound state without deleting source, and shell creation
  refuses to overwrite an existing manifest;
- compact and full Timeline controls scrub, pause, resume, and step the shared
  time spine rather than maintaining per-window display-only playheads;
- the main application strip is document-only. World Outliner, Asset Browser,
  GUI Hierarchy, Script Browser, Tile Map, Properties, World Settings, AI
  Controls, Output, and AI Chat are independent tool routes that move among
  left, right, and bottom groups. Project, Assets, AI Output, and Systems evidence are persistent
  filters inside the single Output route rather than duplicate movable panes.
  The host draws target guides and a placement ghost before release, persists
  exact route placement and the selected Output filter through context
  snapshots, and can present a tool in its context-backed native window. Native
  close and titlebar redock return that route without logging out of the editor.
  Window exposes per-tool recovery; named Map, Scene, AI, and Systems secondary
  outputs remain explicit Window/Settings choices. Automatic monitor placement
  remains unfinished, and native interaction remains an operator eye-test gate;
- `scene.document` owns stable scene object identity, typed scene components,
  semantic operations, atomic transactions, undo/redo, Tier-0 construction,
  and deterministic snapshot projection;
- live editor create, duplicate, delete, drag completion, camera reset, helper
  visibility, script rotation, Canvas2D creation, Arcade/Plant Lab preview
  synchronization, and Forest Factory placement now enter one typed command
  gateway. Atomic document transactions own durable meaning and history; the
  entity vector is rebuilt from the committed projection for rendering and UI;
- `scene.interaction` resolves ray selection, drag ownership, and Focus through
  persistent scene object IDs rather than mutable vector positions;
- `scenesnapshot`, `sceneserializer`, and `scene.persistence` own canonical
  `epoch_snapshot 2`, bounded validation, migration-only legacy readers,
  verified temporary writes, and atomic replacement. `scene.runtime` compiles
  that exact revision into the renderer-neutral project-preview projection;
- explicit Save, Play, Build, and Run paths fail closed when durable scene
  evidence cannot be committed or accepted by the runtime projection;
- `authoring.texture` is the first four-layer authoring vertical slice: stable
  document meaning, semantic history, sparse layers and tiles, deterministic
  owning RGBA8 mip artifacts, bounded deterministic source serialization,
  integrity-checked exact restoration, and disposable
  standalone/atlas/bindless/sparse residency planning. Planning is advisory,
  defaults to standalone-only, and does not count physical choices as runtime
  capability evidence. `round_path_v2` converts sparse pointer control samples
  into deterministic pressure/tilt-interpolated execution stamps at bounded
  half-radius spacing, with a quarter-pixel floor. Derived execution work must
  fit the document sample budget before tile discovery or allocation, while the
  semantic journal retains only the original path intent. The Assets texture
  canvas uses this continuous brush; `round_stamp_v1` remains available for
  exact legacy/programmatic stamp replay. EpochGui opacity/hardness sliders and
  RGBA channel toggles enter that same semantic stroke descriptor; the editor
  refuses to disable the final active channel. MSVC Debug/Release and managed
  Clang 22 plus all 32 CTests prove deterministic gap fill, raw-intent retention,
  unchanged legacy spacing, soft-versus-hard edge coverage, selective channel
  writes, exact property journaling, and atomic over-budget/invalid-mask refusal.
  Canonical per-layer integer translation, horizontal/vertical mirroring, and
  deterministic grayscale/invert/signed-brightness filtering now remain authored
  meaning while compilation alone derives transformed pixels. EpochGui exposes
  those properties without rewriting sparse source tiles. Source schema 2 stores
  a generation-checked layer-extension table; schema 1 is integrity-checked and
  migrated in memory without rewriting the source. Contracts prove unchanged
  canonical pixels, exact transformed output, undo/redo artifact restoration,
  exact schema-2 reopen, and atomic malformed/integrity refusal. Native pointer
  feel and control layout remain eye-test gates. Compressed and
  color-conversion lanes fail closed;
- `project.asset_registry` now owns deterministic path-derived runtime project/asset
  identity separately from authoring documents, compiled content,
  filesystem case behavior, and
  physical caches. It normalizes portable logical paths, rejects traversal and
  case-fold collisions, issues generation-checked handles and deterministic
  keys, and publishes explicit current source revisions;
- project.texture_resources is bound to one validated project registry,
  authenticates compiled texture source revision, seals full artifact identity,
  and issues Canvas2D references from project asset key plus content-derived
  artifact revision. Foreign project operations fail before handle resolution,
  including identical generation/index handle bits;
- the same project-bound service owns bounded decoded T0-CPU resource sets and
  may acquire disposable native residency through render.texture.artifact.
  Current execution remains an explicit linear RGBA8 mip-0 lane;
- `project.texture_library` persists validated serialized artifacts beneath
  `<project>/Library/Textures` using deterministic project/path identity,
  bounded per-asset scans, verified temporary writes, atomic publication, and
  integrity-checked exact/latest reads. Portable case collisions fail before
  disk mutation;
- `project.texture_pipeline` is the single coordinator across Library output,
  the asset registry, decoded runtime publication, restore-on-demand, and owned
  Canvas2D resource leases. Recreated pipelines recover the same logical
  project texture without preserving physical cache state;
- `authoring.tilemap` owns stable tileset, palette, layer, cell, and object
  identity; sparse chunks; semantic operations; bounded history; undo/redo;
  deterministic snapshots; and reproducible compilation;
- `asset.tilemap_artifact` owns the runtime schema, canonical hash and order,
  bounded serialization, texture dependencies, collision/object payloads, and
  malformed-input validation independently of editor/UI linkage;
- `project.tilemap_library` and `project.tilemap_pipeline` atomically persist
  exact compiled revisions beneath `<project>/Library/TileMaps`, restore
  exact/latest state, and bind stable project asset identity;
- `render.canvas2d_tilemap` culls visible chunks and compiles deterministic
  animated, transformed, material-bound Canvas2D submissions plus collision
  and object outputs without exposing physical cache identity;
- project.tilemap_source owns canonical bounded Assets/Maps/*.epochmap source
  validation, exact reload, verified temporary writes, and atomic replacement;
- editor.tilemap_workspace binds that source document to tilesets, virtualized
  palettes, layers, grid editing, objects, collision, undo/redo, diagnostics,
  exact Library publication, and revision-cached Canvas2D preview compilation.
  Stable canvas object hit targets, hierarchy rows, staged name/type/transform
  properties, direct drag capture, and semantic apply/duplicate/delete now
  operate on generation-checked map-object handles without committing per-frame
  inspector or pointer motion. Layer selection now resolves a generation-checked
  handle while staged visibility, lock, collision source, phase, draw order,
  opacity, and parallax commit as one operation; create/duplicate/delete preserve
  unique names, deterministic draw order, and at least one layer. Handoff,
  source reload, exact Library restore, and runtime preview preserve accepted
  object and layer descriptors. Generation-checked palette selection now owns a
  disposable collision draft; one Apply records exact shape, bounds, and filter
  intent, and quick collision toggles preserve the rest of the descriptor;
- project.tilemap_runtime now prepares one immutable Canvas2D scene from the
  exact compiled map revision and its authenticated texture closure. Authoring
  builds regenerate Library/TileMaps from canonical source; game-only builds
  restore Library artifacts with tilemap authoring compiled out. Runtime
  verifies every declared texture dependency, then leases only the unique
  logical textures referenced by visible textured cells. Empty visible regions
  retain an empty valid resource closure instead of binding unrelated textures;
- generated project manifests now own an explicit tilemap path. Canvas2D
  ProjectPlayScene resolves the executable project root, publishes the prepared
  map once per active context, and retires that context-owned scene on exit or
  mode replacement;
- generated Game2D projects also own a canonical project input source and
  compiled artifact. `project.input_profile` validates stable actions/bindings,
  keyboard/controller schemas, fixed-point dead zones, deterministic codecs,
  source-first publication, and legacy defaults without confusing editor-camera
  shortcuts with project controls. `project.input_controller` maps one immutable,
  generation-checked process snapshot into that profile per project frame,
  preserves held/axis continuity, and consumes each physical press edge once.
  `editor.project_input_settings` now owns the Project Defaults Input Manager:
  edits are staged, physical-binding conflicts are non-destructive and visible,
  Apply is revision-guarded, and Discard, staged defaults, and disk reload are
  explicit. Settings routes directly to that surface. Generated Game shells and
  Platformer materialize the canonical profile; Tool and engine-development
  shells remain absent by default with one explicit opt-in policy;
- `project.actor2d_runtime` consumes evaluated action frames and authored map
  collision through `physics.solver2d`. It owns fixed-step actor movement,
  spawn, pause, reset, snapshots, bounded catch-up, stable contacts, and a
  renderer-neutral state that ProjectPlayScene publishes as Canvas2D content;
- live editor `ProjectPlayScene` now hosts `project.gameplay2d_runtime` instead
  of duplicating map, input, actor, animation, and audio composition. The host
  retains physical input sampling, GUI impulses, camera, per-context scene
  publication/retirement, protected GUI replay, and presentation. Gameplay
  opens from scene `load`, closes before context retirement, preserves a static
  map fallback after preparation failure, and owns idempotent repeated-session
  teardown including active-destination move assignment;
- EpochGui owns the renderer-neutral tile workspace state, hit testing, pan/zoom,
  palette virtualization, layer selection, layout, and standalone tests. The
  engine adapter owns project documents, persistence, drawing, and input;
- whole-editor context replacement serializes unsaved tile history and portable
  view state, then recreates disposable preview artifacts in the new context;
- `asset.texture_import` decodes bounded uncompressed BMP, TGA, and P6 PPM
  sources into canonical RGBA8 artifacts. `project.texture_source` atomically
  stores exact editable documents beneath `Assets/Textures`; the
  project-scoped controller owns create/open/save, semantic layer and drag-stroke
  editing, undo/redo, live preview, deterministic compile/publication, bitmap
  import, Library regeneration, scene assignment, and actionable diagnostics.
  Project Assets and the Outliner share one guarded activation path, refuse to
  replace unsaved edits, and cache real compiled thumbnails by artifact identity.
  Editable preview reads the live authoring document even without a selected
  compiled row. Spatial R8 mask layers bind to content by generation-checked
  handle with canonical strength/invert state and exact schema-3 reopen.
  The central Asset Manager separates Browser, Textures, Scripts, and Graph;
  Browser uses the reusable virtualized EpochGui asset grid, resolves source
  identity through canonical filesystem equivalence, and hydrates the bounded
  project-thumbnail atlas as soon as a matching texture catalog is presented.
  Assets derives compact aspect-preserving thumbnails and registers the project
  catalog through one bounded EpochGui atlas transaction; pixels, placement, and
  sprite handles remain disposable cache state. A build-safe EpochGui contract
  proves stable handles across unchanged and in-place replaced surface pixels;
- scene snapshot format 3 serializes semantic texture materials and exact
  logical revisions. Runtime compilation and save/reopen rebuild physical
  resources through the project pipeline instead of persisting cache handles;
- the World Outliner Assets tab exposes project source discovery, import,
  Library restore, and selected-scene material assignment through the same
  controller; generated project scenes default to no Engine Arcade package
  unless package admission is explicit;
- `editor.canvas2d_scene` now accepts typed logical texture material intent and
  an exact immutable resource lease. Solid entities retain their fallback;
  missing, mismatched, duplicate, or unexpected project texture bindings are
  rejected before scene replacement;
- project.texture_admission validates authenticated artifacts against explicit
  sampled-image representation evidence and derives effective dimension,
  sampled-image, resident-memory, and upload limits across project, platform,
  and renderer budgets. Strict policy rejects experimental providers;
- render.canvas2d_scene publishes an immutable exact resource closure. Duplicate,
  missing, stale-revision, invalid, and unexpected texture bindings fail
  atomically while old readers retain their owning resource lifetime;
- authored tilesets now carry logical texture material intent instead of a
  physical `TextureHandle`. Transient render-surface materials retain their
  explicit physical graph-output binding;
- `render.texture.artifact` remains the sole validated artifact-to-residency
  adapter and no longer treats source edit sequence as artifact revision;
- `asset.texture_artifact` now owns the always-built artifact schema, stable
  hash protocol, deterministic little-endian serializer, bounded reader, and
  integrity validator. The authoring compiler re-exports and
  consumes it, while a standalone Clang contract proves artifact consumption
  with the authoring platform and texture editor disabled;
- `package.registry` owns fail-closed extension evidence policy, not download,
  verification, or native activation;
- EpochEngineExtensions now owns the manifest-backed capability technique
  gallery: labeled context/tier scene stations describe required features,
  evidence, fallbacks, and scene intent without importing a second renderer
  spine or claiming unproved native effects;
- Engine Arcade now validates its canonical cabinet/screen scene nodes, sampled
  render-surface material binding, geometry storage, and built-in scene catalog;
- World Outliner now owns `World`, `Assets`, and `Scripting` tabs; the old
  top-level Assets route forwards into the dockable tool surface. Script source
  uses selection-aware caret, clipboard, focus, drag selection, word movement,
  and scrolling behavior instead of a whole-field edit flag;
- EpochGui now owns a reusable primal multi-line text document controller with
  line indexing, revision/dirty state, find/replace, save acknowledgement, and
  standalone tests. Embedded and standalone EpochGui source surfaces are
  synchronized;
- editor Focus updates every active renderer child context that presents the
  selected scene, and default standard/sandbox scenes no longer inject the
  unwanted `StarterCube`;
- OS AI supports direct offline `llama-cli` inference as a captured,
  timeout-bounded child process beside the existing local API lane. The
  executable-local provider now has a pinned,
  integrity-checked installation contract for llama.cpp `b10516` and the
  community Qwen3.8 27B Q4_K_M GGUF. The tracked installer writes receipts only
  after exact hash/size validation, and runtime readiness requires both receipts
  and payloads. Generated projects materialize a disabled-by-default
  `Assets/AI/project_ai.epochai` profile and preserve three explicit provider
  choices: Off, shared Epoch-local Qwen3.8, or an external model endpoint with
  Epoch MCP guards. Windows and CMake project builds carry the profile without
  copying weights. The external provider path remains available and is not replaced by
  the local install. Ordinary AI Authoring now requests strict
  `EPOCH_AUTHORING_PLAN_V1` scene/GUI plans from
  the selected model, validates the bounded allowlist, shows every call, and
  applies semantic commands only after a visible operator approval. AI requests
  use owned `jthread` lifetime plus one cancellation generation; Pause, goal
  edit/delete, chat shutdown, and application shutdown interrupt WinHTTP, curl,
  or direct CLI work before joining it. A first already-satisfied reconcile is
  zero progress and requests a distinct milestone; repeating the same semantic
  call signature pauses the goal. Opening AI no longer silently replaces the
  active project with the Engine Development sandbox. Package Manager stages
  the human-approved Extensions setup plan without starting a server or fetching
  model weights;
- `ai.development_guard` owns immutable SHA-256 proposal identity,
  generation-checked sessions, typed operation/risk/workspace/content intent,
  normalized allowlists, separate review and operator approval, bounded
  lifetimes, cancellation, verified terminal evidence, and ordered audit
  records. Its `ExecutionPermit` is a private guard-issued capability that can
  be claimed once before execution; callers cannot forge one from digest fields;
- `editor.ai_development_controller` composes the guard with trusted monotonic
  production time, serialized execution entry, executor-only source completion,
  and distinct live-snapshot and writable-iteration roots. Model proposals read
  exact preimages from the live source tree, materialize those bytes beneath a
  unique `cache/ai/iterations/session_*` root, and execute only in that sandbox.
  Contracts prove the live source remains byte-identical while the sandbox
  receives the approved postimage. The externally driven clock exists only for
  deterministic contracts and rejects backward time;
- `ai.development_proposal_codec` accepts only a strict bounded data protocol
  for source-area, path, summary, replacement bytes, and lifetime. The trusted
  host resolves the canonical workspace, captures preimages, derives SHA-256
  before/after evidence, and owns risk, identity, review, approval, permit, and
  execution. Raw model reply bytes remain separate from display-normalized chat;
- `ai.development_executor` applies bounded source-only exact-content
  transactions within the controller-selected iteration root. It verifies
  canonical paths and approved preimages/postimages, writes and flushes exclusive
  same-directory temporaries, revalidates immediately before commit, verifies
  committed bytes, and records rollback/preimage evidence. It does not own Git,
  release, updater, package, network, or shell authority;
- `ai.iteration_loop` remains the build-safe deterministic policy spine rather
  than treating model text as evidence. Engine Development now owns one
  contained production slice: workload-specific 64K context/32K output
  budgets; raw prompt preservation; an exact-copy buildable workspace with
  The strict Qwen source protocol admits one to four related exact-file
  operations per immutable proposal, so interface, implementation, build
  registration, and contract coverage can be validated as one atomic generation
  instead of being artificially split into single-file attempts.
  SHA-256 and live-source rechecks; generation-owned TaskGraph materialization;
  strict exact proposal and new-digest approval; sandbox-first transactions; a
  hidden direct MSBuild Debug and Release compiler passes; separate build-safe
  engine-contract children for both; a separately scheduled HeadlessCI Debug
  Pre-staging packet failures now receive at most two deterministic
  host-diagnosed Qwen correction attempts over the same reviewed context.
  Correction prompts stage no bytes and cannot bypass the immutable proposal or
  operator approval gates; exhaustion stops for operator refinement.
  build and asset-light run; and a separately operator-approved Release
  `--engine-validation-self-test` covering registered project profiles,
  generated-child self-tests, and the AI gate. Cancellation and stale-generation
  rejection remain mandatory, with at most three fresh-generation repairs from
  bounded host diagnostics.
  Qwen3.8-class coding models pass the related-files writer gate. Every changed
  repair waits for exact operator approval. Only after all seven Debug, Release,
  HeadlessCI, and full-validation evidence completions succeed for the current
  generation may the operator invoke a separate `Stage Live Promotion` action.
  That action rechecks live preimages and compiler-tested sandbox postimages,
  reparses the retained strict proposal through a fresh live-root controller,
  requires exact operation equality, and displays a new digest without writing
  source. `Approve Live Promotion` performs those
  checks again, creates a fresh single-use permit, and atomically applies only
  that exact reviewed source. Stale live files, sandbox tampering, changed
  operations, unsafe paths, failed commits, cancellation, and replay fail
  closed. Success consumes the candidate and grants no automatic next request,
  shell, Git, release, updater, package, network, or approval authority.
  Static analysis, sanitizer, architecture, visual, and frontier host adapters
  remain visibly blocked;
- `ai.mcp` now owns the strict `EPOCH_TOOL_PLAN_V1` active-project protocol.
  A selected local model may name exactly one argument-free
  `project.inspect`, `project.save`, `project.build`, `project.run`,
  `project.test`, or `diagnostics.read` call; it cannot select a path,
  native command, permission, or second call. AI Chat `/tool` stages the exact
  parsed packet beside an Apply/Discard decision, and the trusted editor
  revalidates that immutable call through the existing registry before using
  canonical project owners. Run has its own visible approval. Test may perform
  a canonical prerequisite build and then a cancellable hidden
  `--project-self-test` only from accepted active-project artifact evidence;
  completion from a replaced project is discarded. The model never receives
  permit issuance, unrestricted shell/native execution, Git, network, release,
  updater, or approval authority. Debug and Release editor builds and aggregate
  build-safe contracts plus the HeadlessCI Debug build and no-graphics run pass.
  The full-validation adapter and its visible approval/state contracts are
  build-proven, but the operator-approved full-validation child was not launched
  for this source proof. No live model/GUI approval flow, approved Run, or
  approved generated-child Test was launched;
- `systems.registry` publishes bounded lifecycle, dependency, execution-order,
  frame, and per-system timing snapshots. Sampling remains disabled unless the
  Systems workspace is active;
- `editor.task_scheduler` now routes AI evidence builds, selected script
  builds, project builds, and the approved tool harness through one bounded
  `taskgraph.dotsystem`. The Systems panel exposes that graph as read-only Live
  Scheduler evidence with revision-cached rows, accepted/queued/started/finished
  timestamps, queue/run totals and peaks, and bounded timing history. The Time
  view labels registry-update span truthfully and fixed-size chart surfaces
  replace atlas pixels in place instead of growing residency;
- `authoring.task_graph` remains a separately labeled editable Learning Graph
  with stable handles, semantic operations, undo/redo, validation, topology,
  critical-path analysis, and parallel-wave simulation. It cannot execute or
  mutate the live scheduler;
- the reusable node-graph canvas now owns persistent pan/zoom/fit, node movement,
  selection, connection, and disconnection intent. GUI Editor's Structure Graph
  maps that intent into validated `authoring.gui_document` reparent operations;
  invalid containers, cycles, tab-page parentage, and stale identities are
  rejected by the canonical document;
- selected `.ascript.cpp` files compile as C++23 shared libraries through an
  argument-vector child process. The compiler revalidates source and prior output,
  verifies a bounded candidate, publishes it by same-filesystem atomic
  replacement, and verifies the published artifact before success;
- `project.lifecycle` defines a strict generation-stamped decision path for the
  selected project, committed scene, materialized shell, build inputs, active
  build, verified artifact, and runtime. Its contracts reject stale,
  cross-project, and mismatched evidence. The production editor now owns that
  ledger for selection, verified scene save, shell materialization, build-input
  fingerprinting, asynchronous completion, and external Run; Project displays
  the active action, gate reason, and project/scene/input/build/artifact
  generations. Script Build remains distinct from Project Build and external Run;
- `platform.child_process` owns a fixed-capacity, generation-checked native
  process supervisor. It validates executable and working-directory ownership,
  launches exact argument vectors without a Linux shell, prevents duplicate or
  cross-project runtime overlap, polls exit state, and owns Focus, graceful Stop,
  forced Stop, elapsed-time, PID, and terminal evidence. Project Run binds the
  selected project plus accepted artifact generation and SHA-256 to that
  supervisor, and the Project workspace exposes the observed process rather than
  assuming a dispatched command succeeded;
- EpochGui dependency work adds portable DPI-aware font, image, input,
  rounded-rectangle, toggle, text, layout, docking, popup, panel, and
  floating-window primitives. Font measurement requires explicit logical-pixel
  height and DPI; rounded controls are the default Settings policy. Static
  rounded palette sprites use edge-clamped atlas gutters, active tabs use their
  palette fill, and close hover changes color without an active underline;
- Engine Arcade now uses one validated cabinet mesh with screen/control details
  and camera-facing solid culling instead of overlapping preview boxes;
- one shared Arcade attract-pattern contract now drives backend-owned sampled
  scene surfaces in OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX, and Software;
  build-safe contracts prove routing and ownership while visual presentation
  remains `Partial` pending the corrected operator eye test. Every adapter uses
  the shared positive-Z cabinet plane and shared front-view predicate, so the
  sampled display is not visible through the rear face;
- `editor.application` is the shared application registry. Standard Editor,
  Plant Lab, and GUI Editor own separate C++23 implementation units, canonical
  scene seeds, surface masks, camera/dock defaults, pane policy, and
  run/authoring permissions while reusing one editor shell and service spine;
- Plant Lab is the separate launcher editor for authoring custom tree assets and
  reusable forest configurations. `ForestAssetDocument` owns stable genome
  identity, bounded semantic profile edits, undo/redo, deterministic revision and
  content hash, an integrity-checked project source, and a content-addressed
  immutable Library artifact. Its compiler derives one validated morphology
  graph/time sample, renderer-neutral preview, voxel LOD plan, and bounded voxel
  occupancy; Plant Lab reopens that project pair for `PlantLabPreview`, while
  Forest Factory places the same revision. The common preview projection now
  carries editor/runtime Euler transforms and represents trunks and branches as
  oriented midpoint segments plus deterministically oriented foliage in every
  active renderer context. Compiled output import, sparse voxel materialization,
  interactive node editing, mesh/impostor output, and accepted per-context pixels
  remain unfinished.
  The two products do not merge surfaces, documents, or ownership;
- `temporal.request` owns explicit global/sample time mapping, rates, anchors,
  forward/reverse/frozen direction, bounded exact/nearest/bracket observation,
  truth/reconstruction evidence, retained-history metrics, and
  generation-checked subject retirement; `ecs.entityhistory` is its typed ECS
  facade rather than a dead private history implementation;
- `render.canvas2d` owns validated project settings, camera/viewport scaling,
  generation-checked sprites, logical texture/material declarations,
  deterministic quad batching, tile descriptors, immutable frame submissions,
  final-compose plans, diagnostics, and build-safe contracts; editor snapshots
  and the Canvas2D Project surface preserve and expose its core policy;
- `render.texture.residency` owns bounded, generation-checked physical texture
  records keyed by stable compiled artifact identity, with deterministic reuse,
  priority/LRU eviction, pinning, upload/entry/byte budgets, stale-handle
  rejection, forward-only backend epochs, plan-then-commit miss eviction,
  transactional recreation, explicit transient-replacement limits, upload
  accounting, metrics, and staged fake-device proof. Failed allocation, upload,
  or readiness cannot retire an existing entry;
- `render.device` validates explicit texture upload regions/row pitches, while
  the OpenGL-family device and `opengl.textures` provide context-guarded native
  allocation, base-mip upload, readiness, and destruction hooks without moving
  backend handles into authoring or Canvas2D state;
- `render.canvas2d_presentation` validates complete frame/raster identity,
  derives artifact digests from actual pixel bytes, acquires disposable output
  through the residency cache, and emits an explicit surface/image/native
  packet; `opengl.canvas2d` implements the active-editor final compositor with
  viewport-confined clears, top-left coordinate conversion, context-owned
  texture validation, and scoped GL state restoration. An immutable per-context
  scene exchange now maps committed editor entities to semantic solid sprites
  and invokes that compositor in the protected live scene slot;
- `render.canvas2d_evidence` compares canonical T0-CPU presentation images to
  normalized native readback with explicit origin, stride, tolerance, hash,
  budget, and mismatch metrics. The OpenGL scene adapter owns a capture-lane
  one-shot viewport readback after warmup, restores all touched read/pixel-pack
  state, publishes a queryable snapshot, and never runs that readback during
  normal frames;
- `render.canvas2d_runtime` owns one generation/content/frame/output-aware scene
  raster session shared by native adapters. OpenGL, SDL3, SFML3, Raylib3,
  Vulkan, DirectX/D3D11, and Software consume the same immutable scene and
  final-compose contract while retaining backend-owned upload, sampling,
  blending, viewport, and teardown state. Vulkan reuses same-size residency and
  commits mapped buffers transactionally; all seven adapters are MSVC
  Debug/Release build-proven, while non-OpenGL live presentation remains `Partial`;
- `render.canvas2d_limits` now maps each renderer capability profile into one
  fail-closed Canvas2D compile, CPU-raster, native-upload, and residency budget.
  All seven baseline paths consume those derived logical limits; the
  OpenGL/SDL3/SFML3/Raylib3/D3D11 presenters receive a profile-sized two-slot
  transactional output cache, Vulkan replaces its hard-coded 128 MiB upload
  allowance with the mapped canvas/upload ceiling, and Software consumes the
  compile/raster limits without claiming GPU residency. Runtime cache identity
  now includes limits and raster policy, so a stricter profile cannot reuse an
  over-budget prior image. MSVC Debug/Release contracts plus the full managed
  Clang 22 build and all 32 CTest contracts pass; native pixel, memory, resize,
  and switch-soak evidence remains `Partial`;
- the shared Canvas2D runtime contract now proves that an output resize compiles
  one fresh compose while preserving the logical canvas hash, then reuses the
  stable resized result. The presentation contract rejects out-of-bounds host
  surfaces before allocation, preserves viewport/filter/origin/alpha identity,
  rolls back a newly uploaded revision after failed native dispatch, and owns
  one current output lease. Sixty-four same-epoch content revisions remain at
  two transactional slots and one live texture; 64 forward-only backend-hook
  replacements finish with idempotent, exactly balanced fake-device retirement;
- `project.lifecycle` centralizes Save, materialize, Build, Run, wait, and
  focus-existing-runtime decisions with generation-safe attempt tracking;
- all first-party C++ files follow the canonical one-dot owner grammar,
  exact module/file identity, and owned directory layout enforced by the source
  naming validator;
- the launcher opens the three editor applications, selects a live context
  before launch, and keeps update/exit actions direct;
- the desktop host assigns exactly one logical active-editor authority,
  independent of renderer-context creation order, parent-grid side, or dock
  state. Every live physical renderer context may undock or redock, and routed
  pane windows retain their same logical identity. Windows uses one parent
  container; Linux keeps its current standalone X11 shell while applying the
  same tier/work-area client policy to parent-requested editor use.

These facts are contracts, not blanket runtime claims. Current checkpoint proof
includes MSVC Debug/Release editor builds and contracts, the managed Clang 22
full-engine Release build against GCC 12/libstdc++12 on the Ubuntu 22.04/GLIBC
2.35 baseline, the current Clang 22 Debug full-engine build and all 32
no-display Linux CTests, the portable
`core.format` contract, and 6/6 standalone EpochGui feature tests. Operator
evidence proves correct filled scene orientation
in SDL3, SFML3, DirectX, and Software. Current Raylib/Vulkan and repeated
replacement regressions remain `Partial` pending fresh proof. The seven Arcade sampled scene-surface
implementations compile and pass build-safe contracts but remain `Partial` until
the current source candidate receives visual and switch-cycle proof. Canvas2D
has deterministic `T0-CPU` reference raster and image-hash proof on MSVC and
Clang. Renderer-neutral residency, cache recreation, OpenGL-family hook routing,
presentation packet staging, the compiled primary OpenGL compositor, and
real-hook no-context refusal are build-proven on MSVC and managed Clang 22.
Current MSVC contracts also prove origin/stride-aware pixel comparison, exact
and tolerant mismatch accounting, hash validation, bounded failure, and safe
OpenGL evidence refusal without a registered native context. Approved parented
captures now prove the native OpenGL Canvas2D viewport exactly matches the
canonical T0-CPU image across 1,178,872 pixels with zero outliers and zero
channel error. The core-profile state guard restores polygon mode through
`GL_FRONT_AND_BACK`; live platform/render-thread context ownership prevents
sprite batches from selecting an unrelated logical context.
MSVC contracts also prove deterministic temporal texture payload compilation,
artifact-integrity rejection, project-scoped logical identity, exact sampled
image admission, foreign-registry rejection, collision-safe resource keys,
atomic immutable scene closure, cache reuse, backend recreation/reset,
stale-handle rejection, synchronous upload copy, verified Project Library
round trips, restore-on-demand, portable path collision refusal, and textured
editor scene raster publication. A clean authoring-disabled
managed Clang configuration independently proves the runtime artifact schema,
hashing, and validator without authoring document/UI linkage. Immutable scene
publication, replacement lifetime, semantic entity mapping, CPU shading, and
protected OpenGL scene-slot routing are build-proven. All registered generated
MSVC profiles now pass materialize, production save/reopen, build, and child-runtime
self-test; GUI Editor also passes the exact standalone external Run arguments.
The bounded Assets interaction command and all seven Canvas2D adapters compile
in Debug and Release. Its live run and non-OpenGL pixel/switch evidence remain `Partial`.

## Completed Source Checkpoint

Source and packaged runtime `v0.89.28` use a release-only public updater
boundary. The corrected Windows/Linux packages and the persisted-key signature
are live and independently verified. Historical `v0.89.27` and `v0.89.06`
bytes plus the
exact `multicontext-base-stable` branch remain available without substituting
for the new release. The accepted v0.89.23 collision slice
remains intact. The current candidate adds manifest-selected project sessions,
fail-closed overwrite protection, launcher-owned Plant Lab routing, canonical
scene/GUI selection and history commands, working GUI Canvas persistence,
shared timeline controls, live detached-pane projection, visible pane recovery
tabs, and one process-owned physical controller snapshot boundary. The boundary
now feeds compiled project actions through a monotonic per-scene adapter, and
Project Controls owns persisted controller button, axis, slot, and dead-zone
edits beside keyboard rebinding. Project Audio mutations now decode before
saving source, atomically publish the immutable
`Library/Audio/project_audio.epochaudioc` PCM artifact, and expose whether
that artifact is current. Runtime preparation recompiles valid authoring source
or restores the verified artifact when source/WAV files are intentionally
absent.

File owns Save Project while Tools remains camera-only. Generated manifests
declare `project_format: epoch-project-v1` and the unique
`build_profile: epoch-runtime-static`. Open may inspect a known legacy shell
without rewriting it; Save Project, Build, and Run atomically migrate missing
canonical fields. Malformed, duplicated, future, or conflicting metadata fails
before project admission or child compilation. The versioned CMake/Linux build
path links the child to the canonical reusable `EpochRuntime` static target and
excludes tests plus native extensions without changing the default editor build.

Native Visual Studio Debug and Release editor builds pass. The aggregate
build-safe engine contract passes in both configurations, the Windows CMake
preset configures with the native file-dialog dependency, and the isolated
`epoch_project_input_controller_contract` and Project Audio source/artifact
contracts build and pass, including corruption refusal and artifact-only
restore. The updater-equivalent managed-vcpkg Clang 22.1.8 Release lane builds
the complete engine and passes all 33 no-display CTests. The exact standalone
EpochGui subtree also builds in Clang 22.1.8 Release and passes all 10 of its
contracts. An isolated managed
Clang 22.1.8 Debug build also produces `libEpochRuntime.a` with static-runtime
mode on and native extensions off; the normal executable target rebuilds with
static-runtime mode off. The fresh generated
`twodstudio` acceptance child passes materialize, production save/reopen,
static-runtime profile validation, Build, and child validation with artifact
acceptance mask 63/63 for scene,
tilemap, input, sprite animation, audio, and GUI. Its composed gameplay runtime
records 187 input frames, 187 fixed steps, 188 animation samples, three audio
triggers, deterministic Canvas2D hash `14053744896759252690`, and accepted
close/teardown. Live editor `ProjectPlayScene` now uses that same composition;
MSVC Debug/Release and the managed Clang 22.1.8 Release lane compile the host
adapter, and aggregate contracts cover deterministic replay, a 64-session
open/advance/idempotent-close soak with balanced gameplay/audio ownership, and
active-session move-assignment teardown. The source-name validator passes all 483 first-party
files. No GUI process was launched for the
v0.89.27 interaction changes, so native file-dialog interaction, pane
drag/recovery, timeline feel, Project Audio controls, physical controller/audio,
and layout remain unclaimed. Release packaging and Site publication must retain
these source/build distinctions and expose the exact verified checksums.

Project GUI publication is exact and independently atomic beside scene source;
it is not yet a cross-file journal. Runtime compilation consumes the validated
GUI artifact published from canonical source rather than authoring history,
editor scene vectors, thumbnail sprites, or atlas/cache identity. Artifact
regeneration and source/scene publication are still separate atomic operations.

Temporal texture schema 3, sparse R8 masks, live editable preview restoration,
and the batched project-thumbnail atlas pass compile in MSVC Debug/Release and
managed Clang. The isolated texture contract and aggregate project-controller
contract prove schema-1/2 migration, mask binding/strength/invert, conceal/reveal,
exact undo/redo/reopen artifacts, thumbnail downsampling, and atomic
malformed/integrity rejection. Native visual
appearance remains part of the v0.89.27 eye test.

The Release AI lifetime repair, semantic no-op goal policy, and temporal forest
asset compiler build in native MSVC Debug and Release. The aggregate build-safe
engine contract passes in both configurations, including the default Plant Lab
semantic journal, branch-start edit/undo/redo, deterministic recompile, temporal
sample, voxel LOD identity, occupancy, and shared forest preview routing. Plant
Lab now owns one live temporal document; its controls mutate semantic history,
its preview projects the compiled revision, Forest Factory places that same
compiled revision into the active scene, and package staging records matching
revision/hash/LOD evidence. No GUI process was launched in this pass; Release
goal cancel/shutdown responsiveness, broad-goal progression, Plant Lab
interaction, native forest pixels, and restart-safe forest artifact reopen
remain operator/runtime gates.

The multicontext startup repair keeps native child windows hidden and
noninteractive until their backend reports ready and completes one successful
frame. SDL3 and SFML3 hosted children no longer force-show while initializing.
Ordinary full editor contexts capture or restore one renderer-neutral camera
snapshot exactly once on their first editor frame, including panes launched
through Menu; routed GUI popouts and explicit context-switch snapshots remain
independent. OpenGL, SDL3, SFML3, Raylib3, Vulkan, DirectX/D3D11, and Software
now consume one context-keyed, camera-target-snapped preview grid with bounded
fixed line count and adaptive power-of-two spacing. The shared geometry-routing
policy keeps authored scene geometry filled, removes its permanent diagnostic
wire pass, retains selection outlines only while selected, and represents
camera/light editor-only objects with purpose-specific gizmos. Raylib always
executes that canonical grid, solid, and marker pass before optional native
models, while SDL3 submits indexed canonical solid triangles. DirectX divides
its preview into background-grid, solid, sampled-surface, and foreground-helper
passes so the grid cannot overwrite scene surfaces. The Windows parent host
snapshots its context registry under lock,
then performs native placement and resize callbacks after releasing the lock so
reentrant resize handling cannot deadlock the UI thread. EpochGui text-control
tests prove that inserted spaces survive and Backspace removes one character
without reinserting native control bytes. Native MSVC Debug and Release editor
builds and both aggregate build-safe contracts pass, including preview-grid
bounds/index validity, helper-fill policy, and geometry-routing policy. No GUI process was launched for
this repair, so six-pane startup, native grid and solid pixels, movement, and
teardown remain operator eye-test gates.

The Plant Lab persistence slice is now source- and build-proven. One project-
scoped `forest_asset` identity owns a bounded, integrity-checked
`Assets/Forest/*.epoch_forest` source and content-addressed immutable
`Library/Forest` artifact. Publication verifies the artifact before atomically
replacing the canonical source; exact/latest reopen verifies the embedded
source and compiler-derived preview/voxel descriptor. The contract proves
restart reopen, old-revision lookup, corruption refusal, and metrics. Project
switches clear Plant Lab state, first access reopens a matching project pair,
and explicit Publish/Reopen plus package activation use this same spine. The
renderer-neutral marker contract now carries scene rotation; compiled branches
use real midpoint/length transforms instead of endpoint cubes, and common
oriented solid/selection geometry preserves vegetation color while exposing a
selected outline. Debug and Release editor builds and aggregate build-safe
contracts pass, including one rotated branch's exact solid/wire counts and
non-axis-aligned extents. GUI and native pixel eye testing remain separate.

## Remaining Implementation Order

1. Eye-test keyboard/controller Project Controls, duplicate refusal, dead-zone
   edits, Restore Default, and physical-device behavior while preserving
   accepted collision, GUI, Assets, Scripts, Systems, camera, and workspace
   behavior.
2. Eye-test the composed live editor `ProjectPlayScene`, then prove repeated
   Play/Stop, interactive external Run, disposable-cache
   regeneration, native presentation, and resource teardown. Build and the
   fresh generated-child build-safe acceptance path are already proven.
3. Eye-test Project Audio import/edit/persistence, then prove looping ambient
   playback, jump/land cues, physical-device output, and repeated Play/Stop over
   the existing process-owned audio boundary.
4. The shared trunk/branch/leaf projection is source- and build-proven through
   the one common geometry stream consumed by every active renderer context.
   Gather approved native pixel, repeated-switch, resize, and teardown evidence
   without changing protected frame, queue-drain, GUI replay, subpass, or present
   order; keep the backend-specific evidence `Partial` until those runs complete.
## Backend Repair Within This Gate

The source gate keeps previous operator evidence separate from the current
Canvas2D adapter batch. Debug/Release builds plus the renderer-neutral lifecycle
soak prove shared resize/replacement/retirement policy, not native pixels,
orientation, minimized/restore behavior, stable native memory, or live repeated
context switching.

- shared preview geometry defines the clockwise-outward object convention;
- SDL3, SFML3, DirectX, and Software are accepted orientation references for
  their current projected/native paths;
- native child windows remain hidden and reject GUI input until backend-ready
  state and one successful frame prove that the pane can be exposed;
- ordinary editor contexts normalize from one initial renderer-neutral camera
  snapshot exactly once; later camera movement remains context-local;
- all Canvas2D adapters consume one immutable top-left/premultiplied compose
  contract and retain backend-owned native resources;
- Vulkan retains same-size image/descriptors, retires inactive state, and leaves
  failed mapped allocations uncommitted;
- Raylib defers texture destruction until its owning native context is current
  and always draws the canonical solid scene before optional native models;
- SDL3 submits explicit indexed triangles for canonical solid preview faces;
- D3D11 honors tight-pitch uploads and RGBA letterbox clears;
- queue drain, GUI replay, subpass, depth, and present order remain unchanged;
- current Raylib/Vulkan and non-OpenGL presentation remain `Partial` until live proof;
- this parity work must not delay the `T1-GL` 2D product unless shared contracts
  regress.

## Settings And Control Rule

Every maturing system must update its settings, controls, diagnostics, and
persistence scope with the implementation:

- unavailable options are absent or disabled with a reason;
- partial capabilities are labeled experimental;
- defaults come from project profile, measured limits, and budgets;
- advanced controls use progressive disclosure;
- EpochGui owns portable control state and layout;
- engine adapters own backend input/drawing, project state, native hosts, and
  evidence;
- mobile/game/headless profiles can omit floating and docking hosts.

## Selective Reference Intake

`Autodidac/tiered_gfx_OpenGL_modular_context_demo` is a technique lab. Only
license-audited, surgically translated ideas may enter Epoch:

- capability/quality controls;
- material and view descriptors;
- RTT/final composition;
- OpenGL techniques behind Epoch render-device/graph contracts;
- manifest-backed CC0 diagnostic assets.

Do not import its alternate resource spine, platform scaffolding, vendored GUI,
hard-coded scenes, raw GL ownership outside the OpenGL adapter, or capability
claims. Advanced effects follow the playable 2D loop.

## Forbidden Expansion

Do not use this gate to:

- mutate historical `v0.89.06` assets or move
  `multicontext-base-stable` away from
  `ad6c416d930b348a61bc37ceb7d4522742be084a`; after publication, any change to
  verified release source or artifact bytes requires a fresh bounded release;
- claim native PBR, shadows, water, collision solving, physical audio, hardware
  ray query, or RT pipelines without implementation and proof;
- start multiplayer, persistent unscripted AI, planetary terrain, or a broad 3D
  authoring campaign;
- add a second capability registry, renderer resource spine, texture identity,
  node framework, or GUI library;
- make atlases, descriptors, GPU buffers, pipelines, previews, or caches
  canonical authoring state;
- copy unreviewed code/assets from the OpenGL demo or `addons/`.

## Acceptance

The v0.89.32 source gate records these completed source contracts:

- immutable proposal digest, separate review/operator approval, private permit
  issuance, one execution claim, expiry/reuse/cancellation refusal, trusted
  monotonic production time, exact-content source execution, and verified
  executor-only source evidence, plus strict data-only multi-file model source
  proposals whose workspace roots, preimages, hashes, risk, and authority are
  assigned by the trusted host;
- diagnostics-off fast paths, bounded/revision-cached snapshots, the read-only
  Live Scheduler over real shared editor work, accepted/queued/started/finished
  timing, queue/run totals and peaks, a bounded Time view, in-place chart atlas
  updates, and a hard authority boundary around the editable Learning Graph;
- real C++23 script compilation with source/output revalidation, candidate
  verification, atomic publication, published-artifact verification, and
  single-flight editor scheduling;
- generation-stamped project, scene, shell, build-input, build-attempt, artifact,
  and runtime evidence with stale/cross-project refusal;
- bounded native process ownership, exact duplicate-run focus, exclusive project
  runtime groups, observed exit/elapsed/PID evidence, graceful/forced stop, and
  Windows/Linux standalone contracts without shell-based Linux launch.

The source gate does not claim GUI eye proof, Release responsiveness, model-driven
multi-step dispatch, or complete OS filesystem transaction semantics.

1. CMake and MSVC metadata contain each new module/source exactly once.
2. Debug and Release `EpochEditor` build.
3. Debug and Release `--engine-contract-self-test` pass.
4. Capability selection proves CPU-only, GLES baseline, OpenGL compute,
   equivalent explicit tiers, deterministic fallback, project matching, and
   no-overclaim behavior.
5. Texture tests prove deterministic revisions, sparse boundedness, semantic
   undo/redo, reproducible compilation, and physical-plan independence.
6. Tier-0 scene tests prove selection, Focus, ground, light, spawn, save/reopen,
   and Run/Build use the same project-owned state.
7. Canvas2D, tilemap, input, solver, and actor contracts prove deterministic
   replay, source/artifact persistence, stable contacts, bounded stepping,
   ordering, culling, animation selection, transforms, collision/object output,
   blend/sampling,
   resource-binding validation, exact Project Library persistence/reopen,
   textured editor publication, and bounded malformed-input failure.
8. Renderer docs keep current Raylib/Vulkan regression status, non-OpenGL
   Canvas2D presentation, and repeated replacement `Partial` until live capture
   and eye proof.
9. Engine Arcade geometry is nondegenerate, camera-facing culling removes rear
   solids, and the sampled screen remains bound to the cabinet scene contract.
10. Launcher actions open the standard editor, Plant Lab, and GUI Editor
    with prelaunch context policy and without duplicate editor shells.
11. The parent host maintains one logical active-editor authority while every
    physical renderer context and compatible routed pane retains popout and
    explicit left/right context redock. One EpochGui guide set exposes four
    logical tab stacks plus upper-left and upper-right physical-context targets.
    Routed panes render that set locally; detached full contexts project the
    target-centered guides and hovered destination ghost into the parent host;
    AI Chat defaults to Bottom Right but follows the same placement rules as
    every other tool. Each editor application restores bounded atomic user
    layout/theme preferences without changing project or release data.
12. Each editor application validates one canonical scene/camera, rejects
    cross-application surfaces, and enforces pane/run/entity policy through the
    shared shell. The standard editor retains Forest Factory import/placement;
    Plant Lab owns the dedicated custom-tree and forest-configuration authoring scene; Forest Factory owns standard-editor placement.
13. Temporal request tests prove forward/reverse/frozen mapping, exact,
    nearest, bracket, boundary clamp, bounded retention, reconstruction flags,
    cumulative metrics, and stale-handle rejection.
14. Texture residency tests prove reuse, bounded eviction, pinning, stale-handle
    rejection, upload refusal, backend reset, and deterministic recreation;
    OpenGL hooks fail safely without an active native context.
15. No updater, release, generated cache, or unrelated operator file is staged.
16. GUI document tests prove exact codec round trip, hierarchy restoration,
    integrity rejection, bounded failure, and stable revision reconstruction.
17. Project GUI source saves atomically beneath `Assets/Gui`, rereads exact
    evidence, survives project reload, and remains separate from disposable
    scene projection, thumbnails, and renderer caches.
18. Learning Graph interaction reaches semantic drag/connect/disconnect,
    add/remove/reset, selection, undo/redo, and cycle diagnostics without gaining
    live scheduler authority.
19. Project Assets restores source/Library texture catalogs after restart and
    exposes real artifact-keyed thumbnails plus semantic material controls.
20. The editor preview owns one stable renderer-neutral view descriptor across
    perspective, free orthographic, and six locked axis views. LMB selection,
    Alt+LMB orbit, MMB pan, Alt+RMB dolly, RMB fly with WASD/QE, speed
    modifiers, Focus, and Reset are build-proven; pane splitters capture input
    before camera navigation.
21. Standard Editor, Plant Lab, and GUI Editor validate separate tabbed
    workspace layouts over one centered monitor-aware host. Standard Editor
    exposes one canonical project-GUI placement canvas; projectless GUI Editor
    owns Canvas, Runtime Preview, Component Graph, Styles, verified `.epochgui`
    Open/Save, and reusable template authoring over the same document.
22. Systems uses one document tab row, graph-first scheduler/learning/time
    views, compact evidence, and an explicit zero-work state instead of an
    empty black graph.
23. Portal/RTT source owns stable portal/view identity, recursive deterministic
    planning, cycle/depth/pixel-budget refusal, clip-plane view descriptors,
    logical output identity, disposable physical cache state, and render-pass
    bindings. Native stencil/oblique clipping and live portal pixels remain
    unclaimed.
24. A local descriptor-only extension catalog records Arcade, Forest Factory,
    Plant Lab, terrain, voxel, ocean, portal demonstrations, and local AI
    provenance/integrity/activation requirements without claiming that missing
    external payloads were installed or reconstructed.
25. Project Audio source edits compile before save and publish one immutable,
    digest-verified decoded PCM artifact under `Library/Audio`. Game-only
    preparation restores that artifact without authoring source, while malformed
    or stale present source fails closed and never silently selects old audio.
26. `project.gameplay2d_runtime` composes authenticated map/texture closure,
    input, fixed-step actor physics, animation, audio events, GUI, and Canvas2D
    output. A request-driven cost snapshot joins the immutable Canvas2D plan,
    logical texture bytes, actor/physics activity, and process-audio residency
    without rasterizing. A fresh generated `twodstudio` child accepts all 63
    artifact bits, deterministic frame/step/sample/event counts, a stable
    Canvas2D hash, 1 KiB of logical texture data, 65 sprites in one batch, one
    collision surface with one peak contact, 224640 resident audio bytes, zero
    Canvas2D rejections, zero `T1-GLES/mobile_30` budget violations, and
    close/teardown on Windows. Live `ProjectPlayScene`
    hosts that composition
    without changing context publication or GUI/present order; Debug/Release
    MSVC and managed Clang Release build it, and contracts prove deterministic
    replay, 64 balanced open/advance/idempotent-close cycles, and active-session
    move-assignment teardown. Linux proof does not claim native pixels.

## Next Gate

Eye-test v0.89.33 monitor-aware launcher/editor geometry, project
open/switch/close, launcher-owned Plant Lab, dedicated GUI template Open/Save,
embedded GUI placement/Delete/Undo/Redo, scene deselection/history, shared timeline controls,
pane tab/window routing and recovery tabs, Project Controls interaction,
live controller-provider evidence, and gameplay input from a physical device.
Build, fresh generated-child gameplay acceptance, and live-editor composition
are build-safe proven. Next prove repeated Play/Stop, interactive external Run,
cache regeneration, native
presentation, and Project Audio cue/music playback over one authored map. GUI
runtime artifact work must reconcile the existing compiler/runtime contracts
with the editor adapter before any completion claim.
Bounded scene/GUI creation is the first host-authorized non-source AI lane; project save/build/run/test and broader document tools remain follow-up behind their existing human-owned authority. Preserve the
verified published `v0.89.33` runtime release, historical `v0.89.30`,
`v0.89.29`, `v0.89.28`, `v0.89.27`, and `v0.89.06` assets, the exact
`multicontext-base-stable` ref, and the Site-hosted updater contract.
The canonical schedule is
`Changes/roadmap.md`; durable follow-up is `Changes/mission_cache.md`.
