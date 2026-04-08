# Epoch AI Content

This folder stores repo-safe AI assets for the engine.

Tracked here:

- curated JSONL datasets
- dataset schemas
- eval cases
- tokenizer/manifests
- prompt templates

Do not commit normal Git history with:

- raw checkpoints
- quantized weights
- cache files
- compiled local model outputs that should stay local or ship through releases

Compiled local-only outputs live under:

- `workspace/ai/checkpoints/`
- `workspace/ai/models/`
- `workspace/ai/cache/`

Raw/staging capture can live under:

- `workspace/auto_train.jsonl`
- `workspace/mcp_capture.jsonl`

Those capture files are Git-safe JSON/JSONL, but they are still staging data.
Review them, promote only the intentional pieces into curated repo datasets or
evals, and delete outdated/bad artifacts when the training direction changes.

Current engine AI runtime roles:

1. internal EpochBot
2. local MCP/control bots that can operate and train EpochBot

External local LLMs such as LM Studio are development helpers for testing,
evaluation, curation, and iteration speed. They are not a third in-engine
runtime role.
