"""Execute the production Windows contract environment scope without a runtime."""
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]

class PackageValidationScratch(unittest.TestCase):
    @unittest.skipUnless(shutil.which("pwsh"), "PowerShell is required for the runtime packager")
    def test_source_only_workflow_skips_only_older_platform_pins(self):
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent, prefix="package-version-") as directory:
            root = Path(directory)
            reader = Path("Tools/build/get_epoch_version.ps1")
            (root / reader.parent).mkdir(parents=True)
            shutil.copyfile(ROOT / reader, root / reader)
            module = root / "Engine/modules/epoch.version.ixx"
            module.parent.mkdir(parents=True)
            driver = root / "check.ps1"
            driver.write_text("param($helper, $root)\n$ErrorActionPreference = 'Stop'\n. $helper\n"
                              "Get-EpochRuntimePackageVersion -RepoRoot $root -Platform linux-x64 -SkipSourceOnly\n")
            for pin, expected_exit, output in ((13, 0, ""), (14, 0, "0.91.14"), (15, 1, "")):
                with self.subTest(pin=pin):
                    module.write_text("".join(
                        f"#define EPOCH_{prefix}VERSION_{part}_VALUE {value}\n"
                        for prefix, revision in (("", 14), ("LINUX_PACKAGED_", pin))
                        for part, value in (("MAJOR", 0), ("MINOR", 91), ("REVISION", revision))))
                    result = subprocess.run(["pwsh", "-NoProfile", "-File", str(driver),
                                             str(ROOT / "Tools/build/epoch_package_layout.ps1"), str(root)],
                                            capture_output=True, text=True, timeout=20)
                    self.assertEqual(result.returncode, expected_exit, result.stderr)
                    self.assertEqual(result.stdout.strip(), output)

    @unittest.skipUnless(shutil.which("pwsh"), "PowerShell is required for the runtime packager")
    def test_staging_uses_built_shaders_and_preserves_fallbacks(self):
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent, prefix="package-shaders-") as directory:
            runtime, stage = (Path(directory) / name for name in ("runtime", "stage"))
            shaders = Path("assets/vulkan/shaders")
            for path in (stage, runtime):
                (path / shaders).mkdir(parents=True)
            for name in ("frag", "atmosphere.frag", "clouds.frag", "gui_vert"):
                (stage / shaders / (name + ".spv")).write_bytes(b"source fallback")
            for name in ("frag", "atmosphere.frag", "clouds.frag", "unrelated"):
                (runtime / shaders / (name + ".spv")).write_bytes(name.encode())
            driver = Path(directory) / "check.ps1"
            driver.write_text("param($helper,$runtime,$stage)\n$ErrorActionPreference='Stop'\n"
                              ". $helper\nCopy-EpochBuiltVulkanShaders -RuntimeRoot $runtime -Stage $stage\n")
            subprocess.run(["pwsh", "-NoProfile", "-File", str(driver),
                            str(ROOT / "Tools/build/epoch_package_layout.ps1"), str(runtime), str(stage)],
                           check=True, timeout=30)
            self.assertEqual({p.stem: p.read_bytes() for p in (stage / shaders).glob("*.spv")},
                             {"frag": b"frag", "atmosphere.frag": b"atmosphere.frag",
                              "clouds.frag": b"clouds.frag", "gui_vert": b"source fallback"})

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
            archive = "Compress-Archive" if name == "windows" else "$archiveScript ="
            move = source.index("Move-EpochPackageValidationEvidence -Stage $stage")
            self.assertLess(source.index("--engine-contract-self-test"), move)
            self.assertLess(move, source.index(archive))
        linux = (ROOT / "Tools/build/stage_epoch_linux_release.ps1").read_text()
        self.assertIn("export TMPDIR='__LOGS__/scratch'", linux)
        self.assertLess(linux.index("export TMPDIR="), linux.index("./epoch --engine-contract-self-test"))
