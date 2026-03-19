# Epoch v0.82.40

## Highlights

- Routed updater status back through Epoch logging with sanitized updater messages so the commandline pane stops showing the raw `♪◙` newline artifacts.
- Kept the newer-source fallback intact so a confirmed update still rebuilds from source when no newer packaged runtime exists.
- Repackaged the runtime drop as a fresh `main.zip` release for updater testing.

## Notes

- Packaged runtime releases still ship as `main.zip`.
- `main` will move ahead again after this release so source-update checks keep a newer live target.
