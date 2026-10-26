"""Read a first-party implementation and its feature-owned textual parts.

Source-contract tests inspect implementation bodies, including deliberately
separated .inl files. This is not a C++ preprocessor or a source-admission API.
It never follows system includes or leaves the selected Engine source owners.
"""
from pathlib import Path
import re

_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"\r\n]+\.inl)"[^\r\n]*(?:\r?\n|$)')


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
    path = Path(path).resolve(strict=True)
    engine = next((parent for parent in path.parents if parent.name == 'Engine'), None)
    owners = () if engine is None else (engine/'src', engine/'modules', engine/'include',
                                        engine/'dep/EpochSelfCoding/integration')
    if not any(path.is_relative_to(owner) for owner in owners):
        return path.read_text(encoding=encoding, errors=errors)
    files, total = 0, 0

    def expand(current, stack):
        nonlocal files, total
        current = current.resolve(strict=True)
        if not any(current.is_relative_to(owner) for owner in owners):
            raise ValueError(f'Implementation include escapes its admitted source owners: {current}')
        if current in stack:
            raise ValueError(f'Cyclic implementation include: {current}')
        files += 1
        total += current.stat().st_size
        if files > maximum_files or total > maximum_bytes:
            raise ValueError('Implementation include expansion exceeds its budget')
        text = current.read_text(encoding=encoding, errors=errors)
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

    return expand(path, ())
