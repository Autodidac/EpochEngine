# Runtime And Editor Workflows

This guide documents the current intended workflow for running Epoch honestly:
editor, project, scripts, systems, AI, and runtime should all travel through
the same engine-owned path.

## Startup model

- enter through the normal engine bootstrap so scripting, AI, backend setup,
  logging, capture, and project/runtime selection share one path
- desktop editor wiring lives under `Engine/examples/EpochEditor/`
- multicontext behavior depends on the active runtime/config macros documented
  in `../build/build_configuration_flags.md`
- the Windows parented multicontext host should fit the active desktop work area
  by default so the full context matrix remains visible on baseline hardware
- the longer-term shell default should converge toward explicit, independently
  owned context surfaces rather than a mixed hidden/proxy shell: editor favors a
  stable OpenGL scene path today, Windows now has a first DirectX/D3D11 native
  renderer path, D3D12 remains future work, software remains safe-launch/debug
  GUI fallback, backend switching is explicit, and inactive backends must be torn
  down instead of running hidden behind the active shell
- packaged Linux releases should follow that same main-runtime rule: the normal
  packaged `epoch` entry is the product path, while updater-shell mode remains
  an explicit bootstrap build instead of the default Linux release identity
- Smart Update is binary-first. It discovers the newest matching versioned
  runtime archive, verifies the signed release manifest and complete payload,
  and stages replacement before considering source. Authorized encrypted source
  remains both an explicit operator choice and the fallback when a compatible
  package is absent or fails verification/staging; it never outranks a valid
  compatible binary.
- the active packaged asset contract is versioned runtime archives such as
  `epoch_win10_x64_vX.Y.Z.zip` and `epoch_linux_x64_vX.Y.Z.tar.gz`
- the updater extracts the packaged version directly from the archive name
- Epoch-owned update discovery and payloads are hosted at
  `https://epoch.adamrushford.chatgpt.site`: packaged releases come from
  `/api/epoch/releases`, build admission from `/api/epoch/actions/runs`, and
  runtime bytes come from immutable release paths. Anonymous EpochEngine Git and
  source-archive routes are disabled. The public source-version response is only
  a capped legacy-client lane-selection sentinel; current clients obtain private
  source metadata exclusively through authorization. The updater has no automatic
  GitHub fallback. Standalone EpochGui remains public at
  `/git/EpochGui.git`; bundled `Engine/dep/EpochGui` must remain identical.
  GitHub URLs remain only where an independently owned managed tool, such as
  vcpkg, Git for Windows, LLVM, or 7-Zip, publishes its own payload.
- packaged release discovery consumes the Site's canonical integrity document,
  requires its pinned-key Ed25519 signature, exact schema/key identity, and
  same-Site release URL boundary, then hashes the complete payload against the
  SHA-256 authenticated inside that signed document before extraction or
  replacement. Companion `.sha256` objects remain public for manual tooling;
  Epoch fails closed when signature or digest evidence is absent or invalid
- WSL is treated as Linux for runtime package selection and should consume the
  same `epoch_linux_x64_vX.Y.Z.tar.gz` asset unless a future package layout
  proves a separate WSL asset is necessary
- source checkout installs use the same packaged-runtime-first update rule.
  Restricted source uses explicit browser approval for first device enrollment.
  Windows persists a signing-only ECDSA P-256 key in CNG, prefers the platform
  provider, falls back to the software KSP, and disables private-key export.
  Later source actions request a short-lived challenge bound to the exact compact
  request bytes and sign its domain-separated SHA-256 payload. Each archive still
  uses a fresh ephemeral P-256 ECDH/HKDF key, a five-minute in-memory bearer, a
  signed private manifest, and AES-256-GCM ciphertext. Linux keeps the one-time
  browser flow until an admitted TPM or Secret Service provider exists; it must
  not claim non-exportability from protected-at-rest storage alone. Static source
  credentials, bearer tokens, and plaintext archive keys never ship in the
  executable or browser bundle.
- Site v28 is live from exact Site commit
  `1b7e0b2236c2db6cf43228a5425e9431ba11d98e`, deployment
  `appgdep_6a901ac57fc88191bbc974db18ccbdbc`, environment revision 7. Its
  owner-only `/admin` CMS uses direct Sign in with ChatGPT and never accepts a
  native device code. Extended starts advertise
  `epoch-source-device-enroll/v1` only after all signing-JWK, client-nonce,
  device-label, and key-protection fields validate; legacy starts stay on the
  one-time flow without that capability. Both verification fields remain the
  same generic no-query `/private/epoch-engine/device` page, and the owner
  enters the displayed code through authenticated same-origin lookup. Enrolled
  devices later use short-lived persistent-key challenge/token exchanges with
  one-use replay refusal and owner CMS revocation. Successful runtime and
  encrypted-source downloads update only anonymous aggregate artifact, asset,
  byte-count, and last-time metrics visible to the owner. Active artifact
  `epoch-engine-v0.89.28-8d9f9541b5f4` is a committed-tree-only Git archive of
  `8d9f9541b5f4bc55a7295c57fb7571beae47791a`. Its live 59,726,694-byte
  ciphertext hashes to
  `bade9212fc6167f73e9ce275a91737da33739d556ee8e4be38af40cf60c301be`;
  authenticated 59,726,678-byte plaintext hashes to
  `1d98e67565676bfe3af078e7e9afdaa28d418021136b4128c67ac7d25184bea5`.
  Owner-approved live proof verifies the signed manifest, client-key binding,
  P-256 ECDH/HKDF unwrap, AES-256-GCM decrypt, and byte identity. Token replay
  fails 401 and consumed authorization replay fails 400. The old artifact stays
  inactive for rollback, and no credentials or private key material are recorded.
- editor update checks must also prove the matching hosted build lane before
  surfacing an update: Windows waits for `windows-msvc`, Linux waits for
  `linux-clang-engine`, and pending/failing/missing job evidence withholds the
  update affordance
- editor update checks are automatic after the editor has loaded. The modal is
  shown for a newer current-platform package or a proven missing compatible
  package with the authorized fallback enabled. `Source Options...` separately
  exposes `Pair / Re-pair This Device`, `Build / Update From Source`, and
  `Download Source Project`. Pair / Re-pair clears only the nonsecret local
  registration ID; no action is a public or anonymous source download.
- update work must stay visible while it runs. During checking, packaged
  install, and source rebuild, the toolbar/modal/output evidence should report
  elapsed time, current lane, and completion/failure state instead of leaving a
  silent background worker.
- source rebuild workers must create `epoch_update_handoff.log` as soon as they
  start, mirror major download/dependency/build stages into it, and surface
  `[ERROR]` lines as failed update evidence instead of parking the UI at a
  progress ceiling.
- starting a source rebuild worker must clear stale cancel/source/replacement
  logs before the detached process is spawned. A previous worker's final error
  or staged replacement script is not valid evidence for the new run.
- authorized source ciphertext and plaintext archives live only under a hidden,
  permission-restricted `.source_access/<random>/` directory inside the owning
  cache. The native process authenticates, decrypts, verifies signed ciphertext
  and plaintext hashes, then transfers the archive into the disposable source
  worker or project-cache operation and removes the authorization directory.
  Source-build extraction remains in per-run work roots under
  `cache/updates/work/`. Cancel covers browser approval, encrypted transfer,
  decryption, and the later worker checkpoints.
- source rebuild workers always build the Windows `Release|x64` runtime lane,
  even when the editor was launched from `Debug|x64`. Debug update tests should
  keep Epoch open, watch the handoff/progress evidence, and only restart after
  the Release replacement executable is proven ready.
- source rebuild workers disable MSBuild node reuse and build parallelism for
  the update lane. The worker console stays hidden by default; set
  `EPOCH_UPDATER_SHOW_WORKER_CONSOLE=1` only when deliberately debugging the
  detached update script.
- authorized source distribution, automatic source fallback, and project-source
  download are enabled by default. Compile with
  `-DEPOCH_BINARY_ONLY_DISTRIBUTION=ON` in CMake or
  `/p:EpochBinaryOnlyDistribution=true` in MSBuild to remove those source lanes
  from a binary-only product while retaining signed packaged updates.
- managed-vcpkg source updates stage disposable overlay ports under
  `cache/updates/` when old dependency ports need modern CMake policy options;
  do not mutate the user's vcpkg checkout or mask restore failures.
- Linux source updates resolve an installed vcpkg checkout first and otherwise
  bootstrap one under the executable-local update tools cache. The worker passes
  that exact root to `build.sh`, rewrites only its disposable source snapshot to
  the managed registry revision, and builds the supported Clang full-engine
  lane. Linux renderer availability remains runtime-evidence-driven; package
  feature metadata must not override an operator-verified context.
- runtime-created update/package/cache data is app-local: updater work,
  temporary probes, source-access material, extraction folders, and helper tools
  live under `cache/updates/`; downloaded release packages live under
  `cache/packages/`; generated/runtime atlases live under `cache/atlases/`.
  These folders are disposable runtime state, not public release payload and not
  tracked source.
- downloaded update packages use the release/source asset name with its version
  suffix inside `cache/packages/`. The current always-redownload policy removes
  same-URL package caches before download so recreated release assets cannot
  reuse stale bytes; invalid package caches are still deleted and redownloaded,
  and source rebuilds use fresh per-run source snapshots.
- source snapshot extraction must prove the manifest root before running vcpkg
  or MSBuild. If a downloaded hosted archive leaves one nested top-level folder,
  the worker may repair that shape only when the nested root contains
  `Engine/vcpkg.json`; otherwise the update fails with visible evidence instead
  of continuing into a missing-manifest toolchain shutdown.
- launcher/editor mode changes and update replacement waits should use the shared
  EpochGui loading-screen/progress primitive. During an active update the
  launcher hides unrelated Start/Quit actions and exposes only the valid
  update-stage action: Cancel while the source worker can still honor it, then
  Restart after replacement evidence is ready.
- OpenGL editor composition is queue-explicit: build the normal GUI/backend
  batch before the scene, render the scene preview once, drain follow-up work,
  then replay only the explicit GUI top-layer batch for command menus and modal
  chrome. This protected order keeps command windows scene-over without
  reviving the slow scene/menu flicker.

## Project-centric runtime direction

- the editor should play the active project and scene, not a hardcoded sample
  game menu
- when no `EPOCH_EDITOR_START_WORKSPACE` override is set, the editor should
  start on a scene-backed Perspective surface; evidence-only Project/System
  surfaces may temporarily clear the scene viewport, but they must not be the
  default first-run view
- the bottom dock `Output` tab is also a scene-workbench recovery path; a user
  should not need command-line arguments to get back to the live Perspective
  viewport after inspecting project or systems evidence
- `Play Project` should reject non-project scene ids from the editor path so the
  live shell cannot quietly fall back to built-in sample launches
- built-in sample games should move behind project templates or script actions
- generated projects declare the `epoch-runtime-static` build profile and link
  a canonical reusable `EpochRuntime` static target instead of copying engine
  source into each project
- that static-compile path consumes the active engine surface from
  `Engine/include/`, `Engine/modules/`, `Engine/src/`,
  `Engine/src/scripts/`, and `Engine/resource/`
- that project shell should support both game projects and software/tool
  projects so Epoch remains a creative software platform as well as a game
  engine
- the first generated shell flow should create a real on-disk project root,
  manifest, world file, script starter, and README for both game and tool
  projects
- generated shells should land under repo-root `Projects/` so creation stays
  stable regardless of the current working directory; this is a creation
  default, not a restriction on where a user-owned project may live
- generated shells should emit `project.paths.txt` so the editor log, build
  actions, and troubleshooting flow can point at concrete files on disk
- Open Project and Switch Project use the Windows native file dialog to select
  an existing `project.epoch.json`. Admission validates that exact manifest in
  place, does not rewrite it, and rejects a duplicate project identity already
  admitted from another root
- generated-output folders are not a second project catalog. The editor does
  not populate a prebuilt project dropdown by scanning Debug/Release output;
  one admitted manifest owns one canonical project root and session
- project creation refuses to overwrite an existing manifest. An existing
  project is opened through its manifest rather than regenerated over its source
- a newly created project shell should become the active editor project instead
  of forcing the user to restart or manually stitch a second fake load path
- generated embedded-engine shells emit a child project file, a build script, a
  versioned build fragment, and script include fallback. New manifests declare
  `project_format: epoch-project-v1` and
  `build_profile: epoch-runtime-static`. Open may inspect a known legacy shell
  without mutating it; Save Project, Build, and Run atomically add missing
  canonical fields. Malformed, duplicated, future, or conflicting metadata
  fails before admission or child compilation
- generated Linux builds use Bash plus the managed current CMake, Ninja, Clang,
  and vcpkg toolchain, then build the project-owned child target into the
  platform-specific output directory
- File owns Open/Switch/Close/Save Project. Tools owns camera actions and must
  not duplicate project persistence commands
- the project launcher is a prelaunch surface for editor application, live
  renderer context, update, and exit policy; it is not another editor shell
- the direct actions are Open Editor, Plant Lab, GUI Editor, Update Epoch
  Engine, and Quit. Launch Settings selects a live compiled context before an
  editor application opens; layered game/puzzle menus do not belong here
- desktop editor hosts sample usable display work area and the requested
  capability tier, then create and center one stable native host before launcher
  or editor composition begins. Windows selects the monitor under the launch
  cursor; Linux uses the EWMH `_NET_WORKAREA` when available and falls back to
  the X11 screen extent. Default client policies are 1920 x 1200 for 4K work
  areas, 1600 x 1000 for 2K, 1440 x 900 for 1080p-class work areas, and
  1024 x 640 for compact displays. Mobile/deck capability tiers scale those
  dimensions proportionally; explicit command-line dimensions remain
  authoritative. Generated project runtimes and the sealed updater shell keep
  their own dimensions. Launcher-to-editor admission reuses the selected
  geometry instead of visibly resizing or moving the host
- Epoch defines no built-in Secondary Map, Display 2, or hardcoded output route.
  A project may create a map, radar, telemetry, or similar EpochGui pane and use
  the same tab, float, context-host, close, and redock behavior as every other
  compatible pane. The application owns any visible secondary-display controls
- ordinary pane movement defaults to in-host tab docking. Choosing Float uses
  the pane's existing context-backed native host; there is no second promotion
  feature. The pane retains one logical identity and does not clone canonical
  editor state or acquire active-editor authority
- desktop fullscreen policy may choose one borderless same-process host spanning
  selected display work areas or multiple native windows. Selection is governed
  by monitor geometry, per-display DPI, refresh/capability policy, renderer
  support, and product settings; it must not assume equal displays or one
  contiguous coordinate scale. An application that enables secondary
  presentation must retain a control on its main surface that disables it
- renderer/context-host routing remains an explicit capability for a
  renderer-backed view or diagnostic. It is not the fallback for ordinary
  Outliner, Inspector, Assets, Console, AI, or code panes. The current source
  proves monitor-aware primary sizing, central document-tab docking, and
  context-backed pane floating. Automatic display placement and fullscreen
  spanning remain incomplete
- the launcher may open project demos directly or preload a project before the
  editor, but it should not drift back into multiple menu layers or become a
  fake game shell
- launcher and editor theme ownership should stay split: the launcher can keep
  its classic steel palette while the editor stays on the darker neutral tool
  palette, and any future theme selector should preserve that separation rather
  than forcing one skin across both shells
- backend ownership stays explicit: the Editor Settings backend selector reports the
  active backend each frame. When the selected backend is compiled but not
  live, the Windows single-window host captures editor state, retires and fully
  cleans the old native backend, creates one docked replacement in the same
  host, adopts that exact manager-created context, and restores project, layout,
  selection, camera, timeline, GUI, and font state before normal backend
  activation. The render thread publishes backend-specific readiness only after
  its first successful frame, and the transaction remains held until the normal
  session path restores the editor snapshot and a queue-ordered render-thread
  acknowledgement proves the first restored editor frame completed. Raylib
  readiness additionally requires a successful owner-thread GL activation and
  completed first present; OpenGL also requires first-present evidence. A failed
  activation skips drawing so `BeginDrawing` never runs against another
  backend's context. Once adopted,
  Raylib's GLFW child is also reparented and resized through the render-thread
  owner queue so UI-thread layout never blocks its input subclass while the
  manager window lock is held. The host drains that queue before `BeginDrawing`,
  separately from draw commands, then reapplies the authoritative owner-thread
  viewport before each frame. A successful `EndDrawing` publishes first-present
  evidence; replacement completion still waits for queue-ordered editor-state
  restoration and its acknowledged frame. The replacement
  is never a second editor shell, and unavailable or failed targets remain
  visible failures rather than persisted fake selections.
- the Settings selection transaction, not the generic multicontext scan, owns replacement
  session adoption. Only that exact target is gated and excluded from generic
  enumeration until its first successful frame. It then enters the normal
  session path while remaining transaction-owned and commits only after editor
  state restoration and the first restored frame succeed; unrelated multicontext
  windows continue normally. Additional switch requests are rejected throughout
  adoption. Routed, floating, closing, failed, initializing, or stopped contexts
  are not eligible as whole-editor targets.
- normal editor switching owns one logical active backend at a time. The host keeps its
  parent window alive during the rendererless replacement gap, does not start
  the target until deferred source cleanup is complete, keeps the replacement
  transaction held through target initialization and session restoration, and
  recreates the source backend from the same preserved snapshot when target
  creation, first-frame execution, or restoration fails.
  Backend-owned child HWNDs are closed by their renderer cleanup; the stable
  manager host remains the UI-thread lifetime control and is destroyed only
  after its renderer thread finishes cleanup. Active windows, render threads,
  and deferred cleanup entries transfer under one serialized retirement
  boundary, so `IsContextRetired` cannot observe a false gap and Release builds
  cannot race container mutation against cleanup. A backend-owned child window
  procedure may mark its context stopped, but it must marshal active-window
  retirement to the manager UI thread instead of mutating session-owned
  containers from the renderer thread. Backend initialization and cleanup
  exceptions are contained at the context lifecycle boundary, logged, and
  converted into failed replacement evidence rather than escaping `noexcept`.
  Once renderer retirement is proven, the old scene, menu, GUI/font, chat, and
  preview session is destroyed before the target backend starts. The restore
  record carries only the editor snapshot and never retains the retired source
  context for a second close.
  Raylib's owner-thread resize changes size without replacing the grid-assigned
  position, restores the native GL viewport at the frame boundary, and teardown
  clears the destroyed GLFW GL binding before another backend starts. Its
  owner-thread redock is synchronous. A completed present is the backend's
  readiness evidence; the session transaction independently owns parent,
  restoration, and restored-frame acceptance. Adopted
  Raylib/SDL/SFML child windows forward text and keyboard events to the shared
  GUI input queue as well as preserving their backend event path. Failed Linux
  thread initialization also runs backend cleanup before fallback. Mixed-backend
  grids remain explicit diagnostics and are not this workflow.
- launcher context selection is configuration, not an action button: the chosen
  live context is promoted to the primary host slot before Open Editor, Plant
  Lab, or GUI Editor starts
- `editor.application` owns the registry, profile contract, surface mask,
  initial camera/dock policy, pane ownership, authoring/run permissions, and
  scene validation shared by all editor applications
- `editor.standard.cpp`, `editor.plant_lab.cpp`, and `editor.gui.cpp` own the
  three canonical editor scenes. Plant Lab owns separate custom-tree and forest-configuration authoring
  and GUI canvas transforms remain GUI-scene data
- Forest Factory remains a standard-editor surface/tool. It browses and imports
  Plant Lab/package outputs and places vegetation into project-owned scenes
- `editor.scene.cpp` owns ordinary project/script profiles and project
  serialization, routing application projects through the application registry
- `editor.application.cpp` is the live shell and seed-to-runtime adapter. It must enforce
  application policy and must not recreate dedicated scene generation
- default editor seed profiles should stay lean. Sandbox, Project Hub, and
  software/tool startup should keep only the workspace/root, camera, and light
  entities they need; starter cubes, grids, player starts, tray panels, fake
  tool panels, and vegetation props must be created only by explicit
  application/package actions or real scene data.
- current `.epoch` scene/world files are canonical snapshot-format-3 project
  documents. The editor production-loads them into `SceneDocument`, projects
  the UI/render mirror from that document, atomically saves/reopens them, and
  compiles the same semantic object/material revision for project runtime.
  Seed entities remain creation defaults and migration fallback, not parallel
  canonical state
- repo-root `Projects/` is a generated local-project area. The editor can use
  project manifests, build logs, output paths, and `PROJECT_NOTES.md` there as
  evidence, but those files are not automatically promoted into tracked source

## Project Save, Build, And External Run

### Project input and creation defaults

Generated game/platformer shells receive the canonical project input profile
and controller through checkpoint `38d541b2`; Tool/sandbox remains explicit
opt-in. The project-owned settings controller at `408c496f` stages keyboard,
controller, and dead-zone edits with validation, discard, reset, and canonical
save semantics. The editor workflow landed at `bbda8b7b` and its recorded
contract at `de937045`. Project Defaults stores only the future-creation choice
until project creation; it never silently rewrites an existing project.

The same defaults surface keeps optional self-iteration provisioning off by
default. Local Qwen, engine-selected, and external MCP remain distinct choices,
and provisioning does not start a model, connect to an endpoint, or create a
listener. Build-safe contracts prove mapping and persistence; no live GUI
eye-test is claimed for this churn checkpoint.

The project workflow has one ownership path:

1. Materialize or open one validated project root and manifest.
2. Save dirty scene and script authoring state before a dependent operation.
3. Prepare build inputs stamped to the selected project and committed scene,
   then start one generation-stamped build attempt through `project.lifecycle`.
4. Accept an output only when its project, input, build, artifact, and verification
   evidence match; run only that accepted artifact for the same project revision.
5. Track runtime generation against the accepted artifact, report launch/exit
   errors visibly, and return focus according to editor policy.

Project Save is not a UI-only flag. Build does not compile an unrelated sample,
and Run does not quietly fall back to an editor-internal game. A selected-script
build is a narrower compiler operation and cannot mark the project built or
authorize external Run.

The strict `project.lifecycle` ledger covers the selected project, committed
scene, materialized shell, prepared inputs, active build, verified output, and
runtime. The production editor populates it directly. Project selection uses a
stable project identity; successful atomic scene persistence records the scene
revision; verified shell creation records materialization; source, project,
script, and link-input stamps produce the build-input generation; and both
asynchronous completion consumers settle the exact claimed build generation.
Completion recomputes input freshness and records a SHA-256 digest for the
accepted executable; immediate and later Run recheck those exact bytes.
Missing, failed, changed, stale, cross-project, or mismatched evidence fails
closed. Scene-only saves advance scene evidence without forcing a relink. The
Project workspace shows the current lifecycle action, reason, and
project/scene/input/build/artifact generations.

The source contains production materialize/save-reopen/build/child-self-test
lanes for the registered generated profiles and a standalone external GUI Run
proof. `platform.child_process` now owns the bounded native process boundary:
stable handles, exact argument vectors, duplicate-correlation focus, one
exclusive project-runtime group, polling, PID/elapsed/exit evidence, graceful
Stop, and forced Stop. The editor correlation key includes selected project,
accepted artifact generation, and SHA-256; lifecycle runtime evidence starts
only after native launch and stops only after observed termination. Linux uses
`fork`/`execv` and an isolated process group instead of a shell command.

The Project workspace exposes that observed state and the Focus/Stop controls.
An isolated Windows MSVC/Linux Clang contract proves supervisor behavior without
requiring unrelated renderer packages. The production Linux full-engine lane
remains Clang 22 plus the shared vcpkg manifest and validates every enabled
backend through that vcpkg-backed build.
Visible repeated external Run, Focus/Stop interaction, cooperative build
cancellation, and current-pass eye proof remain acceptance work; build-safe
contracts must not be described as a live runtime pass.

## Scripting and reload workflow

- script sources belong to the engine/project scripting tree used by the active
  project
- engine-owned scripting is compiled C++23, not a text-macro or string-eval
  layer. `scripting.compiler` launches the selected compiler with an argument
  vector and targets the canonical shared-library path beside the source
- compilation captures source and current-output evidence, writes a bounded
  same-directory candidate, rejects source/output changes during the build,
  verifies the candidate, publishes it by same-filesystem atomic replacement,
  and verifies the published artifact before reporting success
- the editor saves dirty selected source before Build Selected Script, permits
  only one build at a time, and routes that work through the shared editor
  TaskGraph while retaining visible target, generation, completion, and failure
  state
- `scripting.system` resolves the same verified output path for load/reload.
  Compiler launch, nonzero exit, source/output races, candidate/publication
  verification, load, and entry-point failures remain separate diagnostics
- a script build does not mark the project built. Project Save, Build, and
  external Run continue through `project.lifecycle`
- the scripting/project phase must account for editor-hosted work and generated
  static-runtime builds that consume the same engine surface from
  `Engine/include/`, `Engine/modules/`, `Engine/src/`, `Engine/src/scripts/`,
  and `Engine/resource/`
- the project/assets dock should expose script lists, source paths, run/build
  actions, and compile/load diagnostics
- the `Assets` document surface owns script visibility: it creates project-local
  `.ascript.cpp` stubs, lists project and engine script files, and exposes the
  active-project browser so scripts can be selected without command-line
  digging. The Outliner Assets/Scripts tabs are compact navigators into that
  full-width surface rather than duplicate editors.
- the central Asset Manager is intentionally not the Sandbox. Its Browser,
  Textures, Scripts, and Graph tabs keep navigation, texture authoring,
  full-width source editing, and dependency evidence in one workspace while
  the bottom Assets dock remains compact status
- the Assets workspace also owns one project texture controller: bounded BMP,
  TGA, and P6 PPM discovery/import, atomic Library publication, exact restore,
  selected-scene semantic material assignment, and diagnostics share the same
  project pipeline used by runtime compilation; physical handles never enter
  scene files
- the current Asset Browser and World Outliner Assets tool share one project
  texture controller. The central surface is rooted at the active project,
  exposes bounded folder navigation, search and type filters, and classifies
  project GUI sources separately from images, maps, audio, models, and code.
  Folder rows remain navigable under a type filter. Browser results use the
  reusable virtualized EpochGui asset grid with stable selection/activation,
  bounded right-click actions, and lazy visible-thumbnail resolution instead of
  an unbounded button list. Open, Show Details, and Copy Path resolve against
  the stable context target and existing project controllers.
- the controller hydrates editable texture source and compiled Library catalogs
  after restart, preserves current source revision across reimport, creates
  Checker/Gradient/Solid P6 sources, imports bounded BMP/TGA/P6 content, and
  regenerates exact RGBA8 artifacts. Compiled previews use reusable image-button
  controls keyed by artifact identity rather than text-only fake thumbnails.
- active texture editing exposes sparse layers, drag strokes, blend, opacity,
  visibility, locking, order, undo/redo, sampling, independent U/V addressing,
  alpha cutoff, color space, scene assignment, and an interactive image canvas.
  Source is canonical; decoded images, thumbnails, Library output, and native
  residency remain reproducible or disposable.
- GUI Editor creates panel, button, text, image, image-button, tab-set,
  text-input, slider, and scroll-area widgets through
  `authoring.gui_document`. Reusable tabs select Widgets, Layout, and Hierarchy;
  authored tab pages use stable handle identity separately from editable labels.
  Content, layout, style, visibility, enablement, keyboard/pointer policy,
  actions, and tooltips enter semantic operations with document undo/redo.
- Standard Editor GUI Canvas and GUI Editor share one canonical document rather
  than renderer cameras aimed at debug rectangles. Standard Editor deliberately
  exposes one interactive project-placement canvas plus hierarchy/layout/history
  tools. The projectless launcher GUI Editor owns widget creation and the full
  Canvas, Runtime Preview, Component Graph, and Styles workspace. Canvas and
  Runtime Preview read the same document; hierarchy is parent-relative,
  selection opens real Layout and Properties controls, text inputs and sliders
  update semantic content, tab sets select stable page handles, and image
  controls resolve through the project texture catalog. The scene projection
  remains for compatible project/runtime publication.
- the Hierarchy view projects canonical document parentage and commits reorder,
  root, nest, and delete operations through `authoring.gui_document`. The GUI
  Structure Graph shares the reusable pan/zoom/fit/drag canvas and converts
  parent-output to child-input connections into validated temporal reparenting;
  right-click disconnect moves a legal child back to the document root. Invalid
  containers, cycles, tab-page parentage, stale handles, and limits fail without
  partial mutation.
- GUI Editor is not a generated project. It opens a real project manifest when
  project binding is needed, preserves its application role while doing so, and
  can create Blank Canvas, Desktop App, Dashboard, Mobile App, and Game HUD
  documents. Native `.epochgui` Open and Save Template use the same bounded
  integrity codec as project GUI source; writes use verified temporary files and
  atomic replacement, and unsaved replacement requires explicit confirmation.
- Canonical GUI source is `<project>/Assets/Gui/main.epochgui`. Registered 2D
  project profiles declare that path explicitly in `project.epoch.json`. Missing
  legacy fields migrate narrowly; malformed, duplicate, alternate, symlinked,
  or over-budget source fails closed. A missing source receives the shared Game
  HUD semantic template once, while an existing valid authored document is
  preserved and recompiled. Materialization atomically publishes source,
  persists the content-addressed `Library/Gui` artifact, restores it exactly,
  and builds one renderer-neutral runtime frame before project build proceeds.
  The bounded integrity-checked codec restores the exact document revision at
  project load;
  Save writes and verifies a same-directory temporary before atomic replacement.
  Save Project, Build, and external Run require both GUI and scene publication
  evidence. The scene/canvas view is a preview projection, not GUI authority.

  The editor also recognizes one exact historical generated source: the
  canonical snapshot containing only Epoch's default `MainCanvas` root. That
  complete serialized shape, and no broader “single root” heuristic, may
  migrate to the current Game HUD or Desktop App starter. An authored Blank
  Canvas is therefore preserved. Migration is an in-memory, visibly unsaved
  edit until GUI Canvas Save or Project Save publishes it.
  Save compiles the accepted document into a validated immutable artifact under
  `Library/Gui`; ProjectPlayScene restores the matching logical source through
  the project GUI runtime and EpochGui adapter. `move_x`, `move_y`, `jump`,
  `interact`, `pause`, and `reset` widget actions are admitted only when present
  in the compiled project input profile. Activations and normalized slider
  values merge once into the next deterministic action frame; unknown actions,
  incompatible events, and absent profile actions fail closed. `editor.return`
  is the explicit host command. GUI keyboard capture suppresses physical
  gameplay sampling without suppressing admitted GUI impulses. Arbitrary
  application/script command routing remains future work.
- physical controller ownership remains process-wide in `input.controller`.
  `project.input_controller` reads one immutable generation-checked snapshot,
  maps only sources declared by the compiled project profile, preserves held and
  axis state, rejects stale/malformed snapshots without partial mutation, and
  suppresses repeated press edges when multiple project frames observe the same
  physical revision. Focused GUI controls consume the physical revision into a
  discarded frame so releasing focus cannot replay an old press.
- Project Controls edits keyboard keys, controller buttons, axes, slots, and one
  uniform dead zone through semantic profile operations. Every accepted edit
  advances source revision, recompiles, saves canonical
  `Assets/Config/input_profile.epochinput`, and publishes the matching
  `Library/InputProfiles/input_profile.epochinputc`; duplicate physical sources
  fail closed.
- GUI Canvas Save publishes that canonical source. Reload rereads the same file
  only when doing so cannot discard an unsaved document revision. Undo, Redo,
  Delete, and deselection resolve stable GUI document identity and then rebuild
  the compatible scene projection; they do not edit the preview vector directly.
  Global history/delete shortcuts are muted while a reusable GUI text/select
  control owns keyboard input.
- Scene and GUI history share command placement but not document storage. World
  Edit Undo/Redo operate on `scene.document`; GUI Canvas Undo/Redo operate on
  `authoring.gui_document`. Each editor window, Inspector, and detached pane
  reads the current canonical projection instead of keeping an independent scene.
- script source resolution should prefer the active project's local `scripts/`

### Typed workspace commands

`editor.workspace_commands` is the data-only command boundary for World, GUI
Canvas, Forest Factory, Plant Lab, Timeline, Project, Assets, AI Development,
and Systems. Each surface owns an explicit command catalog and typed target.
Intent planning captures workspace, surface, project, document, selection, and
async generations; any stale fact fails closed before mutation.

The editor's global Undo, Redo, Duplicate, Delete, and Focus routes resolve
against the active surface. Unsupported actions are disabled with visible
evidence; they never fall back to the World document. AI Development can
request bounded planning work, but its command vocabulary cannot express
approval, live-source promotion, release publication, Git, or unrestricted
execution.
  folder before falling back to template or engine-owned script roots, so the
  dock and editor run actions operate on the real generated project shell
- script starter creation should append `PROJECT_NOTES.md` entries; a useful
  Sandbox iteration must leave at least one of: build log output, script-host
  log output, explicit tool-trace or eval evidence, selected project file path, or project
  notes explaining what changed
- build diagnostics should now cover the generated child-project build path too:
  entry source, generated project file, build script, build log, and expected
  output executable should all be visible from the Project workspace
- generated child projects, including the Sandbox shell, should repair stale
  Windows toolset metadata to `v143` before invoking MSBuild, and the checked-in
  engine projects they reference should stay on the same VS 2022 toolset.
- generated child projects must also repair their Windows app linker surface to
  match the editor target: `RAYLIB_DLL`, `raylib.lib`, SDL3 static Windows
  system libraries, and no mixed SFML static/dynamic library set. Backend
  selection stays runtime-driven through `--backend`, not a project rewrite.
- generated child projects expose `--project-self-test` so Sandbox and
  ProjectLauncher output can be verified without opening GUI windows.
- the editor exposes explicit Open Project, Switch Project, and Close Project
  commands over discovered profiles. Open admits the selected authoring scene;
  Close releases project-bound editor/runtime evidence and returns to the
  current application scene without deleting or rematerializing source files.
  Plant Lab is a launcher-owned authoring application, not a project profile.
- the checked-in engine exposes `--editor-project-self-test <id>` for the same
  route from the real engine binary. The route materializes the selected
  profile, production-loads its canonical scene, atomically saves it, reopens
  and compares it, builds the child, then runs generated `--project-self-test`.
  Gameplay-capable child evidence includes the immutable Canvas2D hash plus
  logical texture bytes, emitted sprite/batch counts, collision surfaces and
  peak contacts, resident audio bytes, and Canvas2D rejection count. The live
  Project Runtime Preview reads the same request-driven snapshot on a bounded
  cadence without rasterizing or changing backend frame order. Generated
  gameplay acceptance enforces the `T1-GLES/mobile_30` platform budget and logs
  its violation mask; the preview labels the same portable baseline explicitly.
  Supported generated profile IDs are `sandbox`, `platformer`, `twodstudio`,
  `projectlauncher`, and `softwarestudio`. Tool-trace/eval evidence and project
  notes remain additive. If the pass should bind to a local helper model, set
  `EPOCH_AI_MODEL` or a compatible explicit model variable before launch;
  discovery still remains separate from activation.
- `--editor-ai-gate-self-test` runs the deterministic helper-review gate without
  launching the GUI. Use it before letting helper LLM replies influence curated
  evidence review, eval changes, or source-change planning.
- `--engine-contract-self-test` runs only the pure Forest Factory, package
  registry/model-gate, timeline streaming-save, input profile, and scene
  snapshot/serializer checks, then exits before project-profile builds, child
  runtimes, updater work, OS-AI gates, or renderer startup. Use this as the safe
  fast contract check when GUI/runtime validation is not explicitly approved.
- `--engine-validation-self-test` first runs the pure engine contract lane for
  Forest Factory, package registry/model-gate, timeline streaming-save, input
  profile, and scene snapshot/serializer behavior, then runs every registered
  editor project profile through the engine-side project self-test route, then
  runs the OS AI gate. The contract lane is build-safe, but the full validation
  command remains operator-gated in this worktree because project self-tests can
  create child processes and touch
  renderer/runtime state.
- generated game project shells can reference the built-in `engine_arcade`
  mini-runtime scenes such as Snake/Tetris/Pacman through the script host; they
  never copy those implementations out of the kernel engine or install them as
  a Package Manager payload. The runtime also records the shared
  `engine_arcade.screen` 512x512
  sampled render target. One deterministic attract-pattern contract is rendered
  into backend-owned scene surfaces by OpenGL, SDL3, SFML3, Raylib3, Vulkan,
  DirectX, and Software, then sampled by the cabinet screen instead of relying
  on project-local ad hoc textures. This source path remains `Partial` pending
  visual and repeated-switch proof. Editor selection and runtime scene state
  remain engine-owned and never imply an add-on install/remove transaction.
- Plant Lab is the dedicated launcher application for scene-backed procedural
  vegetation authoring and deterministic temporal-graph preview.
- Forest Factory is the standard-editor placement portal and a core editor
  feature. Optional heavy vegetation libraries may be described by the Site,
  but no project payload is installable until EpochEngineExtensions provides an
  immutable, licensed, hash-bound source artifact admitted as an individual
  add-on. The Extensions repository is authority, not itself a package.
- the command-menu Package Manager discovers individual project add-ons, tools,
  and model assets from the bounded Epoch Site catalog. EpochGui is already
  linked into every non-CLI application, core engine systems are already in the
  engine, and catalog/repository containers are filtered rather than displayed
  as installs. Downloadable source packages must compile through a human-gated
  project path and must not auto-run servers, listeners, inference, or services.
- Package Manager install attempts must show visible per-package state in the
  modal using the shared GUI progress bar. Package selection should use a
  scrollable list with per-package Install/Remove/Review Gate actions instead of
  a combo-box-only selector. Selecting a package should update the selected
  package/status text immediately; pressing Install should either
  materialize a local package, stage a human-approved download/build gate, or
  display the reason the package is blocked. The modal body is a clipped shared
  GUI scroll area; package rows and progress bars must not bleed into the scene
  or into command-menu/modal chrome.
- `scenesnapshot`, `sceneserializer`, and `scene.persistence` now own canonical
  `epoch_snapshot 2` project scene evidence. Older v1/editor text is accepted
  only through the bounded migration reader; new saves always emit v2.
- explicit Save, Play, Build, and Run validate the active project/application,
  write a verified temporary payload, atomically replace the scene file, and
  fail closed when durable evidence cannot be committed.
- project preview consumes the same saved revision through `scene.runtime`, a
  deterministic renderer-neutral projection. The live editor entity collection
  remains a transitional UI adapter until every mutation routes through
  `SceneDocument` semantic commands.
- generated project manifests may declare one canonical `tilemap` path beneath
  `Assets/Maps`. Canvas2D project runtime resolves that path from the actual
  project root, not the engine checkout.
- authoring-enabled runtimes load and compile canonical map source, atomically
  publish its exact artifact beneath `Library/TileMaps`, restore authenticated
  texture dependencies, and publish one immutable Canvas2D scene per context.
  Game-only builds can compile out map authoring and restore the same exact
  Library artifact. Missing source may fall back only under explicit
  prefer-source policy; malformed source always fails closed.
- map source and semantic history remain canonical, Library output is
  reproducible, and Canvas2D scene/resource residency is disposable. Context
  exit or replacement retires only the physical scene publication.
- OS model rows are on-demand user assets. The current admitted payloads are
  Qwen3.8 27B and NVIDIA Nemotron 3 Nano 4B BF16. An explicit Install action
  downloads pinned Hugging Face files with resume/retry into a sibling staging
  directory, verifies exact paths, file types, sizes, and SHA-256 values in a
  background task, and atomically publishes a deterministic receipt under
  `cache/models/<package>/versions/<revision>/`. A failed digest removes only
  the bad staged file so retry is bounded. Generated projects reference the
  shared cache only after explicit selection; weights are never copied into
  project source/build output and installation never activates inference.
- Project Run is project-owned, not editor-clone-owned. The Project workspace
  must expose the target backend/context and child project launches should use
  standalone single-context flags such as
  `--standalone --window-mode standalone --backend opengl`.
  Empty or stale `auto` backend payloads are clamped to `opengl` before the
  child process is launched so generated projects do not accidentally revive the
  parented multicontext shell.
  If the expected child executable is missing after the build, the editor must
  block the run with visible evidence instead of falling back to a parent
  multicontext `Project Runtime` scene across every dock.
- Launch Single Context uses a child-build freshness gate. The editor saves and
  repairs project evidence, then launches the existing child executable when the
  output is newer than the project entry source, active script source, generated
  Windows project file, generated build script, and engine static library.
  A current matching child is focused instead of relaunched; another artifact or
  project holding the runtime group is surfaced for Focus/Stop instead of being
  overlapped. When
  any of those inputs are newer or the output is missing, the normal serialized
  project build path still runs before launch. Scene and manifest writes are
  runtime inputs and should not force a relink by themselves.
- Docked editor panes use selected-backend multi-pane ownership by default.
  Switching the 3D rendering window to a different backend is an explicit
  diagnostic/accurate-preview action; normal docking should not mix unrelated
  renderer families unless the operator requested that proof grid.
- The centered Run button follows the same split: normal generated projects
  save project evidence and launch through the selected single-context child
  backend, rebuilding only when the freshness gate says the executable is stale;
  the guarded engine-development lane stays editor-shaped because it manipulates the
  checked-out engine and needs visible build/review evidence.
- workspace changes from the launcher/editor toolbar may use short loading
  feedback through the shared GUI progress primitive only after that feedback is
  proven not to change scene viewport geometry or revive menu/modal flicker.
  Console Dock is status-only and must not drive these workspace switches or
  package actions.
- network/server packages follow the same gate. Shared client/network runtime
  contracts may ship inertly in the engine, but authoritative dedicated
  headless server support is an optional package for projects that explicitly
  choose that model. Client listen/nondedicated and future competitive
  client-predicted paths remain separate opt-in packages. Software projects,
  single-player games, and minimal generated engine clones must not receive
  server/listener code by default.
- Heavy optional add-on source lives outside the engine repository. The
  canonical source/catalog authority is
  `https://epoch.adamrushford.chatgpt.site/git/EpochEngineExtensions.git`.
  Each admitted row describes one generated-project dependency or explicit
  child tool; the repository itself is never installed. EpochEngine has no
  native plugin ABI/loader: reusable core systems remain linked into the engine,
  while admitted add-on source compiles into the project that selected it.
  Downloaded/generated package payloads resolve under executable-local
  `cache/packages/` and never execute automatically.
- research prototypes such as voxel terrain, planetary rendering, procedural
  vegetation, and tool harnesses should enter Package Manager as local
  research-package candidates first. A package candidate needs provenance,
  source/hash, build/test commands, known limitations, and a proposed engine API
  boundary before mainline source promotion.
- Engine Development Sandbox controls always target the `sandbox` profile. The
  profile is classified as a guarded engine-development lane, not a normal
  game/tool project, so it mirrors the checked-out engine/editor shape for
  manipulation, build, and testing instead of inheriting generated-project
  presentation behavior.
- Engine Development source proposals are two-pass and context-first. AI Controls
  owns objective entry, host-curated existing-file candidates, model/endpoint
  evidence, and the explicit Share Curated Context or Reject Selection action.
  Candidate ranking uses path metadata only; sharing reads and sends only the
  displayed bounded UTF-8 files to the selected endpoint. Models cannot request or
  invent paths. Review Proposal and Approve Sandbox or Cancel Proposal remain
  attached to the resulting AI Chat response. Changing the objective invalidates
  reviewed evidence. The returned proposal must survive deterministic objective
  relevance, exact-source-symbol, no-op, preservation, and module-ownership checks
  before it becomes a reviewable data record executed only in a disposable
  sandbox; it cannot write live source or self-certify build/test results.
  A request for one source-proven defect in a named subsystem is bounded without
  a preselected symbol: one insufficient-evidence reply receives one recheck
  against the unchanged reviewed bytes, and a repeated refusal stops. A fresh
  share clears prior correction state. The larger source-file/byte status is the
  local disposable build-workspace copy, not extra model context.
  Each completed source-model generation is routed into the source controller
  exactly once; malformed packets receive at most two automatic host-diagnosed
  retries without another click. Direct llama transport strips only line-framed
  bytes outside a complete structured envelope. Unique-search remains mandatory,
  so an ambiguous objective is rejected rather than assigned an inferred target.

- project shells should only be materialized by explicit operator action:
  File > Save Project, Project > Save Active Project, or the centered Run
  button. Merely selecting a project profile must not create files silently.
- the centered Run button now saves normal generated-project evidence, checks
  child executable freshness, rebuilds only when source/build inputs are stale,
  and launches the selected single-context child backend. If the build fails,
  launch is canceled so stale `Projects/**/bin/...` outputs are not mistaken for
  the result of the current run. The Engine Development Sandbox is
  intentionally excluded from that generated-project launch path and remains
  editor-shaped for visible engine manipulation, build evidence, and review
  gates.
- generated project builds are serialized inside the editor process, and emitted
  Windows `build_project.ps1` scripts also take a repo-level build lock. Until
  ProjectLauncher/Sandbox child builds have isolated engine-object/module/PDB
  output directories, every generated child build must either hold that lock or
  fail visibly instead of racing over shared `EpochEngine` outputs.
- the Project workspace should also surface simple existence checks for the
  manifest, entry source, build script, `project.paths.txt`, expected output,
  build log, and active script source so the user can tell whether the shell is
  real without leaving the editor
- normal generated projects expose a selectable camera style from the Project
  workspace. The first production choices are editor orbit, first-person runtime,
  and locked 2D canvas. That setting applies to Play In Editor and the project
  preview path; the Engine Development Sandbox keeps following editor tools
  because it is an engine/editor manipulation lane.
- viewport movement starts with shared input actions: LMB pan, RMB orbit,
  wheel zoom, movement/look actions, and `Home` reset. Editor Settings and the
  Project workspace expose the first input-profile presets, and project runtime
  launches carry the selected profile. The shipped presets keep movement and
  look keys non-overlapping so a single key press does not translate and rotate
  the camera at the same time. The next promoted version needs per-action
  rebinding and project/package serialization so projects opt into bindings
  instead of inheriting every editor-only control.
- hot reload remains a development feature and needs smoke coverage instead of
  trust

## 2D Scene/UI editor surface

- `2D Scene/UI` is the same scene viewed through a dedicated Canvas2D camera, not a
  separate scene or project island.
- entering `2D Scene/UI` creates/selects an editor-only `Canvas2D` plane and switches
  the preview camera to the locked 2D Canvas rig
- the `Canvas2D` plane is an upright XY-style editor canvas viewed by a
  front-facing orthographic camera. It should not be a floor-like XZ plane; the
  2D workspace reuses the scene view from a locked 2D perspective, similar to
  Unity's 2D scene editing mode.
- `render.preview_grid` owns the Canvas2D projection helper. Editor object
  selection and the OpenGL, DirectX, Raylib, SDL, SFML, Vulkan, and
  software-preview paths should all use that helper so 2D editing behaves like
  a Unity-style scene camera locked to a 3D canvas instead of drifting per
  backend.
- future 2D work should add tile/layer/canvas tools on top of this same
  entity/project spine

## Surface-relative placement

- `scene.surface_alignment` is the renderer-neutral authority for vertical
  local bounds, world-bound transforms, support-surface elevation, group-bound
  merging, and bottom-to-surface placement.
- Centered box geometry, base-anchored plant geometry, negative scale, and
  signed ground elevation use explicit bounds rather than unrelated half-height
  arithmetic. Applying the same alignment twice is idempotent.
- Static-mesh creation, supported AI transforms, duplication, Plant Lab preview
  projection, and Forest Factory placement route through this contract. Plant
  and forest groups translate as one set from their merged lowest bound, so the
  generated form retains its internal offsets while contacting the primary
  Ground surface.
- This is source/contract placement truth. Arbitrary imported-mesh bound
  extraction, terrain collision, physical settling, and exact-build visual
  acceptance remain separate gates.

## Systems And Task Graph Workspace

The Systems workspace has two data authorities and one portable projection:

- `systems.registry` supplies live registry lifecycle, dependency order,
  initialization state, frame counts/timing, and per-system update/last/peak/total
  timing. Timing sampling is disabled by default and must be enabled only while the
  central Systems workspace is visible; leaving or hiding the workspace disables
  it again so normal system updates retain the direct fast path.
- `editor.task_scheduler` owns the bounded shared editor operation lane over
  `taskgraph.dotsystem`. AI evidence builds, selected script builds, project
  builds, and the approved tool harness submit there. Its immutable snapshot
  reports revision, lifecycle, workers, queued/running nodes, cumulative
  completions/failures, truncation, accepted/queued/started/finished timestamps,
  queue latency, run duration, totals, peaks, and per-node state/dependencies as
  read-only Live Scheduler evidence.
- `authoring.task_graph` supplies an isolated temporal learning document with
  stable handles, semantic add/remove/rename/connect operations, undo/redo,
  validation, deterministic topology, critical path, and parallel-wave
  simulation. It never schedules, cancels, or executes a live task.
- EpochGui's `SystemWorkspace` controller owns bounded row replacement,
  hierarchy, filter/sort/status projection, selection, keyboard navigation,
  summaries, and explicit empty/error/stale states without importing engine
  systems.
- `editor.systems_workspace` adapts registry, live scheduler, and learning
  document snapshots into that portable controller. It owns sampling intent,
  revision-aware refresh, bounded timing history, and truthful
  unknown/unavailable/warming/available/paused states, not renderer drawing or
  scheduler execution authority.

- EpochGui's node-graph workspace now supplies stable node/pin/edge projection,
  contain-aligned hit testing, selection, navigation, and viewport intent for
  the Learning Graph. The authoring document remains the mutation authority
- Live Scheduler snapshots include bounded dependency edges, and editor task
  exceptions settle as failed nodes/futures rather than falsely incrementing
  completed work
The central workspace exposes one tab row for concise Overview, Systems,
Scheduler, Task Graph, Time, and Renderer views. Graphs are the primary content;
zero captured scheduler work has an explicit compact empty state rather than a
blank graph surface. Lists and detail panes consume cached
owning snapshots. Expensive graph textures/layout are rebuilt only when source
revision, viewport, filter, selection, or explicit refresh changes; they are not
regenerated every editor frame. Same-size chart surfaces replace pixels in their
existing atlas entries and preserve sprite handles. The Console Dock mirrors
compact status only.

The visible data should include lifecycle, order, dependencies, state, queue and
worker pressure, frame/system timing, peak/total/update count, failure text,
snapshot age/revision, truncation, critical path, parallel waves, and selected
node/system details. Unknown and unsampled values remain labeled as such.

The controller and editor panels are integrated in source. The visible Systems
surface enables registry timing only while selected, caches scheduler rows and
learning projection by revision, shows real editor work in the read-only Live
Scheduler, and provides add/connect/remove, undo/redo, reset, filtering,
selection, topology, critical-path, and parallel-wave inspection only in the
Learning Graph. The Time view shows bounded registry-update spans and real
scheduler queue/run evidence; it does not label registry measurement as total
editor frame time. Release visual responsiveness, interactions, and screenshots
remain unclaimed until an operator-approved GUI run.

Top-level editor modes still route the central work area. `3D Scene` and `2D
Scene/UI` keep the real scene viewport; Assets, Forest Factory, Video, Project,
AI, and Systems use dedicated surfaces. The movable Output pane owns Project,
Assets, AI Output, and Systems evidence filters rather than duplicate status
tabs or command surfaces.
- the stable Windows top-row contract is visible real child panes:
  `GLFW30`, `SDL_app`, and `SFML_Window`
- helper `EpochChild` wrappers are implementation detail only:
  they should stay aligned to the same dock slot while parented and must stop
  lingering as floating top-level shells after a real redock
- SDL3 and SFML3 should keep obeying their elevated host-child hierarchy: the
  visible proxy host is the movable shell during undock/redock, while the real
  backend child stays nested inside that shell instead of pretending to be an
  independent top-level owner
- Raylib currently uses the direct-child dock contract with a hidden parked host
  rather than a live proxy child, so harness and runtime checks should judge it
  by that honest ownership model instead of forcing the SDL/SFML proxy-child
  expectations onto it
- Raylib dock/undock/redock commands must be executed on the Raylib owner
  thread. The Win32 dock proc may queue those commands, but it must not mutate
  the GLFW/Raylib child HWND directly from the parent host thread.
- when validating Win32 parented multicontext behavior, a live window-tree probe
  should show the real backend child classes as visible pane owners and helper
  wrappers hidden in the docked state
- if the synthetic harness disagrees with a clean human six-pane validation,
  treat the live editor behavior plus the Win32 subsystem logs as the deciding
  truth and repair the harness/probe afterward instead of mutating runtime code
  to satisfy the harness
- maximize/restore validation should keep using the pane owner that is actually
  parented into the grid slot at that moment, not a stale abstract "primary"
  HWND that may still be mid-takeover
- child-docked backends should consume the grid/`WM_SIZE` dimensions as the
  authoritative resize request; avoid feeding stale child rects back into the
  same resize path
- once a backend is docked as a child pane, do not reapply top-level backend
  window-size APIs that can silently restore window chrome semantics and break
  the parented slot geometry
- editor text-entry validation is still an active gap:
  the shell needs a repeatable typed-text smoke for AI chat and other edit
  boxes, not just click/focus proof
- the first shared arbitrary scroll-area primitive now exists and is used by
  the World Outliner, Inspector, and non-output Console Dock pages. Future
  editor windows should build on this path instead of adding new per-panel
  scrolling hacks.
- the World Outliner, Inspector, Console Dock, and AI Chat now have first-pass
  open/close layout state plus draggable side/bottom splitters. This is the
  current docked layout control layer, not yet the final IDE-class window host.
- GUI draw and hit testing should remain clipped to active panel content so
  buttons, rows, and text do not bleed over or steal input from the Perspective
  scene view.
- detached context windows are explicit optional panel hosts, not hidden
  always-running backends. Real pane title-bar drag/release gestures route
  existing panes such as World Outliner, Inspector, Console Dock, and AI Chat
  into cloned native context panels. A successful route hides the docked source
  pane until the routed pane closes, docks back, or receives native close
  cleanup. The routed context refreshes GUI/font upload state before its first
  panel frame so cloned panes do not inherit broken atlas state. The Window menu
  owns show/hide/reset and emergency source-pane restore; it is not the primary
  detach surface. Optional low-level routes such as `floating.gui` remain host
  infrastructure, not the current user-facing feature. Backend selection stays
  in Editor Settings. Additional GUI containers must use visible
  request/status paths with focus ownership, teardown, and evidence logging.
  Games, mobile apps, console targets, and headless tools may omit native host
  routes entirely while still using the portable `EpochGui` layout library.
  True drag/drop redock behavior remains a later native-host movement feature.
- editor context selection is an in-process handoff, not a process restart. The
  Editor Settings selector starts one state-preserving whole-editor replacement
  transaction. While the manager owns retirement, creation, restoration, and the
  restored-frame acknowledgement, the selector becomes a visible non-interactive
  status and cannot emit another request. Unsupported hosts fail closed without
  opening another shell or changing only the label. This remains a desktop
  editor/tool-host workflow, not a requirement for game/mobile products.
- time diagnostics should show the shared simulation clock state: pause/resume,
  scale, fixed-step cadence, accumulator, and simulated time
- time diagnostics should also show the current frame step budget and the
  max-steps-per-frame clamp so pacing policy is visible, not implied
- the shared preview marker should prefer the editor focus projected onto the
  grid plane, then fall back to the center camera ray and last valid hit, so
  editor panning and the visible look spot stay stable across contexts
- this surface should help unify renderer/backend behavior instead of becoming
  another debug text dump

### Context Implementation Contract

The editor has two separate ideas that must stay separate in code and UI:

1. **Backend/context selection**: the Editor Settings combobox chooses the
   renderer/context family for the editor session.
2. **Floating/routed GUI containers**: pane title-bar drag/release gestures
   present existing editor panes in their own cloned native/context windows.
   Optional desktop host routes such as `floating.gui` can still exercise lower
   level GUI hosting, but they are not the user-facing popout feature.

Do not merge those ideas back into one "Context Driver" button. A context
selection action may open or focus an editor context, but it must not open the
floating GUI proof panel. A floating GUI route may show its renderer as evidence,
but it must not own backend switching.

Project capability policy is a third, separate concern. It declares what a
project requires and whether experimental/software fallback is admissible. The
Settings surface reports both editor and project-run admission; selecting a
context still performs the existing state-preserving editor replacement rather
than mutating the project policy.

Current source ownership:

| Responsibility | Primary files/modules |
| --- | --- |
| Editor command/result payloads | `Engine/modules/editor.core.ixx` |
| Toolbar context combobox and visible status | `Engine/src/editor/editor.application.cpp` |
| Editor state capture/restore for handoff | `Engine/src/editor/editor.application.cpp` |
| Session loop, live context discovery, handoff fallback | `Engine/src/epoch.engine_legacy.cpp` |
| Native detached context/window request and `WindowData::guiRoute` | `Engine/modules/context.multiplexer.ixx`, `Engine/modules/context.window.ixx`, `Engine/src/renderers/host/engine.context.host.*.cpp` |
| Reusable GUI layout state | `Engine/include/gui`, `Engine/src/epochgui`, `Engine/dep/EpochGui` |
| Engine GUI adapter/render/input bridge | `Engine/modules/gui.engine.ixx`, `Engine/src/epochgui/gui.engine.cpp` |

Context switching acceptance:

- the combobox label tracks the actual active backend, not stale desired state
- choosing the active backend logs that it was kept and does not create windows
- choosing another live backend captures and restores editor state first, then
  atomically transfers logical active-editor authority to that context and parks
  other editor sessions back to launcher/menu; failed restore keeps the old
  authority
- choosing an available backend with no live editor context uses the serialized
  replacement transaction on the Windows parent host: retire the old active
  backend, create the exact target, restore state, and require
  backend plus restored-frame evidence before claiming success
- an explicit backend request must fail closed when that backend is unavailable;
  it must not silently substitute the priority/default backend
- request evidence is staged: `OpenDetachedContextWindow == true` means a native
  request was posted or accepted by the host, not that the window was created,
  entered the session loop, restored editor state, or presented a valid frame
- successful handoff claims require the later evidence state: window/context
  created, session entered, snapshot restored, and focus/present path live
- snapshot capture or restore failure must log visibly and must not be reported
  as a complete context switch
- choosing a backend that cannot be created fails closed with visible log/status
  evidence
- the launcher has no `Switch Context` command. Launch Settings selects the
  target live context, and opening an editor workspace promotes it without
  spawning another editor shell
- no context switch persists across full engine restarts unless a future
  profile setting explicitly owns that policy
- no switch path may fake success by only changing labels

Current platform truth:

| Host | Missing-live-target behavior from combobox | Notes |
| --- | --- | --- |
| Windows desktop editor | Live promotion plus serialized replacement | Live diagnostic targets are promoted only after snapshot restore; missing compiled targets use the same-parent replacement transaction and restored-frame evidence. |
| Linux/WSL | Fail closed today | Single-context OpenGL remains the default proof path. |
| Mobile/console/headless | Excluded unless a product host implements it | These targets should hide or reject native popout/context-create commands while keeping portable `EpochGui` controls available. |

Background context selection is a separate future system. It should passively
measure normal single-context editor sessions and recommend the best default
backend based on stability, frame pacing, input latency, memory pressure,
feature support, and user-visible evidence. It must not use parented
multicontext diagnostic grids as scoring data, because simultaneous panes
distort FPS, timing, memory, upload contention, and input ownership. Mixed
backend grids remain comparison/diagnostic tools, not runtime truth for choosing
the editor's default context.

Floating/routed GUI acceptance:

- route ids are explicit strings such as `floating.gui`, not overloaded window
  titles
- a routed GUI window runs `editor_run_context_panel(ctx, route)` and draws only
  the requested panel content
- `floating.gui` is a single GUI host proof with its own title, size, input
  capture, close path, and status evidence
- current editor pane popouts start from pane title-bar drag/release gestures
  and use concrete pane routes such as `pane.outliner`, `pane.inspector`,
  `pane.console`, and `pane.ai_chat`; native `floating.gui` remains optional
  host infrastructure, not required for games, mobile apps, console apps, or
  headless tools
- route windows are optional desktop editor/tool features. If a product target
  excludes native popouts, the command should be absent or report unsupported,
  not create hidden shells
- active-editor authority is logical and independent of physical context creation
  order, parent-grid side, or dock state. Successful launcher selection and
  editor switching transfer that authority transactionally. Every physical
  renderer context may undock/redock, and routed panel windows retain the same
  capability through the EpochGui action model
- routed pane popouts keep docking guides in their own context-local coordinate
  space; they may return to logical tab stacks or preserve their pane-owned
  native context through an explicit physical-context target
- detached full renderer contexts publish only parent topology and cursor data;
  EpochGui centers left/right guides in the actual parent destination previews,
  and the parent host paints the matching placement ghost. Child renderers do
  not paint a duplicate local full-context overlay
- drawing, hover testing, and release selection consume one EpochGui guide
  layout. SDL3, SFML3, Raylib3, Vulkan, OpenGL, DirectX, and Software retain
  their existing native lifetime, reparent, owner-thread, and frame-order paths

When this area is split across agents, keep file ownership disjoint: one agent
may work on editor UI/status, another on session/window host code, another on
`EpochGui` primitives/build metadata, and another on docs/build evidence. Do
not run two workers against `editor.application.cpp` or `epoch.engine_legacy.cpp` simultaneously unless
the write ranges are explicitly isolated.

## Naming and structure direction

- legacy `aengine*` naming and older catch-all labels such as `multiplexer`
  should be treated as transitional debt, not as the final public structure
- source filenames should move toward `engine.*`, `epoch.*`, or
  subsystem-specific ownership in small tested batches.
- current completed filename batch: the former `aengine*` and `aeditor*`
  headers/modules/source files have been moved to `engine*`, `editor*`, or
  subsystem-owned paths in CMake and MSBuild. Keep `a2048like` as the explicit
  module-name exception because numeric-leading modules are invalid.
- when a subsystem is touched, file names, module names, and exported surfaces
  should move toward consistent professional ownership instead of growing more
  orphan naming
- `source_shape_audit.md` is the current guard for config/header/module/backend
  cleanup. Use it before touching compatibility headers, backend adapters, Perf
  Manager integration, or backend file splits.

## Time-system spine

- Epoch is time-based in the simulation sense, not in an extra-dimensions sense
- the first shared time spine lives under `core.time`
- the first milestone is:
  - fixed-step accumulation
  - pause / resume
  - time scaling
  - single-step
  - shared stats for editor/runtime/systems visibility
  - step-budget and frame-cap pacing visibility
- scene play, scripting, pacing, timeline, and replay behavior should route
  through that shared clock ownership instead of inventing parallel timing
  systems
- `System Info` remains the diagnostics surface for renderer/backend/context
  graphs, support policy, and live system lists. It no longer owns pacing
  buttons or frame-step controls.
- `Video` is the dedicated 4D/time-based workspace. It reads the
  shared `core.time` stats, exposes manual/interval/frame/timeline-key
  checkpoint modes, owns pacing/playhead controls, and presents configurable
  streaming-save status beside the editor scene flow. When no media source is
  admitted, it shows an explicit no-source/no-pixels state and disables media
  transport; it never substitutes scene geometry or placeholder frames.
- `media.timeline_preview` owns project-relative source identity, lowercase
  SHA-256, provider revision, dimensions, pixel aspect, rotation, frame timing,
  monotonic revision admission, contain-fit geometry, seek/play/pause/stop, and
  loop behavior. Its current presentation levels are `no_source` and
  `metadata_only`. Even valid admitted metadata cannot produce pixels until a
  separately verified decoder-frame provider supplies them.
- scene-backed editor workspaces reserve a bottom Video Timeline strip when
  there is enough room. That strip shrinks the scene viewport instead of
  letting 3D/2D rendering draw behind timeline controls or timeline graph
  chrome. Its slider scrubs the shared time spine and pauses playback; Play
  resumes the shared clock, and step controls advance the same playhead consumed
  by the full Timeline workspace. Timeline data and scene time therefore cannot
  drift into separate per-window clocks. Playback anchors to the current simulation
  timestamp when synchronization begins and advances only by subsequent elapsed
  deltas; absolute engine uptime must never clamp a newly played clip to its end.
- Epoch's 4D direction means timing is not a side panel: timeline authoring,
  streaming-save cadence, checkpoint retention, replay keys, package preview
  playback, and future deterministic simulation review all route through the
  shared time spine before they become generated-project behavior.
- `timeline.system` is the first explicit timeline data model. It owns editor
  tracks, keyed events, playhead state, scrub helpers, recording-gate state, and
  conversion into scene timeline keys so the UI can grow around engine data
  instead of ad hoc status buttons.
- Timeline view metrics now live in `timeline.system` too: visible range,
  seconds-per-pixel, playhead X, visible key count, and per-track event summaries
  are data outputs that the editor can render as lanes once the GUI library has
  proper timeline controls.
- Timeline lane layout is also engine data. Tracks produce lane rectangles and
  keyed events produce marker positions inside the visible range, so the editor
  can grow selectable/draggable 4D timeline lanes from validated data instead of
  hardcoded drawing.
- `saveload.system` remains the contract layer for streaming timeline
  checkpoints: it defines cadence, checkpoint labels, timeline keys, packages,
  and approval-gated writers. Active project `.epoch` scenes now use the
  separate canonical v2 `scene.persistence` lane described above. Streaming
  replay/retention is still gated and must not be inferred from active-scene
  Save/Build/Run persistence.
- Streaming-save profiles are descriptor-backed engine data, not loose UI
  switches. `saveload.system` owns stable profile IDs, labels, summaries,
  activation defaults, retention caps, and included-data flags for manual
  review, 15-second editor streams, 120-frame editor streams, and timeline-keyed
  streams. Checkpoint records carry retention, output path, included-data flags,
  scene payload byte counts, and timeline-key counts so build-safe tests can
  verify evidence before any runtime writer/restore path is promoted.
- Streaming-checkpoint packages now bind the checkpoint record, deterministic
  scene payload, manifest line, and payload hash. This gives the writer/restore
  path an auditable package shape before real disk writes, rolling cleanup, or
  editor/runtime replay are claimed complete.
- Checkpoint write plans are now explicit but non-writing. They stage the
  snapshot path, serialized scene payload path, manifest path, and manifest line
  under the configured streaming-save root so Video can expose the
  future write layout before a human-approved disk writer/restore gate lands.
- Streaming-save cadence plans make the next checkpoint decision visible: due
  now, waiting for a manual/timeline-key action, or scheduled by frame/seconds
  from shared `core.time` stats. Video displays that next-capture
  summary while disk writing remains gated.
- Checkpoint restore plans mirror the staged write layout for the future replay
  gate. They name the checkpoint label, snapshot path, scene payload path, and
  manifest path without reading disk or mutating the live scene.
- The checkpoint writer path exists but remains approval-gated. It writes scene
  payload, snapshot metadata, and manifest entries only when an explicit human
  approval object is supplied; the build-safe contract lane verifies the blocked
  gate and metadata payload without touching disk.
- Checkpoint retention plans are non-destructive. They evaluate staged
  checkpoint records against the active rolling-retention cap and report retained
  versus prune-candidate checkpoints for Video, but no cleanup or
  delete operation is enabled until a separate human-approved disk gate exists.
- Streaming-save profile-change plans are review-first. They let the Video
  Editor show what an interval/frame/manual/keyed profile transition would
  change before any dropdown mutates live save configuration.
- `input.engine` is the shared input profile spine. The default editor profile
  now names camera reset-to-center, frame selection, clipboard copy/paste,
  right-click context menu, play-in-editor, timeline play/step, and package
  install actions with modifier-aware key bindings and mouse bindings. GUI
  surfaces should consume those named actions instead of hardcoding per-window
  shortcuts. `project.input_profile` is the separate project/runtime contract:
  generated Game2D projects persist canonical action and keyboard/controller
  bindings under `Assets/Config`, compile reproducible Library artifacts, and
  evaluate fixed-point dead zones without coupling gameplay to editor shortcuts.
  Generated Game shells and the registered Platformer profile always materialize
  the same canonical source and compiled artifact. Tool and engine-development
  sandboxes remain input-profile-free by default; only the explicit
  `EditorProjectInputProvision::explicitly_enabled` creation route opts those
  project kinds into runtime input ownership.
  A process-owned SDL3 provider publishes generation-checked controller state
  once per engine frame. ProjectPlayScene maps that snapshot through the
  compiled project bindings without replaying press edges; physical-device and
  native selector interaction remain operator proof gates.
- Generated Game2D projects also declare one canonical sprite-animation source
  and compiled Library artifact. Authoring builds regenerate or materialize the
  default idle/run/rise/fall sheet from authenticated map texture metadata;
  game-only builds restore the compiled artifact without requiring animation UI.
  ProjectPlayScene samples fixed-tick animation state and publishes logical
  source rectangles and facing through the existing Canvas2D resource closure.
- `RunContextSessionLoop` owns one `audio.playback_runtime` for the process.
  Renderer/context replacement does not close or duplicate its optional SDL3
  device. ProjectPlayScene acquires one generation-checked session, routes actor
  jump/landing/pause/reset events, and releases only that session on stop.
  Unavailable devices and queue backpressure remain visible diagnostics and are
  nonfatal to project simulation. `project.audio_profile` owns canonical source
  plus the immutable, digest-verified
  `Library/Audio/project_audio.epochaudioc` artifact for decoded PCM clips,
  buses, cue semantics, volume/mute, loop/autoplay, and actor event bindings.
  Editor edits and Build/Run preflight decode valid source before saving or
  refreshing that artifact. A game-only runtime may restore it without
  authoring source/WAV files; malformed present source fails closed rather than
  selecting stale derived output. Project and Assets expose one Project Audio
  editor with visible artifact freshness. Physical-device ear proof and
  repeated Play/Stop proof remain follow-up.
- The non-GUI engine contract self-test now exercises the Forest Factory,
  package registry, streaming-save package, input profile, and scene snapshot
  serializer/parser contracts before the heavier project-profile and OS-AI
  validation gates. New timeline, package, model, or input contracts should join
  that lane before being exposed as generated-project behavior.

## Hardware support strategy

- default automatic compatibility target:
  - 6-core desktop CPU class
  - GTX 1660 Ti-era GPU class
  - modern Linux laptops/desktops
- baseline tier:
  broad editor/runtime reach with stable dependencies
- standard tier:
  stronger GPU/backends and fuller renderer/tooling paths
- extended tier:
  heavier libs/features enabled per project by the game developer

Epoch should prefer broad automatic support with explicit opt-in for heavier
features over forcing every integration on every machine.

## Renderer direction

- prioritize the GPU-driven baseline first:
  visibility -> surface -> lighting -> temporal -> reconstruction -> present
- use frame/task graph guidance and temporal history/reconstruction as the main
  planning surface
- keep heavier paths such as ray tracing, path tracing, mesh shaders, virtual
  shadowing, sparse-resource-heavy flows, and similar techniques behind
  Standard/Extended tiers or explicit project opt-in
- keep backend convergence visible in the System Info workspace so OpenGL, Vulkan,
  DirectX, the software fallback, SDL, SFML, Raylib, and future D3D12 do not drift
  without tooling feedback
- OpenGL is the first Tier 1 reference; Software is the Tier 0 CPU fallback and
  deterministic comparison path. SDL3, SFML3, Raylib3, Vulkan, and Direct3D 11
  consume the same scene, Canvas2D, texture, material, and GUI contracts.
- Preserve scene-first / GUI-over composition. The active backend draws the
  scene, EpochGui replays tool windows, command menus, and modal layers above
  it, and the host presents exactly once.
- Normal editor context selection transfers logical active-editor authority, or
  replaces a missing target as one serialized capture, retire, create, restore,
  and acknowledge transaction.
  Multicontext remains diagnostic and must not be used for runtime scoring.
- Native context hosts may publish `host FPS` and lifecycle diagnostics during
  local runs. These are useful for finding duplicated presentation, blocked
  hosts, or failed teardown, but they are not visual-parity evidence.
- The seven-backend acceptance gate, canonical scene, pixel policy, and
  resource-soak requirements live in `renderer_regression_smoke_plan.md`.
  Current capability truth lives in `renderer_feature_matrix.md`; version
  chronology lives in `Changes/changelog.txt`.
- When `EPOCH_SINGLE_PARENT=0`, the launch config must force standalone
  top-level contexts even if CLI defaults still prefer parented mode. This mode
  is used to isolate resize/flicker from the single-parent dock host, so any
  parent-window creation in that build is a regression.

## Logging

- the engine logger is the preferred way to tag subsystem output
- keep backend-specific noise behind subsystem names so multi-context runs stay
  readable
- repeated identical startup/error lines should collapse instead of flooding the
  console
- optimized GUI builds write through checked platform console handles and RAII
  file streams; switching must not depend on unchecked CRT `stdout` or raw
  `FILE*` lifetime
- one tagged line per repeated condition is the goal, with subsystem/source
  context still preserved

## Editor viewport controls

- LMB selects scene objects; GUI, menu, title-bar, and splitter capture prevents
  that input from leaking into camera navigation
- Alt+LMB orbits perspective and free-orthographic views
- MMB pans in the current view plane without requiring a modifier
- Alt+RMB dollies without depending on empty grid space
- RMB captures fly/look; WASD translates, Q/E descends/ascends, Shift is fast,
  and Ctrl is precision
- the wheel zooms while navigating normally and adjusts fly speed while RMB fly
  is active
- F focuses the selected stable scene object; Home resets the active view
- Perspective, free Orthographic, Front, Back, Left, Right, Top, and Bottom keep
  independent framing behind one stable logical view identity
- project/runtime camera policy remains separate from the editor navigation
  view. Mobile, console, and authored project-camera input maps remain future
  bindings over this same contract

## AI Runtime, MCP, And Guarded Development

The current source workbench and campaign UI are real editor adapters over
guarded engine contracts, not alternate authority. `73c86889` restores
project-owned file selection, UTF-8 editing, scrolling, save/reload, and bounded
screen use. `94c7357c` projects the durable queue/scheduler/supervisor state into
an operational campaign surface, while `60ce0032` exposes sealed patch review
and exact approval/refusal evidence. World Outliner hierarchy is owned by
`6deaf036`; responsive workspace routing is owned by `4cedf62b`.

The host-callable MCP supervisor adapter is implemented locally but its two
modules and three implementation/contract files are not registered, built, or
published because shared build-metadata approval is still pending. It opens no
transport and grants no filesystem, model, apply, promotion, or release
authority. The source-iteration worker is blocked on missing prerequisites and
has no committed checkpoint; the `disposable_sandbox` design is still pending.
Neither lane is presented as working editor behavior.

Epoch runs an operator-selected external model through a local
OpenAI-compatible endpoint or a directly selected `llama-cli`/GGUF pair. It
does not own or train an internal LLM, start a server, bind a port, or activate a
discovered model without operator choice.

Optional voice interaction follows the same authority path. A desktop authoring
profile may use operator-selected local STT/TTS only after per-session
microphone consent; submitted transcripts remain visible proposals and cannot
bypass model confirmation, plan review, or operator approval. No hidden
listener/server or default raw-audio retention is permitted, and game/headless
profiles can compile the voice host out. This is a workflow requirement, not a
claim that voice capture or synthesis is currently implemented; the normative
contract is in `os_ai_tooling_and_evidence_policy.md`.

`ai.mcp` owns bounded provider-independent tool messages.
`ai.development_guard` separately owns immutable proposal identity, review,
operator approval, bounded lifetimes, cancellation, and evidence. Its private
`ExecutionPermit` is issued by the guard and claimed once before work:

1. register complete typed operations, exact content transitions, and SHA-256
   digest;
2. review that exact digest;
3. obtain separate allowlisted operator approval when policy requires it;
4. issue and claim one expiring private capability;
5. execute only the approved operation and return verified terminal evidence.

The non-GUI self-iteration stack now carries this boundary end to end without
turning the model transport into an executor. A strict project profile selects
disabled, local, shared, or external MCP inference; the durable campaign and
orchestrator restore exact generations and evidence digests; the MCP bridge
returns mutating work as host-pending receipts; the patch adapter applies only a
reviewed text bundle inside the disposable sandbox; and the validation adapter
requests the seven trusted local build stages through injected host tasks. No
adapter launches a shell, editor, renderer, child, or network connection by
itself.

Successful validation aggregates existing `epoch.build_validation` receipts
into deterministic Site-readable JSON bound to the exact candidate, bundle,
authority, source commit/tree, toolchain, and configuration. This is admission
evidence, not publication authority: upload and release permissions are always
false, and Site ingestion remains a separate explicitly authorized operation.
Generated-project self-iteration uses the same mechanics only when its project
profile enables it; engine-source authority is never inherited by that project.

`editor.ai_development_controller` maps production calls to trusted monotonic
time, serializes execution entry, and rejects caller-driven backdating. Before a
source request reaches a model, the trusted host tokenizes the operator objective,
gives an exact canonical objective path precedence, ranks existing files beneath
the approved read-only source roots, and presents a bounded candidate list without
reading or transmitting file contents. Only the visible
`Share Curated Context` action reads those unchanged candidates and sends bounded
full-file or objective-centered excerpt evidence to the displayed selected
endpoint. The same action opens the reviewed files in the large source workspace
instead of leaving the 3D scene dominant. `Project Scripts` returns to the normal
script editor and `3D Scene` restores the scene surface. EpochGui decodes UTF-8
for layout, drawing, hit testing, caret motion, and deletion; unsupported
box/block decorations use a readable ASCII fallback without changing source
bytes.
Model-originated path requests are rejected; models cannot browse, invent, or
expand the candidate set. The controller preserves exact raw model reply bytes
and accepts source intent only through the strict bounded
`EPOCH_SOURCE_PATCH_PROPOSAL_V1` exact-block codec. The trusted host owns
canonical workspace roots, full preimages/postimages, hashes, risk, actor
identity, approval, and permit issuance. Direct local source inference runs
below normal priority with half logical CPUs, matching batch threads, and
`--gpu-layers 0`; ordinary chat retains its configured GPU acceleration.
Build/run evidence comes from registered host paths. Source completion cannot be
The direct transport selects one line-framed structured header and matching
terminator before strict decoding, and each completed generation is ingested only
once. The prompt's packet sample uses the first exact reviewed evidence path;
explicit two-literal replacements use exact unique reviewed search and requested
replacement bytes. Packet rejection queues no more than two complete
host-diagnosed retries over the same evidence.

submitted through the public completion path; it must come from
`ai.development_executor`.

The source executor verifies operation IDs, canonical relative paths, approved
preimages/postimages, and exact replacement bytes. It exclusively creates and
flushes same-directory temporaries, revalidates parent chains and destinations
before commit, verifies committed bytes, and records rollback evidence. It owns
no Git, release, updater, package, network, or unrestricted shell authority.

This remains bounded process-local protection, not complete OS transaction
semantics. Hostile external-writer exclusion, directory crash
journaling/durability, and complete ACL/xattr/alternate-stream/ownership
preservation remain future hardening. Strict bounded model source-proposal
parsing is complete; generic non-source build/run/test/capture multi-tool
dispatch remains incomplete.

Project creation, document/script changes, Save, Build, external Run, test,
capture, and diagnostics use the existing project and evidence contracts.
Normal chat is not captured automatically; explicit traces remain bounded
evidence rather than training data. Source v0.89.28 claims these source
contracts, not GUI eye proof. See
`os_ai_tooling_and_evidence_policy.md` for the normative authority contract.

## Procedural/time-node direction

- the later procedural authoring phase should cover SpeedTree-like modular
  vegetation/world generation plus time-node authoring
- treat O2L as a later integration source for that phase when the code is in
  the workspace
- do not block the current time-system or Systems milestones on O2L being
  present now

## Editor Shell Direction

The bottom `Console Dock` is a temporary evidence/status strip, not the final
editor-window system. It hosts one reusable `Output` tool pane with persistent
`All`, `Project`, `Assets`, `AI Output`, and `Systems` filters. The
categories filter logs, build evidence, status, model inventory, and visual
feedback without creating duplicate dock routes or becoming the main command
surface. `AI Chat` remains a separate movable tool. Action controls that start
AI plan/build/harness passes belong in the Inspector or owning central workspace,
not in Output filters.

The real editor shell target is a set of independently focusable/dockable
editor frames and views:

- Scene/Game editor view for project objects, play state, and runtime scene
  editing
- Software/Tool editor view for generated apps/tools and code-oriented project
  work
- Engine Development Sandbox view for guarded source-development passes,
  reviewed proposals,
  builder/verifier gates, and human approvals
- AI Visualizer view for model state, proposal/evidence replay, scene-state
  diffs, and
  future 3D weight/model views
- Project Hub view for project launch/update/context selection. The
  compatibility id/path may still be `projectlauncher` /
  `Projects/ProjectLauncher` until a safe generated-artifact migration lands.
- Build/Output view for logs, diagnostics, and release/build evidence
- Asset Browser view for decoded image/model thumbnails, active project assets,
  and import/organization actions. The current `Assets` tab is only the first
  file-type-card version of that view.

These views should be backed by reusable GUI controls and custom UI powered by
an automated texture-atlas system, not by hardcoded editor-only tab strips that
cannot scale.

All editor-shell work should be production-minded. A feature can be incomplete
or acceptance-gated, but the code that lands must still be owned, buildable,
usable, and honest about its limits. Do not add fake controls, duplicate command
paths, placeholder windows, or temporary UI experiments unless they preserve a
working path and are documented with the next promotion/removal condition.

Current editor-shell gaps:

- the World Outliner needs stronger grouping, clipping, and resizable columns;
  the current compact button rows are a first cleanup pass, not the final
  desktop-grade control
- the engine GUI now has reusable `tab_bar`, `scroll_text_panel`, and modal
  focus overlay paths; selectable text is currently row-level and must grow into
  true text-range selection/copy support
- `tab_bar` is a real tab primitive now, not a segmented button alias. Future
  work should keep tabs visually connected to their content pane and reserve
  ordinary buttons for actions.
- the Asset Manager now has bounded filters, a virtualized thumbnail grid,
  stable selection, contextual Open/Details/Copy actions, and a full-width
  source editor. Rename/move workflows, richer import actions, multi-selection,
  and column/list alternatives remain future work.
- launcher/editor settings buttons should open modal windows with concrete
  backend, display, package, project, and AI safety controls. Modal close
  affordances should be normal top-right X controls with overlay-priority z-order.
- Project Hub should present a basic project/game/software launcher mockup, not
  a miniature duplicate of the editor shell.
- World Outliner rows should show human project/entity names, type, and useful
  grouping instead of implementation-ish labels.
- the AI Visualizer should eventually expose proposal/evidence graphs, scene-state diffs,
  and sampled weight/memory terrain views; it must not attempt to draw billions
  of raw parameters directly.
- the GUI still needs broader domain context-menu coverage, resize cursors,
  resize handles, column controls, arbitrary compatible stack creation, and
  user tab reordering
  for desktop editor/tool builds; product targets that do not support native
  floating hosts should exclude those routes instead of carrying hidden shells
- editor theme selection remains user-facing: `System
  Light/Dark` follows the platform app-theme preference when available, while
  `Light` and `Dark` are manual choices. Classic Launcher, Midnight Blue, Ember
  Forge, Forest Terminal, and Aurora Steel are optional named palettes. Light
  uses dark glyphs over a true light palette, and rounded controls default on.
- pane visibility, Left/Right/Bottom Left/Bottom Right placement, active tabs,
  the bottom-column split ratio, theme, and rounded-control preference persist
  per editor application in versioned user configuration outside project and
  release data. Output and AI Chat default to the two bottom cells but remain
  ordinary movable panes; either cell may collapse when empty. Invalid state
  falls back to that application's defaults.
- global UI scaling should behave like normal desktop software, with explicit
  user scale/font controls instead of one hardcoded pixel density
- separate editor windows/domains are still needed inside the application:
  project/game editor, software/tool editor, Engine Development Sandbox, and AI
  visualizer should be independently launchable/dockable surfaces
- the software/tool editor name does not mean the software renderer is a normal
  production backend. The software renderer's target role is safe launch,
  debug/error messages, capture diagnostics, and headless validation; Windows
  native rendering is now moving through the first DirectX/D3D11 slice, with
  D3D12 still future work.
- linked-context hosts are explicit presentations of exact tool routes such as
  Properties, Asset Browser, Script Browser, AI Chat, and Output. Project,
  Assets, AI Output, and Systems are filters within Output rather than floating
  routes of their own. Tool hosts are operator-opened, visible, closable, and
  logged. Native-window X and titlebar drag restore the route to a remembered
  tool group; floating content does not expose command-style Dock Back or Close
  Window controls.
- ordinary tool tabs dock into Left, Right, Bottom Left, or Bottom Right in-host
  groups. Physical contexts use separate upper-left and upper-right targets;
  bottom guide targets always retire a detached host into a logical tab stack.
  World/GUI/etc. document tabs and scene views remain outside this movement
  model. Dragging a tool tab or pane title shows MSVC-style target guides and a
  ghost of the proposed group or floating placement. External context-backed
  presentation remains explicit and does not clone canonical editor state.
  Window menu entries recover each exact tool and layout. Selection commits on
  the pressed tab before drag begins; movement activates the exact destination
  route and resolves a valid source fallback. Tabs retain readable label width
  while reserving one close affordance. Any external route owns input and
  overlay priority only inside its bounds and returns the same logical pane when
  hidden, closed, or redocked.
- routed-pane cleanup is deterministic. Close/redock detaches the route from the
  host. Context retirement, completed-thread joining, command-queue clearing,
  and native GL/DC/window release occur only when the route owns a dedicated
  context; a reused host remains alive. Full shutdown resets route maps, shared
  detached projection, and passive context scores
- guarded engine development needs visual state, not only console rows. The
  first visible surface is the AI loop card visualizer; later passes should add
  proposal/evidence replay,
  scene-state diff views, and a 3D model/weight visualization surface

## Multicontext proxy-shell behavior

- SDL3 and SFML3 use a visible `EpochChild` proxy shell inside the six-context
  parent rather than exposing the nested backend child directly as the grabbed
  dock target.
- The visible proxy shell is the thing that should undock: once dragged outside
  the parent, it must become a real top-level window, keep mouse/input control,
  and continue following the drag instead of freezing in place under the parent.
- SDL/SFML proxy shells must be moved back into dock slots by their posted
  proxy-host redock command. The parent grid may decide the slot, but it must
  not directly reparent or resize the nested SDL/SFML host/child pair while the
  backend-owned shell is redocking.
- A proxy redock command must not request another parent layout from inside the
  redock handler. Drag release or the parent layout pass owns the next layout
  request; re-requesting from the proxy handler can loop placement and recreate
  redock flicker/crash behavior.
- Focused six-pane parent validation is the current honest runtime gate for this
  path. The all-backends sequential harness still needs extra sequencing cleanup
  after the Raylib pass before it should outrank focused SDL/SFML evidence.

## Linux / WSL Runtime Shape

Linux and WSL do not use the Windows parented multicontext editor shell by
default. The current WSL-proven runtime path is a single OpenGL editor context.
Do not automatically fall back to Vulkan in WSL; Vulkan remains explicit
validation work on that lane until it is locally proven, and DirectX is
Windows-only.

Project-run backend selection on Linux/WSL currently exposes only the proven
OpenGL single-context choice in the editor. Other Linux backends can still be
compiled or launched as explicit validation work, but they should not be
presented as normal project-run choices until runtime proof exists.

Other renderer/tool outputs can still exist on Linux as project output choices,
but they should launch as explicit child processes from visible editor controls
instead of hidden parented contexts. Software rendering remains a safe-launch,
debug/error-message, capture-diagnostic, and headless fallback path; it is not a
normal peer renderer for the Linux editor shell.

## Generated project shell self-tests

Run these from the repository root after building the editor runtime:

```powershell
.\x64\Debug\EpochEditor.exe --editor-project-self-test sandbox
.\Projects\Sandbox\bin\windows\Debug\x64\EpochEngine.exe --project-self-test
.\x64\Debug\EpochEditor.exe --editor-project-self-test projectlauncher
.\Projects\ProjectLauncher\bin\windows\Debug\x64\ProjectLauncher.exe --project-self-test
```

The editor command materializes the selected shell, verifies
`project_format: epoch-project-v1`, `build_profile: epoch-runtime-static`, and
the versioned build inputs, production-loads and atomically saves/reopens its
canonical scene, builds the child, and runs its headless
`--project-self-test`. A direct child command remains useful for
isolating an already-built executable. Sandbox reports the engine-development
identity; ProjectLauncher reports its launcher-tool identity. A GUI project
external-run check should additionally launch its generated executable with the
same `--scene`, `--backend`, and standalone arguments emitted by the editor.

`--engine-validation-self-test` chains the registered project profile self-tests
and the AI evidence gate in one non-GUI pass. It is intentionally broader than a
single profile smoke, but it still does not replace manual eye testing for GUI
composition, command-menu z-order, renderer flicker, or dock behavior.

The engine-side command appends one explicit MCP-style tool trace and writes the
evidence path into project notes. It does not create hidden AI iteration/session state,
capture normal chat, promote source, or treat build output as training material.
Build logs, child self-test output, and canonical project paths remain ordinary
operator-review evidence.

## Troubleshooting checklist

- verify the expected backend/config macros are enabled
- launch Windows smoke tests from `x64/Debug/` or `x64/Release/`
- prefer engine-owned capture output over ad hoc desktop grabs
- treat black or invalid software captures as failed proof that needs
  investigation, not as a successful screenshot
- treat DirectX screenshots as valid Windows product-renderer proof only when
  the pane shows real editor preview content, Inspector, AI Chat, and normal GUI
  chrome from an asset-bearing output directory.
- keep Windows resources under `Engine/resource/`
- keep local compiled AI artifacts out of the repo
- when investigating backend issues, prefer backend-local fixes over broad
  multiplexer edits unless the shared layer is clearly proven at fault
