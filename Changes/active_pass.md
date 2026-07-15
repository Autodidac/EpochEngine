# Active Pass

## Gate

Backend-native sampled render-to-texture for OpenGL-derived contexts.

## Sealed Baseline

The published `v0.87.69` runtime release and updater are accepted and frozen.
This pass must not edit updater behavior, updater UI, worker/handoff scripts,
packaging, release metadata, tags, or release assets unless the operator
explicitly reopens that gate. Development-source version metadata may advance
without changing the packaged baseline.

## Why This Gate Matters

This is the first vertical proof of Epoch's renderer-resource spine. It
validates shared descriptors, backend-native allocation, render-target
ownership, material texture binding, graph pass execution, and the Engine
Arcade screen path without turning the arcade package into the whole mission.

## Current Spine Goal

Build a truthful renderer spine where every modern OpenGL feature lands as a
reusable engine feature family, proves itself in OpenGL first, reports
capability truth per backend, and leaves Vulkan/DirectX with clean equivalent
contracts instead of drift.

## Current Evidence

- OpenGL owns the first real native sampled-RTT hook factory for FBO/color
  texture/depth renderbuffer/sampler allocation.
- The engine contract harness now imports that real hook factory, installs it
  on the OpenGL-family device, and verifies the shared `engine_arcade.screen`
  descriptor/handle/work-order path can allocate and destroy records without
  claiming live GPU allocation when no GL context is registered.
- `render.graph` now rejects `MaterialTextureSlot::render_surface` bindings
  unless the referenced texture is owned by a sampled render-texture asset with
  a sampler. Plain texture handles no longer count as arcade/runtime screen
  surfaces.
- Sampled render-texture samplers are now first-class graph resources. The graph
  maps `engine_arcade.screen` color texture, sampler, and render target handles
  together, and skips ordinary sampler teardown because the RTT asset owns native
  destruction.
- `render_surface` material slots now carry an explicit sampler resource. The
  graph binds the sampled RTT only when that sampler matches the owning
  render-texture asset sampler, and the harness proves mismatched samplers are
  rejected instead of silently sampling the wrong surface state.
- `render.arcade` now makes `engine_arcade.screen` a real two-pass proof shape:
  the populate pass targets the sampled render texture with a tiny scene model,
  and the cabinet pass samples that render surface through a material slot.
- Engine Arcade now stages an actual cabinet preview assembly in both the
  editor package preview and generated game-shell scene files instead of a
  single placeholder box: base/body/control deck plus screen/marquee entities.
- SDL3 now owns the first live non-OpenGL sampled-RTT presentation path: its
  registered renderer allocates an SDL target texture, renders the deterministic
  Engine Arcade attract pass into it, then samples it onto the staged screen
  marker. The no-renderer path still fails closed, so SDL3 remains `Partial`
  rather than claiming unconditional support. SFML3 and Raylib remain
  runtime-availability-gated without equivalent presentation proof.
- Raylib3 now keeps its logical backend identity inside the shared
  OpenGL-family device, so the build-only `engine_arcade.screen` graph,
  cabinet material/model, fake-native hook, and descriptor/work-order contracts
  cover OpenGL, SDL3-over-GL, SFML-over-GL, and Raylib-over-GL without claiming
  live native allocation.
- OpenGL editor preview now consumes sampled render-surface markers for Engine
  Arcade: it owns a native texture/FBO/depth target, paints a deterministic
  attract pass into that target, and samples the color texture onto the staged
  `EngineArcadeScreen` panel as the first visible presentation proof.
- The build-safe arcade RTT contract now covers OpenGL-family, SDL3, SFML3, and
  Raylib cabinet graph submission honestly. OpenGL-family command contexts keep
  post-frame render-target evidence after the pass closes, SDL3/SFML3/Raylib
  cabinet checks require sampled bindings only when a live native runtime exists,
  and otherwise prove fail-closed no-runtime behavior without claiming allocation.
- Raylib's render device now owns CPU-side resource records for graph buffers,
  materials, meshes, models, binding sets, and submitted model evidence, so the
  Engine Arcade cabinet graph can prove model submission in the Raylib lane while
  native RTT allocation remains runtime-availability-gated.
- System Info and the contract harness now report sampled RTT as layered
  evidence instead of a single support claim: descriptor contract, graph proof,
  hook/adaptor readiness, live native allocation, presentation proof, and the
  sampled-RTT rollup are distinct. SDL3 presentation is now `Partial`; SFML3
  and Raylib remain `Missing` until a live allocation and display path is proven.
- EpochGui now owns a backend-neutral `TextControlController` with UTF-8-safe
  caret boundaries, ranged selection, multiline and word navigation,
  insert/delete/copy/cut/paste intent, read-only behavior, and measured scroll
  visibility. Native clipboard access, glyph measurement, rendering, and input
  translation remain engine-adapter responsibilities.
- The editor toolbar now exposes 3D scene construction and 2D game/UI
  construction through an EpochGui-backed segmented scene-mode control, so
  Canvas2D work is an explicit compact mode switch instead of a second wide tab
  or a transient dropdown.
- Windows source now performs editor context selection as an exclusive
  replacement transaction: capture state, retire and clean the source backend,
  create one docked replacement in the same host, adopt and restore its editor
  session, and hold the transaction until native backend readiness. Failed targets retire and
  recover through the source backend; SDL/SFML/Raylib thread ownership and
  backend-child shutdown stay tied to the stable host, manager-host destruction
  waits for renderer cleanup, and Linux partial initialization is cleaned before
  fallback. The operator accepted SDL, SFML, OpenGL, Vulkan, DirectX, and
  software replacement behavior from the `v0.87.71` build. The `v0.87.72`
  source makes Raylib readiness depend on a successful owner-thread GL bind and
  completed first present. The `v0.87.73` source additionally routes adopted
  GLFW child layout through Raylib's render-thread queue to remove the remaining
  click-time UI/render lock inversion. The `v0.87.74` post-readiness session
  gate was too broad and stalled threaded hardware replacements while Software
  continued to work. The `v0.87.75` source makes the dropdown transaction adopt
  the exact context returned by the multicontext manager, restores editor state
  before normal backend activation, and excludes only that target from generic
  multicontext enumeration until render-ready. Focused all-backend testing
  remains required before runtime acceptance.
- Release checkpoint: `v0.87.32` keeps launcher-initiated updates in the
  launcher window until packaged handoff is staged or source worker handoff
  evidence is ready. Packaged runtime installs can still distinguish stable
  release parity from newer main-source availability after the follow-up source
  bump.
- Source checkpoint: `main` is advanced to v0.87.33 after the v0.87.32
  Windows/Linux updater release so packaged installs can intentionally continue
  from release parity into a main-source rebuild.
- Release checkpoint: `v0.87.34` keeps nested package resolution in the updater
  and stages Windows release archives with the runtime payload at archive root
  so older packaged updaters can install the repaired line.
- Source checkpoint: `main` is advanced to v0.87.35 after the v0.87.34
  packaged-updater release while the published stable runtime remains v0.87.34.
- Release checkpoint: `v0.87.36` fixes GUI-host updater child-process stdio so
  hidden downloader, extractor, staged handoff, and source rebuild workers do
  not inherit invalid descriptors from launcher/editor windows.
- Source checkpoint: `main` is advanced to v0.87.37 after the v0.87.36 updater
  stdio release while the published stable runtime remains v0.87.36.
- Release checkpoint: `v0.87.38` routes editor source-only update confirmation
  directly to the detached source worker and reports guarded failures instead
  of leaving the modal at the early install progress band.
- Source checkpoint: `main` is advanced to v0.87.39 after the v0.87.38 editor
  source-update handoff release while the published stable runtime remains
  v0.87.38.
- Release checkpoint: `v0.87.42` restores updater restart countdown/progress
  behavior, gives the launcher a dedicated update progress/cancel/restart
  surface, foregrounds restarted Windows runtimes from the handoff scripts, and
  moves GUI/context implementation ownership to `src/epochgui` and
  per-backend `src/renderers/...` folders.
- Release repair checkpoint: `v0.87.42` now also clears stale updater cancel,
  source, and handoff logs before spawning the detached source worker, refuses
  uncleared source snapshot roots, repairs nested GitHub archive roots only when
  they contain `Engine/vcpkg.json`, and drives launcher/editor loading states
  through the reusable EpochGui loading-screen primitive. The retry path also
  redownloads same-URL packaged archives, isolates source rebuild
  downloads/extraction in per-run work roots, lets Cancel clear the active
  disposable source cache, and removes fake launcher update actions while
  preserving restart-only completion evidence.
- Repair evidence: the refreshed `v0.87.42` release candidate passed MSVC
  Debug and Release `ConsoleApplication1`, Debug and Release
  `--engine-contract-self-test`, staged Windows package `EpochEditor.exe
  --version`, Windows package unzip verification, Linux Clang Release
  no-manifest OpenGL/software build, Linux CTest, Linux `epoch --version`,
  Linux tarball executable verification, and refreshed Windows/Linux checksums.
  The WSL manifest/vcpkg lane is blocked in this environment until
  `python3.10-venv` is available for the `libsystemd` port, so the package lane
  used the already-supported no-vcpkg Clang path instead of shipping stale bits.
- Source checkpoint: `main` is advanced to v0.87.43 after the refreshed
  v0.87.42 Windows/Linux packaged-updater release while the published stable
  runtime remains v0.87.42.
- Release checkpoint: `v0.87.44` hardens updater cancel/retry behavior after
  the same-session crash reports: recent source-update cancellation now blocks
  launcher mode switches, active source rebuilds are reported as source attempts
  to editor state machines, launcher/editor source-worker evidence pumps catch
  log/filesystem exceptions into visible update failure states, and the legacy
  CWD-relative `REPO-main` cleanup path is disabled in favor of executable-local
  tokenized update work roots.
- Source checkpoint: `main` is advanced to v0.87.45 after the v0.87.44
  Windows/Linux updater crash repair release while the published stable runtime
  remains v0.87.44.
- Release checkpoint: `v0.87.48` restores the Linux/WSL vcpkg source-update
  lane. Linux `build.sh` now validates stale vcpkg baselines, resolves
  `clang-scan-deps`, rejects unsupported full-engine Unix Makefiles early,
  keeps SDL3/SFML/Raylib vcpkg feature sets release-safe without making vcpkg
  Windows-only, and repairs static Raylib GLAD/cgltf ownership plus SDL module
  backend registration.
- Source checkpoint: `main` is advanced to v0.87.49 after the v0.87.48
  Windows/Linux updater release while the published stable runtime remains
  v0.87.48.
- Release checkpoint: `v0.87.50` repairs the Linux Raylib atlas lane by moving
  Raylib texture backend storage out of `Context::native_drawable`, uploading
  from immutable atlas pixel snapshots, making the Raylib context atlas hook
  perform a real upload instead of returning a synthetic handle, marking the
  Raylib frame active before queued GUI uploads drain, and packaging the
  tracked GUI font from `Engine/assets/fonts` into the Linux release payload.
- Source checkpoint: `main` is advanced to `v0.87.51` after the refreshed
  `v0.87.50` Windows/Linux Raylib atlas release while the published stable
  runtime remains `v0.87.50`.
- Release checkpoint: `v0.87.52` serializes Linux GLAD initialization before
  render threads can use its process-global loader state. The current static
  Raylib package exports an incompatible GLAD ABI, so Linux release/source
  builds leave Raylib disabled while retaining vcpkg-backed OpenGL, SDL, and
  software lanes. The package includes the tracked `assets/fonts/Roboto-Regular.ttf`
  and passed bounded isolated editor startup checks for every active Linux lane.
- Release repair checkpoint: the refreshed `v0.87.52` source-update lane now
  resolves or bootstraps vcpkg on Linux, passes its exact vcpkg root and policy
  overlay to `build.sh`, and uses the supported Clang full-engine path without
  falling back to GCC modules. The tracked Linux manifest now uses a public
  baseline shared by the Windows and Linux toolchains, keeps incompatible
  Linux Vulkan/SFML/Raylib packages out of the install, and preserves the
  operator-verified Linux SDL, OpenGL, and software context policy.
- Source checkpoint: `main` is advanced to `v0.87.53` after the `v0.87.52`
  stable Windows/Linux release so packaged installs can exercise source-update
  detection while published stable remains `v0.87.52`.
- Build evidence: MSVC Debug and Release x64 `ConsoleApplication1`, Windows
  CMake/MSVC Debug build plus CTest, Linux Clang Release engine build plus
  CTest, and Linux `ninja-clang-debug` build plus CTest passed for the v0.87.30
  updater checkpoint. The local MSVC Debug `ConsoleApplication1` target now
  passes for the v0.87.32 launcher-update status/handoff fix, and
  build-safe contract tests now pass after the Raylib/OpenGL-family/SDL/SFML
  arcade RTT contract and capability-layer updates. The hosted
  `linux-clang-engine` lane caught module-sensitive include gaps in the
  EpochGui implementation translation units; the source now includes
  `<cstdint>` explicitly before relying on `std::uint32_t` or
  `std::uint64_t`, `opengl.textures` now includes the Linux X11 `Window`
  declaration before binding GLX drawable state, and `engine.gui` imports the
  `epoch.gui` module rather than including its headers in the global module
  fragment. Keep the Linux Clang full-engine job green before calling a
  checkpoint sealed, because the
  portable Linux Clang, GCC, and Windows lanes can pass while this full-engine
  lane still catches C++23 module/header hygiene regressions.

## Allowed Source Areas

- `Engine/modules/render.device.ixx`
- `Engine/modules/render.graph.ixx`
- `Engine/modules/render.arcade.ixx`
- `Engine/src/epoch.render.graph.cpp`
- OpenGL backend resource/context/render files
- SDL3 OpenGL-backed context/resource files
- SFML3 OpenGL-backed context/resource files
- Raylib3 OpenGL-backed context/resource files
- `package.registry` only if the Engine Arcade render asset contract needs a
  small correction
- System Info renderer capability reporting if existing code supports it
- `Engine/docs/engine/renderer_feature_matrix.md`
- `Changes/roadmap.md`

## Source Slice Ownership

High-output work on this gate should land buildable code slices, not stop at
inventory. Split work by ownership when using subagents:

- descriptor/resource contract slice:
  `render.device` handles/descriptors, backend requirements, sampled RTT
  ownership records, and destroy/allocation contracts
- graph/binding slice:
  `render.graph` compile/bind validation, render-pass read/write resolution,
  sampler/material slot correctness, and contract harness assertions
- proof consumer slice:
  `render.arcade` and `package.registry` declarations for
  `engine_arcade.screen`, cabinet graph passes, and package-visible resource
  requirements
- OpenGL-family native slice:
  OpenGL FBO/texture/sampler/depth hooks plus SDL3/SFML3/Raylib runtime-gated
  native resource adapters, each kept in its backend-owned files
- capability/status slice:
  System Info and renderer matrix truth so `Present` is never claimed from
  descriptor-only or no-runtime code
  - promote capability reporting beyond booleans: distinguish descriptor
    contract, build-only graph proof, hook readiness, live native allocation
    readiness, and presentation proof
  - DirectX/Vulkan sampled-RTT rows stay `Partial`/`Missing` until real
    `render.device_*` native implementations exist
  - OpenGL-family rows must separate hook factory readiness from allocation in
    a live context
  - SDL3/SFML3/Raylib rows must keep no-runtime refusal separate from live
    runtime allocation support
- build/metadata slice:
  CMake, MSVC project/filter, and focused docs/changelog updates after the code
  builds

Run subagents only on disjoint slices with clear file ownership. The main agent
keeps the integration path and final build proof.

## Forbidden Source Areas

- Software renderer parity
- Shadows
- Deferred rendering
- Skeletal animation
- Particles
- D3D12
- Broad GUI redesign
- Broad Package Manager redesign
- OS AI/model/tooling changes
- Unrelated source-shape cleanup
- Documentation-only pass
- Updater/release code, UI, scripts, packaging, tags, or assets unless the
  operator explicitly reopens the sealed `v0.87.69` baseline

## Acceptance

- OpenGL-derived contexts allocate real native texture, sampler,
  framebuffer/render-target, and optional depth/stencil objects from shared
  descriptors.
- Render graph compilation resolves the sampled render texture into readable
  texture/sampler bindings and writable render-target bindings.
- A render pass can target the render texture.
- A later pass/material can sample the render texture through
  `MaterialTextureSlot::render_surface`.
- Engine Arcade declares and uses `engine_arcade.screen` as a proof surface.
- Capability reporting says present only for actually implemented behavior;
  otherwise partial/missing/deferred.
- Build/check passes using the safest command allowed by `AGENTS.md`.

## Stop Conditions

Stop and report partial progress if:

- backend context ownership prevents safe resource creation,
- handle mapping needs a new backend registry,
- existing capability reporting has no present/partial/missing/deferred model,
- build fails twice on the same issue.

Do not broaden scope to compensate.

## Mission Cache Pointer

Durable cross-pass mission memory lives in `Changes/mission_cache.md`. Keep this
file focused on the current gate; do not widen a source pass because the cache
contains broader roadmap work.

## v0.87.54 Release Gate

- Linux full-engine Release builds with current Clang 22.1.8, CMake 4.4.0,
  Ninja 1.13.2, and vcpkg 2026.06.24.
- OpenGL, Vulkan, SDL, SFML, Raylib, and Software configure and link together;
  SFML is the only shared vcpkg component and is staged under `lib/`.
- Linux CTest and the build-safe engine contract pass from an asset-bearing
  output. Windows MSVC 2022 Release and the same contract also pass.

## v0.87.54 Replacement Linux Startup Gate

- Raylib's embedded GLAD 2 loader is invoked through its actual resolver ABI;
  Linux OpenGL startup must not call it through GLAD 1's zero-argument API.
- The packaged executable RUNPATH is exactly `$ORIGIN/lib`, and staged dynamic
  dependencies must not resolve through vcpkg or build-machine paths.
- SFML and Vulkan runtime libraries are packaged under `lib/`.
- Release staging must pass the contract test and a bounded OpenGL editor
  startup smoke from the isolated package directory.

## v0.87.56 Linux Updater Registry Gate

- Installed vcpkg candidates are accepted only when their checked-out registry
  contains the source manifest baseline in its ancestry.
- An updater that hands `build.sh` a stale checkout must recover through an
  exact-baseline managed checkout under the updater tool cache and rebuild its
  policy overlays from that checkout.
- Direct developer builds fail early with a clear stale-registry message unless
  managed toolchain bootstrap was explicitly requested.
