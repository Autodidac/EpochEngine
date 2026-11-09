"""Actual central Editor tick lambda and Platform simulation-clock implementation."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib,json,subprocess,unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

class SimulationSingleStep(unittest.TestCase):
 def test_actual_host_request_reaches_clock_once_debug_release(self):
  module=(ROOT/'Engine/dep/EpochPlatformEngine/modules/core.time.ixx').read_text(encoding='utf-8')
  source=(ROOT/'Engine/dep/EpochPlatformEngine/src/core/core.time.cpp').read_text(encoding='utf-8')
  host=(ROOT/'Engine/src/epoch/implementation/epoch.session_runtime.inl').read_text(encoding='utf-8')
  api=(ROOT/'Engine/modules/editor.core.ixx').read_text(encoding='utf-8')
  declarations=module[module.index('export namespace'):].replace('export namespace','namespace',1)
  methods='namespace epochengine::core::time{'+''.join(definition(source,s) for s in (
   '    std::uint64_t now_ns()', '    void frame_clock::start()', '    void frame_clock::tick()',
   '    void simulation_clock::start()', '    void simulation_clock::tick(',
   '    std::uint32_t simulation_clock::step_budget()', '    void simulation_clock::consume_steps('))+'}'
  tick=definition(host,'                        auto tick_time_spine =')+';tick_time_spine();'
  code=r'''
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
DECLARATIONS METHODS
namespace epochengine{CONTROL;EditorTimeControl configured;unsigned consumed{};
EditorTimeControl editor_time_control(const int*){return configured;}
void editor_consume_time_step_request(const int*){configured.step_once=false;++consumed;}}
struct Session{epochengine::core::time::simulation_clock simulation;};
void actual_tick(Session&session,float dt){const auto ctx=std::make_shared<int>(1);TICK}
int main(){using namespace epochengine;
 for(double fixed:{1.0/30.0,1.0/60.0,1.0/120.0}){
  Session session;session.simulation.start();configured={};configured.paused=true;configured.fixed_dt_seconds=fixed;
  auto priorConsumed=consumed;actual_tick(session,.1f);CHECK(session.simulation.simulated_steps==0&&consumed==priorConsumed);
  configured.step_once=true;actual_tick(session,.1f);
  CHECK(session.simulation.simulated_steps==1&&std::abs(session.simulation.simulated_seconds-fixed)<1e-12);
  CHECK(!configured.step_once&&!session.simulation.step_once_requested&&consumed==priorConsumed+1);
  actual_tick(session,.1f);CHECK(session.simulation.simulated_steps==1&&consumed==priorConsumed+1);
  configured.time_scale=0;configured.step_once=true;actual_tick(session,.1f);
  CHECK(session.simulation.simulated_steps==2&&std::abs(session.simulation.simulated_seconds-2*fixed)<1e-12);
  configured.time_scale=1;configured.paused=false;actual_tick(session,float(fixed*2.01));CHECK(session.simulation.simulated_steps==4);
  CHECK(!configured.step_once&&consumed==priorConsumed+2);
 }
}
'''.replace('DECLARATIONS',declarations).replace('METHODS',methods).replace('CONTROL',definition(api,'    export struct EditorTimeControl').replace('export ','',1)).replace('TICK',tick)
  proof=ROOT/'Artifacts/Reports/simulation-step-oct29'/hashlib.sha256(code.encode()).hexdigest()[:12];proof.mkdir(parents=True,exist_ok=True)
  cpp=proof/'step.cpp';cpp.write_text(code,encoding='utf-8')
  results=compile_and_run(self,cpp,proof)
  (proof/'result.json').write_text(json.dumps({'passed':True,'results':results,'scope':'Actual host tick lambda and actual Platform clock methods; paused one-step, request consumption, no repeat, zero speed and normal forward ticks. No renderer/model/native window.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
