"""Run the source-layout validator against real and missing MSBuild inputs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(os.name == 'nt', 'Windows PowerShell source validator')
class SourceLayoutInputs(unittest.TestCase):
    def test_incremental_groups_resolve_without_hiding_missing_sources(self):
        parent = ROOT / 'build' / 'contract-tests'
        parent.mkdir(parents=True, exist_ok=True)
        fixture = Path(tempfile.mkdtemp(prefix='source-layout-', dir=parent))
        self.addCleanup(shutil.rmtree, fixture)
        engine = fixture / 'Engine'
        owner = engine / 'src' / 'editor' / 'application'
        owner.mkdir(parents=True)
        (owner / 'editor.example.inl').write_text('// fixture\n', encoding='utf-8')
        (engine / 'CMakeLists.txt').write_text('# fixture\n', encoding='utf-8')
        (engine / 'Engine.vcxitems.filters').write_text('<Project/>', encoding='utf-8')
        for name in ('EpochPlatformEngine', 'EpochAudioEngine', 'EpochMediaEngine', 'EpochSelfCoding'):
            dependency = engine / 'dep' / name
            dependency.mkdir(parents=True)
            for suffix in ('.vcxproj', '.vcxproj.filters'):
                (dependency / (name + suffix)).write_text('<Project/>', encoding='utf-8')
            if name == 'EpochSelfCoding':
                for suffix in ('.vcxitems', '.vcxitems.filters'):
                    (dependency / ('EpochSelfCodingIntegration' + suffix)).write_text('<Project/>', encoding='utf-8')

        def run(pattern, extra=''):
            (engine / 'Engine.vcxitems').write_text(
                '<Project><ItemGroup><EpochImplementationFragment Include="'
                '$(MSBuildThisFileDirectory)src/editor/application/' + pattern + '"/>'
                + extra + '</ItemGroup></Project>', encoding='utf-8')
            return subprocess.run(
                ['powershell.exe', '-NoProfile', '-File', str(ROOT / 'Tools/source/validate_source_layout.ps1'),
                 '-RepoRoot', str(fixture)], capture_output=True, text=True, timeout=30)

        valid = run('*.inl')
        self.assertEqual(valid.returncode, 0, valid.stdout + valid.stderr)
        empty = run('missing*.inl')
        self.assertNotEqual(empty.returncode, 0)
        self.assertIn('missing implementation inputs', empty.stdout)
        missing = run('*.inl', '<ClInclude Include="$(MSBuildThisFileDirectory)src/editor/missing.hpp"/>')
        self.assertNotEqual(missing.returncode, 0)
        self.assertIn('references missing source: src/editor/missing.hpp', missing.stdout)


if __name__ == '__main__':
    unittest.main()
