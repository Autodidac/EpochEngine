# Epoch AI Content

This folder stores repo-safe AI assets for the engine.

Tracked here:

- curated JSONL datasets
- dataset schemas
- eval cases
- control-loop contracts
- tokenizer/manifests
- prompt templates

Do not commit normal Git history with:

- raw checkpoints
- quantized weights
- cache files
- compiled local model outputs that should stay local or ship through releases

Compiled local-only outputs live under:

- `Engine/examples/ConsoleApplication1/workspace/ai/checkpoints/`
- `Engine/examples/ConsoleApplication1/workspace/ai/models/`
- `Engine/examples/ConsoleApplication1/workspace/ai/cache/`

Raw/staging capture can live under:

- `Engine/examples/ConsoleApplication1/workspace/auto_train.jsonl`
- `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl`

Those capture files are Git-safe JSON/JSONL, but they are still staging data.
Review them, promote only the intentional pieces into curated repo datasets or
evals, and delete outdated/bad artifacts when the training direction changes.

Current engine AI architecture:

1. EpochBot, the primary engine-owned trainable LLM/runtime path
2. local tool/MCP control harnesses that operate the editor and collect proof
3. an offline/injectable OSS or tiny backup LLM path for fallback, generated
   software embedding, and EpochBot training support

External local OpenAI-compatible LLMs such as LM Studio or Ollama are
development helpers for testing, evaluation, curation, and iteration speed while
EpochBot grows into an engine-owned LLM trained on Epoch evidence and
editor/tool behavior. They are selected teacher/reviewer providers, not hidden
authority and not substitutes for the internal backup LLM path.

EpochBot's long-term target is not just a chat assistant. It is a small
engine-owned LLM/runtime that learns the engine, editor, sandbox scenes, project
output, and tool schemas over time, with a backup tiny internal LLM available
for offline/fallback behavior. Until that model is genuinely capable, selected
local LLMs act as teachers/reviewers over evidence, not as hidden authority.

## AI Technique Policy

Epoch's AI loop should follow current practical agent guidance rather than
older "let it train itself" folklore:

- Treat AI code, datasets, evals, tools, and UI surfaces as production engine
  work. Experimental learning loops must still be bounded, reviewable,
  evidence-captured, and reversible; no fake autonomy or unverifiable training
  claim should be presented as progress.
- Keep agent workflows simple, composable, and inspectable. The live loop is a
  staged workflow, not a swarm of hidden autonomous agents.
- Treat model/tool integration as a controlled tool-calling loop: the model can
  request or propose work, Epoch executes the editor/build/tool action, and the
  resulting output is fed back as evidence.
- Treat games, tools, apps, and servers as valid project outputs, but keep
  bypass-capable runtime activation human-gated. EpochBot may generate or
  modify code for review; it must not create or run an app/service that gives
  the model a bypass channel, self-accessible server, hidden control surface,
  listener, port bind, or network-serving behavior automatically.
- Allow local game and tool tests through approved editor/tool-harness controls
  when the action is visible, evidence-captured, and does not expose a new
  model-accessible network/control surface.
- Keep tracing/capture artifacts for model replies, tool actions, build logs,
  scene changes, and staged packets so failures are debuggable.
- Build evals early and use them before prompt, model, dataset, or tool-harness
  changes are treated as improvements.
- Do not train recursively on unverified generated output. Model-written
  suggestions are raw material only; they need real tool/build/scene evidence,
  labels, and review before curated promotion.
- Keep training and eval material separate. Raw captures stay in the workspace,
  curated datasets live under `Engine/ai/datasets/curated/`, and eval cases live
  under `Engine/ai/evals/`.

## Closed-Loop Agentic Cognition Target

EpochBot is not meant to become useful by adding more prompt text to a stateless
chat path. The target is an always-running, evidence-gated cognition loop around
the selected model and the future engine-owned LLM runtime:

1. base model for language/reasoning
2. working memory for current goal, task stack, observations, files, tools,
   assumptions, errors, and recent actions
3. persistent semantic, episodic, and procedural memory
4. retrieval/ranking that injects only relevant memory into context
5. explicit goal stack with constraints, success conditions, and failure states
6. planner that continuously replans from new evidence
7. executor that performs visible tool/editor/build actions
8. verifier that checks compilers, tests, screenshots, logs, diffs, and evals
9. scoring that rewards verified progress and penalizes constraint violations
10. self-state tracker for known unknowns, confidence, tools, failures, and mode
11. attention controller for interrupts, priorities, and tool requirements
12. real-time loop: observe, update memory, retrieve, evaluate goals, plan, act,
    verify, commit memory, and replan

Long-term memory alone is not enough. The practical intelligence jump comes from
closed-loop agency against reality: goal, action, evidence, correction, next
action. EpochBot must not promote memories, datasets, code changes, or
self-status claims without visible evidence from that loop.

## Engine Self-Iteration Loop

The current Phase 3/4 foundation is an evidence-gated engine self-iteration
lane, not blind self-modifying autonomy and not the normal game/software editor:

1. planner produces or updates an explicit staged packet
2. executor proposes work only from the staged packet and current operator goal
3. builder creates fresh build evidence
4. verifier checks build/runtime/capture/eval output
5. gate promotes curated training/eval records or discards the attempt

The live contract is stored in:

- `Engine/ai/control/continuous_build_loop.json`

The editor AI workspace now has a continuous build lane that watches active
project/script evidence, runs one child-project build at a time, and stages a
fresh packet after successful builds. The current compatibility profile id is
`sandbox`, but generated child artifacts present as `EpochEngine` because this is
the engine-upgrade lane rather than a separate game project. That packet is the
durable handoff into future replay, verifier scoring, and curated training
promotion. It does not grant blind write-through to the repo.

The engine also exposes `--editor-ai-gate-self-test` as a deterministic guard
for helper-model review quality. It rejects status-only replies, missing
verifier evidence, bypass/server requests, and any self-iteration reply that
tries to promote without human approval. Keep this check green before allowing
local helpers such as Nemotron to supervise EpochBot packets.

## Measured Training And Eval Scorecard

EpochBot should be moved toward frontier-style coding behavior by measured
supervision, not by trusting a low-capacity helper that says it is fine. As of
May 21, 2026, the production path is:

1. collect verified editor/build/tool traces
2. train or adapt candidate 4B/20B models on curated traces only
3. evaluate with project-specific tasks, external coding/reasoning harnesses,
   and visual/runtime checks
4. promote only when scores improve without new safety or workflow regressions

Track these metrics before any helper output becomes curated data:

- `gate_accuracy`: accepted/rejected helper-review decisions match labels
- `false_accept_rate`: unsafe or low-evidence replies accepted by mistake
- `false_reject_rate`: valid evidence-backed replies rejected by mistake
- `evidence_coverage`: packet/build/output/verifier/capture paths all present
- `tool_trace_coverage`: action/result/error/file-diff records captured
- `build_pass_rate`: generated project or engine build passed
- `runtime_smoke_pass_rate`: child self-test or smoke proof passed
- `self_iteration_completion_rate`: watcher/build/gate reaches a final state
- `training_promotion_rate`: only reviewed traces enter curated datasets
- `server_bypass_block_rate`: model-accessible server/listener requests blocked
- `regression_rate`: accepted changes later break build, GUI, or runtime proof

The repo-safe scorecard seed lives at:

- `Engine/ai/evals/epochbot_scorecard.json`

Useful current references for the summer 2026 target are OpenAI eval/agent-eval
guidance, Hugging Face TRL for supervised/preference/RL-style post-training,
EleutherAI `lm-evaluation-harness` for model eval plumbing, and SWE-bench style
coding benchmarks. These are guidance inputs, not authority over Epoch's local
evidence gate.

For the current self-iteration lane, "finished" means all of the following are
true in the same packet: project manifest exists, build log exists, output
artifact exists, generated child self-test/verifier passed, UI gate state is
visible, no required evidence path is marked `[missing]`, and promotion still
waits for human approval.

## Next AI Implementation Pass

After the v0.84.35 DirectX/multicontext checkpoint, the next focused AI pass is
to turn the existing sandbox controls into the first real EpochBot control loop.
That pass should not spend time inventing another prompt surface. It should wire
the current evidence paths into a visible, reviewable loop:

1. `WorkingMemory`: active goal, current project, staged files, recent events,
   selected model, open editor surface, last tool action, known blockers, and
   active hard rules.
2. `LongTermMemory`: curated facts, episodic build/run history, procedural
   tool recipes, and exact file/project indexes. Vector recall is optional
   support, not the only memory.
3. `Retriever`: ranks memory by recency, relevance, authority, and current goal,
   then injects only the useful slice into a model/tool request.
4. `Planner`: proposes one bounded pass from the goal and evidence, with clear
   success and failure conditions.
5. `Executor`: runs only approved editor/build/tool actions through the visible
   harness and records stdout, stderr, file diffs, screenshots, scene state, and
   exit codes.
6. `Verifier`: checks compiler/test/runtime/screenshot/log evidence and rejects
   vague self-status claims.
7. `Scorer`: rewards verified progress and penalizes constraint violations,
   unrelated churn, missing evidence, and repeated failures.
8. `Gate`: requires human approval before source promotion, dataset promotion,
   server/listener activation, or any bypass-capable runtime.

The first usable milestone is not autonomous repo mutation. It is an
operator-visible loop where EpochBot can:

- inspect the active project and scene state
- propose one small improvement
- run a sandbox build/test or scene-training task
- show exactly what changed and where
- ask for approval before promotion
- append notes, packet evidence, and curated training candidates

Minimal next-pass prompt:

```text
Read AGENTS.md, Changes/roadmap.md, Engine/ai/README.md, and
Engine/ai/control/continuous_build_loop.json. Preserve the v0.84.35
multicontext checkpoint. Implement the smallest working EpochBot closed-loop
control slice: working memory, staged goal packet, visible executor action,
verifier evidence, score/gate result, and updated notes in the AI Sandbox. Do
not add hidden autonomy, auto servers, bypass channels, or unreviewed dataset
promotion. Build, run the editor from x64/Debug, capture proof, update docs, and
commit only after the GUI and AI evidence are verified.
```

The same evidence route is available without opening the GUI:

```powershell
.\x64\Debug\ConsoleApplication1.exe --editor-project-self-test sandbox
.\x64\Debug\ConsoleApplication1.exe --editor-project-self-test projectlauncher
```

Those commands materialize and build the selected shell, append an MCP-style
tool capture to `Engine/examples/ConsoleApplication1/workspace/mcp_capture.jsonl`,
and stage a packet under
`Engine/examples/ConsoleApplication1/workspace/ai/iterations/`. The generated
child outputs should then pass their own `--project-self-test` routes before an
AI pass is treated as verified.

The editor AI workspace also has an AI tool harness. It builds and runs the
selected tooling script through the real `EpochScriptHost`, captures before/after
editor state, records the result as tool evidence, and stages a packet when the
tool action succeeds. This is the first bridge from "AI can talk about tooling"
to "AI can learn from an editor action that actually changed state."

The selected local model can also review the latest project output evidence.
Use `Ask Selected Model For Plan` after a project/script build exists; the prompt
includes the project root, manifest, build log, output executable, tool-state
summary, and packet root, then asks for one builder/verifier pass that advances
EpochBot's engine-owned LLM/tool-training pipeline. That reply is still a
proposal until the operator approves a follow-up pass.

The local chat path is explicit: the editor scans `/v1/models` and sends chat
requests to `/v1/chat/completions` on the configured local endpoint. The default
endpoint is `http://localhost:1234`; override it with `EPOCH_AI_ENDPOINT`,
`EPOCH_OPENAI_BASE_URL`, `LM_STUDIO_BASE_URL`, or `OPENAI_BASE_URL` before
launching the editor. CLI/self-iteration runs may also set the explicit model
with `EPOCH_AI_MODEL`, `EPOCH_OPENAI_MODEL`, `LM_STUDIO_MODEL`, or
`OPENAI_MODEL`; this is an operator-selected override, not a first-detected
model fallback. Tool evidence capture files, including the legacy
`mcp_capture.jsonl` path, are evidence logs, not a hidden second model runtime.
If a selected model returns blank visible assistant content and only
`reasoning_content`, EpochBot rejects that reply as a model/API configuration
failure. Hidden reasoning is never displayed as chat and is never promoted as
curated assistant training data.

## Server And Addon Safety

Epoch can create games, tools, apps, and server projects, but creation is not
permission to run anything that could bypass the gate. Any generated server
code, multiplayer service, local web service, control API, model-accessible app,
or network listener must remain inert until the operator explicitly enables and
launches it from a visible editor or shell action. The AI loop can ask for that
approval and explain why it is needed; it cannot grant approval to itself.

Local game runs, local tool tests, script harness passes, and non-networked
runtime checks are allowed through approved editor/tool-harness controls because
they are the evidence path Epoch needs. The boundary is not "never run local things"; it
is "never let the model create or activate a new service/control surface it can
use to bypass human review."

The repo-local `addons/` folder is intentionally ignored. It may contain starter
projects, experiments, and future Epoch candidates, but those projects must be
reviewed and intentionally promoted before anything from that folder becomes
tracked source.

EpochBot must not answer that self-iteration, training, or tooling is "working
fine" unless it can cite concrete evidence: a staged packet, build log, runtime
capture, tool evidence capture, scene state change, eval output, or retained operator note.
Helper-model replies that do not cite packet/build/output/verifier evidence are
proposal noise and must be rejected by review/eval gates instead of promoted
into training data or source changes.
The editor now exposes a `Stage Scene Training Task`/engine-scene packet action
so the bot can be given watchable 3D scene-edit/test exercises without confusing
the engine self-iteration lane with ProjectLauncher game/software work.

## Running The Engine AI Controls

From a developer checkout:

1. Configure/build the editor:
   `cmake --preset windows-msvc-debug`
2. Build after code changes:
   `cmake --build --preset windows-msvc-debug`
3. Launch the editor executable:
   For the Visual Studio solution build: `x64/Debug/ConsoleApplication1.exe`.
   For the CMake preset build: `build/windows-msvc-debug/Engine/Debug/epoch.exe`.
4. Open the central `AI Sandbox`/Engine AI surface for EpochBot chat, model
   selection, evidence status, and self-iteration controls. The Inspector may
   mirror the current command set. Bottom `Console Dock -> AI` remains compact
   evidence/status output only; World Outliner should stay focused on scene
   hierarchy plus compact bot status, not duplicate the full AI workspace.
5. Use `Scripts` to create/select/build/run project-local script stubs and the
   shallow project file browser. Use `Assets` to inspect first-pass file-type
   cards for active scene/model/image/audio/text assets.
6. Use the Inspector for the actual AI command buttons:
   - `Engine Self-Iteration`: watcher, manual build queue, and scene-training packets
   - `Harness`: run selected scripts through the editor tool harness
   - `Assistant`: selected-model planning and evidence packet staging
   - `Launcher`: generated project/build/source evidence repair
   - `Training`: raw capture, curated dataset, and eval promotion controls
   - `Ops / How-To`: operating instructions and readiness state
7. If the Inspector or AI status body is taller than the window, use the
   in-panel scrollbar. AI controls should remain reachable without stretching
   the app across multiple monitors.

Suggested first run:

1. In `Project`, select the built-in engine self-iteration lane. It still uses
   the `sandbox` compatibility id/root, but generated artifacts and verifier
   output present as `EpochEngine`.
2. In `Scripts`, create a project script stub or select an existing script, then
   use `Build Selected Script` and `Run Selected Script` so the project notes and
   output log show visible evidence.
3. In `Assets`, confirm the scene/model/asset cards for the active project and
   select any asset path that should be part of the iteration evidence.
4. In the central Engine AI surface or World Outliner `EpochBot` tab, inspect
   the current loop gate. Use the Inspector to click
   `Arm Evidence Watcher` or `Queue Engine Build Pass`.
5. In `Bottom Dock -> AI`, select `Harness`, then use the Inspector to click
   `Run AI Tool Harness`.
6. Inspect the `Output` dock tab and `Project -> Show Project Notes` for
   build/tool logs, selected file paths, and human-readable change notes.
7. Review staged packets under
   `Engine/examples/ConsoleApplication1/workspace/research/staged/iteration_packets/`.
8. Use `Stage Scene Training Task` when EpochBot needs a watchable
   3D edit/test exercise before training or evaluation.
9. Promote only reviewed, evidence-backed captures from `AI -> Training`.

The AI sandbox should be boringly explicit: it can watch, build, run approved
local games/tools, capture evidence, and stage packets, but curated training,
eval promotion, and bypass-capable app/server runtime activation remain
review-gated actions.

The evidence watcher is intentionally manual-gated. It may observe and refresh
evidence state, but it must not start background build loops or self-promote
source changes after a file change. Kernel/driver-level instability, repeated
build loops, or missing output evidence should leave the gate blocked until the
operator explicitly queues one build/tool pass and reviews the result.
