"""Run the actual cleanup command against tiny checkout-local fake repositories."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Tools/build/clean_epoch_intermediates.ps1"
FIXTURES = ROOT / "Artifacts/Reports/output-layout-oct27/cleanup-fixtures"


@unittest.skipUnless(os.name == "nt", "Windows PowerShell cleanup owner")
class IntermediateCleanup(unittest.TestCase):
    def setUp(self):
        FIXTURES.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="repo-", dir=FIXTURES)
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name)
        self.script = self.repo / "Tools/build/clean_epoch_intermediates.ps1"
        self.script.parent.mkdir(parents=True)
        shutil.copyfile(SCRIPT, self.script)
        self.run_command(["git", "init", "--quiet", str(self.repo)])
        for bucket in ("Artifacts/Runtime", "Artifacts/Install", "Projects"):
            path = self.repo / bucket / "keep.txt"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(bucket, encoding="utf-8")

    def run_command(self, command):
        return subprocess.run(command, text=True, capture_output=True, check=True,
                              timeout=30)

    def cleaner(self, *arguments, active_compiler=False):
        # Process inventory is independent of the generated checkout. Make it
        # deterministic while exercising the real script and its safety checks.
        process = "[pscustomobject]@{ ProcessName = 'MSBuild' }" if active_compiler else ""
        quoted_script = str(self.script).replace("'", "''")
        command = ("function Get-Process { [CmdletBinding()] param() " + process
                   + " }; & '" + quoted_script + "' " + " ".join(arguments))
        return subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy",
                               "Bypass", "-Command", command],
                              text=True, capture_output=True, timeout=30)

    def intermediate(self, root="Artifacts/Intermediate"):
        path = self.repo / root / "compiler.obj"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"disposable compiler fixture\n")
        return path

    def preserved(self):
        for bucket in ("Artifacts/Runtime", "Artifacts/Install", "Projects"):
            self.assertEqual((self.repo / bucket / "keep.txt").read_text(), bucket)

    def receipts(self):
        return [json.loads(path.read_text(encoding="utf-8-sig")) for path in
                (self.repo / "Artifacts/Reports/cleanup").glob("*.json")]

    def junction(self, link, target):
        # Both endpoints are inside this isolated fixture, never another checkout.
        quoted_link = str(link).replace("'", "''")
        quoted_target = str(target).replace("'", "''")
        self.run_command(["powershell.exe", "-NoProfile", "-Command",
                          "New-Item -ItemType Junction -Path '" + quoted_link +
                          "' -Target '" + quoted_target + "' | Out-Null"])

    def test_preview_then_remove_preserves_runnable_products_and_projects(self):
        path = self.intermediate()
        preview = self.cleaner()
        self.assertEqual(preview.returncode, 0, preview.stdout + preview.stderr)
        self.assertTrue(path.exists())
        self.assertEqual(self.receipts()[-1]["mode"], "preview")
        self.assertFalse(self.receipts()[-1]["removed"])
        removed = self.cleaner("-Remove")
        self.assertEqual(removed.returncode, 0, removed.stdout + removed.stderr)
        self.assertFalse(path.parent.exists())
        self.assertTrue(any(row["removed"] for row in self.receipts()))
        self.preserved()

    def test_legacy_is_explicit_and_never_selects_candidate_trees(self):
        legacy = self.intermediate("build/obj")
        current = self.intermediate()
        candidate = self.intermediate("ai/session/build/obj")
        result = self.cleaner("-LegacyMSVC", "-Remove")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(legacy.parent.exists())
        self.assertTrue(current.exists())
        self.assertTrue(candidate.exists())
        self.preserved()

    def test_tracked_intermediate_is_refused(self):
        path = self.intermediate()
        self.run_command(["git", "-C", str(self.repo), "add", "--",
                          "Artifacts/Intermediate/compiler.obj"])
        result = self.cleaner("-Remove")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("untracked", result.stderr)
        self.assertTrue(path.exists())
        self.preserved()

    def test_active_compiler_refuses_removal(self):
        path = self.intermediate()
        result = self.cleaner("-Remove", active_compiler=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Finish active compiler/build processes", result.stderr)
        self.assertTrue(path.exists())
        self.assertFalse(self.receipts())
        self.preserved()

    def test_redirected_ancestor_is_refused(self):
        shutil.rmtree(self.repo / "Artifacts")
        elsewhere = self.repo / "Elsewhere"
        path = elsewhere / "Intermediate/compiler.obj"
        path.parent.mkdir(parents=True)
        path.write_bytes(b"must survive")
        self.junction(self.repo / "Artifacts", elsewhere)
        result = self.cleaner("-Remove")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("redirected", result.stderr)
        self.assertEqual(path.read_bytes(), b"must survive")
        self.assertTrue((self.repo / "Projects/keep.txt").exists())

    def test_redirected_leaf_is_refused(self):
        path = self.intermediate()
        self.junction(path.parent / "runtime-link", self.repo / "Artifacts/Runtime")
        result = self.cleaner("-Remove")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("redirected", result.stderr)
        self.assertTrue(path.exists())
        self.preserved()


if __name__ == "__main__":
    unittest.main()
