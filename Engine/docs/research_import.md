# Research Import

Epoch planning material can come from anywhere in the workspace. PDFs, HTML
prototypes, scratch notes, and downloaded documents should be staged into a
neutral research-import path before they rewrite roadmap language, datasets, or
automation policy.

## Staging path

- Stage imports under `workspace/research/staged/`
- Treat that area as review-only input, not promoted truth
- Keep the original source path and hash in provenance so the origin stays
  auditable

## Import tool

Use [Tools/research_import.ps1](/C:/Users/iammi/.codex/worktrees/2a8f/epoch_vibed/Tools/research_import.ps1):

```powershell
powershell -File Tools/research_import.ps1 -InputPath "C:\path\to\doc.pdf"
```

The tool writes:

- `extracted.txt`
- `review.md`
- `provenance.json`

Each staged import gets its own timestamped folder so later promotion can point
back to one specific reviewed input.

## Source types

- PDF: extracted through the local `python` or `py` launcher with `pypdf` when
  available
- HTML: stripped into readable text with scripts/styles removed
- text/markdown: copied through as raw text

## Promotion rule

Imported research is staged only until a reviewer promotes the relevant
findings. Promotion should be explicit and narrow:

1. Identify the specific insight that matters
2. Link back to the staged provenance
3. Update the roadmap/docs/datasets/policy intentionally
4. Leave utility-only sources out of Epoch planning truth unless they really
   inform the engine

## Botface note

`botface.html` is a local LLM/control surface experiment by default. It is not
Epoch roadmap truth just because it exists in the repo. If something from it is
useful, stage and review that finding like any other import.
