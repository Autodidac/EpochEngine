# Epoch AI Workspace

Epoch uses operator-selected models to assist with projects and improve Engine
source in disposable Candidate Lab generations. It does not train models or
silently replace live source. Local v0.89.35 remains unpublished; the active
acceptance gate is [Changes/active_pass.md](../../Changes/active_pass.md).

## Model Selection And Requests

The production API supports an operator-managed OpenAI-compatible endpoint and
an explicitly selected `llama-cli` executable/GGUF pair. A verified Epoch-local
Qwen3.8 package is optional; installation never starts a server or inference.
Generated-project AI profiles default to Off and never bundle model weights.

For the local API, a current explicit/configured selection wins. Otherwise Epoch
restores an eligible endpoint-bound preference, or selects the operator-confirmed
small default `nvidia/nemotron-3-nano-4b`. A missing inventory entry does not erase
the selection or substitute another model. Inventory presence is not proof that
a model is loaded or responsive.

Restoration and scanning do not load, infer, or eject. An actual user Send/Start
can prepare an eligible local selection without redundant endpoint confirmation; the
request then follows the normal transport and failure path. Remembered local
consent does not transfer to a different or remote endpoint. Legacy model-only
preferences can be upgraded on a user request at the original
`http://localhost:1234` endpoint. Explicitly disabled project profiles remain Off.

Nemotron 4B is the requested quick-assistant role. Qwen 3.5+ is the requested
self-coding tier, with Qwen 3.8 preferred for long-horizon work. Role guidance is
not a task-success guarantee or a model-name allowlist. An unknown explicitly
selected agentic model may attempt the same bounded workflow: actual source,
packet, host-budget and build validation remain mandatory. The automatic or
remembered Nemotron helper requires an explicit coding choice before a source
request; that request retains its objective while the user selects a model.
Choose / Change Coding Model is available directly in Engine Self-Coding.
Queued requests retain selection leases through worker retirement and source
reply handoff, preventing cross-context model switches during active work.
Native role/selection UI and restart acceptance still require an eye test.

## Operator-Started Candidate Lab

The automatic sandbox workflow is implemented in production, not just described
by `ai.iteration_loop`. It does not require an operator-named subsystem, path,
symbol, diagnostic code, or packet header:

1. Start with an ordinary-language objective. The model receives a path-only
   catalog and selects a coherent working set of at most 12 canonical C++ paths.
2. The host validates and displays that selection before reading its source.
   Proposal requests include the actual bounded UTF-8 evidence. Local identity
   hashing and the full buildable workspace are not extra outbound context.
3. The model returns an exact proposal or requests a bounded context revision.
   Host-owned preimages, digests, generation checks and transactions admit
   changes only to the disposable sandbox. Invalid packets and verified failures
   have bounded correction/repair budgets; cancellation does not retry.
4. Within the operator-started lab, plan/proposal, apply, Debug/Release compiler
   and contract checks, HeadlessCI and full validation advance automatically.
   The separate manual one-change workflow retains its explicit review actions.
   Neither mode accepts model-authored build or test success as evidence.
5. A validated candidate launches as a separately supervised editor PID for
   bottom-grid comparison. Keep Current retires the challenger. Choose Candidate
   retains it as the next sandbox parent and continues the saved mission. Stop
   Lab retires its owned work. Selection never promotes bytes into live source.

AI Controls and Detailed Session Activity own the self-coding objective,
progress, review and host evidence. Project Assistant conversation and project
goals are a separate workflow; an AI Chat reply is not source-edit authority.
Selectable source inspection does not imply writable validated preview code.

Reply consumption and automatic initial/successor planning run in the owning
context tick, even when AI Controls is hidden or another inspector tab is active.
Stop latches automatic progression off and requests cancellation of owned model,
workspace, compiler, test and preview work. Restart waits for retirement, then
retains the objective/chosen sandbox parent and mission checkpoints while
resetting stale request/scope identity. Renderer-free production contracts cover
these transitions; they are not native two-generation runtime evidence.

The source contains exact-artifact binding, request cancellation, bounded repair,
chosen-parent continuity and owned child-retirement checks. These implementations
do **not** establish full end-to-end acceptance. The real-model rig must complete
two actual accepted compiles and candidate admissions, including the successor
after Choose; a plan, a launched worker or a synthetic receipt cannot pass it.

## Remaining Acceptance And Boundaries

- Complete the actual local-model/build/validation/comparison/succession run,
  including losing-process retirement and a genuinely unfinished objective.
- Eye-test working/elapsed/cancel state, readable actions, Keep/Choose, and
  context/session Close while model or compiler work is pending. Existing
  component and HTTP evidence is not native editor proof.
- Finish real OS confinement for Candidate Lab. The current launches retain
  the user's token. Environment replacement, private runtime-data routing,
  transaction guards and Job Objects are not filesystem/network isolation.
  The optional Windows restricted-child primitive is not connected to the lab;
  network/IPC, compiler dependencies, embedded HWND compatibility and abandoned
  permission-lease recovery remain open.
- Keep host receipts, permits, mission lineage, other projects and live Engine
  source outside model authority. Source promotion is a separate host-verified,
  explicit operator transaction, never an automatic consequence of Choose.
- Static-analysis, sanitizer, architecture, visual and frontier-review adapters
  beyond the connected validation actors remain unimplemented. No hidden
  listener, model training, autonomous live-source replacement, Git or release
  authority is implied by the sandbox loop.

## Project Assistance And Tooling

The existing authoring and project-tool paths use allowlisted semantic commands,
visible proposal decisions, exact active-project identity and host-owned
execution. Project Save/Build/Run, script actions and diagnostics retain their
own lifecycle and approval rules; they do not inherit Engine-development powers.
MCP protocol adapters and the explicit external-agent bridge remain separate
from inference. A registered protocol is not a running server or listener.

## Source And Runtime Layout

- `prompts/` contains tracked system prompts.
- `evals/` and `evals/fixtures/` contain reviewed, non-training cases.
- `manifests/` declares providers and tool protocols.
- `control/continuous_build_loop.json` is the actual HeadlessCI control contract;
  its legacy filename does not enable a hidden background loop.
- Ordinary model preferences/assets use executable-local `cache/models/`;
  disposable AI state uses `cache/ai/`. Explicitly retained exchanges and tool
  traces use the configured workspace. These are not release payloads.
- A bound candidate data root redirects its model preferences/cache, workspace,
  configuration, logs and private Projects independently of validated code.
  It does not import the parent editor's conversation, credentials or model
  weights. Path routing remains distinct from the unfinished OS execution boundary.

Normal chat and build results are not training data. Explicit evidence capture
and reviewed eval fixtures own any retained records. See the
[OS AI policy](../docs/engine/os_ai_tooling_and_evidence_policy.md) for the detailed
contracts, [roadmap](../../Changes/roadmap.md) for scheduling and
[mission cache](../../Changes/mission_cache.md) for unresolved operator intent.
