# Epoch Engine public source

Reusable software/game engine and editor source, with inference and ordinary
project assistance. Engine self-development automation and private developer
source/update integration are excluded from this edition.

Build from Engine.sln on Windows or the root CMake presets. Read
[build guidance](Engine/docs/build/presets.md), [GUI](Engine/docs/engine/gui.md)
and [workflows](Engine/docs/engine/workflows.md). License terms remain in the
included LICENSE files. Source projection checks do not establish a passing
build or runtime; see the release's separate validation evidence.

Run `python Tools/build/verify_public_source.py` for the public source gate.
`source-projection.json` declares `sha256-git-text-lf-v1`: known text files hash
with CRLF converted to LF, all other bytes preserved; binary files hash raw.
The gate documents the exact text extensions and basenames. Exported file bytes
and archive digests remain raw. Intentional public source changes require an
explicit `--refresh-manifest` after staging, which repeats the same safety gates.
