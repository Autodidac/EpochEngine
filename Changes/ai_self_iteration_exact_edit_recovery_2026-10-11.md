# AI Self-Iteration: Recover Compacted Exact Edit Windows (2026-10-11)

## Failure reproduced from operator evidence

A coding generation could return concrete `epoch_propose_source_patch` operations (for example, `Engine/src/epochgui/gui.engine.cpp`) while the model's earlier exact source window was not retained in its current prompt. The host rejected the packet with `exact counted source or excerpt evidence is missing` and spent its small source-packet correction limit asking the model again without restoring those bytes. The run then stopped with zero sandbox operations staged, despite a valid-looking proposed edit.

## Change

`Engine/src/editor/ai/editor.ai_development_panel.cpp` now distinguishes a model's stale-but-unique edit anchor from an ungrounded or invented edit. Before consuming another source-action correction, the host looks for each requested exact-block search in **the current sandbox source snapshot**. It may schedule a bounded source-window recovery only if:

- the file was previously admitted to the reviewed source workspace;
- the canonical resolved file is a regular source file inside the reviewed snapshot root;
- the proposed original search text matches exactly **once** in that file;
- the revision-verified search region is not already present in resident source evidence;
- the search is at most 4096 bytes and the first-line cursor is within the navigation limits.

A read is not an edit. The original proposal is **not** accepted or staged by this recovery. The existing source-sharing pipeline re-reads the exact region, updates the digest-bound curated scope, and requires a fresh model action against newly resident source before the guarded proposal, sandbox transaction and compiler/test gates can run. Invalid paths, ambiguous matches, missing preimages and unsafe scope remain rejected.

The project contract in the same source file now exercises a compacted anchor at line 900, successful re-admission, resident range protection, unlisted paths and ambiguous edit anchors.

## Validation and limits

- Independently compiled and executed a C++23 harness containing the actual new helper function using representative source-workspace types: PASS (unique recovery, scope confinement, ambiguous-search rejection, already-resident protection).
- The new engine contract test is included in `editor.ai_development_panel.cpp`, but the **Windows Debug/Release builds and full Epoch engine contract suite were not run in the current Linux container**. Native Windows/MSVC validation is still required before treating this as an engine release.
- This repairs a specific no-edit dead end. It does not establish that an arbitrary model-generated patch compiles or passes the required engine review. Do not advertise successful source promotion without compiler, sandbox and operator receipts.

## Windows validation

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine.sln /t:EpochEditor /p:Configuration=Debug /p:Platform=x64 /m:1
.\x64\Debug\EpochEditor.exe --engine-contract-self-test
```

Then run a disposable engine self-coding case that targets a source function outside the first excerpt. Confirm a `Recovering ... exact model edit window` status followed by newly admitted `FILE_EXCERPT` evidence, a **new** patch proposal, a committed sandbox source transaction and host build/test receipts. Any unstaged outcome must display its first causal diagnostic without claiming completion. Validate Release and ReleaseWithDebugInfo separately after Debug passes.
