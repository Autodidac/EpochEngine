# OS AI, MCP, Harness, And Evidence Policy

Epoch local source candidate `v0.89.35` does not train or silently activate a
model. Public Windows/Linux source and packaged-runtime authority is
`v0.89.34`, while macOS remains `v0.89.30`. The candidate can
invoke the verified Epoch-local Qwen3.8 installation or offload inference to an
operator-managed external model machine, and it keeps model transport, MCP
authority, execution, and evidence as separate concerns. The published
historical packaged runtimes and their updater evidence remain sealed.

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
model byte count. `ai.model_install` now owns the same compiled authority for
Package Manager plus an exact ten-file Nemotron 3 Nano 4B BF16 plan. Explicit
transfers resume into sibling staging, reject symlinks and unexpected files,
verify every size and SHA-256 off the GUI thread, and atomically publish exact
receipts under `cache/models/<package>/versions/<revision>/`. The tracked
`Tools/ai/install_epoch_qwen38.ps1` lane shares that immutable Qwen layout and
can migrate an already verified legacy file without redownloading. External
model compute plus Epoch MCP remains a peer choice; install never activates a
provider, inference process, server, or listener.

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

`ai.mcp_stdio` is the transport-neutral, stdio-first JSON-RPC codec around that
registry. It accepts and emits one bounded UTF-8 JSON-RPC message per line, with
no embedded literal newline bytes, and implements the `2025-11-25` MCP
`initialize` / `notifications/initialized`, `tools/list`, `tools/call`, and
cancellation shapes. The lifecycle refuses tool operations until both
initialization stages complete. Session and request identities are typed;
duplicate IDs, result replay, excessive byte/depth/field/tool/call/hop budgets,
nested model arguments, and model-selected paths, commands, URLs, Git, release,
promotion, or approval fields fail closed. Tool discovery is filtered by the
host-granted capabilities, and tool calls are still revalidated through the
existing exact session capability and per-call approval authority plus optional
host validation hooks.

The codec owns no pipe, process, executable discovery, socket, listener, or tool
executor. It yields an immutable validated pending call for a separately
authorized host dispatcher and encodes one bounded terminal result.

`ai.mcp_child_host` owns the optional local stdio boundary. It is disabled by
default and accepts only a host-approved canonical executable, digest, argument
vector, working root, bounded environment, and explicit start action. The host
uses injected process and byte-channel ports, never a shell, socket, listener,
or server. Timeouts, stderr evidence, frame/request budgets, graceful stop,
force-stop, and cancellation remain host-owned. Its contracts use fake ports
and launch no process.

Candidate Lab also owns the opposite direction: an explicit local MCP client
rig for asking an operator-selected coding agent to produce the already-defined
bounded plan/proposal packets. On Windows the packaged
`assets/ai/epoch.local_mcp_iteration.ps1` bridge is launched directly as a
supervised hidden child only after `External MCP` is selected and a disposable
`cache/ai/iterations/session_*` workspace exists. The bridge launches the
installed `codex mcp-server` with redirected standard streams and no shell,
performs `initialize` and `tools/list`, and calls `codex` with approvals denied
and workspace-write rooted at that disposable session. AI Controls displays the
bridge PID, elapsed time, receipt, and a real stop action.

Codex MCP reply-thread identity is scoped to one MCP server process. The bridge
therefore does not claim cross-process `codex-reply` continuity. The engine
persists the reviewed mission plan and chosen-candidate checkpoints and includes
them in each next fresh request. The bridge receipt records bridge/server PIDs,
elapsed time, exact executable SHA-256, and exact prompt/response SHA-256; the
editor deletes transient prompt/response files after ingesting the response.
No MCP response bypasses the exact protocol parser, reviewed-source bounds,
disposable transaction, trusted build actors, candidate comparison, or later
human-owned promotion boundary.

`ai.iteration_session` is the non-GUI coordinator for one bounded source
candidate. The trusted host supplies the selected model, verified authority,
objective, and files; the coordinator rehashes every curated current file before
accepting the scope. A cached authority receipt preserves the authenticated
archive version, commit, format, and archive SHA-256 provenance, but it is not a
claim that extracted files remained unchanged. An explicit checkout may also be
dirty relative to its reported Git commit. Current curated-file digests are the
source bytes authorized for that request.

The ordinary one-change policy remains `manual_each_candidate`. Candidate Lab
enables `auto_validate_within_approved_scope` only after the operator has
reviewed one objective and exact source scope. Within that boundary the model
may produce successive sandbox candidates and the host may apply and validate
them without repeating approval UI; strict proposal, preimage, transaction,
actor, receipt, generation, digest, and repair checks are not bypassed. No
candidate can promote itself to live source.

The durable orchestration pipeline is now source- and contract-complete:

- `ai.iteration_campaign_queue` (`445bc3f0`) owns bounded durable
  admission, ordering, cancellation, replay refusal, and checkpoint state.
- `ai.iteration_campaign_scheduler` (`9ea6deaa`) binds that queue to the
  existing MCP/orchestrator boundary without moving execution authority into
  transport.
- project session admission and deterministic restoration are checkpointed at
  `8ab6cc5c` and `7711648d`; cross-project, stale-generation, tampered, or
  authority-broadened restoration fails closed.
- `ai.iteration_supervisor_control` (`c101507d`) owns explicit query,
  pause/resume/cancel/retry/approve/reject transitions and their receipts.
- `ai.curated_context_bundle` (`c8e1fdb8`) accepts only host-supplied
  reviewed bytes, emits bounded evidence metadata/chunks, and never scans or
  reads paths.
- `ai.mcp_supervisor_adapter` exposes the supervisor's caller-fed inbound
  control surface as a canonical JSON-RPC allowlist. It provides read-only
  campaign and curated-evidence queries plus explicit
  pause/resume/cancel/retry/approve/reject requests. Every request is bound to
  adapter, session, actor, campaign, project, curated session, host generations,
  state digests, time, replay state, and an operator-approved call boundary.
  Its checkpoint is size-bounded, canonical, integrity-checked, and refuses
  cross-session or stale restoration.
- deterministic source proposal and staging are owned by `2daec382` and
  `a39ea309`; the editor's exact sealed-review surface is `60ce0032`.
- deterministic local-build admission receipts are emitted by `148fffa0` and
  documented at `eadfea94`. They are Site-readable evidence with upload and
  release authority false.

The editor operational flow is deliberately guided rather than disconnected.
Starting Candidate Lab constructs its queue, scheduler, supervisor, and bridge,
shows the selected provider and endpoint, then submits the first bounded plan
request. The returned numbered plan is digest-bound, displayed, retained across
candidate selections, and supplied with selection checkpoints to each next
generation. Same-scope proposal, disposable apply, validation, and bounded
repair proceed automatically until a validated candidate is ready for native
comparison. This does not approve live-source mutation, promotion, Git, upload,
publication, listener, server, or release. Complete model responses remain
visible in AI Chat when the operational preview is truncated.

`ai.mcp_supervisor_adapter` is registered in the Engine source/build graph and
the build-safe aggregate contract. It remains a transport-neutral protocol
surface, not an implicitly running server: it opens no socket, pipe, process,
listener, or model endpoint and cannot read files or source bytes. The host must
inject the current supervisor query/submit gateways and optional curated bundle.
All authority-bearing command flags for source writes, arbitrary reads, model
launch, promotion, release, servers, and listeners remain false. A future stable
transport may carry these canonical request/response bytes, but transport does
not acquire supervisor or live-source authority by doing so.

- `ai.project_profile` strictly decodes generated-project choices for disabled,
  Epoch-local, shared, or external MCP operation. It preserves
  `engine_source_write=false` and rejects autostart, listener, server, network,
  or invented provider authority.
- `ai.iteration_campaign` stores bounded campaign reports atomically beneath the
  admitted cache root. Resume revalidates source authority, curated-file
  identity, budgets, generation, and state digest; interrupted work never
  resumes as trusted evidence.
- `ai.mcp_campaign`, `ai.mcp_orchestrator_bridge`, and `ai.mcp_child_host` join
  the strict stdio protocol to campaign/orchestrator actions without moving
  execution authority into the transport. Read-only inspection is synchronous;
  every mutation remains an immutable host-pending receipt bound to request,
  call, campaign, session, generation, state, and evidence digests.
- `ai.self_iteration_orchestrator` owns the deterministic plan, curated
  evidence, proposal, manual review, apply decision, sandbox result, validation,
  checkpoint, resume, and cancellation state machine. Engine-source and
  generated-project campaigns have distinct authority handles.
- `ai.source_patch_bundle` owns the narrow text-only unified-diff artifact. Each
  admitted file is bound to a curated relative path, exact preimage SHA-256,
  size, encoding, and line-ending policy. Path escapes, binaries, symlinks,
  duplicate/overlapping hunks, ambiguous input, replay, and partial commits fail
  closed through an injected disposable-sandbox file port.
- `ai.iteration_patch_adapter` binds one reviewed bundle and one approved MCP
  receipt to one disposable sandbox transaction. It exposes per-file/hunk review
  evidence, verifies postimages, journals stage/commit/evidence transitions, and
  recovers fail-closed if the host crashes between them.
- `ai.iteration_validation_adapter` schedules the seven trusted Debug, Release,
  HeadlessCI, and full-validation actors only through injected host tasks. Every
  task/result is bound to campaign, session, generation, orchestrator state,
  candidate, bundle, authority, source commit/tree, toolchain, configuration,
  task, receipt, and evidence digests. Ordered or explicitly bounded parallel
  completion, retry budgets, cancellation, crash resume, and stale/replay/
  forged/out-of-order refusal are contract-proven.

The final adapter reuses `epoch.build_validation` receipts and admission policy.
It emits deterministic machine-readable admission JSON suitable for later Site
display or ingestion, but the record always states
`upload_permitted=false` and `release_permitted=false`. It contains no upload
credential and grants no Git, updater, publication, promotion, process, network,
listener, or server authority. A trusted local build host must still perform the
actual tasks and a separate human-owned publication pass must admit any release.
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

1. The operator enters the desired result in ordinary language. The host
   enumerates the existing C++ paths beneath the selected read-only source root
   and sends the selected model only that verified names-only catalog with the
   objective. It does not require an internal subsystem, filename, or symbol and
   does not read or transmit source bytes during this selection request.
2. The model chooses a coherent slice of at most 12 catalog paths. The host
   accepts the complete `EPOCH_SOURCE_CONTEXT_REQUEST_V1` form or a bounded
   compact list, then revalidates root containment, catalog membership, type,
   uniqueness, and count. The exact admitted path list is recorded in Detailed
   Session Activity. Unknown fields, invented paths, traversal, and paths outside
   the catalog fail before any source read.
3. The host automatically opens only the validated paths inside Candidate Lab.
   Local full-file reads/hashes allow at most 8 MiB per selected file; those
   identities are not outbound model context. The combined context stays within
   184 KiB including metadata. Each file receives a fair share of that budget:
   complete counted evidence up to 48 KiB when it fits, otherwise one UTF-8-safe
   excerpt of at most 16 KiB. The exact evidence bytes—not only names
   and hashes—are included in the proposal request. If more evidence is needed,
   the model may request a revised complete working set of catalog-listed paths;
   the host performs at most three bounded reselections. The model may otherwise return exactly
   `EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1` or one strict
   `EPOCH_SOURCE_PATCH_PROPOSAL_V1` exact-block packet. The separately reported
   file/byte count describes the complete disposable build workspace copied
   locally; it is not additional model context.
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
inside the selected iteration root. AI Controls accepts the desired result in
ordinary language. The host first sends only a verified names-only catalog of
existing C++ paths; the selected model chooses a coherent slice of at most 12
paths, and the host revalidates that exact list before opening any file. The
operator does not need to name a subsystem, filename, symbol, diagnostic code,
or protocol token. AI Controls owns read-only inspection of the selected model,
endpoint, admitted paths, sandbox, current phase, elapsed work, retries, next
action, and evidence. Changing the objective invalidates the admitted source
evidence before another request can be staged. Advanced controls are read-only
evidence. No action
automatically promotes sandbox bytes into live source: a verified candidate
requires distinct `Stage Live Promotion` and `Approve Live Promotion` actions.
No chat/build/tool result becomes an automatic live-source change or training
record. No GUI eye-test or responsiveness proof is claimed for this source candidate.

### Sandbox Candidate Preview Context

The visual-comparison milestone reuses the existing child-process supervisor
and multicontext host. A sandbox-built candidate is a separate child engine
registered as a custom `candidate_preview` context from its generation-checked
process handle, PID, and visible native-window identity. The child runs only from
its candidate sandbox root; the context record also binds source, executable,
validation, and capture digests. Candidate A and Candidate B never share a
writable root or process handle.

The comparison surface presents the current editor and a candidate in the bottom
context grid. Keep Current retires the challenger; Choose Candidate retires the
prior sandbox child and retains the chosen child as the next sandbox parent;
Stop Lab retires both. The buttons and native regression harness use the same
panel transition. The next iteration materializes from the selected sandbox
bytes and retains the mission plan. Selection does not copy bytes to live source,
replace the parent editor, promote a candidate, or grant
Git, updater, release, package, network, listener, or server authority. Automated
live-source replacement is deliberately not part of this milestone.

This is currently application-level transaction isolation plus process lifecycle
supervision, not an OS security sandbox. Windows candidates still use the
launching user's token. Candidate Lab compiler, test and preview launches now
replace the ambient environment and disconnect host stdin. A Job Object owns
retirement but does not restrict filesystem or network access. Adversarial
candidate-execution containment remains unproved; source/path or environment
contract tests must not be presented as that proof.

The platform launcher distinguishes legacy environment inheritance from an
explicit allowlist, including an explicitly empty environment. It rejects
duplicate names, invalid UTF-8/NUL data and oversized blocks, and never logs the
values or changes the host environment while preparing a child. Windows supplies
a sorted Unicode block and dedicated NUL stdin through its existing handle
allowlist. POSIX prepares the explicit execve vector before fork and checks
stdio setup before exec; the Engine's Linux candidate compiler remains
unconnected. See [Windows environment blocks](https://learn.microsoft.com/en-us/windows/win32/procthread/changing-environment-variables)
and [process creation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).

Linux supervised launches now close every descriptor above stderr except the
CLOEXEC setup-error writer before exec, including sparse descriptors above a
lowered descriptor limit. This requires the Linux `close_range` syscall
(kernel 5.9 or later and matching build headers); missing or denied support
fails before execution, without an incomplete close-limit fallback. The setup
channel handles interrupted/partial transfers and preserves the original errno.
Other POSIX platforms retain their existing descriptor behavior. This closes
an inherited-capability leak; it does not deny new file opens, sockets or IPC.

Windows retirement uses a signalled process handle, not the `STILL_ACTIVE`
numeric exit code, to determine parent exit. Before job termination it captures
bounded, verified member handles. The exclusive group remains owned until the
parent signals, job accounting is empty and observed member handles signal;
zero accounting alone was observed before a descendant handle signalled.
Missing observation evidence remains a retirement error even after a stop was
requested. A short/incomplete native job list receives at most two rechecks with
a short yield; partial IDs are never accepted as a complete snapshot. Persistent
failure records the native error and assigned/returned counts without dropping
ownership. Departed/recycled snapshot PIDs require a fresh complete job-list
confirmation, with at most two retries while still listed; other failures are
not treated as ordinary exits. This is lifecycle synchronization, not an adversarial OS boundary or
a proof against every concurrent process-creation race. The console regressions
exercise exit code 259 and a real grandchild during normal exit, cancellation and
timeout. See [process termination](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess)
and [job accounting](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information).

Launch failure is not necessarily the absence of a process. The platform
`LaunchResult::owns_new_process()` distinguishes fresh reservations (including
failed native/security admission) from borrowed focused/busy handles. Candidate
Lab records an owned build/test handle independently of the worker's future
before diagnostic allocation. A throwing worker, stale artifact epoch or context
Close still transfers that handle to retirement; no pending retirement is
accepted as build/test evidence. Preview failure uses the same retirement pump
without stopping another request's borrowed handle. A missing snapshot alone is
not stale proof: only successful release or the supervisor's typed stale result
ends ownership. These contracts do not grant OS execution confinement.

Candidate Lab uses workspace-local `cache/process/` temp, profile and package
paths; it copies only the three validated ProgramFiles install-root values, not
host PATH, proxy/model credentials, automation flags or user-profile settings.
OS system paths and fixed noninteractive tool settings complete the allowlist.
Compiler discovery and the original read-only dependency authority remain
host-owned across Choose. MSBuild disables automatic response files and node
reuse. These paths are preferences, not permission enforcement: Windows APIs or
arbitrary candidate code can still access resources allowed by the inherited
token until an actual OS execution boundary is implemented and tested.

The source materializer always excludes the owned checkout-local runtime tree
`Engine/examples/EpochEditor/workspace`, including explicit-file includes.
Initial materialization, repair and chosen-parent succession use this same
component-aware exclusion. Model exchanges, tool traces and staged runtime
research are not compiler inputs. Unrelated source directories named
`workspace`, and the distinct `workspace_sources` neighbor, remain admissible.
Synthetic transaction tests verify retained chosen source, unchanged host and
candidate traces, exact file/byte counts and the manifest digest. This prevents
an unintended local copy; it is not a claim that real private bytes were observed
or sent to a model.

The optional Windows `WorkspaceIsolation` launch policy now provides a
backend-owned restricted-process primitive, not yet Candidate Lab integration.
It requires exact executable identity, explicit environment, disconnected stdin
and captured output. A fresh random per-launch AppContainer identity receives
zero capabilities and opts out of ALL_APPLICATION_PACKAGES access. At most 16
disjoint read-only/writable roots must be proper descendants of one host-owned
generation; local canonical paths, ancestors and bounded existing descendants
are pinned while permissions change. Admission refuses reparse points, aliases,
hardlinked files and absent/invalid DACLs before any inherited grant is applied.
The caller must establish actual generation ownership; a pathname cannot do so.

The suspended child's real token must match its fresh identity, zero capabilities,
low integrity and no elevation/UIAccess before resume. If Windows rejects the
LPAC token-information class with invalid-parameter, in-memory `AccessCheck`
tests must deny ALL_APPLICATION_PACKAGES and permit ALL_RESTRICTED_APPLICATION_PACKAGES;
an unavailable flag is never accepted as proof. Other query failures stop launch.
Read-only trees receive read/execute; writable trees receive modify, not
permission/ownership editing. Captured output is an intentional inherited write
handle and untrusted diagnostic data, never a host receipt or permission grant.

Native process/job ownership is retained across failed admission and exceptions.
Only observed whole-tree retirement permits fresh-SID grant/profile removal;
incomplete cleanup retains the supervisor slot and exclusive group. Retirement
accepts an internal hardlink set only when every link is accounted for under the
same grant mode, pins/rechecks identities and revokes once per identity. Missing,
cross-mode or redirected links fail cleanup rather than editing an outside ACL.
Normal teardown does not prove recovery after abrupt parent termination; no
destructor silently revokes permissions while a child might still run.

The explicitly invoked console-only `--epoch-isolation-contract-only` probe
copies its own component executable into synthetic fixture roots. It does not
launch the editor, render, infer, serve or run a generated project. September 5
evidence passes token verification, file-access/refusal, writable scratch,
internal hardlink retirement, prior-DACL restoration and owned process cleanup.
The strict network subcheck remains failed: `WSAStartup` returns 10107 before
socket creation, not an observed access-denied connection. No request bytes are
sent. The probe preserves this failure alongside independent passing results;
zero capabilities alone are not relabelled as an exercised network test.
Unsupported platforms reject explicit isolation, never fall back to an ordinary
token. Existing launches without this policy retain their previous behavior.

The remaining execution-boundary implementation must separate compiler writes
from validated preview inputs: compiler source/build/temp may be writable only
inside that generation; preview code, DLLs and assets must be immutable, with
separate writable runtime state. Host receipts, permits and lineage records
cannot live inside child-write grants. The Windows AppContainer/LPAC component
above is not currently active Candidate Lab policy. Its remaining qualification
must exercise network/IPC denial, compiler dependencies and interactive HWND
hosting, and recover abandoned identity leases safely. Do not grant broad user-profile/live-source access to make a failed
compatibility test pass. Linux also still needs a real filesystem/network/IPC
execution boundary; descriptor cleanup is only one prerequisite.

The current source layout is not yet a valid phase grant layout: `Engine.sln`
lives at the generation root, intermediates/output are its siblings of `Engine`,
and runtime writes still occur below both source and executable directories.
Before enabling the policy, move those consumers to distinct owned phase roots
and relocate `.epoch/local_mcp` control/receipt files outside child grants.
Installed MSBuild/VC/SDK and vcpkg dependencies are outside the generation and
cannot be admitted by the existing owned-descendant grant API. Their immutable
dependency closure and discovery paths must be qualified explicitly; copying
only MSBuild.exe or broadly granting the host profile is not sufficient.

Windows foreign-window admission verifies a live supervised PID/window and an
exact native attachment lease: parent, styles, client-slot geometry and an
opaque registration cookie. It does not install a callback in the foreign
process, publish a host pointer, destroy a foreign window, or turn attachment
into fabricated renderer frame evidence. The application retains admitted
identity independently of top-level discovery and removes the registration on
retirement, including after a crash or destroyed window. Native DPI, attachment,
input, detachment and pixel behavior still require the exact-build eye test.

Editor teardown detaches state from storage before joining work, cancels its
source task tickets before the scheduler drains, and retires supervised previews
and owned MCP workers by process handle. Whole-session shutdown requests stop
for every extracted context before joining any scheduler. Generic parent Close
does not post to raw foreign HWNDs.
Final process liveness and supervisor release determine retirement; an OS stop
failure retains the supervisor slot and reports unresolved ownership. Per-context
HTTP cleanup requests that chat's own stop token, interrupts its serial model
queue/transport wait, and discards its response. The chat retains its request
state and joins the worker before declaring retirement. Whole-engine shutdown
also cancels the captured global request epoch; later requests acquire a fresh
epoch. An old token or response cannot cancel or overwrite the next iteration.

Windows model POSTs use worker-owned asynchronous WinHTTP operations. Header
completion and fixed-length body writes are separate operations; each write
waits for its own completion and validates the reported byte count. A headers-only
REQUEST_SENT progress notification never admits response receipt. Body and
response envelopes are bounded at 8 MiB, and the entire exchange shares one
deadline. Header/timeout configuration failures are fatal rather than ignored.
Automatic redirects, ambient authentication and cookie handling are disabled
for model POSTs: the reviewed request is for the selected endpoint only. Stop
callbacks wake the worker but never close a handle from another thread while
an initiating API call is running. Request bodies, read buffers and callback
state survive until the final HANDLE_CLOSING notification. Closure has one
bounded settlement wait; uncertain retirement retains callback ownership,
reports a distinct retirement-failed terminal state, and disables further
Windows HTTP POSTs until the Engine process restarts. This state takes precedence
over both cancellation and a successful response in the service and editor;
it cannot be flattened into a clean cancellation or silently retried. This follows
[WinHTTP's concurrency rules](https://learn.microsoft.com/en-us/windows/win32/winhttp/concurrency-in-winhttp)
and [handle closure requirements](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpclosehandle).
libcurl requests use their own stop token through the progress callback;
cancellation latency depends on curl/resolver progress, not the editor frame
rate. Direct CLI requests also check the same token and bound their final output
drain. Neither HTTP cancellation nor CLI process retirement proves that a
remote server stopped inference or establishes candidate OS confinement.

Payload-free request observation distinguishes queue admission, send, actual
Windows send completion, response waiting and response receipt. Observers run
on the requesting worker, outside internal mutexes, never inside the Windows
callback. The editor copies only the stage into its request-owned atomic state;
UI rendering reads that state without touching worker output. Its elapsed time
and animated activity are not token progress. Stop takes precedence over a
late receiving/completed stage until the worker actually finishes.

The console-only `Engine/examples/AiTransportContract` probe is deliberately
absent from default builds, CTest, runtime startup and installation. Build its
MSVC project directly or enable `EPOCH_BUILD_AI_TRANSPORT_CONTRACT=ON` and build
the CMake `epoch_ai_transport_contract` target. Invocation requires all of
`--endpoint http://localhost:1234 --model qwen/qwen3.8-27b --run`; without those
arguments it sends nothing. It uses synthetic prompts only, cancels request A
after actual send/receive admission, then requires a visible canary from B while
repeating A's old stop token. It creates no editor, renderer, listener, model
installation or source-selection session. Its watchdog cancels only its two
owned requests; if retirement fails, only the probe exits with a failure code.
Neither a started request nor A's cancellation alone can pass this probe.
The B watchdog allows both attempts of the production chat timeout, plus a
small retirement allowance; it must not preempt the normal client budget.

September 5 observed result: the repaired Windows body-write path completed
upload and A cancellation with confirmed retirement; B uploaded independently
but missed the initial 90-second probe deadline. With the probe watchdog aligned
to the actual production timeout/retry budget, B returned its exact visible
canary after about 98 seconds without a retry; the probe passed with exit 0.
This is genuine HTTP ownership/cancellation and subsequent-response evidence,
not native editor/context-close, server-generation-stop or self-coding proof.

The platform child-process snapshot exposes both `platform_process_id` and
`platform_window_id`; hidden/headless children report no visible window. The
editor-owned custom-context registry, capture lifecycle, Candidate Lab evidence
UI, and explicit teardown/selection contract are implemented. The remaining
acceptance gate is one real local-model session through candidate build, visible
preview, selection, losing-process teardown, and continuation from the selected
sandbox head. The explicit `self-coding-local-smoke` rig now requires a nonblank
`EPOCH_EDITOR_SELF_CODING_OBJECTIVE` naming genuinely unfinished work. It tests
two actual accepted compiles and candidate admissions, with Choose by default
(`EPOCH_EDITOR_SELF_CODING_CHOICE=keep` is a separate comparison test). It cannot
pass merely because a successor plan arrived. Neither a default already-built
feature nor a synthetic validation record is a model-to-build success.

The visible workflow states its I/O boundary before the first send: the
objective, path-only catalog, selected UTF-8 C++ excerpts and bounded recovery
diagnostics form the model workload. Model output is a plan, validated context
request or exact source proposal. Host-mediated source writes target the
generation-owned disposable session root; project roots and live engine source
remain separate and read-only to that transaction executor. This does not confer
the still-missing OS boundary on compiled children. Contract fixtures reject
absolute paths, traversal, non-source
files, unreviewed canary content, stale revisions, and cross-root mutation, and
prove that sandbox repair leaves the live source preimage byte-identical.

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
actors pass, the manual workflow exposes a separate `Run Full Validation`
decision. An already-started Candidate Lab session queues that validation
automatically within its approved disposable-workspace scope. Both paths run
the Release editor's `--engine-validation-self-test`,
covering aggregate contracts, registered project profiles, generated-child
self-tests, and the AI gate. Failure in any actor may feed bounded verified
diagnostics into at most three fresh-generation repair proposals. Each changed
repair has a new digest. The manual workflow waits for exact operator approval;
Candidate Lab automatically requests the next proposal after successful fresh
repair materialization and continues the authorized sandbox validation sequence.
Neither mode grants live-source promotion. Cancellation and stale completions
are rejected. Only all seven evidence completions for the
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

`ai.development_proposal_codec` uses a two-stage bounded protocol. The operator
states an outcome in ordinary language; Epoch first supplies a verified C++ path
catalog without source bytes, and the selected model requests a coherent slice
of at most 12 files. The host accepts the full
`EPOCH_SOURCE_CONTEXT_REQUEST_V1` envelope and the common safe compact forms
emitted by current agentic models: `PATH <canonical-path>` or a bare canonical
path after the header. Optional reason/count/terminator fields may be inferred,
but every selected path is still deduplicated, rooted, size-limited, checked
against the offered catalog, and disclosed in Detailed Session Activity before
its bytes enter Candidate Lab. Unknown fields, traversal, missing paths, prose,
and paths outside the selected workspace remain rejected.

The second request includes the selected counted `FILE_CONTENT` or
`FILE_EXCERPT` bytes and accepts the production
`EPOCH_SOURCE_PATCH_PROPOSAL_V1` exact-block protocol. A proposal is a data
protocol, not a command language: no Markdown fences, unknown fields, trailing
bytes, model-selected permissions, or approximate edits are accepted. The model
may request another listed source slice when the current evidence is incomplete;
Epoch performs at most three context reselections and never lets the model invent
a path. Each request is the complete next set of at most twelve paths, not an
append-only list: relevant old paths may be retained and irrelevant ones replaced.
Direct context requests and insufficient-evidence retries share that budget; an
already-reserved retry is not charged twice when its selected paths arrive.
Source iteration also recognizes exactly
`EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1` as a safe refusal after expansion is
exhausted. Framed and raw direct-CLI transcripts use the same line-boundary
packet extractor, so ordinary prose cannot enter the source codec.
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

OpenAI-compatible workloads leave the selected provider's reasoning mode unchanged.
They do not send an unsupported `reasoning_effort: none`/off override or a Qwen
`/no_think` directive. Context selection and source-edit calls carry strict JSON schemas;
the transport accepts only their bounded fields and deterministically converts
them into the same `EPOCH_SOURCE_CONTEXT_REQUEST_V1` and
`EPOCH_SOURCE_PATCH_PROPOSAL_V1` packets consumed by the trusted host. The model
therefore does not have to reproduce fragile line-protocol punctuation, while
the schema adapter grants no path, permission, apply, or execution authority.
The source-edit schema accepts mutually exclusive `patch` and `context` actions.
Patch requires nonempty title/rationale/operations and empty reason/paths. Context
requires a reason and one to twelve paths, with no operations or edit metadata;
it passes through the existing context codec/catalog gate before reading files.
Legacy exact patch JSON remains readable. Unknown or duplicated fields and mixed
read/edit responses are rejected. Proposal metadata does not have to repeat words
from the operator's objective: lexical overlap does not prove relevance or safety.
Exact-byte grounding, ownership checks, actual validation and operator candidate
choice remain separate requirements.
The transport honors the declared 600-second source timeout rather than the
ordinary short chat timeout. One transport, API, hidden-reasoning, malformed-
schema, or empty-content failure is retried with an explicit final-answer
request; cancellation remains immediate and is never retried. While either
attempt is pending, AI Controls displays an animated local-model activity bar,
the selected model, and elapsed time.


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
- `cache/models/<package>/versions/<revision>/` stores exact receipt-bound,
  operator-managed model snapshots; sibling staging remains disposable;
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
downloaded project add-ons, model-accessible control surfaces, release work,
and other external side effects remain inert behind their own human-owned
gates. Epoch has no native engine plugin lane; optional compiled source belongs
to the selected generated project.

> The model proposes immutable work. The operator approves exact authority.
> The guard issues and records one execution claim. Verified evidence closes the
> loop.
