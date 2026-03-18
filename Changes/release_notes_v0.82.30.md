# Epoch v0.82.30 Release Notes

## Highlights

- Restored the source-update path so it no longer stops at a downloaded source
  snapshot. It now restores manifest dependencies with `vcpkg`, rebuilds the
  runtime from the extracted tree, and replaces the running binary from that
  fresh build output.
- Fixed updater version comparison so local builds newer than the latest
  packaged release no longer attempt to downgrade themselves.
- Normalized updater console line output so captured update logs stop showing
  raw CR/LF glyphs at the end of status lines.
