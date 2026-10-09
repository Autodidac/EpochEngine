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

HTTP coding requests expose two mutually exclusive actions: an exact patch or a
read-only context request for the next missing region. Exactly one complete tool
call is admitted; mixed/multiple calls remain invalid. Context requests use
verified catalog IDs; patches still require reviewed IDs and supplied preimages.
Read-only selection stages cannot propose edits. Missing bytes therefore need
not become an empty patch followed by a blind separate source-selection call.

A navigation call selects unique `source_ids` and up to 256 read records within
the active byte budget. This is a defensive packet ceiling, not a twelve-file
workflow restriction. Patches permit up to 64 exact blocks, including distinct
non-overlapping blocks of the same file; they are grouped against one immutable
preimage into one atomic file postimage. Whole-file/block mixing, ambiguous
matches and overlapping blocks remain invalid.
Distinct line/query windows may repeat a selected source ID; an identical
path/line/query record is rejected. Canonical packets list each path once, then
use `read_path:` for additional selectors on that already selected path. Every
window survives schema normalization, codec validation and source handoff; the
path list remains unique and grants no additional authority.

## Indexed Relationships

Module imports resolve to unique indexed interfaces; quoted/angle includes
resolve only to indexed paths (local directory, exact root, then unique suffix).
Same-module declaration/implementation units form a bounded one-hop neighborhood.
Initial source seeds and shortened catalogs retain these actual owners before
heuristic directory matches. Imports/importers queries are exact path/module
relationships, not substring aliases. Ambiguous owners/includes remain unresolved.
Comments and ordinary/raw literals cannot create declarations or dependency
edges; conditional compilation is not evaluated, so this is navigation metadata,
not compiler truth. Compact maps reserve verified paths first, ordered by
relevance, then optional symbols/edges within the remaining byte budget. Zero
keyword matches do not hide a project path. The Panel uses the indexed file
count rather than a separate 384-path cap. Maps reserve their footer and emit
edges only when both paths are listed. The exact source-owned `Engine/src/build`
folder remains indexed/copied; generated build directories and nested caches
remain excluded. Indexing still stays inside the selected scope.

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

Automatic rereads prefer the newest valid remembered region rather than jumping
back to a generic objective match elsewhere in the file. Explicit multiple reads
share their file's bounded allocation; useful prior regions retain a reserved
share when possible. Insufficient space for the minimum windows produces a
precise smaller-request diagnostic, not a claim that all remembered bytes are
resident. Revision checks and the total byte/envelope ceilings still apply.
An old selector is not re-executed while another path is requested: its verified
region is retained instead of advancing the old query to unrelated matches.
If that automatic remembered region must shrink, objective terms center the
smaller excerpt inside the same verified range rather than retaining only its
license prefix. Explicit current line/query selectors retain exact semantics.

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
After successful fallback admission, its verified first-line anchor replaces the
failed query/line in navigation memory. With several selectors on one file, only
the exact failed path/line/query record is normalized; valid reads stay intact.
Never guess selectors from a path-only catalog: use the automatic window until
actual source/search evidence supplies a line or exact literal.

Automatic literal lookup prefers the first match outside revision-verified prior
ranges and emits up to sixteen `SOURCE_QUERY_MATCH_LINES` plus the selected line.
An explicit nonzero line retains fixed forward-search semantics. These hints
are navigation only; every counted exact source window gets its own binding to
the same reviewed source ID. Compiler repair prioritizes bounded windows around
the first error in each of up to four already-reviewed candidate paths, rereading
the failed candidate revision rather than repeating the original objective window.
It cannot admit an unreviewed compiler path or authorize a source edit.
Widening an
unrelated older file during budget repacking
does not count as progress for the model's requested lookup. Requested genuinely
new bytes or re-entering evicted ranges retain their existing admission path.
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

`Engine.AI.Source` logs admitted session/scope identities and resident path,
first/last lines, byte count and content digest, plus resolved fallback anchors.
It does not record source bodies, raw queries, prompts or credentials. Candidate
Lab labels an unbuilt workspace as source, not a validated executable. Neither
label nor range telemetry proves successful model edits, compilation or docking.

## Authority

Repository-map entries, search hits and compact navigation notes are discovery aids only. They cannot authorize writes. Exact reviewed source bytes, verified checkout identity, SOURCE_ID/REVIEWED_SOURCE_ID admission, sandbox-only mutation and operator/build gates remain authoritative.

The repository index reads the selected source area locally. Its compact map
contains bounded declaration/import metadata as well as verified PATH entries;
it is not merely a list of filenames and is not complete patch evidence. Private
runtime state and credentials must not be placed in indexed source. Navigation
counts include only actual line-start PATH entries under the selected area.

Restarting a stopped objective preserves its retained plan and accepted sandbox
checkpoints. An explicitly different objective clears the previous mission plan;
late results from the old generation still cannot stage a build.
Completed invalid source actions carry `EPOCH_SOURCE_ACTION_REJECTED_V1` and
the actual host diagnostic through the public reply path. They do not undergo
an extra transport-format retry or fall into the proposal-header decoder.
Action/schema corrections and provider recovery share at most two host retries
without verified progress; control rebinding does not reset that counter.
Exhaustion stops and preserves the saved plan, accepted evidence and sandbox,
instead of silently replanning the same pass. Actual newly admitted source bytes
or an accepted patch reset it. Short retries for genuine transport exceptions
remain separate from this host budget.
These are CPU contract guarantees, not evidence that a native candidate was
successfully docked or chosen by an operator.

## Incremental Task Memory

The orchestrator stores complete admitted plan text plus its SHA-256 inside its
bounded, atomically replaced checkpoint. Payload v3 also stores a compact journal
of host observations and the source-turn review counter. Escaped fields, size
and plan digest are checked on reload. Payload v2 retains its plan but has no
journal; v1 hash-only checkpoints remain readable without inventing lost text.
Failed plan/proposal publication leaves the pending receipt, counters and
in-memory state unchanged for retry. Resume revalidates source/host/model
authority and clears pending operations, approvals and validation. Memory is
continuity data, never carried permission or source-edit evidence.

Fresh Candidate Lab objectives use a separate administrative supervisor turn
through the same selected model provider: three to six objective-specific tasks,
dependencies/success criteria and a concrete `NEXT_GENERATION` handoff ending
with `END_NEXT_GENERATION`, requested under 300 words. The complete plan/handoff
is digest-bound and retained. Worker turns receive that plan, recent host
observations and current exact source, and return one useful read or buildable
edit unit rather than restating/replanning the mission. Source reselection keeps
the design. After three accepted source-sharing turns, the next generation is a
brief supervisor reconciliation against the retained facts; it saves a revised
handoff and resets the review counter. Other reads/repairs reuse the plan without
another planning inference. This cadence is not a retry/file/mission limit.
Host-only plan-origin metadata distinguishes re-admitting saved memory from an
actual supervisor response: only the latter resets the persisted review counter.
A reload during a worker request therefore retains the administrative cadence.

The process-local host journal retains complete recent records in at most
12 KiB: admitted read targets/reasons, atomically applied paths/digest, and trusted
validation outcomes. It enters the next durable campaign checkpoint and prompt
as observations, not executable instructions, raw reasoning or completion proof.
Actual patch journals and validation receipts remain authoritative. Chosen/revised
candidate checkpoints accompany later prompts as continuity, not evidence that
unverified tasks completed. The existing typed scheduler/host supervisor still
owns admission and execution; no second model process or external agent obtains
Epoch filesystem authority by being called a supervisor.

Proposals request the smallest next buildable unit, preferably one file or a
required coupled edit set. Existing exact patch transactions journal completed
units separately from compiler/runtime receipts. Mid-generation reload discards
unfinished arguments: there is no token/KV-cache recovery or partial-file apply.
Restart UI presently resumes its known checkpoint; cold-start checkpoint
discovery and demonstrated file-by-file native recovery are still acceptance
work. The former generic deterministic host checklist is no longer substituted
for a fresh objective's implementation plan. CPU fixtures prove the compact
plan -> saved checkpoint -> coding handoff, review cadence and reuse, with legacy
reload, journal bounds and failed-publication regressions. Native task usefulness
is measured by an actual patch/build/candidate, not by a plan response.
