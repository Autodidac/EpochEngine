# SPDX-License-Identifier: LicenseRef-MIT-NoSell
# Copyright (c) 2026 Adam Rushford
"""Exercise the committed source owner's repository boundary and exact archives."""
from contextlib import redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

OWNER = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('extensions_source_package', OWNER / 'tools/package_source.py')
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)
validator_spec = importlib.util.spec_from_file_location('extensions_manifest_validator', OWNER / 'tools/validate_packages.py')
validator = importlib.util.module_from_spec(validator_spec)
validator_spec.loader.exec_module(validator)


class ManifestSourceBinding(unittest.TestCase):
    def test_actual_runner_repository_matches_manifest_and_refuses_legacy_route(self):
        manifests = sorted((OWNER / 'packages').glob('*/package.epoch-package.json'))
        self.assertTrue(manifests)
        scratch = OWNER / 'build/package-owner-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        for manifest_path in manifests:
            manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
            script = manifest_path.parent / manifest['script']['path']
            repository = manifest['source']['repo']
            with self.subTest(package=manifest['id']), tempfile.TemporaryDirectory(dir=scratch) as directory:
                self.assertEqual(validator.validate_script_repository(script, repository), [])
                fixture = Path(directory) / script.name
                fixture.write_text(script.read_text(encoding='utf-8').replace(
                    repository, 'https://github.com/Autodidac/EpochEngineExtensions'), encoding='utf-8')
                self.assertTrue(validator.validate_script_repository(fixture, repository))
                # A matching URL elsewhere must not hide a wrong plan binding.
                with fixture.open('a', encoding='utf-8') as output:
                    output.write('\n// '+repository+'\n')
                self.assertTrue(validator.validate_script_repository(fixture, repository))
                self.assertTrue(validator.validate_script_repository(script, ''))


class SourcePackaging(unittest.TestCase):
    def setUp(self):
        scratch = OWNER / 'build/package-owner-tests'
        scratch.mkdir(parents=True, exist_ok=True)
        self.scratch = tempfile.TemporaryDirectory(dir=scratch)
        self.root = Path(self.scratch.name)

    def tearDown(self):
        self.scratch.cleanup()

    def make_repository(self):
        source = self.root / 'source'
        source.mkdir()
        files = {
            '.gitattributes': b'raw-source.hpp -text\n',
            'raw-source.hpp': b'// exact provenance bytes\r\n',
            'run.sh': b'#!/bin/sh\nexit 0\n',
            'LICENSE': b'fixture license\n',
            'README.md': b'fixture source\n',
            'CMakeLists.txt': b'project(EpochEngineExtensions VERSION 0.2.3 LANGUAGES CXX)\n',
            'catalog/extensions.json': b'{}\n',
            'demonstrations/tiered_terrain/modules/tiered_terrain.generator.ixx': b'export module fixture;\n',
            'packages/_api/epoch_package_script_api.hpp': b'#pragma once\n',
        }
        for name, body in files.items():
            target = source / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(body)
        def git(*arguments):
            return subprocess.check_output(['git', '-C', str(source), *arguments], stderr=subprocess.STDOUT)
        git('init', '--quiet')
        git('config', 'core.autocrlf', 'true')
        git('config', 'core.filemode', 'false')
        git('remote', 'add', 'origin', packer.REPOSITORY_URL)
        git('add', '.')
        git('update-index', '--chmod=+x', 'run.sh')
        git('-c', 'user.name=Epoch package contract', '-c', 'user.email=package-contract@local',
            '-c', 'commit.gpgsign=false', 'commit', '--quiet', '-m', 'Fixture source owner')
        return source, files, git

    def test_missing_git_never_invokes_git_or_discovers_parent(self):
        (self.root / '.git').mkdir()
        child = self.root / 'detached-source'
        child.mkdir()
        with patch.object(packer, 'ROOT', child), patch.object(packer.subprocess, 'check_output') as call:
            with self.assertRaisesRegex(RuntimeError, 'own physical .git'):
                packer.committed_source()
            call.assert_not_called()

    def test_git_uses_explicit_owned_directory_and_work_tree(self):
        (self.root / '.git').mkdir()
        with patch.object(packer, 'ROOT', self.root), patch.object(packer.subprocess, 'check_output', return_value=b'ok') as call:
            self.assertEqual(packer.git('rev-parse', 'HEAD'), b'ok')
            call.assert_called_once_with(['git', '--git-dir', str(self.root / '.git'),
                                          '--work-tree', str(self.root), 'rev-parse', 'HEAD'])

    def test_foreign_origin_refused(self):
        source, _, git = self.make_repository()
        git('remote', 'set-url', 'origin', 'https://github.com/Autodidac/EpochEngineExtensions.git')
        with patch.object(packer, 'ROOT', source):
            with self.assertRaisesRegex(RuntimeError, 'Unexpected source repository'):
                packer.committed_source()

    def test_dirty_source_refused(self):
        source, _, _ = self.make_repository()
        (source / 'README.md').write_bytes(b'uncommitted\n')
        with patch.object(packer, 'ROOT', source):
            with self.assertRaisesRegex(RuntimeError, 'Commit the reviewed source'):
                packer.committed_source()

    def test_exact_committed_site_source_and_receipt(self):
        source, _, git = self.make_repository()
        output = self.root / 'package'
        with patch.object(packer, 'ROOT', source), redirect_stdout(io.StringIO()):
            packer.package(output)
        receipt = json.loads((output / 'source-package-receipt.json').read_text())
        self.assertEqual(receipt['repository'], packer.REPOSITORY_URL)
        self.assertEqual(receipt['commit'], git('rev-parse', 'HEAD').decode().strip())
        with zipfile.ZipFile(next(output.glob('*.zip'))) as archive:
            with tarfile.open(next(output.glob('*.tar.gz')), 'r:gz') as tar:
                for entry in archive.infolist():
                    relative = entry.filename.split('/', 1)[1]
                    self.assertNotIn('.git', Path(relative).parts)
                    self.assertEqual(archive.read(entry), git('show', 'HEAD:' + relative))
                    mode = int(git('ls-tree', 'HEAD', relative).split()[0], 8)
                    self.assertEqual(entry.external_attr >> 16, mode)
                    self.assertEqual(tar.getmember(entry.filename).mode, mode & 0o777)
        with patch.object(packer, 'ROOT', source), redirect_stdout(io.StringIO()):
            with self.assertRaises(FileExistsError):
                packer.package(output)

    @unittest.skipUnless(os.name == 'nt', 'Windows PowerShell wrapper')
    def test_manifest_wrapper_propagates_validator_exit(self):
        source = self.root / 'wrapper'
        (source / 'tools').mkdir(parents=True)
        runner = source / 'run_validate_packages.ps1'
        runner.write_bytes((OWNER / runner.name).read_bytes())
        (source / 'tools/validate_packages.py').write_text('raise SystemExit(7)\n')
        run = subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(runner)],
                             capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 7, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
