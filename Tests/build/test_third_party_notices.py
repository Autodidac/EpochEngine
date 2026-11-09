"""Execute the actual notice collector; preserve license and NOTICE bytes separately."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import struct
import subprocess
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Tools/build/collect_third_party_notices.ps1"
LIBRARIES = ("EpochGui", "EpochPlatformEngine", "EpochAudioEngine", "EpochMediaEngine")


class ThirdPartyNotices(unittest.TestCase):
    def setUp(self):
        if os.name != "nt":
            self.skipTest("Actual Windows PowerShell release helper")
        self.proof = ROOT / "Artifacts/Reports/notice-packaging-oct28" / str(time.time_ns())
        self.repo, self.installed, self.output = (self.proof / name for name in ("repository", "installed", "payload"))
        self.proof.mkdir(parents=True)
        self.assertTrue(self.output.resolve().is_relative_to((ROOT / "Artifacts/Reports").resolve()))
        sources = ["Engine/third_party/glad/LICENSE", "Engine/assets/fonts/OFL.txt"]
        sources += [f"Engine/dep/{name}/{leaf}" for name in LIBRARIES for leaf in ("LICENSE", "NOTICE")]
        self.source_bytes = {}
        for source in sources:
            data = (ROOT / source).read_bytes()
            destination = self.repo / source
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            self.source_bytes[source] = data
        for name, data in (("fixture-permissive", b"MIT fixture\nKeep this notice.\n"), ("fixture-empty", b"")):
            folder = self.installed / "share" / name
            folder.mkdir(parents=True)
            (folder / "copyright").write_bytes(data)
            (folder / "vcpkg.spdx.json").write_text(json.dumps({"packages": [{
                "SPDXID": "SPDXRef-port", "name": name, "versionInfo": "1.2.3",
                "licenseConcluded": "MIT", "downloadLocation": "https://example.invalid/license-fixture"}]}), encoding="utf-8")

    def run_collector(self):
        command = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(OWNER),
                   "-RepoRoot", str(self.repo), "-VcpkgInstalledRoot", str(self.installed), "-Destination", str(self.output)]
        # A Python child of PowerShell 7 otherwise passes that host's module
        # directories to Windows PowerShell 5, shadowing its Utility module.
        # Select the Windows host's standard modules for this isolated child.
        environment = os.environ.copy()
        environment["PSModulePath"] = str(Path(environment["WINDIR"]) / "System32/WindowsPowerShell/v1.0/Modules")
        result = subprocess.run(command, cwd=ROOT, env=environment,
                                capture_output=True, text=True, timeout=30)
        (self.proof / "collector.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        return result

    def test_all_compiled_libraries_preserve_exact_license_and_notice(self):
        result = self.run_collector()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        manifest = json.loads((self.output / "THIRD_PARTY_COMPONENTS.json").read_text(encoding="utf-8"))
        by_name = {entry["name"]: entry for entry in manifest}
        self.assertEqual(set(by_name), {*LIBRARIES, "Epoch bundled GLAD loader", "Roboto", "fixture-permissive", "fixture-empty"})
        combined = (self.output / "THIRD_PARTY_NOTICES.txt").read_text(encoding="utf-8")
        for name in LIBRARIES:
            entry = by_name[name]
            self.assertEqual(entry["license"], "MPL-2.0")
            self.assertEqual(entry["source"], f"https://github.com/Autodidac/{name}")
            for source_leaf, target_field, hash_field in (("LICENSE", "notice", "sha256"),
                                                          ("NOTICE", "attribution_notice", "attribution_sha256")):
                original = self.source_bytes[f"Engine/dep/{name}/{source_leaf}"]
                preserved = self.output / entry[target_field]
                self.assertEqual(preserved.read_bytes(), original)
                self.assertEqual(entry[hash_field], hashlib.sha256(original).hexdigest())
                self.assertIn(original.decode("utf-8").replace("\r\n", "\n").strip(), combined)
        self.assertEqual((self.output / by_name["Roboto"]["notice"]).read_bytes(), self.source_bytes["Engine/assets/fonts/OFL.txt"])
        self.assertEqual((self.output / by_name["Epoch bundled GLAD loader"]["notice"]).read_bytes(), self.source_bytes["Engine/third_party/glad/LICENSE"])
        self.assertEqual((self.output / by_name["fixture-empty"]["notice"]).read_bytes(), b"")
        self.assertIn("empty copyright file", combined)
        # A second collection produces the same inventory/hashes and does not
        # remove unrelated payload files outside its owned licenses directory.
        first = (self.output / "THIRD_PARTY_COMPONENTS.json").read_bytes()
        (self.output / "unrelated.txt").write_text("keep", encoding="utf-8")
        self.assertEqual(self.run_collector().returncode, 0)
        self.assertEqual((self.output / "THIRD_PARTY_COMPONENTS.json").read_bytes(), first)
        self.assertEqual((self.output / "unrelated.txt").read_text(encoding="utf-8"), "keep")
        (self.proof / "result.json").write_text(json.dumps({
            "passed": True, "scope": "Actual PowerShell collector over isolated installed-port and real repository license/NOTICE bytes",
            "ownerSha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(),
            "components": manifest, "nativeAcceptance": False}, indent=2) + "\n", encoding="utf-8")

    def test_missing_library_notice_refuses_collection(self):
        (self.repo / "Engine/dep/EpochMediaEngine/NOTICE").unlink()
        result = self.run_collector()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("EpochMediaEngine attribution notice", result.stdout + result.stderr)
        self.assertFalse((self.output / "THIRD_PARTY_COMPONENTS.json").exists())

    def test_bundled_font_metadata_matches_exact_official_release_notice(self):
        font = (ROOT / "Engine/assets/fonts/Roboto-Regular.ttf").read_bytes()
        names = {}
        for index in range(struct.unpack_from(">H", font, 4)[0]):
            tag, _, offset, _ = struct.unpack_from(">4sIII", font, 12 + 16 * index)
            if tag != b"name":
                continue
            _, count, strings = struct.unpack_from(">HHH", font, offset)
            for record in range(count):
                platform, _, _, name, length, position = struct.unpack_from(">HHHHHH", font, offset + 6 + 12 * record)
                raw = font[offset + strings + position:offset + strings + position + length]
                text = raw.decode("utf-16-be" if platform in (0, 3) else "mac_roman", errors="strict")
                names.setdefault(name, set()).add(text)
        notice = (ROOT / "Engine/assets/fonts/OFL.txt").read_bytes()
        self.assertIn(notice.decode("utf-8").splitlines()[0], names[0])
        self.assertEqual(names[1], {"Roboto"})
        self.assertEqual(names[5], {"Version 3.009; 2024"})
        self.assertTrue(any("SIL Open Font License, Version 1.1" in value for value in names[13]))
        # Exact unmodified upstream OFL blob at official Roboto v3.009 commit
        # cc4324ac5e7632b8f5521ed2ad470dc681e249ac, fetched 2026-10-28.
        self.assertEqual(hashlib.sha1(b"blob " + str(len(notice)).encode() + b"\0" + notice).hexdigest(),
                         "9c48e05a24fa2267e207ecf17560291b5621c197")


if __name__ == "__main__":
    unittest.main()
