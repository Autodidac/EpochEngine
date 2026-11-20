"""Exercise the actual software frame owner with CPU recording collaborators."""
from pathlib import Path
import json
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]


def fixture():
    owner = (ROOT / "Engine/src/renderers/software/software.context_render.cpp").read_text(encoding="utf-8-sig")
    process = definition(owner, "    bool softrenderer_process(")
    profiling = owner[owner.index("        using PhaseClock = "):owner.index("\n    }\n\n    bool softrenderer_process(")]
    return r'''#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <vector>
#define EPOCH_USE_CLEAR_COLOR 1
std::string calls;
unsigned presentations{}, captures{};
bool expectProjectContent{true};
namespace epochengine {
namespace perf { enum class frame_activity {foreground, background}; }
namespace core {
 namespace env { std::optional<std::string> get(const char*){return {};} }
 enum class ContextType {Software};
 enum class ScenePreviewMode {None,Editor};
 struct WindowData {bool close{};void* hwnd{};std::shared_ptr<void> context;
  bool get_should_close()const{return close;}};
 struct Context {ContextType type{ContextType::Software};bool priority{};
  bool drawProject{true}, canvas2d{};
  ScenePreviewMode mode{ScenePreviewMode::Editor};
  perf::frame_activity activity{perf::frame_activity::foreground};
  std::shared_ptr<WindowData> windowData;
  perf::frame_activity frame_pacing_activity()const{return activity;}
  ScenePreviewMode scene_preview_mode()const{return mode;}
  bool gui_overlay_priority()const{return priority;}};
 inline std::array<float,4> clear_color_for_context(ContextType){return {0,0,0,1};}
 struct CommandQueue {unsigned pending{};unsigned depth()const{return pending;}
  void drain();void clear(){pending=0;calls+="X";}};
}
namespace previewgrid {
 enum class CameraMode {Editor,Canvas2D};
 CameraMode camera_mode_for(const core::Context* ctx){return ctx->canvas2d?CameraMode::Canvas2D:CameraMode::Editor;}
}
namespace logger {
 enum class LogLevel {INFO};
 struct Sink {
  unsigned samples{};
  int width{}, height{};
  double fps{};
  std::uint64_t frames{};
  std::array<double,5> phase_ms{};
  void logf(LogLevel,std::source_location,const char*,int w,int h,double rate,
      std::uint64_t count,double clear,double scene,double gui,double capture,double present)
  {++samples;width=w;height=h;fps=rate;frames=count;phase_ms={clear,scene,gui,capture,present};}
 } sink;
 Sink& get(const char*){return sink;}
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
 void capture_frame_if_requested(const std::vector<std::uint32_t>& frame,int,int,std::uintptr_t,bool projectContent){
  calls+="C";++captures;assert(frame[1]==2&&frame[2]==3);assert(projectContent==expectProjectContent);}
 namespace detail {
  void refresh_dimensions(core::Context&){calls+="D";}
  bool render_scene_preview(const core::Context& ctx){calls+="S";
   auto& state=s_softrendererstate;assert(!state.arcadeScreen.preparedThisFrame);
   assert(state.framebuffer[1]==0xFF000000u&&state.framebuffer[2]==0xFF000000u);
   state.framebuffer[1]=2;return ctx.drawProject;}
  bool present_frame(const core::Context&,const SoftRendState& state){
   calls+="P";++presentations;assert(state.framebuffer[1]==2&&state.framebuffer[2]==3);return true;}
''' + profiling + r'''
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
 assert(logger::sink.samples==0);

 anativecontext::s_softrendererstate.running=true;
 context->windowData=std::make_shared<core::WindowData>();context->windowData->context=context;
 context->drawProject=false;context->canvas2d=true;context->mode=core::ScenePreviewMode::None;
 expectProjectContent=false;
 assert(anativecontext::softrenderer_process(*context,queue));
 // A GUI-only project can qualify through GUI replay, while Canvas2D needs a scene blit.
 context->canvas2d=false;expectProjectContent=true;
 assert(anativecontext::softrenderer_process(*context,queue));
 context->windowData->context.reset();context->windowData.reset();

 using namespace std::chrono_literals;
 using namespace anativecontext::detail;
 FramePhaseEvidence evidence;
 const PhaseDurations points{1ms,2ms,3ms,4ms,5ms};
 const auto start=PhaseClock::time_point{};
 for(int n=0;n<4;++n)evidence.observe(*context,1600,1000,start+n*1s,start+(n+1)*1s,points);
 assert(logger::sink.samples==0);
 context->activity=perf::frame_activity::background;
 evidence.observe(*context,1600,1000,start+4s,start+5s,points);
 assert(evidence.frames==0&&logger::sink.samples==0);
 context->activity=perf::frame_activity::foreground;
 evidence.observe(*context,1280,800,start+5s,start+9s,points);
 for(int n=0;n<5;++n)evidence.observe(*context,1600,1000,start+(n+9)*1s,start+(n+10)*1s,points);
 assert(logger::sink.samples==1&&logger::sink.frames==5&&logger::sink.fps==1);
 assert(logger::sink.width==1600&&logger::sink.height==1000);
 assert((logger::sink.phase_ms==std::array<double,5>{1,2,3,4,5}));
 for(int n=0;n<14;++n)evidence.observe(*context,1600,1000,start+n*5s,start+(n+1)*5s,points);
 assert(logger::sink.samples==12);
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
            "proof": "Full-frame order and opt-in phase profile with focus, resize and report limits."}, indent=2), encoding="utf-8")

    def test_project_smoke_waits_before_reserving_capture(self):
        from Tests.support.cpp import compile_and_run
        owner = (ROOT / "Engine/src/renderers/software/software.context_capture.cpp").read_text(encoding="utf-8-sig")
        capture = definition(owner, "    void capture_frame_if_requested(")
        report = ROOT / "Artifacts/Intermediate/tests/software-project-capture"
        report.mkdir(parents=True, exist_ok=True)
        source = report / "fixture.cpp"
        source.write_text(r'''#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace epochengine {
namespace core::cli {
 bool capture_requested{true}, smoke_requested{true};
 std::string scene_name{"project:PlatformerExample2D"};
 int reservations{}, releases{};
}
extern "C" const char* epoch_reserve_capture_path_utf8(const char* backend, std::uintptr_t window) {
 assert(std::string_view(backend)=="software" && window==1);
 ++core::cli::reservations;
 return "frame.bmp";
}
extern "C" void epoch_release_capture_path_utf8(const char* backend, std::uintptr_t window) {
 assert(std::string_view(backend)=="software" && window==1);
 ++core::cli::releases;
}
namespace core::log {
 enum class level {info,warn};
 void write(level, const char*, std::string_view) {}
}
namespace detail {
 bool canWrite{true};
 int writes{};
 bool write_bmp(const std::filesystem::path&, const std::vector<std::uint32_t>&, int, int) {
  ++writes;
  return canWrite;
 }
}
''' + capture + r'''
}
int main() {
 using namespace epochengine;
 const std::vector<std::uint32_t> pixels(4, 0xFF000000u);
 for (int frame=0; frame<20; ++frame)
  capture_frame_if_requested(pixels, 2, 2, 1, false);
 assert(core::cli::reservations==0 && detail::writes==0);
 capture_frame_if_requested(pixels, 2, 3, 1, true);
 assert(core::cli::reservations==0);
 capture_frame_if_requested(pixels, 2, 2, 1, true);
 assert(core::cli::reservations==1 && detail::writes==1 && core::cli::releases==0);
 core::cli::smoke_requested=false;
 capture_frame_if_requested(pixels, 2, 2, 1, false);
 assert(core::cli::reservations==2 && detail::writes==2);
 core::cli::smoke_requested=true;
 core::cli::scene_name="menu";
 detail::canWrite=false;
 capture_frame_if_requested(pixels, 2, 2, 1, false);
 assert(core::cli::reservations==3 && detail::writes==3 && core::cli::releases==1);
}
''', encoding="utf-8")
        compile_and_run(self, source, report, compile_timeout=60, run_timeout=15)


if __name__ == "__main__":
    unittest.main()
