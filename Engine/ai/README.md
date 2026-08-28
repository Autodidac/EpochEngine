# Epoch AI Workspace

Epoch integrates operator-selected external or source-available models and an
optional verified Epoch-local Qwen3.8 installation. It does not train,
fine-tune, silently activate, or self-modify any model.

## Implemented

- OpenAI-compatible model inventory, explicit per-session selection, and
  asynchronous chat. The endpoint may run on an operator-managed external
  machine while Epoch retains MCP tool authority and validation.
- Direct `llama-cli` inference with an operator-selected GGUF, captured output,
  timeout, cancellation, termination, and no server.
- A pinned, operator-invoked Epoch-local installer for llama.cpp `b10516` and
  the community `Qwen3.8-27B-UD-Q4_K_M.gguf` quantization. Exact hashes,
  revision, model size, and executable-local receipts gate readiness.
- Generated-project `epoch.project.ai.v1` profiles with three explicit modes:
  Off, shared Epoch-local Qwen3.8, or an operator-managed external model
  endpoint using the same Epoch MCP guards. New projects default to Off, copy
  only the profile into build output, and never bundle weights.
- One movable AI Controls pane for model consent, single-command scene/GUI
  authoring, and guarded Engine Development; AI Chat remains a separate movable
  conversation pane.
- `EPOCH_AUTHORING_PLAN_V1` parsing with exactly one allowlisted semantic command
  per approval, canonical scene inventory, stable object identity, support
  placement, visible Apply/Discard, and persistent Play/Pause/Edit/Delete goals.
- `EPOCH_TOOL_PLAN_V1` parsing for exactly one argument-free active-project
  inspect, save, build, run, test, or diagnostics proposal. AI Chat `/tool`
  keeps approval attached to the parsed response; the trusted host revalidates
  it through the MCP registry and invokes only canonical project owners. The
  model receives no path, native command, permit, Git, network, release, updater,
  or self-approval authority.
- `EPOCH_SOURCE_CONTEXT_REQUEST_V1` path validation and passive review, plus an
  explicit Share Requested Context action that reads only the displayed UTF-8
  files after canonical-root, unchanged-objective, and 184 KiB budget checks.
  Exact counted contents or absent-path evidence are request-local and sent only
  to the displayed selected endpoint; no directory scan or automatic handoff runs.
- Strict grounded `EPOCH_SOURCE_PROPOSAL_V1` parsing. Existing-file proposals
  require exact source evidence, while generic logger/singleton/entry rewrites,
  placeholders, stubs, duplicate wrappers, unrelated cleanup, invented
  architecture, and validation claims fail before staging. A deterministic
  quality gate also rejects no-op replacements, `.ixx` textual includes,
  destructive whole-file shrinkage, removed license/module/namespace ownership,
  and proposals whose generic subsystem does not match the current objective.
- AI Controls owns development objectives and read-only request/evidence detail.
  Review, Share/Reject, and Approve/Cancel remain attached to the originating AI
  Chat response; changing the objective invalidates previously reviewed source
  evidence.
- Disposable source-iteration sandboxes with digest-bound review, approval,
  single-use permits, exact-content transactions, rollback evidence, and a
  separate two-action verified live-source promotion transaction after Debug and
  Release compiler plus contract-test success and a distinct HeadlessCI Debug
  build plus asset-light run, followed by separately operator-approved full
  project-profile, generated-child, and AI-gate validation.
- Manual project creation, save, build, run, script build/run, diagnostics, and
  generated-project self-test paths.
- An operator-invoked Tool Harness that records explicit before/after evidence.
- `ai.mcp` protocol types, bounded tool registry, capability/approval validation,
  cancellation, budgets, and build-safe contract proof.
  Source proposals may now contain one to four related exact-file operations in
  one immutable generation, allowing interface/implementation/build/test changes
  to travel together while retaining per-file preimages, one digest, and one
  visible operator approval.
  Malformed, ungrounded, or deterministic quality-gate failures can trigger at
  most two host-diagnosed packet-correction requests before stopping for
  operator refinement. These retries stage no source and cannot bypass proposal
  review or exact digest approval.
- `ai.iteration_session` now coordinates one typed, non-GUI source candidate
  across a verified source authority, current-byte curated file SHA-256 values,
  explicit context sharing, manual candidate approval, disposable execution,
  bounded repair identity, and seven trusted Debug/Release/Headless/full-validation
  actors. Validation evidence names the exact candidate digest. Source authority
  resolves an explicit checkout with a readable Git identity first, then an
  authenticated cached-source receipt, and otherwise fails visibly. In both
  accepted cases the files actually shared are freshly hashed from current
  bytes; neither a checkout commit nor an archive receipt claims the extracted
  working tree stayed immutable.
- Ambiguous host curation now returns visible `selection_required` and sends no
  bytes. Engine Development no longer asks a model to invent source paths.
  Live-source promotion, Git, release, network, and self-approval authority stay
  outside the coordinator.- A contract-proven `ai.iteration_loop` risk/milestone state machine plus one
  contained Engine Development production slice: Qwen3.8-class proposal,
  digest approval, exact-copy sandbox application, hidden Debug and Release
  compiler passes, build-safe engine contract tests for both configurations, a
  distinct HeadlessCI Debug build and asset-light run, an explicitly approved
  full engine validation, and at most three fresh-generation repairs from
  bounded diagnostics. `Stage Live Promotion` verifies exact live and sandbox
  evidence plus a freshly reparsed
  identical operation set; a separate `Approve Live Promotion` rechecks and
  atomically applies that exact source.
- One-owner asynchronous sandbox compiler/contract, HeadlessCI, and approved
  Diagnostics do not consume its future, apply source edits, or create training records.

## Not Implemented

- model training, weight mutation, or automatic dataset promotion;
- automatic/background llama.cpp or GGUF download; installation remains an
  explicit operator-invoked action;
- an MCP network server or hidden listener;
- unattended autonomous iteration or chained/multi-call model tool execution;
- additional test-suite, static-analysis, sanitizer, architecture-review,
  visual-harness, and frontier-review host adapters for `ai.iteration_loop`;
- autonomous patch application to live source, commit, push, or release;
- hidden continuous development or self-building.

## Repository Layout

- `prompts/`: tracked system prompts.
- `evals/`: deterministic behavior and safety cases.
- `evals/fixtures/`: explicitly reviewed non-training fixtures and expected
  results.
- `manifests/`: provider and MCP protocol declarations.
- `control/continuous_build_loop.json`: legacy path retained for the headless
  contract probe; its content defines the operator-gated planner/builder/verifier
  policy and does not enable a continuous background loop.

Local-only runtime state belongs under executable-local `cache/` and the example
workspace:

- `workspace/model_exchange.jsonl` for explicitly retained model exchanges;
- `workspace/tool_trace.jsonl` for explicit structured tool evidence;
- `cache/models/` for operator-managed model assets;
- `cache/ai/` for disposable AI and iteration cache.

Normal chat, build results, and tool results are not automatically captured,
promoted, or treated as training data. Explicit harness, trace, and reviewed eval
fixture actions own retained evidence.

See
[`os_ai_tooling_and_evidence_policy.md`](../docs/engine/os_ai_tooling_and_evidence_policy.md)
for permissions, MCP, source context, project-operation, and Extensions rules.