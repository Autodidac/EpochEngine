# Current Engine Architecture

## Snapshot

Epoch is now documented as a module-first engine with the active runtime living
under `Engine/modules/` and `Engine/src/`, while older compatibility/archive
surfaces have been retired and mapped into active replacements.

Current source version: `v0.84.40`

## Architecture highlights

- **Multi-context runtime**: the shared context layer and multiplexer coordinate
  backend-owned windows, command queues, and render-thread work.
- **Backend stack**: OpenGL, DirectX/D3D11, SDL, Raylib, SFML, software fallback,
  and noop/headless are all represented in the active engine tree; Vulkan
  remains experimental.
- **Custom UI on automated texture/atlas plumbing**: GUI layout, atlas upload,
  sprite submission, clipped panels, tab bars, scroll text, arbitrary scroll
  areas, dock visibility, first-pass splitter resize state, generated runtime
  surface atlases, and font/text rendering are engine-owned systems shared
  across the active render paths.
- **Launcher/editor split**: project and game entry now live in the launcher,
  while the editor uses a more traditional desktop-style menu flow.
- **Task graph + scripting**: reload and background work are funneled through
  task scheduling rather than ad hoc threaded entry points, and editor-triggered
  compiled scripts now run through an explicit host API instead of a loose
  filewatch-first loop.
- **Project browser + asset cards**: the editor now exposes project-local script
  stub creation, project/engine script selection, a shallow file/folder browser,
  and an `Assets` workspace with first-pass file-type thumbnail cards.
- **Mode-specific editor surfaces**: Scene/Game modes keep the 3D viewport,
  while Project, Assets, Self-Iteration Sandbox, and Systems now own central GUI
  surfaces so build, asset, AI, and systems controls are no longer packed only
  into the bottom console dock.
- **Generated project verification**: generated Sandbox and ProjectLauncher
  child builds now expose non-GUI self-tests, and the checked-in engine exposes
  `--editor-project-self-test <id>`, so project shells can be materialized,
  built, and verified without pretending a GUI launch happened.
- **Generated project build gate**: editor Run/build requests are serialized
  while generated child projects still share the checked-in engine
  `StaticLib1` module/PDB output surface. A second request should fail visibly
  instead of corrupting the active build.
- **Executable-root runtime resolution**: fonts, scripts, shaders, captures,
  workspace paths, and updater scratch roots are being normalized around one
  executable-root/runtime-root resolver instead of cwd guesses.
- **Three-piece engine AI path**: the intended internal pieces are EpochBot,
  the local MCP/control/tool harness layer, and an offline/injectable backup
  LLM path. External local OpenAI-compatible LLMs remain selected
  teacher/reviewer helpers for testing, curation, and acceleration.
- **Systems workspace direction**: central Systems and AI Sandbox surfaces now
  render engine-generated graph textures through the dedicated runtime-surface
  atlas instead of relying on text-only diagnostics or the bottom dock. Systems
  keeps render/frame flow, task/thread scheduling, and support-tier status in
  readable graph rows.
- **Renderer feature direction**: `renderer_feature_matrix.md` now tracks the
  imported OpenGL/Vulkan/Direct3D feature families, separates existing/partial
  engine coverage from missing renderer backlog work, and records DirectX/D3D11
  as an active first-pass Windows renderer while D3D12 remains future work.
- **OpenGL loader ownership**: CMake now uses `EPOCH_GLAD_PROVIDER` to select a
  single GLAD owner per target. `auto` prefers vcpkg `glad::glad`, then falls
  back to the checked-in loader; duplicate-loader masking with linker force
  flags is not part of the supported build shape.
- **Editor status strip**: the in-editor status line now reports source/build
  identity, thread capacity, active renderer, and zoom without repeating
  launcher/editor mode labels.
- **Voxel/planetary package direction**:
  `voxel_planetary_package_track.md` records the long-horizon voxel-first world
  spine and keeps operator prototypes as package-gated research inputs instead
  of direct mainline source imports.
- **Migration map**: formerly archived compatibility surfaces are now either
  preserved in active modules or called out explicitly in
  `Engine/docs/engine/legacy_feature_map.md`.

## Current strengths

- Strong module-first organization for the active engine.
- Safer runtime path ownership now that more subsystems resolve assets and
  support files from the executable/runtime root instead of the working
  directory.
- Broad backend coverage for desktop experimentation and tooling.
- Clearer support target discipline around 6-core / 1660 Ti-era desktops and
  modern Linux laptops as the default automatic compatibility baseline.
- Good separation between active code and archived compatibility material after
  moving the legacy tree under `Engine/`.
- Startup and shutdown behavior are getting more disciplined as backend-local
  diagnostics and hot-path logging are trimmed back out of the render loop.
- SDL, Software fallback, and DirectX now share enough editor preview-grid
  geometry/palette behavior to support scene-view smoke tests, while DirectX
  also proves the Windows-native renderer track with D3D11 preview rendering.
- Parent-window shutdown now behaves more like a real engine host lifecycle:
  docked children are marked for close and the session exits instead of leaving
  a dead console/process behind.
- The parented docking layout now stays intact during shutdown instead of
  undocking backend panes as the host window closes.
- SDL3 and SFML3 are back on the real promoted-proxy path under the six-context
  Win32 parent: the visible shell is what detaches, the backend child stays
  nested inside that shell, and the detached window can escape the parent as a
  true top-level host instead of freezing under the parent bounds.
- Live pane management is back in a better place too: undocked windows keep the
  information they need to redock cleanly, and only still-docked panes are
  considered part of the parent grid layout.
- Manual pane management is now usable again without modifier keys: a dedicated
  drag strip provides left-drag docking while leaving the rest of the pane free
  for normal backend/editor interaction.
- Parent-close ownership now behaves like a real multi-window host lifecycle:
  undocked promoted backend windows can outlive the parent host, but once they
  are redocked they return to normal parent-owned shutdown behavior.
- The last surviving undocked pane now shuts the process down cleanly when it
  closes, even if that backend window is owned by a render thread.
- The editor shell now maps more cleanly to production-tool expectations:
  launcher responsibilities are separated from editor responsibilities, and the
  updater is confirmation-gated before it can run from the UI.
- The source-update path is back to behaving like an engine updater instead of
  a passive download tool: it now restores manifest dependencies with `vcpkg`,
  rebuilds from the downloaded snapshot, and replaces the running runtime from
  the rebuilt output.
- Update/install policy is platform-aware: Windows uses `.zip` runtime assets,
  Linux/WSL uses `.tar.gz` runtime assets, and source installs fall back to a
  source snapshot rebuild only after packaged-runtime parity is reached or no
  newer packaged asset is available.
- Console dock text, buttons, project files, scripts, active assets, and
  generated project self-tests now have first-pass GUI/build affordances for
  visible Sandbox iteration evidence instead of relying on command-line-only
  inspection.
- Game project shells can now expose an `engine_arcade` local runtime-mini
  package as project assets and a script bridge while the actual mini-runtime
  implementations remain kernel-engine modules. The Package Manager modal is
  the first command-menu surface for these local packages; future downloadable
  source packages must stay human-gated through updater-style build paths.
- Server-capable runtime work follows the same package boundary. Shared
  network/runtime contracts may exist as inert engine capabilities, but
  optional authoritative dedicated headless server support, client
  listen/nondedicated hosting, and future competitive client-predicted paths
  must stay explicit project/package choices. Software projects, single-player
  games, and minimal generated clones should not inherit server/listener code,
  port binding, or network attack surface by default.
- `Autodidac/EpochEngineExtensions` is the intended source home for bulky
  optional package implementations. EpochEngine mainline should carry the
  descriptors, package manager/updater gates, cache paths, and stable API
  boundaries, not imported extension source trees or generated server payloads.
- World Outliner, Inspector, Console Dock, and AI Chat can now be hidden,
  reopened, reset, and resized with first-pass splitters. The bottom
  Console/AI Chat split is drag-only now; the old sizing button strip has been
  removed. True borderless linked-context panel popouts remain the next
  context-host step.
- The central editor area now has a tabbed `Editor Workbench` layer for
  Perspective, Game/2D, Assets, Project, and AI Sandbox. Systems opens as a
  dedicated Systems-only surface so renderer/task/support graphs are not nested
  behind another cross-surface submenu.
- OpenGL editor composition renders the scissored scene preview first and then
  drains GUI commands, keeping AI Chat, Inspector, dropdowns, and scene titles
  above 3D/2D content.
- Opening AI Sandbox from the toolbar, bottom AI dock tab, or Window menu now
  restores Inspector and AI Chat together before activating the sandbox, so the
  control surface is not hidden behind stale dock visibility state.
- `EPOCH_SINGLE_PARENT=0` is a true standalone-context mode: launch config
  resolution must not create the parent/dock host path when that compile flag
  is off. This keeps resize/flicker isolation honest while the parent host is
  being debugged.
- The main engine implementation filenames have started moving away from
  `aengine*.cpp` toward `engine.*.cpp` source paths. Module names/imports remain
  compatibility-stable until each follow-up batch can be built and tested.
- Splitters now render through dedicated neutral GUI chrome instead of fake
  blank buttons, and the centered Run action rebuilds generated child projects
  before launch so stale ProjectLauncher output is not mistaken for a fresh
  build.
- Game/2D mode now creates/selects an editor-only upright `Canvas2D` plane and
  switches to a locked front-facing Canvas2D camera. Projection selection is
  owned by `render.preview_grid`, so editor picking and the OpenGL, DirectX,
  Raylib, SDL, SFML, Vulkan, and software-preview paths share the same Canvas2D
  orthographic framing while tile/layer tooling is still being built.
- Backend presentation is more visually coherent now that the active renderer
  base colors are being pulled toward the same darker Vulkan-style baseline,
  and SFML’s shared preview path is clipped back to the intended scene view.

## Current cautions

- Vulkan is present but should still be treated as a migration/integration path,
  not the default renderer, even though the editor palette, editor grid preview,
  and GUI presentation have been brought closer to the OpenGL baseline.
- DirectX/D3D11 is active first-pass Windows proof, not yet a complete renderer
  resource API. D3D12 remains reserved scaffolding until a separate
  context/device/shader/resource path is promoted and validated.
- A few minor archival and compatibility surfaces still exist and should be
  changed carefully.
- Backend fixes are usually safest when applied locally to the affected backend
  instead of globally in the multiplexer.
- Raylib parent/docking work should continue to respect GLFW/raylib ownership of
  the native GL context instead of swapping in fresh Win32 DC handles after
  initialization.
- Raylib dock/redock changes must stay owner-thread queued. Direct parent-host
  Win32 mutation of the GLFW/Raylib child is a known crash-risk pattern.
- Linux Clang build/headless validation is current for `v0.84.35`; refreshed
  Linux/WSLg or native Linux visual proof remains a follow-up gate before README
  screenshots are replaced.
- The `Assets` workspace currently uses file-type cards, not decoded image/model
  preview thumbnails. Full thumbnail decoding/render previews remain next-pass
  GUI/asset-browser work.

## Recommended priorities

1. Keep renderer/backend smoke coverage improving, especially around startup,
   resize, docking, and shutdown behavior.
2. Continue reducing duplicated compatibility naming in public surfaces while
   preserving compile compatibility where needed.
3. Promote only active, tested functionality into the main engine surface and
   keep migration notes honest when a retired system still needs a modern home.
