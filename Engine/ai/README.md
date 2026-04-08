# Epoch AI Content

This folder stores repo-safe AI assets for the engine.

Tracked here:

- curated JSONL datasets
- dataset schemas
- eval cases
- tokenizer/manifests
- prompt templates

Do not commit:

- raw checkpoints
- quantized weights
- cache files
- compiled local model outputs that should ship only through releases or local installs

Compiled local-only outputs live under:

- `workspace/ai/checkpoints/`
- `workspace/ai/models/`
- `workspace/ai/cache/`

Staging captures can live under:

- `workspace/auto_train.jsonl`

That capture file is still Git-safe JSONL. It should be reviewed and either
kept as a staged training log or promoted into `Engine/ai/datasets/curated/`
instead of being treated like a binary artifact.

Current AI roles:

1. Embedded tiny Epoch model for in-engine English + C++ assistance
2. MCP-backed operating layer for retrieval, operations, and normalized capture
3. LM Studio teacher/oracle for bootstrapping, evals, accelerated editor help, and on-the-fly teaching of EpochBot
