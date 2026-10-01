# EpochEngine v0.90.02 - Self-Coding Observability And Sandbox Ownership

This source checkpoint improves Candidate Lab observability without changing the human Pass/Fail promotion policy.

## Diagnostics

- Candidate iteration is the human-review generation.
- Pass iteration counts coding/repair proposal passes within the current candidate generation.
- Total coding passes is cumulative since Start.
- Current pass age, six-stage workflow stage, repair attempt, context expansions, model corrections, controller generation, and sandbox materialized file/byte totals are visible in Candidate Lab.
- Internal controller resets do not reset the operator-facing counters; explicit Start/Restart does.

## Fast controller

The host-side System-1 remains deterministic source triage plus deterministic bounded planning. The selected coding model is reserved for source proposals and repairs. Compiler/test/runtime evidence drives subsequent repairs.

## Engine-owned source layout

Engine self-coding treats these as maintained in-tree EpochEngine source owners:

- `Engine/src`, `Engine/modules`, `Engine/include`, `Engine/resource`
- `Engine/dep/EpochGui`
- `Engine/dep/EpochEngineExtensions`
- `Engine/dep/EpochPackageDescriptors`

Candidate Lab materializes the complete `Engine` tree into the disposable sandbox while model context remains sparse/bounded. Admitted edits retain their repository-relative `Engine/...` path and are applied only inside the candidate sandbox. The model is explicitly instructed not to redirect in-tree component edits to external checkouts, absolute paths, temp paths, or live source.

## Verification note

Targeted source/invariant checks passed. The available Linux container cannot perform the authoritative C++23 module build because `clang-scan-deps` is unavailable. Project CMake/toolchain requirements were not weakened for the deliverable.
