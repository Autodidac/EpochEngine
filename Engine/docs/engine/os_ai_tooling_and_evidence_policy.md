# OS AI, MCP, Harness, And Evidence Policy

Epoch source `v0.89.28` does not train or silently activate a model. It can
invoke the verified Epoch-local Qwen3.8 installation or offload inference to an
operator-managed external model machine, and it keeps model transport, MCP
authority, execution, and evidence as separate concerns. The published
`v0.89.06` packaged runtime and updater remain sealed.

## Model Boundary

Supported inference transports are:

- an operator-managed OpenAI-compatible endpoint, local or on an external
  machine, using `/v1/models` and `/v1/chat/completions` while Epoch retains
  the provider-independent MCP guard;
- a directly selected `llama-cli` executable plus an operator-licensed GGUF
  model.

Epoch also exposes a disabled-by-default, executable-local provider named
`epoch_local_qwen38`. Its tracked install contract pins llama.cpp release
`b10516` at revision `b95502ba9aa0eb73a2f4fc8878d7fbe6a847a0b9` and the
community `Qwen3.8-27B-UD-Q4_K_M.gguf` quantization at revision
`4ca720788d1e01f1bff70c033e0d0028fd02e502`, with exact artifact SHA-256 and
model byte count. `Tools/ai/install_epoch_qwen38.ps1` performs an explicit
operator-invoked transfer into the selected Epoch executable directory's
`cache/packages` and `cache/models` buckets. It writes readiness receipts
only after artifact verification. External model compute plus Epoch MCP remains
a peer choice; local installation never replaces it.

Generated projects carry `Assets/AI/project_ai.epochai` with AI disabled by
default. The explicit project choices are `disabled`, `epoch_local_qwen38`,
and `external_mcp`; project builds copy the profile but never model weights.
New profiles also make the inference transport and provider-independent
`epoch_mcp_v1` tool protocol explicit.

Direct CLI inference is one captured child process with an argument vector,
bounded output, timeout, termination, and offline/no-server flags. Epoch does
not start `llama-server`, bind a port, silently choose a discovered model, ship
or mutate model weights, fine-tune a model, or maintain a hidden internal
persona. Discovery is inventory; activation is an explicit operator choice.
Personal AI research, training, and model development remain separate from
Epoch and are neither scanned nor admitted as engine evidence.

Every inference request publishes through generation-checked shared state. Pause,
goal edit/delete, and pane replacement invalidate the active generation without
closing WinHTTP or joining a transport from inside a GUI callback. Application
shutdown owns bounded hard cancellation and teardown. A stale or cancelled result
cannot approve, apply, retry, or retain editor state. The asynchronous sandbox
compiler future has one completion owner; diagnostics are read-only observers.

## Voice Interaction Policy

Voice is an optional input/output adapter around the existing bounded AI path,
not a new authority channel. Desktop authoring builds may integrate
operator-selected local speech-to-text (STT) and text-to-speech (TTS) providers.
Epoch does not silently install a provider, start a speech server, bind a port,
open a hidden listener, or begin recording because a model or audio device was
discovered.

Microphone access requires explicit per-session consent that is separate from
model selection. Push-to-talk is the safe default. A hands-free conversation
mode requires another visible session action, persistent recording/listening
status, mute/stop controls on the primary host, and immediate capture shutdown
when the session, project, pane, or application closes. Consent does not survive
an engine restart and cannot be granted by a model, project, package, script, or
remembered layout.

STT produces a visible, editable transcript. Submitting that transcript follows
the same limits as typed input. Chat remains chat; scene/GUI authoring remains a
parsed proposal; source development still requires exact review, operator
approval, a private execution permit, and trusted evidence. TTS may read an
accepted assistant response through the existing audio boundary, but generated
speech cannot trigger tools, approve a plan, confirm a model, or satisfy an
evidence gate. Interruption cancels only speech playback or capture unless the
operator separately cancels the associated bounded request.

Raw microphone audio is transient and is not persisted by default. Explicit
capture for diagnostics or evaluation requires a visible opt-in that names the
destination, retention scope, selected provider, and session. Retained
transcripts follow the existing evidence policy; hidden audio, background
recordings, and automatic dataset collection are forbidden.

Game, mobile, console, server, and headless profiles must be able to compile out
microphone capture, STT/TTS adapters, conversation controls, and desktop audio
host integration independently of text-based AI protocol types. A product that
intentionally ships voice support owns its own visible permission and platform
policy. `ai.voice_session` now owns the bounded permission, listening,
transcription, review, response, speech, interruption, and cancellation state
machine with build-safe contract proof. Current source still does not claim a
working microphone capture adapter, STT/TTS provider adapter, or
voice-conversation eye proof.

## Tool And Authority Boundaries

`ai.mcp` owns provider-independent tool descriptors, structured calls/results,
capabilities, budgets, cancellation, and evidence attachment. It does not grant
host authority. Epoch starts no MCP network server or hidden listener.

`ai.development_guard` owns the development authority state machine. Its inputs
are bounded data, not shell commands:

- generation-checked session and proposal identities;
- typed operation, risk, workspace area, canonical relative path, permission,
  and exact content-transition intent;
- normalized workspace allowlist rules;
- immutable SHA-256 proposal digests;
- separate review and operator-approval records;
- bounded proposal, approval, and permit lifetimes;
- required terminal evidence, cancellation, and ordered bounded audit records.

The guard has no filesystem, process, network, Git, package, updater, or release
executor. Its `ExecutionPermit` is a private guard-issued capability: callers
can inspect it but cannot construct a valid permit from a digest and selected
fields. Claiming that permit moves the proposal to executing exactly once;
expiry, reuse, mismatch, cancellation, or a second claim fails closed.

Production controller time comes from an internal `steady_clock` mapping and
does not trust caller-supplied timestamps. The externally driven clock policy is
restricted to deterministic contract workspaces and rejects backward time.

## Exact Development Workflow

AI-assisted development follows one fail-closed sequence:

1. Before the model receives a source request, the host tokenizes the approved
   objective, removes conversational filler, adds known subsystem-owner aliases,
   and gives explicit module/file owners priority while ranking existing files
   beneath the selected read-only source roots. An exact canonical objective
   path has precedence. It stages at most six candidates; the first exact primary
   may use the 184 KiB evidence ceiling while additional related files remain
   under the 128 KiB aggregate budget. This metadata-only curation does not read
   or transmit source bytes, and the shared-context status retains the primary
   evidence path for operator inspection.
2. The host displays the selected model, endpoint, objective, and complete
   curated path list. The model cannot request, discover, invent, or expand paths;
   legacy `EPOCH_SOURCE_CONTEXT_REQUEST_V1` output is rejected. If the host cannot
   find credible bounded context, the pass stops for a better objective or human
   selection instead of asking the model to browse.
3. Only the explicit `Share Curated Context` action reads source. It revalidates
   the unchanged objective, endpoint, canonical root, regular-file identity, and
   byte budgets, then sends complete counted evidence through 48 KiB or one
   UTF-8-safe 16 KiB objective-centered excerpt for a larger reviewed file.
   Evidence goes only to the displayed selected endpoint and is request-local;
   no automatic persistence or live-source write occurs. The model may return
   exactly `EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1` or one strict
   `EPOCH_SOURCE_PATCH_PROPOSAL_V1` exact-block packet; it may not negotiate for
   more filesystem access. The separately reported file/byte count describes
   the complete disposable build workspace copied locally; it is not the model
   context and does not expand the explicitly reviewed path list.
4. The trusted host resolves a read-only live source root, snapshots exact
   preimages, computes before/after SHA-256 states, assigns operation IDs,
   permissions, risk, actor identity, and expiration, validates bounds and
   allowlists, stores the immutable proposal, and computes its digest.
5. The host materializes exact approved preimages beneath a unique writable
   `cache/ai/iterations/session_*` root and verifies bytes and SHA-256 there.
6. A reviewer accepts or rejects that exact digest.
7. When policy requires it, the allowlisted operator separately approves or
   rejects the same digest with a bounded expiration.
8. The guard issues its private expiring execution capability for the exact
   proposal, and the registered host claims it exactly once.
9. Source work passes through the exact-content transaction executor inside the
   selected iteration root. Build and validation work returns trusted host
   evidence and cannot self-attest through model text.
10. The guard accepts only verified terminal evidence bound to the same session,
    proposal digest, permit, and actor.

Changing any operation, path, permission, rationale, risk, content state, or
execution scope requires a new proposal and digest. A model reply, confidence
value, chat message, button label, previous approval, or successful child-process
start is never reusable authority.

`editor.ai_development_controller` composes this sequence, serializes execution
entry, maps production calls to trusted monotonic time, and refuses public
source-completion evidence unless its internal transaction executor produced it
inside the selected iteration root. AI Controls owns objective entry, Request
Proposal, the host-curated candidate list, Share Curated Context or Reject
Selection, and read-only inspection of the selected model, endpoint, and sandbox
evidence. After context is explicitly shared, proposal-specific Review Proposal
and Approve Sandbox or Cancel Proposal authority stays attached to the resulting
AI Chat response. Changing the objective invalidates reviewed source evidence before another
request can be staged. Advanced controls are read-only evidence. No action
automatically promotes sandbox bytes into live source: a verified candidate
requires distinct `Stage Live Promotion` and `Approve Live Promotion` actions.
No chat/build/tool result becomes an automatic live-source change or training
record. No GUI eye-test or responsiveness proof is claimed for this source candidate.

## Bounded Iteration Loop

`ai.iteration_loop` turns the development policy into a deterministic,
build-safe state machine. It sequences:

1. inspect existing architecture and ownership;
2. state invariants and non-negotiable boundaries;
3. research and produce one bounded plan;
4. obtain operator approval for the immutable task/risk scope;
5. apply exact reviewed source through the guarded transaction path;
6. compile, test, run static analysis, and run the applicable sanitizer;
7. let the selected local model review the resulting diff once;
8. require architecture, visual, and frontier review according to risk.

The risk vocabulary is contained feature, related files, large subsystem,
cross-subsystem, architecture campaign, lifetime critical, concurrency critical,
and Vulkan synchronization. Cross-subsystem, architecture, lifetime,
concurrency, and Vulkan work requires a separate frontier review before the
loop can complete. Large subsystem work requires architecture review. Visual
work may opt into a mandatory visual-harness gate.

The selected local model is a bounded writer and reviewer, not an evidence
authority. Its milestone reply is held as a candidate until the operator accepts
it. The compiler, test runner, analyzer, sanitizer runtime, visual harness, and
frontier reviewer each own distinct evidence actors; one cannot impersonate
another. Repair attempts are bounded and validation failure returns only to the
approved implementation scope. Exhaustion blocks the campaign for human
diagnosis.

`ai.iteration_loop` remains a build-safe policy/state-machine contract, not
unrestricted editor autonomy. Engine Development connects one contained live
slice: a Qwen3.8-class coding model may enter the related-files writer lane,
receive the host-curated source workload, return a strict proposal, and—after
exact digest approval—apply it only to a generation-owned exact-copy buildable
workspace. The host runs hidden direct MSBuild Debug and Release compiler passes
and a separate build-safe engine-contract child from each editor output.
Successful evidence from those four actors queues a distinct HeadlessCI Debug
build and asset-light run against the same disposable workspace. After those six
actors pass, the UI exposes a separate `Run Full Validation` decision. Visible
operator approval runs the Release editor's `--engine-validation-self-test`,
covering aggregate contracts, registered project profiles, generated-child
self-tests, and the AI gate. Failure in any actor may feed bounded verified
diagnostics into at most three fresh-generation repair proposals. Each changed
repair has a new digest and waits for exact operator approval; cancellation and
stale completions are rejected. Only all seven evidence completions for the
current generation create a verified promotion candidate; that candidate does
not itself write live source.

Static-analysis, sanitizer, architecture, visual, and frontier host adapters are
not connected. Do not replace missing adapters with model confidence, hidden
source application, manual pass checkboxes, or synthetic evidence.

## Verified Live-Source Promotion

Live promotion is a separate host-controlled transaction after sandbox
acceptance. `Stage Live Promotion` requires all seven same-generation evidence
completions: Debug and Release editor compiler/contract pairs, the HeadlessCI
Debug build/run, and the separately operator-approved full validation. The host
verifies distinct canonical live
and sandbox roots, allows only reviewed existing-file source transitions, and
checks every live file against its captured preimage plus every sandbox file
against its compiler-tested postimage. It then reparses the retained strict
proposal through a new controller rooted on live source, requires exact
operation equality, and displays that controller's new digest. Staging writes
nothing.

`Approve Live Promotion` rechecks the live preimages, sandbox postimages, and
operation set. Only then does the new controller record review and exact
operator approval, issue a fresh single-use permit, and call the same atomic
source executor against live source. The executor still owns symlink/path
refusal, preimage revalidation immediately before commit, verified postimages,
rollback evidence, and partial-commit recovery. A stale live file, tampered
sandbox, changed operation, unsafe path, expired or cancelled permit, commit
failure, or replay fails closed.

Success consumes the candidate. It does not automatically request another model
reply, rebuild the running editor, invoke Git, start a listener, publish,
release, or mutate the updater. The model never receives the promotion permit or
approval authority. Deterministic contracts prove accepted promotion,
stale-live refusal, sandbox-tamper refusal, exact operation equality, atomic
postimages, and replay refusal. The Debug fixture closes its evidence readers
before atomic replacement so Windows handle lifetime matches production. The
live GUI/model flow itself remains an operator evidence gate.

## Reviewed Active-Project Tool Plans

`ai.mcp` owns the strict bounded `EPOCH_TOOL_PLAN_V1` protocol for non-source
active-project work. AI Chat `/tool <objective>` may ask the selected local
model for exactly one argument-free call from this allowlist:

- `project.inspect`;
- `project.save`;
- `project.build`;
- `project.run`;
- `project.test`;
- `diagnostics.read`.

The reply contains one title, one summary, one bare call, and one end marker.
Unknown tools, arguments, paths, duplicate fields, a second call, trailing
content, or an oversized reply fail closed. Parsing creates no authority. The
editor shows the exact staged packet beside Apply/Discard, revalidates it through
the existing MCP registry after approval, and dispatches only through canonical
active-project owners. Run has a distinct `Approve Project Run` action.

Approved `project.test` may ask the host to save/materialize and perform the
canonical prerequisite build. Only accepted artifact evidence can schedule the
hidden direct child with `--project-self-test`; that task is cancellable and
completion is ignored if the active project changed. It never silently becomes
a project runtime launch.

The model cannot select a filesystem path or native command, issue/claim a
source permit, approve itself, start a listener/server, use unrestricted shell,
invoke Git, access release/updater authority, or chain operations. Host compiler
and child-process results remain the evidence actors. Debug and Release editor
builds plus aggregate build-safe contracts prove the source/codec path; the live
model/GUI approval flow and approved Run/Test execution remain explicit operator
runtime gates.

## Strict Model Source Proposals

`ai.development_proposal_codec` accepts the production
`EPOCH_SOURCE_PATCH_PROPOSAL_V1` exact-block protocol after host-curated
context. Its legacy whole-file and context-request decoders remain compatibility
parsers only; the editor prompt does not ask a model to negotiate paths or
regenerate a complete file. A proposal is a data protocol, not a command
language: no prose, Markdown fences, unknown fields, trailing bytes, paths
outside the selected workspace, or model-selected permissions are accepted.
Source iteration also recognizes exactly
`EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1` as a safe refusal. A request to identify
one source-proven defect in a named subsystem is already bounded even when the
operator has not pre-named a symbol. Its first refusal receives exactly one
deterministic recheck against the same reviewed bytes; the model must identify a
concrete cause, violated nearby invariant, and bounded repair or repeat the
refusal. A repeated refusal is terminal and stages nothing. Framed and raw
direct-CLI transcripts use the same line-boundary strict-packet extractor, so
ordinary prose cannot enter the source codec.
The direct llama.cpp transport normalizer prefers patch, legacy proposal, and
context-request headers in that order before the insufficient marker, then
extracts exactly through the matching line-framed `end_proposal` or
`end_request`. Prefix/suffix transcript or model commentary is transport framing
and never enters the strict codec; malformed bytes inside the selected envelope
remain a hard rejection. The editor consumes each completed source-model
generation exactly once and clears the transient raw reply after routing it. A
rejected packet may receive at most two complete, fresh correction attempts
carrying only a bounded deterministic host diagnostic and the same reviewed
evidence. No correction stages bytes, changes paths, bypasses review, or expands
authority. Sharing a newly reviewed context resets the prior request's correction
and diagnostic-recheck state.


Every operation requires reviewed path context and one exact search block that
occurs exactly once in the corresponding full live preimage. Complete files at
or below 48 KiB may enter evidence as counted `FILE_CONTENT`; larger reviewed
files provide one UTF-8-safe, nearby-newline-aligned 16 KiB
`FILE_EXCERPT` centered on objective vocabulary. Excerpts authorize only exact
block replacement. The trusted controller resolves the search against the full
live preimage, reconstructs the complete postimage, and stages its immutable
hashes and bytes. New-file proposals remain rejected; adding a path requires a
future explicit host-owned path-creation control rather than a model request.
The source-format example uses the first exact path proven by counted
`FILE_CONTENT` or `FILE_EXCERPT` evidence instead of a fake path. When the
objective supplies two quoted literals and the first occurs exactly once in that
reviewed file or excerpt, the example uses those exact search/replacement bytes
and correct no-newline flags. This scaffolds small local models without changing
the decoder or authority boundary; ambiguous literals remain rejected.

The source prompt requires the objective-specific owner and minimum supported
change. Generic logger or singleton additions, entry-point rewrites,
placeholders, stubs, duplicate wrappers, unrelated cleanup, invented
paths/symbols/services/includes/imports, no-op replacements, textual `.ixx`
includes, destructive shortening, removed license/module/namespace ownership,
and claims that code compiled or passed tests are rejected before staging. The
transport preserves raw reply bytes separately from display-normalized chat. The
trusted controller owns preimages, hashes, risk, permissions, actor, timestamps,
review, approval, permit issuance, and execution authority.

## Reviewed Scene And GUI Authoring

Ordinary AI Authoring uses a separate bounded data protocol,
`EPOCH_AUTHORING_PLAN_V1`. The selected model may propose only `scene.clear`,
`scene.create`, `scene.reconcile`, stable-ID `scene.transform`, and `gui.create`
calls through fixed allowlists. `scene.create` and `gui.create` are idempotent
minimum-count requests: matching canonical scene objects or GUI widgets are reused
and only a missing remainder may be created. `scene.reconcile` expresses an exact
final count from zero through eight and creates or removes only the difference.
`scene.transform` applies bounded finite position, rotation, or scale values to
one exact canonical object. Counts remain one through eight. Replies are bounded
to 32 KiB and exactly one call; malformed, unknown, duplicate, incomplete, or
trailing content fails closed.

Epoch displays the parsed title, summary, and the one call before mutation. Only
the operator's visible `Apply Plan` action creates call-scoped
authority. The host revalidates the single-call plan, applies it through the same
scene and GUI semantic gateways used by human controls, and appends structured
tool evidence. Opening AI Authoring does not change the active project or enter
the Engine Development Sandbox.

Any completed local-model response that validates as this bounded authoring
protocol is staged once, whether it arrived through AI Authoring or AI Chat.
AI Chat attaches Apply Plan and Discard directly below the staged response so
the approval belongs to the proposal instead of a distant inspector panel. The
proposal remains unapplied until Apply Plan is pressed. Successful application
reports created, removed, reused, and transformed results, advances the
canonical scene revision, and refreshes the scene view. An idempotent create or
reconcile milestone whose target already exists records reuse rather than adding
a duplicate and requests a distinct unmet milestone. Every attempted milestone
signature is retained for the active goal. The semantic tool-call signature,
rather than model-authored title or summary wording, detects repeats across the
whole goal; a repeated satisfied call pauses the goal. Any other no-op plan pauses
immediately instead of claiming completion.

`/plan <request>` asks for one scene-aware bounded proposal. `/goal <objective>`
starts or replaces a persistent objective, bare `/goal` resumes it, and
`/goal stop` ends it. The persistent AI Chat task strip provides Play/Pause,
Edit, and Delete controls so the objective can change during a session. Each
milestone is independently parsed, displayed, approved, applied, and recorded;
the next milestone queues automatically only after the previous approved plan
changes canonical scene revision. A goal never grants continuing mutation
authority. The prompt includes stable IDs, transforms, canonical archetype
counts, and object inventory so proposals reuse existing scene meaning,
reconcile duplicates or constrained objects, transform exact objects, and
preserve unconstrained editor infrastructure.

This lane cannot save, build, run, test, edit source, start processes, invoke
native commands, alter Git, mutate the updater, publish a release, or approve
itself. A model response is a proposal and never authority.

## Exact-Content Source Executor

`ai.development_executor` applies a bounded source-only transaction. In the
editor-driven lane its workspace root is the unique disposable iteration root;
the live source root remains a separate read-only snapshot authority. It accepts
a real guard permit plus operation IDs, canonical workspace-relative paths,
approved preimages/postimages, and exact replacement bytes. Contract proof
requires the live file to remain byte-identical while the iteration copy receives
the approved postimage. The iteration root isolates accidental model writes but
is not claimed as an operating-system security boundary. The executor then:

- canonicalizes the workspace and rejects traversal, duplicate/aliased paths,
  symbolic links, non-regular destinations, invalid portable names, and
  out-of-budget files;
- verifies existence, byte count, and SHA-256 preimages before preparing work;
- creates same-directory temporary files exclusively, flushes their contents
  with `FlushFileBuffers` or `fsync`, preserves standard filesystem permissions
  for replacements, and re-reads the exact approved postimage;
- revalidates parent chains and destination preimages immediately before each
  commit;
- uses same-filesystem replacement, verifies every committed postimage, and
  records per-file preimage, replacement, commit, rollback, and manifest
  evidence;
- attempts rollback after a partial commit or exception and verifies restored
  preimages before reporting rollback complete.

The controller mutex and one-time execution claim prevent concurrent reuse
inside the registered process. This is not a general filesystem transaction or
an operating-system sandbox. A hostile external writer may still race between
checks; directory crash journaling and directory durability are not complete;
and standard permission copying does not preserve every Windows ACL, alternate
stream, POSIX ACL, xattr, ownership, or platform-specific metadata. Those limits
must remain visible until OS-specific directory-handle/locking, journal
recovery, and metadata policies are implemented and proven.

## Other Executor Responsibilities

Registered project and development executors may cover:

- inspect project, scene, script, diagnostics, and capability state;
- create a project through the validated project shell;
- apply exact reviewed document, script, or source changes inside approved roots;
- save canonical authoring state;
- build through strict `project.lifecycle` evidence where the adapter supplies it,
  or through the dedicated
  verified script compiler;
- run an explicitly approved child project or script;
- execute bounded contract tests;
- capture editor/runtime evidence and return structured results;
- cancel bounded work and preserve a visible trace.

Project creation, Save, Build, and external Run use the same admission, path,
package, process, generation, and evidence contracts as human controls.
Selected-script compilation is a separate C++23 operation and cannot mark the
project built or authorize external Run.

Engine-source writes require engine-source permission, exact content states,
review, operator approval, and resulting evidence. Ordinary AI development
authority cannot commit, push, publish, release, edit the sealed `v0.89.06`
updater/release lane, start a listener, or grant itself broader permission.
Process-native editor code is trusted host code; model-facing tool schemas must
not expose approval, permit issuance, or unrestricted native invocation.

Bounded source-context request parsing, passive path review, explicit reviewed
source-byte handoff, grounded proposal parsing, and sandbox staging are
implemented. Reviewed scene and GUI creation are the first non-source authoring
lane. Save, build, run, test, capture, live iteration orchestration, and broader
multi-tool dispatch remain unfinished; until each tool uses an existing host-owned
authority and validated evidence contract, operator-invoked editor/project paths
remain the real executors.

## Evidence Storage

Runtime exchanges and traces are evidence, not training data:

- `workspace/model_exchange.jsonl` records explicitly retained exchanges;
- `workspace/tool_trace.jsonl` records structured tool calls and results;
- `cache/models/` stores operator-managed model assets;
- `cache/ai/` stores disposable runtime cache;
- `Engine/ai/evals/` stores deterministic behavior/evidence cases;
- `Engine/ai/evals/fixtures/` stores explicitly reviewed non-training fixtures
  and expected-result material.

Normal chat is not automatically written to disk. A visible Capture/Trace action
or harness execution creates evidence. Hidden reasoning is never displayed,
harvested, or treated as authority. Raw microphone audio is likewise excluded
unless the operator explicitly enables a named diagnostic/evaluation capture;
normal voice use retains at most the submitted transcript under the same rules
as typed input.

Every retained evidence record identifies the selected model, project/source revision,
tool schema, proposal digest, review, approval where required, permit serial,
calls, results, errors, evidence paths, budgets, cancellation, and terminal
state. Failed, missing, unverified, or mismatched evidence cannot complete work.

## Epoch-Local Runtime Package

`local_ai_llama_cpp_runtime` is an executable-local, versioned package
installed only by the explicit tracked installer. Epoch launches the verified
`llama-cli` as a captured direct child; it does not launch `llama-server` or
create a listener. The runtime and GGUF retain separate provenance and receipts.
Project selection is only a provider reference to the shared Epoch cache, so
generated project source and build output never duplicate the model. External
model inference remains independently selectable through an operator-managed
endpoint while Epoch retains MCP validation and execution authority.

## Safety Invariant

Generated games, tools, apps, and server source may be proposed and authored
after operator approval. Listener start, port binding, server processes,
downloaded native extensions, model-accessible control surfaces, release work,
and other external side effects remain inert behind their own human-owned gates.

> The model proposes immutable work. The operator approves exact authority.
> The guard issues and records one execution claim. Verified evidence closes the
> loop.
