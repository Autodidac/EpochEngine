"""Execute the production Windows contract environment scope without a runtime."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class PackageValidationScratch(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "Windows production staging scope")
    def test_actual_scope_restores_environment_on_success_and_failure(self):
        source = (ROOT / "Tools/build/stage_epoch_windows_release.ps1").read_text()
        block = source.split("$contractScratch =", 1)[1].split("if ($contractProcess.ExitCode", 1)[0]
        block = "$contractScratch =" + block
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent, prefix="package-scratch-") as folder:
            script = Path(folder) / "scope.ps1"
            script.write_text("""param($verifyLogs)
$ErrorActionPreference = 'Stop'
$stage = $verifyLogs
$contractData = $verifyLogs
$env:TEMP = 'original-temp'
$env:TMP = 'original-tmp'
function Start-Process {
    if ($env:TEMP -ne $contractScratch -or $env:TMP -ne $contractScratch) { throw 'wrong scratch' }
    if (!(Test-Path -LiteralPath $env:TEMP)) { throw 'missing scratch' }
    if ($script:failLaunch) { throw 'expected launch failure' }
    return @{ ExitCode = 0 }
}
foreach ($script:failLaunch in @($false, $true)) {
    try {
""" + block + """
    } catch { if (!$script:failLaunch -or $_.Exception.Message -ne 'expected launch failure') { throw } }
    if ($env:TEMP -ne 'original-temp' -or $env:TMP -ne 'original-tmp') { throw 'environment leaked' }
}
""", encoding="utf-8")
            result = subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script), folder], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_validation_evidence_is_not_deleted_or_packaged(self):
        for name in ("windows", "linux"):
            source = (ROOT / f"Tools/build/stage_epoch_{name}_release.ps1").read_text()
            self.assertIn("[guid]::NewGuid()", source)
            self.assertNotIn("Remove-EpochPackageItem -LiteralPath $verifyLogs", source)
        linux = (ROOT / "Tools/build/stage_epoch_linux_release.ps1").read_text()
        self.assertIn("export TMPDIR='__LOGS__/scratch'", linux)
        self.assertLess(linux.index("export TMPDIR="), linux.index("./epoch --engine-contract-self-test"))
