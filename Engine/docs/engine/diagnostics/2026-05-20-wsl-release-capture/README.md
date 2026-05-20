# WSL Release Capture Blocker - 2026-05-20

This diagnostic folder records the `v0.84.35` Linux/WSL release-proof attempt.

## Evidence

- WSL distro: `Ubuntu`, WSL2.
- Linux binary reports `Epoch v0.84.35`.
- `cmake --build --preset ninja-clang-debug` completed with no work needed
  after the current editor patch.
- `ctest --preset ninja-clang-debug --output-on-failure` passed
  `epoch_ci_headless`.
- Linux package staged at
  `C:\tmp\epoch_release\epoch_linux_x64_v0.84.35.tar.gz`.
- The staged Linux package reports `Epoch v0.84.35` and passes
  `./epoch_ci_headless .` after packaging the source-shaped runtime support
  paths needed by that validation executable.
- OpenGL command:
  `epoch --backend opengl --standalone --smoke --capture --scene linux-wsl-proof`
- OpenGL result:
  `linux-wsl-proof-opengl-black.png` is black and is not valid release proof.
- WSLg desktop capture result:
  `grim` fails because the compositor does not expose
  `wlr-screencopy-unstable-v1`; ImageMagick `import -window root` and `xwd`
  also fail to read the WSLg root image on this workstation.
- SFML command:
  `epoch --backend sfml --standalone --smoke --capture --scene linux-wsl-proof-sfml`
- SFML result:
  WSL/GLX failed `MakeCurrent` with `BadAccess`.
- Software command:
  `epoch --backend software --standalone --smoke --capture --scene linux-wsl-proof-software`
- Software result:
  exited without emitting a capture file.

## Acceptance Gate

Do not replace the README Linux screenshot from this pass until WSLg or a
native Linux desktop can produce an honest non-black visual smoke capture from
the same source line. If the Linux runtime package is published before that
visual fix, label it as build/headless/version-smoked proof, not as visual
screenshot proof.
