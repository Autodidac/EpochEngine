"""Exact optional-data holds preserve private implementation completeness."""
import json
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
