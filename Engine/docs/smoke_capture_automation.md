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
- chat text input and caret behavior
- wheel zoom and camera movement
- docked window ownership and absence of stray promoted panes
- backend palette parity when clear colors should match
- Systems workspace graph clipping and pan/zoom behavior

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
- if only one backend is under investigation, capture that backend directly
- use a maximized or 4K-sized editor host when validating six-context layouts
- keep the screenshot tied to the source version shown in the README
- refresh the README multicontext proof at least every 10th feature version, or
  sooner whenever visible renderer color, layout, or docking behavior changes
  enough that the existing proof is misleading

## AI smoke prompts

When a local helper model is running, use at least one editor-context prompt and
one C++/engine prompt:

- `In Epoch editor, project 'Sandbox' has 6 entities. Suggest one concrete next edit and one gameplay follow-up.`
- `Explain why mixed C++23 module units should use module; before legacy includes.`

Expected smoke behavior:

- the selected helper model is logged
- the helper path should use the first model returned by `/v1/models` unless a
  future explicit selector is added
- the AI dock returns a visible reply
- raw capture lands in `workspace/auto_train.jsonl`
- MCP/control snapshots can land in `workspace/mcp_capture.jsonl`
- no `workspace/ai/*` checkpoints, compiled models, or caches show up as
  staged Git changes
- `qwen/qwen3.5-9b` is the current fast local helper baseline when loaded

## Systems/graph checks

- `Systems` must remain the landing zone for frame graph, task graph, and
  threading surfaces
- generated graph textures must stay clipped to the dock layout
- graph pan/zoom must work for wide surfaces
- the displayed diagnostics should reinforce the compatibility baseline and
  support-tier strategy instead of hiding them in separate docs only

## Commit pattern memory

The working commit/push pattern is:

- sync with `origin/main`
- keep unrelated dirt out of the pass
- bump `aengine.version.ixx`
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
