from pathlib import Path
import json
import shutil
import tempfile
import unittest

from Tools.source.validate_source_size import audit

ROOT = Path(__file__).resolve().parents[2]


class SourceSize(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(prefix='source-size-', dir=parent))

    def tearDown(self):
        self.assertTrue(self.root.resolve().is_relative_to((ROOT / 'build/contract-tests').resolve()))
        shutil.rmtree(self.root)

    def write(self, relative, lines):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('line\n' * lines, encoding='utf-8')
        return path

    def test_exact_limit_passes_and_one_extra_line_fails(self):
        self.write('Engine/src/domain/domain.exact.cpp', 4000)
        self.write('Engine/src/domain/domain.large.inl', 4001)
        checked, violations = audit(self.root)
        self.assertEqual(checked, 2)
        self.assertEqual([(v.path, v.lines) for v in violations],
                         [('Engine/src/domain/domain.large.inl', 4001)])

    def test_generated_and_third_party_outputs_are_excluded(self):
        for path in ('Engine/src/third_party/large.hpp', 'Engine/src/stb/large.h',
                     'build/generated/large.cpp', 'Engine/examples/build/large.cpp'):
            self.write(path, 4001)
        self.assertEqual(audit(self.root), (0, []))

    def test_owned_dependency_is_checked_but_exact_mirror_is_not(self):
        self.write('Engine/dep/Owned/src/large.cpp', 4001)
        self.write('Engine/dep/Mirror/src/large.cpp', 4001)
        manifest = self.root / 'Engine/dep/dependencies.json'
        manifest.write_text(json.dumps({'dependencies': [
            {'path': 'Engine/dep/Owned', 'mode': 'engine_owned_gui_fork'},
            {'path': 'Engine/dep/Mirror', 'mode': 'exact_mirror'}]}))
        checked, violations = audit(self.root)
        self.assertEqual(checked, 1)
        self.assertEqual(violations[0].path, 'Engine/dep/Owned/src/large.cpp')

    def test_production_build_folders_are_not_generated_exclusions(self):
        self.write('Engine/src/build/build.feature_probe.hpp', 4001)
        self.write('Tools/build/epoch.bootstrapper.cpp', 4001)
        checked, violations = audit(self.root)
        self.assertEqual(checked, 2)
        self.assertEqual([row.path for row in violations], [
            'Engine/src/build/build.feature_probe.hpp',
            'Tools/build/epoch.bootstrapper.cpp'])

    def test_repository_has_no_oversized_first_party_source(self):
        checked, violations = audit(ROOT)
        self.assertGreater(checked, 500)
        self.assertEqual(violations, [], '\n'.join(f'{v.path}: {v.lines} {v.reason}' for v in violations))
