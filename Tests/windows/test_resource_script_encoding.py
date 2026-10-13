#!/usr/bin/env python3
"""Guard Windows resource scripts from UTF-8 BOM corruption.

Run from any working directory:
    python Tests/windows/test_resource_script_encoding.py
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
RESOURCE = ROOT / "Engine" / "resource"
VERSION_MODULE = ROOT / "Engine" / "modules" / "epoch.version.ixx"


class WindowsResourceContract(unittest.TestCase):
    def test_all_resource_scripts_are_ascii_without_bom(self):
        scripts = sorted(RESOURCE.rglob("*.rc"))
        self.assertTrue(scripts)
        for file in scripts:
            with self.subTest(file=file.relative_to(ROOT)):
                raw = file.read_bytes()
                self.assertFalse(raw.startswith(b"\xef\xbb\xbf"), "RC cannot reliably read UTF-8 BOM")
                self.assertFalse(raw.startswith((b"\xff\xfe", b"\xfe\xff")), "The resource scripts are standardized on ASCII")
                self.assertTrue(raw.isascii(), "Use ASCII, or consciously migrate to UTF-16 with testing")

    def test_editor_icon_and_resource_references_exist(self):
        script = (RESOURCE / "WinResource.rc").read_text(encoding="ascii")
        self.assertRegex(script, r'IDI_EPOCH_EDITOR\s+ICON\s+"icon\.ico"')
        self.assertTrue((RESOURCE / "icon.ico").is_file())
        self.assertTrue((RESOURCE / "resource.h").is_file())
        self.assertTrue((RESOURCE / "icon.ico").read_bytes().startswith(b"\x00\x00\x01\x00"))
        self.assertIn("STRINGTABLE", script)
        self.assertIn("VS_VERSION_INFO VERSIONINFO", script)

    def test_resource_version_matches_source_version(self):
        source = VERSION_MODULE.read_text(encoding="utf-8")
        def value(name):
            match = re.search(r'^#\s*define\s+' + re.escape(name) + r'\s+(\d+)\s*$', source, re.M)
            self.assertIsNotNone(match, name)
            return int(match.group(1))

        major = value("EPOCH_VERSION_MAJOR_VALUE")
        minor = value("EPOCH_VERSION_MINOR_VALUE")
        revision = value("EPOCH_VERSION_REVISION_VALUE")
        script = (RESOURCE / "WinResource.rc").read_text(encoding="ascii")
        self.assertIn(f"FILEVERSION {major},{minor},{revision},0", script)
        self.assertIn(f"PRODUCTVERSION {major},{minor},{revision},0", script)
        version = f"{major}.{minor}.{revision:02d}"
        self.assertIn(f'VALUE "FileVersion", "{version}"', script)
        self.assertIn(f'VALUE "ProductVersion", "{version}"', script)


if __name__ == "__main__":
    unittest.main(verbosity=2)
