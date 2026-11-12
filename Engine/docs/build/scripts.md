# Local Build Scripts And Release Packaging

All new generated output belongs under the repository's `Artifacts/` folder.
Opening either the repository or `Engine/` in VS Code uses presets and leaves
source folders free of compiler output.

| Folder | Contents | Safe to discard after builds stop? |
|---|---|---|
| `Artifacts/Intermediate/` | Object files, module caches and CMake configure trees | Yes; the next build recreates them. |
| `Artifacts/Runtime/` | Runnable programs, libraries and staged assets | Keep the builds you use. |
| `Artifacts/Install/` | Installed/staged distributions | Keep until their purpose is finished. |
| `Artifacts/Dependencies/` | Installed third-party build dependencies | Keep while building; removing them requires reinstalling dependencies. |
| `Artifacts/Packages/` | Release packages | Keep published and pending delivery artifacts. |
| `Artifacts/Reports/` | Test, cleanup and diagnostic evidence | Keep evidence required by the active pass. |

MSVC uses `Intermediate/msvc/<edition>/<platform>/<configuration>/<project>`
and `Runtime/windows/<edition>/<platform>/<configuration>`. Managed CMake trees
use `Intermediate/cmake/<lane>` and put runnable outputs under
`Runtime/cmake/<lane>`; multi-configuration generators append the configuration.
Custom external CMake build roots retain their explicit output convention.
Retained older candidate source keeps its declared v1 layout; new source uses v2.
Do not move an existing CMake cache: it contains absolute paths. Reconfigure it
at the new location, then discard the obsolete cache after validation.

Preview compiler cleanup from the repository root:

```powershell
powershell -NoProfile -File Tools/build/clean_epoch_intermediates.ps1
```

Add `-Remove` to remove only `Artifacts/Intermediate`. Add `-LegacyMSVC` to
target only the old root `build/obj` instead. The command refuses active build
processes, tracked files and redirected paths, and keeps a receipt under Reports.
It never targets projects, candidate source, Runtime, Install or Packages.

The helper scripts under `Engine/` are optional, but they are still the fastest
repeatable path for local builds when you want the tree, output folders, and
docs flow to stay predictable.

Epoch has a shared source/ownership contract expressed in CMake and the existing
native MSVC projects; the latter are not generated from CMake. Keep both graphs
in sync when sources, dependencies or compile policy change. CMake presets,
VS Code/Codium, native Linux scripts and WSL helpers select the corresponding
toolchain/cache. Windows BuildAll uses CMake from the existing Editor project;
there is no separate combined-build project.

## `build.sh`

Run from `Engine/`:

```bash
./build.sh [--no-vcpkg] [--updater-shell] [--bootstrap-current-toolchain] [--tool-cache-root <path>] [gcc|clang] [Debug|Release] [-- <extra cmake args>]
```

What it does:

- configures from the `Engine/` source root
- builds into out-of-tree locations under `Artifacts/Intermediate/cmake/linux/<edition>/`, or under
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

This launches the `epoch` runtime from the matching `Artifacts/Runtime/cmake/linux/<edition>/<Compiler>-<Config>` output
directory.

## `install.sh`

```bash
cd Engine
./install.sh [gcc|clang] [Debug|Release]
```

Installs into `Artifacts/Install/linux/<edition>/<Compiler>-<Config>`.

## `clean.sh`

```bash
cd Engine
./clean.sh
```

Removes only this edition's `Artifacts/Intermediate/cmake/linux/<edition>`
after resolving and checking its exact path. It keeps Runtime and Install.
It leaves other editions, custom build roots, retained legacy outputs and project
source untouched. Verified removal of obsolete pre-migration data is a separate
operator-approved cleanup step.

## Commit and test discipline

When a pass changes runtime, editor, backend, AI, or capture behavior:

- reconcile against the current Site-admitted Epoch checkpoint when the local branch has drifted
- keep unrelated dirt out of the commit
- commit only stable, verified changes
- bump canonical `Engine/modules/epoch.version.ixx` and Windows source resource
  metadata; `engine.version` re-exports that authority, not duplicate macros
- preserve separately pinned packaged versions until actual platform releases
  are admitted; a source update does not replace downloads
- use a descriptive commit title without baking the version number into the
  commit message
- rebuild `EpochEditor` in both `Debug|x64` and `Release|x64`
- when the operator approves a native run, launch from the asset-bearing output
  directory, never the source folder; build-only proof does not authorize GUI tests
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
  Site result after publication instead of trusting the raw file text alone

If a pass touches Linux or WSL behavior, validate the matching Linux build path
too instead of pretending Windows proof is enough.

Hosted build/report discipline:

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

### Version advancement

1. Record the intended source version, the existing runtime version for each
   edition and platform, and which release pointers the delivery will advance.
   Use the same source version for the matching Public and Development snapshots.
2. Change the source version literals in `Engine/modules/epoch.version.ixx` and
   all four Windows resource fields in `Engine/resource/WinResource.rc`
   (`FILEVERSION`, `PRODUCTVERSION`, `FileVersion` and `ProductVersion`).
   `Engine/modules/engine.version.ixx` re-exports the canonical module.
   Read the formatted version with `Tools/build/get_epoch_version.ps1` and
   verify the Windows resource strings and built executable report that value.
   Packaged version overrides are inputs for the platform and runtime release
   being staged. Confirm the active runtime versions independently of the source.
3. Update the owner README's current-source text, the changelog and the release
   notes. Regenerate the Public README and `source-projection.json` from the
   reviewed source. Its source badge, alt text and source tag link come from the
   resource's `ProductVersion`. Its runtime badge and link follow GitHub's latest
   stable release. Read back the README on GitHub's default branch, including
   the badge image, alt text and link targets.
4. Freeze the two reviewed commits. Match each tag or Site source entry, archive
   filename, checksum sidecar and receipt to its exact edition, version, commit
   and tree. Mark a GitHub source-only release as a prerelease so the latest
   stable release continues to identify the packaged runtime. For a runtime
   release, also check the embedded distribution identity,
   platform asset names, signed integrity document and updater discovery. Verify
   the source and runtime links on GitHub, the Public Site and the owner portal.
5. Verify the live downloads against the approved bytes. Read back the matching
   source backups and checksums before retiring any superseded local copies.

### Preserving Development source

Public EpochEngine source and runtime packages are published on
[GitHub](https://github.com/Autodidac/EpochEngine). Publish the audited Public
source projection with the public repository's ancestry. Verify that the
archive and its Git tree contain no private source or private history.

Development source and runtime packages use the separate owner-authenticated
Site. Keep its smart-Git repository, commit/tree browser, source archives,
runtime downloads and mutable aliases behind server-side authorization. Use
short-lived authenticated downloads where the Site issues them. Keep
administrative credentials out of source, manifests, logs and documentation.

- Preserve the full Development commit and its private ancestry in the owner
  repository. Keep a restricted source backup with commit, tree, archive hashes
  and a verified readback receipt. Retain the prior accepted checkpoint for
  rollback. Keep operator state, credentials and withheld private data out of
  release archives.
- Build Public from the audited projection in a separate checkout with Public
  ancestry. Run `Tools/build/verify_public_source.py` and verify the Git tree
  and release archive agree. Limit public staging and pushes to that checkout.
- Stage Development archives through the owner-only encrypted intake. Verify
  archive contents, file modes and decrypted checksums against the approved
  commit before activating the source pair. Confirm signed-in downloads, anonymous
  denial with `no-store`, and retirement of any one-use intake grant. Keep earlier
  release bytes and rollback records intact.

Hosted repositories and source archives must be full committed-source
snapshots. Freeze the reviewed publishable working set into one commit before
hosting it; never expose transient uncommitted state or trim source to match a
runtime/updater-shell payload. Verify each standalone dependency commit against
its admitted source manifest and the corresponding engine pin.
Preserve original stable checkpoint `ad6c416d930b348a61bc37ceb7d4522742be084a`
in history. The operator-authorized `multicontext-base-stable` branch mirrors
main by fast-forward; do not rewrite its ancestry, recreate it from a runtime
archive or substitute the v0.89.06 tag.

Public update discovery uses the signed GitHub release integrity document.
Development source updates use the authenticated Site. Use the owner download
for Development runtime packages while its automatic runtime acquisition is
disabled. Verify that each updater refuses the other edition's payload.
Keep historical releases immutable. Publish Windows ZIP and Linux tar.gz
runtime forms with checksum sidecars, then read back the live commit, asset
digest, edition and version from the owning channel.

Build the Public release artifacts from the audited Public commit on GitHub
Actions using its pinned Windows and Linux jobs. The package checks refuse
personal build paths in the executable and bundled libraries, including paths
recorded by the compiler, linker, or a dependency build. A local build may
pass contracts and still be unsuitable for distribution. Verify the exact
returned packages and sign their hashes for the GitHub updater.

The updater contract stays binary-first:

1. check the newest packaged runtime asset first
2. update into that runtime when it is newer
3. only continue to source when packaged parity is already reached

Install/update type matrix:

- packaged Windows runtime installs use the newest matching
  `epoch_win10_x64_vX.Y.Z.zip` asset
- packaged Linux and WSL runtime installs use the newest matching
  `epoch_linux_x64_vX.Y.Z.tar.gz` asset
- Public and Development installs each stay on their own distribution channel
- WSL is treated as Linux for release-asset naming; do not publish a separate
  WSL-only runtime asset unless the runtime/package layout actually diverges
- updater-shell packages are bootstrap installers only and must keep their own
  `epoch_updater_shell_only_*` names

Do not reintroduce standalone packaged version text assets as the primary
contract. The packaged version identity should be clear from the tagged source
and the packaged asset filename itself.

### Local build-admission receipts

The Site does not compile Epoch on demand. A local or controlled build machine
must prove the exact committed source, artifact, and required validation lanes,
then materialize the canonical evidence with
`Tools/build/write_epoch_build_validation_receipt.ps1`. The writer reads the
tracked source and platform-packaged version authorities, refuses a mismatched
version, requires one exact 40-character commit and committed-tree SHA-256,
hashes the built artifact itself, and admits only a bounded, duplicate-free
Release check set. Windows receipts require source-name, compile, engine
contract, headless, package-inventory, and dependency evidence. Linux and macOS
also require shared-library resolution. A failed required lane cannot be
reported as a release candidate; renderer smoke may be explicitly skipped only
when the publication truthfully leaves native pixels unclaimed.

The output is deterministic `epoch.build-validation/v1` JSON plus a SHA-256
sidecar. It is evidence for Site admission, not publication authority: it does
not upload, deploy, alter a release, create a listener, or grant an AI model
release control. The Site may expose the admitted receipt and status beside
source/runtime version metadata after independently checking the committed
tree and uploaded bytes. Run the tool's pure contract before changing its wire
format:

```powershell
.\Tools\build\write_epoch_build_validation_receipt.ps1 -SelfTest
```

For a real candidate, pass the platform/compiler/configuration, target and
toolchain identifiers, staged artifact, exact source commit and tree digest,
the JSON array of check evidence, and an output path. The receipt writer derives
source and packaged versions from `epoch.version`; callers cannot substitute a
different authority.

Before publishing a Windows packaged runtime zip:

- stage a production package root, not a stripped CMake folder and not a
  source-shaped repo bundle
- include one public editor/runtime executable named `EpochEditor.exe`
- include the runtime `assets/` folder beside `EpochEditor.exe`
- do not include generated/cache `atlases/`; the runtime owns atlas
  regeneration
- do not include source-shaped `Engine/` folders, duplicated `x64/Debug` or
  `x64/Release` folders, headless smoke executables, or
  legacy executable aliases in the public runtime package
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

- Use `stage_epoch_linux_release.ps1 -SoftwareOpenGLSmoke` for the required
  offscreen graphics test. The bounded software-renderer test runs without
  separate approval and records evidence for the exact staged executable.
  `-SkipRuntimeSmoke` is for build-only preflight. It still verifies version,
  engine contracts, notices, dependencies, `$ORIGIN/lib`, GLIBC baseline,
  archive contents, and checksums. Release acceptance requires a graphics
  smoke receipt.
- rebuild from the same bumped production source that will be tagged; a closing
  docs-only commit must carry an exact production-input comparison and both
  build-commit and release-commit identities in provenance. If a README is
  refreshed in an already validated archive, preserve every executable/library,
  mode and symlink, independently compare all entries and record the repack and
  new artifact hash. Never relabel it as byte-identical to the original CI archive.
- use the validated Clang full-engine path for the package build, through
  `Engine/build.sh clang Release`, unless a later release pass proves another
  Linux compiler path
- keep vcpkg enabled for the normal Linux release and updater-source rebuild
  lanes; `--no-vcpkg` is not the default package/update path
- use the pinned Clang 22.1.8 toolchain and matching `clang-scan-deps`, normally
  through `--bootstrap-current-toolchain`
- do not publish the Linux package while the hosted `linux-clang-engine` build
  lane is failing
- build the published Linux artifact on the pinned Ubuntu 22.04 release runner
  and reject binaries or bundled libraries that require newer than
  `GLIBC_2.35`; download and start the hosted artifact on that baseline before
  publication
- keep C++23 language mode independent from optional standard-library surface;
  use the engine-owned `core.format` contract for production formatting so the
  Ubuntu 22.04 libstdc++ baseline remains buildable
- verify the Linux package reports the same version as the tag/source archive
- verify `./epoch --version` from the staged package directory
- smoke the no-args packaged entry locally under Linux/WSLg or a real Linux
  desktop before calling the release runtime-ready
- when static Raylib is unavailable because of the Linux GLAD ABI boundary,
  prove the active OpenGL, SDL, and software editor lanes instead; do not ship
  a Raylib-linked archive that crashes other renderer selections at startup
- bind the versioned Linux runtime asset, for example
  `epoch_linux_x64_vX.Y.Z.tar.gz`, to the exact reviewed source commit for its
  edition
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
- a proof run must come from `Artifacts/Runtime/windows/<edition>/x64/Debug` or
  `Artifacts/Runtime/windows/<edition>/x64/Release` with assets present
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

Epoch integrates operator-selected external models; release packages do not
contain model weights, training datasets, or a hidden model runtime. Discovery
may inventory local OpenAI-compatible endpoints or an explicitly configured
`llama-cli`, but Epoch sends no request until the operator confirms a model for
the current run.

Normal chat and scene authoring send only visible, bounded prompt context. A
source-development request is stricter: the host ranks existing paths without
reading or transmitting source, gives an exact canonical objective path
precedence, and displays the reviewed selection and endpoint. Share Curated
Context is a separate explicit operator action that revalidates the source root,
unchanged objective, endpoint, and byte budgets, then sends complete counted
content through 48 KiB or one UTF-8-safe 16 KiB objective-centered excerpt for a
larger reviewed file. It performs no model-directed path discovery and retains no
source context beyond the request. Existing-file proposals remain blocked without
that exact evidence; only a strict exact-block packet may stage, and model output
never certifies its own build, test, review, or promotion evidence.

Git-safe AI contracts live under:

- `Engine/ai/evals/`
- `Engine/ai/manifests/`
- `Engine/ai/prompts/`
- `Engine/ai/control/`

Downloaded models and disposable execution state stay out of Git under
executable-local `cache/models/`, `cache/packages/`, and `cache/updates/`.
Explicit `model_exchange.jsonl` and `tool_trace.jsonl` files are operator-review
evidence only. Epoch does not automatically promote chat, build output, traces,
or source proposals into training data or live source.

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

## Local OpenAI-compatible endpoint notes

A local endpoint such as LM Studio is an operator-selected provider, not an
automatic helper pool. Discovery may query `/v1/models`, but Epoch does not
activate a model, broadcast work to several models, or send prompt/source data
until the operator confirms the exact model for the current run.

Use the selected endpoint for visible chat, bounded scene/GUI proposals, strict
source-context requests, and only source context the operator explicitly shares
from the reviewed path list. Existing-file proposals require exact counted
source evidence. Generic logger/singleton/entry rewrites, placeholders, stubs,
duplicate wrappers, unrelated cleanup, invented architecture, generated reasoning
text, and model-authored build/test claims are rejected rather than promoted into
chat, traces, evals, or source.

If a selected endpoint returns blank content, reasoning-only content, or stalls,
surface the failure and let the operator retry or select another model. Do not
silently harvest hidden reasoning or fan requests out to other discovered
models.

## Related docs

- `presets.md`
- `../engine/workflows.md`
- `../engine/captures.md`
- `../engine/ai_policy.md`
- `../../Changes/roadmap.md`
- `tools.md`
