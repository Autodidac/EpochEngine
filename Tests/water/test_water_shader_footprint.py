"""Check the exact near-water shaders without starting a graphics context."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import time
import unittest

from Tests.support.embedded_glsl import opengl_scene_fragment


ROOT = Path(__file__).resolve().parents[2]


def function_body(assembly, symbol):
    body = re.search(rf'^\s*{re.escape(symbol)} = OpFunction\b(.*?)OpFunctionEnd', assembly, re.M | re.S)
    if not body:
        raise ValueError(f'SPIR-V function missing: {symbol}')
    return body[1]


class WaterShaderFootprint(unittest.TestCase):
    def test_water_derivatives_precede_vulkan_shoreline_branch(self):
        installed = ROOT / 'Artifacts/Dependencies/vcpkg_installed/x64-windows/tools/shaderc/glslc.exe'
        compiler = str(installed) if installed.is_file() else shutil.which('glslc')
        if not compiler:
            self.skipTest('Shaderc glslc is required')
        proof = ROOT / 'Artifacts/Reports/water-shader-footprint' / str(time.time_ns())
        proof.mkdir(parents=True)
        opengl = proof / 'opengl.frag'
        opengl.write_text(opengl_scene_fragment(), encoding='utf-8')
        vulkan = ROOT / 'Engine/assets/vulkan/shaders/frag.glsl'
        commands = []
        for backend, source, options in (
            ('opengl', opengl, ['--target-env=opengl', '-fauto-map-locations', '-fauto-bind-uniforms']),
            ('vulkan', vulkan, ['--target-env=vulkan1.0']),
        ):
            output = proof / f'{backend}.spvasm'
            command = [compiler, '-fshader-stage=frag', *options, '-S', str(source), '-o', str(output)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            commands.append({'command': command, 'exit': result.returncode})
            (proof / f'{backend}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            assembly = output.read_text(encoding='utf-8')
            symbol = re.search(r'OpName (%\S+) "waterDetail\(', assembly)
            self.assertIsNotNone(symbol)
            water = function_body(assembly, symbol[1])
            main = function_body(assembly, '%main')
            self.assertNotRegex(water, r'\bOpDPd[xy]\w*\b')
            call = main.index(f'OpFunctionCall %v3float {symbol[1]}')
            self.assertIn('OpDPdx', main[:call])
            self.assertIn('OpDPdy', main[:call])
            if backend == 'vulkan':
                shoreline = main.index('OpLoad %float %waterDetailTag')
                self.assertNotRegex(main[shoreline:], r'\bOpDPd[xy]\w*\b')
                self.assertIn('OpDPdx', main[:shoreline])
                self.assertIn('OpDPdy', main[:shoreline])

        (proof / 'result.json').write_text(json.dumps(commands, indent=2), encoding='utf-8')
