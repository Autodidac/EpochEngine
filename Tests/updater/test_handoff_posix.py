"""Execute the emitted production POSIX payload transaction, without graphics."""
from pathlib import Path
import argparse
import hashlib
import os
import signal
import subprocess
import tempfile
import time
import unittest

SCRIPT = None
OUTPUT = None


class PayloadHandoff(unittest.TestCase):
    def setUp(self):
        if os.name != "posix" or SCRIPT is None:
            self.skipTest("Explicit emitted POSIX helper required")
        self.folder = Path(tempfile.mkdtemp(prefix=self._testMethodName + "-", dir=OUTPUT))
        self.root, self.source = self.folder / "installed", self.folder / "source"
        self.root.mkdir(); self.source.mkdir()
        self.work = self.root / ".epoch-update-test"
        self.old = {}
        rows = []
        for relative in ["a.so", "b.so", "assets/new.txt", "installed"]:
            path = self.source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(("new:" + relative).encode())
            if relative != "assets/new.txt":
                target = self.root / relative
                target.write_bytes(("old:" + relative).encode())
                self.old[relative] = target.read_bytes()
            rows.append(str(path) + "\t" + relative + "\t" + hashlib.sha256(path.read_bytes()).hexdigest())
        (self.root / "user.settings").write_bytes(b"unrelated")
        self.plan = self.folder / "plan.tsv"
        self.plan.write_text("\n".join(rows) + "\n")
        self.env = dict(os.environ, EPOCH_UPDATE_ROOT=str(self.root), EPOCH_UPDATE_WORK=str(self.work), EPOCH_UPDATE_PLAN=str(self.plan), EPOCH_UPDATE_PLAN_SHA256=hashlib.sha256(self.plan.read_bytes()).hexdigest())

    def run_mode(self, mode):
        result = subprocess.run(["/bin/sh", str(SCRIPT)], env=dict(self.env, EPOCH_UPDATE_MODE=mode), capture_output=True, text=True, timeout=10)
        (self.folder / (mode + ".log")).write_text(result.stdout + result.stderr)
        return result.returncode

    def assert_old(self):
        for relative, value in self.old.items(): self.assertEqual((self.root / relative).read_bytes(), value)
        self.assertFalse((self.root / "assets/new.txt").exists())
        self.assertEqual((self.root / "user.settings").read_bytes(), b"unrelated")

    def block_move(self, relative, pause=False):
        wrappers = self.folder / "commands"
        wrappers.mkdir()
        script = wrappers / "mv"
        # Inject failure only for the owned staged install, or a recovery copy.
        script.write_text("#!/bin/sh\ncase \"$*\" in\n  *staged/" + relative + "*) " + ("sleep 30; exit 1" if pause else "exit 1") + ";;\nesac\nexec /bin/mv \"$@\"\n")
        script.chmod(0o755)
        self.env["PATH"] = str(wrappers) + os.pathsep + os.environ["PATH"]
        return script

    def test_success_and_prior_version_backups(self):
        self.assertEqual(self.run_mode("stage"), 0)
        self.assert_old()
        self.assertEqual(self.run_mode("commit"), 0)
        for p in self.source.rglob("*"):
            if p.is_file(): self.assertEqual((self.root / p.relative_to(self.source)).read_bytes(), p.read_bytes())
        for relative, value in self.old.items(): self.assertEqual((self.work / "backup" / relative).read_bytes(), value)
        self.assertEqual((self.work / "state").read_text(), "complete")

    def test_second_support_copy_missing_leaves_installed_intact(self):
        (self.source / "b.so").unlink()
        self.assertNotEqual(self.run_mode("stage"), 0)
        self.assert_old()

    def test_admitted_source_change_refused(self):
        (self.source / "b.so").write_bytes(b"changed")
        self.assertNotEqual(self.run_mode("stage"), 0)
        self.assert_old()

    def test_corrupt_stage_refused_before_mutation(self):
        self.assertEqual(self.run_mode("stage"), 0)
        (self.work / "staged/b.so").write_bytes(b"corrupt")
        self.assertNotEqual(self.run_mode("commit"), 0)
        self.assert_old()

    def test_second_install_failure_rolls_back_first(self):
        self.assertEqual(self.run_mode("stage"), 0)
        self.block_move("b.so")
        self.assertNotEqual(self.run_mode("commit"), 0)
        self.assert_old()
        self.assertEqual((self.work / "state").read_text(), "rolled_back")

    def test_exe_failure_rolls_back_support_and_new_asset(self):
        self.assertEqual(self.run_mode("stage"), 0)
        self.block_move("installed")
        self.assertNotEqual(self.run_mode("commit"), 0)
        self.assert_old()

    def wait_changed(self, process, path, value):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            if path.read_bytes() == value: return
            if process.poll() is not None: raise AssertionError("Owned helper exited before interruption point")
            time.sleep(.025)
        raise AssertionError("Owned helper did not reach interruption point")

    def test_interrupted_commit_is_recoverable(self):
        self.assertEqual(self.run_mode("stage"), 0)
        self.block_move("b.so", pause=True)
        process = subprocess.Popen(["/bin/sh", str(SCRIPT)], env=dict(self.env, EPOCH_UPDATE_MODE="commit"), start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            self.wait_changed(process, self.root / "a.so", (self.source / "a.so").read_bytes())
        finally:
            os.killpg(process.pid, signal.SIGKILL); process.wait(timeout=5)
        self.assertEqual(self.run_mode("recover"), 0)
        self.assert_old()

    def test_interrupted_recovery_can_resume(self):
        self.assertEqual(self.run_mode("stage"), 0)
        self.assertEqual(self.run_mode("commit"), 0)
        wrappers = self.folder / "commands"
        wrappers.mkdir()
        script = wrappers / "mv"
        script.write_text("#!/bin/sh\ncase \"$*\" in\n  *.epoch-recover-*a.so*) sleep 30; exit 1;;\nesac\nexec /bin/mv \"$@\"\n")
        script.chmod(0o755)
        self.env["PATH"] = str(wrappers) + os.pathsep + os.environ["PATH"]
        process = subprocess.Popen(["/bin/sh", str(SCRIPT)], env=dict(self.env, EPOCH_UPDATE_MODE="recover"), start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            self.wait_changed(process, self.root / "installed", self.old["installed"])
        finally:
            os.killpg(process.pid, signal.SIGKILL); process.wait(timeout=5)
        self.env["PATH"] = os.environ["PATH"]
        self.assertEqual(self.run_mode("recover"), 0)
        self.assert_old()

    def test_repeated_commit_does_not_revert_success(self):
        self.assertEqual(self.run_mode("stage"), 0)
        self.assertEqual(self.run_mode("commit"), 0)
        self.assertNotEqual(self.run_mode("commit"), 0)
        self.assertEqual((self.root / "installed").read_bytes(), (self.source / "installed").read_bytes())

    def test_traversal_is_refused(self):
        self.plan.write_text(self.plan.read_text().replace("\ta.so\t", "\t../escaped\t", 1))
        self.env["EPOCH_UPDATE_PLAN_SHA256"] = hashlib.sha256(self.plan.read_bytes()).hexdigest()
        self.assertNotEqual(self.run_mode("stage"), 0)
        self.assert_old()
        self.assertFalse((self.root.parent / "escaped").exists())

    def test_unrelated_changes_not_overwritten_by_recovery(self):
        self.assertEqual(self.run_mode("stage"), 0)
        self.assertEqual(self.run_mode("commit"), 0)
        (self.root / "b.so").write_bytes(b"external")
        self.assertNotEqual(self.run_mode("recover"), 0)
        self.assertEqual((self.root / "b.so").read_bytes(), b"external")

    def test_symlink_destination_is_refused(self):
        other = self.folder / "unrelated"
        other.write_bytes(b"unrelated")
        (self.root / "b.so").unlink()
        (self.root / "b.so").symlink_to(other)
        self.assertNotEqual(self.run_mode("stage"), 0)
        self.assertEqual(other.read_bytes(), b"unrelated")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--script", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    SCRIPT, OUTPUT = args.script, args.output
    OUTPUT.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[__file__, "-v"])
