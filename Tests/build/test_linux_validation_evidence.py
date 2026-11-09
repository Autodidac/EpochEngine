"""Run the production evidence move against real files; no runtime involved."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHELL = shutil.which('pwsh') or (shutil.which('powershell.exe') if os.name == 'nt' else None)


@unittest.skipUnless(SHELL, 'PowerShell is required for the production packager')
class LinuxValidationEvidence(unittest.TestCase):
    def test_preserves_runtime_logs_and_leaves_unknown_payload_for_audit(self):
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent, prefix='linux-evidence-') as folder:
            root = Path(folder)
            stage, evidence = root/'stage', root/'evidence'
            reports = 'Artifacts/Reports/epoch_engine_timeline.tsv'
            admitted = ('logs/session.log', 'cache/update.bin', 'build/logs/epoch_engine_timeline.tsv',
                        'build/cache/atlas.bin', reports)
            unadmitted = ('build/unexpected.txt', 'Artifacts/unexpected.txt')
            for name in admitted + unadmitted:
                path = stage/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(name.encode())
            script = root/'move.ps1'
            script.write_text('param($helper,$stage,$evidence,$root)\n$ErrorActionPreference="Stop"\n. $helper\nMove-EpochPackageValidationEvidence -Stage $stage -VerifyLogs $evidence -PackageRoot $root\n')
            command = [SHELL, '-NoProfile', '-File', str(script), str(ROOT/'Tools/build/epoch_package_layout.ps1'), str(stage), str(evidence), str(root)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            for name in admitted:
                self.assertEqual((evidence/'runtime'/name).read_bytes(), name.encode())
                self.assertFalse((stage/name).exists())
            self.assertFalse((stage/'Artifacts/Reports').exists())
            for name in unadmitted:
                self.assertEqual((stage/name).read_bytes(), name.encode())
            # Never overwrite retained evidence on a repeat attempt.
            (stage/'logs').mkdir()
            (stage/'logs/new.log').write_text('new')
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue((stage/'logs/new.log').exists())
            # A caller cannot retain diagnostics inside the archive itself.
            command[-2] = str(stage/'inside')
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
