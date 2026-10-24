#!/usr/bin/env python3
"""Insert concise, evidence-based Epoch system notes under copyright headers.

Safeguards: code-only; no vendor/build trees; no license modification; BOM and
newline preservation; at most four notes; idempotent; --check is read-only.
"""
from __future__ import annotations
import argparse
import re
from pathlib import Path

EXTENSIONS = {'.cpp', '.cc', '.cxx', '.hpp', '.h', '.ixx'}
SKIP_SEGMENTS = {'third_party', '3rdparty', 'external', 'vendor', 'build', 'built', 'Bin', 'x64', '.git', 'vcpkg_installed'}
NOTES = {
 'stl': 'Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.',
 'context': 'Epoch Context: window/context state, dispatch and input ownership remain scoped.',
 'gui': 'EpochGUI: use consistent draw/hit-test geometry and preserve child input ownership.',
 'render': 'Epoch Render: preserve backend boundaries and resource lifetime contracts.',
 'ai': 'Assistant: preserve model selection, candidate isolation and host verification gates.',
 'editor': 'Epoch Editor: keep editor state and source changes within their owning workflows.',
 'platform': 'Epoch Platform: keep native code isolated behind platform-specific interfaces.',
 'core': 'Epoch Core: preserve engine-defined primitives, APIs and module contracts.',
}


def select_notes(rel: Path, text: str) -> list[str]:
    name = '/'.join(rel.parts).lower()
    body = text[:10000]
    keys = []
    if ('core.stl_types.hpp' in body or re.search(r'\bepochengine::(?:string|wstring|string_view|vector|span)\b', body) or name.endswith('core.stl_types.hpp')):
        keys.append('stl')
    if re.search(r'(?:^|[/.])context(?:[/.]|$)', name) or 'context.multiplexer' in body or 'context.admission' in body:
        keys.append('context')
    if '/epochgui/' in name or '/gui.' in name or name.startswith('engine/modules/gui.'):
        keys.append('gui')
    if '/render/' in name or name.startswith('engine/modules/render.') or '/render.' in name:
        keys.append('render')
    if '/ai/' in name or name.startswith('engine/modules/ai.') or name.startswith('engine/ai/'):
        keys.append('ai')
    if '/editor/' in name or name.startswith('engine/modules/editor.'):
        keys.append('editor')
    if '/platform/' in name or name.startswith('engine/modules/platform.'):
        keys.append('platform')
    if '/core/' in name or name.startswith('engine/modules/core.'):
        keys.append('core')
    return [NOTES[key] for key in keys[:4]]


def annotate(data: bytes, rel: Path) -> bytes:
    bom = b'\xef\xbb\xbf' if data.startswith(b'\xef\xbb\xbf') else b''
    payload = data[len(bom):]
    nl = b'\r\n' if b'\r\n' in payload[:6000] else b'\n'
    text = payload.decode('utf-8')
    if 'Epoch system notes (auto-checked)' in text[:1800]:
        return data
    notes = select_notes(rel, text)
    if not notes:
        return data
    note_lines = ('// Epoch system notes (auto-checked):\n' + ''.join('// ' + note + '\n' for note in notes)).encode('utf-8').replace(b'\n', nl)
    # Insert immediately below the leading copyright/license block when present.
    lead = re.match(r'\A[ \t\r\n]*/\*.*?\*/', text, re.S)
    if lead:
        point = len(text[:lead.end()].encode('utf-8'))
        return bom + payload[:point] + nl + note_lines + payload[point + len(nl):] if payload[point:point + len(nl)] == nl else bom + payload[:point] + nl + note_lines + payload[point:]
    # Preserve an existing contiguous // copyright header.
    lead_lines = text.splitlines(keepends=True)
    point = 0
    for line in lead_lines:
        if line.lstrip().startswith('//'):
            point += len(line.encode('utf-8'))
        else:
            break
    return bom + payload[:point] + note_lines + payload[point:]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('source_root', type=Path, help='Epoch Engine checkout root')
    parser.add_argument('--check', action='store_true', help='dry-run: report only')
    args = parser.parse_args()
    root = args.source_root.resolve()
    source = root/'Engine'
    if not source.is_dir():
        parser.error('Engine/ not found; use the Epoch Engine checkout root')
    found = changed = 0
    for path in sorted(source.rglob('*')):
        if not path.is_file() or path.suffix not in EXTENSIONS:
            continue
        if any(part in SKIP_SEGMENTS for part in path.relative_to(root).parts):
            continue
        found += 1
        try:
            before = path.read_bytes()
            after = annotate(before, path.relative_to(root))
        except (UnicodeDecodeError, OSError):
            continue
        if before != after:
            changed += 1
            if not args.check:
                path.write_bytes(after)
    print(f'{"Would annotate" if args.check else "Annotated"}: {changed} / {found} files; no license blocks changed.')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
