"""Run the production executable transaction against local failure fixtures."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / "Artifacts/Reports/updater-handoff-nov07"


class ExecutableHandoff(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name != "nt":
            raise unittest.SkipTest("Windows installed MSVC transaction lane")
        REPORT.mkdir(parents=True, exist_ok=True)
        cls.root = Path(tempfile.mkdtemp(prefix="fixtures-", dir=REPORT))
        cls.scripts = {}
        toolset = sorted(Path("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC").glob("*"))[-1]
        kitroot = Path("C:/Program Files (x86)/Windows Kits/10")
        kit = sorted((kitroot / "Include").glob("*"))[-1]
        for mode in ("Debug", "Release"):
            folder = cls.root / mode
            folder.mkdir()
            binary = folder / "emitter.exe"
            code = (ROOT / "Tests/updater/updater.handoff_contract.cpp").read_text()
            production = (ROOT / "Engine/src/updater/updater.system.cpp").read_text(encoding="utf-8")
            helpers = production[production.index("    struct BinaryHandoff\n"):production.index("    bool replace_binary_from_script(")]
            code = code.replace("PRODUCTION_HELPERS", helpers).replace('../../Engine/src/updater/updater.handoff_script.hpp', (ROOT / 'Engine/src/updater/updater.handoff_script.hpp').as_posix())
            unit = folder / "contract.cpp"
            unit.write_text(code)
            args = [str(toolset / "bin/Hostx64/x64/cl.exe"), "/nologo", "/std:c++latest", "/EHsc", "/W4", "/WX",
                    *( ["/Od", "/D_DEBUG", "/MDd"] if mode == "Debug" else ["/O2", "/DNDEBUG", "/MD"] ),
                    "/I" + str(toolset / "include"), "/I" + str(kit / "ucrt"), "/I" + str(kit / "shared"), "/I" + str(kit / "um"),
                    "/Fo" + str(folder / "emitter.obj"), "/Fe" + str(binary), str(unit),
                    "/link", "/LIBPATH:" + str(toolset / "lib/x64"), "/LIBPATH:" + str(kitroot / "Lib" / kit.name / "um/x64"),
                    "/LIBPATH:" + str(kitroot / "Lib" / kit.name / "ucrt/x64")]
            compiled = subprocess.run(args, cwd=folder, text=True, capture_output=True, timeout=90)
            (REPORT / (mode + "-compile.log")).write_text(compiled.stdout + compiled.stderr)
            if compiled.returncode:
                raise AssertionError(compiled.stdout + compiled.stderr)
            subprocess.run([str(binary), str(folder)], cwd=folder, check=True, timeout=10)
            cls.scripts[mode] = folder / "handoff.ps1"

    @classmethod
    def tearDownClass(cls):
        # Keep scripts and compiler proof; fixture payloads are tiny and diagnostic.
        pass

    def fixture(self, mode):
        folder = self.root / mode / self._testMethodName
        folder.mkdir()
        installed, source = folder / "installed", folder / "source"
        installed.mkdir(); source.mkdir()
        files = ["a.dll", "b.dll", "assets/new.txt", "installed.exe"]
        rows = []
        old = {}
        for relative in files:
            path = source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(("new:" + relative).encode())
            if relative != "assets/new.txt":
                target = installed / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(("old:" + relative).encode())
                old[relative] = target.read_bytes()
            rows.append(str(path) + "\t" + relative + "\t" + hashlib.sha256(path.read_bytes()).hexdigest())
        (installed / "user.settings").write_bytes(b"unrelated settings")
        plan = folder / "plan.tsv"
        plan.write_text("\n".join(rows) + "\n", encoding="utf-8")
        work = installed / ".epoch-update-test"
        env = dict(os.environ, EPOCH_UPDATE_ROOT=str(installed), EPOCH_UPDATE_WORK=str(work), EPOCH_UPDATE_PLAN=str(plan), EPOCH_UPDATE_PLAN_SHA256=hashlib.sha256(plan.read_bytes()).hexdigest(), LOG=str(folder / "handoff.log"))
        return installed, source, work, plan, old, env

    def command(self, mode, attempts=1):
        return ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", str(self.scripts[mode]), "-Attempts", str(attempts)]

    def run_mode(self, mode, env, operation, attempts=3):
        completed = subprocess.run(self.command(mode, attempts), env=dict(env, EPOCH_UPDATE_MODE=operation), capture_output=True, text=True, timeout=12)
        return completed.returncode

    def assert_old(self, installed, old):
        for relative, data in old.items():
            self.assertEqual((installed / relative).read_bytes(), data)
        self.assertFalse((installed / "assets/new.txt").exists())
        self.assertEqual((installed / "user.settings").read_bytes(), b"unrelated settings")

    def test_success_stages_entire_payload_then_commits_with_backups(self):
        for mode in self.scripts:
            with self.subTest(mode=mode):
                installed, source, work, plan, old, env = self.fixture(mode)
                self.assertEqual(self.run_mode(mode, env, "stage"), 0)
                self.assert_old(installed, old)
                self.assertEqual(self.run_mode(mode, env, "commit"), 0)
                for path in source.rglob("*"):
                    if path.is_file(): self.assertEqual((installed / path.relative_to(source)).read_bytes(), path.read_bytes())
                for relative, data in old.items(): self.assertEqual((work / "backup" / relative).read_bytes(), data)
                self.assertEqual((work / "state").read_text(), "complete")
                self.assertEqual((installed / "user.settings").read_bytes(), b"unrelated settings")

    def test_missing_second_file_preserves_every_installed_file(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            (source / "b.dll").unlink()
            self.assertNotEqual(self.run_mode(mode, env, "stage"), 0)
            self.assert_old(installed, old)

    def test_changed_source_is_not_accepted(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            (source / "b.dll").write_bytes(b"unadmitted bytes")
            self.assertNotEqual(self.run_mode(mode, env, "stage"), 0)
            self.assert_old(installed, old)

    def test_work_collision_is_not_overwritten(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            work.mkdir(); (work / "sentinel").write_bytes(b"existing")
            self.assertNotEqual(self.run_mode(mode, env, "stage"), 0)
            self.assertEqual((work / "sentinel").read_bytes(), b"existing")
            self.assert_old(installed, old)

    def test_corrupt_staged_support_file_prevents_all_mutation(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            self.assertEqual(self.run_mode(mode, env, "stage"), 0)
            (work / "staged/b.dll").write_bytes(b"corrupt")
            self.assertNotEqual(self.run_mode(mode, env, "commit"), 0)
            self.assert_old(installed, old)

    def test_second_dll_failure_rolls_back_first_dll_and_keeps_exe(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            self.assertEqual(self.run_mode(mode, env, "stage"), 0)
            with (installed / "b.dll").open("rb"):
                self.assertNotEqual(self.run_mode(mode, env, "commit", attempts=1), 0)
            self.assert_old(installed, old)
            self.assertEqual((work / "state").read_text(), "rolled_back")

    def test_executable_failure_rolls_back_all_support_and_new_files(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            self.assertEqual(self.run_mode(mode, env, "stage"), 0)
            with (installed / "installed.exe").open("rb"):
                self.assertNotEqual(self.run_mode(mode, env, "commit", attempts=1), 0)
            self.assert_old(installed, old)
            self.assertEqual((work / "state").read_text(), "rolled_back")

    def wait_changed(self, process, path, expected):
        import time
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            try:
                if path.read_bytes() == expected: return
            except (PermissionError, FileNotFoundError):
                pass
            if process.poll() is not None: raise AssertionError("Owned transaction exited before interruption point")
            time.sleep(.025)
        raise AssertionError("Owned transaction failed to reach interruption point")

    def test_interrupted_commit_recovers_from_journal(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            self.assertEqual(self.run_mode(mode, env, "stage"), 0)
            with (installed / "b.dll").open("rb"):
                process = subprocess.Popen(self.command(mode, attempts=60), env=dict(env, EPOCH_UPDATE_MODE="commit"), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                try:
                    self.wait_changed(process, installed / "a.dll", (source / "a.dll").read_bytes())
                finally:
                    process.kill(); process.wait(timeout=5)
            self.assertEqual((installed / "installed.exe").read_bytes(), old["installed.exe"])
            self.assertEqual(self.run_mode(mode, env, "recover"), 0)
            self.assert_old(installed, old)

    def test_interrupted_rollback_is_resumable(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            self.assertEqual(self.run_mode(mode, env, "stage"), 0)
            self.assertEqual(self.run_mode(mode, env, "commit"), 0)
            # An explicit recover can restore the retained prior version. Block a
            # later file and interrupt after the EXE was restored, then resume.
            with (installed / "a.dll").open("rb"):
                process = subprocess.Popen(self.command(mode, attempts=60), env=dict(env, EPOCH_UPDATE_MODE="recover"), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                try:
                    self.wait_changed(process, installed / "installed.exe", old["installed.exe"])
                finally:
                    process.kill(); process.wait(timeout=5)
            self.assertEqual(self.run_mode(mode, env, "recover"), 0)
            self.assert_old(installed, old)

    def test_traversal_manifest_is_refused(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            rows = plan.read_text().replace("\ta.dll\t", "\t../escaped.dll\t", 1)
            plan.write_text(rows)
            env["EPOCH_UPDATE_PLAN_SHA256"] = hashlib.sha256(plan.read_bytes()).hexdigest()
            self.assertNotEqual(self.run_mode(mode, env, "stage"), 0)
            self.assert_old(installed, old)
            self.assertFalse((installed.parent / "escaped.dll").exists())

    def test_changed_unrelated_target_is_not_overwritten_by_recovery(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            self.assertEqual(self.run_mode(mode, env, "stage"), 0)
            self.assertEqual(self.run_mode(mode, env, "commit"), 0)
            (installed / "b.dll").write_bytes(b"external changes")
            self.assertNotEqual(self.run_mode(mode, env, "recover"), 0)
            self.assertEqual((installed / "b.dll").read_bytes(), b"external changes")

    def test_repeated_commit_refuses_consumed_stage(self):
        for mode in self.scripts:
            installed, source, work, plan, old, env = self.fixture(mode)
            self.assertEqual(self.run_mode(mode, env, "stage"), 0)
            self.assertEqual(self.run_mode(mode, env, "commit"), 0)
            self.assertNotEqual(self.run_mode(mode, env, "commit"), 0)
            # A rejected replay must not silently roll a completed update back.
            self.assertEqual((installed / "installed.exe").read_bytes(), (source / "installed.exe").read_bytes())


@unittest.skipUnless(os.name == "nt", "Windows source update worker")
class WindowsSourceWorkerHandoff(unittest.TestCase):
    def test_cancel_after_staging_preserves_install_and_retry_recovers(self):
        REPORT.mkdir(parents=True, exist_ok=True)
        root = Path(tempfile.mkdtemp(prefix="source-worker-", dir=REPORT))
        updater = ROOT / "Engine/src/updater"
        worker = (updater / "updater.source_worker_win.cpp").read_text(encoding="utf-8")
        helper = (updater / "updater.handoff_script.hpp").read_text(encoding="utf-8")
        self.assertIn('<< source_runtime_handoff_script()', worker)
        self.assertNotIn('Remove-Item -LiteralPath $targetExe', worker)
        wrapper = re.search(r'source_runtime_handoff_script\(\).*?R"epoch\((.*?)\)epoch"', worker, re.S).group(1)
        transaction = re.search(r'windows = R"epoch\((.*?)\)epoch"', helper, re.S).group(1)
        installed = root / "installed"
        built = root / "built"
        repository = root / "repository-assets"
        for path in (installed, built / "assets", built / "lib", repository):
            path.mkdir(parents=True, exist_ok=True)
        for name in ("EpochEditor.exe", "engine.dll", "user.settings"):
            (installed / name).write_bytes(b"previous " + name.encode())
        for name in ("EpochEditor.exe", "engine.dll", "epoch-distribution.json", "assets/skin.dat", "lib/support.dat"):
            (built / name).write_bytes(b"next " + name.encode())
        (installed / "unchanged.dll").write_bytes(b"identical payload")
        (built / "unchanged.dll").write_bytes(b"identical payload")
        (repository / "skin.dat").write_bytes(b"repository asset takes precedence")
        script = root / "worker.ps1"
        script.write_text(r"""$ErrorActionPreference = 'Stop'
$targetDir = Join-Path $PSScriptRoot 'installed'
$targetExe = Join-Path $targetDir 'EpochEditor.exe'
$builtDir = Join-Path $PSScriptRoot 'built'
$builtExe = Join-Path $builtDir 'EpochEditor.exe'
$builtAssetsDir = Join-Path $builtDir 'assets'
$sourceRepoAssetsDir = Join-Path $PSScriptRoot 'repository-assets'
$runDir = Join-Path $PSScriptRoot 'run'
$handoffLog = Join-Path $runDir 'handoff.log'
[void][IO.Directory]::CreateDirectory($runDir)
function Write-Handoff($level, $message) {
    if ($env:EPOCH_FIXTURE_CANCEL -eq '1' -and $message -eq 'Waiting for runtime handoff.') { $script:cancelled = $true }
}
function Test-Cancel { if ($script:cancelled) { exit 130 } }
$transactionBody = @'
""" + transaction + "\n'@\n" + wrapper, encoding="utf-8")

        def run(cancel):
            modules = Path(os.environ["SystemRoot"]) / "System32/WindowsPowerShell/v1.0/Modules"
            result = subprocess.run(
                ["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", str(script)],
                env=dict(os.environ, PSModulePath=str(modules), EPOCH_FIXTURE_CANCEL=str(int(cancel))), cwd=root,
                capture_output=True, text=True, timeout=45, creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(result.returncode, 130 if cancel else 0, result.stdout + result.stderr)

        work = installed / ".epoch-source-handoff"
        run(cancel=True)
        self.assertEqual((work / "state").read_text(), "staged")
        self.assertEqual((installed / "EpochEditor.exe").read_bytes(), b"previous EpochEditor.exe")
        self.assertEqual((installed / "engine.dll").read_bytes(), b"previous engine.dll")
        self.assertFalse((installed / "assets").exists())

        # Model interruption after one installed file changed. The real transaction
        # tests above interrupt the writer; this tests the source worker's recovery entry.
        previous_dll = (installed / "engine.dll").read_bytes()
        next_dll = (built / "engine.dll").read_bytes()
        backup = work / "backup"
        backup.mkdir()
        (backup / "engine.dll").write_bytes(previous_dll)
        (work / "journal.tsv").write_text(
            "engine.dll\t" + hashlib.sha256(previous_dll).hexdigest() + "\t" + hashlib.sha256(next_dll).hexdigest() + "\n",
            encoding="utf-8")
        (work / "state").write_text("committing", encoding="utf-8")
        (installed / "engine.dll").write_bytes(next_dll)
        # A reader can keep identical bytes open while other payload files update.
        with (installed / "unchanged.dll").open("rb"):
            run(cancel=False)
        self.assertEqual((work / "state").read_text(), "complete")
        retained = list(installed.glob(".epoch-source-handoff-*"))
        self.assertEqual(len(retained), 1)
        self.assertEqual((retained[0] / "state").read_text(), "rolled_back")
        self.assertEqual((work / "backup/engine.dll").read_bytes(), previous_dll)
        self.assertFalse((work / "backup/unchanged.dll").exists())
        self.assertEqual((work / "backup/EpochEditor.exe").read_bytes(), b"previous EpochEditor.exe")
        self.assertEqual((installed / "EpochEditor.exe").read_bytes(), (built / "EpochEditor.exe").read_bytes())
        self.assertEqual((installed / "engine.dll").read_bytes(), (built / "engine.dll").read_bytes())
        self.assertEqual((installed / "assets/skin.dat").read_bytes(), (repository / "skin.dat").read_bytes())
        self.assertEqual((installed / "lib/support.dat").read_bytes(), (built / "lib/support.dat").read_bytes())
        self.assertEqual((installed / "user.settings").read_bytes(), b"previous user.settings")


if __name__ == "__main__":
    unittest.main()
