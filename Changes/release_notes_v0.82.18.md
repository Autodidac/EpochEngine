# Epoch v0.82.18

## Highlights

- Fixed the remaining Windows editor script compiler spawn bug by using a
  direct executable launch for real LLVM paths instead of the PATH-search
  variant.
- This closes the last known `Program Files` path split that still produced
  `Files/LLVM/bin/clang++.exe` style failures during editor-run script builds.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Re-checked the compiler launch logic against the installed LLVM path
- Refreshed the active release/version surfaces to `v0.82.18`
