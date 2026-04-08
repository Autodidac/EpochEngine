# Smoke And Capture Automation

This doc records the current working method for local validation so backend/UI
passes can be repeated without guessing.

## Launch root

For Windows editor and multicontext smoke tests, launch from:

- `x64/Debug/`
- `x64/Release/`

Those folders carry the runtime assets used by the editor host and the docked
backend panes. Launching from a source directory is more likely to give false
asset/font failures.

## Standard Windows validation

For runtime/editor/backend changes:

1. Build `ConsoleApplication1 | Debug | x64`
2. Build `ConsoleApplication1 | Release | x64`
3. Launch the real binary from `x64/Debug/` for interactive validation
4. Verify the intended contexts actually appear, render, and respond to input
5. Close the live windows before finishing the pass

When a pass is multicontext-specific, validate:

- top-row and bottom-row context responsiveness
- chat text input and caret behavior
- wheel zoom and camera movement
- docked window ownership and absence of stray promoted panes
- backend palette parity when clear colors should match
- AI dock/provider status when the pass touches local model or oracle behavior

## Engine-owned capture flow

The engine already supports capture-driven smoke work.

Available knobs:

- runtime flag: `--capture`
- runtime flag: `--smoke`
- env var: `EPOCH_CAPTURE_DIR`
- warmup control: `capture_warmup_frames` in `core.commandline`

Capture outputs commonly land under:

```text
logs/captures/
```

Backend capture surfaces already exist for:

- OpenGL
- Software
- SDL
- SFML
- RayLib

When a desktop screenshot is visually misleading because of WSLg or extra-window
behavior, prefer the engine-owned capture path.

## Screenshot guidance

- Use the real editor, not the updater shell, for README proofs.
- Prefer a full multicontext frame when validating layout changes.
- If only one backend is under investigation, capture that backend directly to
  isolate the regression.
- Use a maximized or 4K-sized editor host when validating six-context layouts.
- Keep the screenshot tied to the source version shown in the README.

## AI smoke prompts

When LM Studio is running locally, use at least one editor-context prompt and
one C++/engine prompt:

- `In Epoch editor, project 'Sandbox' has 6 entities. Suggest one concrete next edit and one gameplay follow-up.`
- `Explain why mixed C++23 module units should use module; before legacy includes.`

Expected smoke behavior:

- the selected model is logged
- the AI dock returns a visible reply
- raw capture is written only to `workspace/auto_train.jsonl`
- no `workspace/ai/*` checkpoints or models show up as staged Git changes

## Systems/graph checks

- The `Systems` tab should remain the landing zone for frame graph, task graph,
  and threading surfaces.
- If the graph view is rendered as a generated texture, verify it stays clipped
  to the dock and does not escape the layout when the graph is wide.

## Commit pattern memory

The working commit/push pattern is:

- sync with `origin/main`
- keep unrelated dirt out of the pass
- bump `aengine.version.ixx`
- update docs/README/changelog when the behavior is user-visible
- use a versioned commit title such as `v0.83.60 ...`
- verify builds before pushing

## Next automation target

The next honest automation step is to script:

- build
- launch from the asset-bearing output folder
- wait for warmup
- request engine-owned captures
- verify expected files landed
- close the runtime cleanly

That should replace ad hoc manual screenshot passes over time.
