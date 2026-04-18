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
- embedded-engine compilation through the active engine surface:
  `Engine/include/`, `Engine/modules/`, `Engine/src/`,
  `Engine/src/scripts/`, and `Engine/resource/`

Do not document only one path if the project/scripting shell is supposed to
support both.

Generated project shells now land under repo-root `Projects/` and emit:

- `project.epoch.json`
- `project.paths.txt`
- `epoch.project.cmake`
- a generated child project file
- build scripts
- script include fallback

That keeps generated shells honest across headers, modules, source, scripts,
and resources instead of stopping at headers alone.

If editor logs claim a generated project exists but the user cannot tell where
it landed, inspect `Projects/<ProjectName>/project.paths.txt` first.

The Project/Scripts workspaces should now also expose simple existence proof for
the generated shell surface:

- manifest exists
- entry source exists
- build script exists
- `project.paths.txt` exists
- expected output exists
- build log exists
- active script source exists

If those checks are missing or vague, the generated-project loop still is not
proved honestly enough.

## Local helper probing discipline

At the start of a phase:

- probe `/v1/models`
- respect any helper-use preference the operator has already made explicit
- ask which loaded models are allowed only when that allow-list is unclear
- keep the first detected model as the runtime-parity/in-engine smoke model
- for the current `9900X` + `5800` workstation target, prefer two loaded
  helper models with four drafting lanes each for eight total helper parallels

Launcher scope for that shell should stay flat and direct:

- open project demos directly
- preload or reopen projects
- open the editor
- adjust contexts/settings
- run updates
- quit cleanly

Do not drift back into layered `Games/Tools/Puzzle` menu stacks when the real
goal is a project/editor/bootstrap surface.

## Release packaging discipline

Before publishing a Windows updater-shell zip:

- stage from `x64/Release/`, not from a source folder
- copy the required backend DLLs beside `ConsoleApplication1.exe`
- copy the full VC143 CRT payload from
  `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT\`
  into the release folder so clean Windows machines do not need a separate
  VC++ redistributable install first
- keep `assets/` beside the executable in the packaged folder
- run `ConsoleApplication1.exe --version` from the staged folder before zipping
- smoke the no-args launcher shell once before publishing so a release does not
  ship a dead bootstrap entry path

Before publishing a Linux/WSL2 asset:

- rebuild from the same bumped source commit that will be tagged
- verify the Linux package reports the same version as the tag/source archive
- keep the packaged `linux_main.tar.gz` and the GitHub source snapshot aligned
  to the same commit, not just the same version string
- do not quietly reuse an older Linux artifact after source has changed

## Multicontext regression contract

When a pass touches parented Win32 multicontext behavior, prove these rules
before finishing:

- the parented grid lays out the HWND that actually owns the slot at that
  moment, not a stale abstract primary handle
- the visible backend pane contract is explicit:
  the stable Windows top-row contract is the real child surfaces `GLFW30`,
  `SDL_app`, and `SFML_Window`, with helper `EpochChild` wrappers hidden while
  docked
- after a drag-undock-redock cycle, proxy `EpochChild` hosts must be reattached
  to the parent and hidden again; a pass is not green if a helper host is left
  floating as a top-level orphan
- do not “promote” SDL/SFML backend children to direct grid-pane ownership just
  to hide the host, because that has already regressed maximize stability,
  input, and missing-pane behavior
- after maximize, the visible child rect matches the intended slot rect instead
  of silently growing beyond it
- for SDL/SFML startup fixes, a run still fails if content only becomes visible
  after manual resize, maximize, focus juggling, or drag; the first meaningful
  frame must appear from the normal launch path
- non-maximized startup must survive the startup settle pass with the same
  visible top-row child contract, not a briefly visible extra SDL/SFML wrapper
- closing one visible child pane early must not kill the parent editor
- a proof run must come from `x64/Debug` or `x64/Release` with assets present
- if the same pass touches Linux/WSL2/WSLg behavior, document whether that path
  was also revalidated or still needs follow-up
- do not waive MSVC warnings as harmless drift; mixed module units should keep
  the global module fragment limited to preprocessor directives only, and new
  warnings should be fixed or explicitly justified before sign-off

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
- in editor preview mode, keep OpenGL and Vulkan on the same shared
  `render.preview_grid` camera/projection math instead of letting one backend
  drift onto its own preview-camera implementation

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
- if the second detected helper is vision-capable, use it for screenshot and
  layout review while still keeping the first detected model as the engine's
  runtime-parity smoke target

When two local helper models are loaded, supervisor passes should treat them as
two helper pools with up to four parallel drafting lanes each. Use those lanes
for roadmap phrasing, code-shape proposals, doc rewrites, screenshot review,
bounded subsystem design, and changelog drafting before integrating the final
answer locally.

When possible, route direct helper drafting through LM Studio `/v1/responses`
or `/v1/chat/completions` with bounded output tokens. If the selected local
model rejects an explicit reasoning setting, retry without the reasoning field
instead of treating the helper as broken or empty.

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

When using local helpers through LM Studio direct responses, prefer the
lightest visible-output settings the loaded model actually accepts. For Qwen
helpers that means disabling reasoning when supported; for non-reasoning models
it means omitting the reasoning field entirely.

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
- when only one of those helpers has reliable vision, reserve that helper for
  screenshot/layout review instead of burning main-model tokens on image triage

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
