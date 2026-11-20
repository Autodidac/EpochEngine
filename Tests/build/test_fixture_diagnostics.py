"""Verify CPU fixture CRT failures are routed to captured stderr, not dialogs."""
import json
import os
from pathlib import Path
import tempfile
import unittest

from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == "nt", "Windows CRT policy")
class FixtureDiagnostics(unittest.TestCase):
    def test_actual_debug_and_release_report_policy(self):
        parent = ROOT / "Artifacts/Reports/fixture-diagnostics"
        parent.mkdir(parents=True, exist_ok=True)
        report = Path(tempfile.mkdtemp(prefix="contract-", dir=parent))
        source = report / "contract.cpp"
        source.write_text(r'''
#include <cstdlib>
#include <crtdbg.h>
int main() {
    if (_set_error_mode(_REPORT_ERRMODE) != _OUT_TO_STDERR) return 11;
    const auto behavior = _set_abort_behavior(0, 0);
    if (behavior & (_WRITE_ABORT_MSG | _CALL_REPORTFAULT)) return 12;
#ifdef _DEBUG
    for (const int kind : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        if (_CrtSetReportMode(kind, _CRTDBG_REPORT_MODE) != _CRTDBG_MODE_FILE)
            return 13;
    }
#endif
    return 0;
}
''', encoding="utf-8")
        records = compile_and_run(self, source, report)
        (report / "result.json").write_text(json.dumps(records, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
