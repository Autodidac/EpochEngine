# AI Source Discovery Architecture

Epoch Engine Self-Coding uses a host-owned retrieval layer. The model does not receive the whole repository and does not gain arbitrary filesystem authority.

## Flow

1. Build a read-only repository index for the selected source area.
2. Rank a compact repository map and seed likely source using the operator objective.
3. Add exact reviewed source to a cumulative per-iteration workspace.
4. When more evidence is needed, perform bounded needle searches over path, identifier, text, import/reference relationships or a requested line/literal window.
5. Append newly verified evidence to the workspace; do not replace earlier reviewed evidence.
6. Pack only the currently useful exact evidence into the model prompt within the active budget.
7. Permit edits only against REVIEWED_SOURCE_ID values backed by exact bytes supplied in that patch request.
8. Stage edits in the disposable sandbox and retain the existing build/test/review/promotion gates.

## Budgets

The workspace is bounded by bytes/tokens, not a small operational file count. Individual navigation requests stay small so one model call cannot explode the working set. When LM Studio reports the loaded context length, Epoch may use the additional capacity as discovery grows; it does not fill the context window by default.

The loader allocates from actual bounded file-size demand rather than splitting
the source budget equally among all paths. It reserves useful minimum windows,
completes small declarations, gives current requested paths surplus first, then
distributes remaining space among older paths. Exact range residency remains
limited by the prompt envelope; navigation history is not the prompt itself.

The workspace retains at most four range descriptors per file and 256 total,
containing path, byte offset/count and whole-file SHA-256, not another copy of
source. The loader re-reads the authorized path and admits an older region only
if that revision still matches and the byte/envelope budget permits. Overlapping
or same-line ranges merge; disjoint regions retain separate counted envelopes
and curated entries. Changed revisions invalidate old range descriptors for
that path. A fresh objective clears the range memory.

Curated entry capacity now matches the 256-path workspace ceiling, rather than
silently retaining a separate 32-entry limit. Each request may contain several
non-overlapping ranges of one file; its file list is still unique. Source-ID
binding, total/entry/chunk byte ceilings and exact preimage validation remain.
Legacy exact-block proposals may match any supplied range, but an ambiguous
match across ranges is refused and excerpt-only evidence cannot replace a file.

## Stagnation

Repeated discovery that adds no verified evidence is deduplicated. Two stagnant rounds stop the source-discovery loop and return a useful failure/question instead of spending a fixed sequence of replacement expansions.

Navigation intent is not progress. A changed line/query that falls back to an
already supplied window does not justify another coding request. The host
records bounded path/actual-first-line/content SHA-256 fingerprints after exact
reads. Navigation is prepared on a copy and committed only after authority,
source loading, selected endpoint/model and curated-bundle admission succeed.
Failed sharing leaves accepted selectors, context and evidence history intact.
Two unchanged actual reads preserve that accepted state and retire the stalled
loop. Changed bytes or a genuinely different source range remain eligible.
Fingerprint history is process-local, bounded to 4,096 entries of at most
2,048 bytes each and reset for a new objective; it is not source storage or an
edit grant. `REMEMBERED_RANGE` is navigation-only metadata. It does not guarantee
residency or authorize a patch. A previously seen range that is absent from the
current curated evidence may be requested and made resident again; an unchanged
already-resident window remains stagnant. Only the exact counted blocks and
current curated entries grant patch evidence. Dependency-aware ranking and
native usefulness of the new packing remain acceptance work, not a claim of
complete whole-project ingestion.

## Authority

Repository-map entries, search hits and compact navigation notes are discovery aids only. They cannot authorize writes. Exact reviewed source bytes, verified checkout identity, SOURCE_ID/REVIEWED_SOURCE_ID admission, sandbox-only mutation and operator/build gates remain authoritative.

The repository index reads the selected source area locally. Its compact map
contains bounded declaration/import metadata as well as verified PATH entries;
it is not merely a list of filenames and is not complete patch evidence. Private
runtime state and credentials must not be placed in indexed source. Navigation
counts include only actual line-start PATH entries under the selected area.

Restarting a stopped objective preserves its retained plan and accepted sandbox
checkpoints. An explicitly different objective clears the previous mission plan;
late results from the old generation still cannot stage a build. Two exhausted
reasoning corrections stop instead of silently resetting the same retry budget.
These are CPU contract guarantees, not evidence that a native candidate was
successfully docked or chosen by an operator.
