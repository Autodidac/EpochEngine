# AI Source Discovery Architecture

Epoch Engine Self-Coding uses a host-owned retrieval layer. The model does not receive the whole repository and does not gain arbitrary filesystem authority.

## Flow

1. Build a read-only repository index for the selected source area.
2. Rank a compact repository map and seed likely source using the operator objective.
3. Add exact reviewed source to a cumulative per-iteration workspace.
4. When more evidence is needed, perform bounded needle searches over path, identifier, text, import/reference relationships or a requested line/literal window.
5. Append newly verified evidence to the workspace; do not replace earlier reviewed evidence.
6. Pack only the currently useful exact evidence into the model prompt within the active budget.
7. Permit edits only against REVIEWED_SOURCE_ID values backed by exact bytes supplied in that patch request.
8. Stage edits in the disposable sandbox and retain the existing build/test/review/promotion gates.

## Budgets

The workspace is bounded by bytes/tokens, not a small operational file count. Individual navigation requests stay small so one model call cannot explode the working set. When LM Studio reports the loaded context length, Epoch may use the additional capacity as discovery grows; it does not fill the context window by default.

## Stagnation

Repeated discovery that adds no verified evidence is deduplicated. Two stagnant rounds stop the source-discovery loop and return a useful failure/question instead of spending a fixed sequence of replacement expansions.

## Authority

Repository-map entries, search hits and compact navigation notes are discovery aids only. They cannot authorize writes. Exact reviewed source bytes, verified checkout identity, SOURCE_ID/REVIEWED_SOURCE_ID admission, sandbox-only mutation and operator/build gates remain authoritative.

The repository index reads the selected source area locally. Its compact map
contains bounded declaration/import metadata as well as verified PATH entries;
it is not merely a list of filenames and is not complete patch evidence. Private
runtime state and credentials must not be placed in indexed source. Navigation
counts include only actual line-start PATH entries under the selected area.

Restarting a stopped objective preserves its retained plan and accepted sandbox
checkpoints. An explicitly different objective clears the previous mission plan;
late results from the old generation still cannot stage a build. Two exhausted
reasoning corrections stop instead of silently resetting the same retry budget.
These are CPU contract guarantees, not evidence that a native candidate was
successfully docked or chosen by an operator.
