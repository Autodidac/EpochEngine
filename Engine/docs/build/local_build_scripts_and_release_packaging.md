# Local Build Scripts And Release Packaging

The helper scripts under `Engine/` are optional, but they are still the fastest
repeatable path for local builds when you want the tree, output folders, and
docs flow to stay predictable.

Epoch has one CMake-owned build graph. Visual Studio/MSBuild on Windows,
CMake presets, VS Code/Codium CMake Tools, direct command-line configure/build
commands, native Linux scripts, and Windows-hosted WSL builds are entry points
into that graph rather than separate platform projects. A fix to target source,
features, module ownership, or compile policy belongs in CMake first; helper
scripts select and validate the matching toolchain and cache layout.

## `build.sh`

Run from `Engine/`:

```bash
./build.sh [--no-vcpkg] [--updater-shell] [--bootstrap-current-toolchain] [--tool-cache-root <path>] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
```

What it does:

- configures from the `Engine/` source root
- builds into out-of-tree locations under `Engine/Bin/`, or under
  `EPOCH_BUILD_ROOT` when a native-filesystem build root is required
- enables module scanning flags
- uses vcpkg by default on Windows, Linux, and WSL when `VCPKG_ROOT` or a
  normal local vcpkg checkout can be found
- verifies that the manifest builtin baseline exists in the local vcpkg clone
  and fetches it before configure when the clone is stale
- resolves `clang-scan-deps` for Clang module builds and passes it to CMake
- verifies or bootstraps the pinned CMake, LLVM/Clang, and Ninja toolchain,
  then keeps vcpkg port builds on that same compiler/tool set
- selects the tracked `x64-linux-epoch` triplet on Linux; the graph remains
  static except for SFML so SFML and Raylib can coexist without duplicate STB
  ownership
- generates docs when Doxygen is available

`--no-vcpkg` is an explicit diagnostic/system-package escape hatch. It is not
the normal Linux updater or release lane.

Release optimization remains target-owned by CMake. Current Clang Release
builds use `-O3` generally, with narrowly documented source-file overrides only
for reproducible compiler defects. LLVM 22.1.8 currently requires
`core.commandline.ixx` and `network.core.ixx` at `-O0` because its CGSCC/inliner and
`globalopt` passes crash on those modules; this does not disable optimization
for updater, editor, runtime, renderer, or other engine code.

Examples:

```bash
cd Engine
./build.sh clang Release
./build.sh gcc Debug -- -DEPOCH_CI_HEADLESS_ONLY=ON
./build.sh --no-vcpkg clang Debug -- -DEPOCH_ENABLE_RAYLIB=OFF
```

Use Ninja or Visual Studio generators for full-engine C++23 module builds.
Unix Makefiles are intentionally rejected for full-engine targets because they
do not provide the module dependency flow Epoch needs.

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
- bump `Engine/modules/epoch.version.ixx`
- use a descriptive commit title without baking the version number into the
  commit message
- rebuild `ConsoleApplication1` in both `Debug|x64` and `Release|x64`
- launch from the asset-bearing `x64/Debug/` or `x64/Release/` runtime, not
  from a source folder
- close live windows after validation
- avoid disposable runs from bad folders that leave stray logs or captures in
  the wrong place
- keep updater downloads, extraction work, temporary probes, and managed helper
  tools under the runtime's executable-local `cache/updates/` folder; downloaded
  package archives belong in `cache/packages/`, and generated atlases belong in
  `cache/atlases/`
- do not include `cache/updates/`, `cache/packages/`, or `cache/atlases/` in
  public runtime packages
- when the README or other public-facing markdown changes, verify the rendered
  GitHub result after push instead of trusting the raw file text alone

If a pass touches Linux or WSL behavior, validate the matching Linux build path
too instead of pretending Windows proof is enough.

GitHub CI/workflow discipline:

- keep workflows build-only unless a real headless/runtime-safe automation path
  exists
- do not depend on GUI launch, desktop focus, or screenshot capture in CI
- keep the Linux Clang engine lane as build-only graphics coverage: it should
  build the real `epoch` target with OpenGL, Vulkan, SDL, SFML, Raylib, and
  software dependencies, then run headless CTest without opening windows
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

- `Engine/modules/epoch.version.ixx`
- README/public version badges
- changelog/release notes
- release tags
- packaged asset filenames

Version numbers do **not** belong in commit titles.

`Tools/ai/get_epoch_version.ps1` is the canonical release-tooling reader for
`Engine/modules/epoch.version.ixx`. It emits the same zero-padded revision used
by `GetEngineVersion()`. Hosted and local staging must consume that result
instead of reconstructing a version string independently.


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

- stage a production package root, not a stripped CMake folder and not a
  source-shaped repo bundle
- include one public editor/runtime executable named `EpochEditor.exe`
- include the runtime `assets/` folder beside `EpochEditor.exe`
- do not include generated/cache `atlases/`; the runtime owns atlas
  regeneration
- do not include source-shaped `Engine/` folders, duplicated `x64/Debug` or
  `x64/Release` folders, headless smoke executables, or
  `ConsoleApplication1.exe` aliases in the public runtime package
- copy the required backend DLLs beside `EpochEditor.exe`
- copy the full VC143 CRT payload from
  `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT\`
  into the release folder so clean Windows machines do not need a separate
  VC++ redistributable install first
- keep `assets/` beside the executable in the packaged folder
- run `EpochEditor.exe --version` from the staged folder before zipping
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
- use the validated Clang full-engine path for the package build, through
  `Engine/build.sh clang Release`, unless a later release pass proves another
  Linux compiler path
- keep vcpkg enabled for the normal Linux release and updater-source rebuild
  lanes; `--no-vcpkg` is not the default package/update path
- use the pinned Clang 22.1.8 toolchain and matching `clang-scan-deps`, normally
  through `--bootstrap-current-toolchain`
- do not publish the Linux package while the hosted `linux-clang-engine` build
  lane is failing
- verify the Linux package reports the same version as the tag/source archive
- verify `./epoch --version` from the staged package directory
- smoke the no-args packaged entry locally under Linux/WSLg or a real Linux
  desktop before calling the release runtime-ready
- when static Raylib is unavailable because of the Linux GLAD ABI boundary,
  prove the active OpenGL, SDL, and software editor lanes instead; do not ship
  a Raylib-linked archive that crashes other renderer selections at startup
- keep the packaged versioned Linux runtime asset, for example
  `epoch_linux_x64_vX.Y.Z.tar.gz`, and the GitHub source snapshot aligned
  to the same commit, not just the same version string
- verify the packaged Linux artifact starts the main runtime path by default
  instead of accidentally shipping an updater-shell-only bootstrap
- do not quietly reuse an older Linux artifact after source has changed
- include one runtime executable, required shared libraries, root `assets/`,
  README, and LICENSE
- do not include generated/cache `atlases/`, source-shaped `Engine/` folders,
  duplicated build-output folders, or headless smoke executables in the public
  Linux runtime package
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

## OS AI asset policy

Epoch documents OS/open-source model integration, not bundled model weights:

- an engine-owned OS-model harness for prompts, memory, tools, verification,
  evidence metrics, and dataset/eval gates
- local MCP/control/tool harnesses that operate the engine and collect proof
- operator-selected model lanes for Qwen, Nemotron, FLUX, Wan, and TRELLIS

External local LLMs such as LM Studio are development helpers. They are useful
for testing, curation, evaluation, and speeding up documentation/build work,
but they are selected reviewer providers rather than hidden authority.

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
- `Engine/ai/prompts/`

Downloaded model/package artifacts stay out of Git:

- executable-local `cache/packages/`
- executable-local `cache/updates/`
- future package-manager model cache folders

Git-safe staging capture paths include:

- `Engine/examples/ConsoleApplication1/workspace/model_exchange.jsonl`
- `Engine/examples/ConsoleApplication1/workspace/tool_trace.jsonl`

Explicit `model_exchange.jsonl` and `tool_trace.jsonl` writes are session
evidence, not automatic model training. Review or delete stale traces and
promote only intentional fixtures into `Engine/ai/evals/` or reviewed evidence
sets.When using local helpers through LM Studio direct responses, prefer the
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
- validating that selected OS models produce visible answers through the engine path

If multiple helper models are loaded:

- use only the models the operator has allowed for helper drafting
- fan out up to five bounded prompts total when the local server supports it
- keep the in-engine runtime path on the explicitly selected editor model
- when only one allowed helper has reliable vision, reserve that helper for
  screenshot/layout review, pane/layout checks, and color/parity triage

When the helper returns mostly reasoning text or stalls:

- use the helper for bounded drafting, not as a blocker for compile-critical work
- prefer refining small helper drafts locally over waiting on long monolithic answers
- if `content` is blank but `reasoning_content` contains useful-looking text,
  reject it as engine assistant output. Do not harvest hidden reasoning into
  AI chat, explicit MCP tool traces, model exchanges, or reviewed eval fixtures.

## Related docs

- `cmake_presets_and_builds.md`
- `../engine/runtime_and_editor_workflows.md`
- `../engine/smoke_capture_and_screenshot_workflow.md`
- `../engine/os_ai_tooling_and_evidence_policy.md`
- `../../Changes/roadmap.md`
- `developer_tools_and_dependencies.md`
