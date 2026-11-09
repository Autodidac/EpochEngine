"""Real coarse planetary shoreline clipping; CPU-only Debug/Release modules."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import time
import unittest

ROOT=Path(__file__).resolve().parents[2]

class PlanetaryShoreline(unittest.TestCase):
    def test_actual_coarse_shoreline_debug_release(self):
        package=ROOT/'Engine/dep/EpochEngineExtensions/packages/planetary_terrain'
        ocean=package.parent/'fft_ocean'
        for owner in (package, ocean):
            if not owner.exists():
                self.skipTest(f'Optional {owner.name} source is not present; this test requires the separately admitted Extensions packages.')
        consumer=package/'tests/water.planetary_shoreline_consumer.cpp'
        cache_consumer=package/'tests/water.planetary_shoreline_cache_consumer.cpp'
        for required in (package/'package.epoch-package.json',package/'CMakeLists.txt',
                         package/'modules/planet.terrain.ixx',package/'modules/water.planetary.ixx',
                         ocean/'package.epoch-package.json',ocean/'modules/water.fft.ixx',
                         consumer,cache_consumer):
            self.assertTrue(required.is_file(), f'Incomplete optional planetary water source: {required}')
        if os.name!='nt': self.skipTest('Installed Windows MSVC lane only')
        cmake=shutil.which('cmake')
        if not cmake: self.skipTest('Installed CMake required')
        report=ROOT/'Artifacts/Reports/planetary-shoreline-nov01'/str(time.time_ns())
        report.mkdir(parents=True); source=report/'source';source.mkdir()
        temporary=report/'tmp';temporary.mkdir()
        env=os.environ.copy()
        for key in ('TEMP','TMP','TMPDIR'): env[key]=str(temporary)
        cache=ROOT/'Artifacts/Reports/launcher-theme-oct31/native-water-handoff-retest/data/Projects/GameProject01/cache/planet/a9b32734f6cc492b88bdedb3ed00481e18629655a436a3a9860ed69e42d53c18/87536e5ef6b58d8edb0fe5d7d45808c23e7d1a8700ba194b011a0806419437c2/1337-16-0-ocean'
        cache_hashes=lambda:{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in cache.iterdir() if p.is_file()} if cache.is_dir() else {}
        cache_before=cache_hashes()
        cmake_text=f'''cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0141 NEW)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(EpochPlanetShoreline LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(EPOCH_ENGINE_SOURCE_ROOT "{ROOT.as_posix()}")
set(EPOCH_PLANET_BUILD_MESH_EXPORT ON CACHE BOOL "" FORCE)
set(EPOCH_PLANET_BUILD_WATER ON CACHE BOOL "" FORCE)
set(EPOCH_PLANET_WATER_SOURCE_ROOT "{(package.parent/'fft_ocean').as_posix()}" CACHE PATH "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("{package.as_posix()}" planet)
add_executable(water_consumer "{consumer.as_posix()}")
target_link_libraries(water_consumer PRIVATE epoch_extension_planetary_water)
add_executable(cache_consumer "{cache_consumer.as_posix()}")
target_link_libraries(cache_consumer PRIVATE epoch_extension_planetary_water)
target_link_options(cache_consumer PRIVATE "/MANIFEST:EMBED" "/MANIFESTINPUT:{ROOT.as_posix()}/Engine/resource/epoch.manifest")
'''
        (source/'CMakeLists.txt').write_text(cmake_text,encoding='utf-8')
        paths=[Path(__file__),consumer,cache_consumer,package/'modules/water.planetary.ixx',
            package/'upstream/EpochPlanet-0.2.3/src/core/terrain.cpp',package/'CMakeLists.txt']
        hashes=lambda:{str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        before=hashes(); verdict={'passed':False,'source_sha256':before,'cache_sha256':cache_before,'invocations':[],'gpu_or_model_started':False}
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
                run('build-'+config,[cmake,'--build',str(build),'--config',config,'--target','water_consumer','cache_consumer','--parallel','1'])
                run('contract-'+config,[str(build/config/'water_consumer.exe'),str(report/(config+'.txt'))],30)
                if cache_before:
                    run('cache-'+config,[str(build/config/'cache_consumer.exe'),str(report/(config+'-cache.txt')),str(cache)],180)
                    self.assertEqual(cache_before,cache_hashes(),'Retained cache changed')
            self.assertEqual(before,hashes(),'Source changed while checking')
            verdict['passed']=True
        finally:
            (report/'result.json').write_text(json.dumps(verdict,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':unittest.main()
