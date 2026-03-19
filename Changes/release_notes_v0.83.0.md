# Epoch v0.83.0 Complete Release Notes

## Summary

`v0.83.0` is the release-notes consolidation for the long updater and launcher
repair arc that ran from `v0.82.17` through the current tree. The headline
feature is the new updater shell: a dedicated bootstrap runtime that can ship
as a tiny Windows package, present one clear update action, hand control to the
console-driven updater, rebuild from source when `main` is newer than the last
packaged release, replace the local runtime, and relaunch into the updated
engine.

This document compresses the scattered point releases in that range into one
story that can be read as a single milestone instead of dozens of hotfix notes.
It also marks the first public binary release where the downloadable runtime is
the updater shell itself: a small `Release|x64` bootstrap build that can update
forward into the current full engine/editor.

## Headline Outcome

- Epoch now has a working release-to-source update path on Windows.
- Packaged releases can remain stable while `main` moves ahead for active
  development.
- The updater can detect when there is no newer packaged runtime and then fall
  through to the newer `main` source snapshot automatically.
- The updater shell gives new users a simple, obvious entry point into the
  engine/editor without exposing the full launcher surface first.

## What Changed Across v0.82.17 to Current

### 1. Release surfaces and version metadata were cleaned up

- The repo moved to a more consistent release-note structure, starting with the
  `v0.82.17` documentation cleanup and continuing through repeated version-surface
  refreshes after packaged releases.
- Active version metadata, README snapshots, docs, and release notes were kept
  aligned so the updater had a reliable newer source target to detect.
- Repo-relative paths replaced machine-local assumptions in the example project
  build surfaces, which was necessary for source snapshot rebuilds to work on
  other machines.

### 2. Packaged update detection was repaired

- The updater was moved onto the real GitHub release flow instead of stale
  executable URLs or mismatched source probes.
- Version detection was corrected so packaged runtime checks compare against the
  latest GitHub release, while source checks compare against
  `Engine/modules/aengine.version.ixx` on `main`.
- Temporary-file handling was hardened so sequential packaged/source checks do
  not trip over shared files or stale probe results.

### 3. Source fallback became a real rebuild path

- When `main` is newer than the latest packaged release, Epoch can now download
  the current source snapshot, restore dependencies with managed tooling, build
  `ConsoleApplication1`, and replace the local runtime from
  `<source-root>/x64/Release`.
- The Windows update flow was moved into detached worker/handoff stages so the
  rebuild and replacement can continue after the live runtime exits.
- `epoch_source_update.log` and `epoch_update_handoff.log` now leave behind a
  concrete trail for failed restore/build/replacement steps instead of silently
  stalling.
- Managed updater dependencies now include the ability to provision `vcpkg`
  and Git automatically for end users instead of assuming an existing dev
  machine setup.
- The updater also stages a patched `glad` overlay port so the source rebuild
  path survives current CMake policy behavior cleanly.

### 4. Handoff and replacement became reliable

- The updater now resolves the actual running executable path before replacement
  instead of relying on fragile startup-path assumptions.
- Hidden process launches replaced shell-heavy `system()` calls for restore,
  build, and handoff work.
- Runtime unlock waits and build retry behavior were strengthened so the final
  copy/restart step is less timing-sensitive.

### 5. Console and diagnostics readability improved

- Raw updater junk, duplicated console lines, and control-character artifacts
  were removed or redirected into log files.
- Command-line logging was collapsed into cleaner single-line summaries.
- Startup spam from backend confirmation, atlas/menu churn, and third-party
  initialization noise was reduced substantially.
- Missing-font failures in the GUI path were throttled so incomplete payloads do
  not spam the console every frame.

### 6. Launcher and editor presentation improved

- The launcher/editor split was clarified so project selection, game launching,
  and tool entry live in the launcher while the editor stays focused on editing.
- Visible version strings were restored in the launcher and editor about flows.
- GUI wrapping and description readability were improved so longer status text
  remains readable in the shell and launcher views.

### 7. The updater shell was introduced and stabilized

- Epoch now ships a dedicated updater-shell build with a minimal package shape:
  the runtime executable, required release DLLs, and font assets.
- The shell is software-only, intentionally avoiding the heavier mixed-backend
  path used by the full engine launcher.
- Pressing the update button no longer blocks inside the shell render loop. The
  shell window closes first, then the console-driven update work continues in
  process, which avoids the hang/close crash path seen in earlier iterations.
- The public binary can now boot straight into updater-shell mode by default,
  which makes the downloadable `main.zip` package a focused update/bootstrap
  surface instead of a full launcher drop.
- The live release flow has been demonstrated end to end:
  packaged release -> source fallback -> source rebuild -> runtime replacement
  -> automatic relaunch.

## Why The Updater Shell Matters

- It gives new users a cleaner first-run entry point than the full launcher.
- It demonstrates Epoch's self-update story with a minimal UI surface.
- It reduces the amount of runtime state that can interfere with binary
  replacement on Windows.
- It gives the project a stable bootstrap package that can point users at the
  current source even when there is no newer full release yet.

## Verification Snapshot

- Packaged updater releases now ship as `main.zip` assets containing only the
  updater-shell runtime, required DLLs, and font assets.
- Manual close of the updater shell exits cleanly.
- Live release builds have been tested in external sandboxes against newer
  source revisions on `main`.
- The public flow now succeeds with:
  packaged version detection,
  source fallback when appropriate,
  detached rebuild/handoff,
  binary replacement,
  and relaunch into the updated runtime.

## Known Follow-Up Areas

- The source rebuild path still shows a noisy first failed MSBuild attempt in
  some logs before the successful retry; the update still completes, but the log
  presentation can be cleaned further.
- The stripped-down updater shell is now stable enough to serve as the public
  bootstrap runtime, but the broader launcher/editor UX can still be made more
  modular and more polished.
- The long hotfix trail from `0.82.x` is now ready to roll into a cleaner
  `0.83.0` era with fewer repair releases and more feature-focused milestones.
