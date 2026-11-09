"""Read a first-party implementation and its feature-owned textual parts.

Source-contract tests inspect implementation bodies, including deliberately
separated .inl files. This is not a C++ preprocessor or a source-admission API.
It never follows system includes or leaves the selected Engine source owners.
"""
from contextlib import contextmanager
from contextvars import ContextVar
import hashlib
import io
from pathlib import Path
import re

_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"\r\n]+\.inl)"[^\r\n]*(?:\r?\n|$)')
_DISCOVERY_SNAPSHOT = ContextVar('owned_source_discovery_snapshot', default=None)
_MISSING = object()


class _DiscoverySnapshot:
    def __init__(self, engine):
        self.engine = engine
        self.expansions = {}
        self.dependencies = {}

    def read_text(self, requested, canonical, encoding, errors):
        raw = canonical.read_bytes()
        fingerprint = (canonical, hashlib.sha256(raw).digest())
        previous = self.dependencies.setdefault(requested, fingerprint)
        if previous != fingerprint:
            raise RuntimeError(f'Source changed during Python discovery: {requested}')
        # Match Path.read_text's decoding and universal-newline behavior, while
        # hashing the exact bytes used for this snapshot (not a second read).
        with io.TextIOWrapper(io.BytesIO(raw), encoding=encoding, errors=errors) as stream:
            return stream.read()

    def validate(self):
        # This is an end-of-discovery consistency check, not a filesystem lease.
        # Test bodies always use fresh reads after this scope has ended.
        for requested, (canonical, digest) in self.dependencies.items():
            try:
                current = requested.resolve(strict=True)
                if current != canonical:
                    raise RuntimeError(f'Source redirected during Python discovery: {requested}')
                if hashlib.sha256(current.read_bytes()).digest() != digest:
                    raise RuntimeError(f'Source changed during Python discovery: {requested}')
            except (OSError, ValueError) as error:
                raise RuntimeError(f'Source unavailable after Python discovery: {requested}') from error

    def clear(self):
        self.expansions.clear()
        self.dependencies.clear()


@contextmanager
def discovery_source_snapshot(engine_root):
    """Reuse source expansions only while the shared runner imports its tests.

    The runner selects its own root/Engine. Other Engine trees remain fresh.
    All read dependencies must still have the same bindings and bytes on exit;
    a failed validation aborts discovery before any suite can be run or listed.
    """
    if _DISCOVERY_SNAPSHOT.get() is not None:
        raise RuntimeError('Source discovery snapshots cannot be nested')
    engine = Path(engine_root).resolve(strict=True)
    if engine.name != 'Engine':
        raise ValueError('Source discovery snapshot requires an Engine root')
    snapshot = _DiscoverySnapshot(engine)
    token = _DISCOVERY_SNAPSHOT.set(snapshot)
    completed = False
    try:
        yield
        completed = True
    finally:
        _DISCOVERY_SNAPSHOT.reset(token)
        try:
            if completed:
                snapshot.validate()
        finally:
            snapshot.clear()


def definition(source, signature):
    """Extract a balanced source body for the existing CPU contract consumers."""
    index = source.index(signature)
    opening = source.index('{', index)
    level, cursor = 1, opening + 1
    while level and cursor < len(source):
        level += (source[cursor] == '{') - (source[cursor] == '}')
        cursor += 1
    if level:
        raise ValueError(f'Unclosed source definition: {signature}')
    return source[index:cursor]


def read_owned_source(path, *, encoding='utf-8', errors=None,
                      maximum_files=512, maximum_bytes=16 * 1024 * 1024):
    """Expand quoted implementation includes within the main owner's source tree.

    The selected source tree may be an isolated Public build fixture. Each
    include must stay inside that Engine/src or its exact private integration owner; aliases,
    cycles, unbounded expansion and missing parts fail instead of hiding source.
    """
    requested = Path(path).absolute()
    snapshot = _DISCOVERY_SNAPSHOT.get()
    if snapshot is not None and not requested.is_relative_to(snapshot.engine):
        snapshot = None
    key = (requested, encoding, errors, maximum_files, maximum_bytes)
    if snapshot is not None:
        cached = snapshot.expansions.get(key, _MISSING)
        if cached is not _MISSING:
            return cached

    path = requested.resolve(strict=True)
    engine = next((parent for parent in path.parents if parent.name == 'Engine'), None)
    if snapshot is not None and engine != snapshot.engine:
        snapshot = None
    owners = () if engine is None else (engine/'src', engine/'modules', engine/'include',
                                        engine/'dep/EpochSelfCoding/integration')

    def read_text(current, requested):
        return current.read_text(encoding=encoding, errors=errors) if snapshot is None else \
            snapshot.read_text(requested, current, encoding, errors)

    if not any(path.is_relative_to(owner) for owner in owners):
        text = read_text(path, requested)
        if snapshot is not None:
            snapshot.expansions[key] = text
        return text
    files, total = 0, 0

    def expand(current, stack):
        nonlocal files, total
        requested = current.absolute()
        current = requested.resolve(strict=True)
        if not any(current.is_relative_to(owner) for owner in owners):
            raise ValueError(f'Implementation include escapes its admitted source owners: {current}')
        if current in stack:
            raise ValueError(f'Cyclic implementation include: {current}')
        files += 1
        total += current.stat().st_size
        if files > maximum_files or total > maximum_bytes:
            raise ValueError('Implementation include expansion exceeds its budget')
        text = read_text(current, requested)
        output = []
        for line in text.splitlines(keepends=True):
            match = _INCLUDE.fullmatch(line)
            if match:
                name = match.group(1).replace('\\', '/')
                include = current.parent / name
                if Path(name).is_absolute():
                    raise ValueError('Implementation includes must be relative')
                output.append(expand(include, (*stack, current)))
            else:
                output.append(line)
        return ''.join(output)

    text = expand(requested, ())
    if snapshot is not None:
        snapshot.expansions[key] = text
    return text
