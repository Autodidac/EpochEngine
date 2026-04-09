# Smoke And Capture Automation

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

When the pass is multicontext-specific, validate:

- top-row and bottom-row context responsiveness
- all six expected contexts are visibly present when the layout is meant to be a
  six-context proof
- chat text input and caret behavior
- wheel zoom and camera movement
- docked child-window ownership and absence of stray promoted panes or fake host
  wrappers
- validate the expected ownership model instead of assuming every backend should
  hide its host:
  SDL/SFML currently keep a visible `EpochChild` slot host with the real backend
  child inside it, while Raylib still shows the real `GLFW30` child directly
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
- for the current hosted SDL/SFML design, it is acceptable for the visible pane
  owner to be `EpochChild` as long as the real `SDL_app` or `SFML_Window` child
  is alive, visible, and rendering correctly inside it
- if only one backend is under investigation, capture that backend directly
- prefer the fitted parented multicontext host so six-context layouts stay
  visible on normal desktop work areas instead of drifting off-screen
- keep the screenshot tied to the source version shown in the README
- refresh the README multicontext proof at least every 10th feature version, or
  sooner whenever visible renderer color, layout, or docking behavior changes
  enough that the existing proof is misleading
- if the engine-owned software capture path is black or otherwise invalid, use
  an honest asset-bearing desktop capture from `x64/Debug` or `x64/Release`
  instead of publishing a misleading proof image
- if a supposedly valid proof still shows fake wrapper ownership, oversized
  child rects, or an early maximize crash, do not update the README screenshot
  yet

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
- raw capture lands in `workspace/auto_train.jsonl`
- MCP/control snapshots can land in `workspace/mcp_capture.jsonl`
- no `workspace/ai/*` checkpoints, compiled models, or caches show up as
  staged Git changes
- `qwen/qwen3.5-9b` is the current fast local helper baseline when loaded
- if the first detected helper model is changed locally, keep using the first
  `/v1/models` entry instead of provoking extra model loads during smoke runs
- when driving local Qwen helpers directly, prefer `/v1/responses` with
  `reasoning.effort = none` so helper output stays visible and does not waste
  output budget on hidden reasoning
- when two helper models are loaded, helper-first passes can use up to four
  parallel drafting prompts per model for planning/review work, while the engine
  runtime itself still stays on the first detected model for parity
- prefer LM Studio `/v1/responses` for offloaded helper drafts, while keeping
  the engine runtime itself on the first detected local model for parity
- if a helper returns blank `content` but useful `reasoning_content`, harvest
  that output for drafting/review instead of discarding the helper pass

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
- use a versioned commit title such as `v0.83.63 ...`
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
