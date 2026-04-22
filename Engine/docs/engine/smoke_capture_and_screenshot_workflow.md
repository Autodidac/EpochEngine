# Smoke Capture And Screenshot Workflow

This doc records the working validation loop for backend, editor, AI, and
systems passes so future automation can follow one predictable method.

## Launch root

For Windows editor and multicontext smoke tests, launch from:

- `x64/Debug/`
- `x64/Release/`

Those folders carry the runtime assets used by the main editor host and the
docked backend panes. Do not launch disposable tests from source folders unless
you are deliberately testing a broken-path scenario.

## Standard Windows validation

For runtime/editor/backend changes:

1. Build `ConsoleApplication1 | Debug | x64`
2. Build `ConsoleApplication1 | Release | x64`
3. Launch the real binary from `x64/Debug/`
4. Verify the intended contexts appear, render, and respond to input
5. Capture proof through the engine where possible
6. Close live windows before finishing the pass
7. Clean up disposable logs/captures created outside the proper runtime path

For release-facing passes, add two more checks:

8. Launch `ConsoleApplication1.exe` once with no extra args from `x64/Release/`
   and confirm the expected packaged shell actually appears instead of hanging
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
- Systems workspace graph clipping and pan/zoom behavior
- Systems time controls and pacing diagnostics when the pass touches the shared
  time spine

## Engine-owned capture flow

Available knobs:

- runtime flag: `--capture`
- runtime flag: `--smoke`
- env var: `EPOCH_CAPTURE_DIR`
- warmup control: `capture_warmup_frames` in `core.commandline`

Capture outputs commonly land under:

```text
logs/captures/
```

Prefer engine-owned capture over ad hoc desktop grabs whenever possible.

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
- refresh the README multicontext proof at least every 10th feature version, or
  sooner whenever visible renderer color, layout, or docking behavior changes
  enough that the existing proof is misleading
- the current PowerShell harness screenshot is diagnostic proof, not the final
  long-term screenshot system; the target remains an engine-owned parent-window
  proof path for fullscreen multicontext captures
- if the engine-owned software capture path is black or otherwise invalid, use
  an honest asset-bearing desktop capture from `x64/Debug` or `x64/Release`
  instead of publishing a misleading proof image
- if a supposedly valid proof still shows fake wrapper ownership, oversized
  child rects, or an early maximize crash, do not update the README screenshot
  yet
- when docking/redocking is under investigation, run both single-backend and
  full-grid parented harness passes before signing off
- treat focus-only proof as incomplete for text input:
  the next missing automation gate is a typed-text editor smoke for AI chat and
  other edit boxes
- if the same pass touches Linux/WSL2/WSLg launcher or parented behavior,
  document whether that path was actually revalidated or still needs follow-up

## AI smoke prompts

When a local helper model is running, use at least one editor-context prompt and
one C++/engine prompt:

- `In Epoch editor, project 'Sandbox' has 6 entities. Suggest one concrete next edit and one gameplay follow-up.`
- `Explain why mixed C++23 module units should use module; before legacy includes.`

Expected smoke behavior:

- the selected helper model is logged
- the helper path should use the first model returned by `/v1/models` unless a
  future explicit selector is added
- local helper drafting for docs/code/review is encouraged, but runtime parity
  testing should still stay on the first detected model
- the AI dock returns a visible reply
- if the first detected model rejects explicit reasoning configuration, the
  request path should retry without the reasoning field instead of surfacing an
  empty reply
- raw capture lands in `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl`
- MCP/control snapshots can land in `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl`
- no `Engine/examples/ConsoleApplication1/workspace/ai/*` checkpoints, compiled models, or caches show up as
  staged Git changes
- if the first detected helper model is changed locally, keep using the first
  `/v1/models` entry instead of provoking extra model loads during smoke runs
- when driving local helpers directly, prefer bounded `/v1/responses` or
  `/v1/chat/completions` requests; omit explicit reasoning config when the
  loaded model rejects it
- probe `/v1/models` at the start of a phase, respect any already-stated
  helper-use preference, and only ask which loaded models are allowed when that
  allow-list is not already clear
- for the current `9900X` + `5800` workstation target, prefer two loaded helper
  models with up to four parallel drafting prompts per model for eight total
  helper lanes, while the engine runtime itself still stays on the first
  detected model for parity
- prefer LM Studio `/v1/responses` for offloaded helper drafts, while keeping
  the engine runtime itself on the first detected local model for parity
- if a helper returns blank `content` but useful `reasoning_content`, harvest
  that output for drafting/review instead of discarding the helper pass
- if the first two detected helpers split text and vision strengths, keep the
  first model as runtime parity and use the vision-capable helper for screenshot
  review, pane/layout checks, and color/parity triage
- run GUI harness sessions serially, not in parallel; parallel parented runs can
  contaminate drag/focus evidence and should not be treated as trustworthy
- keep startup-render proof and dock/undock validation as separate harness passes:
  use the normal harness path for drag truth, and opt into `-CaptureStartupProof`
  only when you are intentionally collecting first-frame rendered-content samples
- for README or doc screenshot refreshes, prefer the new startup-only harness
  path: pair `-CaptureStartupProof` with `-StartupOnly` so proof capture does
  not depend on synthetic drag input when the runtime bug under investigation is
  unrelated to startup layout
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
- if a serial harness run leaves a non-responding `ConsoleApplication1`, kill it
  and discard that run instead of treating the JSON as evidence

## Release asset checks

- Windows packaged-runtime zips should include the app-local VC143 CRT DLL set,
  not rely on the target machine already having the redistributable installed
- smoke the staged packaged folder with `--version` before uploading
- smoke the no-args packaged entry path once before uploading
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

## Commit pattern memory

The working commit/push pattern is:

- sync with `origin/main`
- keep unrelated dirt out of the pass
- bump `aengine.version.ixx`
- keep `Changes/roadmap.md` current when the steering surface changes
- update README/docs/changelog when the behavior is user-visible
- use a descriptive commit title without baking the version number into the
  commit message
- verify builds before pushing
- do not leave live windows or bad-folder logs behind

## Automation target

The next honest automation step is to script:

- build
- launch from the asset-bearing output folder
- wait for warmup
- request engine-owned captures
- verify expected files landed
- close the runtime cleanly
- optionally refresh the README proof when the version cadence or visible
  renderer/layout changes require it
