# OS AI Tooling And Evidence Policy

This doc records the current working method for Epoch's operator-selected
open-source model lanes, editor tool loop, evidence capture, and promotion
gates. It replaces the older internal-bot/training framing: Epoch is not
shipping a personal bundled model as the core plan, and model weights are
operator-selected assets that download on demand.

## OS AI roles

Epoch documents three AI/control pieces. The model direction is OS/open-source
model integration, not internal bundled weights:

1. an engine-owned OS-model harness for memory, retrieval, planning, tool use,
   verification, evidence metrics, and dataset/eval gates
2. local MCP/control/tool harnesses that operate the editor and collect proof
3. operator-selected Qwen/Nemotron local model lanes for coding and review,
   with FLUX/Wan/TRELLIS tracked as package-managed creative model lanes

External local LLMs such as LM Studio are selected runtime/helper providers.
They can speed up testing, evaluation, curation, and documentation/build work,
but they are not hidden authority. Epoch does not treat bundled runtime model
weights as the practical path.

Canonical OS-model source pages:

- `https://huggingface.co/nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16`
- `https://huggingface.co/Qwen/Qwen3.6-27B`
- `https://huggingface.co/Wan-AI/Wan2.1-VACE-1.3B`
- `https://huggingface.co/microsoft/TRELLIS.2-4B`
- `https://huggingface.co/black-forest-labs/FLUX.2-klein-4B`

Model discovery is inventory only. Epoch may list available local models, but
it must not auto-name or activate one from discovery. The operator-selected
model is the only active runtime/helper target for in-editor calls unless a
phase explicitly allows extra helper lanes for drafting or review.

## Storage rules

Repo-safe:

- `Engine/ai/datasets/curated/`
- `Engine/ai/datasets/schema/`
- `Engine/ai/evals/`
- `Engine/ai/manifests/`
- `Engine/ai/prompts/`

Local-only compiled artifacts:

- executable-local `cache/models/` for on-demand OS model weights
- executable-local `cache/ai/` for local AI runtime cache
- `Engine/examples/ConsoleApplication1/workspace/ai/checkpoints/`
- `Engine/examples/ConsoleApplication1/workspace/ai/iterations/`
- `Engine/examples/ConsoleApplication1/workspace/research/staged/`

Use [research_import_and_promotion.md](research_import_and_promotion.md)
and [research_import.ps1](/C:/Users/iammi/.codex/worktrees/2a8f/epoch_vibed/Tools/research_import.ps1)
to stage PDFs, HTML, and notes there with provenance before they affect the
roadmap, datasets, or automation policy.

Raw/staging capture:

- `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl`
- `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl`

Staging captures are not curated truth. Review them, promote the good parts,
and delete outdated or bad training artifacts when the training direction
changes.

AI training, memory, and tool-loop changes are production engine work, not
throwaway experiments. A learning pass may be experimental in scope, but it must
still be bounded, reviewable, evidence-captured, reversible, and honest about
what was actually verified.

Iteration packets are staged truth only. They are useful because they bind one
task, one project snapshot, one model/provider snapshot, and one set of
evidence paths together before any later build/verify/promotion loop happens.

## Local self-iteration direction

Treat "self-rebuilding" as gated iteration over versioned artifacts rather than
as an unconstrained model rewriting itself.

- keep fast selected OS models available for bounded local engine tasks
- download Qwen/Nemotron weights only on demand into `cache/models/`; engine
  self-iteration may use selected models from cache or an already running local
  endpoint, but it must not clone or bundle those weights for routine engine
  iterations
- include model weights in generated projects only after explicit package
  opt-in plus license/notice review; otherwise projects should carry metadata
  and download recipes only
- use stronger on-demand local or hosted helpers for critique, labeling, and
  candidate generation when the operator allows them
- build toward an engine-owned harness around selected Qwen/Nemotron models from
  curated Epoch evidence, editor/tool traces, evals, and reviewable sandbox
  exercises rather than trying to ship a homemade bundled model runtime
- let the verifier own promotion decisions through build, runtime, and scenario
  evidence
- prefer adapters, prompts, datasets, tool schemas, and evals as the mutable
  artifacts instead of treating dense base weights as the first thing to rewrite

## Episode capture shape

When the MCP/control layer or other AI-assisted tooling performs bounded engine
work, the capture format should preserve enough structure to replay and score
that work later:

- task specification and constraints
- repo/toolchain/workspace/model snapshot
- MCP tool registry or schema snapshot
- interaction trace including tool calls, arguments, outputs, and failures
- verifier outputs such as build, tests, smoke, replay, or scenario results
- promotion/discard decision plus model lineage

The editor AI workspace now stages the first lightweight version of that under
`Engine/examples/ConsoleApplication1/workspace/ai/iterations/<packet>/` with:

- `iteration.json` for machine-readable provenance
- `task.md` for quick human/helper review
- current project/scene/script context
- current provider/model/manifest snapshot
- current raw capture / MCP capture / curated dataset / eval roots
- concrete evidence paths such as project manifest, build log, output target,
  script source, and scene path

## Working smoke pattern

1. Build `ConsoleApplication1 | Debug | x64`
2. Build `ConsoleApplication1 | Release | x64`
3. Launch from `x64/Debug/`
4. Confirm the selected helper model is logged
   - discovery may list models, but the engine should not activate one until
     the operator selects it
5. Submit at least one editor prompt and one C++ prompt
6. Confirm the reply is visible in the AI dock
7. Confirm raw/staged JSONL capture is updated
8. Confirm no checkpoint/model/cache paths are staged
9. Use engine-owned capture when the pass is user-visible
10. Close windows before finishing the pass

## Preferred smoke prompts

- `In Epoch editor, project 'Sandbox' has 6 entities. Suggest one concrete next edit and one gameplay follow-up.`
- `Explain why mixed C++23 module units should use module; before legacy includes.`
- `Summarize the current project runtime target and the active script in one short answer.`

Current helper availability is intentionally treated as dynamic. The exact names
vary with local load order, so the workflow rule is more important than the
pair itself: probe `/v1/models`, use only operator-allowed helpers for bounded
drafting/review, and keep the in-engine/runtime path disabled until the operator
selects the active model in the editor.

CLI and self-iteration runs can bind the same explicit selection with
`EPOCH_AI_MODEL`, `EPOCH_OPENAI_MODEL`, `LM_STUDIO_MODEL`, or `OPENAI_MODEL`
before launch. That is a deliberate operator override for a known local model
such as `nvidia/NVIDIA-Nemotron-3-Nano-4B-BF16`; it must not become an automatic
first-discovered-model selection path.

For the current April 2026 workstation passes, LM Studio can provide multiple
parallel helper lanes. Treat those lanes as drafting/review acceleration, not
as automatic OS AI model selection.

Use local helpers aggressively for:

- drafted reasoning on roadmap and architecture steps
- code-outline drafts for bounded editor/runtime subsystems
- documentation rewrites and smoke-prompt refinement
- screenshot/layout review when the helper supports multimodal analysis

Then refine the result locally and keep compile/build proof as the final source
of truth.

At the start of a phase, probe `/v1/models`, respect any already-stated helper
preference from the operator, and only ask which loaded models are allowed if
that allow-list is not already clear.

For the current `9900X` + `5800` workstation target, helper-first supervisor
passes may use up to five bounded LM Studio helper prompts when the operator has
allowed them. Keep the in-engine/runtime model operator-selected.

That helper-first check should happen at the start of a phase, not as an
afterthought once source edits are already underway.

When possible, send direct helper drafts through LM Studio `/v1/responses` or
`/v1/chat/completions` with bounded output tokens. If the selected model rejects
an explicit reasoning field, retry without it instead of treating the helper
path as broken.

The engine runtime itself must follow the selected-model rule: if a Responses
API call comes back empty because the selected model rejects the reasoning
configuration, retry without the reasoning field so the AI dock still shows a
visible answer.

Low-capacity helper replies are never promotion evidence by themselves. A reply
that does not cite a staged packet path, build log, output executable, verifier
result, runtime/capture evidence, or eval gate must be treated as a rejected
draft and kept out of curated training records.

The runtime gate for this policy is `--editor-ai-gate-self-test`. It should pass
before a helper model is trusted for a self-iteration review pass, and it should
fail any reply that says the engine is "working fine" without evidence, omits
child/self-test verifier proof, requests automatic promotion, or attempts to
create a model-accessible server/listener/bypass channel. It also verifies that
non-promotable assistant replies are blocked from capture promotion while an
evidence-backed final answer remains eligible. `--engine-validation-self-test`
wraps that AI gate with every registered editor project profile self-test, so a
single non-GUI command can prove the current project shell and OS-AI evidence
lanes before a source push.

Any generated app, server, listener, port bind, model-accessible control
surface, or hidden bypass channel must remain inert until an explicit human
enable/run action. Local game/tool tests are allowed through visible editor,
MCP, or harness controls when they are evidence-captured and do not expose a
new model-accessible network surface.

If a local helper returns useful-looking text only in `reasoning_content` while
`content` is blank, treat that as a model/API configuration failure for
user-visible OS AI. Hidden reasoning must not be surfaced as chat output and
must not be promoted into curated training data as an assistant answer. Retry
with a content-producing model/configuration, or keep the raw response only as
private diagnostic evidence explaining why the pass was rejected. The active
source line now also rejects non-promotable assistant text before local raw
training capture and MCP chat capture, including no-model/no-decode errors,
local API errors, hidden-reasoning-only replies, and obvious leaked reasoning
drafts.

## Evidence and evaluation metrics

Epoch's OS-model harness needs measurable evidence pressure before it can become
the engine's primary coding/runtime assistant. The current practical path is
Qwen/Nemotron model lanes supervised by stronger local or hosted helpers when
available, verified by tools, and promoted through evidence gates instead of
trust.

Use these metrics for every candidate model, adapter, prompt, tool schema, or
dataset promotion:

- gate accuracy
- false accept rate
- false reject rate
- evidence coverage across packet, build log, output, verifier, and capture
- tool trace coverage across action, arguments, output, errors, file diffs, and
  screenshots when available
- build pass rate
- runtime smoke pass rate
- self-iteration completion rate
- curated promotion rate
- server/bypass block rate
- regression rate after promotion
- time to verified patch

The current deterministic seed is `--editor-ai-gate-self-test`, which now logs
aggregate accept/reject, false-accept/false-reject, safety-block, average
evidence-score, and accuracy statistics. Keep that gate green before treating
Nemotron, Qwen, or any other selected OS model as a reviewer. Use
`--engine-validation-self-test` when project-shell build/run evidence must be
validated in the same pass.

Self-iteration is not complete just because a helper says it is. A completed
iteration needs the same packet to show: project manifest, build log, output
artifact, generated child self-test/verifier result, visible gate state, no
required `[missing]` markers, and a human-review hold before source or dataset
promotion.

As of May 21, 2026, use current public practice as guidance: explicit evals for
model/task behavior, tool-call traces as ground truth, preference or RL-style
post-training only from curated/verifiable data, and coding benchmarks as
external pressure tests. Epoch's local compiler, runtime, screenshot, and editor
evidence remain the final authority.

## Commit memory

- sync with `origin/main` when possible
- keep unrelated dirt out of the pass
- bump `Engine/modules/engine.version.ixx`
- use a descriptive commit title without baking the version number into the
  commit message
- document user-visible behavior in README/changelog/runtime docs
- prefer engine-owned capture over ad hoc desktop screenshots
- do not leave bad-folder logs or disposable debug junk behind
