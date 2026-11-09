from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock
import Tools.source.owned_source_text as owned

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

    def test_discovery_snapshot_reuses_one_expansion_then_validates(self):
        main = self.write('owner.snapshot.cpp', '#include "owner.snapshot.inl"\n')
        part = self.write('owner.snapshot.inl', 'part α\n')
        part.write_bytes('part α\r\n'.encode('utf-8'))
        original = Path.read_bytes
        with mock.patch.object(Path, 'read_bytes', autospec=True, side_effect=original) as reads:
            with owned.discovery_source_snapshot(self.scratch/'Engine'):
                self.assertEqual(read_owned_source(main), 'part α\n')
                self.assertEqual(read_owned_source(main), 'part α\n')
                self.assertEqual(reads.call_count, 2)
            self.assertEqual(reads.call_count, 4)

    def test_discovery_snapshot_refuses_same_length_changed_include(self):
        main = self.write('owner.snapshot.cpp', '#include "owner.snapshot.inl"\n')
        part = self.write('owner.snapshot.inl', 'before\n')
        with self.assertRaisesRegex(RuntimeError, 'Source changed during Python discovery'):
            with owned.discovery_source_snapshot(self.scratch/'Engine'):
                self.assertEqual(read_owned_source(main), 'before\n')
                part.write_text('after!\n', encoding='utf-8')
                self.assertEqual(read_owned_source(main), 'before\n')
        self.assertEqual(read_owned_source(main), 'after!\n')

    def test_discovery_snapshot_refuses_missing_include(self):
        main = self.write('owner.snapshot.cpp', '#include "owner.snapshot.inl"\n')
        part = self.write('owner.snapshot.inl', 'part\n')
        with self.assertRaisesRegex(RuntimeError, 'Source unavailable after Python discovery'):
            with owned.discovery_source_snapshot(self.scratch/'Engine'):
                read_owned_source(main)
                part.unlink()
        with self.assertRaises(FileNotFoundError):
            read_owned_source(main)

    def test_discovery_snapshot_refuses_changed_root(self):
        main = self.write('owner.snapshot.cpp', 'before\n')
        with self.assertRaisesRegex(RuntimeError, 'Source changed during Python discovery'):
            with owned.discovery_source_snapshot(self.scratch/'Engine'):
                read_owned_source(main)
                main.write_text('after!\n', encoding='utf-8')

    def test_discovery_snapshot_refuses_redirected_dependency_binding(self):
        main = self.write('owner.snapshot.cpp', '#include "owner.snapshot.inl"\n')
        part = self.write('owner.snapshot.inl', 'same\n').resolve()
        other = self.write('owner.other.inl', 'same\n').resolve()
        resolve = Path.resolve
        redirected = False
        def redirect(path, *args, **kwargs):
            return other if redirected and path == part else resolve(path, *args, **kwargs)
        with self.assertRaisesRegex(RuntimeError, 'Source redirected during Python discovery'):
            with mock.patch.object(Path, 'resolve', autospec=True, side_effect=redirect):
                with owned.discovery_source_snapshot(self.scratch/'Engine'):
                    read_owned_source(main)
                    # Do not require Windows symlink creation privileges: change
                    # the binding before context exit, retaining the same bytes.
                    redirected = True

    def test_discovery_snapshot_does_not_leak_after_success_or_exception(self):
        main = self.write('owner.snapshot.cpp', 'before\n')
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            read_owned_source(main)
            retained = owned._DISCOVERY_SNAPSHOT.get()
        self.assertFalse(retained.expansions)
        self.assertFalse(retained.dependencies)
        main.write_text('after!\n', encoding='utf-8')
        self.assertEqual(read_owned_source(main), 'after!\n')
        with self.assertRaisesRegex(LookupError, 'discovery failed'):
            with owned.discovery_source_snapshot(self.scratch/'Engine'):
                read_owned_source(main)
                main.write_text('newest\n', encoding='utf-8')
                raise LookupError('discovery failed')
        main.write_text('latest\n', encoding='utf-8')
        self.assertEqual(read_owned_source(main), 'latest\n')
        self.assertIsNone(owned._DISCOVERY_SNAPSHOT.get())

    def test_discovery_exception_survives_changed_include_and_clears_cache(self):
        main = self.write('owner.snapshot.cpp', '#include "owner.snapshot.inl"\n')
        part = self.write('owner.snapshot.inl', 'before\n')
        with self.assertRaisesRegex(LookupError, 'original discovery error'):
            with owned.discovery_source_snapshot(self.scratch/'Engine'):
                read_owned_source(main)
                retained = owned._DISCOVERY_SNAPSHOT.get()
                part.write_text('after!\n', encoding='utf-8')
                raise LookupError('original discovery error')
        self.assertIsNone(owned._DISCOVERY_SNAPSHOT.get())
        self.assertFalse(retained.expansions)
        self.assertFalse(retained.dependencies)
        self.assertEqual(read_owned_source(main), 'after!\n')

    def test_discovery_snapshot_leaves_plain_files_outside_engine_fresh(self):
        plain = self.scratch/'note.txt'
        plain.write_text('before\n', encoding='utf-8')
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            self.assertEqual(read_owned_source(plain), 'before\n')
            plain.write_text('after!\n', encoding='utf-8')
            self.assertEqual(read_owned_source(plain), 'after!\n')

    def test_discovery_snapshot_isolates_budgets_and_encoding(self):
        main = self.write('owner.snapshot.cpp', '#include "owner.snapshot.inl"\n')
        part = self.write('owner.snapshot.inl', '\ufeffpart\n')
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            self.assertEqual(read_owned_source(main), '\ufeffpart\n')
            self.assertEqual(read_owned_source(main, encoding='utf-8-sig'), 'part\n')
            with self.assertRaisesRegex(ValueError, 'budget'):
                read_owned_source(main, maximum_files=1)
            with self.assertRaisesRegex(ValueError, 'budget'):
                read_owned_source(main, maximum_bytes=1)
        part.write_bytes(b'\xff\n')
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            self.assertEqual(read_owned_source(main, errors='replace'), '\ufffd\n')
            with self.assertRaises(UnicodeDecodeError):
                read_owned_source(main)

    def test_discovery_snapshot_leaves_other_engine_fixtures_fresh(self):
        other = self.scratch/'other/Engine/src/domain/owner.document.cpp'
        other.parent.mkdir(parents=True)
        other.write_text('before\n', encoding='utf-8')
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            self.assertEqual(read_owned_source(other), 'before\n')
            other.write_text('after!\n', encoding='utf-8')
            self.assertEqual(read_owned_source(other), 'after!\n')

    def test_discovery_snapshot_leaves_nested_engine_fixtures_fresh(self):
        other = self.source/'fixtures/Engine/src/domain/owner.document.cpp'
        other.parent.mkdir(parents=True)
        other.write_text('before\n', encoding='utf-8')
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            self.assertEqual(read_owned_source(other), 'before\n')
            other.write_text('after!\n', encoding='utf-8')
            self.assertEqual(read_owned_source(other), 'after!\n')

    def test_discovery_snapshot_preserves_escape_cycle_and_budget_refusals(self):
        cycle = self.write('owner.cycle.cpp', '#include "owner.cycle.inl"\n')
        self.write('owner.cycle.inl', '#include "owner.cycle.inl"\n')
        escape = self.write('owner.escape.cpp', '#include "../../outside.inl"\n')
        (self.source.parent.parent/'outside.inl').write_text('outside\n', encoding='utf-8')
        budget = self.write('owner.budget.cpp', '#include "owner.part.inl"\n')
        self.write('owner.part.inl', 'part\n')
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            with self.assertRaisesRegex(ValueError, 'Cyclic'):
                read_owned_source(cycle)
            with self.assertRaisesRegex(ValueError, 'escapes'):
                read_owned_source(escape)
            with self.assertRaisesRegex(ValueError, 'budget'):
                read_owned_source(budget, maximum_files=1)

    def test_discovery_snapshot_cannot_be_nested(self):
        with owned.discovery_source_snapshot(self.scratch/'Engine'):
            with self.assertRaisesRegex(RuntimeError, 'cannot be nested'):
                with owned.discovery_source_snapshot(self.scratch/'Engine'):
                    self.fail('nested snapshot opened')

    def test_discovery_snapshot_keeps_parent_components_until_resolution(self):
        main = self.write('owner.alias.cpp', 'lexical path\n')
        target = self.write('owner.target.cpp', 'resolved binding\n').resolve()
        (self.source/'nested').mkdir()
        requested = self.source/'nested/..'/main.name
        resolve = Path.resolve
        def resolve_binding(path, *args, **kwargs):
            # A symlink before '..' may resolve differently from lexical
            # normalization. Preserve that binding without Windows privileges.
            return target if path == requested else resolve(path, *args, **kwargs)
        with mock.patch.object(Path, 'resolve', autospec=True, side_effect=resolve_binding):
            with owned.discovery_source_snapshot(self.scratch/'Engine'):
                self.assertEqual(read_owned_source(requested), 'resolved binding\n')
