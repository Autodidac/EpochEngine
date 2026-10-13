#!/usr/bin/env python3
"""Embedded/native EngCoder ownership check; not a model-load test."""
from pathlib import Path
import json

repo = Path(__file__).resolve().parents[2]
engine = repo / 'Engine'
editor = (engine / 'src/editor/editor.application.cpp').read_text(encoding='utf-8')
ai = (engine / 'src/ai/ai.engine.cpp').read_text(encoding='utf-8')
bridge = (engine / 'src/ai/ai.native_runtime.cpp').read_text(encoding='utf-8')
model = (engine / 'src/ai/ai.model_install.cpp').read_text(encoding='utf-8')
manifest = json.loads((engine / 'ai/manifests/open_source_model_provider.json').read_text(encoding='utf-8'))
shared = (engine / 'Engine.vcxitems').read_text(encoding='utf-8')

controls = editor.split('void render_native_ai_controls(', 1)[1].split('void render_ai_model_picker(', 1)[0]
assert 'Native Epoch AI runtime' in controls
for action in ('provision()', 'install(nativeModelId)', 'load(nativeModelId)', 'unload()', 'select_for_epoch_requests'):
    assert action in controls, action
assert 'native_runtime::run_agent(user_text, cancellation)' in ai
assert 'std::make_unique<epoch_ai::runtime::RuntimeHost>' in bridge
assert 'host.load_native_model(id)' in bridge
assert 'host.provision_native_runtime()' in bridge
assert 'host.run_task(' in bridge
assert 'qwenflash://' in bridge
assert 'Config::from_environment()' in bridge
assert 'Engine/EngCoderServer.exe' not in editor
assert 'EngCoderServer.exe' not in controls
assert 'request.environment =' not in controls
assert 'qwen3.8-flash-next' not in model.lower() # Not an Epoch-managed model package.
assert 'qwen_flash_runtime.cpp' in shared
assert 'agent_runtime.cpp' in shared
assert manifest['coding_model_lanes'][0]['license'] == 'qwen-community-1.0'
assert 'Flash-Next' in manifest['coding_model_lanes'][0]['id']
print('PASS: native EngCoder/Qwen Next Flash source contract; no external EngCoderServer required')
