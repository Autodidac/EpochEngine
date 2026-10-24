from Tools.source.owned_source_text import read_owned_source
"""Exercise the real provider projection; no model/process/network is started."""
from pathlib import Path
import importlib.util
import os
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / 'Engine/src/ai/native'
spec = importlib.util.spec_from_file_location('internal_provider_projection',
    ROOT / 'Tools/build/epoch_source_projection.py')
projection = importlib.util.module_from_spec(spec)
spec.loader.exec_module(projection)


class PublicInternalProviderBoundary(unittest.TestCase):
    def projected(self, suffix, edition):
        path = NATIVE / ('ai.backend_model_multiplex_provider.' + suffix)
        text, regions = projection.strip_regions(str(path.relative_to(ROOT)), (read_owned_source(path).encode('utf-8') if edition == 'development' else path.read_bytes()), edition)
        if edition == 'public':
            self.assertTrue(regions)
        return text.decode('utf-8')

    def test_public_physically_omits_internal_host_and_fallback(self):
        header = self.projected('hpp', 'public')
        source = self.projected('cpp', 'public')
        for private in ('InternalModelHost', 'internal_host_', 'internal_provider(',
                'internal_url_', 'canonical_internal_model(', 'ensure_started(',
                'training_backend', 'prepare_model(', 'release_idle_addons('):
            self.assertFalse(private in header + source, 'Retained private dependency: ' + private)
        self.assertFalse('"internal",' in source, 'Internal discovery metadata survived')
        self.assertFalse('"id", "engcoder://keymaker"' in source, 'Internal model advertised')
        self.assertTrue('"native-inference"' in source, 'Missing truthful native description')
        # Neither completion protocol may reroute a removed runtime to an
        # unrelated provider. The reserved selector is rejected explicitly.
        self.assertEqual(source.count('This model runtime is unavailable in this edition.'), 3)
        self.assertTrue('if (is_internal_model(model)) return false;' in source)
        for method in ('complete(', 'complete_stream('):
            self.assertTrue('external_.' + method in source, 'External transport removed: ' + method)
        self.assertTrue('return complete_stream(request, {});' in source, 'Native non-streaming bridge removed')
        self.assertTrue('(*provider)->complete_stream(translated, forward)' in source, 'Native streaming transport removed')
        self.assertTrue('native_provider(resolved_model, translated.deadline, translated.stop)' in source)

    def test_development_keeps_existing_backend(self):
        header = self.projected('hpp', 'development')
        source = self.projected('cpp', 'development')
        self.assertTrue('InternalModelHost& internal_host' in header)
        for token in ('InternalModelHost& internal_host', 'internal_host_.ensure_started()',
                'internal_host_.prepare_model(canonical_internal_model(resolved_model))',
                'internal_host_.residency_status(false)', 'internal_host_.release_idle_addons()'):
            self.assertTrue(token in source, 'Development backend removed: ' + token)
        self.assertFalse('This model runtime is unavailable in this edition.' in source)

    @unittest.skipUnless(os.name == 'nt', 'Production MSVC syntax lane is Windows-only')
    def test_actual_development_provider_still_compiles(self):
        program = Path(os.environ.get('ProgramFiles', 'C:/Program Files'))
        toolsets = sorted((program / 'Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kits = sorted(Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')).glob(
            'Windows Kits/10/Include/*'))
        dependencies = ROOT / 'Engine/vcpkg_installed/x64-windows/include'
        if not toolsets or not kits or not (dependencies / 'nlohmann/json.hpp').exists():
            self.skipTest('Installed MSVC/dependency headers not available')
        toolset, kit = toolsets[-1], kits[-1]
        compiler = toolset / 'bin/Hostx64/x64/cl.exe'
        result = subprocess.run([str(compiler), '/nologo', '/std:c++latest', '/Zs', '/TP', '/EHsc',
            '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'),
            '/I' + str(kit / 'um'), '/I' + str(dependencies),
            str(NATIVE / 'ai.backend_model_multiplex_provider.cpp')],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
