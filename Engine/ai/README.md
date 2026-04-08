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
- auto-captured local training data

Local-only outputs live under:

- `workspace/auto_train.jsonl`
- `workspace/ai/checkpoints/`
- `workspace/ai/models/`
- `workspace/ai/cache/`

Current AI roles:

1. Embedded tiny Epoch model for in-engine English + C++ assistance
2. MCP-backed operating layer for retrieval, operations, and normalized capture
3. LM Studio teacher/oracle for bootstrapping, evals, and accelerated editor help
