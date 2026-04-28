# Epoch AI Content

This folder stores repo-safe AI assets for the engine.

Tracked here:

- curated JSONL datasets
- dataset schemas
- eval cases
- factory-loop contracts
- tokenizer/manifests
- prompt templates

Do not commit normal Git history with:

- raw checkpoints
- quantized weights
- cache files
- compiled local model outputs that should stay local or ship through releases

Compiled local-only outputs live under:

- `Engine/examples/ConsoleApplication1/workspace/ai/checkpoints/`
- `Engine/examples/ConsoleApplication1/workspace/ai/models/`
- `Engine/examples/ConsoleApplication1/workspace/ai/cache/`

Raw/staging capture can live under:

- `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl`
- `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl`

Those capture files are Git-safe JSON/JSONL, but they are still staging data.
Review them, promote only the intentional pieces into curated repo datasets or
evals, and delete outdated/bad artifacts when the training direction changes.

Current engine AI runtime roles:

1. internal EpochBot
2. local MCP/control bots that can operate and train EpochBot

External local LLMs such as LM Studio are development helpers for testing,
evaluation, curation, and iteration speed. They are not a third in-engine
runtime role.

## Factory Loop

The current Phase 5 foundation is an evidence-gated dark-factory loop, not
blind self-modifying autonomy:

1. planner produces or updates an explicit staged packet
2. executor proposes work only from the staged packet and current operator goal
3. builder creates fresh build evidence
4. verifier checks build/runtime/capture/eval output
5. gate promotes curated training/eval records or discards the attempt

The live contract is stored in:

- `Engine/ai/factory/continuous_build_loop.json`

The editor AI workspace now has a continuous build lane that watches active
project/script evidence, runs one child-project build at a time, and stages a
fresh packet after successful builds. That packet is the durable handoff into
future replay, verifier scoring, and curated training promotion.

The editor AI workspace also has an AI tool harness. It builds and runs the
selected tooling script through the real `EpochScriptHost`, captures before/after
editor state, records the result as MCP evidence, and stages a packet when the
tool action succeeds. This is the first bridge from "AI can talk about tooling"
to "AI can learn from an editor action that actually changed state."
