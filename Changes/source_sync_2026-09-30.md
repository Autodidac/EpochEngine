# September 30 source synchronization — 0.90.33 preparation

## Scope

Integrated the operator's `EpochEngine-0.90.33-msvc-internal-entry-dual-build.zip`
into this chat's checkout. This includes the cumulative source-discovery AI
work, event-driven and neuromorphic rendering, focus-owned multicontext input,
scene selection/undo changes, persistent renderer telemetry, EpochGui updates,
MSVC internal-entry ownership, and the combined Debug+Release configuration.
Retained the four existing local fix commits after GitHub `3331f362`, including
long-path manifests, sandbox graphics linkage and six-second admission.

Supplied archive SHA-256:
`5df6727f89de6451b035c03a4f8626240fabf1eac8f3ddf980d104cebd605dd9`.

The archive's `AGENTS.md` is not substituted for governing instructions. The
active pass combines the current request with the prior acceptance limitations.
Source files missing from the snapshot are not implicitly deleted. Generated
Projects, runtime/cache output, ZIPs and unrelated local deletions are excluded.
The import staging and overwritten-file backups are disposable local evidence
under `build/import-20260930`, not tracked product source.

## Validation

- First-party source naming: passed, 610 files.
- Debug Editor build: passed, `build/import-20260930/debug-build.log`.
- Release Editor build: passed, `build/import-20260930/release-build.log`.
  Both normal x64 configurations link; the combined solution configuration and
  generated ProjectLauncher workflow were not exercised. Existing duplicate
  logger and optimization-override warnings remain.
- Build-safe pure engine contracts: completed with exit 7, not a pass. The
  September 30 run in `x64/Debug/logs/Engine.Editor.SelfTest.log` retains three
  failures: `ai.openai_source_iteration_request`, `ai.development_proposal_codec`
  (prompt contract line 458), and `ai.development_panel` (maximum-budget source
  and repair prompt continuity). These also failed before the 0.90.33 merge.
  New cumulative source-discovery checks pass. Preserve this exact evidence for
  a focused request-format/budget repair; do not loosen source edit admission or
  invent a successful candidate run to make this checkpoint appear green.
- GUI/model/candidate/runtime testing: not run; operator-owned.
- No public binary release, tag, stable-branch update or live-source promotion.

## Publication contract

Commit and fast-forward GitHub main without rewriting history. Give the Site
chat the exact pushed revision and validation status for an additive source
mirror. A source checkpoint does not admit replacement binary downloads or
claim self-coding/runtime acceptance.
