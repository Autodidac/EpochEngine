# Local Build Scripts And Release Packaging

The helper scripts under `Engine/` are optional, but they are still the fastest
repeatable path for local builds when you want the tree, output folders, and
docs flow to stay predictable.

## `build.sh`

Run from `Engine/`:

```bash
./build.sh [--no-vcpkg] [--updater-shell] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
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
./build.sh clang Release
./build.sh gcc Debug -- -DEPOCH_CI_HEADLESS_ONLY=ON
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
- commit only stable, verified changes
- bump `Engine/modules/engine.version.ixx`
- use a descriptive commit title without baking the version number into the
  commit message
- rebuild `ConsoleApplication1` in both `Debug|x64` and `Release|x64`
- launch from the asset-bearing `x64/Debug/` or `x64/Release/` runtime, not
  from a source folder
- close live windows after validation
- avoid disposable runs from bad folders that leave stray logs or captures in
  the wrong place
- when the README or other public-facing markdown changes, verify the rendered
  GitHub result after push instead of trusting the raw file text alone

If a pass touches Linux or WSL behavior, validate the matching Linux build path
too instead of pretending Windows proof is enough.

GitHub CI/workflow discipline:

- keep workflows build-only unless a real headless/runtime-safe automation path
  exists
- do not depend on GUI launch, desktop focus, or screenshot capture in CI
- keep the Linux Clang engine lane as build-only graphics coverage: it should
  build the real `epoch` target with runner-safe OpenGL/software/SFML
  dependencies, then run headless CTest without opening windows
- `v0.84.35` Windows proof adds DirectX/D3D11 to the local multicontext screenshot
  matrix. DirectX is Windows-only and should be disabled automatically for Linux
  packages and hosted Linux lanes.
- keep Linux/GCC hosted and local shared presets headless by default until GCC
  module BMI writing is reliable enough for full-engine validation
- keep workflow action runtimes current so the repo does not drift onto stale
  Node/action baselines

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
- do not auto-select or auto-name an in-engine model from discovery alone
- keep in-engine chat/tool execution disabled until the operator selects a model
- use available LM Studio helper lanes for drafting/review only when the
  operator has allowed them; current workstation passes may use up to five
  bounded helper parallels

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

## Release naming and publication contract

Keep these identities separate:

- **development source line**: the active repo on `main`
- **stable runtime release**: the current packaged product snapshot
- **bootstrap updater-shell release**: the small updater-focused package that
  exists only to move users into the newer runtime

Version numbers belong in:

- `Engine/modules/engine.version.ixx`
- README/public version badges
- changelog/release notes
- release tags
- packaged asset filenames

Version numbers do **not** belong in commit titles.

GitHub source archives should stay full source snapshots. Do not trim them down
to match packaged runtime or updater-shell assets.

The updater contract stays binary-first:

1. check the newest packaged runtime asset first
2. update into that runtime when it is newer
3. only continue to source when packaged parity is already reached

Install/update type matrix:

- packaged Windows runtime installs use the newest matching
  `epoch_win10_x64_vX.Y.Z.zip` asset
- packaged Linux and WSL runtime installs use the newest matching
  `epoch_linux_x64_vX.Y.Z.tar.gz` asset
- source checkout installs still check packaged runtime first, then rebuild from
  the GitHub source snapshot only when the packaged runtime is already
  version-equal/newer or no newer packaged asset exists
- WSL is treated as Linux for release-asset naming; do not publish a separate
  WSL-only runtime asset unless the runtime/package layout actually diverges
- updater-shell packages are bootstrap installers only and must keep their own
  `epoch_updater_shell_only_*` names

Do not reintroduce standalone packaged version text assets as the primary
contract. The packaged version identity should be clear from the tagged source
and the packaged asset filename itself.

Before publishing a Windows packaged runtime zip:

- stage from `x64/Release/`, not from a source folder
- copy the required backend DLLs beside `ConsoleApplication1.exe`
- copy the full VC143 CRT payload from
  `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT\`
  into the release folder so clean Windows machines do not need a separate
  VC++ redistributable install first
- keep `assets/` beside the executable in the packaged folder
- run `ConsoleApplication1.exe --version` from the staged folder before zipping
- smoke the no-args packaged entry once before publishing so a release does not
  ship a dead startup path
- name the runtime asset with the packaged version baked in, for example
  `epoch_win10_x64_vX.Y.Z.zip`
- if the same release family also includes a bootstrap updater-shell drop, keep
  that as a separate versioned asset such as
  `epoch_updater_shell_only_win10_x64_vX.Y.Z.zip` instead of overloading
  the runtime package name
- if the release is meant to be the published stable runtime, update the README
  stable-release badge and any public release-note surfaces to match that exact
  packaged version before tagging

Before publishing a Linux/WSL2 asset:

- rebuild from the same bumped source commit that will be tagged
- use the validated Clang full-engine path for the package build unless a later
  release pass proves another Linux compiler path
- do not publish the Linux package while the hosted `linux-clang-engine` build
  lane is failing
- verify the Linux package reports the same version as the tag/source archive
- verify `./epoch --version` from the staged package directory
- smoke the no-args packaged entry locally under Linux/WSLg or a real Linux
  desktop before calling the release runtime-ready
- keep the packaged versioned Linux runtime asset, for example
  `epoch_linux_x64_vX.Y.Z.tar.gz`, and the GitHub source snapshot aligned
  to the same commit, not just the same version string
- verify the packaged Linux artifact starts the main runtime path by default
  instead of accidentally shipping an updater-shell-only bootstrap
- do not quietly reuse an older Linux artifact after source has changed
- include the runtime executable, required shared libraries, assets, shaders,
  and scripts in the staged package instead of assuming the repo tree exists
  beside the executable
- keep any Linux bootstrap drop separate, for example
  `epoch_updater_shell_only_linux_x64_vX.Y.Z.tar.gz`
- keep the Linux naming and published-version story aligned with Windows so the
  stable runtime line is obvious across both platforms

After a release is cut:

- keep the tagged source, packaged asset names, and public release notes aligned
- then move `main` forward again as the development line
- update the README badges so `Current Source Development` and
  `Published Stable Release` stay honest instead of collapsing into one label

## Multicontext regression contract

When a pass touches parented Win32 multicontext behavior, prove these rules
before finishing:

- the parented grid lays out the HWND that actually owns the slot at that
  moment, not a stale abstract primary handle
- the visible backend pane contract is explicit:
  the stable Windows top-row contract is the real visible pane owners
  `GLFW30`, `SDL_app`, and `SFML_Window`, with helper `EpochChild` shells kept
  as implementation detail instead of stray top-level clutter
- after a drag-undock-redock cycle, proxy `EpochChild` hosts must be reattached
  to the parent and stop lingering as floating top-level orphans
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
- for README proof refreshes after `v0.84.35`, the normal Windows six-context
  public lineup is Raylib, SDL, SFML, Vulkan, OpenGL, and DirectX; Software
  belongs in fallback/debug/headless proof unless the release explicitly tests
  safe-launch behavior
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

Epoch currently documents three internal AI/control pieces:

- EpochBot inside the engine/editor/runtime
- local MCP/control/tool harnesses that operate the engine and collect proof
- the offline/injectable OSS or tiny backup LLM path for fallback, generated
  software embedding, and EpochBot training support

External local LLMs such as LM Studio are development helpers. They are useful
for testing, curation, evaluation, and speeding up documentation/build work,
but they are selected teacher/reviewer providers rather than hidden authority.

Future automated passes should use available local helpers aggressively for
draft reasoning, documentation, screenshot review, and bounded code sketches
before spending main-model tokens on the final implementation path.

At the start of each phase:

- probe `/v1/models`
- respect operator-selected/allowed models before sending helper traffic
- treat discovered models as available helper pools, not as the active in-engine
  model
- keep the engine runtime/chat/tool path disabled until the operator selects a
  model in the editor
- when the operator allows it, fan out up to five bounded LM Studio helper
  prompts for roadmap phrasing, code-shape proposals, docs, screenshot review,
  bounded subsystem design, and changelog drafting

When possible, route direct helper drafting through LM Studio `/v1/responses`
or `/v1/chat/completions` with bounded output tokens. If an allowed helper model
rejects an explicit reasoning setting, retry without the reasoning field
instead of treating the helper as broken or empty.

Git-safe AI assets live under:

- `Engine/ai/datasets/curated/`
- `Engine/ai/datasets/schema/`
- `Engine/ai/evals/`
- `Engine/ai/manifests/`
- `Engine/ai/tokenizer/`
- `Engine/ai/prompts/`

Local-only compiled AI artifacts stay out of Git:

- `Engine/examples/ConsoleApplication1/workspace/ai/checkpoints/`
- `Engine/examples/ConsoleApplication1/workspace/ai/models/`
- `Engine/examples/ConsoleApplication1/workspace/ai/cache/`

Git-safe staging capture paths include:

- `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl`
- `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl`

`append_training_sample(...)` and MCP capture writes are raw/staging data, not
automatic curated truth. Review them, delete bad or outdated samples when the
training direction changes, and only then promote intentional records into
`Engine/ai/datasets/curated/` or `Engine/ai/evals/`.

When using local helpers through LM Studio direct responses, prefer the
lightest visible-output settings the loaded model actually accepts. For helpers
that expose reasoning controls, disable reasoning when supported; for
non-reasoning models, omit the reasoning field entirely.

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
- discovery lists available models, but the editor must not name or activate
  one until the operator selects it
- helper lanes can be used for drafting, evaluation, and smoke prompts only
  when the operator has allowed that loaded model

Use the helper model for:

- editor-context smoke prompts
- dataset cleanup suggestions
- roadmap/doc phrasing assistance
- drafted reasoning and code-outline assistance for bounded engine tasks
- validating that EpochBot receives visible answers through the engine path

If multiple helper models are loaded:

- use only the models the operator has allowed for helper drafting
- fan out up to five bounded prompts total when the local server supports it
- keep the in-engine runtime path on the explicitly selected editor model
- when only one allowed helper has reliable vision, reserve that helper for
  screenshot/layout review, pane/layout checks, and color/parity triage

When the helper returns mostly reasoning text or stalls:

- use the helper for bounded drafting, not as a blocker for compile-critical work
- prefer refining small helper drafts locally over waiting on long monolithic answers
- if `content` is blank but `reasoning_content` contains the useful answer,
  harvest it as helper output instead of discarding the pass

## Related docs

- `cmake_presets_and_builds.md`
- `../engine/runtime_and_editor_workflows.md`
- `../engine/smoke_capture_and_screenshot_workflow.md`
- `../engine/ai_training_memory_and_dataset_policy.md`
- `../../Changes/roadmap.md`
- `developer_tools_and_dependencies.md`
