"""Compile the optional real FFT source against the shared Water owners."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import time
import unittest

ROOT=Path(__file__).resolve().parents[2]

class FFTWaterIntegration(unittest.TestCase):
    def test_actual_fft_shared_water_debug_release(self):
        package=ROOT/'Engine/dep/EpochEngineExtensions/packages/fft_ocean'
        if not package.exists():
            self.skipTest('Optional FFT ocean source is not present; this test requires the separately admitted Extensions package.')
        consumer=package/'tests/water.fft_consumer.cpp'
        for required in (package/'package.epoch-package.json', package/'CMakeLists.txt',
                         package/'modules/water.fft.ixx', consumer):
            self.assertTrue(required.is_file(), f'Incomplete optional FFT ocean source: {required}')
        if os.name!='nt': self.skipTest('Installed Windows MSVC lane only')
        cmake=shutil.which('cmake')
        if not cmake: self.skipTest('Installed CMake required')
        report=ROOT/'Artifacts/Reports/water-fft-oct31'/str(time.time_ns())
        report.mkdir(parents=True); source=report/'source';source.mkdir()
        temporary=report/'tmp';temporary.mkdir()
        env=os.environ.copy()
        for key in ('TEMP','TMP','TMPDIR'): env[key]=str(temporary)
        engine=ROOT/'Engine'
        modules=('render.math','voxel.field','voxel.storage','water.system','water.surface','asset.texture_artifact','asset.mesh_artifact')
        owners=[engine/'src/water/water.system.cpp',engine/'src/water/water.surface.cpp',engine/'src/assets/texture/asset.texture_artifact.cpp']
        owner_text='\n'.join(f'"{p.as_posix()}"' for p in owners)
        module_text='\n'.join(f'"{(engine/"modules"/(m+".ixx")).as_posix()}"' for m in modules)
        cmake_text=f'''cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0141 NEW)
project(EpochFFTWater LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
set(EPOCH_ENGINE_SOURCE_ROOT "{ROOT.as_posix()}")
set(EPOCH_FFT_BUILD_WATER_ADAPTER ON CACHE BOOL "" FORCE)
add_subdirectory("{package.as_posix()}" fft)
add_library(water_owners STATIC {owner_text})
target_sources(water_owners PUBLIC FILE_SET water_modules TYPE CXX_MODULES
 BASE_DIRS "{(engine/'modules').as_posix()}" FILES {module_text})
target_compile_options(water_owners PUBLIC /sdl /permissive- /Zc:__cplusplus /utf-8 /W4 /WX)
target_link_libraries(water_owners PUBLIC epoch_extension_water_fft)
add_executable(water_consumer "{consumer.as_posix()}")
target_link_libraries(water_consumer PRIVATE water_owners)
'''
        (source/'CMakeLists.txt').write_text(cmake_text,encoding='utf-8')
        paths=[*owners,consumer,Path(__file__),engine/'modules/water.wave.ixx',engine/'cmake/epoch_water_wave.cmake',package/'modules/water.fft.ixx',package/'CMakeLists.txt',package/'src/fft_ocean.cpp',package/'include/epoch/extensions/fft_ocean/fft_ocean.hpp',*[engine/'modules'/(m+'.ixx') for m in modules]]
        hashes=lambda:{str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        before=hashes(); verdict={'passed':False,'source_sha256':before,'invocations':[],'gpu_or_model_started':False}
        def run(name,cmd,timeout=120):
            started=time.monotonic()
            result=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,timeout=timeout)
            transcript=result.stdout+result.stderr;(report/(name+'.log')).write_text(transcript,encoding='utf-8')
            verdict['invocations'].append({'phase':name,'command':cmd,'exit':result.returncode,'wall_seconds':time.monotonic()-started})
            self.assertEqual(result.returncode,0,f'{report}\n{transcript[-6000:]}')
        try:
            build=report/'build'
            run('configure',[cmake,'-S',str(source),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64','-T','host=x64','-DCMAKE_TOOLCHAIN_FILE=','-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false'])
            for config in ('Debug','Release'):
                run('build-'+config,[cmake,'--build',str(build),'--config',config,'--target','water_consumer','epoch_extension_fft_ocean_self_test','--parallel','1'])
                run('legacy-'+config,[str(build/'fft'/config/'epoch_extension_fft_ocean_self_test.exe')],30)
                run('contract-'+config,[str(build/config/'water_consumer.exe'),str(report/(config+'.txt'))],30)
            self.assertEqual(before,hashes(),'Source changed while checking')
            self.assertEqual((report/'Debug.txt').read_bytes(),(report/'Release.txt').read_bytes())
            verdict['passed']=True
        finally:
            (report/'result.json').write_text(json.dumps(verdict,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':unittest.main()
