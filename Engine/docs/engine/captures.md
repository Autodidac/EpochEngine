# Smoke Capture And Screenshot Workflow

This doc records the working validation loop for backend, editor, AI, and
systems passes so future automation can follow one predictable method.

Read [AGENTS.md](../../../AGENTS.md) and the
[active pass](../../../Changes/active_pass.md) for the current authorized run
and competing-work limits. Bounded local repair/retest authority does not need
repeated approval. A screenshot session uses only the selected Editor/project;
it does not authorize a model load, unrelated build, publication or interruption
of another test. The release software-OpenGL smoke needs no separate approval.
Reuse attributed captures when live testing is held; never relabel an older
screenshot as a newer binary.

The v0.90.35 README uses the operator's October 7 v0.90.34 AI workbench capture
at `Images/readme/windows-ai-workbench-v09034-20261007.png`. It shows cumulative
source discovery before the checkpoint, not successful native sandbox succession.
Older images remain additive archives.

## Launch root

For Windows editor and multicontext smoke tests, launch from:

- `Artifacts/Runtime/windows/development/x64/Debug/`
- `Artifacts/Runtime/windows/development/x64/Release/`
- `Artifacts/Runtime/windows/development/x64/ReleaseWithDebugInfo/` for the
  Windows/MSVC optimized-symbols mode

Public source uses `public` in place of `development`; the physical edition
marker selects that directory. [EpochOutputLayout.props](../../../Tools/MSVC/EpochOutputLayout.props)
is the direct-MSVC owner. Managed CMake products use
`Artifacts/Runtime/cmake/<preset>` with a configuration suffix for
multi-configuration generators. Configure trees remain in
`Artifacts/Intermediate/cmake/<preset>`. BuildAll dispatches the existing CMake
workflow; there is no `BuildAll` executable or separate combined-build project.
Do not fall back to an old `x64` binary when the graph declares this layout.

Those folders carry the runtime assets used by the main editor host and the
docked backend panes. Do not launch disposable tests from source folders unless
you are deliberately testing a broken-path scenario.

## Standard Windows validation

For runtime/editor/backend changes:

1. Build and check the changed production path using the
   [validation commands](../../../AGENTS.md#3-current-validation-commands).
   Reuse a verified exact binary for a screenshot-only session.
2. Record executable hash, edition, version, configuration and source/build receipt.
3. Launch from its asset-bearing output folder with a fresh checkout-local
   `--candidate-data-root`, or reuse the identified existing Editor. Use the
   selected Release build for showcase images. Keep test data separate from the
   operator's saved project.
4. Verify the intended context and scene are ready, render, and respond to input.
5. Capture proof through the engine where possible, then inspect the actual image.
6. Retire only owned test processes, unless the operator asked to leave the
   identified Editor open. Preserve unrelated windows and running tests.
7. Keep captures and receipts checkout-local. Do not delete diagnostic evidence
   or operator data as routine cleanup.

For a model-free candidate screenshot, provide the complete process-local
native-input binding: `EPOCH_AI_SHARED_NATIVE_INPUTS_DISABLED=1` **and**
`EPOCH_AI_SHARED_MODEL_CACHE_ROOT=<absolute checkout-local empty model-cache>`.
Leave the child runtime executable and DLL-directory overrides absent. The
`ai.native_shared_inputs.hpp::from_environment` contract rejects the disabled
flag alone with `Incomplete shared native input binding`; disabled is not an
independent Boolean switch. The November 15 first screenshot launch reproduced
that refusal and its owned process retired. The corrected binding permits
catalog-only operation without discovering or copying a native runtime. Do not
change the user's saved model configuration for a screenshot.

Candidate projects must be physically under that data root's `Projects/`.
Preserve their manifest IDs by using a fresh data root when an existing test
already owns the same ID. To preload an admitted copied project, set
`EPOCH_EDITOR_PROJECT_ID` and `EPOCH_EDITOR_PROJECT_MANIFEST` only in the launched
process; the manifest must point inside its private Projects directory.

For release-facing passes, add two more checks:

8. Check the exact staged package using the
   [release procedure](../build/scripts.md#version-advancement), including its
   bounded software-renderer smoke. Native presentation remains a separate check.
9. If a Windows zip is being published, verify the staged release folder carries
   the VC143 CRT DLLs app-local before zipping

When the pass is multicontext-specific, validate:

- top-row and bottom-row context responsiveness
- all six expected contexts are visibly present when the layout is meant to be a
  six-context proof
- chat text input and caret behavior
- wheel zoom and camera movement
- docked child-window ownership and absence of stray promoted panes or fake host
  wrappers
- validate the current parented ownership model directly:
  the stable Windows top-row proof should show the real child panes `GLFW30`,
  `SDL_app`, and `SFML_Window`, with helper `EpochChild` wrappers hidden while
  docked
- for SDL/SFML dock probes, treat both docked contracts as valid when they are
  what the runtime is actually using:
  real child directly parented into the grid with the helper host hidden, or
  child attached to a proxy host shell during a true detached window state
- when validating SDL/SFML multicontext undock in the parented editor, start
  the drag from the visible `EpochChild` proxy shell rather than the nested
  `SDL_app` / `SFML_Window` child; that shell is the real pane surface the
  operator grabs, and using the child HWND gives misleading drag results
- for SDL/SFML proxy drags, the current honest contract is release-based redock:
  prove `MidTopLevel=true`, keep the pane detached while the drag is still held,
  then prove `EndRedocked=true` after the release returns inside the parent
- for Raylib dock probes, treat the docked state as valid when the visible
  `GLFW30` child is parented directly into the grid and the parked `EpochChild`
  helper host remains hidden under the parent with no proxy child in use
- for Raylib redock probes, remember that undock/move/redock
  commands are routed through the Raylib owner/render thread command queue.
  Do not reintroduce direct cross-thread `SetParent` / style / position changes
  against the GLFW/Raylib child from the Win32 dock proc just to make a harness
  sample immediate.
- for proxy-host backends, verify the helper `EpochChild` host reattaches to the
  parent and stays hidden after redock; do not sign off if the host remains a
  visible or top-level orphan after the drag cycle
- for SDL/SFML startup work, do not sign off if the pane only becomes visible
  after resize, maximize, focus juggling, or drag; a first-frame fix must hold
  from the normal launch path
- for resize/maximize regressions, verify the grid is operating on the HWND that
  actually owns the dock slot at that moment and then perform at least one early
  child-close check without killing the parent editor
- for Raylib/SDL/SFML parented panes, compare the visible child rect against the
  intended slot rect after maximize; do not sign off if a docked child silently
  grows beyond the slot
- for Win32 parented multicontext checks, keep a temporary live window-tree
  probe handy so the proof can explicitly show visible backend child classes and
  hidden helper wrappers
- do not treat `--smoke --capture` as valid pane proof if the run exits before
  backend child takeover settles; fall back to a bounded stable `--editor` run
  and confirm the visible child classes directly
- if startup settle timing is under investigation, run both a normal startup
  and a maximize pass; the same hidden-wrapper contract must survive both
- backend palette parity when clear colors should match
- GUI theme parity: `System Light/Dark` must follow the OS app-theme preference
  when available, while manual `Light` and `Dark` selections visibly change the
  editor palette without changing only the native window chrome
- command-menu input capture: open the Window menu, click every visible
  show/hide/reset row, and reject the build if toolbar tabs, context selectors,
  scene widgets, or dock controls behind the menu receive the same click
- pane popout input: drag the real World Outliner, Inspector, Console Dock, and
  AI Chat title bars far enough and release to request their routed native
  popouts; verify the result is that pane's cloned content, not a second editor
  shell or generic GUI test window. A successful popout must hide the source
  pane in the original editor, restore it on Dock Back, Close, or native window
  close, and preserve normal text/font rendering after the routed context spawns
- Systems workspace graph clipping and pan/zoom behavior
- Video Editor time controls, pacing diagnostics, and the bottom scene timeline
  strip when the pass touches the shared time spine

## Engine-owned capture flow

Available knobs:

- runtime flag: `--capture`
- runtime flag: `--capture-frame <1-36000>` to delay a native capture without smoke mode
- runtime flag: `--smoke`
- env var: `EPOCH_CAPTURE_DIR`
- warmup control: `capture_warmup_frames` in `core.commandline`

`--candidate-data-root <root>` routes captures to `<root>/logs/captures/` and
wins over `EPOCH_CAPTURE_DIR`. Without that private data root, the explicit
`EPOCH_CAPTURE_DIR` applies, otherwise captures use executable-local
`logs/captures/`. The owner is `Engine/src/platform/core.path.cpp`.

`--capture` requests one frame per backend/window. `--smoke` adds the current
12-frame warmup in `core.commandline`; it does not wait for terrain streaming,
package admission or asynchronous assets to finish. `--scene` supplies a scene
hint/capture stem, not a project file to open. For settled showcase images use
Tools > Copy Window Screenshot (`F12`) after the real scene is ready. This
engine action copies to the clipboard. Save that captured image explicitly and
verify the saved file. A target-window capture is a fallback.

For scripted world captures use `--capture --capture-frame 900` and verify
terrain residency in the captured frame. The number is counted per backend
window. It leaves the separately timed Windows parent-window smoke proof alone.
For the OpenGL pass probe, set `EPOCH_OPENGL_PREVIEW_SAMPLE_RADIUS` to the
camera distance divided by the body radius. `EPOCH_OPENGL_PREVIEW_SAMPLE_PIXEL`
adds one viewport-relative `x,y` sample measured from the bottom left.
`EPOCH_OPENGL_PREVIEW_SAMPLE_DWELL_MS` extends the stable-camera wait from
750 ms up to 30,000 ms when streamed geometry settles after the camera.
For an explicit Software project smoke, the warmup counts frames after the
renderer draws the project preview or replays the GUI of a GUI-only project.
The Canvas2D fallback grid leaves the screenshot available for a later frame.
Check the run log and a later native frame if startup capture is inconclusive.

Prefer engine-owned capture over ad hoc desktop grabs whenever possible.

## Current world and cloud showcase

Use an orbit image and a surface image of the same identified project. Compose
from a lit view with terrain, water and useful cloud depth visible, then retain
one full Editor image showing the actual authoring workflow. Wait for accepted
terrain and texture residency; a settled frame is required even if FPS is high.
Record cloud visibility, camera/location, simulation time and the selected view
mode. Wireframe intentionally omits the cloud composition.

The current OpenGL density path is owned by `opengl.cloud_volume`, using the
`asset.cloud_volume` recipe delivered by the optional Planetary provider in a
v5 `terrain.epoch_planet_patch`. The November 12 source has regional coverage,
advecting fronts and varied vertical density. Consult
[renderer evidence](renderers.md#evidence-policy) for implemented and unverified
parts. Old body inventories (`.epoch_pbod`) and old geometric clouds do not
prove density transport. A screenshot of a blank or coarse startup scene does
not prove the new clouds.

Before opening an existing project, verify its admitted Planet and Water
sources, saved recipe and matching provider receipt. A disposable copy must
preserve those required inputs and the exact admitted `Packages/.runtime`
provider when reusing a verified build. Copying only Assets/worlds leaves no
provider for cloud/LOD work. Never synthesize cloud data or bypass admission
just to obtain a screenshot. A source/package upgrade goes through Package
Manager review and regeneration; record if this needs a later build rather than
silently starting one alongside another workload.

For each chosen image, keep a concise receipt in the owning pass with the
capture path/hash, executable identity, source/build receipt, project identity,
backend, time and limitations. Store diagnostic attempts under
`Artifacts/Reports/<pass>/`. Approved reusable images are additive under
`Images/readme/`; preserve old images. Inspect every image for private paths,
chat/source text and credentials before any public use. A screenshot session
does not change the root README, its badges, layout, captions or image links.
Follow the [version procedure](../build/scripts.md#version-advancement) for a
later publication; do not advance a version just to name a capture.

## Screenshot guidance

- use the real editor, not the updater shell, for README proofs
- prefer a full multicontext frame when validating layout changes
- a multicontext proof is only valid when every intended pane is present and no
  backend is replaced by a fake wrapper or black/empty surface
- do not publish a proof that still shows a visible extra SDL/SFML wrapper in
  the top row; the docked pane should be the real child surface
- if the harness and a clean human six-pane drag test disagree, do not refresh
  the README proof until the runtime truth has been confirmed by direct editor
  use plus subsystem logs and the harness has been brought back into line
- if only one backend is under investigation, capture that backend directly
- prefer the fitted parented multicontext host so six-context layouts stay
  visible on normal desktop work areas instead of drifting off-screen
- keep the screenshot tied to the source version shown in the README
- refresh the README per-backend proofs from clean startup-only full-window
  captures instead of reusing stale crops after layout, palette, or dock
  chrome changes
- flag an outdated README proof when visible renderer color, layout or docking
  behavior makes it misleading; stage a fresh image without rewriting the README
  until that publication change is requested
- README proof must reflect the current source and evidence matrix. Use the
  seven-provider regression plan for release evidence; include Software when
  proving the CPU fallback rather than presenting an obsolete fixed lineup.
- use the current engine-owned full-window capture where available; retain
  harness captures as attributed diagnostic evidence and verify all child panes
  are present before claiming full multicontext proof
- if the engine-owned software capture path is black or otherwise invalid, use
  an honest target-window capture from the verified asset-bearing output folder
  instead of publishing a misleading proof image
- if a supposedly valid proof still shows fake wrapper ownership, oversized
  child rects, or an early maximize crash, do not update the README screenshot
  yet
- when a runtime issue is visually diagnosed during a pass, store the relevant
  captures under `Engine/docs/engine/diagnostics/<date>-<topic>/` with a short
  README that names the defect and the acceptance gate. Do not leave the only
  copy in `C:\tmp` or an operator screenshot folder.
- when docking/redocking is under investigation, run both single-backend and
  full-grid parented harness passes before signing off
- treat focus-only proof as incomplete for text input:
  the next missing automation gate is a typed-text editor smoke for AI chat and
  other edit boxes
- if the same pass touches Linux/WSL2/WSLg launcher or parented behavior,
  document whether that path was actually revalidated or still needs follow-up
- if WSL visual capture is black or the GLX backend crashes, keep the Linux
  result as build/headless proof only. Do not refresh README Linux screenshots
  or publish a Linux runtime asset until a real WSLg/native-Linux visual smoke
  produces an honest non-black frame.
- for Linux/WSL single-OpenGL editor proof, use the editor surface explicitly:
  `epoch --renderer opengl --standalone --editor --smoke --capture`.
  A plain runtime smoke can exit correctly while writing a black capture because
  it does not necessarily open the editor/workbench surface being proven.
- If Linux/WSL visual proof regresses, start by checking distro visibility,
  DISPLAY, WAYLAND_DISPLAY, XDG_RUNTIME_DIR, and glxinfo -B before
  changing engine code.
- when operator-provided specialty screenshots are promoted into README proof,
  copy them into `Images/readme/` with versioned names and keep the original
  screenshots untouched.

## AI smoke prompts

The [AI runtime contract](ai_runtime.md), [source workflow](ai_source.md) and
[active pass](../../../Changes/active_pass.md) own model selection, permissions,
resource limits and exact test scope. A screenshot run does not start inference.
When an AI workflow is in scope, reuse its existing admitted engine path and
selected model; do not add external helper servers or repeated readiness prompts.
Record the actual model, visible result, cancellation/retirement and resource
receipt. A model list or healthy service alone does not prove a completed task.
Keep raw transcripts and mutable state in the selected local data root, outside
public source/release payloads. Do not surface hidden reasoning as an answer.

- for GUI behavior changes, helper review prompts should explicitly check theme
  mode names, menu click capture, detached pane identity, and whether any click
  behind a visible menu fired a different command
- run GUI harness sessions serially, not in parallel; parallel parented runs can
  contaminate drag/focus evidence and should not be treated as trustworthy
- keep startup-render proof and dock/undock validation as separate harness passes:
  use the normal harness path for drag truth, and opt into `-CaptureStartupProof`
  only when you are intentionally collecting first-frame rendered-content samples
- for startup-layout evidence, pair the existing harness's
  `-CaptureStartupProof` with `-StartupOnly` to avoid synthetic drag dependencies.
  Use a later settled capture for project, terrain, cloud or texture showcases.
- when validating proxy-child backends, prefer the new `ContractMode` and
  `LateDetached` fields over older assumptions that every backend should already
  be redocked before mouse release
- start proxy-shell drags from the real visible dock shell, not from the parent
  origin or the nested backend child, otherwise the harness invents a bogus
  drag offset and can make SDL/SFML look frozen inside the parent when the
  runtime is actually waiting on the wrong synthetic cursor path
- when proving dock -> undock -> redock in the parented harness, poll for the
  actual detached/redocked state transition instead of assuming a fixed sleep is
  enough under full-grid renderer load
- for SDL/SFML six-pane proof, require both `MidTopLevel=true` and
  `MidEscapedParentBounds=true`; a top-level proxy shell that still sits fully
  inside the parent bounds is not honest detached proof
- if the harness reports a docking mismatch, cross-check the engine log before
  locking in a runtime conclusion; SFML full-grid runs in particular have shown
  real undock/redock commands completing after a too-early harness sample
- treat live human six-pane validation as the final gate for SDL/SFML proxy
  undock behavior; the harness is still useful for repeatable observation and
  screenshots, but it should not outrank clean manual proof when synthetic input
  diverges
- if the maximized full-grid parented pass times out waiting for expected panes,
  treat that as a real blocker and do not substitute reduced-geometry proof for
  a maximize-ready signoff
- when validating SFML redock, trust the actual release position more than the
  last remembered drag point; the Win32 path now redocks from the release-point
  test specifically to avoid stale drag-state failures in loaded full-grid runs
- if a serial harness run leaves a non-responding `EpochEditor`, preserve the
  failure evidence, retire only its exact owned process tree, and stop runtime
  probing until the cause is understood. Do not terminate an operator Editor.

## Release asset checks

- Windows packaged-runtime zips should include the app-local VC143 CRT DLL set,
  not rely on the target machine already having the redistributable installed
- smoke the staged packaged folder with `--version` before uploading
- smoke the no-args packaged entry path once before uploading
- generated ProjectLauncher/Sandbox child builds share checked-in engine
  `EpochEngine` outputs today; editor Run and emitted Windows build scripts now
  serialize through the generated-project build lock until their
  engine-object/module/PDB outputs are isolated. If a hand-run child build skips
  that lock, failures on `EpochEngine` clean logs, module IFC/BMI files, or PDB
  locks should be treated as a build-lane collision first.
- Linux/WSL2 packaged assets must report the same version as the tagged source
  commit they were built from
- Linux/WSL2 packaged assets should boot the main runtime path by default;
  updater-shell builds are explicit bootstrap variants, not the standard Linux
  release identity
- do not publish a Linux asset rebuilt from one commit while GitHub source
  downloads point at another

## Systems/graph checks

- `Systems` must remain the landing zone for frame graph, task graph, and
  threading surfaces
- generated graph textures must stay clipped to the dock layout
- graph pan/zoom must work for wide surfaces
- fixed-step/time diagnostics should remain visible when the time spine is in
  scope
- the displayed diagnostics should reinforce the compatibility baseline and
  support-tier strategy instead of hiding them in separate docs only
- black or empty software captures are not valid proof; they should trigger
  deeper investigation or a different honest capture path

## Publication and source identity

Use the single [version and release procedure](../build/scripts.md#version-advancement)
and its [Development preservation rules](../build/scripts.md#preserving-development-source).
The capture procedure does not maintain another version checklist. A fresh
screenshot cannot establish an edition's build, privacy, updater or Linux
acceptance. Keep the exact source and executable receipts separate from the
image, and label any unverified behavior honestly.

## Automation target

The next honest automation step is to script:

- build
- launch from the asset-bearing output folder
- wait for warmup
- request engine-owned captures
- exercise feature-specific GUI actions such as theme selection, Window-menu row
  clicks, and pane-title drag/release popouts when those areas changed
- verify expected files landed
- close the runtime cleanly
- stage verified showcase images additively, leaving README publication as a
  separate requested change
