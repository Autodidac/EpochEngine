"""Exercise the actual software frame owner with CPU recording collaborators."""
from pathlib import Path
import json
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


def fixture():
    owner = (ROOT / "Engine/src/renderers/software/software.context_render.cpp").read_text(encoding="utf-8-sig")
    process = definition(owner, "    bool softrenderer_process(")
    return r'''#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#define EPOCH_USE_CLEAR_COLOR 1
std::string calls;
unsigned presentations{}, captures{};
namespace epochengine {
namespace core {
 enum class ContextType {Software};
 struct WindowData {bool close{};void* hwnd{};std::shared_ptr<void> context;
  bool get_should_close()const{return close;}};
 struct Context {ContextType type{ContextType::Software};bool priority{};
  std::shared_ptr<WindowData> windowData;
  bool gui_overlay_priority()const{return priority;}};
 inline std::array<float,4> clear_color_for_context(ContextType){return {0,0,0,1};}
 struct CommandQueue {unsigned pending{};unsigned depth()const{return pending;}
  void drain();void clear(){pending=0;calls+="X";}};
}
namespace diagnostics {struct FrameTiming {
 FrameTiming(core::ContextType,std::uintptr_t,const char*){}void finish(){}};}
namespace telemetry {
 struct RendererTelemetryTags {core::ContextType type;std::uintptr_t window;const char* tag{};};
 void emit_gauge(const char*,std::int64_t,RendererTelemetryTags){}
}
namespace anativecontext {
 struct SoftRendState {bool running{true};int width{2},height{2};
  std::vector<std::uint32_t> framebuffer=std::vector<std::uint32_t>(4,0xDEADBEEFu);
  struct {bool preparedThisFrame{true};}arcadeScreen;
  int lastTelemetryWidth{},lastTelemetryHeight{};std::size_t lastTelemetryBufferLength{};
  std::int64_t lastTelemetryCommandDepth{-1};};
 SoftRendState s_softrendererstate;
 void capture_frame_if_requested(const std::vector<std::uint32_t>& frame,int,int,std::uintptr_t){
  calls+="C";++captures;assert(frame[1]==2&&frame[2]==3);}
 namespace detail {
  void refresh_dimensions(core::Context&){calls+="D";}
  void render_scene_preview(const core::Context&){calls+="S";
   auto& state=s_softrendererstate;assert(!state.arcadeScreen.preparedThisFrame);
   assert(state.framebuffer[1]==0xFF000000u&&state.framebuffer[2]==0xFF000000u);
   state.framebuffer[1]=2;}
  bool present_frame(const core::Context&,const SoftRendState& state){
   calls+="P";++presentations;assert(state.framebuffer[1]==2&&state.framebuffer[2]==3);return true;}
 }
}
namespace gui {
 bool render_deferred_batch(core::Context*){calls+="B";anativecontext::s_softrendererstate.framebuffer[0]=1;return true;}
 bool render_top_layer_batch(core::Context*){calls+="T";anativecontext::s_softrendererstate.framebuffer[3]=4;return true;}
}
void core::CommandQueue::drain(){calls+="Q";pending=0;anativecontext::s_softrendererstate.framebuffer[2]=3;}
namespace anativecontext {
''' + process + r'''
}
}
int main(){
 using namespace epochengine;auto context=std::make_shared<core::Context>();
 context->windowData=std::make_shared<core::WindowData>();context->windowData->context=context;
 core::CommandQueue queue;
 // Two identical frames and an empty queue must both redraw and present.
 for(unsigned turn=0;turn<2;++turn){calls.clear();assert(anativecontext::softrenderer_process(*context,queue));
  assert(calls=="DBSQBTCP"&&presentations==turn+1&&captures==turn+1);}
 // Overlay priority preserves the scene/queue/base/top order, suppressing only pre-scene GUI.
 context->priority=true;calls.clear();queue.pending=1;
 assert(anativecontext::softrenderer_process(*context,queue));assert(calls=="DSQBTCP"&&queue.pending==0);
 // A context without GUI publication still draws and presents its full frame.
 context->windowData->context.reset();context->windowData.reset();calls.clear();
 assert(anativecontext::softrenderer_process(*context,queue));assert(calls=="DSQCP");
 // Close retires queued commands without drawing, capture, or presentation.
 anativecontext::s_softrendererstate.running=false;queue.pending=1;calls.clear();
 assert(!anativecontext::softrenderer_process(*context,queue));
 assert(calls=="X"&&queue.pending==0&&presentations==4&&captures==4);
}
'''


class SoftwareFullFrame(unittest.TestCase):
    def test_unchanged_frames_redraw_present_and_preserve_order(self):
        from Tests.support.cpp import compile_and_run
        report = ROOT / "Artifacts/Intermediate/tests/software-full-frame"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "fixture.cpp"
        source.write_text(fixture(), encoding="utf-8")
        records = compile_and_run(self, source, report, compile_timeout=60, run_timeout=15)
        (report / "result.json").write_text(json.dumps({"records": records, "native_gpu": False,
            "proof": "Repeated unchanged frames clear, render, drain, replay and present; overlay and close order retained."}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
