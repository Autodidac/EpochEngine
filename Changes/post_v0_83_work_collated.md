# Epoch Post-v0.83.0 Work Collated

This file collates the active `v0.83.x` work log after the loose `release_notes_v*.md` series stopped at `v0.83.0`.

---

## [v0.83.59]
- Restored SDL and SFML dock takeover ownership so the real backend child window remains the active pane after docking settles, while the placeholder host stays available only as the container.
- Unified Vulkan clear color with the shared context palette so Vulkan no longer drifts visually from the other editor backends.
- Pulled the uploaded `botface.html` into repo history, then restored the cleaner UTF-8 working copy on top so the worktree and GitHub stay aligned without preserving the broken encoded variant.

---

## [v0.83.53]
- Added editor viewport drag-pan so held left mouse movement now slides the preview focus while wheel zoom and right-drag orbit remain active.
- Added a shared camera look-hit marker derived from the center view ray and projected it through the active preview backends so the current focus spot is visible in-editor.
- Tightened software preview invalidation and telemetry churn by tracking camera revisions directly and only re-emitting software framebuffer gauges when their values actually change.
- Captured a fresh 4K six-context editor proof from the asset-bearing local runtime for the README.

---

## [v0.83.52]
- Aligned the Windows resource move into `Engine/resource/` at the repo/build level, including the CMake resource path and the active Visual Studio shared-item surface.
- Refreshed the editor chrome toward a proper tool-style shell with `File`, `Edit`, `Asset`, `Window`, `Tools`, and `Help` plus mode/system tabs instead of fake project placeholders.
- Added mouse-wheel zoom for the editor preview camera and surfaced the live zoom value in the inspector/output panels.
- Captured and documented the refreshed multicontext editor run from the asset-bearing `x64/Debug` runtime so the README reflects the actual current local launch path.

---

## [v0.83.51]
- Kept the Windows multi-context host visible for docked SDL and SFML panes so backend child windows no longer disappear behind the editor host during local smoke runs.
- Improved Windows host/child handle matching in the multiplexer so resize, enqueue, cleanup, and dock layout logic follow the real live pane handle instead of only the original host HWND.
- Documented the asset-bearing output-folder launch path for multi-context smoke tests so local editor runs match the runtime asset layout used by the docked backends.

---

## [v0.83.4]
- Rolled `main` forward after the packaged `0.83.3` updater-shell release so the public binary can demonstrate the live fallback path into the newer source revision with the repaired runtime-version comparison.
- Fixed managed-vcpkg baseline rewriting inside updater sandboxes so the source fallback no longer walks up to the Epoch repo's own `HEAD` when it needs a vcpkg registry git revision.
- Made the source-update worker ignore any inherited `VCPKG_ROOT` and pin a worker-local managed toolchain instead, so updater rebuilds stay isolated from preexisting user vcpkg installs.

---

## [v0.83.3]
- Rolled `main` forward after the packaged `0.83.2` updater-shell hotfix release so the public binary can demonstrate the full release-to-source update path against a newer source revision.
- Trimmed the source-update manifest down to the graphics-only `SFML` and non-audio `raylib` feature set so updater-managed rebuilds no longer depend on the flaky `libogg`/`vorbis`/`openal` download chain.

---

## [v0.83.2]
- Fixed the updater worker's managed-vcpkg Git `HEAD` detection so the public shell can continue past managed tool bootstrap and reach the real restore/build/restart path during source fallback updates.

---

## [v0.83.1]
- Fixed the updater-shell source fallback so it always uses the updater-managed `vcpkg` toolchain instead of mutating any configured local `VCPKG_ROOT` checkout during the rebuild path.

---

## [v0.83.0]
- Shipped the first dedicated `Release|x64` updater-shell runtime as the downloadable `main.zip` payload, so first launch now lands directly on the minimal bootstrap UI instead of the full launcher surface.
- Replaced the source-updater dependency bootstrap with a managed toolchain flow that can download `vcpkg`, provision Git when needed, patch the `glad` port for current CMake, and rebuild from source without assuming a preinstalled development environment.
- Moved live source-update work into shorter managed work roots and kept the console/process output readable, which makes the shell demonstration path much easier to follow while leaving the rebuilt runtime path aimed at the full engine.

---

## [v0.83.54]
- Wired Win32 child-window wheel and text/key messages back into the editor GUI path so docked editor contexts can finally deliver wheel zoom and live AI chat input instead of relying on dead plumbing.
- Flattened the preview look-hit marker onto the grid plane and removed the floating camera-to-hit indicator so the center-look spot reads like an editor targeting aid instead of a hovering false collision.
- Replaced the placeholder asset/actions shell with real editor entity controls for adding meshes, lights, spawns, duplicating the current selection, deleting the current selection, and sending a live scene prompt into the AI panel.

