# Build Scripts

The helper scripts under `Engine/` are optional, but they are still the fastest
repeatable path for local builds when you want the tree, output folders, and
docs flow to stay predictable.

## `build.sh`

Run from `Engine/`:

```bash
./build.sh [--no-vcpkg] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
```

What it does:

- configures from the `Engine/` source root
- builds into out-of-tree locations under `Engine/Bin/`
- enables module scanning flags
- attempts to discover `VCPKG_ROOT` unless `--no-vcpkg` is used
- generates docs when Doxygen is available

Examples:

```bash
cd Engine
./build.sh gcc Release
./build.sh --no-vcpkg clang Debug -- -DEPOCH_ENABLE_RAYLIB=OFF
```

## `run.sh`

```bash
cd Engine
./run.sh [gcc|clang] [Debug|Release] [-- <runtime args>]
```

This launches the `epoch` runtime from the matching `Engine/Bin/...` output
directory.

## `install.sh`

```bash
cd Engine
./install.sh [gcc|clang] [Debug|Release]
```

Installs into `Engine/built/bin/<Compiler>-<Config>`.

## `clean.sh`

```bash
cd Engine
./clean.sh
```

Removes:

- `Engine/Bin/`
- `Engine/build/`
- `Engine/built/`
- top-level CMake cache/state under `Engine/`

## Commit and test discipline

When a pass changes runtime, editor, backend, AI, or capture behavior:

- sync with `origin/main` if the local branch has drifted
- keep unrelated dirt out of the commit
- bump `Engine/modules/aengine.version.ixx`
- use a versioned commit title such as `v0.83.63 ...`
- rebuild `ConsoleApplication1` in both `Debug|x64` and `Release|x64`
- launch from the asset-bearing `x64/Debug/` or `x64/Release/` runtime, not
  from a source folder
- close live windows after validation
- avoid disposable runs from bad folders that leave stray logs or captures in
  the wrong place

If a pass touches Linux or WSL behavior, validate the matching Linux build path
too instead of pretending Windows proof is enough.

Generated project shells should keep two honest integration modes working:

- duplicated engine source/layout projects
- static engine compilation through the exported `Engine/include/` surface

Do not document only one path if the project/scripting shell is supposed to
support both.

The generated project shell now emits an `epoch.project.cmake` fragment and
uses `__has_include` fallback for the script API so embedded-engine projects can
prefer `Engine/include/` without instantly breaking older include-root setups.

Launcher scope for that shell should stay flat and direct:

- open project demos directly
- preload or reopen projects
- open a clean editor
- adjust contexts/settings
- run updates
- quit cleanly

Do not drift back into layered `Games/Tools/Puzzle` menu stacks when the real
goal is a project/editor/bootstrap surface.

## Multicontext regression contract

When a pass touches parented Win32 multicontext behavior, prove these rules
before finishing:

- the parented grid lays out the HWND that actually owns the slot at that
  moment, not a stale abstract primary handle
- the visible backend pane contract is explicit:
  `GLFW30` is still the visible Raylib pane, while SDL/SFML currently use a
  visible `EpochChild` host that owns the slot and contains the real `SDL_app`
  or `SFML_Window` child
- do not “promote” SDL/SFML backend children to direct grid-pane ownership just
  to hide the host, because that has already regressed maximize stability,
  input, and missing-pane behavior
- after maximize, the visible child rect matches the intended slot rect instead
  of silently growing beyond it
- closing one visible child pane early must not kill the parent editor
- a proof run must come from `x64/Debug` or `x64/Release` with assets present

Two specific implementation rules should stay written down because they have
already regressed:

- do not re-query a stale child `GetClientRect(...)` and overwrite the explicit
  size the grid or `WM_SIZE` handler just asked for
- do not call top-level backend window-size APIs on a child-docked backend after
  it has been reparented into the grid, or the backend can reintroduce
  top-level chrome-sized growth and break maximize stability
- for SDL/SFML hosted panes, keep the host as the grid slot owner and resize the
  real backend child inside that host instead of swapping the slot owner during
  maximize

## AI asset policy

Epoch currently documents two engine AI runtime roles:

- internal EpochBot inside the engine/editor/runtime
- local MCP/control bots that can operate the engine and also train EpochBot

External local LLMs such as LM Studio are development helpers. They are useful
for testing, curation, evaluation, and speeding up documentation/build work,
but they are not a third engine runtime role.

Future automated passes should use available local helpers aggressively for
draft reasoning, documentation, screenshot review, and bounded code sketches
before spending main-model tokens on the final implementation path.

At the start of each phase:

- probe `/v1/models`
- use the first two detected models as helper drafting pools when available
- keep the first detected model as the only runtime-parity/in-engine smoke
  model

When two local helper models are loaded, supervisor passes should treat them as
two helper pools with up to four parallel drafting lanes each. Use those lanes
for roadmap phrasing, code-shape proposals, doc rewrites, screenshot review,
bounded subsystem design, and changelog drafting before integrating the final
answer locally.

When possible, route direct helper drafting through LM Studio `/v1/responses`
with `input` payloads and `reasoning.effort = none`, so helper output stays
fast, visible, and easy to integrate without provoking hidden reasoning churn.

Git-safe AI assets live under:

- `Engine/ai/datasets/curated/`
- `Engine/ai/datasets/schema/`
- `Engine/ai/evals/`
- `Engine/ai/manifests/`
- `Engine/ai/tokenizer/`
- `Engine/ai/prompts/`

Local-only compiled AI artifacts stay out of Git:

- `workspace/ai/checkpoints/`
- `workspace/ai/models/`
- `workspace/ai/cache/`

Git-safe staging capture paths include:

- `workspace/auto_train.jsonl`
- `workspace/mcp_capture.jsonl`

`append_training_sample(...)` and MCP capture writes are raw/staging data, not
automatic curated truth. Review them, delete bad or outdated samples when the
training direction changes, and only then promote intentional records into
`Engine/ai/datasets/curated/` or `Engine/ai/evals/`.

When using local Qwen helpers through LM Studio direct responses, prefer
`reasoning.effort = none` for fast drafting instead of settings that fall back
to reasoning-heavy output.

## Hardware support strategy

The default compatibility target is:

- 6-core desktop CPU class
- GTX 1660 Ti-era GPU class
- modern Linux laptop/desktop environments

Support strategy:

- baseline tier:
  stable editor/runtime path with broad reach
- standard tier:
  full OpenGL/Vulkan-capable hardware path
- extended tier:
  heavier backend/libs/features that developers explicitly opt into per project

The point is broad automatic support first, not making every game carry every
integration by default.

## LM Studio development-helper notes

When a local helper model is available:

- endpoint is usually `http://localhost:1234`
- model selection is currently first-detected from `/v1/models` so the engine
  does not provoke extra model loads
- validated fast helper baseline is `qwen/qwen3.5-9b`
- stronger local helpers such as Gemma can be used for drafting, evaluation,
  and smoke prompts when available

Use the helper model for:

- editor-context smoke prompts
- dataset cleanup suggestions
- roadmap/doc phrasing assistance
- drafted reasoning and code-outline assistance for bounded engine tasks
- validating that EpochBot receives visible answers through the engine path

If two helper models are loaded:

- use the first two `/v1/models` entries for helper drafting work
- fan out up to four concurrent prompts per model when the local server supports
  it
- keep the first detected model as the only runtime-parity/in-engine smoke model
  so the engine does not provoke extra model loads during testing

When the helper returns mostly reasoning text or stalls:

- keep the first-detected model rule intact
- use the helper for bounded drafting, not as a blocker for compile-critical work
- prefer refining small helper drafts locally over waiting on long monolithic answers
- if `content` is blank but `reasoning_content` contains the useful answer,
  harvest it as helper output instead of discarding the pass

## Related docs

- `build_presets.md`
- `runtime_operations.md`
- `smoke_capture_automation.md`
- `ai_build_memory.md`
- `../../Changes/roadmap.md`
- `tools_list.md`
