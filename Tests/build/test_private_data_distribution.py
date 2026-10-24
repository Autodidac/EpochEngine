"""Exact optional-data holds preserve private implementation completeness."""
import json
import os
import subprocess
from pathlib import Path
import tempfile
import unittest
from Tools.build import package_epoch_source as packer
ROOT = Path(__file__).resolve().parents[2]
COMPONENT = 'Engine/dep/EpochSelfCoding/'

class PrivateDataDistribution(unittest.TestCase):
    def test_actual_collection_omits_only_reviewed_seeds_and_keeps_tools(self):
        holds=packer.held_component_data(ROOT)
        self.assertEqual(len(holds),4)
        for name in holds:self.assertTrue((ROOT/name).is_file())
        collected={p.relative_to(ROOT).as_posix() for p in packer.collect(ROOT)}
        self.assertFalse(holds & collected)
        for name in ('assets/ai/epoch.local_mcp_iteration.ps1',
                     'assets/ai/native/training/model_worker.py',
                     'assets/ai/native/training/curriculum_compiler.py',
                     'data_distribution.json','CMakeLists.txt'):
            self.assertIn(COMPONENT+name,collected)

    @unittest.skipUnless(os.name == 'nt', 'Installed PowerShell fixture lane')
    def test_actual_runtime_staging_blocks_retain_worker_and_remove_only_holds(self):
        parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True)
        policy=(ROOT/COMPONENT/'data_distribution.json').read_text()
        for owner in ('stage_epoch_windows_release.ps1','stage_epoch_linux_release.ps1'):
            text=(ROOT/'Tools/build'/owner).read_text()
            block=text.split('# EPOCH_'+'PRIVATE_SOURCE_BEGIN: private_runtime_asset_staging',1)[1].split('# EPOCH_'+'PRIVATE_SOURCE_END: private_runtime_asset_staging',1)[0]
            with tempfile.TemporaryDirectory(dir=parent,prefix='runtime-held-data-') as folder:
                fixture=Path(folder);component=fixture/COMPONENT;component.mkdir(parents=True)
                (component/'data_distribution.json').write_text(policy)
                for held in json.loads(policy)['held_paths']:
                    path=component/held;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('held fixture')
                worker=component/'assets/ai/native/training/model_worker.py';worker.write_text('worker fixture')
                stage=fixture/'build/packages/development/v0.90.183/runtime'
                (stage/'assets').mkdir(parents=True)
                script=fixture/'stage-fixture.ps1'
                script.write_text("param($repo,$stage,$helper)\n$ErrorActionPreference='Stop'\n. $helper\nfunction Require-Path { param($Path,$Label) if(!(Test-Path -LiteralPath $Path)){throw $Label} }\n$edition=[pscustomobject]@{Directory='development'}\n$packageRoot=Join-Path $repo 'build/packages'\n"+block)
                run=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(script),str(fixture),str(stage),str(ROOT/'Tools/build/epoch_package_layout.ps1')],capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                self.assertTrue((stage/'assets/ai/native/training/model_worker.py').is_file())
                for held in json.loads(policy)['held_paths']:
                    self.assertFalse((stage/held).exists())
                    self.assertTrue((component/held).is_file())

    def test_missing_or_broadened_policy_fails_closed(self):
        parent=ROOT/'build/contract-tests';parent.mkdir(parents=True,exist_ok=True)
        policy=json.loads((ROOT/COMPONENT/'data_distribution.json').read_text())
        with tempfile.TemporaryDirectory(dir=parent,prefix='private-data-policy-') as folder:
            fixture=Path(folder)
            self.assertEqual(packer.held_component_data(fixture),set())
            component=fixture/COMPONENT;component.mkdir(parents=True)
            with self.assertRaises(FileNotFoundError):packer.collect(fixture)
            path=component/'data_distribution.json';path.write_text(json.dumps(policy))
            held=next(iter(packer.held_component_data(fixture)))
            target=fixture/held;target.parent.mkdir(parents=True);target.write_text('fixture')
            retained=target.with_name('operator-review.jsonl');retained.write_text('fixture')
            self.assertNotIn(target,packer.collect(fixture))
            self.assertIn(retained,packer.collect(fixture))
            policy['held_paths'][0]='assets/ai/native/training/model_worker.py'
            path.write_text(json.dumps(policy))
            with self.assertRaises(RuntimeError):packer.collect(fixture)

if __name__=='__main__':unittest.main()
