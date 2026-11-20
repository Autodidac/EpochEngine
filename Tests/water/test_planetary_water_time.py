"""Actual optional FFT provider, typed snapshot codec and worker-only time updates."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import unittest

ROOT=Path(__file__).resolve().parents[2]

class PlanetaryWaterTime(unittest.TestCase):
    def test_actual_typed_water_time_debug_release(self):
        windows = os.name == 'nt'
        if not windows and sys.platform != 'linux':
            self.skipTest('Windows MSVC or Linux Clang lane required')
        cmake=shutil.which('cmake')
        if not cmake:self.skipTest('Installed CMake required')
        if not windows:
            clang = shutil.which('clang++')
            if not clang or not shutil.which('clang-scan-deps') or not shutil.which('ninja'):
                self.skipTest('Linux requires Clang, clang-scan-deps and Ninja')
        package=ROOT/'Engine/dep/EpochEngineExtensions/packages/planetary_terrain'
        if not package.is_dir(): self.skipTest('Optional planetary source is unavailable')
        report=ROOT/'Artifacts/Reports/planetary-water-time-nov01'/str(time.time_ns())
        report.mkdir(parents=True);temporary=report/'tmp';temporary.mkdir()
        env=os.environ.copy()
        for name in ('TEMP','TMP','TMPDIR'):env[name]=str(temporary)
        default_build = ROOT/'Artifacts/Intermediate/cmake'/('planetary-water-time-nov01' if windows else 'planetary-water-time-linux')
        build=Path(os.environ.get('EPOCH_PLANET_TEST_BUILD',default_build))
        paths=[ROOT/'Engine/modules/project.planetary_snapshot.ixx',ROOT/'Engine/src/project/project.planetary_snapshot.cpp',
            ROOT/'Engine/modules/water/water.wave.ixx',package/'modules/water.planetary.ixx',package/'tools/planet.stream_export.cpp',
            package/'tests/planet.water_time_consumer.cpp',package/'CMakeLists.txt',Path(__file__)]
        hashes=lambda:{str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()for p in paths}
        before=hashes();verdict={'passed':False,'sources':before,'invocations':[],'gpu_or_models':False,'configurations':{}}
        def run(name,command,timeout=120):
            started=time.monotonic();result=subprocess.run([str(c)for c in command],cwd=ROOT,env=env,capture_output=True,text=True,timeout=timeout)
            (report/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
            verdict['invocations'].append({'phase':name,'command':[str(c)for c in command],'exit':result.returncode,'wall_seconds':time.monotonic()-started})
            self.assertEqual(result.returncode,0,f'{report}\n{result.stdout[-3500:]}\n{result.stderr[-3500:]}')
        try:
            generator = ['-G','Visual Studio 17 2022','-A','x64',
                '-DCMAKE_TOOLCHAIN_FILE=','-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false',
                '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded'] if windows else [
                '-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_CXX_EXTENSIONS=OFF',f'-DCMAKE_CXX_COMPILER={clang}']
            run('configure',[cmake,'-S',package,'-B',build,*generator,
                '-DBUILD_TESTING=ON','-DEPOCH_PLANET_BUILD_MESH_EXPORT=ON','-DEPOCH_PLANET_BUILD_WATER=ON',
                '-DEPOCH_ENGINE_SOURCE_ROOT='+str(ROOT),'-DEPOCH_PLANET_WATER_SOURCE_ROOT='+str(package.parent/'fft_ocean')])
            for config in (('Debug','Release') if windows else ('Release',)):
                (report/config).mkdir()
                parallel = ['--','/m:1','/nr:false','/p:CL_MPCount=2'] if windows else ['--parallel','2']
                run('build-'+config,[cmake,'--build',build,'--config',config,'--target','epoch_planet_mesh_export','epoch_planet_water_time_contract',*parallel])
                binary_dir = build/config if windows else build
                suffix = '.exe' if windows else ''
                producer=binary_dir/f'epoch_planet_mesh_export{suffix}';consumer=binary_dir/f'epoch_planet_water_time_contract{suffix}'
                wet_transition=report/(config+'-wet-transition.txt')
                run('wet-transition-'+config,[consumer,'--wet-transition',wet_transition],30)
                self.assertIn('wet_transition=passed',wet_transition.read_text())
                # Ask the actual terrain owner for a point over ocean; the
                # original Land pose is dry ground and correctly has no nearby
                # Water recipe. Do not infer water from display colors.
                pose=report/(config+'-ocean-pose.txt')
                run('ocean-pose-'+config,[consumer,pose])
                components=pose.read_text().split();camera=components[:3]
                self.assertLess(float(components[3]),-100.0)
                started=time.monotonic();rows=[]
                for step in range(1,9):
                    output=report/config/f'step-{step}'
                    run(f'provider-{config}-{step}',[producer,'--stream-step-preview',output,report/config/'cache','1337','16','0',
                        *map(str,camera),'720','1.05',os.environ.get('EPOCH_PLANET_TEST_LEVEL','18'),'384',str(128*1024*1024),'128',str(step),'1',
                        '--mesh-bytes',str(32*1024*1024),'--seconds','0','--patch-transport-v1','sha256:'+'a'*64+':'+'b'*64,
                        *(['--metric-frame-v1','--water-appearance-v1']if os.environ.get('EPOCH_PLANET_TEST_METRIC')=='1'else[])],30)
                    fields=dict(line.split('=',1)for line in (output/'terrain.stream').read_text().splitlines()[1:]if'='in line)
                    rows.append(fields)
                    if fields['pending']=='0':break
                self.assertEqual(rows[-1]['pending'],'0','The selected LOD must settle before time resampling')
                self.assertEqual(rows[-1]['snapshot_ready'],'1')
                if os.environ.get('EPOCH_PLANET_TEST_METRIC')=='1':
                    self.assertTrue((output/'terrain.water_appearance').is_file())
                run('water-time-'+config,[consumer,output,str(step),report/(config+'-time.txt')],60)
                if os.environ.get('EPOCH_PLANET_TEST_METRIC')=='1':
                    receipt=(report/(config+'-time.txt')).read_text()
                    self.assertEqual(receipt.splitlines().count('water_edge_blend=1'),3)
                verdict['configurations'][config]={'steps':step,'cold_wall_seconds':time.monotonic()-started,'stream':rows,
                    'time_receipt':str(report/(config+'-time.txt')),'wet_transition_receipt':str(wet_transition)}
            self.assertEqual(before,hashes(),'Frozen water source changed')
            verdict['passed']=True
        finally:
            (report/'result.json').write_text(json.dumps(verdict,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':unittest.main()
