"""Compile the GUI adapter and its production owners with a real-module CPU consumer."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import time
import unittest

from Tools.source.owned_source_text import definition
from Tests.support.cpp import msvc_2022_toolsets

ROOT = Path(__file__).resolve().parents[2]


class GuiDockingAdapter(unittest.TestCase):
    def test_actual_implementation_units_debug_release(self):
        if os.name != 'nt':
            self.skipTest('Uses matching accepted MSVC Engine IFCs; Linux integration is a separate gate')
        toolset = msvc_2022_toolsets()[-1]
        sdk = Path('C:/Program Files (x86)/Windows Kits/10')
        kit = sorted((sdk / 'Include').glob('*'))[-1]
        report = ROOT / 'Artifacts/Reports/gui-docking-adapter-oct29' / str(time.time_ns())
        report.mkdir(parents=True)
        sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
        production = [ROOT / 'Engine/src/epochgui' / name for name in
                      ('gui.engine.cpp', 'gui.docking_adapter.cpp', 'gui.clipboard.cpp')]
        source_hashes = {str(path.relative_to(ROOT)): sha(path) for path in production}
        declarations = definition(production[0].read_text(encoding='utf-8'), 'namespace docking_adapter_detail')
        consumer = (ROOT / 'Tests/gui/gui.docking_adapter_contract.cpp').read_text(encoding='utf-8').replace('PRODUCTION_DECLARATIONS', declarations)
        outcomes = []
        for configuration in ('Debug', 'Release'):
            runtime = ROOT / 'Artifacts/Runtime/windows/development/x64' / configuration
            intermediate = ROOT / 'Artifacts/Intermediate/msvc/development/x64' / configuration
            cl_log = intermediate / 'EpochEngine/EpochEngine.tlog/CL.command.1.tlog'
            link_log = intermediate / 'EpochEditor/EpochEditor.tlog/link.command.1.tlog'
            if not cl_log.is_file() or not link_log.is_file():
                self.skipTest('Matching accepted production Engine build inputs are required')
            output = report / configuration
            output.mkdir()
            source = output / 'consumer.cpp'
            source.write_text(consumer, encoding='utf-8')
            main = output / 'main.cpp'
            main.write_text('extern "C" int epoch_gui_docking_adapter_contract();\nint main() { return epoch_gui_docking_adapter_contract(); }\n', encoding='utf-8')
            providers = {}
            for folder in intermediate.iterdir():
                if folder.is_dir():
                    for path in folder.glob('*.ixx.ifc'):
                        providers.setdefault(path.name[:-8].lower(), path)
            for quoted, plain in re.findall(r'/reference\s+(?:"([^"]+)"|(\S+))', cl_log.read_text(encoding='utf-16'), re.I):
                value = quoted or plain
                if '=' in value:
                    name, path = value.split('=', 1)
                    if Path(path).is_file(): providers[name.lower()] = Path(path)
            self.assertIn('gui.engine', providers)
            self.assertIn('epoch.gui', providers)
            command_lines = cl_log.read_text(encoding='utf-16').splitlines()
            owner_command = next(command_lines[index + 1] for index, line in enumerate(command_lines[:-1])
                                 if line.startswith('^') and line[1:].upper() == str(production[0]).upper())
            owner_command = re.sub(re.escape(str(production[0])), '', owner_command, flags=re.I)
            # MSBuild tlogs uppercase reference identifiers; C++ module names
            # are case-sensitive. Keep the actual provider paths, normalize the
            # names to the existing lowercase module identities, one per module.
            owner_command = re.sub(r'/reference\s+(?:"[^"]+"|\S+)', '', owner_command, flags=re.I)
            for name, provider in providers.items():
                owner_command += ' /reference "' + name + '=' + str(provider) + '"'
            # CL distinguishes /Fp (PCH output) from /fp:precise and /Fi from
            # /FI. Preserve those ABI/preprocessor flags case-sensitively.
            owner_command = re.sub(r'/(?:Fo|Fd|Fa|Fp|FR|Fi|ifcOutput|sourceDependencies|scanDependencies)(?:\s*"[^"]*"|\S+)', '', owner_command)
            owner_command = re.sub(r'/FS(?=\s|$)|/Zi(?=\s|$)|/Z7(?=\s|$)', '', owner_command, flags=re.I)
            owner_command = re.sub(r'/WX-?(?=\s|$)', '', owner_command, flags=re.I)
            owner_command = re.sub(r'/errorReport:\S+', '/errorReport:none', owner_command, flags=re.I)
            producer_scan = intermediate / 'EpochEngine/gui.engine.ixx.ifc.dt.module.json.command'
            if producer_scan.exists():
                scan_bytes = producer_scan.read_bytes()
                scan_text = scan_bytes.decode('utf-16' if scan_bytes.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
                sdk_version = re.search(r'Windows Kits[\\/]10[\\/]Include[\\/]([0-9.]+)', scan_text, re.I)
                if sdk_version: kit = sdk / 'Include' / sdk_version.group(1)
            for directory in (toolset / 'include', *(kit / name for name in ('ucrt', 'um', 'shared', 'winrt', 'cppwinrt'))):
                owner_command += f' /I"{directory}"'
            owner_command += ' /FIcompare'
            tracked = [runtime / 'EpochEngine.lib', runtime / 'EpochEditor.exe', providers['gui.engine'], providers['epoch.gui']]
            accepted_inputs = {str(path): sha(path) for path in tracked}
            env = dict(os.environ)
            env['PATH'] = ';'.join([str(toolset / 'bin/Hostx64/x64'), str(sdk / 'bin' / kit.name / 'x64'), str(runtime), env.get('PATH', '')])
            for name, folder in (('LOCALAPPDATA', 'local'), ('APPDATA', 'roaming'), ('HOME', 'home'), ('USERPROFILE', 'home'), ('TEMP', 'temp'), ('TMP', 'temp')):
                directory = output / folder
                directory.mkdir(exist_ok=True)
                env[name] = str(directory)
            objects = []
            phases = []
            for ordinal, path in enumerate([*production, source, main]):
                obj = output / (str(ordinal) + '.obj')
                response = output / (str(ordinal) + '-compile.rsp')
                response.write_text(owner_command + f' /nologo /Z7 /WX /Fo"{obj}" "{path}"', encoding='utf-8')
                started = time.perf_counter()
                result = subprocess.run([str(toolset / 'bin/Hostx64/x64/cl.exe'), '@' + str(response)], cwd=output, env=env,
                                        capture_output=True, text=True, timeout=120, creationflags=subprocess.CREATE_NO_WINDOW)
                (output / (str(ordinal) + '-compile.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
                self.assertEqual(result.returncode, 0, str(response) + '\n' + result.stdout + result.stderr)
                phases.append({'phase': 'compile', 'source': str(path), 'exit': result.returncode, 'seconds': time.perf_counter() - started})
                objects.append(obj)
            lines = link_log.read_text(encoding='utf-16').splitlines()
            header = next(line for line in lines if line.startswith('^'))
            options = next(line for line in lines if line.upper().startswith('/OUT:'))
            options = re.sub(r'/(?:OUT|PDB|IMPLIB|SUBSYSTEM|ENTRY):(?:"[^"]*"|\S+)', '', options, flags=re.I)
            options = re.sub(r'(?:"[^"]+\.(?:OBJ|RES)"|\S+\.(?:OBJ|RES))(?=\s|$)', '', options, flags=re.I)
            options = re.sub(r'/DEBUG(?::\S+)?', '', options, flags=re.I)
            libraries = []
            for value in header[1:].split('|') + lines:
                if value.upper().endswith('.LIB') and not value.startswith(('^', '/')) and value.lower() not in [item.lower() for item in libraries]: libraries.append(value)
            binary = output / 'consumer.exe'
            response = options + '\n' + f'/OUT:"{binary}" /IMPLIB:"{output / "consumer.lib"}" /ENTRY:mainCRTStartup /SUBSYSTEM:CONSOLE /DEBUG:NONE\n'
            response += '\n'.join('"' + str(path) + '"' for path in [*objects, *libraries]) + '\n'
            for directory in (toolset / 'lib/x64', sdk / 'Lib' / kit.name / 'ucrt/x64', sdk / 'Lib' / kit.name / 'um/x64'): response += f'/LIBPATH:"{directory}"\n'
            link_response = output / 'link.rsp'
            link_response.write_text(response, encoding='utf-8')
            for name, command in (('link', [str(toolset / 'bin/Hostx64/x64/link.exe'), '@' + str(link_response)]), ('run', [str(binary)])):
                started = time.perf_counter()
                result = subprocess.run(command, cwd=output, env=env, capture_output=True, text=True, timeout=90 if name == 'link' else 15, creationflags=subprocess.CREATE_NO_WINDOW)
                (output / (name + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
                self.assertEqual(result.returncode, 0, str(output) + '\n' + result.stdout + result.stderr)
                phases.append({'phase': name, 'exit': result.returncode, 'seconds': time.perf_counter() - started})
            self.assertEqual(accepted_inputs, {str(path): sha(path) for path in tracked})
            outcomes.append({'configuration': configuration, 'phases': phases, 'accepted_inputs_unchanged': True, 'inputs': accepted_inputs})
        self.assertEqual(source_hashes, {str(path.relative_to(ROOT)): sha(path) for path in production})
        (report / 'result.json').write_text(json.dumps({'actual_production_implementation_units': list(source_hashes), 'source_sha256': source_hashes,
            'actual_primary_gui_engine_and_epoch_gui_ifcs': True, 'results': outcomes, 'window_renderer_models': False,
            'helper_flags': 'Z7/DEBUG:NONE affect only helper outputs; matching accepted CRT/std/IFCs and real production link inputs'}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
