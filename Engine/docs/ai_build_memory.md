# AI Build And Smoke Memory

This doc records the current working method for Epoch's AI training and smoke
loop.

## Engine AI roles

Epoch documents two engine runtime AI roles:

1. internal EpochBot
2. local MCP/control bots that can operate and train EpochBot

External local LLMs such as LM Studio are development helpers. They can speed
up testing, evaluation, curation, and documentation/build work, but they are
not a third in-engine runtime role.

## Storage rules

Repo-safe:

- `Engine/ai/datasets/curated/`
- `Engine/ai/datasets/schema/`
- `Engine/ai/evals/`
- `Engine/ai/manifests/`
- `Engine/ai/tokenizer/`
- `Engine/ai/prompts/`

Local-only compiled artifacts:

- `workspace/ai/checkpoints/`
- `workspace/ai/models/`
- `workspace/ai/cache/`

Raw/staging capture:

- `workspace/auto_train.jsonl`
- `workspace/mcp_capture.jsonl`

Staging captures are not curated truth. Review them, promote the good parts,
and delete outdated or bad training artifacts when the training direction
changes.

## Working smoke pattern

1. Build `ConsoleApplication1 | Debug | x64`
2. Build `ConsoleApplication1 | Release | x64`
3. Launch from `x64/Debug/`
4. Confirm the selected helper model is logged
   - use the first model returned by `/v1/models` so validation does not
      trigger extra model loads
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

Current validated fast helper baseline:

- `qwen/qwen3.5-9b`

Stronger local helpers can be used when loaded, but the fast baseline is still
useful for repeated smoke passes.

Use local helpers aggressively for:

- drafted reasoning on roadmap and architecture steps
- code-outline drafts for bounded editor/runtime subsystems
- documentation rewrites and smoke-prompt refinement
- screenshot/layout review when the helper supports multimodal analysis

Then refine the result locally and keep compile/build proof as the final source
of truth.

When two local helper models are loaded, helper-first supervisor passes should
use the first two `/v1/models` entries as two drafting pools and can fan out up
to four parallel prompts per model for bounded tasks. Keep the first detected
model as the in-engine/runtime parity baseline.

That helper-first check should happen at the start of a phase, not as an
afterthought once source edits are already underway.

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
- use a versioned commit title
- document user-visible behavior in README/changelog/runtime docs
- prefer engine-owned capture over ad hoc desktop screenshots
- do not leave bad-folder logs or disposable debug junk behind
