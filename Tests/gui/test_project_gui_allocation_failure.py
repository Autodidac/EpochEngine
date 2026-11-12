"""Allocation refusal cannot mutate or terminate the actual GUI session focus APIs."""
from pathlib import Path
from Tests.support.cpp import engine_module_source
import hashlib
import json
import os
import subprocess
import unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
CODE=r'''#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
static bool deny_allocations{};
static std::size_t allocation_attempts{};
void* operator new(std::size_t bytes){
 // MSVC's noexcept default string constructor allocates this Debug-only
 // bookkeeping proxy. Its own OOM terminates even outside an Engine call.
 // Deny actual event-vector/string payload allocations, not that STL proxy.
 bool metadata=false;
#if _ITERATOR_DEBUG_LEVEL != 0
 metadata=bytes==sizeof(std::_Container_proxy);
#endif
 if(deny_allocations&&!metadata){++allocation_attempts;throw std::bad_alloc{};}
 if(auto* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc{};}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
import asset.gui_artifact;
import project.gui_runtime;
namespace asset=epochengine::asset;
namespace rt=epochengine::project_gui_runtime;
namespace fixture{
HELPERS

}
int main(int argc,char**argv){
 if(argc!=2)return 2;const std::string_view mode{argv[1]};
 std::set_terminate([]{std::fputs("TERMINATED: allocation escaped noexcept owner\n",stderr);std::_Exit(92);});
 auto source=fixture::artifact();
 if(mode=="activate_long"){source.widgets[2].action=std::string(256,'x');source.identity.key=asset::gui::payload_content(source);}
 rt::RuntimeSession runtime{std::move(source)};if(!runtime.valid()||runtime.focus({3})!=rt::RuntimeCode::ready)return 3;
 if(mode!="retained_capacity")(void)runtime.drain_events();
 const auto before=runtime.metrics();deny_allocations=true;
 const auto result=mode=="focus"?runtime.focus({4}):mode=="activate_long"||mode=="activate_short"?runtime.activate_focused():runtime.clear_focus();
 deny_allocations=false;
 const auto after=runtime.metrics();
 std::fprintf(stderr,"mode=%.*s result=%.*s focus=%llu focus_changes=%llu->%llu allocations=%zu\n",int(mode.size()),mode.data(),
  int(rt::runtime_code_name(result).size()),rt::runtime_code_name(result).data(),
  static_cast<unsigned long long>(runtime.focused_widget().value),static_cast<unsigned long long>(before.focus_changes),static_cast<unsigned long long>(after.focus_changes),allocation_attempts);
 if(mode=="retained_capacity"&&result==rt::RuntimeCode::ready)
  return allocation_attempts==0&&runtime.focused_widget().value==0?0:83;
 if(result!=rt::RuntimeCode::allocation_failure||allocation_attempts==0)return 84;
 if(runtime.focused_widget().value!=3||after.focus_changes!=before.focus_changes||after.actions_emitted!=before.actions_emitted)return 81;
 const auto retained=runtime.drain_events();
 if(retained.size()!=(mode=="retained_capacity"?1u:0u))return 85;
 return 0;
}
'''

class ProjectGuiAllocationFailure(unittest.TestCase):
    def test_actual_focus_failure_atomicity_debug_release(self):
        if os.name!="nt":self.skipTest("Installed MSVC CPU module consumer")
        contract=(ROOT/"Engine/src/project/project.gui_runtime_contract.cpp").read_text(encoding="utf-8")
        helpers="\n".join(definition(contract,m) for m in (
            "        [[nodiscard]] asset::gui::CompiledWidget widget(",
            "        [[nodiscard]] asset::gui::CompiledGuiArtifact artifact()"))
        code=CODE.replace("HELPERS",helpers)
        owner=ROOT/"Engine/src/project/project.gui_runtime.cpp"
        identity=hashlib.sha256(code.encode()+owner.read_bytes()).hexdigest()[:12]
        lane=ROOT/"Artifacts/Reports/project-gui-allocation-oct28"/identity;lane.mkdir(parents=True,exist_ok=True)
        (lane/"fault.cpp").write_text(code,encoding="utf-8")
        modules=[engine_module_source(ROOT/"Engine/modules", n) for n in ("core.sha256.ixx","asset.gui_artifact.ixx","project.gui_runtime.ixx")]
        sources=[ROOT/"Engine/src/assets/gui/asset.gui_artifact.cpp",owner]
        (lane/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.30)\nproject(ProjectGuiAllocation LANGUAGES CXX)\n'
            +'add_executable(fault fault.cpp '+" ".join('"'+p.as_posix()+'"' for p in sources)+')\n'
            +'target_compile_features(fault PRIVATE cxx_std_23)\ntarget_compile_options(fault PRIVATE /W3 /WX /utf-8 /EHsc)\n'
            +'target_sources(fault PRIVATE FILE_SET CXX_MODULES BASE_DIRS "'+ROOT.as_posix()+'" FILES '+" ".join('"'+p.as_posix()+'"' for p in modules)+')\n',encoding="utf-8")
        build=ROOT/"Artifacts/Intermediate/cmake/project-gui-allocation"/identity
        runs=[];cases=[]
        commands=[["cmake","-S",str(lane),"-B",str(build),"-G","Visual Studio 17 2022","-A","x64"]]
        for config in ("Debug","Release"):commands.append(["cmake","--build",str(build),"--config",config,"--parallel","1"])
        for index,command in enumerate(commands):
            r=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=120)
            (lane/f"compile-{index}.log").write_text(r.stdout+r.stderr,encoding="utf-8");runs.append({"command":command,"exit":r.returncode})
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        for config in ("Debug","Release"):
            for mode in ("retained_capacity","clear","focus","activate_short","activate_long"):
                r=subprocess.run([str(build/config/"fault.exe"),mode],cwd=lane,capture_output=True,text=True,timeout=10)
                (lane/f"{config}-{mode}.log").write_text(r.stdout+r.stderr,encoding="utf-8")
                cases.append({"configuration":config,"mode":mode,"exit":r.returncode,"retired":True,"evidence":r.stderr})
                (lane/"result.json").write_text(json.dumps({"passed":r.returncode==0 and len(cases)==10,
                    "scope":"Actual RuntimeSession/asset producer with event-vector/string payload allocation denial. MSVC Debug-only iterator bookkeeping proxies are permitted because std::string default construction is noexcept inside the STL. No GUI/model invocation.",
                    "before_failure_receipt":"Artifacts/Reports/project-gui-keyboard-oct28/allocation-proof/before-result.json",
                    "owner_sha256":hashlib.sha256(owner.read_bytes()).hexdigest(),"commands":runs,"cases":cases},indent=2),encoding="utf-8")
                self.assertEqual(r.returncode,0,r.stdout+r.stderr)

if __name__=="__main__":unittest.main()
