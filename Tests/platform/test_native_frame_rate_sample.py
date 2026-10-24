"""Actual shared render-frame sampling calculation; no window or GPU starts."""
from Tools.source.owned_source_text import read_owned_source
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NativeFrameRateSample(unittest.TestCase):
    def test_native_counter_calculation_for_both_editions(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        contract = r'''
#include "epoch.edition.hpp"
#include "host.frame_rate_sample.hpp"
#include <limits>
#define CHECK(...) do { if(!(__VA_ARGS__)) return __LINE__; } while(false)
bool close(double a,double b) { return std::abs(a-b)<1e-8; }
int main() {
    using epochengine::renderhost::frame_rate::Sample;
    Sample measured;
    CHECK(!measured.observe(100,0) && !measured.has_sample());
    CHECK(!measured.observe(110,0.5) && !measured.has_sample());
    CHECK(measured.observe(220,1) && measured.has_sample());
    CHECK(close(measured.live_rate(),120) && close(measured.average_rate(),120));
    CHECK(measured.observe(460,2));
    CHECK(close(measured.live_rate(),240) && close(measured.average_rate(),180));
    CHECK(measured.observe(760,3));
    CHECK(measured.observe(1160,4));
    CHECK(measured.observe(1660,5));
    CHECK(measured.observe(2260,6));
    CHECK(close(measured.live_rate(),600) && close(measured.average_rate(),408));

    // Different interval lengths are time-weighted, never averaged as if each
    // reported rate had equal duration.
    Sample variable;
    CHECK(!variable.observe(0,0));
    CHECK(variable.observe(150,1.5));
    CHECK(variable.observe(450,2.5));
    CHECK(close(variable.live_rate(),300) && close(variable.average_rate(),180));
    CHECK(variable.observe(890,3.6));
    CHECK(close(variable.live_rate(),400) && close(variable.average_rate(),890.0/3.6));
    CHECK(variable.observe(1290,5.6));
    CHECK(close(variable.average_rate(),(90.0+300.0+440.0+400.0)/5.0));

    // Thousands of UI polls must not become thousands of frames.
    Sample producer;
    CHECK(!producer.observe(0,0));
    for(unsigned ui=1;ui<=2000;++ui)
        (void)producer.observe(ui/10,static_cast<double>(ui)/1000.0);
    CHECK(close(producer.live_rate(),100) && close(producer.average_rate(),100));
    CHECK(producer.observe(200,3));
    CHECK(producer.live_rate()==0 && producer.has_sample());

    // A long stall dilutes actual work, drops expired history, and cannot keep
    // a stale high-rate interval or fabricate a catch-up frame count.
    CHECK(producer.observe(201,33));
    CHECK(close(producer.live_rate(),1.0/30.0));
    CHECK(close(producer.average_rate(),1.0/30.0));
    CHECK(producer.observe(202,34));
    CHECK(close(producer.average_rate(),(4.0/30.0+1.0)/5.0));

    Sample high;
    constexpr std::uint64_t base=1ull<<40;
    CHECK(!high.observe(base,0));
    CHECK(high.observe(base+1'000'000,1));
    CHECK(close(high.live_rate(),1'000'000) && close(high.average_rate(),1'000'000));
    CHECK(!high.observe(2,2) && !high.has_sample()); // counter reset
    CHECK(high.observe(62,3) && close(high.live_rate(),60));
    int first{}, second{};
    CHECK(!high.observe(62,3,&first) && !high.has_sample());
    CHECK(high.observe(182,4,&first) && close(high.live_rate(),120));
    CHECK(!high.observe(182,4,&second) && !high.has_sample());
    CHECK(!high.observe(182,3,&second)); // time reset, not a negative rate
    CHECK(!high.observe(999,std::numeric_limits<double>::quiet_NaN(),&second));
    CHECK(high.observe(242,4,&second) && close(high.live_rate(),60));
    return 0;
}
'''
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='native-frame-rate-', dir=parent))
        try:
            toolset, kit = toolsets[-1], kits[-1]
            for edition, value in (('development', 0), ('public', 1)):
                lane = scratch / edition
                lane.mkdir()
                marker = read_owned_source(ROOT / 'Engine/include/epoch.edition.hpp', encoding='utf-8')
                (lane / 'epoch.edition.hpp').write_text(marker.replace(
                    '#define EPOCH_SOURCE_EDITION_PUBLIC 0', '#define EPOCH_SOURCE_EDITION_PUBLIC ' + str(value)), encoding='utf-8')
                source = lane / 'contract.cpp'
                source.write_text(contract, encoding='utf-8')
                binary = lane / 'contract.exe'
                args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX',
                    '/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'),
                    '/I' + str(ROOT / 'Engine/src/renderers/host'), '/Fo' + str(lane / 'contract.obj'), '/Fe' + str(binary),
                    str(source), '/link', '/LIBPATH:' + str(toolset / 'lib/x64'),
                    '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'), '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
                result = subprocess.run(args, cwd=lane, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout[-3500:] + result.stderr)
                result = subprocess.run([str(binary)], cwd=lane, capture_output=True, text=True)
                lines = contract.splitlines()
                failure = lines[result.returncode - 1] if 0 < result.returncode <= len(lines) else ''
                self.assertEqual(result.returncode, 0, edition + ' native frame rate: ' + failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_both_surfaces_observe_native_completions_without_ui_override(self):
        editor = read_owned_source(ROOT / 'Engine/src/editor/editor.application.cpp', encoding='utf-8')
        badge = editor.split('if (ctx->windowData)\n        {\n            const double chromeNow', 1)[1].split('const double chromeThrottle', 1)[0]
        self.assertIn('successfulFrameGeneration.load(std::memory_order_acquire)', badge)
        self.assertIn('chromeFrameRate.live_rate()', badge)
        self.assertNotIn('++', badge)
        self.assertNotIn('editorFramesPerSecond', editor)
        for platform in ('win', 'linux'):
            host = read_owned_source(ROOT / ('Engine/src/renderers/host/host.context_' + platform + '.cpp'), encoding='utf-8')
            sampler = host.split('void record_native_title_frame(', 1)[1].split('void forget_native_title_frame_source', 1)[0]
            self.assertIn('window.successfulFrameGeneration.load(std::memory_order_acquire)', sampler)
            self.assertIn('state.frameRate.average_rate()', sampler)
            self.assertNotIn('editorFramesPerSecond', sampler)
            self.assertNotIn('++state.frameCount', sampler)
            self.assertIn('L" FPS avg"', host)
            self.assertLess(host.rindex('win.successfulFrameGeneration.fetch_add'), host.rindex('record_native_title_frame('))


if __name__ == '__main__':
    unittest.main()
