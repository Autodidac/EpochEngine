#!/usr/bin/env python3
"""Static Windows/MSVC regression checks for EpochEngine's embedded EngCoder transport.

Run: python Tests/ai/check_embedded_engcoder_msvc.py
This does not replace a real Windows/MSVC build or model-load test.
"""
from pathlib import Path
import json
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
engine = root / 'Engine'
model_path = root / 'Engine/src/ai/native/ai.backend_model_lm_studio_provider.cpp'
model = model_path.read_text(encoding='utf-8')

# The directives must occur before the first included third-party/standard header.
first_include = model.index('#include')
assert '#if defined(_WIN32)' in model[:first_include]
assert '#  ifndef NOMINMAX' in model[:first_include]
assert '#    define NOMINMAX' in model[:first_include]
assert '#  ifndef WIN32_LEAN_AND_MEAN' in model[:first_include]
assert '#  ifdef max' in model and '#    undef max' in model
assert '#  ifdef min' in model and '#    undef min' in model
assert 'std::numeric_limits<long>::max()' in model

msbuild_ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
for name in ('EpochEngine', 'EpochEditor'):
    path = engine / 'examples' / name / f'{name}.vcxproj'
    doc = ET.parse(path).getroot()
    conditions = {group.attrib.get('Condition', ''): group for group in doc.findall('m:ItemDefinitionGroup', msbuild_ns)}
    for config in ('Debug|x64', 'Release|x64'):
        matches = [group for condition, group in conditions.items() if f"'$(Configuration)|$(Platform)'=='{config}'" in condition]
        assert len(matches) == 1, (name, config, len(matches))
        definitions = matches[0].findtext('m:ClCompile/m:PreprocessorDefinitions', default='', namespaces=msbuild_ns)
        assert 'NOMINMAX;' in definitions, (name, config, definitions)
        assert 'WIN32_LEAN_AND_MEAN;' in definitions, (name, config, definitions)
        if name == 'EpochEditor':
            libs = matches[0].findtext('m:Link/m:AdditionalDependencies', default='', namespaces=msbuild_ns).split(';')
            if config.startswith('Debug'):
                assert 'libcurl-d.lib' in libs and 'zlibd.lib' not in libs, libs
                assert 'libcurl.lib' not in libs and 'zlib.lib' not in libs, libs
            else:
                assert 'libcurl.lib' in libs and 'zlib.lib' not in libs, libs
                assert 'libcurl-d.lib' not in libs and 'zlibd.lib' not in libs, libs
            dirs = matches[0].findtext('m:Link/m:AdditionalLibraryDirectories', default='', namespaces=msbuild_ns)
            assert '$(EpochVcpkgInstallRoot)debug\\lib' in dirs if config.startswith('Debug') else '$(EpochVcpkgInstallRoot)lib' in dirs

manifest = json.loads((engine/'vcpkg.json').read_text(encoding='utf-8'))
assert {'curl','zlib'}.issubset({v if isinstance(v,str) else v['name'] for v in manifest['dependencies']})
assert manifest['version-string'] == '0.90.46'
module = (engine/'modules/epoch.version.ixx').read_text(encoding='utf-8')
assert '#  define EPOCH_VERSION_REVISION_VALUE 46' in module
resource = (engine/'resource/WinResource.rc').read_bytes()
assert not resource.startswith(b'\xef\xbb\xbf')
assert b'FILEVERSION 0,90,46,0' in resource
assert b'PRODUCTVERSION 0,90,46,0' in resource
assert (engine/'src/build/build.cpp_feature_probe.hpp').is_file()
assert (engine/'resource/icon.ico').is_file()

print('PASS: embedded EngCoder MSVC macro guards, x64 Debug/Release dependencies, vcpkg, resources and version')

# The real editor must enable manifest installation without importing all of vcpkg's libraries automatically.
editor_project=(engine/'examples/EpochEditor/EpochEditor.vcxproj').read_text(encoding='utf-8')
assert '<VcpkgEnabled Condition="\'$(Configuration)\'!=\'BuildAll\'">true</VcpkgEnabled>' in editor_project
assert '<VcpkgAutoLink>false</VcpkgAutoLink>' in editor_project
assert '<VcpkgManifestInstall>true</VcpkgManifestInstall>' in editor_project

# Only the normal logger belongs in Epoch's MSVC shared items.
shared_items=(engine/'Engine.vcxitems').read_text(encoding='utf-8')
assert 'src\\core\\core.log.cpp' in shared_items
assert 'src\\core\\core.log_headless.cpp' not in shared_items
assert 'src/core/core.log_headless.cpp' in (engine/'CMakeLists.txt').read_text(encoding='utf-8')

for relative in ('modules/ai.engine.ixx','src/ai/ai.engine.cpp','src/core/core.log.cpp','src/platform/platform.window.cpp'):
    text=(engine/relative).read_text(encoding='utf-8')
    assert 'ifndef WIN32_LEAN_AND_MEAN' in text, relative
print('PASS: v0.90.46 editor final-link dependency ownership, headless logger isolation and Windows macro definitions')
