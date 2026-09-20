# Active Pass

Updated September 20, 2026. This is the only current resume point. Historical
checkpoints/evidence belong in `changelog.txt` and Git, not alternative
instructions. Ordering is in `roadmap.md`, durable requirements in
`mission_cache.md`, and behavior in the owning subsystem contracts.

## Current Acceptance Result

September 20 resume: native LM Studio inventory (`/api/v1/models`) now reads
top-level LLM keys, includes unloaded models, and excludes embedding/instance
identifiers. Compatibility discovery is a fallback only for unsupported native
routes. Model Settings has session-only Paste API Key / Clear API Key controls;
LM_API_TOKEN remains an optional environment fallback. Generation still uses
the OpenAI-compatible chat endpoint, not the Bionic agent protocol.
The supplied logo is preserved as `Engine/resource/epoch-logo.png`; the tracked
conversion script generates the seven-size Windows icon without altering it.
Serial Release build `build/native_model_discovery_release.log` and its pure
contract run passed September 19 at 15:16:49. Current executable: 13,317,120 bytes,
SHA-256 `78b8d8a19ded6938908110754e137c53ac3f521d66376761c14f34618736b59b`.
This supersedes the narrower build identity immediately below.

The operator explicitly approved the full native Qwen two-candidate test on
September 20. Preflight currently returns HTTP 401 and this task process has no
LM_API_TOKEN. The Windows computer-use helper failed before initialization with
`apply deny-read ACLs`. Subsequently the operator chose manual in-Editor token
entry; the rebuilt Editor launched successfully as PID 36248 with private data
under `build/native_auth_eye_20260920`. It is responsive; no model generation or
comparison is claimed. Obtain the credential through operator UI, never chat/source/logs,
and restore the visual-control lane before claiming interactive acceptance.
GitHub is reachable; Engine main can advance without force. Gui and Extensions
GitHub/Site histories diverge: reconcile additively, do not replace the richer
GitHub package tree with the reduced Site descriptor catalog. No new release,
stable-ref advance, or Site READY handoff is justified by these contracts alone.

September 19 immediate repair: restore local-model connectivity first. The live
localhost:1234 server now requires authentication; an unauthenticated inventory
request was rejected and the operator-provided credential successfully listed
models. This new authentication setting does not explain the historical failures.
Use LM_API_TOKEN from the Editor process environment for the default local
LM Studio endpoint; never save the credential in source, docs or runtime logs.
Discovery and ordinary responses now allow three minutes; each local self-coding
and source-review request allows three hours, including loading/evaluation.
These are request deadlines, not a limit on the overall multi-iteration mission.
The existing 90-minute automated smoke harness and external MCP deadlines remain
separate. Serial Release build passed (`build/local_model_connection_release.log`)
and the rebuilt build-safe aggregate passed with waited exit 0 at 10:05:46.
Executable: `x64/Release/EpochEditor.exe`, 13,270,528 bytes, SHA-256
`8c7e3153174c66238492da7f554a4fc4606fc1397b6104bf9e04902c0c5b3c3f`.
No GUI launch or generation request was made in this repair pass. Authentication
was tested through a separate read-only HTTP inventory request; the Editor must
inherit LM_API_TOKEN at startup for its new authentication path to work.
After connectivity, resume cumulative whole-project context navigation and the
real two-build Keep/Choose acceptance below; neither is proven by this patch.

Finish the **v0.90.1 Self-Coding Candidate Lab**: ordinary-language objective,
automatic source discovery, saved plan, exact sandbox edits, build/test and
bounded repair, separately supervised candidate PID embedded in the parent
context grid, Keep Current / Choose Candidate, then a second actual build from
the one chosen sandbox parent. Neither a staged patch nor a successor plan alone
completes this result. The operator must not need to name files or systems.

Routine work within an explicitly started, authorized sandbox session should
advance automatically and visibly. Keep/Choose is the sandbox lineage decision;
live-source promotion is separate and never implicit. Project source, other
campaigns, Git, releases, Site state, listeners and host validation authority
remain outside model control. Existing directory routing is NOT OS confinement.

The operator requested resumption after the documentation checkpoint. Resume
the next production repair below. Preserve the major playable 2D game goal and
all other unfinished missions; today's integration/SDK target does not delete
or redefine them.

## Current Source And Evidence

Current implementation checkpoint: `f36acfd028d94b0f74a99b8e5f343f5577c5dbab`,
following stage/wire repair `3f459b8f`, implementation `93d542f4`, planning `056ab2e9`, implementation
`f494ed5f`, receipt/resource `dc96a778` and host-alias `7d538759`.
The focused four-file Stop/timing/budget repair and earlier implementation batches are saved.
This table supersedes the stacked reset notes; implementation and current-build
evidence are separate.

| Area | Actual state | Missing acceptance / next action |
| --- | --- | --- |
| Stage-specific model contract | Host-owned first-line envelope selects plan/selection/edit; plans remain numbered prose, HTTP selection/patch uses JSON schema, direct CLI uses canonical packets. Repair references cannot switch stage; codec-valid source literals are not rejected by chat prose heuristics | Actual request-body/codec/repair/quoted-marker/literal contracts pass in Debug and Release; real Qwen plan-to-build progression remains unverified |
| Session usability and setup cost | One session-wide Stop covers queued/model/build/comparison/retirement work. Monotonic total time survives retries and Keep/Choose alongside current-request time. Plan/selection output is limited to 4,096 tokens; patch/repair retains 32,768. Concise planning and content-free timing/size diagnostics implemented | Rebuilt Debug/Release contracts pass, including actual Keep/Choose continuity. Exact native readability, Stop and Qwen latency remain unverified; elapsed state is process-local, not restored from older saved sessions |
| Host-alias compiler handoff | Trusted executable canonicalization and 91,520 same-group dock-move regression passed at `7d538759`; redundant Qwen clamp was not promoted | Reuse these foundations |
| Compiler failure receipts | UTF-8-safe summary/full-status SHA/causal error/32 KiB repair context retained; accepted repair starts a fresh candidate receipt set; all seven exact-candidate actors required | Rebuilt verbose-failure/repair/seven-pass and invalid receipt contracts pass; native malformed-build repair remains open |
| Repair prompt budget | Mandatory source/causal evidence/hash/failed proposal precede optional catalog; complete PATH lines fitted within 256 KiB; max-budget and source-reselection regressions pass | Native model use of the preserved repair context remains open |
| Validation transaction | Copy-staged orchestrator transaction and immutable generation/digest records; atomic outer checkpoint, exact resume and redirect guards implemented | Rebuilt Debug orchestrator fault contracts pass; native persistence/repair continuity remains open |
| Resource admission | Global heavy-work ownership, measured CPU/RAM and nonblocking 30-second healthy/cooldown admission wired through source/project model, compiler, tests and previews, including floating chat and parked context retirement | Component CTests 2/2 and rebuilt Debug source-path contract pass; native dispatch/retirement and generated-child resource behavior remain unverified |
| Task activity | Toolbar uses active/queued/idle counts; Systems reads nullable existing scheduler | Exact Release eye test required |
| Version preparation | Script resolves 0.90.1, historical 0.89.06 and separate macOS 0.89.30; receipt platform resolver and updater contracts repaired | PowerShell 7/5.1 self-tests and rebuilt Debug updater contracts pass; exact Release/package evidence remains open |
| Model timeout | September 19: local source/review attempts allow 10,800 seconds; chat/authoring and discovery allow 180 seconds. Total-budget timeout is terminal; only an earlier recoverable failure retries once. MCP retains its separate 900-second deadline | Release aggregate passes; real Qwen loading/background/minimize/Stop proof remains open |
| Model/UI foundations | Model memory/default, hidden-pane progression, Stop/restart ownership, candidate-data routing and centered-divider geometry have prior contract/build evidence | Exact current native readability, cancellation, docking and two-build Qwen acceptance still open |
| Candidate execution | Private data roots/process supervision implemented; optional LPAC component has partial console proof | Candidate Lab still uses inherited OS identity; filesystem/network/IPC confinement and embedding compatibility unproven |
| SDK/research/demo integrations | Requirements and planned contracts recorded | No SDK access/UI, curated research loader or new demo/Space library integration claimed |

The exact current serial Debug Editor build exited 0
(`build/self_coding_session_debug.log`). Its rebuilt build-safe aggregate completed
14:26:09–14:26:20 with all 180 checks passing, including actual stage-specific requests,
terminal metadata, MCP deadline, repair followed by seven fresh validations
and atomic checkpoint faults. The serial Release build also passed
(`build/self_coding_session_release.log`); its 14:32:04–14:32:09 aggregate passed
all 180 checks, with process exit 0 explicitly waited. Release EXE: 13,263,872
bytes, SHA-256
`01beda26c452c8ef71d73fc2fa373a07f31201da5411a57048d3ab02fbfe1a2e`.
Existing duplicate-logger and optimization-override warnings remain. No native
Qwen comparison, WSL package, Site publication or stable-ref advance is proven
for this tree. Builds do not substitute for pixels or security.

## Actual Failure To Repair

The operator's September 6 14:10:21 screenshot shows an exhausted 1,800-second
request, redundant Stop controls and no total session timer. The original
13:08–13:38 model log has been removed; the operator confirmed no retained copy.
The replacement `.2.log` begins after the failure. Do not infer that model
reasoning, focus or transport silence caused that particular timeout. The host
previously gave planning/selection the same 32,768-token output allowance as
patch generation. `f36acfd0` separates those setup budgets and adds the single
Stop and total timer; smaller budgets are not proof of faster successful Qwen
output. No new model/GUI request ran in this pass. Next acceptance must check
useful complete setup replies, real elapsed/Stop behavior and the actual build
loop; do not ask the operator to repeat a run solely to recover missing logs.

The operator's September 6 10:47:42 Qwen screenshot proves a separate host
prompt contradiction: `EPOCH_SELF_ITERATION_PLAN_V2` requested a numbered plan,
but the common source workload system instructions demanded an `EPOCH_SOURCE_`
header and atomic patch. HTTP selection/edit prompts also demanded line-framed
packets while supplying a JSON response schema. `3f459b8f` fixes these conflicts
without loosening edit admission. Source/failed-patch text no longer selects
the response stage via substring search. Exact decoded code packets may contain
`reasoning_content`/`<think>` literals without the prose filter rejecting them.
Next evidence is an actual model plan followed by edits/build/repair/comparison,
not another request-format contract or a claimed model capability improvement.

September 6 campaign `fbd95e52...` staged source and ran MSBuild. The OpenGL
proposal added an unmatched brace; the earliest causal failure was
`opengl.context_init.cpp(156,1): C1075`. Its 66,198-byte compiler status exceeded
receipt limits and was rejected as stale/malformed, preventing repair. Later
MSB4181 messages are consequences, not separate causes.

- Preserve the malformed candidate; **do not promote it into live OpenGL**.
- Historical model evidence: `C:/Users/iammi/.lmstudio/server-logs/2026-09/2026-09-06.1.log`
  was inspected earlier but has since been removed; do not treat it as available.
- Candidate: `Engine/examples/EpochEditor/workspace/cache/ai/iterations/session_117223600226305`.
  Its `logs/ai_source_debug_build.msbuild.log` and `.output.log` retain full output.
- Host evidence: `x64/Release/logs/Engine.AI.Candidate.log`.
- Orchestrator: `x64/Release/cache/ai/orchestrations/engine/ca36a260b432e69b82a470642a6aff607801ed86ee52be7afc98fe8ca65b8554/state.epochai`.

These are disposable local evidence locations, not tracked source or release
payload. Do not rerun a model merely to recover an already saved failure.

The later operator-reported disconnect is separately diagnosed: the Epoch host
requests beginning 08:09:19 and 08:19:21 disconnected at 08:19:19 and 08:29:19,
matching the old 600-second attempt deadline. Background OpenGL owner ticks
continued; focus/minimize did not cancel those requests. Later LM Studio records
include another client's tests and are not evidence of another Epoch attempt.
Never dump unrelated model reasoning, prompts or process credentials to report
transport status.

## Exact Next Production Work

1. **Respect the current native-launch stop.** The September 6 native Qwen
   launch from the exact Release folder was rejected by execution policy before
   CreateProcess. No test data root, Editor process or model request started.
   The model lane was released back to its owner. Do not retry through another
   tool, launcher, script or command shape to bypass this denial. The operator
   may launch the rebuilt Editor; inspect resulting logs or proceed only through
   a legitimately available/authorized native lane. The green `f36acfd0`
   source checkpoint and all older goals are preserved; do not substitute SDK,
   demos, releases or broad feature work for this unproven P0 gate.
2. **Exercise and repair the real workflow.** Use an ordinary-language,
   genuinely unfinished `EPOCH_EDITOR_SELF_CODING_OBJECTIVE`, not an existing
   feature. Run Qwen through failure recovery, all validation, distinct embedded
   comparison and Choose, then another accepted Release compile from that parent.
   Prove Keep, Stop and context-close retirement too. Repair the first causal
   failure before retry; a new plan alone is not a successful successor.
3. **Qualify admission and interaction in that run.** Observe nonblocking
   CPU/RAM/30-second waits, real activity/elapsed/Stop, source/project separation,
   candidate attachment, chosen-baseline survival and loser/worker retirement.
   Confirm exactly one self-coding Stop, total time through retries/build/choice,
   and separately labeled current-request time. Totals currently survive only
   within the Editor process; old persisted sessions have no timing history.
   Use the Windows computer-use skill for native pixels/input. Do not call the
   automated harness's HWND/identity checks visual acceptance.

Native launch preparation is source-reviewed: use the physical checkout's
asset-bearing `x64/Release`, `--editor --parented --renderer opengl` and a fresh
precreated real absolute `--candidate-data-root C:/tmp/<unique-test-root>`.
Set process-local `EPOCH_EDITOR_AUTO_COMMAND=self-coding-local-smoke`, explicit
Qwen model/localhost:1234 endpoint and `EPOCH_EDITOR_SELF_CODING_CHOICE=choose`.
The unfinished objective is two successive Candidate Lab feedback changes:
first exact seven-check progress/states, then readable Keep/Choose/Stop and next
step guidance. Do not ask it to reimplement a feature already present. The rig
disables parent update checks and cannot invoke publication/promotion. Its
90-minute overall timeout remains separate from each model/build request.
Trace: private-root `logs/epoch_editor_auto_command.log`; require explicit PASS
after two distinct validated artifacts/PIDs and retirement, not exit code alone.
Coordinate LM Studio start/clear with task `01a03627-5f71-7521-902c-64a31631367d`,
which reported its probe lane clear and received Epoch's subsequent CLEAR after
the launch denial. This agent started no model request and loaded/ejected no
model during the wire-repair pass; the operator's Qwen run supplied the new
prompt-conflict evidence. WSL is not reserved by this task; recheck the lane
before any future compiler/runtime work.

The operator authorized Site publication **after completion/checks** and asked
to replace outdated current-facing source/release/download listings. Site task
`01a03f60-0009-7ab2-b0cf-679ccfd9a78d` acknowledged the conditional instruction:
wait for exact final source/platform artifacts/receipts and explicit READY;
show only the admitted current release, preserve immutable rollback objects.
No Site mutation, public upload, GitHub push, historical deletion or stable-ref
advance occurred from the wire repair or this authorization notice.

Independent agents may work on disjoint bounded source/contracts while the root
integrates and serializes heavy work. A minimal curated local architecture/API/
failure pack may support P0 under the planned research contract; web research,
a full SDK or broad context rewrites must not replace this acceptance.

## Safety And Native Evidence Still Required

- September 5's visible run was operator-approved but tool-denied before startup.
  That is historical evidence, not a permanent current-permission diagnosis.
  Check present tools and authorization before a new native run; do not bypass
  a fresh denial. Follow AGENTS.md's exact-run and asset-bearing-folder rules.
- Optional Windows restricted-child console proof passed token verification,
  synthetic read-only/writable/outside-file checks, hardlink cleanup, DACL restore
  and process/profile retirement. Network qualification **failed at Winsock
  startup 10107 before socket creation**; it does not prove connection denial.
- Qualify compiler/test/preview grants, immutable validated inputs, separate
  writable runtime state, host receipts outside child grants, trusted
  MSBuild/VC/SDK/vcpkg closure, network/IPC restrictions, embedded HWND compatibility
  and abrupt-parent cleanup before claiming isolation. Linux descriptor cleanup
  is not filesystem/network/IPC confinement.
- Current resource sampling covers Windows/Linux CPU/RAM only. CPU needs two
  observations; stale/unavailable metrics stay visible. macOS and GPU/VRAM
  admission are unsupported/unproven; no universal overload-protection claim.
- Inspect processes before reserving WSL. None was reserved at the checkpoint;
  old messages are not a live lane lock. Never kill another task's reusable
  MSBuild nodes or model processes.

If native execution fails, hangs or destabilizes graphics, stop that probe,
preserve logs/owned-process state and continue safe source/build work. If an
essential authority or isolation gate is unavailable, record the exact blocker;
SDK, demos or other features do not substitute for P0 completion.

## Acceptance Ledger

- [ ] Maximum-budget requests preserve source and repair evidence; oversized
  logs, malformed receipts and failed persistence cannot strand state.
- [ ] Transport retries once; source/proposal packets at most twice; navigation/
  repair budgets stay bounded and cancellable. Unsafe/stale/unauthorized work
  does not gain authority through retry.
- [ ] Explicit > remembered eligible same-endpoint > Nemotron 4B assistant
  selection works, with coding-role guidance and no silent ejection/substitution.
  Each request shows phase, elapsed, next action and Stop.
- [ ] CPU/RAM admission, 30-second cooldown, comparison pause and global heavy
  ownership work through real dispatch, not only controller tests.
- [ ] Real Qwen reaches two accepted builds across Choose; Keep retains its
  original parent; loser retirement and saved-plan continuation are observed;
  no orphan workers or duplicate baseline PIDs remain.
- [ ] Exact Release eye evidence covers choice controls, narrow/high-zoom action
  labels, input capture, floating/redocking and saved centered dividers.
- [ ] Candidate filesystem/network/IPC tests prove the claimed boundary against
  live source, other projects/candidates, secrets and host receipts, including
  abrupt stop. Unsupported/failed platforms are reported individually.
- [ ] One exact tree has consistent versions, final builds/contracts/native
  evidence, immutable packages, checksums/receipts and rollback verification.

## Today's Follow-On And Release Boundary

`roadmap.md` owns the September 6 target: P0; current GUI/Extensions and loadable
software/project workflows; proven software/context compatibility freeze before
major game changes; separate Sim/Space demo/library integration; private SDK/
About/Site last. The full playable **2D game remains a major product goal**, not a
removed goal or a synonym for the demos. Apply the architecture alignment review
each pass. The target date never waives evidence.

Local **v0.90.1 preparation is incomplete and unpublished**. Last recorded public
Windows/Linux runtime/private-source authority is v0.89.34; macOS is v0.89.30.
This note is not fresh Site verification. Preserve immutable
v0.89.34/.33/.30/.29/.28/.27/.06 and independent library versions. Advance the
authorized stable branch only after compatibility acceptance, preserving
`ad6c416d930b348a61bc37ceb7d4522742be084a`. Checkpoints do not publish, tag or
advance refs. Never stage caches/logs/generated projects/captures/unrelated files.
Protected backend frame/queue/replay order remains unchanged.
