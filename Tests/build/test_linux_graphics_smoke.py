"""Runner failure/retirement contracts; fixture processes are not graphics proof."""
import os
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

from Tools.build import run_linux_graphics_smoke as smoke
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]
READY = "[INFO] Context.Linux: Backend OpenGL reached render-ready state for hwnd=0x200002.\n"
DONE = "[INFO] Engine.Editor: " + smoke.COMPLETE + "\n"


class LinuxGraphicsSmoke(unittest.TestCase):
    def setUp(self):
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        self.scratch = tempfile.TemporaryDirectory(dir=parent, prefix="linux-graphics-runner-")
        self.addCleanup(self.scratch.cleanup)
        self.folder = Path(self.scratch.name)

    def test_requires_real_provider_ready_then_frame_budget_and_normal_exit(self):
        accepted = smoke.classify(READY + DONE, 0, False)
        self.assertEqual(accepted["result"], "pass")
        for key in ("hardware_parity", "pixel_parity", "input_parity"):
            self.assertIs(accepted[key], False)
        for text, code, timed_out in (
            ("", 0, False),
            (READY, 0, False),
            (DONE, 0, False),
            (DONE + READY, 0, False),
            ((READY + DONE).replace("OpenGL", "SDL"), 0, False),
            (READY + READY + DONE, 0, False),
            (READY + DONE, 7, False),
            (READY + DONE, 0, True),
        ):
            with self.subTest(text=text, code=code, timed_out=timed_out):
                self.assertEqual(smoke.classify(text, code, timed_out)["result"], "fail")

    def test_rejects_automatic_updater_requests_even_after_bounded_completion(self):
        requests = ('[INFO] Updater: Downloading: https://example.invalid/integrity.json -> cache\n'
                    '[INFO] Updater: Downloading: https://example.invalid/version.ixx -> cache\n')
        result=smoke.classify(READY+requests+DONE,0,False)
        self.assertEqual(result['result'],'fail')
        self.assertEqual(result['automatic_update_requests'],2)
        self.assertEqual(smoke.classify(READY+DONE,0,False)['automatic_update_requests'],0)

    def test_actual_editor_smoke_suppresses_only_automatic_discovery(self):
        lifecycle=(ROOT/'Engine/src/editor/application/editor.application_context_lifecycle.inl').read_text(encoding='utf-8')
        suppress=definition(lifecycle,'    void editor_suppress_startup_update_check(')
        bootstrap=(ROOT/'Engine/src/editor/application/editor.frame_bootstrap.inl').read_text(encoding='utf-8')
        queued=definition(bootstrap,'        if (editor.autoUpdateCheckQueued)')
        source=r'''
#include <cassert>
#include <memory>
#include <optional>
namespace core { struct Context{}; namespace cli { bool smoke_requested{}; } }
enum class EditorUpdateState { Idle, SourceWorkerRunning, RestartReady };
struct EditorState { bool autoUpdateCheckQueued{true},showUpdateConfirmModal{true},showSourceUpdateConfirmModal{true};
 std::optional<int> updateCheckPending; EditorUpdateState updateState{EditorUpdateState::Idle}; };
EditorState state; unsigned requests{};
EditorState& editor_state_for(const std::shared_ptr<core::Context>&) {return state;}
void start_editor_update_check(EditorState&,bool quiet) { assert(quiet); ++requests; }
'''+suppress+'\nvoid pump_automatic(EditorState& editor) {\n'+queued+r'''
}
int main() {
 auto context=std::make_shared<core::Context>();
 core::cli::smoke_requested=false;
 editor_suppress_startup_update_check(context);
 assert(state.autoUpdateCheckQueued && !state.showUpdateConfirmModal && !state.showSourceUpdateConfirmModal);
 pump_automatic(state);assert(requests==1 && !state.autoUpdateCheckQueued);
 pump_automatic(state);assert(requests==1);
 core::cli::smoke_requested=true;
 editor_suppress_startup_update_check(context);assert(!state.autoUpdateCheckQueued);
 pump_automatic(state);assert(requests==1);
 // Fresh initial state and restored snapshots may queue again; bootstrap still blocks smoke discovery.
 state.autoUpdateCheckQueued=true;pump_automatic(state);assert(requests==1 && !state.autoUpdateCheckQueued);
 // Never cancel or replace an already-owned operation merely because a smoke flag was set.
 for(auto mode:{EditorUpdateState::Idle,EditorUpdateState::SourceWorkerRunning,EditorUpdateState::RestartReady}) {
  state.updateState=mode;state.updateCheckPending=7;state.showUpdateConfirmModal=state.showSourceUpdateConfirmModal=true;
  editor_suppress_startup_update_check(context);
  assert(state.updateCheckPending==7 && state.updateState==mode);
  assert(state.showUpdateConfirmModal && state.showSourceUpdateConfirmModal && !state.autoUpdateCheckQueued);
 }
 // Ordinary product startup remains automatic. Explicit update calls are untouched in both modes.
 core::cli::smoke_requested=false;state.autoUpdateCheckQueued=true;pump_automatic(state);assert(requests==2);
 start_editor_update_check(state,true);assert(requests==3);
 core::cli::smoke_requested=true;start_editor_update_check(state,true);assert(requests==4);
 editor_suppress_startup_update_check({});assert(requests==4);
}
'''
        report=ROOT/'build/contract-tests/linux-smoke-updater';report.mkdir(parents=True,exist_ok=True)
        path=report/'contract.cpp';path.write_text(source,encoding='utf-8')
        compile_and_run(self,path,report)

    def test_exact_argv_handles_spaces_and_metacharacters_without_shell(self):
        evidence = self.folder / "evidence spaces; literal"
        evidence.mkdir()
        marker = self.folder / "must-not-exist"
        payload = "argument with spaces; & echo no $(touch " + str(marker) + ")"
        argv = [sys.executable, "-c", "import sys; print(sys.argv[1])", payload]
        log, code, timed_out, elapsed, samples = smoke._run_probe(
            argv, self.folder, evidence, 5, dict(os.environ))
        self.assertEqual(code, 0)
        self.assertFalse(timed_out)
        self.assertEqual(log.read_text().strip(), payload)
        self.assertFalse(marker.exists())
        self.assertGreaterEqual(elapsed, 0)

    def test_timeout_does_not_accept_printed_success_and_reaps_owned_process(self):
        evidence = self.folder / "timeout"
        evidence.mkdir()
        pid_file = self.folder / "pid"
        fixture = self.folder / "slow fixture.py"
        fixture.write_text("import os,sys,time\n"
                           "from pathlib import Path\n"
                           "Path(sys.argv[1]).write_text(str(os.getpid()))\n"
                           f"print({(READY + DONE)!r}, flush=True)\n"
                           "time.sleep(20)\n", encoding="utf-8")
        started = time.monotonic()
        log, code, timed_out, elapsed, samples = smoke._run_probe(
            [sys.executable, str(fixture), str(pid_file)], self.folder,
            evidence, 1, dict(os.environ))
        self.assertTrue(timed_out)
        self.assertLess(time.monotonic() - started, 8)
        self.assertNotEqual(code, 0)
        self.assertEqual(smoke.classify(log.read_text(), code, timed_out)["result"], "fail")
        if sys.platform == "linux":
            self.assertFalse(Path("/proc/" + pid_file.read_text()).exists())

    def test_preflight_refuses_runtime_local_evidence_before_launch(self):
        # ELF check is later: this ensures unsafe evidence paths never start a child.
        binary = self.folder / "epoch"
        binary.write_bytes(b"fixture")
        with self.assertRaisesRegex(ValueError, "outside the runtime"):
            smoke.run_smoke(binary, self.folder / "evidence")

    def test_preflight_accepts_staged_package_and_report_evidence_paths(self):
        release = self.folder / "Artifacts/Packages/public/v0.91.02"
        runtime = release / "epoch_linux_x64_v0.91.02"
        runtime.mkdir(parents=True)
        binary = runtime / "epoch"
        binary.write_bytes(b"fixture")
        with patch.object(smoke, "ROOT", self.folder.resolve()):
            for evidence in (release / "verify_epoch_linux_logs/graphics",
                             self.folder / "Artifacts/Reports/linux-graphics"):
                with self.subTest(evidence=evidence):
                    with self.assertRaisesRegex(ValueError, "Expected a Linux"):
                        smoke.run_smoke(binary, evidence)
            with self.assertRaisesRegex(ValueError, "outside the runtime"):
                smoke.run_smoke(binary, runtime / "graphics")
            with self.assertRaisesRegex(ValueError, "Smoke evidence must stay"):
                smoke.run_smoke(binary, self.folder / "source/graphics")

    def test_streaming_hash_handles_multiple_blocks(self):
        payload = bytes(range(256)) * 9000
        source = self.folder / "hash-input"
        source.write_bytes(payload)
        self.assertEqual(smoke.sha256_file(source), hashlib.sha256(payload).hexdigest())

    def test_each_linux_provider_requires_its_own_readiness(self):
        for renderer, token in (("opengl", "OpenGL"), ("sdl", "SDL"), ("sfml", "SFML"),
                                ("raylib", "RayLib"), ("vulkan", "Vulkan"), ("software", "Software")):
            with self.subTest(renderer=renderer):
                own_ready = READY.replace("OpenGL", token)
                self.assertEqual(smoke.classify(own_ready + DONE, 0, False, renderer)["result"], "pass")
                fallback = READY.replace("OpenGL", "DirectX")
                self.assertEqual(smoke.classify(fallback + DONE, 0, False, renderer)["result"], "fail")
                self.assertEqual(smoke.classify(fallback + own_ready + DONE, 0, False, renderer)["result"], "fail")
        with self.assertRaises(ValueError):
            smoke.classify(READY + DONE, 0, False, "directx")

    def test_software_vulkan_requires_installed_matching_cpu_icd(self):
        with self.assertRaisesRegex(ValueError, "mesa-vulkan-drivers"):
            smoke.lavapipe_icd((self.folder,))
        icd = self.folder / "lvp_icd.json"
        icd.write_text('{"ICD":{"library_path":"libvulkan_lvp.so"}}')
        selected, library = smoke.lavapipe_icd((self.folder,))
        self.assertEqual(selected, icd.resolve())
        self.assertEqual(library, "libvulkan_lvp.so")
        icd.write_text('{"ICD":{"library_path":"hardware-driver.so"}}')
        with self.assertRaisesRegex(ValueError, "CPU provider"):
            smoke.lavapipe_icd((self.folder,))

    def test_software_vulkan_pins_loader_and_exact_renderer_argv(self):
        runtime = self.folder / "runtime"
        font = runtime / "assets/fonts/Roboto-Regular.ttf"
        font.parent.mkdir(parents=True)
        font.write_bytes(b"fixture")
        binary = runtime / "epoch"
        binary.write_bytes(b"\x7fELF\x02\x01fixture")
        icd = self.folder / "lvp_icd.json"
        icd.write_text('{"ICD":{"library_path":"libvulkan_lvp.so"}}')
        observed_data = []

        def fixture_probe(argv, cwd, evidence, timeout, environment):
            runtime_data = evidence / "runtime-data"
            self.assertEqual(argv, [str(binary.resolve()), "--editor", "--renderer", "vulkan", "--smoke",
                                    "--candidate-data-root", str(runtime_data)])
            self.assertTrue(runtime_data.is_dir())
            self.assertEqual(list(runtime_data.iterdir()), [runtime_data / "logs"])
            self.assertEqual(environment["EPOCH_LOG_DIR"], str(runtime_data / "logs"))
            self.assertEqual(cwd, binary.parent.resolve())
            observed_data.append(runtime_data)
            self.assertEqual(environment["VK_DRIVER_FILES"], str(icd))
            self.assertEqual(environment["VK_ICD_FILENAMES"], str(icd))
            log = evidence / "editor-output.log"
            log.write_text(READY.replace("OpenGL", "Vulkan") + DONE)
            return log, 0, False, 0.01, []

        with patch.object(smoke, "lavapipe_icd", return_value=(icd, "libvulkan_lvp.so")), \
             patch.object(smoke, "_run_probe", side_effect=fixture_probe):
            receipt, evidence = smoke.run_smoke(binary, self.folder / "evidence",
                                               renderer="vulkan", software_vulkan=True)
            # A later probe must not consume even this earlier probe's state.
            (observed_data[0] / "saved-model-settings.json").write_text("fixture")
            later, _ = smoke.run_smoke(binary, self.folder / "evidence",
                                      renderer="vulkan", software_vulkan=True)
        self.assertEqual(receipt["result"], "pass")
        self.assertEqual(later["result"], "pass")
        self.assertEqual(len(set(observed_data)), 2)
        self.assertEqual(receipt["runtime_data_root"], str(observed_data[0]))
        self.assertIn("not OS confinement", receipt["data_boundary"])
        self.assertTrue(receipt["software_vulkan_requested"])
        self.assertFalse(receipt["hardware_parity"])
        self.assertEqual(receipt["vulkan_icd_sha256"], smoke.sha256_file(icd))

    def test_cmake_registers_real_opt_in_linux_runner(self):
        cmake = (ROOT / "Engine/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('option(EPOCH_ENABLE_GRAPHICS_TESTS ', cmake)
        option = next(line for line in cmake.splitlines()
                      if line.startswith('option(EPOCH_ENABLE_GRAPHICS_TESTS '))
        self.assertTrue(option.endswith(' OFF)'))
        gate = 'if(EPOCH_ENABLE_GRAPHICS_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")'
        block = cmake.split(gate, 1)[1].split('        endforeach()', 1)[0]
        for required in ('foreach(_epoch_linux_renderer IN ITEMS opengl sdl sfml raylib vulkan software)',
                         'if(NOT EPOCH_${_epoch_renderer_option}_ACTIVE)',
                         'add_test(NAME epoch_linux_${_epoch_linux_renderer}_smoke', 'xvfb-run REQUIRED',
                         'Tools/build/run_linux_graphics_smoke.py',
                         '--binary "$<TARGET_FILE:epoch>"',
                         '--renderer "${_epoch_linux_renderer}"',
                         'set(_epoch_renderer_flags --software-opengl)',
                         'set(_epoch_renderer_flags --software-vulkan)',
                         '${_epoch_renderer_flags} --timeout-seconds 30',
                         'TIMEOUT 45 RUN_SERIAL TRUE',
                         'LABELS "graphics;linux;${_epoch_renderer_label}"',
                         '"$<TARGET_FILE_DIR:epoch>/assets"'):
            self.assertIn(required, block)


if __name__ == "__main__":
    unittest.main()
