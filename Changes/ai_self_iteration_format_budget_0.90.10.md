# EpochEngine 0.90.10 - Self-Iteration Format Budget

- Source-packet correction attempts are now scoped to the coding pass instead of each HTTP request.
- The old `1/2 -> 2/2 -> new request -> 1/2` reset loop is removed.
- After two malformed proposal corrections, Epoch preserves the same candidate sandbox and opens a bounded recovery pass.
- At most two recovery passes are allowed for one consecutive format-failure streak; exhausting them stops self-coding cleanly and preserves the candidate for diagnosis.
- A successfully admitted source proposal clears the consecutive format-recovery streak.
- Harness Diagnostics now exposes the active format-recovery count.
- Provider-drop, reasoning-only, compiler/test repair, source-navigation, and human Candidate Lab behavior remain separate from format recovery.
