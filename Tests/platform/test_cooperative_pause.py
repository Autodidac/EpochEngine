"""Compile the actual platform pause with deterministic OS mocks; no real wait."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def definition(source, beginning):
    start = source.index(beginning)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class CooperativePause(unittest.TestCase):
    def test_actual_windows_handle_lifetime_and_bounded_failure(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC CPU lane')
        toolsets = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*'))
        kitroot = Path('C:/Program Files (x86)/Windows Kits/10')
        kits = sorted((kitroot / 'Include').glob('*'))
        if not toolsets or not kits:
            self.skipTest('MSVC/SDK unavailable')
        production = (ROOT / 'Engine/dep/EpochPlatformEngine/src/platform/platform.window.cpp').read_text(encoding='utf-8')
        timer = definition(production, 'class CooperativePauseTimer final') + ';'
        pause = definition(production, 'void cooperative_pause() noexcept')
        contract = r'''
#include "epoch.edition.hpp"
#include <thread>
#include <type_traits>
using HANDLE=void*;
struct LARGE_INTEGER { long long QuadPart; };
constexpr unsigned TIMER_MODIFY_STATE=2, SYNCHRONIZE=0x100000;
constexpr unsigned WAIT_OBJECT_0=0, WAIT_TIMEOUT=258, WAIT_FAILED=0xffffffff;
constexpr int FALSE=0;
#define CHECK(...) do { if(!(__VA_ARGS__)) return __LINE__; } while(false)
namespace mock {
unsigned creates{},sets{},waits{},sleeps{},cancels{},closes{},warnings{};
unsigned result=WAIT_OBJECT_0;
bool noCreate{},failHigh{},failArm{},badArguments{};
int handle;
void reset() {
    creates=sets=waits=sleeps=cancels=closes=warnings=0;
    result=WAIT_OBJECT_0; noCreate=failHigh=failArm=badArguments=false;
}
}
HANDLE CreateWaitableTimerExW(void* attributes,void* name,unsigned flags,unsigned access) {
    ++mock::creates;
    if(attributes || name || access!=(TIMER_MODIFY_STATE|SYNCHRONIZE)) mock::badArguments=true;
    return mock::noCreate || (mock::failHigh && flags) ? nullptr : &mock::handle;
}
int SetWaitableTimer(HANDLE h,const LARGE_INTEGER* due,int period,void* callback,void* arg,int resume) {
    ++mock::sets;
    if(h!=&mock::handle || due->QuadPart!=-1 || period || callback || arg || resume) mock::badArguments=true;
    return !mock::failArm;
}
unsigned WaitForSingleObject(HANDLE h,unsigned milliseconds) {
    ++mock::waits;
    if(h!=&mock::handle || milliseconds!=2) mock::badArguments=true;
    return mock::result;
}
int CancelWaitableTimer(HANDLE h) { ++mock::cancels; if(h!=&mock::handle) mock::badArguments=true; return 1; }
int CloseHandle(HANDLE h) { ++mock::closes; if(h!=&mock::handle) mock::badArguments=true; return 1; }
void Sleep(unsigned milliseconds) { ++mock::sleeps; if(milliseconds!=1) mock::badArguments=true; }
namespace epochengine::core::log {
void warn(const char*,const char*) { ++mock::warnings; }
}
namespace epochengine::platform {
'''
        main = r'''
}
int main() {
    using epochengine::platform::CooperativePauseTimer;
    static_assert(!std::is_copy_constructible_v<CooperativePauseTimer>);
    static_assert(!std::is_copy_assignable_v<CooperativePauseTimer>);
    mock::reset();
    {
        CooperativePauseTimer timer;
        for(unsigned i=0;i<1000;++i) CHECK(timer.pause());
        CHECK(mock::creates==1 && mock::sets==1000 && mock::waits==1000);
        CHECK(mock::sleeps==0 && mock::closes==0 && !mock::badArguments);
    }
    CHECK(mock::closes==1 && mock::cancels==1);

    mock::reset(); mock::failHigh=true;
    {
        CooperativePauseTimer timer;
        CHECK(timer.pause());
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
        CHECK(mock::creates==2);
#else
        CHECK(mock::creates==1);
#endif
    }
    CHECK(mock::closes==1 && mock::sleeps==0);

    mock::reset(); mock::noCreate=true;
    {
        CooperativePauseTimer timer;
        const unsigned initial=mock::creates;
        for(unsigned i=0;i<1000;++i) CHECK(!timer.pause());
        CHECK(mock::creates==initial && mock::sleeps==1000);
        CHECK(mock::sets==0 && mock::waits==0);
    }
    CHECK(mock::closes==0 && !mock::badArguments);

    for(unsigned failure : {WAIT_TIMEOUT,WAIT_FAILED}) {
        mock::reset(); mock::result=failure;
        {
            CooperativePauseTimer timer;
            CHECK(!timer.pause() && !timer.pause());
            CHECK(mock::creates==1 && mock::sets==1 && mock::waits==1);
            CHECK(mock::sleeps==2 && mock::closes==1 && mock::cancels==1);
        }
        CHECK(mock::closes==1 && !mock::badArguments);
    }
    mock::reset(); mock::failArm=true;
    {
        CooperativePauseTimer timer;
        CHECK(!timer.pause() && !timer.pause());
        CHECK(mock::creates==1 && mock::sets==1 && mock::waits==0);
        CHECK(mock::sleeps==2 && mock::closes==1);
    }
    CHECK(mock::closes==1 && !mock::badArguments);

    // The actual exported wrapper owns one timer per calling thread and
    // releases it at thread exit; neither pause path creates a worker.
    mock::reset();
    std::thread([] { for(unsigned i=0;i<1000;++i) epochengine::platform::cooperative_pause(); }).join();
    CHECK(mock::creates==1 && mock::sets==1000 && mock::waits==1000);
    CHECK(mock::closes==1 && mock::warnings==0);
    mock::reset(); mock::noCreate=true;
    std::thread([] { for(unsigned i=0;i<1000;++i) epochengine::platform::cooperative_pause(); }).join();
    CHECK(mock::sleeps==1000 && mock::sets==0 && mock::waits==0);
    CHECK(mock::warnings==1 && mock::closes==0 && !mock::badArguments);
    return 0;
}
'''
        parent = (ROOT / 'build/contract-tests').resolve()
        parent.mkdir(parents=True, exist_ok=True)
        scratch = Path(tempfile.mkdtemp(prefix='cooperative-pause-', dir=parent))
        try:
            toolset, kit = toolsets[-1], kits[-1]
            for edition, value in (('development', 0), ('public', 1)):
                for high_resolution in (False, True):
                    lane = scratch / (edition + ('-high' if high_resolution else '-baseline-sdk'))
                    lane.mkdir()
                    marker = (ROOT / 'Engine/include/epoch.edition.hpp').read_text(encoding='utf-8')
                    (lane / 'epoch.edition.hpp').write_text(marker.replace(
                        '#define EPOCH_SOURCE_EDITION_PUBLIC 0', '#define EPOCH_SOURCE_EDITION_PUBLIC ' + str(value)), encoding='utf-8')
                    source = lane / 'contract.cpp'
                    source.write_text(contract + timer + '\n' + pause + main, encoding='utf-8')
                    binary = lane / 'contract.exe'
                    args = [str(toolset / 'bin/Hostx64/x64/cl.exe'), '/nologo', '/std:c++latest', '/EHsc', '/WX', '/D_WIN32']
                    if high_resolution:
                        args += ['/DCREATE_WAITABLE_TIMER_HIGH_RESOLUTION=2']
                    args += ['/I' + str(toolset / 'include'), '/I' + str(kit / 'ucrt'), '/I' + str(kit / 'shared'), '/I' + str(kit / 'um'),
                        '/Fo' + str(lane / 'contract.obj'), '/Fe' + str(binary), str(source), '/link',
                        '/LIBPATH:' + str(toolset / 'lib/x64'), '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'um/x64'),
                        '/LIBPATH:' + str(kitroot / 'Lib' / kit.name / 'ucrt/x64')]
                    result = subprocess.run(args, cwd=lane, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout[-3500:] + result.stderr)
                    result = subprocess.run([str(binary)], cwd=lane, capture_output=True, text=True)
                    lines = source.read_text(encoding='utf-8').splitlines()
                    failure = lines[result.returncode - 1] if 0 < result.returncode <= len(lines) else ''
                    self.assertEqual(result.returncode, 0, lane.name + ': ' + failure)
        finally:
            self.assertTrue(scratch.resolve().is_relative_to(parent))
            shutil.rmtree(scratch)

    def test_portable_positive_pause_stays_in_existing_platform_owner(self):
        source = (ROOT / 'Engine/dep/EpochPlatformEngine/src/platform/platform.window.cpp').read_text(encoding='utf-8')
        wrapper = definition(source, 'void cooperative_pause() noexcept')
        self.assertIn('std::this_thread::sleep_for(std::chrono::nanoseconds{ 1 });', wrapper)
        self.assertIn('thread_local CooperativePauseTimer timer;', wrapper)
        self.assertNotIn('INFINITE', wrapper)
        self.assertNotIn('timeBeginPeriod', source)
        self.assertNotIn('std::thread(', source)
        self.assertNotIn('CreateThread', source)


if __name__ == '__main__':
    unittest.main()
