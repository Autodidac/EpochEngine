"""Run the real embedded Windows PowerShell snapshot owner on tiny local fixtures."""
from pathlib import Path
import os
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == 'nt', 'Windows PowerShell 5.1 snapshot entry point')
class ZipSnapshot(unittest.TestCase):
    def test_actual_pruning_source_integrity_and_junction_refusal(self):
        # A parent PowerShell 7 may inject incompatible module search paths.
        # Let Windows PowerShell 5.1 select its own installed standard modules.
        env = {key: value for key, value in os.environ.items()
               if key.casefold() != 'psmodulepath'}
        result = subprocess.run(
            ['powershell.exe', '-NoLogo', '-NoProfile', '-NonInteractive',
             '-ExecutionPolicy', 'Bypass', '-File',
             str(ROOT / 'Tests/source/zip_snapshot_contract.ps1')],
            cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
