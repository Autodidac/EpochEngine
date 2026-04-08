# AI Build And Smoke Memory

This doc records the current working method for Epoch's three-role AI setup.

## AI roles

1. Embedded tiny Epoch model for local English + C++ assistance
2. MCP-backed operating layer for retrieval, operations, and normalized capture
3. LM Studio teacher/oracle for evals, bootstrapping, and accelerated editor help

## Storage rules

Repo-safe:

- `Engine/ai/datasets/curated/`
- `Engine/ai/datasets/schema/`
- `Engine/ai/evals/`
- `Engine/ai/manifests/`
- `Engine/ai/tokenizer/`
- `Engine/ai/prompts/`

Local-only:

- `workspace/auto_train.jsonl`
- `workspace/ai/checkpoints/`
- `workspace/ai/models/`
- `workspace/ai/cache/`

`append_training_sample(...)` is raw local capture. It does not directly create
curated repo training data.

## Working smoke pattern

1. Build `ConsoleApplication1 | Debug | x64`
2. Build `ConsoleApplication1 | Release | x64`
3. Launch from `x64/Debug/`
4. Confirm the selected LM Studio model is logged
5. Submit at least one editor prompt and one C++ prompt
6. Confirm the reply is visible in the AI dock
7. Confirm `workspace/auto_train.jsonl` is updated
8. Confirm no checkpoint/model paths are staged
9. Capture proof with the engine-owned capture path when the pass is user-visible
10. Close windows before finishing the pass

## Preferred smoke prompts

- `In Epoch editor, project 'Sandbox' has 6 entities. Suggest one concrete next edit and one gameplay follow-up.`
- `Explain why mixed C++23 module units should use module; before legacy includes.`
- `Summarize the current project runtime target and the active script in one short answer.`

## Commit memory

- sync with `origin/main` when possible
- keep unrelated dirt out of the pass
- bump `Engine/modules/aengine.version.ixx`
- use a versioned commit title
- document user-visible behavior in README/changelog/runtime docs
- prefer engine-owned capture over ad hoc desktop screenshots
