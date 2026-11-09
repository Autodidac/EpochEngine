"""Exercise production SDK-to-host input forwarding on two real X11 clients.

This uses no renderer/GPU. Run explicitly under xvfb-run on Linux; ordinary
renderer-free suites skip it when no X display is available.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class X11SdkInput(unittest.TestCase):
    @unittest.skipUnless(os.name == "posix" and os.environ.get("DISPLAY")
                         and os.environ.get("EPOCH_X11_PROTOCOL_TESTS") == "1",
                         "Explicit Linux X11 protocol lane")
    def test_actual_forwarding_and_shared_owner_event_order(self):
        production = (ROOT / "Engine/src/renderers/host/host.context_linux.cpp").read_text(encoding="utf-8")
        forward = "void QueueX11PointerInput(" + production.split("void QueueX11PointerInput(", 1)[1].split(
            "    void RequestActiveParentLayout()", 1)[0]
        route = production.split('const Atom pointerInput = XInternAtom', 1)[1].split(
            '                const Atom wmDelete', 1)[0]
        route = 'const Atom pointerInput = XInternAtom' + route
        focus = production.split('            case FocusOut:\n            {', 1)[1].split(
            '                break;', 1)[0]
        mask = 'swa.event_mask =' + production.split('swa.event_mask =', 1)[1].split(
            '                    ::Window win =', 1)[0]
        fixture = (ROOT / "Tests/platform/platform.x11_sdk_input_contract.cpp").read_text(encoding="utf-8")
        fixture = fixture.replace("FORWARD_POINTER", forward).replace("ROUTE_POINTER", route).replace("FOCUS_LOSS", focus).replace("MANAGER_MASK", mask)
        compiler = shutil.which(os.environ.get("CXX", "clang++"))
        self.assertIsNotNone(compiler, "Use the existing managed Clang toolchain")
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent, prefix="x11-sdk-input-") as scratch:
            scratch = Path(scratch)
            source = scratch / "contract.cpp"
            source.write_text(fixture, encoding="utf-8")
            binary = scratch / "contract"
            compiled = subprocess.run([compiler, "-std=c++23", "-Wall", "-Wextra", "-Werror",
                str(source), "-lX11", "-pthread", "-o", str(binary)], cwd=scratch,
                capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            executed = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            lines = fixture.splitlines()
            failure = lines[executed.returncode - 1] if 0 < executed.returncode <= len(lines) else ""
            self.assertEqual(executed.returncode, 0, failure + "\n" + executed.stdout + executed.stderr)
            # Restoring the former host mask must reproduce the real X11 error.
            previous = fixture.replace(
                'swa.event_mask &= ~(ButtonPressMask | ButtonReleaseMask);', ';', 1)
            self.assertNotEqual(previous, fixture)
            source.write_text(previous, encoding="utf-8")
            compiled = subprocess.run([compiler, "-std=c++23", "-Wall", "-Wextra", "-Werror",
                str(source), "-lX11", "-pthread", "-o", str(binary)], cwd=scratch,
                capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            executed = subprocess.run([str(binary)], cwd=scratch, capture_output=True, text=True, timeout=10)
            self.assertNotEqual(executed.returncode, 0)
            self.assertIn("BadAccess", executed.stderr)


if __name__ == "__main__":
    unittest.main()
