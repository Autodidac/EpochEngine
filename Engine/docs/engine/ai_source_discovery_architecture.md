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

## Indexed Relationships

Module imports resolve to unique indexed interfaces; quoted/angle includes
resolve only to indexed paths (local directory, exact root, then unique suffix).
Same-module declaration/implementation units form a bounded one-hop neighborhood.
Initial source seeds and shortened catalogs retain these actual owners before
heuristic directory matches. Imports/importers queries are exact path/module
relationships, not substring aliases. Ambiguous owners/includes remain unresolved.
Comments and ordinary/raw literals cannot create declarations or dependency
edges; conditional compilation is not evaluated, so this is navigation metadata,
not compiler truth. Compact maps reserve their footer and only emit edges whose
targets are also admitted PATH entries.

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
current curated entries grant patch evidence. Dependency-aware ranking is
implemented; native usefulness of the new packing remains acceptance work, not a claim of
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

## Incremental Task Memory

The orchestrator stores complete admitted plan text plus its SHA-256 inside its
bounded, atomically replaced checkpoint. Payload v2 checks size, escaped fields and
digest on reload; v1 hash-only checkpoints remain readable but cannot recover
plan text. Failed plan/proposal publication leaves the pending receipt, counters
and in-memory state unchanged for retry. A resume revalidates source/host/model
authority and clears pending operations, approvals and validation. The saved plan
is continuity data, not permission; it requires fresh admission before execution.
Context reselection keeps it instead of overwriting it with the generic host loop.

Proposals request the smallest next buildable unit, preferably one file or a
required coupled edit set. Existing exact patch transactions journal completed
units separately from compiler/runtime receipts. Mid-generation reload discards
unfinished arguments: there is no token/KV-cache recovery or partial-file apply.
Restart UI presently resumes its known checkpoint; cold-start checkpoint
discovery and demonstrated file-by-file native recovery are still acceptance
work. The current deterministic host plan is not a task-specific model planner.
