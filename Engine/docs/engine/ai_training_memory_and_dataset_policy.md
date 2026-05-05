# AI Training Memory And Dataset Policy

This doc records the current working method for Epoch's AI training and smoke
loop.

## Engine AI roles

Epoch documents three internal AI/control pieces:

1. EpochBot, the primary engine-owned trainable LLM/runtime path
2. local MCP/control/tool harnesses that operate the editor and collect proof
3. an offline/injectable OSS or tiny backup LLM path for fallback, generated
   software embedding, and EpochBot training support

External local LLMs such as LM Studio are selected development helpers. They can
speed up testing, evaluation, curation, and documentation/build work, but they
are not hidden authority and are not the same thing as the internal backup LLM
path.

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
- `Engine/ai/tokenizer/`
- `Engine/ai/prompts/`

Local-only compiled artifacts:

- `Engine/examples/ConsoleApplication1/workspace/ai/checkpoints/`
- `Engine/examples/ConsoleApplication1/workspace/ai/iterations/`
- `Engine/examples/ConsoleApplication1/workspace/ai/models/`
- `Engine/examples/ConsoleApplication1/workspace/ai/cache/`
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

Iteration packets are staged truth only. They are useful because they bind one
task, one project snapshot, one model/provider snapshot, and one set of
evidence paths together before any later build/verify/promotion loop happens.

## Local self-rebuilding direction

Treat "self-rebuilding" as gated iteration over versioned artifacts rather than
as an unconstrained model rewriting itself.

- keep a fast seed/runtime model available for always-on local engine tasks
- use stronger on-demand teacher/helper models for critique, labeling, and
  candidate generation
- build toward an engine-owned LLM plus backup tiny internal model from curated
  Epoch evidence, editor/tool traces, evals, and reviewable sandbox exercises
  rather than assuming any external helper is the final runtime brain
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

For the current April 2026 workstation passes, LM Studio can provide multiple
parallel helper lanes. Treat those lanes as drafting/review acceleration, not
as automatic EpochBot model selection.

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

Any generated app, server, listener, port bind, model-accessible control
surface, or hidden bypass channel must remain inert until an explicit human
enable/run action. Local game/tool tests are allowed through visible editor,
MCP, or harness controls when they are evidence-captured and do not expose a
new model-accessible network surface.

If a local multimodal helper returns its useful answer in `reasoning_content`
while `content` is blank, treat that as a tooling/parsing issue in the helper
path rather than assuming the model had nothing useful to say.

The same rule applies to local Qwen helper drafting in general: if the useful
draft landed in `reasoning_content`, harvest it and move on instead of wasting a
phase waiting for a cleaner helper reply.

## Commit memory

- sync with `origin/main` when possible
- keep unrelated dirt out of the pass
- bump `Engine/modules/aengine.version.ixx`
- use a descriptive commit title without baking the version number into the
  commit message
- document user-visible behavior in README/changelog/runtime docs
- prefer engine-owned capture over ad hoc desktop screenshots
- do not leave bad-folder logs or disposable debug junk behind
