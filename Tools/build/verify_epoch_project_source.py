#!/usr/bin/env python3
"""Admit a generated project's source dependency before configuring or linking.

This is source/edition admission, not OS confinement or publication authority.
Public inputs also pass the existing complete Public source-content/hash gate.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess

EDITIONS = {'epoch.public.github.v1': '1', 'epoch.development.site.v1': '0'}


def reject_aliases(root: Path, path: Path) -> None:
    for part in (path, *path.parents):
        if part == root:
            break
        if part.is_symlink() or getattr(part, 'is_junction', lambda: False)():
            raise RuntimeError('Engine source input contains an alias')


def check_source(root: Path, expected: str) -> None:
    if expected not in EDITIONS:
        raise RuntimeError('Unknown generating edition')
    requested = root.absolute()
    root = requested.resolve(strict=True)
    if root != requested or not root.is_dir():
        raise RuntimeError('Generated project source must be a canonical directory')
    marker = root / 'Engine/include/epoch.edition.hpp'
    reject_aliases(root, marker)
    if marker.is_symlink() or not marker.is_file() or marker.stat().st_size > 4096:
        raise RuntimeError('Physical source edition marker is missing or aliased')
    text = marker.read_text(encoding='utf-8')
    defines = re.findall(r'(?m)^\s*#\s*define\s+EPOCH_SOURCE_EDITION_PUBLIC\b[^\r\n]*', text)
    if len(defines) != 1 or defines[0].strip() != '#define EPOCH_SOURCE_EDITION_PUBLIC ' + EDITIONS[expected]:
        raise RuntimeError('Source edition differs from the generating edition')
    if not (root / 'Engine/CMakeLists.txt').is_file():
        raise RuntimeError('Source root lacks the engine build definition')
    if expected == 'epoch.development.site.v1':
        return

    manifest_file = root / 'source-projection.json'
    if (manifest_file.is_symlink() or not manifest_file.is_file()
            or manifest_file.stat().st_size > 4 * 1024 * 1024):
        raise RuntimeError('Public projects require an audited Public source projection')
    manifest = json.loads(manifest_file.read_text(encoding='utf-8'))
    gate_file = Path(__file__).with_name('verify_public_source.py')
    spec = importlib.util.spec_from_file_location('epoch_project_public_source_gate', gate_file)
    gate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gate)
    names = gate.inventory(root, manifest)
    gate.check(root, manifest, names)

    # Flat ZIP inventory deliberately excludes local build/cache state. Also
    # reject unmanifested implementation files in the actual engine owners:
    # placing private files alongside an otherwise valid projection is unsafe.
    for owner in ('Engine/src', 'Engine/modules', 'Engine/include',
                  'Engine/dep', 'Engine/examples',
                  'Engine/assets/ai', 'Engine/resource/ai'):
        directory = root / owner
        reject_aliases(root, directory)
        if not directory.exists():
            continue
        for path in directory.rglob('*'):
            if path.is_symlink() or (getattr(path, 'is_junction', lambda: False)()):
                raise RuntimeError('Public source owner contains an aliased input')
            if path.is_file() and path.relative_to(root).as_posix() not in names:
                raise RuntimeError('Public source owner contains an unmanifested input')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--edition', required=True, choices=tuple(EDITIONS))
    args = parser.parse_args()
    try:
        check_source(args.root, args.edition)
    except (OSError, RuntimeError, ValueError, KeyError, TypeError,
            subprocess.SubprocessError) as error:
        parser.exit(2, 'Generated project source refused: ' + str(error) + '\n')
    print('generated_project_source.result=pass edition=' + args.edition)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
