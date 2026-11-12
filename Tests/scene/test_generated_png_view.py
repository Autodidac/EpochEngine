"""Retained real output: producer GLB image view normalization, actual C++ decoder."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import hashlib
import importlib.util
import json
import os
import subprocess
import time
import unittest

ROOT=Path(__file__).resolve().parents[2]


class GeneratedPngView(unittest.TestCase):
    @unittest.skipUnless(os.getenv('EPOCH_GENERATED_PNG_REGRESSION_GLB'),'Explicit retained native output CPU regression')
    def test_real_output_repaired_without_relaxing_png_decoder(self):
        original=Path(os.environ['EPOCH_GENERATED_PNG_REGRESSION_GLB']).resolve()
        self.assertTrue(original.is_relative_to(ROOT))
        raw=original.read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(),'5342e71ed051a373a8ce0048d37664452f947441a1df20a8a351ccb869d33faa')
        worker=ROOT/'Tools/ai/triposr_worker.py'
        spec=importlib.util.spec_from_file_location('worker',worker)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        repaired=module.exact_png_image_views(raw)
        self.assertEqual(module.exact_png_image_views(repaired),repaired)
        proof=ROOT/'Artifacts/Reports/triposr-png-view-oct28'/str(time.time_ns());proof.mkdir(parents=True)
        output=proof/'model.glb';output.write_bytes(repaired)
        build=ROOT/'Artifacts/Intermediate/tests/generated-png-view'/proof.name
        modules=[engine_module_source(ROOT / 'Engine/modules', f'{name}.ixx') for name in ('asset.model_import','asset.mesh_artifact','asset.texture_artifact','asset.texture_import')]
        sources=[ROOT/p for p in ('Tests/scene/asset.generated_png_contract.cpp','Engine/src/assets/mesh/asset.model_import.cpp','Engine/src/assets/texture/asset.texture_artifact.cpp','Engine/src/assets/texture/asset.texture_import.cpp','Engine/src/assets/image.stb.cpp')]
        includes=ROOT/'Artifacts/Dependencies/vcpkg_installed/x64-windows/include'
        (proof/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
project(GeneratedPngView LANGUAGES CXX)
add_executable(contract {' '.join('"'+p.as_posix()+'"' for p in sources)})
target_compile_features(contract PRIVATE cxx_std_23)
target_compile_options(contract PRIVATE /W3 /WX /utf-8 /EHsc)
target_include_directories(contract PRIVATE "{includes.as_posix()}" "{(ROOT/'Engine/include').as_posix()}")
target_sources(contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine').as_posix()}" FILES {' '.join('"'+p.as_posix()+'"' for p in modules)})
''',encoding='utf-8')
        commands=[['cmake','-S',str(proof),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64']]
        for mode in ('Debug','Release'):
            commands += [['cmake','--build',str(build),'--config',mode,'--parallel','1'],[str(build/mode/'contract.exe'),str(original),str(output)]]
        results=[]
        for i,command in enumerate(commands):
            ran=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=150)
            (proof/f'lane-{i}.log').write_text(ran.stdout+ran.stderr,encoding='utf-8');results.append({'command':command,'exit':ran.returncode})
            self.assertEqual(ran.returncode,0,ran.stdout[-5000:]+ran.stderr)
        (proof/'result.json').write_text(json.dumps({'passed':True,'commands':results,'original':str(original),'source_sha256':hashlib.sha256(raw).hexdigest(),'repaired':str(output),'repaired_sha256':hashlib.sha256(repaired).hexdigest(),'scope':'Same real GLB geometry/UVs/normals; image view omits three Trimesh alignment spaces while BIN bytes stay unchanged. Actual production CRC/PNG memory decoder rejects original malformed_source and compiles corrected512px sRGB image and full model base-color bundle in Debug/Release. No inference, GUI, model approval or scene import.','owners':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [worker,*modules,*sources,Path(__file__)]}},indent=2),encoding='utf-8')


if __name__=='__main__':unittest.main()
