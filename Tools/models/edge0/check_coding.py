"""Compile previously reviewed model answers against the same fixed C++ fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from Tests.ai.test_asset_job_cli import run_owned_hidden


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report', type=Path)
    parser.add_argument('--reviewed', action='store_true', required=True)
    args = parser.parse_args()
    report = args.report.resolve()
    data = json.loads(report.read_text(encoding='utf-8'))
    fixtures = json.loads(Path(__file__).with_name('coding_fixtures.json').read_text())
    prefix = report.name.removesuffix('-baseline.json')
    results = []
    for case in data['cases']:
        name = case['name']
        if name not in fixtures:
            raise ValueError(f'No fixed fixture for {name}')
        entry = {'name': name, 'finish_reason': case['finish_reason'],
                 'fixture_sha256': hashlib.sha256(fixtures[name].encode()).hexdigest()}
        if case['finish_reason'] != 'stop':
            entry.update(passed=False, cause='incomplete answer', scored=False)
            results.append(entry)
            continue
        text = case['content'].strip()
        fenced = re.fullmatch(r'```(?:cpp|c\+\+)?\s*\n(.+?)\n```', text, re.S)
        code = fenced[1] if fenced else text
        source = report.with_name(f'{prefix}-{name}.cpp')
        executable = source.with_suffix('.exe')
        # Keep faulty in-memory answers bounded, including allocation loops.
        # This is process/memory supervision, not filesystem confinement.
        guarded_fixture = ('\n#define main epoch_fixture_main\n' + fixtures[name]
            + '\n#undef main\n#include <new>\n'
            + 'int main() { try { return epoch_fixture_main(); } '
              'catch (const std::bad_alloc&) { return 90; } catch (...) { return 91; } }\n')
        source.write_text(code + '\n' + guarded_fixture, encoding='utf-8')
        build = subprocess.run(['cl.exe', '/nologo', '/std:c++latest', '/EHsc', '/W4',
                                f'/Fe{executable.name}', str(source)], cwd=report.parent,
                               timeout=30, capture_output=True, text=True)
        entry['compiler_output'] = build.stdout + build.stderr
        entry['compiled'] = build.returncode == 0
        entry['scored'] = True
        entry['passed'] = False
        if entry['compiled']:
            run = run_owned_hidden([executable], report.parent, os.environ.copy(), 5,
                                   memory_limit_bytes=256 * 1024**2)
            entry.update(passed=run['exitCode'] == 0 and not run['timedOut']
                         and run['activeProcessesAtExit'] == 0,
                         test_exit=run['exitCode'], process=run,
                         memory_limit_bytes=256 * 1024**2)
            if run['timedOut']:
                entry['cause'] = 'fixture timeout'
            elif run['exitCode'] == 90:
                entry['cause'] = 'allocation failed under 256 MiB memory limit'
        results.append(entry)
    output = report.with_name(f'{prefix}-code-results.json')
    output.write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps([{k: v for k, v in entry.items() if k not in {'compiler_output', 'test_output'}} for entry in results], indent=2))


if __name__ == '__main__':
    main()
