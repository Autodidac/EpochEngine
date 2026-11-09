#!/usr/bin/env python3
"""Reject private build-path literals in public runtime executables and libraries."""
import argparse
from pathlib import Path
import re


# Generic hosted CI accounts are permitted. Personal profiles and private
# worktree locations are not release inputs, even when found in debug metadata.
PRIVATE_BUILD_PATH = re.compile(
    r"[\\/]\.codex[\\/]worktrees[\\/]"
    r"|(?:[a-z]:[\\/]|/mnt/[a-z]/)Users[\\/]"
    r'(?!(?:Public|Default|runneradmin)[\\/])[^\\/:*?"<>|\x00-\x1f]{1,128}[\\/]'
    r"|[/]home/(?!runner/)[^/\x00-\x1f]{1,128}/",
    re.IGNORECASE | re.ASCII,
)


def audit_binary(stream, name):
    carry = b""
    while block := stream.read(1024 * 1024):
        data = carry + block
        if (PRIVATE_BUILD_PATH.search(data.decode("latin-1"))
                or PRIVATE_BUILD_PATH.search(data.decode("utf-16-le", errors="replace"))
                or PRIVATE_BUILD_PATH.search(data[1:].decode("utf-16-le", errors="replace"))):
            # Do not print the discovered private path in build or CI logs.
            raise ValueError(f"Private build path in public runtime binary: {name}")
        carry = data[-512:]


def audit_windows_runtime(root):
    if not (root / "EpochEditor.exe").is_file():
        raise ValueError("Missing Windows release executable")
    for path in root.rglob("*"):
        if path.is_file() and path.suffix.lower() in {".exe", ".dll"}:
            with path.open("rb") as stream:
                audit_binary(stream, path.relative_to(root).as_posix())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows-runtime", required=True, type=Path)
    args = parser.parse_args()
    try:
        audit_windows_runtime(args.windows_runtime)
    except ValueError as error:
        parser.exit(1, f"{error}\n")
    print("runtime_provenance.result=pass")
