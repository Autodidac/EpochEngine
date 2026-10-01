# AI self-iteration predictor readiness — v0.90.03

This pass keeps Epoch self-iteration small, engine-local and removable while
preparing its real runtime evidence for a later EngCoder/JEV-style learned
controller.

## Current behavior

- The host remains authoritative for source containment, candidate lifecycle,
  writes, builds, tests, cancellation and human Pass/Fail.
- System-1 remains deterministic and cheap: source triage + bounded planning.
- The coding model remains responsible for source proposals and repair passes.
- Missing literal/line navigation is advisory and falls back to grounded source
  context rather than terminating the campaign.
- Reasoning-only responses receive bounded same-pass recovery instead of
  immediately killing Candidate Lab.
- Compiler/test failures repair the same candidate sandbox; human Fail also
  keeps that sandbox and opens another candidate iteration.
- Epoch-owned in-tree components keep their repository-relative paths, including
  EpochGui, EpochEngineExtensions and EpochPackageDescriptors.

## Predictor-ready analytics

Each Candidate Lab workspace now appends:

`logs/self_iteration_analytics.jsonl`

Schema: `epoch.self_iteration.analytics.v1`.

Records are emitted for System-1 triage/re-triage, coding-pass dispatch, source
patch application, navigation/reasoning recovery, repair requests, trusted
validation actors, candidate preview and human review. Features include:

- candidate/pass/total-pass counters;
- controller generation;
- reviewed/materialized file and byte counts;
- context expansions and recovery/correction counts;
- repair attempt count;
- deterministic System-1 top score, score margin and confidence;
- build/test/full-validation state;
- candidate-preview readiness;
- digest-only objective/status identity;
- reserved visual-evidence fields for later screenshot/image-derived features.

`predictor_score` is deliberately `null`. No learned predictor controls the
engine in v0.90.03.

## EngCoder design consideration

The supplied EngCoder 0.5.3 source was used only as design input for the small
pieces that fit this engine-local phase: a low-latency controller with explicit
confidence, deterministic fallback when no learned controller is ready, and
verified evidence suitable for later training/evaluation.

No EngCoder server, HTTP layer, model runtime, training worker, residency stack,
or external service dependency was copied into EpochEngine. A later integration
can train or replace the scoring/controller policy from the JSONL evidence
without changing the self-iteration safety boundary.

Human Pass/Fail is the strongest current outcome label. Compiler, contract,
HeadlessCI and full-validation actors provide deterministic intermediate labels.
Future image identification may add features to the reserved visual fields, but
visual analysis does not gate self-iteration today.
