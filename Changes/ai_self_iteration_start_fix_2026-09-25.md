# AI self-iteration Start fix — 2026-09-25

- Self-coding now accepts a structurally valid extracted/local Epoch source snapshot even when `.git` metadata is absent.
- The local snapshot authority is digest-bound and curated source files remain rehashed before use.
- `local_snapshot` authority is propagated through iteration-session, campaign, MCP, editor, and panel validation.
- The self-coding panel now displays its internal status message directly, so admission/materialization failures are visible instead of looking like a dead Start button.
- Existing same-candidate repair/session fixes from the prior AI-iteration repair are retained.
