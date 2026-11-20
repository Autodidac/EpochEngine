"""Queued cancellation uses the actual Scheduler and production LOD completion owners."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.renderers.test_runtime_preview_controls import source_receipt_fixture

ROOT = Path(__file__).resolve().parents[2]


def fixture():
    code = source_receipt_fixture()
    source = (ROOT / 'Engine/src/editor/application/editor.application_packages_creation.inl').read_text(encoding='utf-8-sig')
    pump = definition(source, '        void pump_planetary_stream(')
    begin = pump.index('                try\n') if '                try\n' in pump else pump.index('                try\r\n')
    catches = pump[pump.index('                catch', begin):pump.index('            // Poll and retire', begin)].rstrip()
    # The last brace closes the ready-future guard, outside the try/catches.
    catches = catches[:catches.rfind('}')]
    editor = r'''
struct EditorCancelProbe {
 std::optional<std::future<int>> planetaryStreamPending;
 bool planetaryStreamFailed{},planetaryStreamContinue{};
 std::string planetaryStreamStatus;
 std::uint64_t planetaryStreamNextTick{};
};
void actual_editor_completion(EditorCancelProbe& editor, bool inspecting=false) {
 const std::uint64_t tick=1000;
 editor.planetaryStreamNextTick=tick+250'000'000ull;
 try {(void)editor.planetaryStreamPending->get();editor.planetaryStreamPending.reset();}
 EDITOR_CATCHES
}
'''.replace('EDITOR_CATCHES', catches)
    code = code.replace('int main(){', editor + '\nint main(){', 1)
    checks = r'''
 // Cancel a real queued job before its body begins. Running-job cancellation
 // returns a result; queue cancellation instead makes future.get throw.
 {
  RuntimeStreamProbe owner{base/"queued-cancel"};owner.m_meshes[7]=retained;
  std::promise<void> entered,release;
  auto blocked=owner.m_planetaryScheduler->submit("Hold worker for queued cancel",[&]{entered.set_value();release.get_future().wait();});
  entered.get_future().wait();owner.pump_planetary_runtime(&context,1280,720);
  (void)owner.m_planetaryScheduler->cancel(owner.m_planetaryCancellation);
  release.set_value();blocked.get();CHECK(wait_ready(owner));
  owner.m_planetaryPaused=true;owner.pump_planetary_runtime(&context,1280,720);
  CHECK(!owner.m_planetaryFailed&&!owner.m_planetaryPending&&owner.m_planetaryContinue);
  CHECK(owner.geometry_admissions==0&&owner.m_meshes[7]==retained);
  owner.m_planetaryPaused=false;owner.m_planetaryNextTick={};
  owner.pump_planetary_runtime(&context,1280,720);CHECK(wait_ready(owner));
  owner.pump_planetary_runtime(&context,1280,720);
  CHECK(!owner.m_planetaryFailed&&owner.geometry_admissions==1);
 }
 {
  editor_tasks::Scheduler scheduler{editor_tasks::SchedulerLimits{.worker_count=1,.retained_finished_nodes=16}};
  std::promise<void> entered,release;
  auto blocked=scheduler.submit("Hold editor worker",[&]{entered.set_value();release.get_future().wait();});
  entered.get_future().wait();
  auto queued=scheduler.submit_cancellable("Cancelled editor LOD",[](std::stop_token){return 7;});
  (void)scheduler.cancel(queued.cancellation);release.set_value();blocked.get();
  EditorCancelProbe editor;editor.planetaryStreamPending.emplace(std::move(queued.completion));
  CHECK(editor.planetaryStreamPending->wait_for(std::chrono::seconds{3})==std::future_status::ready);
  actual_editor_completion(editor);
  CHECK(!editor.planetaryStreamFailed&&!editor.planetaryStreamPending&&editor.planetaryStreamContinue);
  CHECK(editor.planetaryStreamNextTick==250'001'000ull);
  auto stale=scheduler.submit("Worker retired during inspection",[]()->int{throw std::runtime_error("stale failure");});
  editor.planetaryStreamPending.emplace(std::move(stale));
  CHECK(editor.planetaryStreamPending->wait_for(std::chrono::seconds{3})==std::future_status::ready);
  actual_editor_completion(editor,true);CHECK(!editor.planetaryStreamFailed&&!editor.planetaryStreamPending);
  // A genuine worker exception must still stop automatic retries.
  auto failed=scheduler.submit("Genuine editor worker failure",[]()->int{throw std::runtime_error("worker failed");});
  editor.planetaryStreamPending.emplace(std::move(failed));
  CHECK(editor.planetaryStreamPending->wait_for(std::chrono::seconds{3})==std::future_status::ready);
  actual_editor_completion(editor);CHECK(editor.planetaryStreamFailed&&!editor.planetaryStreamPending);
 }
'''
    return code.replace(' CHECK(project_planetary::owner_checks==0);', checks + '\n CHECK(project_planetary::owner_checks==0);')


class PlanetaryCancelRecovery(unittest.TestCase):
    def test_cancel_before_start_retains_mesh_and_can_resume(self):
        suffix = os.environ.get('EPOCH_CANCEL_REPORT_SUFFIX', 'green')
        self.assertRegex(suffix, r'^[a-z0-9_-]+$')
        proof = ROOT / ('Artifacts/Reports/planet-queued-cancel84-nov07/' + suffix)
        proof.mkdir(parents=True, exist_ok=True)
        (proof / 'runtime.cancel_contract.cpp').write_text(fixture(), encoding='utf-8')
        module_map = {}
        owners = (ROOT / 'Engine/modules', ROOT / 'Engine/dep/EpochPlatformEngine/modules')
        for owner in owners:
            for path in owner.rglob('*.ixx'):
                match = re.search(r'^export module\s+([\w.:]+);', path.read_text(encoding='utf-8-sig'), re.M)
                if match:
                    self.assertNotIn(match.group(1), module_map)
                    module_map[match.group(1)] = path
        pending, closure = ['render.preview_grid', 'project.planetary', 'scene.snapshot', 'authoring.texture_detail'], set()
        while pending:
            name = pending.pop()
            if name in closure:
                continue
            self.assertIn(name, module_map)
            closure.add(name)
            pending.extend(re.findall(r'^(?:export\s+)?import\s+([\w.:]+);', module_map[name].read_text(encoding='utf-8-sig'), re.M))
        modules = ' '.join('"' + module_map[name].as_posix() + '"' for name in sorted(closure))
        bases = ' '.join('"' + path.as_posix() + '"' for path in owners)
        sources = ('Engine/src/render/render.preview_camera.cpp', 'Engine/src/render/render.preview_lighting.cpp', 'Engine/src/assets/texture/asset.texture_artifact.cpp',
                   'Engine/src/authoring/texture/authoring.texture_document.cpp', 'Engine/src/authoring/texture/authoring.texture_residency.cpp')
        source_list = ' '.join('"' + (ROOT / path).as_posix() + '"' for path in sources)
        (proof / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetQueuedCancel LANGUAGES CXX)
add_executable(cancel_contract runtime.cancel_contract.cpp {source_list})
target_sources(cancel_contract PRIVATE FILE_SET CXX_MODULES BASE_DIRS {bases} FILES {modules})
target_include_directories(cancel_contract PRIVATE "{ROOT.as_posix()}/Engine/include")
target_compile_features(cancel_contract PRIVATE cxx_std_23)
include("{ROOT.as_posix()}/Engine/cmake/epoch_task_scheduler.cmake")
epoch_admit_task_scheduler(cancel_contract "{ROOT.as_posix()}")
if(MSVC)
 target_compile_options(cancel_contract PRIVATE /sdl /permissive- /utf-8 /Zc:__cplusplus /EHsc)
endif()
enable_testing()
add_test(NAME queued_cancel COMMAND cancel_contract)
set_tests_properties(queued_cancel PROPERTIES TIMEOUT 30)
''', encoding='utf-8')
        configure = ['cmake', '-S', str(proof), '-B', str(proof / 'cmake')]
        configure += (['-G', 'Visual Studio 17 2022', '-A', 'x64'] if os.name == 'nt'
                      else ['-G', 'Ninja Multi-Config'])
        commands = [('configure', configure)]
        for mode in ('Debug', 'Release'):
            commands += [(mode + '-build', ['cmake', '--build', str(proof / 'cmake'), '--config', mode, '--parallel', '1']),
                         (mode + '-test', ['ctest', '--test-dir', str(proof / 'cmake'), '-C', mode, '--output-on-failure'])]
        records = []
        env = dict(os.environ, MSBUILDDISABLENODEREUSE='1', TEMP=str(proof / 'tmp'), TMP=str(proof / 'tmp'))
        (proof / 'tmp').mkdir(exist_ok=True)
        for label, command in commands:
            result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
            (proof / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
            records.append(dict(phase=label, exit=result.returncode, command=command))
            paths = ('Engine/src/epoch/implementation/epoch.project_play_scene.inl',
                     'Engine/src/editor/application/editor.application_packages_creation.inl')
            (proof / 'result.json').write_text(json.dumps(dict(native=False, phases=records,
                sources={path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in paths}), indent=2) + '\n')
            self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-8000:])


if __name__ == '__main__':
    unittest.main()
