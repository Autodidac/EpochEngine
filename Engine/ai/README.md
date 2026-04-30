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

Current engine AI runtime roles:

1. internal EpochBot
2. local MCP/control bots that can operate and train EpochBot

External local LLMs such as LM Studio are development helpers for testing,
evaluation, curation, and iteration speed. They are not a third in-engine
runtime role.

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
editor state, records the result as MCP evidence, and stages a packet when the
tool action succeeds. This is the first bridge from "AI can talk about tooling"
to "AI can learn from an editor action that actually changed state."

EpochBot must not answer that self-iteration, training, or tooling is "working
fine" unless it can cite concrete evidence: a staged packet, build log, runtime
capture, MCP capture, scene state change, eval output, or retained operator note.
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
   `build/windows-msvc-debug/Engine/Debug/epoch.exe`
4. Open the editor workspace panel and select `AI`.
5. Use the AI sub-workspaces:
   - `Sandbox`: separate self-iteration control room for the watcher and staged packets
   - `Harness`: run selected scripts through the editor tool harness
   - `Assistant`: normal project/scene guidance, MCP capture, and packet staging
   - `Launcher`: generated project/build/source evidence
   - `Training`: raw capture, curated dataset, and eval promotion controls
   - `Ops / How-To`: quick operating instructions

Suggested first run:

1. In `Project`, create or select a generated project shell.
2. In `Scripts`, select `Rotate All Entities` or another tooling script.
3. In `AI -> Sandbox`, click `Enable Self-Iteration Watcher` or `Queue Self-Iteration Build`.
4. In `AI -> Harness`, click `Run AI Tool Harness`.
5. Inspect the `Output` workspace for build/tool logs.
6. Review staged packets under
   `Engine/examples/ConsoleApplication1/workspace/research/staged/iteration_packets/`.
7. Use `Stage Sandbox Scene Training Task` when EpochBot needs a watchable
   3D edit/test exercise before training or evaluation.
8. Promote only reviewed, evidence-backed captures from `AI -> Training`.

The AI sandbox should be boringly explicit: it can watch, build, run tooling,
capture evidence, and stage packets, but curated training and eval promotion
remain review-gated actions.
