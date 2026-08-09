# Epoch AI Workspace

Epoch integrates operator-selected local or source-available models. It does not
contain, train, fine-tune, or self-modify an internal LLM.

## Implemented

- OpenAI-compatible local model discovery and chat.
- Direct `llama-cli` inference with a selected GGUF, captured output, timeout,
  cancellation/termination, and no server.
- Explicit model selection and executable-local runtime state.
- Asynchronous editor chat.
- Manual project creation, save, build, run, script build/run, diagnostics, and
  generated-project self-test paths.
- An operator-invoked Tool Harness that records before/after evidence.
- `ai.mcp` protocol types, bounded tool registry, capability/approval
  validation, cancellation, and build-safe contract proof.
- Session packet, tool-trace, eval, and review evidence.

## Not Implemented

- model training or weight mutation;
- automatic llama.cpp or GGUF download;
- an MCP network server;
- a complete model-generated tool-call parser/dispatcher loop;
- autonomous patch application, commit, push, or release;
- hidden continuous development or self-building.

## Repository Layout

- `prompts/`: tracked system prompts.
- `evals/`: deterministic behavior and safety cases.
- `manifests/`: provider and MCP protocol declarations.
- `control/continuous_build_loop.json`: legacy-path compatibility file that now
  describes the operator-gated planner/builder/verifier harness.
- `datasets/curated/`: reviewed fixtures/eval material only.

Local-only runtime state belongs under executable-local `cache/` and the
example workspace:

- `workspace/model_exchange.jsonl`;
- `workspace/tool_trace.jsonl`;
- `workspace/ai/sessions/`;
- `cache/models/`;
- `cache/ai/`.

Chat is not automatically captured. Explicit harness/trace actions own evidence.

See
[`os_ai_tooling_and_evidence_policy.md`](../docs/engine/os_ai_tooling_and_evidence_policy.md)
for permissions, MCP, project-operation, and Extensions rules.