# AI Self-Iteration Repair — 2026-09-25

This patch repairs the existing Epoch Engine self-iteration pipeline without introducing a second controller architecture.

## Behavior

- Start now exposes model consent instead of silently parking deferred dispatch.
- A deterministic host source-triage pass is used first when the objective maps cleanly to owned source; ambiguous requests retain bounded model source selection.
- Candidate Lab uses a deterministic host implementation plan so the coding model spends its turns producing/repairing source changes rather than restating the workflow.
- One disposable candidate workspace is retained through compiler/test failures. Repairs read and modify the current candidate bytes instead of minting a fresh session.
- Source-context reselection changes only the model context and retains the current candidate filesystem.
- Proposal grounding during repair is checked against the current candidate snapshot instead of stale live-source bytes.
- Empty abandoned session shells are cleaned up; sessions containing real candidate files are preserved.
- Human Pass ends self-coding and preserves the fully validated candidate sandbox for manual use/review.
- Human Fail retires the preview and continues repairing the same candidate sandbox.
- Candidate review labels now describe Pass / Fail & Revise semantics directly.

## Qwen work retained

The useful parts of the later Qwen attempts were retained in corrected form: model-consent recovery, same-session repair intent, same-session source reselection, and session-backlog cleanup. Unsafe bounded eviction of completed candidates and the parallel experimental iteration-controller modules were not merged.

## Verification limitation

The source was structurally validated in the available Linux container. The authoritative Windows editor build could not be run here because this checkout requires its Windows/MSVC toolchain and the local container lacks the required C++ module dependency scanner / project CMake environment. No project CMake requirements were weakened in the delivered tree.
