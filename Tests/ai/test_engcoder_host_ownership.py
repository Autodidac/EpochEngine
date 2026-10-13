"""Contract checks for Epoch's embedded EngCoder/Qwen runtime ownership.

Run: python Tests/ai/test_engcoder_host_ownership.py
These are source checks only; no GPU/model inference is performed.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
EDITOR = (ROOT / 'Engine/src/editor/editor.application.cpp').read_text(encoding='utf-8')
BRIDGE = (ROOT / 'Engine/src/ai/ai.native_runtime.cpp').read_text(encoding='utf-8')
AI = (ROOT / 'Engine/src/ai/ai.engine.cpp').read_text(encoding='utf-8')
MODEL_INSTALL = (ROOT / 'Engine/src/ai/ai.model_install.cpp').read_text(encoding='utf-8')
START = EDITOR.split('void render_native_ai_controls(', 1)[1].split(
    'void render_ai_model_picker(', 1)[0]


class EmbeddedEngCoderOwnership(unittest.TestCase):
    def test_engine_embeds_host_not_server(self):
        self.assertIn('epoch_ai::runtime::RuntimeHost', BRIDGE)
        self.assertIn('Native Epoch AI runtime', START)
        self.assertNotIn('EngCoderServer.exe', START)
        self.assertNotIn('request.arguments =', START)

    def test_preserves_model_profile_and_weights(self):
        self.assertIn('Config::from_environment()', BRIDGE)
        self.assertIn('host.unload_native_model()', BRIDGE)
        self.assertNotIn('ENGCODER_MODEL_CACHE_ROOT', BRIDGE)
        self.assertNotIn('request.environment =', BRIDGE)

    def test_native_controls_and_direct_epoch_routing(self):
        for action in ('provision()', 'install(nativeModelId)', 'load(nativeModelId)',
                       'unload()', 'select_for_epoch_requests'):
            self.assertIn(action, START)
        self.assertIn('native_runtime::run_agent(user_text, cancellation)', AI)

    def test_specialized_qwen_loader_and_sandbox(self):
        self.assertIn('qwenflash://', BRIDGE)
        self.assertIn('host.load_native_model(id)', BRIDGE)
        self.assertIn('config.workspace = paths.workspace', BRIDGE)
        self.assertIn('native_storage::initialize(paths)', BRIDGE)
        self.assertNotIn('config.workspace = config.data_root /', BRIDGE)
        self.assertIn('os_model_qwen_3_8_27b', MODEL_INSTALL)
        self.assertNotIn('qwen3.8-flash-next', MODEL_INSTALL.lower())

    def test_no_second_frontend(self):
        self.assertNotIn('EngCoderGUI.exe', START)
        self.assertNotIn('start_engcoder_server', START)
        self.assertIn('Live model text is in Raw Output', START)


if __name__ == '__main__':
    unittest.main(verbosity=2)
