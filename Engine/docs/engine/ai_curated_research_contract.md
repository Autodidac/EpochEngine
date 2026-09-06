# AI Curated Research Contract

Status: **planned design, September 6, 2026; not implemented or accepted**.
This document preserves the requested research direction for Qwen and other
operator-selected agentic models. It does not claim a running research service,
SDK index, web tool, permission boundary, or successful self-coding session.

The immediate gate remains [Active Pass](../../../Changes/active_pass.md):
repair the actual candidate build/repair handoff, finish resource admission,
and prove two real builds across Keep Current / Choose Candidate. A small local
reference pack may support that work; online research and the full SDK must not
become prerequisites for P0 or substitute for its missing acceptance evidence.

## Purpose and ownership

Let an operator describe an outcome in ordinary language. The selected model
should be able to find relevant Engine knowledge, inspect the actual source,
recognize already implemented capabilities, plan an improvement, and use host
diagnostics to repair its own sandbox changes. The operator must not have to
name internal systems, files, symbols, or a research profile to get started.

Research improves the model's evidence; it does not turn retrieved prose into
an instruction, an execution permit, a build result, or authority to change
live source. Existing source selection, exact-byte staging, campaign history,
validation and process owners remain responsible for their current contracts.
Reuse them rather than building a second autonomous controller around research.

The overall product remains software **and games**. CLI/window/GUI software,
the complete playable 2D project loop, temporal authoring and later 3D/game
features are enduring goals. This research plan neither removes them nor makes
an engine self-coding demo the final product. Product ordering belongs to the
[roadmap](../../../Changes/roadmap.md); unresolved operator intent remains in
[mission cache](../../../Changes/mission_cache.md).

## Two distinct reference profiles

| Profile | Useful evidence | Writable result and continuity |
| --- | --- | --- |
| Engine self-coding | Current Engine contracts and architecture; applicable plans; exact source and focused tests; platform/backend constraints; current candidate failures | The one admitted disposable Engine sandbox and its selected-parent lineage. Engine mission, research index, retries and choice history remain Engine-only. |
| Project development | Version-matched public Engine APIs and software/game examples; the active project's documents, scripts and assets; its admitted package contracts and build results | Only the explicitly selected project's admitted authoring/source workspace. Its mission, research index, semantic history and build lineage remain project-only. |

Project development specializes by the actual project profile, without inventing
an engine plugin lane:

- Software: minimal CLI, platform window and GUI application patterns that do
  not require the editor at runtime.
- Games: the 2D map/actor/collision/camera/audio/save/build/run/reopen loop first,
  then relevant renderer and larger game capabilities as they become proven.
- Project add-ons: verified Extensions entries and their separate library
  dependencies, including the planned EpochSimEngine and EpochSpaceEngine
  integrations. A catalog entry or research archive is not an installed,
  licensed, loadable or tested payload.

The same immutable public API reference may be reused by several project
indexes. Mutable histories, private source excerpts, project assets, prompts,
failure logs and inferred summaries must not be shared implicitly. Engine
self-coding never acquires active-project data merely because the project is
open. A project assistant never acquires Engine-private implementation access
merely because it can use an Engine API. Any deliberate cross-domain sharing
needs its own explicit scope and must not merge the two histories.

## Versioned local pack and manifest

Start with a small, explicitly curated set of local material already owned by
this repository. Relevant sources include the active pass, roadmap, subsystem
contracts, source naming rules, public module/header signatures, focused
examples/tests, capability truth and host validation summaries. Select the
relevant portions; do not concatenate every planning document into each prompt
or make every old plan a new mission.

The proposed pack is immutable content plus a host-generated manifest. The
following are required metadata concepts, **not a shipped serialization API**:

- Pack schema/revision, profile, domain owner, source authority and exact
  content-manifest digest; generator identity and generation time.
- Engine source version and exact commit/tree when available, plus actual
  per-file identities. A commit label alone does not identify a dirty checkout.
- For a candidate: sandbox parent identity, campaign/session/generation and
  selected-file preimages. For a project: project identity, actual software/game
  profile, Engine/SDK version and exact admitted dependency revisions.
- Per entry: stable ID, canonical domain-relative locator, byte count, SHA-256,
  content kind, origin/provenance, license/use restrictions, review time,
  applicability and linked evidence. Keep private absolute host paths out of
  model-facing metadata and any outbound research request.
- Distinct implementation labels: `planned`, `implemented_unverified`,
  `verified_for_binding`, `historical` or `deprecated`. Verification must name
  the exact source/configuration/platform and host evidence it covers.
- Distinct currency labels: `current_for_binding`, `stale`, `unknown` or
  `inapplicable`. A recent timestamp or matching hash proves neither correctness
  nor compatibility. External publish dates are provenance, not validation.

Plans describe desired work; source describes the present implementation; host
receipts describe what was actually built or tested. Preserve those distinctions
in retrieval and model input. A successful older renderer test must not make a
newer source tree or another backend appear verified. A TODO is not an API.

Build indexes inside the corresponding disposable Engine or project research
cache, outside authoritative source and validated executable inputs. Admit only
the selected manifest members: no home-folder, sibling-worktree, unrelated repo,
credential, model-training or arbitrary cache scan. Research indexing must not
follow links out of its admitted roots. `addons/` remains local/offline research
and is not automatically indexed, sent to a model or published.

On source, dependency or selected-parent changes, rebuild affected identities
and mark derived entries stale before reuse. Publish a complete new pack
atomically; failed refresh retains the previous identifiable pack, not a mixed
index. Resume revalidates the session binding and selected bytes. Cached
answers whose dependencies changed can remain visible as history, but cannot
stand in for current edit preimages or validation results.

Generated Doxygen/SDK reference is a later pack input, bound to its exact source
and SDK manifest. A docs version string must not silently follow a runtime
version with different source. Owner-only online SDK access remains governed by
separate server-side owner/admin entitlement, not generic source entitlement
or the existence of a local research index.

## Retrieval, finite budgets and useful repair

Within an explicitly started session and its granted scope, the host should
automatically select and refresh a bounded reference set with the model. Show
what is being read and why in understandable activity output; do not require
another named-system prompt or confirmation dialog for every same-session
lookup. New domains, providers or outbound sharing still need their actual
authority; research content cannot grant it.

Use separate inventories for editable source and read-only reference entries.
A documentation hit must not become an editable source path. The existing
12-path editable C++ working-set limit and strict root/preimage checks are not
silently expanded by adding research. The model may replace its working set
within the existing bounded navigation/retry policy and preserve useful
context across iterations without collecting an unbounded transcript.

Prompt construction must account for the complete encoded request, not just
individual sections. Plan for bytes, model context and output reserve. Current
implementation limits described in the active pass include a 256 KiB complete
prompt, up to 184 KiB reviewed source, 32 KiB repair diagnostics and a 16 KiB
failed-proposal envelope. Those separate maxima are **not additive permission**
to overflow the request; negotiated smaller limits take precedence.

Budget in this order:

1. Required protocol, objective, selected-parent/session identity, remaining
   plan and the output allowance needed for an actual response.
2. On repair, reserve the first causal host error, its concise surrounding
   evidence, the full-evidence digest and the failed edit context before
   appending optional material. Keep full logs locally; don't put a long build
   transcript into a small validation receipt.
3. Exact selected edit source and the immediately relevant API/test context.
   If the whole working set does not fit, request a smaller or different exact
   window; never silently remove source bytes after declaring them included.
4. A compact, version-bound local reference selection and complete path-catalog
   entries that fit the remainder. Extra examples, historical research and
   redundant catalog rows are evicted before repair or edit evidence.

Emit explicit omitted-entry counts and a host budget receipt. Truncate only at
valid UTF-8 and complete entry/window boundaries. Never truncate into protocol
fields, hashes or source search blocks, or report included evidence that was
not sent. A repair request for C1075 must actually contain C1075 and the relevant
source context, even with the maximum admitted catalog and a verbose log.

If research is unavailable, stale or unnecessary, continue the source-backed
workflow with a visible limitation when its required evidence still fits.
Optional research must not make a viable local build/repair request fail.
Conversely, do not fabricate source or report success to avoid a genuine
missing authority, exhausted budget or failed validation result.

## Continuity and understandable progress

Persist only a bounded, domain-owned continuation record: operator objective,
current plan, completed/remaining steps, pack and selected-entry digests,
decisions and short rationales, first failure and retry count, host build/test
receipts, chosen sandbox parent and the next concrete action. Do not retain
hidden model reasoning or turn ordinary chat into automatic training data.

Keep Current / Choose Candidate remains the operator's comparison choice.
After selection, retire the loser, retain only the chosen sandbox lineage,
refresh its affected references, and continue at the next unfinished plan step.
Don't repeat a completed milestone just because the model has a new request
identity. Engine and project goals must survive unrelated UI/domain switching
without deleting, replacing or inheriting one another.

Activity should distinguish `finding local references`, `reading selected
source`, `waiting for model`, `repairing compiler error`, `building`,
`cooling down`, `waiting for comparison choice`, `stopping` and terminal failure
or completion. Show elapsed time, current operation, concise reason and Stop.
Host-owned receipts identify actual included input/output bytes, timings and
results; they do not invent token progress for a non-streaming transport.

Research, model and candidate work must share the existing cancellation,
generation and resource-admission owners. A cancelled lookup/reply cannot
restart a stopped session or approve work. New heavy work respects the planned
30-second nonblocking cooldown and measured host-resource checks; a reference
query does not bypass the Keep/Choose pause or start another engine process.

## Read permission, outbound sharing and execution

These are three independent capabilities:

- Reading an admitted local file permits only that read and its host-owned
  index/evidence handling. It is not permission to send the bytes elsewhere.
- Sending selected evidence to the displayed model requires the session's
  provider/endpoint and content authority. Localhost transport is still a
  separately owned service, not proof of process or filesystem confinement.
- Applying source, compiling, testing and launching a candidate require their
  existing exact host operations and supervision. Neither a document nor a
  model response grants executable commands, network access, Git, release,
  live-source promotion, package activation or listener authority.

All retrieved documents, comments, examples, web pages and model-authored
summaries are untrusted data. Instructions embedded in them cannot override
the operator or host policy. Delimit them as evidence, retain provenance and
validate proposed actions independently. Hashing a document does not make
embedded instructions trustworthy. Do not place retrieved content in a trusted
instruction channel or execute its command snippets.

## Optional host-mediated online research

This is a later, separately enabled adapter, not a currently operating feature.
Prefer the applicable local source/contracts first. When online research is
authorized, accept a bounded public technical question from the model, form and
validate the final query in the host, and show the destination and outgoing
query. Use finite query/result/byte/time/retry limits and cancellable requests.
Prefer official APIs, specifications and primary documentation with explicit
version applicability. Do not promise external facts are current without
actually retrieving and checking them.

Never send private source excerpts, repository/host paths, project names or
assets, raw compiler/model logs, authentication headers, tokens, device IDs or
SDK content in a search query or URL. Derive only a public generic question
(for example, an API name and compiler error category) when it can be separated
safely. If it cannot, remain local and explain that specific limitation.

The model cannot choose arbitrary endpoints, headers, credentials or file URLs.
The host revalidates schemes, approved destinations, DNS/address classes and
redirect hops to exclude localhost/private-network/metadata-service access and
credential forwarding. Reject downloads, executables and oversized or malformed
responses; do not create a browser automation bypass, server, MCP listener or
network control surface. A page response cannot request follow-up private data.

Store admitted findings as bounded, dated, provenance-bearing reference data
within the originating domain. Respect licensing and quotation limits. A model
summary is marked derived/unverified and never substitutes for the fetched
evidence or host validation. Online failure is optional-research failure, not
permission to invent a result or abandon an otherwise viable local repair.

## Measurable acceptance

Implement deterministic contracts before claiming this design works:

- Same manifest/input/budget yields the same included identities and budget
  receipt. Tampered bytes, escaping links, stale parent generations and unknown
  source authority fail before an unauthorized read or outbound send.
- Two project fixtures and one Engine fixture cannot retrieve each other's
  private entries or mutate each other's research/goal histories. Shared public
  SDK reference remains read-only and version-matched.
- Planned, unverified, historical and current verified entries retain their
  labels through retrieval and summarization. Changing one source/dependency
  digest invalidates every affected derived reference, not unrelated packs.
- Maximum-size source/catalog, multibyte UTF-8, verbose compiler failure and
  failed-proposal fixtures stay within the final request limit. C1075, its first
  causal context and full-log digest survive; declared omitted entries really
  are absent and exact edit preimages remain exact.
- Malicious document/web fixtures containing requests to override policy,
  execute commands or disclose canary secrets yield no additional capability,
  file read, outbound request or execution. Fake egress ports assert zero
  private bytes in queries/URLs/headers, including redirects and errors.
- Timeout/cancel/restart and concurrent-domain fixtures discard late results,
  preserve the last complete pack, keep the current goal, and never resurrect
  work. Optional empty/offline research still permits a valid local repair.
- A real model run shows the included local references and ordinary-language
  objective, repairs an actual build failure, completes two real accepted
  builds across a choice, and preserves the selected parent/remaining plan.
  Deterministic mock tests cannot replace this native/model acceptance.

## Delivery sequence

1. **Minimal local support for current P0:** define a small explicit reference
   manifest, bind it to the current source/plan, and reserve repair context
   correctly. Integrate through existing source/session owners and tests. Do
   not wait for web, Doxygen, embeddings, a vector database or a new service.
2. **After P0 proof:** qualify separate project/software/game packs against real
   generated-project Build/Run/Stop/Reopen and the full 2D acceptance loop;
   integrate verified project-add-on/library examples without copying unrelated
   repositories into Engine. Preserve the accepted frozen software/context base.
3. **Version-bound SDK and richer reference:** consume generated module/header
   documentation and samples with exact manifests, checked API coverage and
   owner-only Site access. Inspect existing Doxygen/authentication support first.
4. **Optional online support:** add only the scoped host adapter, provenance,
   cancellation and adversarial privacy tests above. Release it only after its
   own evidence passes; it must not hold the local self-coding fix hostage.

These phases preserve the wider roadmap rather than declaring it complete.
Record implementation, verification and native acceptance separately in the
active pass/changelog when they actually happen. The owning safety reference
is [OS AI policy](os_ai_tooling_and_evidence_policy.md); external imported
material remains subject to [research import review](research_import_and_promotion.md).
The separate planned [SDK reference/access contract](sdk_reference_and_access_contract.md)
owns generated-reference coverage and owner/admin authorization requirements.
