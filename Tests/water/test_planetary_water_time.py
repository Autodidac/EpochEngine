"""Actual optional FFT provider, typed snapshot codec and worker-only time updates."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import time
import unittest

ROOT=Path(__file__).resolve().parents[2]

class PlanetaryWaterTime(unittest.TestCase):
    def test_actual_typed_water_time_debug_release(self):
        if os.name!='nt': self.skipTest('Installed MSVC lane')
        cmake=shutil.which('cmake')
        if not cmake:self.skipTest('Installed CMake required')
        package=ROOT/'Engine/dep/EpochEngineExtensions/packages/planetary_terrain'
        report=ROOT/'Artifacts/Reports/planetary-water-time-nov01'/str(time.time_ns())
        report.mkdir(parents=True);temporary=report/'tmp';temporary.mkdir()
        env=os.environ.copy()
        for name in ('TEMP','TMP','TMPDIR'):env[name]=str(temporary)
        build=Path(os.environ.get('EPOCH_PLANET_TEST_BUILD',ROOT/'Artifacts/Intermediate/cmake/planetary-water-time-nov01'))
        paths=[ROOT/'Engine/modules/project.planetary_snapshot.ixx',ROOT/'Engine/src/project/project.planetary_snapshot.cpp',
            ROOT/'Engine/modules/water.wave.ixx',package/'modules/water.planetary.ixx',package/'tools/planet.stream_export.cpp',
            package/'tests/planet.water_time_consumer.cpp',package/'CMakeLists.txt',Path(__file__)]
        hashes=lambda:{str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()for p in paths}
        before=hashes();verdict={'passed':False,'sources':before,'invocations':[],'gpu_or_models':False,'configurations':{}}
        def run(name,command,timeout=120):
            started=time.monotonic();result=subprocess.run([str(c)for c in command],cwd=ROOT,env=env,capture_output=True,text=True,timeout=timeout)
            (report/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
            verdict['invocations'].append({'phase':name,'command':[str(c)for c in command],'exit':result.returncode,'wall_seconds':time.monotonic()-started})
            self.assertEqual(result.returncode,0,f'{report}\n{result.stdout[-3500:]}\n{result.stderr[-3500:]}')
        try:
            run('configure',[cmake,'-S',package,'-B',build,'-G','Visual Studio 17 2022','-A','x64',
                '-DCMAKE_TOOLCHAIN_FILE=','-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false',
                '-DBUILD_TESTING=ON','-DEPOCH_PLANET_BUILD_MESH_EXPORT=ON','-DEPOCH_PLANET_BUILD_WATER=ON',
                '-DEPOCH_ENGINE_SOURCE_ROOT='+str(ROOT),'-DEPOCH_PLANET_WATER_SOURCE_ROOT='+str(package.parent/'fft_ocean')])
            for config in ('Debug','Release'):
                (report/config).mkdir()
                run('build-'+config,[cmake,'--build',build,'--config',config,'--target','epoch_planet_mesh_export','epoch_planet_water_time_contract','--','/m:1','/nr:false','/p:CL_MPCount=2'])
                producer=build/config/'epoch_planet_mesh_export.exe';consumer=build/config/'epoch_planet_water_time_contract.exe'
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
                        *(['--metric-frame-v1']if os.environ.get('EPOCH_PLANET_TEST_METRIC')=='1'else[])],30)
                    fields=dict(line.split('=',1)for line in (output/'terrain.stream').read_text().splitlines()[1:]if'='in line)
                    rows.append(fields)
                    if fields['pending']=='0':break
                self.assertEqual(rows[-1]['pending'],'0','The selected LOD must settle before time resampling')
                self.assertEqual(rows[-1]['snapshot_ready'],'1')
                run('water-time-'+config,[consumer,output,str(step),report/(config+'-time.txt')],60)
                verdict['configurations'][config]={'steps':step,'cold_wall_seconds':time.monotonic()-started,'stream':rows,
                    'time_receipt':str(report/(config+'-time.txt'))}
            self.assertEqual(before,hashes(),'Frozen water source changed')
            verdict['passed']=True
        finally:
            (report/'result.json').write_text(json.dumps(verdict,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':unittest.main()
