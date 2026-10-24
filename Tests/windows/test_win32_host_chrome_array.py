"""MSVC C2079 guard: Windows host uses std::array with an explicit header."""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
HOST = read_owned_source(ROOT / "Engine/src/renderers/host/host.context_win.cpp", encoding="utf-8-sig")

class WindowsChromeArrayContract(unittest.TestCase):
    def test_win32_host_explicitly_includes_array(self):
        win32_imports = HOST.split("#if defined(_WIN32)", 1)[1].split("#endif", 1)[0]
        self.assertRegex(win32_imports, r"(?m)^#\s*include\s*<array>\s*$")
        self.assertLess(HOST.index("#   include <array>"), HOST.index("std::array<HBRUSH"))

    def test_brushes_match_all_eight_existing_theme_ids(self):
        self.assertIn("std::array<HBRUSH, 8u> result{}", HOST)
        self.assertIn("std::array<COLORREF, 8u> fallback{", HOST)
        self.assertIn("index < brushes.size()", HOST)
        self.assertIn("native_window_chrome_palette()", HOST)

if __name__ == "__main__":
    unittest.main()
