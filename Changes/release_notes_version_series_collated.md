# Epoch Versioned Release Notes Collated

This file collates the loose `Changes/release_notes_v*.md` snapshots into one readable archive.

---

<!-- Source: release_notes_v0.82.17.md -->
# Epoch v0.82.17

## Highlights

- Consolidated the older versioned release-note markdowns into a single archive
  file so the `Changes/` folder keeps one historical notes surface while new
  releases continue as individual version files.
- Removed the accidentally tracked Windows `.exp` script-build artifact from
  the repo and added ignore coverage for `.exp` files and the local
  `Temp Script Test/` workspace.

## Verification

- Confirmed the accidental `.exp` artifact was removed from Git tracking
- Added ignore rules so the same class of local script-build outputs stops
  reappearing
- Refreshed the active release/version surfaces to `v0.82.17`

---

<!-- Source: release_notes_v0.82.18.md -->
# Epoch v0.82.18

## Highlights

- Fixed the remaining Windows editor script compiler spawn bug by using a
  direct executable launch for real LLVM paths instead of the PATH-search
  variant.
- This closes the last known `Program Files` path split that still produced
  `Files/LLVM/bin/clang++.exe` style failures during editor-run script builds.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Re-checked the compiler launch logic against the installed LLVM path
- Refreshed the active release/version surfaces to `v0.82.18`

---

<!-- Source: release_notes_v0.82.19.md -->
# Epoch v0.82.19

## Highlights

- Fixed the self-update flow so it now targets the currently running executable
  instead of the old hardcoded `updater.exe` replacement path.
- Added explicit failure reporting for update handoff/install failures so the
  app no longer appears to update successfully when the download or replacement
  step did not complete.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Re-ran the update code path against the rebuilt app
- Refreshed the active release/version surfaces to `v0.82.19`

---

<!-- Source: release_notes_v0.82.20.md -->
# Epoch v0.82.20

## Highlights

- Refreshed the active version, README, changelog, and current docs to
  `v0.82.20`.
- This bump exists specifically to give the self-update flow a fresh live
  target during manual updater validation.

## Verification

- Confirmed `main` was already current with `origin/main`
- Updated the active release/documentation surfaces to `v0.82.20`

---

<!-- Source: release_notes_v0.82.21.md -->
# Epoch v0.82.21

## Highlights

- Rolled the active version and current docs forward to `v0.82.21`
  immediately after the `0.82.20` release tag.
- This gives the self-update flow a newer target than the packaged
  `0.82.20` release snapshot.

## Verification

- Confirmed the `0.82.20` tag exists on `origin`
- Updated the active version/documentation surfaces to `v0.82.21`

---

<!-- Source: release_notes_v0.82.22.md -->
# Epoch v0.82.22 Release Notes

## Highlights

- Fixed the self-updater to target the shipped `main.zip` GitHub release asset.
- Updated the runtime replacement path so Windows installs from the packaged
  runtime directory instead of assuming GitHub serves a single replacement
  executable.
- Routed updater progress/status messages through Epoch logging so captured
  output no longer shows raw control-character glyphs at line endings.

## Technical Notes

- The built-in version check now points at
  `Engine/modules/aengine.version.ixx` on `main`.
- The release download target now points at
  `releases/latest/download/main.zip`.
- Windows update extraction now uses the platform archive tooling path instead
  of assuming a one-file executable swap.

---

<!-- Source: release_notes_v0.82.23.md -->
# Epoch v0.82.23 Release Notes

## Highlights

- Rolled the active version and current documentation forward immediately after
  the fixed `0.82.22` package tag.
- Keeps `main` ahead of the packaged release so the updater has a new target to
  detect during follow-up validation.

---

<!-- Source: release_notes_v0.82.24.md -->
# Epoch v0.82.24 Release Notes

## Highlights

- Fixed the updater to query the latest actual GitHub release instead of the
  `main` branch version file.
- Keeps version detection aligned with the downloadable `main.zip` release
  asset, which prevents `404` mismatches when `main` is ahead of the latest
  packaged release.

---

<!-- Source: release_notes_v0.82.25.md -->
# Epoch v0.82.25 Release Notes

## Highlights

- Rolled the active version and documentation forward after the fixed
  `0.82.24` full release.
- Keeps the working tree ahead of the packaged updater snapshot while the
  updater now correctly follows the latest actual GitHub release.

---

<!-- Source: release_notes_v0.82.26.md -->
# Epoch v0.82.26 Release Notes

## Highlights

- Restored visible console feedback for updater runs launched from the GUI.
- The updater now prints clear status lines even when no newer packaged release
  is available, instead of appearing to do nothing.

---

<!-- Source: release_notes_v0.82.27.md -->
# Epoch v0.82.27 Release Notes

## Highlights

- Rolled the active version and documentation forward after the `0.82.26`
  updater-feedback release.
- Keeps the repo ahead of the currently packaged release while the updater now
  follows the latest real GitHub release and prints visible status in the GUI
  path.

---

<!-- Source: release_notes_v0.82.28.md -->
# Epoch v0.82.28 Release Notes

## Highlights

- Cleaned up the editor update confirmation modal with separate actions for
  packaged runtime updates and source-snapshot downloads.
- Added a deliberate source-snapshot path for advanced testing when `main` is
  ahead of the latest packaged release.
- Source snapshot downloads now extract beside the runtime and do not replace or
  rebuild the running binary.

---

<!-- Source: release_notes_v0.82.29.md -->
# Epoch v0.82.29 Release Notes

## Highlights

- Rolled the active version and documentation forward after the `0.82.28`
  update-flow release.
- Keeps the live repo ahead of the packaged runtime while the new update modal
  and source-snapshot path are available in the downloaded build.

---

<!-- Source: release_notes_v0.82.30.md -->
# Epoch v0.82.30 Release Notes

## Highlights

- Restored the source-update path so it no longer stops at a downloaded source
  snapshot. It now restores manifest dependencies with `vcpkg`, rebuilds the
  runtime from the extracted tree, and replaces the running binary from that
  fresh build output.
- Fixed updater version comparison so local builds newer than the latest
  packaged release no longer attempt to downgrade themselves.
- Normalized updater console line output so captured update logs stop showing
  raw CR/LF glyphs at the end of status lines.

---

<!-- Source: release_notes_v0.82.31.md -->
# Epoch v0.82.31 Release Notes

## Highlights

- Rolled the active version and documentation forward after the `0.82.30`
  updater rebuild release.
- Keeps the live repo ahead of the packaged runtime so the updater can detect a
  newer development target after the released `main.zip`.

---

<!-- Source: release_notes_v0.82.32.md -->
# Epoch v0.82.32 Release Notes

## Highlights

- Fixed updater version checks to use unique temp files, eliminating the
  `remote_version.txt` file-in-use collision during packaged plus source probe
  runs.
- Kept source-snapshot update detection distinct from packaged-release
  detection, so the updater can report the two paths clearly.
- Cleaned the editor update confirmation modal so its content no longer bleeds
  into the title bar.

---

<!-- Source: release_notes_v0.82.33.md -->
# Epoch v0.82.33 Release Notes

## Highlights

- Rolled the active version, docs, and README snapshot forward after the
  packaged `0.82.32` release.
- Keeps `main` ahead of the downloadable runtime so source-update checks can
  still see a newer live target.

---

<!-- Source: release_notes_v0.82.34.md -->
# Epoch v0.82.34 Release Notes

## Highlights

- Removed the updater module's extra direct console output path.
- Updater status now relies on the engine logging path so the commandline view
  stops duplicating updater lines.

---

<!-- Source: release_notes_v0.82.35.md -->
# Epoch v0.82.35 Release Notes

## Highlights

- Rolled the active version, docs, and README snapshot forward after the
  packaged `0.82.34` release.
- Keeps `main` ahead of the downloadable runtime so source-update checks can
  still see a newer live target.

---

<!-- Source: release_notes_v0.82.36.md -->
# Epoch v0.82.36 Release Notes

## Highlights

- Restored a single visible updater status stream for the runtime update flow.
- Direct updater console output now only activates when the engine logger is not
  already configured to own console output.

---

<!-- Source: release_notes_v0.82.37.md -->
# Epoch v0.82.37 Release Notes

## Highlights

- Rolled the active version, docs, and README snapshot forward after the
  packaged `0.82.36` release.
- Keeps `main` ahead of the downloadable runtime so source-update checks can
  still see a newer live target.

---

<!-- Source: release_notes_v0.82.38.md -->
# Epoch v0.82.38

## Highlights

- Fixed the updater fallback so a confirmed update now rebuilds from source when `main` is newer and no newer packaged runtime exists.
- Removed the duplicate source-version probe so the updater stops printing the same source comparison twice before a source rebuild.
- Simplified updater status reporting to one direct console stream during update work, which keeps source/package update progress visible without the earlier doubled logic.

## Notes

- Packaged runtime releases still ship as `main.zip`.
- `main` will continue to move ahead after this release so source-update checks can detect newer source snapshots between packaged drops.

---

<!-- Source: release_notes_v0.82.39.md -->
# Epoch v0.82.39

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.38` release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the post-release target version.

## Notes

- The packaged updater release is `0.82.38`.
- The active source target on `main` is now `0.82.39`.

---

<!-- Source: release_notes_v0.82.40.md -->
# Epoch v0.82.40

## Highlights

- Routed updater status back through Epoch logging with sanitized updater messages so the commandline pane stops showing the raw `â™ªâ—™` newline artifacts.
- Kept the newer-source fallback intact so a confirmed update still rebuilds from source when no newer packaged runtime exists.
- Repackaged the runtime drop as a fresh `main.zip` release for updater testing.

## Notes

- Packaged runtime releases still ship as `main.zip`.
- `main` will move ahead again after this release so source-update checks keep a newer live target.

---

<!-- Source: release_notes_v0.82.41.md -->
# Epoch v0.82.41

## Highlights

- Repaired the Windows updater handoff so packaged and source updates explicitly replace the runtime executable instead of silently leaving the old binary in place.
- Kept the updater logging path sanitized so updater status stays readable in the commandline pane while the handoff runs.

## Notes

- This packaged updater release is `0.82.41`.
- The next `main` bump will move ahead again after the release so source-update checks keep a newer live target.

---

<!-- Source: release_notes_v0.82.42.md -->
# Epoch v0.82.42

## Highlights

- Repaired the Windows updater handoff batch by switching to proper batch variable quoting and explicit executable replacement.
- Added `epoch_update_handoff.log` in the runtime folder so failed packaged/source handoffs leave behind a concrete trail instead of silently stalling.

## Notes

- This packaged updater release is `0.82.42`.
- The next `main` bump will move ahead again after the release so source-update checks keep a newer live target.

---

<!-- Source: release_notes_v0.82.43.md -->
# Epoch v0.82.43

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.42` release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.42`.
- The active source target on `main` is now `0.82.43`.

---

<!-- Source: release_notes_v0.82.44.md -->
# Epoch v0.82.44

## Highlights

- Replaced the Windows updater `system()/cmd/start` flow with hidden process launches for `vcpkg`, `MSBuild`, and the handoff batches so source updates stop opening stray developer prompts.
- Added `epoch_source_update.log` next to `epoch_update_handoff.log` so failed source rebuilds and failed runtime replacement steps leave concrete diagnostics in the runtime folder.
- Fixed the updater's live source-version path to use `Engine/modules/aengine.version.ixx`, which keeps source update checks aligned with the actual repo version file.

## Notes

- This release is intended to repair both the source-update rebuild path and the final replacement handoff from a downloaded runtime.

---

<!-- Source: release_notes_v0.82.45.md -->
# Epoch v0.82.45

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.44` updater repair release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.44`.
- The active source target on `main` is now `0.82.45`.

---

<!-- Source: release_notes_v0.82.46.md -->
# Epoch v0.82.46

## Highlights

- Source updates continue to download the repository snapshot directly from `main`, not from GitHub releases.
- The source updater now does the restore/build sequence the safer way: restore manifest dependencies first, then retry the compile pass once before giving up.
- The runtime still leaves `epoch_source_update.log` and `epoch_update_handoff.log` behind for rebuild and handoff diagnostics.

## Notes

- This release is aimed specifically at the downloaded-runtime source update path, not the packaged binary update path.

---

<!-- Source: release_notes_v0.82.47.md -->
# Epoch v0.82.47

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.46` updater repair release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.46`.
- The active source target on `main` is now `0.82.47`.

---

<!-- Source: release_notes_v0.82.48.md -->
# Epoch v0.82.48

## Highlights

- Windows source updates now hand off to a detached worker process instead of trying to finish the download, restore, build, and replacement inline inside the running app.
- The worker keeps using the direct `main.zip` repository snapshot, restores manifest dependencies first, retries the MSBuild pass after restore, and waits long enough for the runtime handoff to complete cleanly.
- Source updates stay visible by default, and can be run silently on demand with `EPOCH_UPDATER_SILENT=1`.

## Notes

- Runtime-side diagnostics remain in `epoch_source_update.log` and `epoch_update_handoff.log`.
- The packaged updater asset is still published as `main.zip`.

---

<!-- Source: release_notes_v0.82.49.md -->
# Epoch v0.82.49

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.48` detached-worker updater release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.48`.
- The active source target on `main` is now `0.82.49`.

---

<!-- Source: release_notes_v0.82.50.md -->
# Epoch v0.82.50

## Highlights

- Fixed the Windows source updater handoff so the detached worker now resolves the real live executable path, waits for the runtime to exit, replaces it from the rebuilt `x64/Debug` output, and relaunches the updated app cleanly.
- Moved native `vcpkg` and `MSBuild` output into updater log files so restore/build activity no longer dumps raw junk into the live console.
- Replaced hardcoded machine-local sample-project dependency paths with repo-relative vcpkg include/library locations so source snapshot builds are portable across machines.

## Notes

- The packaged updater release is `0.82.50`.
- The active source target on `main` is now `0.82.50`.

---

<!-- Source: release_notes_v0.82.51.md -->
# Epoch v0.82.51

## Highlights

- Rolled the active repo surfaces forward after the packaged `0.82.50` updater release so `main` stays ahead of the downloadable runtime again.
- Kept the README snapshot and current docs aligned with the new post-release source target.

## Notes

- The packaged updater release is `0.82.50`.
- The active source target on `main` is now `0.82.51`.

---

<!-- Source: release_notes_v0.83.0.md -->
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

