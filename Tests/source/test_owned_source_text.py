from pathlib import Path
import shutil
import tempfile
import unittest

from Tools.source.owned_source_text import read_owned_source

ROOT = Path(__file__).resolve().parents[2]


class OwnedSourceText(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'build/contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        self.scratch = Path(tempfile.mkdtemp(prefix='owned-source-', dir=parent))
        self.source = self.scratch / 'Engine/src/domain'
        self.source.mkdir(parents=True)

    def tearDown(self):
        self.assertTrue(self.scratch.resolve().is_relative_to((ROOT / 'build/contract-tests').resolve()))
        shutil.rmtree(self.scratch)

    def write(self, name, value):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value, encoding='utf-8')
        return path

    def test_nested_parts_preserve_exact_body_order_and_unicode(self):
        main = self.write('owner.document.cpp', 'before\n#include "parts/owner.history.inl"\nafter\n')
        self.write('parts/owner.history.inl', 'history α\n#include "owner.undo.inl"\n')
        self.write('parts/owner.undo.inl', 'undo\n')
        self.assertEqual(read_owned_source(main), 'before\nhistory α\nundo\nafter\n')

    def test_normal_includes_are_not_followed(self):
        main = self.write('owner.document.cpp', '#include <system.h>\n#include "owner.detail.hpp"\n')
        self.assertEqual(read_owned_source(main), main.read_text())

    def test_cycle_is_rejected(self):
        main = self.write('owner.document.cpp', '#include "owner.loop.inl"\n')
        self.write('owner.loop.inl', '#include "owner.loop.inl"\n')
        with self.assertRaisesRegex(ValueError, 'Cyclic'):
            read_owned_source(main)

    def test_missing_part_is_rejected(self):
        main = self.write('owner.document.cpp', '#include "owner.missing.inl"\n')
        with self.assertRaises(FileNotFoundError):
            read_owned_source(main)

    def test_escape_is_rejected_before_reading_other_source(self):
        main = self.write('owner.document.cpp', '#include "../../outside.inl"\n')
        (self.source.parent.parent / 'outside.inl').write_text('other source')
        with self.assertRaisesRegex(ValueError, 'escapes'):
            read_owned_source(main)

    def test_expansion_budgets_are_enforced(self):
        main = self.write('owner.document.cpp', '#include "owner.part.inl"\n')
        self.write('owner.part.inl', 'part\n')
        with self.assertRaisesRegex(ValueError, 'budget'):
            read_owned_source(main, maximum_files=1)
        with self.assertRaisesRegex(ValueError, 'budget'):
            read_owned_source(main, maximum_bytes=1)

    def test_nonimplementation_file_is_read_directly(self):
        path = self.scratch / 'note.txt'
        path.write_text('note\n')
        self.assertEqual(read_owned_source(path), 'note\n')
