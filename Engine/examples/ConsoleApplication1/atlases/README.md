# Prebaked Example Atlases

This folder is the tracked prebaked atlas output for the example runtime.

## Contract

- Keep source images under `../assets/`.
- Keep tracked prebaked atlases here when they are needed so new clones do not
  have to regenerate everything before the example runtime is usable.
- Keep disposable generated/runtime atlas output under executable-local
  `cache/atlases/` and out of git.

## Not source of truth

This folder is not the place to hand-edit sprite source art. If an atlas needs
to change, update the source images under `../assets/` and then regenerate or
replace the prebaked atlas output intentionally.
