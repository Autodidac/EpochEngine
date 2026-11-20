"""Verify the exact llama.cpp source tree used by the Android Coach build."""

import hashlib
from pathlib import Path, PurePosixPath
import sys
import tarfile


def verify(archive: Path, root: Path) -> None:
    expected = set()
    with tarfile.open(archive, "r|gz") as source:
        for entry in source:
            parts = PurePosixPath(entry.name).parts
            if not parts or parts[0] != root.name or any(part in {".", "..", "/"} for part in parts):
                raise ValueError("The source archive has an unexpected path.")
            if entry.isdir():
                continue
            if not entry.isfile() or len(parts) < 2:
                raise ValueError("The source archive has an unexpected entry type.")
            relative = Path(*parts[1:])
            if relative in expected:
                raise ValueError("The source archive lists the same file twice.")
            expected.add(relative)
            path = root / relative
            if path.resolve(strict=True) != path or not path.is_file() or path.stat().st_size != entry.size:
                raise ValueError(f"The extracted source path or size changed: {relative}")
            with source.extractfile(entry) as archived, path.open("rb") as extracted:
                if hashlib.file_digest(archived, "sha256").digest() != hashlib.file_digest(extracted, "sha256").digest():
                    raise ValueError(f"The extracted source differs from the pinned archive: {relative}")
    actual = set()
    for path in root.rglob("*"):
        if path.is_symlink() or getattr(path, "is_junction", lambda: False)():
            raise ValueError("The extracted source contains a redirected path.")
        if path.is_file():
            actual.add(path.relative_to(root))
    if actual != expected:
        raise ValueError("The extracted source contains missing or unexpected files.")


if __name__ == "__main__":
    try:
        verify(Path(sys.argv[1]), Path(sys.argv[2]).resolve(strict=True))
    except (OSError, ValueError, tarfile.TarError) as error:
        sys.exit(str(error))
