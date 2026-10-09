# AI Source Discovery Architecture

Epoch Engine Self-Coding uses a host-owned retrieval layer. The model does not receive the whole repository and does not gain arbitrary filesystem authority.

## Operator Workflow And Its Actual Boundaries

1. **Describe the engine change.** Epoch routes engine-source work away from
   Project Assistant scene/GUI authoring. The host indexes the selected source
   authority, retrieves objective-ranked exact bytes and materializes one private
   candidate source snapshot. Indexed paths remain available for later reads;
   secrets, generated output and authored project state are not source context.
2. **Supervisor generation.** The selected provider receives the objective,
   exact reviewed source, verified repository map and any saved host observations.
   It returns a compact task-specific plan and `NEXT_GENERATION` coding handoff.
   This is a separate role/request using the same selected model, not a second
   background model or an external EngCoder agent with filesystem authority.
   Before this repair it saw path/range metadata without the source bodies,
   which could produce a repeated investigation handoff instead of a design.
3. **Coding generations.** Each request is reconstructed from the saved plan,
   recent host facts and currently supplied exact source. It is not an ongoing
   provider conversation or a recovered KV/token cache. The worker should edit
   immediately when the next buildable unit is grounded; otherwise it batches
   known missing regions into one read action. Complete files are supplied when
   they fit; oversized implementations require exact windows. There is no promise
   that the entire repository or two enormous files fit one loaded context.
   Source-selection corrections and insufficient-evidence recovery preserve the
   same exact FILE blocks and use the coding contract once source is admitted;
   they must not replace source with read receipts or force a read-only turn.
   Initial path selection without admitted source remains read-only.
   Source-ID numbers are request-local; saved paths must be resolved against
   the current catalog rather than reusing IDs from a previous generation.
4. **Accepted actions, not raw thoughts.** Exactly one complete read or patch
   tool call is validated. A patch uses current reviewed IDs and exact original
   blocks; several blocks in one file form one atomic postimage. Incomplete tool
   arguments and reasoning streams are not written to source. Read requests grant
   no edit, build or promotion authority. Unusable replies carry the actual host
   diagnostic into a bounded correction, rather than being treated as progress.
5. **Durable progress.** Admitted plans, host observations and accepted patch
   transactions are checkpointed. Source reads retain revision-bound descriptors;
   exact bytes must still be reread and verified before later use. Previously
   needed declarations and disjoint implementation regions receive residency
   priority before a new read is widened when the combined demand fits. Provider
   reload loses the unfinished response, not completed sandbox edits; permissions
   and pending operations do not survive as automatic authority. Known-checkpoint
   resume exists; cold-editor discovery and native recovery remain open gates.
6. **Build and repair.** An accepted buildable unit enters host-owned compiler
   and contract validation. Read-only host dependencies are reused rather than
   installed into each candidate. A failure returns its causal diagnostics and
   current candidate bytes to the worker. Code, a successful request, an admitted
   patch and a passing compiler are distinct milestones. The task list is model
   guidance; semantic completion is not proved merely by its wording or a build.
7. **Independent candidate comparison.** After the required receipts, the host
   launches a separately supervised Editor PID with private writable runtime data
   and embeds its native window for comparison. This is not an in-process engine
   script/hot-load. Process/window leases handle lifecycle ownership; native
   fullscreen, focus, resize, failure and docking acceptance still require proof.
   Source path routing and process Job Objects are not OS security confinement.
8. **Human decision and promotion.** In the current implementation, human Pass /
   Choose finishes that objective, preserves the validated sandbox and remembers
   it as a future parent. Fail / Revise retires its preview and continues on that
   same sandbox with retained mission memory. Choose does not overwrite the
   original Engine checkout or authored project. Starting another objective from
   the chosen parent and explicitly reviewed live-source promotion are separate
   operations; a second native successor is still an acceptance requirement.

The visible candidate/pass counters describe workflow generations, not files
completed or percentage of the requested feature. Three accepted source-sharing
turns trigger a brief administrative reconciliation; other reads/repairs reuse
the plan. A new source read can be legitimate without being useful coding
progress. The native acceptance gate is an actual useful patch, compiler/test
receipts, embedded independent PID, human decision and subsequent chosen-parent
build, not repeated planning or token generation.

Storage: canonical checkout source and authored Projects are durable; build
outputs are reproducible; `Engine/examples/EpochEditor/workspace/cache/ai/iterations/session_*`
retains candidate source, checkpoints, lineage and logs. Do not delete chosen or
recoverable candidate ancestry as ordinary cache. Model settings and transport
logs are executable-local, so Debug and Release do not silently share them. See
`repository_layout_reference.txt` for the folder ownership map.

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

The workspace is bounded by bytes/tokens, not a small operational file count.
Packet ceilings protect decoding; they are not limits on a multi-generation task.
All indexed paths in the selected source area remain available to request, not
only the initial seed. Generated outputs, runtime state and credentials are not
source. A catalog entry grants read navigation, never a live-source write.

Model Settings accepts an optional actual loaded context size, saved for the
exact local endpoint/model. Use it when inventory omits capacity; EngCoder's
inspected `/v1/models` does so. Automatic HTTP mode uses reported loaded capacity,
otherwise reported maximum, otherwise an 81,920-token default for every role.
The fallback is packing policy, not measured model capacity or permission to
hot-change a running campaign. A smaller reported/declared context wins.
The override cannot exceed a reported capacity. Accepted settings range from
8,192 to 1,048,576 tokens; direct-CLI providers do not inherit HTTP overrides.

Panel packing and source transport share one conservative code-text estimate:
reserve requested output and system/tool framing, then estimate three input
bytes per remaining token, at most 2 MiB. This is not an exact tokenizer or an
overflow guarantee. At 81,920 context / 32,768 output tokens it permits 122,880
prompt bytes; source evidence reserves another quarter for protocol/navigation.
Do not assume two enormous implementation files fit that window. Reported or
declared smaller capacities are not raised to an obsolete 256 KiB floor.
Source prompt formatting uses the same 2 MiB evidence ceiling instead of
silently clipping at 512 KiB. Above that ceiling it returns no prompt; exact
counted source frames must remain complete, not become truncated authority.

The loader allocates from actual bounded file-size demand rather than splitting
the source budget equally among all paths. It reserves useful minimum windows,
then reserves useful current reads and mission-owner remembered bytes before
completing passive small files or replaying unrelated historical ranges. Current declarations up to 64 KiB request complete
files; large implementations request 8 KiB per selector within the shared budget.
Remaining capacity preserves exact remembered regions before widening reads.
This prevents retained broad history from reducing a new function read to a
few lines. Under genuine oversubscription these targets are not guaranteed;
the supplied ranges remain the exact authority, not the requested sizes.
Passive paths reconstruct all revision-verified disjoint regions when their
allocation fits instead of enlarging the newest region and evicting the others.
Overlapping or adjacent remembered reads form a verified byte union, not
duplicate demand on the next allocation. Under genuine pressure a new read
keeps a useful share rather than reserving the entire envelope for old bytes.

Each admitted read reconciles the resident working set with the saved mission.
Currently requested paths and known owners named by full path or unique basename
stay resident; unrelated prior owners become archived navigation. Without known
owners, the host preserves the broad working set rather than guessing relevance.
The cumulative workspace keeps those paths, hashes and verified ranges; no source,
candidate edit or checkpoint is deleted. Archived metadata is not current FILE
content or patch authority. A worker retrieves a missing archived dependency
with an exact lookup, without restarting the mission.

Host prompt compaction keeps complete task records and the entire
NEXT_GENERATION/END_NEXT_GENERATION block, never the 4 KiB UI preview of a plan.
Repeated/old observations may be omitted with an explicit marker; the original
checkpoint and host logs remain authoritative. If the protected handoff cannot
fit, dispatch is refused rather than clipped. This is deterministic working-set
and record compaction, not token/KV recovery or an invented semantic summary.
The model supervisor separately refreshes the task checkpoint from actual evidence.
Visible compaction notices and Engine.AI.Context logs report resident bytes/files,
retrievable ranges, omitted records and selected context/output/prompt budgets.
These are byte estimates, not tokenizer counts or completed-work percentages.
Automatic reads supply complete
files when they fit their actual allocation, up to the 1 MiB curated-entry
safety ceiling. Several complete files can share a request; the old 96 KiB file
and 32 KiB excerpt limits no longer force another discovery pass. Explicit
line/query cursors still select the requested region. Passive paths request their
revision-verified bytes rather than unrelated broad windows. Exact range
residency remains limited by the prompt envelope; navigation history is not the
prompt itself.
Multiple automatic lookups in a complete resident file share one source frame.
They must not be refused by charging duplicate framing before recognizing that
the whole file satisfies them. Explicit line cursors retain their own semantics.

The workspace retains at most 256 range descriptors globally, without a four-per-file limit,
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
The search continues beyond those display hints until an unseen match is found;
covered regions are skipped without enumerating every repeated literal.
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
through the same selected model provider with current exact reviewed source and
the verified navigation map, not only filenames/line observations: objective-specific tasks,
dependencies/success criteria and a concrete `NEXT_GENERATION` handoff ending
with `END_NEXT_GENERATION`. There is no word/step quota or counting instruction.
The checkpoint preserves source owners, design decisions, receipt-backed completed
work, remaining dependencies and one next actionable unit. The complete plan/handoff
is digest-bound and retained. Worker turns receive that plan, recent host
observations and current exact source, and return one useful read or buildable
edit unit rather than restating/replanning the mission. Source reselection keeps
the design. After three accepted source-sharing turns, the next generation is a
brief supervisor reconciliation against the retained facts; it saves a revised
handoff and resets the review counter. Other reads/repairs reuse the plan without
another planning inference. This cadence is not a retry/file/mission limit.
Workers use actual supplied-source observations to recognize completed
investigation and advance the next unfinished task. A saved investigation
handoff is not an investigation-only lock. Known missing dependencies should be
batched; observed function/definition names and explicit match cursors take
precedence over repeated generic type/theme searches.
Host-only plan-origin metadata distinguishes re-admitting saved memory from an
actual supervisor response: only the latter resets the persisted review counter.
A reload during a worker request therefore retains the administrative cadence.

The process-local host journal retains complete recent records in at most
12 KiB: admitted read targets/reasons and actual supplied line ranges/bytes with
complete/partial status, atomically applied paths/digest, and trusted
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
