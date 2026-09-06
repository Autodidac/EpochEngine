# Active Pass

## Gate

Finish the v0.90.1 Engine Self-Coding Candidate Lab as one usable, truthful,
end-to-end sandbox succession workflow. The operator describes the desired
result in ordinary language. Epoch and the selected agentic model resolve a
bounded source slice, produce and apply an exact candidate only in a disposable
sandbox, build and validate it, launch the candidate as a separately supervised
editor PID/context, and let the operator keep the current sandbox or choose the
candidate as the sole parent of the next iteration.

This pass does not require the operator to name an internal system, source file,
symbol, or diagnostic code. It also does not grant the model authority over live
source, projects, Git, releases, Site state, listeners, permits, or validation
results.

September 5 scope/ordering: the operator authorized full native self-iteration
and engine testing with the local model, and publication through the Site task
after all build/runtime/visual/safety gates pass. Before broad context expansion,
refresh the explicitly confirmed `multicontext-base-stable` software baseline
against current Engine profiles, without an additional worktree. Preserve the
original checkpoint in history. The platform/context/CLI template architecture,
floating guide readability and gradual EpochPlatformEngine extraction are durable
missions, not reasons to label incomplete self-coding or context behavior complete.

## Current Checkpoint

### September 6 second reset checkpoint — this supersedes the parked state below

The user resumed, then requested an immediate new checkpoint. All agents are
stopped and no owned build/runtime is left running. Preserve this in-flight
source; do not mistake a checkpoint for a tested release.

- Windows Debug EpochEditor build finished with exit 0 during this pass
  (`build/self_coding_resume_debug.log`). It started before the last version/
  host-helper edits, so it is NOT exact-build proof for this final checkpoint.
  Fresh admission and scheduler component builds/CTest passed 2/2 (0.05s).
  The newly built Editor aggregate was NOT run before the reset request.
- Six version files now declare v0.90.1 (source/Windows/Linux, Windows resource,
  compatibility module, vcpkg project version, MCP advertised version and CI
  revision regex); macOS is explicitly 0.89.30. This version pass is INCOMPLETE:
  `Tools/ai/get_epoch_version.ps1` still yields 0.90.01, the receipt writer still
  inherits source major/minor for macOS, and the updater discovery contract
  still requires 0.89.35/source-package equality. Repair those three next and
  prove historical 0.89.06 spelling. Independent EpochGui was not bumped.
- `editor.application.cpp` retains 123 preparatory lines: the admission import,
  EditorState exact-action/epoch/token/lease/status fields, synchronized global
  coordinator, and `cancel_ai_work_admission`, `release_ai_work_lease`,
  `queue_ai_work`, `ai_work_pending`, `admit_ai_work` helpers. They are UNWIRED,
  untested, and do not yet enforce pacing. Finish the dispatch/final-HTTP/tick/
  cancellation/choice/visible-status hooks described below; also finish the
  toolbar and nullable Systems scheduler hooks. Do not release ownership on a
  failed close while actual child/worker retirement is unresolved.
- Independent review found the NEXT real repair-loop risk in
  `campaign_model_prompt`: the path catalog is appended before repair context.
  Reviewed source and catalog each permit 184 KiB, but the whole prompt limit
  is 256 KiB; 32 KiB diagnostics plus a 16 KiB failed proposal can be silently
  omitted. Preserve exact reviewed bytes, reserve the repair envelope first,
  then fit catalog on complete PATH-line boundaries. Add a maximum-budget test
  carrying the actual C1075 error and full-evidence hash before another model run.
- Separate follow-up: orchestrator `record_validation` can still advance inner
  state before `commit/persist` fails from I/O or state-size limits. Its new
  summary preflight is fixed; persistence transaction atomicity is not. Do not
  conflate the two or mark the broader issue repaired.
- No GUI, Qwen, candidate preview, WSL, Site publication, release tagging or
  stable-branch change happened here. SDK and demo packages remain unintegrated.

New same-day scope, explicitly requested and still OPEN: make self-coding and
iteration selection work end to end; finish current EpochGui/Extensions major
objectives before broad new game-specific work; make every generated project
loadable with meaningful Build/Run/Stop/Reopen/dependency behavior; expose the
EpochSimEngine demo and supplied EpochPlanet project through Extensions as
project add-ons, separately depending on their engine libraries. Develop the
planetary library as **EpochSpaceEngine**, not a built-in Engine plugin. Review
the supplied archive and licenses before extracting/integrating any contents.
FINAL same-day milestone, after that entire integration batch: finish Doxygen,
the full owner-only SDK, and authenticated online documentation/website delivery.
The operator explicitly targets the end of September 6, 2026 (America/New_York)
for this too. The deadline is a priority, not permission to claim untested
integrations, bypass access checks, or publish partial code.

### September 6 07:15 EDT reset checkpoint — resume here

The operator is refreshing now. Agents have stopped; no compiler, model or
native runtime was launched for this final checkpoint. Source still declares
v0.89.35; the requested NEXT feature release is now **v0.90.1**, superseding
v0.90.0. No public release or stable branch advanced.

Saved coherent source batch:

- Panel/orchestrator validation receipt summaries are bounded to 2048 bytes,
  retain the full-status SHA-256 and first causal compiler error, and preserve
  separate 32 KiB repair context. Rejected current receipts stop cleanly rather
  than hanging. Added verbose-failure, seven-gate verbose-success and malformed
  receipt non-mutation regressions. These new regressions are NOT yet built/run.
- `platform.work_admission` has a nonblocking 30-second controller, one-second
  cached real Windows/Linux RAM/CPU samples, strict stale/pressure checks and
  monotonic consumed/cancelled token retirement. Three new files plus CMake/
  MSVC wiring are saved. This component is NOT yet compiled/tested and is NOT
  connected to Editor dispatch. No production cooldown enforcement is claimed.
- Scheduler activity snapshot and Systems panel descriptions distinguish real
  active/queued/idle work from registered thread lifetimes. The focused CMake
  Release `epoch_editor_task_scheduler_contract` builds and passes (1/1, 0.04s).
  It was moved into the lightweight software-base CMake lane without duplication.
- `editor.application.cpp` has NO pending edits. Next hook work remains: replace
  toolbar Threads/CPU ratio with actual scheduler activity, and stop creating a
  scheduler merely to view Systems (pass an existing nullable graph instead).

EXACT NEXT ACTION: review/build the saved receipt regressions and admission
contract, then integrate admission in `editor.application.cpp`. Retain one exact
deferred RenderResult plus monotonic token/artifact epoch per context. Queue
model/compiler/test/preview operations centrally, including the final deferred
local HTTP submission. Use a synchronized process-wide heavyweight lease, not
unsynchronized reads of other contexts. Retain that lease through real worker/
child retirement and Keep/Choose. Poll without sleeps on each owning context
tick; show countdown/resource wait and Stop; include queued admission in
execution_pending. Cancel/Restart/Close must discard queued actions without
resurrection and retire active ownership before another operation starts.
macOS metrics and GPU/VRAM admission remain unsupported/unproven; do not claim
the RAM/CPU sampler prevents every possible resource overload.

Then rebuild Windows Debug/Release and run the build-safe aggregate contracts.
Do not launch another Qwen/native/GPU test through a workaround for the existing
tool denial. Resume the actual two-build/choice and isolation gates when allowed.
Epoch owns no WSL lane; the user explicitly authorized sharing that status and
it was delivered to EpochSimEngine. No new Linux work was started here.

The operator requested an immediate durable checkpoint before a usage refresh.
Finish integrating only the scoped work already underway, run the closest
build-safe regressions, then record the exact source status and next action.
Do not start a new native/model/release/SDK expansion merely to fill the pause.

- Last clean commit before this batch: `7d538759db65b854a19a95c6d9f8f0f927f89841`.
- In progress: bounded validation receipt summaries and real compiler-error
  repair handoff; measured CPU/RAM admission and nonblocking 30-second heavy-work
  cooldowns integrated with cancellation/choice; truthful task activity and no
  scheduler allocation merely to view Systems diagnostics.
- Native Qwen two-build/choice/succession and OS isolation remain unproven.
  Existing native tool denial and Winsock 10107 failure still apply. No new
  runtime/source/SDK Site publication is authorized from partial evidence.
- v0.90.1 is the requested next feature release; source/version metadata and
  accepted base refs must be reconciled before release, not guessed from UI.
  Version audit found these exact follow-ups (not yet changed):
  `epoch.version.ixx` and compatibility `engine.version.ixx`; runtime `%02d`
  and `get_epoch_version.ps1` `D2` formatting; CI's two-digit revision regex;
  receipt writer's assumption that all platform major/minor values equal
  source; updater discovery contract's hardcoded v0.89.35; stale Windows
  resource v0.89.32 and vcpkg project metadata v0.88.76. Explicitly pin macOS
  packaged major/minor/revision to 0/89/30 before bumping source, since it
  currently inherits the source minor. Preserve independent EpochGui v0.89.30
  and the dependency baseline. Canonical new release spelling is `0.90.1`.
- After the major self-coding/platform/context repairs pass their required
  acceptance, freeze the stable software/context base at that exact proven
  checkpoint BEFORE major game-engine-specific expansion. Retain the original
  `ad6c416d...` history. Do not freeze an unfinished checkpoint or make the
  reusable base follow every game-specific change automatically.
- Subsequent passes should take substantial, bounded implementation steps with
  explicit owners, tests and evidence. Reuse the stable base and proven systems;
  no padding, duplicate frameworks, speculative rewrites or unbounded churn.

- Source version: v0.89.35, local and unpublished. Starting commit for this
  candidate-data/model-usability pass:
  `3938faf381366f1e030999df86f8de2aa9ef8bd3`.
- Public Windows/Linux runtime and private-source discovery remain v0.89.34;
  public macOS packaged authority remains v0.89.30.
- GitHub push is unavailable while the repository account returns HTTP 403
  `account suspended`; the Epoch Site remains the protected publication path.
- Original `multicontext-base-stable`
  `ad6c416d930b348a61bc37ceb7d4522742be084a` remains preserved. Its restored
  branch label has not advanced; the proven-profile gate still owns that update.
- Completed implementation/checkpoint history belongs in
  `Changes/changelog.txt` and Git, not this live acceptance queue.

The current real-model rig requires an explicit, genuinely unfinished
`EPOCH_EDITOR_SELF_CODING_OBJECTIVE`. Its default comparison decision is
Choose; an explicit Keep remains testable. Success requires two actual accepted
Release compiles and separately identified candidate windows, retained chosen
sandbox lineage, a returned successor plan/proposal, and complete owned worker
and child retirement. A successor plan alone is no longer a successful test.
Source/build evidence for this rig does not replace running it with Qwen.

The restricted-child component has actual Windows console evidence: a fresh
zero-capability LPAC token is verified before resume; synthetic read-only code,
writable scratch, outside-file refusal, hardlink cleanup, prior-DACL restoration
and process/profile retirement pass. Null-DACL, hardlinked and reparse input
trees are refused before execution. The strict network subcheck does NOT pass:
Winsock startup returns 10107 before a socket is created, so connection denial
was not exercised. Its failed fixture and independent passing subchecks remain
in the executable's temporary `epoch-workspace-isolation-*` evidence directories.
Do not widen permissions merely to turn this result green.

This optional platform primitive is NOT wired into Candidate Lab compiler,
test or preview launches. Those launches still use the launching user's token.
Phase-specific grants, compiler dependencies, interactive HWND compatibility,
network/IPC qualification and abrupt-host-exit lease recovery remain open.
Completed source/build evidence and the earlier genuine Qwen transport result
belong in `Changes/changelog.txt`; none proves the full two-build workflow.

Exact checkpoint build identities and completed regression evidence belong in
`Changes/changelog.txt`. The strict network probe remains a failure,
independently of passing source/build/component checks.

The operator explicitly approved the visible Release/Qwen/build/comparison run
on September 5, but the execution tool rejected the subsequent ordinary visible
editor launch before startup. Approval is no longer missing; native execution
remains tool-blocked. Do not route around that denial with a different tool or
relabel component contracts as model/native-preview proof. The shared WSL lane
was released on September 5 after the other task's compiler processes retired;
the narrow HTTP component compile passed and the lane was released again.

## Remaining Work

September 6, current 24-hour priority (supersedes older dated task ordering):

1. Repair the actual Qwen run before broad new work. Campaign `fbd95e52...`
   staged its OpenGL patch and ran MSBuild; the first failure was C1075, an
   unmatched brace in `opengl.context_init.cpp`. Its 66,198-byte compiler status
   exceeded the validation-summary limits and stopped the repair handoff.
   Keep full logs, record bounded receipts, and prove the failure reaches the
   model repair loop. Do not promote that malformed patch into live source.
2. Add measured host-resource admission and a visible, cancellable, nonblocking
   30-second cooldown before model, compiler, validation and new candidate
   process work. Pause heavy work while Keep/Choose is pending; retire the loser
   before resuming. Report actual work separately from registered idle threads.
3. Build and run the closest regression contracts, then complete the exact
   native Qwen two-build/embedded-context/choice/successor and isolation gates
   below. Never substitute source tests for a blocked native or security check.
4. Prepare v0.90.1 as the new base. After every required gate passes, publish
   one exact committed tree to current runtime downloads, authenticated source,
   matching docs and the authorized stable branch. Preserve historical refs,
   archives and rollback bytes. Publication remains conditional on evidence.
5. Final same-day priority AFTER self-coding, EpochGui/Extensions, loadable
   projects and the EpochSimEngine/EpochSpaceEngine demo/library integrations:
   finish the owner-only Epoch Engine SDK and online API/manual documentation
   and website delivery by the end of September 6. First inspect existing
   Doxygen and Site per-project access
   controls. Generate version-bound C++23 module/header reference plus usable
   software-profile samples, build/debug guides and SDK manifests. Serve docs,
   assets, search indexes and SDK downloads only after server-side owner/admin
   authorization; hiding a link is not access control. Test anonymous and
   non-owner denial, direct asset URLs, session expiry and shared-cache leakage.

Use independent subagents continuously where useful, with disjoint source
ownership and serial heavy build/runtime work. At each checkpoint update this
gate and the owning subsystem docs with the next concrete action and actual
evidence. The SDK mission cannot replace a blocked self-coding acceptance run.

September 6 operator run: saved campaign `903a94c397919a67...` against
`8d0080fb` admitted Qwen's proposal and committed its sandbox postimage, then
stopped before the first Debug compiler. No validation artifact or candidate
preview was produced. The supplied `EpochEngine` launch path is a junction;
host workspace discovery retained that spelling while compiler admission
required the physical path. The current bounded repair canonicalizes only the
trusted host executable before sandbox creation and adds precise persisted
handoff/PID evidence. Candidate interior redirect checks remain strict.
The saved docking clamp was reviewed but not promoted: existing source already
guarantees its bound, and exhaustive production tab-move regression covers all
91,520 admitted same-group combinations. The operator requested a rebuilt
executable and will rerun with full logs; native comparison/succession is still
unproven. Do not rerun the model merely to recover the already-saved failure.

Pass ordering follows the September 5 one-day self-coding priority. First finish
the existing candidate-data repair and model-selection/working-state usability,
then exercise the actual model/build/comparison/succession gate below. The small
Output/AI Chat ratio repair supports that workflow; broad context/platform and
other missions stay in the roadmap, not in parallel as substitutes for P0.
Every pass begins from current source and this gate, diagnoses the actual failed
step, changes production behavior, tests that step, and records the evidence.
Only definitely completed requirements leave the mission queues; implementation
without its required real-model/native/safety evidence remains incomplete.

Model policy to finish and prove: current explicit selection wins, then the
last-used eligible same-endpoint model, then `nvidia/nemotron-3-nano-4b` for quick
assistance. Inventory absence/ejection does not erase memory. Actual user
Send/Start can reuse an eligible local selection; discovery alone never loads,
infers, or ejects. Qwen 3.5+ is the requested self-coding tier, with Qwen 3.8
preferred for long-horizon work. Show model purpose, selection source, endpoint,
working/waiting state and recovery actions in plain language; never silently
replace a coding worker with the small assistant.

Immediate continuation checkpoint: candidate-data routing, centered-divider
geometry and model preference/default Send/Start integration are implemented.
Production regressions cover remembered/disabled-profile selection, model
selection leases, hidden-pane progression, Stop/no resurrection, restart after
retirement, and unknown explicitly selected agentic models with real host
budgets. The September 5 combined Debug/Release builds and aggregates passed;
final exact build/transport results are recorded in the changelog. The operator
requested a fresh build after this AI pass, superseding the previous executable
hold. Native eye acceptance and the real two-compile Qwen comparison/succession
gate remain outstanding; do not restart these foundations or claim full success.

Real HTTP evidence now includes a passing warm Qwen cancellation/recovery probe
and an independent health response, alongside the retained initial cold-load
two-deadline failure. Model-server response latency remains material: the warm
canary took about 85 seconds and the independent short reply 53 seconds. No
server configuration or model ejection was changed. Next acceptance is the
exact Release eye test and complete native candidate/successor run when native
execution is available, not another reimplementation of these model controls.

1. Exercise one complete local-model session from an ordinary-language request
   through automatic source selection, plan, exact patch, sandbox apply, build,
   validation, candidate PID/context admission, and Keep/Choose. Record the
   actual request, selected paths, retries, build receipts, process retirement,
   and sandbox lineage without recording hidden reasoning or unrelated source.
   Continue through a second actual sandbox compile after Choose: a successor
   plan alone does not prove retained compiler dependencies or repeatability.
2. Eye-test the exact Release build for the working indicator, elapsed time,
   cancellation, readable actions at narrow/high-zoom layouts, Candidate Lab
   comparison, and unambiguous Keep Current / Choose Candidate behavior.
   Include context/session Close while model or compiler work is pending.
   Per-context model requests now carry an owned stop token through queue,
   retries and transport, with worker-owned asynchronous Windows HTTP closure.
   Build-safe ownership checks and a separate opt-in HTTP probe must be recorded
   independently of native context-close/preview eye evidence.
3. Repair any failure found by that end-to-end run. Transport, API, schema,
   packet, build, and validation failures may retry within their existing
   bounded budgets; cancellation and unsafe/off-catalog path requests fail
   immediately.
4. Finish and adversarially test the candidate execution boundary. The
   current Candidate Lab launch retains the user's OS identity while replacing
   ambient environment variables and disconnecting host stdin;
   Job Object ownership guarantees lifecycle supervision, not filesystem or
   network confinement. Host transaction containment is not proof that arbitrary
   compiled candidate code cannot access live source or other projects.
   Separate writable compiler scratch/source from immutable validated preview
   code and generation-local runtime state; keep host receipts outside child
   grants. Qualify actual Windows token/network policy and compiler/embedded-HWND
   compatibility before admitting the optional restricted-child primitive to
   Candidate Lab. Preserve the current Winsock 10107 compatibility failure as
   uncompleted network evidence. Qualify crash recovery for persisted identity
   grants; normal retirement alone does not prove abrupt-parent cleanup. Linux
   descriptor cleanup is implemented, but filesystem/network/IPC confinement is not.
   The existing root-level solution/build layout, host logs and
   `.epoch/local_mcp` receipts do not yet fit disjoint immutable/writable grants.
   The current source routes candidate test/preview mutable state through a
   fresh sibling data root; qualify all consumers and integration rather than
   rebuilding that router. Compiler layout, host log/receipt placement and
   enforced permissions remain separate unfinished work. Qualify an owned immutable
   MSBuild/VC/SDK/vcpkg dependency closure before enabling restricted launches.
5. Only after the exact candidate passes those gates, stage immutable Windows
   and Linux packages, exact-file validation receipts and sidecars, then hand the
   single reviewed publication set to the Epoch Site task. Do not alter the
   current v0.89.34 public authority until both platforms and rollback evidence
   are accepted.

## Self-Coding Contract

- `Start With AI` sends the plain objective plus a path-only catalog. The model
  may choose or expand a coherent slice of at most 12 canonical C++ paths.
- The host rejects traversal, absolute, duplicate, missing, non-source, and
  off-catalog paths before reading or sending bytes.
- Only the selected exact UTF-8 source evidence enters the request. The proposal
  stage receives those bytes, not merely filenames or hashes.
- Whole-file hashing/local reads are capped at 8 MiB per selected file. Model
  evidence remains capped at 184 KiB in aggregate, with fair per-file budgets
  and UTF-8-safe excerpts of at most 16 KiB for larger sources. Whole-file
  identity size is not charged as outbound excerpt size.
- OpenAI-compatible source requests use strict JSON schemas for path selection
  and edit/context actions. A context action cannot contain edits; its complete
  next source selection still passes the normal catalog gate. Epoch converts the validated JSON into its existing exact
  review packet before normal grounding and apply checks.
- Exact search blocks must match the reviewed source uniquely. No-op,
  destructive, invented, placeholder, ownership-removing, or unrelated edits
  are rejected before sandbox staging.
- Source workloads expose queued, working/waiting, and stopping states, elapsed
  time, and a source-scoped Cancel action. They do not claim token progress from
  a non-streaming HTTP request. Native visibility remains an eye-test gate.
- Transport/API/empty-content failures retry once. Invalid source selection and
  proposal packets receive at most two host-diagnosed corrections. Insufficient
  evidence may revise the selected source slice within the 12-path ceiling and
  shared three-attempt navigation budget. Provider reasoning defaults are not
  overridden to none/off.
- A selected candidate advances only the disposable sandbox lineage. Live
  source promotion remains a separate explicit authority and is not part of the
  Candidate Lab loop.

## Acceptance

- One real local-model workflow completes without an operator-named source and
  without a blank, ambiguous, or falsely gated action.
- Every model request has visible active/elapsed/cancel state and reaches a
  terminal success, failure, or cancellation state.
- The model cannot read outside the reviewed catalog/slice, overwrite live
  source, modify the active project, cross into another candidate session, or
  claim host build/test evidence.
- Candidate and baseline run under distinct supervised identities; the losing
  child is retired and no orphan EpochEditor/model worker remains.
- Keep/Choose persists one unambiguous next sandbox parent and resumes the saved
  mission at its next unfinished step.
- Final source, docs, version authorities, build metadata, receipts, archives,
  checksums, and Site presentation describe the same exact committed tree.

## Protected Boundaries

- Preserve backend frame order, queue drain, GUI replay, subpass, depth, and
  presentation behavior unless a separately proved renderer mission requires a
  change.
- Preserve public v0.89.34 runtime/source objects and all immutable rollback
  releases. Do not rewrite historical tags, packages, manifests or sidecars.
  Advance `multicontext-base-stable` only under the operator's explicit update
  request and the proven-profile gate, retaining its original checkpoint.
- Do not stage generated builds, caches, logs, captures, local projects,
  temporary model exchange data, or unrelated operator files.
- Do not publish a release or Site update from partial source/build evidence.

The compact product schedule is `Changes/roadmap.md`. Durable unresolved intent
is `Changes/mission_cache.md`. Completed checkpoint history belongs in
`Changes/changelog.txt` and Git history, not in these live mission queues.
