"""Source contract: Epoch owns state, creates workspace, tracks async installs."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
BRIDGE=(ROOT/'Engine/src/ai/ai.native_runtime.cpp').read_text(encoding='utf-8')
STORAGE=(ROOT/'Engine/src/ai/ai.epoch_native_storage.hpp').read_text(encoding='utf-8')
MANAGER=(ROOT/'Engine/src/ai/native/ai.backend_model_native_model_manager.cpp').read_text(encoding='utf-8')
QFR=(ROOT/'Engine/src/ai/native/ai.backend_model_qwen_flash_runtime.cpp').read_text(encoding='utf-8')
EDITOR=(ROOT/'Engine/src/editor/editor.application.cpp').read_text(encoding='utf-8')

class NativeOwnership(unittest.TestCase):
    def test_epoch_application_path(self):
        self.assertIn('"EpochEngine"', STORAGE)
        self.assertIn('"AI" / "native"', STORAGE)
        self.assertNotIn('/ "EngCoder"', STORAGE)
        for expected in ('config.data_root = paths.data_root;', 'config.workspace = paths.workspace;',
                         'config.model_cache_root = paths.model_cache;', 'native_storage::initialize(paths);'):
            self.assertIn(expected, BRIDGE)
        self.assertLess(BRIDGE.find('native_storage::initialize(paths)'),BRIDGE.find('std::make_unique<epoch_ai::runtime::RuntimeHost>'))
    def test_accurate_model_progress(self):
        self.assertIn('host.native_model_library()', BRIDGE)
        self.assertIn('state=="installed"', BRIDGE)
        self.assertIn('state=="failed"', BRIDGE)
        self.assertIn('cached_bytes', BRIDGE)
        self.assertIn('install["cache"]', MANAGER)
        self.assertIn('Epoch AI directory', EDITOR)
        self.assertIn('cache.value("installed", false)', QFR)
        self.assertIn('config_.model_cache_root.string()', QFR)
        self.assertNotIn('EngCoder\'s existing user profile', EDITOR)

if __name__=='__main__':unittest.main(verbosity=2)
