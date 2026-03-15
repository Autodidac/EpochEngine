# Epoch v0.82.16

## Highlights

- Fixed the Windows script compiler path so editor-run scripts launch `clang++`
  directly instead of routing through a fragile shell command string.
- This removes the `C:/Program` split failure when LLVM is installed under
  `Program Files`.

## Verification

- Rebuilt `ConsoleApplication1` in `Debug|x64`
- Launched the debug app from `x64/Debug`
- Confirmed the script source file remains ASCII-only
