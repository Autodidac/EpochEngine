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

## Self-Iteration Sandbox Loop

The current Phase 3/4 foundation is an evidence-gated self-iteration sandbox,
not blind self-modifying autonomy and not the normal game/software editor:

1. planner produces or updates an explicit staged packet
2. executor proposes work only from the staged packet and current operator goal
3. builder creates fresh build evidence
4. verifier checks build/runtime/capture/eval output
5. gate promotes curated training/eval records or discards the attempt

The live contract is stored in:

- `Engine/ai/control/continuous_build_loop.json`

The editor AI workspace now has a continuous build lane that watches active
project/script evidence, runs one child-project build at a time, and stages a
fresh packet after successful builds. That packet is the durable handoff into
future replay, verifier scoring, and curated training promotion. It does not
grant blind write-through to the repo.

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
launching the editor. Tool evidence capture files, including the legacy
`mcp_capture.jsonl` path, are evidence logs, not a hidden second model runtime.

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
The editor now exposes a `Stage Sandbox Scene Training Task` action so the bot can
be given watchable 3D scene-edit/test exercises without confusing that sandbox
with ProjectLauncher game/software work.

## Running The AI Sandbox Controls

From a developer checkout:

1. Configure/build the editor:
   `cmake --preset windows-msvc-debug`
2. Build after code changes:
   `cmake --build --preset windows-msvc-debug`
3. Launch the editor executable:
   For the Visual Studio solution build: `x64/Debug/ConsoleApplication1.exe`.
   For the CMake preset build: `build/windows-msvc-debug/Engine/Debug/epoch.exe`.
4. Open the editor workspace panel and select `AI`.
5. Use the AI sub-workspaces:
   - `Sandbox`: separate self-iteration control room for the watcher and staged packets
   - `Harness`: run selected scripts through the editor tool harness
   - `Assistant`: normal project/scene guidance, selected-model chat, and packet staging
   - `Launcher`: generated project/build/source evidence
   - `Training`: raw capture, curated dataset, and eval promotion controls
   - `Ops / How-To`: quick operating instructions

Suggested first run:

1. In `Project`, create or select a generated project shell.
2. In `Scripts`, select `Rotate All Entities` or another tooling script.
3. In `AI -> Sandbox`, click `Enable Self-Iteration Watcher` or `Queue Sandbox Build Pass`.
4. In `AI -> Harness`, click `Run AI Tool Harness`.
5. Inspect the `Output` workspace for build/tool logs.
6. Review staged packets under
   `Engine/examples/ConsoleApplication1/workspace/research/staged/iteration_packets/`.
7. Use `Stage Sandbox Scene Training Task` when EpochBot needs a watchable
   3D edit/test exercise before training or evaluation.
8. Promote only reviewed, evidence-backed captures from `AI -> Training`.

The AI sandbox should be boringly explicit: it can watch, build, run approved
local games/tools, capture evidence, and stage packets, but curated training,
eval promotion, and bypass-capable app/server runtime activation remain
review-gated actions.
