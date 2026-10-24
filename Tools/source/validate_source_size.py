#!/usr/bin/env python3
"""Enforce the 4,000-line first-party C++ ownership limit on every platform."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path

CPP_EXTENSIONS = {'.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx',
                  '.ixx', '.cppm', '.mpp', '.inl', '.inc', '.ipp', '.tpp'}
EXCLUDED_DIRECTORIES = {'.git', 'cache', 'captures',
                        'logs', 'debug', 'release', 'releasewithdebuginfo',
                        'x64', 'x86', 'third_party', 'vendor', 'stb',
                        'node_modules', 'vcpkg_installed', '__pycache__'}


@dataclass(frozen=True)
class Violation:
    path: str
    lines: int | None
    reason: str


def first_party_roots(repo_root: Path):
    for relative in ('Engine/src', 'Engine/modules', 'Engine/include',
                     'Engine/examples', 'Tools', 'Tests'):
        path = repo_root / relative
        if path.is_dir():
            yield path
    manifest = repo_root / 'Engine/dep/dependencies.json'
    if manifest.is_file():
        for dependency in json.loads(manifest.read_text(encoding='utf-8-sig'))['dependencies']:
            if str(dependency.get('mode', '')).startswith('engine_owned_'):
                path = repo_root / dependency['path']
                if not path.resolve().is_relative_to(repo_root):
                    raise ValueError('Owned dependency path escapes the repository')
                if path.is_dir():
                    yield path


def linked(path: Path):
    return path.is_symlink() or getattr(path, 'is_junction', lambda: False)()


def audit(repo_root, maximum_lines=4000):
    repo_root = Path(repo_root).resolve(strict=True)
    checked, violations = 0, []
    seen = set()
    for owner in first_party_roots(repo_root):
        if linked(owner):
            violations.append(Violation(str(owner.relative_to(repo_root)), None, 'linked source ownership'))
            continue
        for directory, children, files in os.walk(owner, followlinks=False):
            current = Path(directory)
            retained = []
            for name in children:
                child = current / name
                if name.lower() in EXCLUDED_DIRECTORIES:
                    continue
                # src/build and Tools/build contain production source. Only
                # example/dependency-owned configure trees are generated here.
                if name.lower() in {'build', 'built', 'bin'} and (
                        owner == repo_root / 'Engine/examples'
                        or (owner.is_relative_to(repo_root / 'Engine/dep')
                            and current == owner)):
                    continue
                if linked(child):
                    violations.append(Violation(str(child.relative_to(repo_root)), None, 'linked source ownership'))
                else:
                    retained.append(name)
            children[:] = retained
            for name in files:
                path = current / name
                if path.suffix.lower() not in CPP_EXTENSIONS or path in seen:
                    continue
                seen.add(path)
                relative = path.relative_to(repo_root).as_posix()
                if linked(path):
                    violations.append(Violation(relative, None, 'linked source ownership'))
                    continue
                with path.open(encoding='utf-8-sig', errors='strict') as stream:
                    count = sum(1 for _ in stream)
                checked += 1
                if count > maximum_lines:
                    violations.append(Violation(relative, count, f'exceeds {maximum_lines} lines'))
    return checked, sorted(violations, key=lambda row: row.path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--maximum-lines', type=int, default=4000)
    args = parser.parse_args()
    if args.maximum_lines <= 0:
        parser.error('maximum-lines must be positive')
    checked, violations = audit(args.repo_root, args.maximum_lines)
    for violation in violations:
        size = f' ({violation.lines} lines)' if violation.lines is not None else ''
        print(f'[ERROR] {violation.path}{size}: {violation.reason}')
    if violations:
        return 1
    print(f'First-party source size passes: {checked} files, maximum {args.maximum_lines} lines.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
