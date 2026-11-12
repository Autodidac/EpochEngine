"""Verify the actual POSIX restart arguments through a native shell."""

from pathlib import Path
import os
import subprocess
import tempfile
import unittest

from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import definition


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == 'posix', 'POSIX updater restart shell')
class PosixRestartArguments(unittest.TestCase):
    def test_source_and_packaged_restarts_preserve_data_root_as_one_argument(self):
        reports = ROOT / 'Artifacts/Reports/updater-handoff-nov07'
        reports.mkdir(parents=True, exist_ok=True)
        report = Path(tempfile.mkdtemp(prefix='posix-restart-', dir=reports))
        owner = (ROOT / 'Engine/src/updater/updater.system.cpp').read_text(encoding='utf-8')
        self.assertEqual(owner.count('posix_restart_arguments(handoff->candidate_data)'), 3)
        quoting = definition(owner, '[[nodiscard]] inline std::string quote_shell_arg(')
        restart = definition(owner, '[[nodiscard]] std::string posix_restart_arguments(')
        source = report / 'restart.cpp'
        source.write_text(r'''
#include <filesystem>
#include <fstream>
#include <string>
namespace system_detail {
''' + quoting + '\n}\n' + restart + r'''
int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    const std::string suffix{argv[2]};
    std::ofstream ordinary("ordinary-" + suffix + ".sh"), candidate("candidate-" + suffix + ".sh");
    ordinary << "exec \"$1\"" << posix_restart_arguments({}) << '\n';
    candidate << "exec \"$1\"" << posix_restart_arguments(argv[1]) << '\n';
    ordinary.close();
    candidate.close();
    return ordinary && candidate ? 0 : 2;
}
''', encoding='utf-8')
        candidate = report / '''disposable 雪 '"\\ $EPOCH_FIXTURE_SHELL `%!& data'''
        compile_and_run(self, source, report, args=lambda mode: (candidate, mode))
        target = report / 'target fixture.sh'
        target.write_text('''#!/bin/sh\nfor arg do printf '%s\\0' "$arg"; done\n''', encoding='utf-8')
        target.chmod(0o755)
        for configuration in ('Debug', 'Release'):
            for name, expected in (('ordinary', b''),
                                   ('candidate', b'--candidate-data-root\0' + os.fsencode(candidate) + b'\0')):
                result = subprocess.run(['/bin/sh', str(report / f'{name}-{configuration}.sh'), str(target)],
                                        cwd=report, capture_output=True, timeout=10, check=True)
                self.assertEqual(result.stdout, expected)
