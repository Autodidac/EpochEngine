"""Actual native-event queue/filter/frame collapse and shared drag owner, no window."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest

from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
FRAME = ROOT / "Engine/src/epochgui/controls/gui.frame_lifecycle.inl"
GUI = ROOT / "Engine/src/epochgui/gui.engine.cpp"
API = ROOT / "Engine/modules/gui.engine.ixx"


def fixture(before_drag=None):
    frame, gui, api = (p.read_text(encoding="utf-8") for p in (FRAME, GUI, API))
    types = "\n".join(definition(api, marker, True).replace("export ", "") for marker in (
        "    export struct Vec2", "    export enum class EventType", "    export struct InputEvent",
        "    export struct DragSurfaceState", "    export struct DragSurfaceOptions", "    export struct DragSurfaceResult"))
    queue = "\n".join(definition(gui, marker) for marker in (
        "        [[nodiscard]] constexpr bool is_coalescible_input(", "        void queue_input_event(",
        "        [[nodiscard]] static bool filter_frame_input("))
    points = "\n".join(definition(gui, marker) for marker in (
        "        [[nodiscard]] static bool point_in_rect(", "        [[nodiscard]] static bool point_in_modal_input_capture(",
        "        [[nodiscard]] static bool point_in_active_clip(", "        [[nodiscard]] static bool left_press_available()",
        "        [[nodiscard]] static bool left_release_available()", "        static void consume_left_release()"))
    lifecycle = "\n".join(definition(frame, marker) for marker in (
        "    void push_input(", "    void push_input_for_context(",
        "    [[nodiscard]] static bool completed_stationary_click(", "    void begin_frame("))
    lifecycle += "\n".join(definition(frame, marker) for marker in (
        "    std::optional<Vec2> mouse_press_position(", "    bool drag_surface_hit_test("))
    drag = before_drag or definition(frame, "    DragSurfaceResult drag_surface(")
    button_state = definition(gui, "        struct ContextPointerButtonState", True)
    trace_state = definition(frame, "    struct PointerFrameTraceState", True)
    cleanup = definition(frame, "    void cleanup_context(")
    cleanup = cleanup[cleanup.index("        g_contextMouseDownStates.erase(ctx);"):cleanup.index("        g_contextActiveWidgets.erase(ctx);")]
    return r'''
#include <algorithm>
#include <stdexcept>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
import epoch.gui.input;
namespace gui_lib=epochengine::gui_lib;
struct TraceRecord{std::string tag,message;};std::vector<TraceRecord> traceRecords;bool traceThrows{};
namespace epochengine::logger {void info(std::string_view tag,std::string message){if(traceThrows)throw std::runtime_error("trace port");traceRecords.push_back({std::string{tag},std::move(message)});}}
namespace core {namespace cli{inline bool trace_menu_button0_rect=false;}struct Context{
 bool focus=true,pointer=true,wheel=true;void* windowData{};
 void* get_hwnd(){return this;}bool has_input_focus_safe(){return focus;}
 bool has_pointer_input_safe(){return pointer;}bool has_hover_scroll_safe(){return wheel;}
};}
void check(bool good,int line){if(!good){std::fprintf(stderr,"Queued drag contract failed line %d\n",line);std::exit(81);}}
#define CHECK(value) check(static_cast<bool>(value),__LINE__)
namespace gui {
TYPES
struct Frame{
 std::shared_ptr<core::Context> ctxShared;core::Context* ctx{};
 const void* visitedActiveWidget{};
 float deltaTime{},caretTimer{};bool caretVisible{};
 bool mouseDown{},prevMouseDown{},mouseRightDown{},prevMouseRightDown{},pointerInputAllowed=true,keyboardFocusCancelled{};
 Vec2 mousePos{};int mouseWheelDelta{};std::vector<InputEvent> events;
 bool justPressed{},justReleased{},rightJustPressed{},rightJustReleased{};
 bool mousePressConsumed{},mouseReleaseConsumed{},rightPressConsumed{},rightReleaseConsumed{};
 bool selectPopupCapturedThisFrame{},selectPopupOwnsInput{},menuPopupBlocksBackground{},menuPopupOwnsInput{};
 std::vector<int> queuedDraws,topLayerDraws,pendingSelectPopups;int topLayerDepth{};
 gui_lib::input::ModalInputArbiter modalInput;
 Vec2 contentMin{},contentMax{};bool insideWindow{};
}g_frame;
constexpr float kCaretBlinkPeriod=1.0f;constexpr std::size_t kMaxPendingInputEvents=512;
std::vector<InputEvent> g_pendingEvents;
std::map<const core::Context*,std::vector<InputEvent>> g_contextPendingEvents;
std::mutex g_contextPendingEventsMutex;
BUTTON_STATE
TRACE_STATE
static thread_local PointerFrameTraceState g_pointerFrameTrace{};
std::map<const core::Context*,ContextPointerButtonState> g_contextMouseDownStates,g_contextRightMouseDownStates;
std::map<const core::Context*,std::size_t> g_contextPressedButtonKeys;
struct SelectBox{const core::Context* owner{};bool visited_this_frame{};};std::map<int,SelectBox>g_selectBoxStates;
void ensure_backend_upload(core::Context&){}
void retire_context_menus_for_right_press(){}
// This fixture owns queued pointer/drag delivery, not text-field rendering.
void reset_edit_box_visits(const void*){}
bool any_select_box_open(const core::Context*){return false;}
void reset_frame(){g_frame.mousePressConsumed=g_frame.mouseReleaseConsumed=false;
 g_frame.rightPressConsumed=g_frame.rightReleaseConsumed=false;g_frame.modalInput.clear();}
bool has_content_clip(){return g_frame.insideWindow;}
namespace popup_input{bool allows_pointer(gui_lib::input::PopupCapture capture){return gui_lib::input::popup_allows_pointer(capture);}}
QUEUE
POINTS
LIFECYCLE
DRAG
}
using namespace gui;
InputEvent event(EventType type,float x,float y,int button=0){return {.type=type,.mouse_pos={x,y},.mouse_button=button};}
const DragSurfaceOptions options{.position={10,10},.size={100,100},.hit_padding=0,.minimum_hit_extent=1,.drag_threshold=3};
void tick(const std::shared_ptr<core::Context>& context,std::initializer_list<InputEvent> events,bool down=false,Vec2 final={}){
 for(const auto& event:events)push_input_for_context(context.get(),event);
 begin_frame(context,0.016f,final,down);
 g_frame.insideWindow=true;g_frame.contentMin={10,10};g_frame.contentMax={110,110};
}
void retire_pointer_state(const core::Context* ctx){CLEANUP_BUTTON_STATE}
void fresh(const std::shared_ptr<core::Context>& context){context->focus=context->pointer=context->wheel=true;
 g_contextMouseDownStates[context.get()]={};g_contextRightMouseDownStates[context.get()]={};
 g_contextPendingEvents[context.get()].clear();tick(context,{});}
void fast_sequences(const std::shared_ptr<core::Context>& context){
 fresh(context);DragSurfaceState state;
 tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 CHECK(!g_frame.justPressed&&!g_frame.justReleased); // Button stationary guard is unchanged.
 CHECK(mouse_press_position()&&mouse_press_position()->x==20&&mouse_press_position()->y==20);
 auto result=drag_surface(state,options);
 CHECK(result.pressed&&result.released&&!result.dragging&&!state.pending&&result.delta.x==40&&result.delta.y==25);
 CHECK(g_frame.mousePressConsumed&&g_frame.mouseReleaseConsumed);
 CHECK(!mouse_press_position());
 DragSurfaceState other;CHECK(!drag_surface(other,options)&&!other.pending);
 fresh(context);state={};tick(context,{event(EventType::MouseDown,20,20)},true);
 result=drag_surface(state,options);CHECK(result.pressed&&state.pending&&!state.dragging);
 tick(context,{event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 result=drag_surface(state,options);CHECK(result.released&&result.delta.x==40&&result.delta.y==25&&!state.pending);
 fresh(context);state={};tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseMove,60,45)},true);
 result=drag_surface(state,options);CHECK(result.pressed&&result.dragging&&state.press_position.x==20&&result.delta.x==40);
 tick(context,{event(EventType::MouseUp,60,45)});result=drag_surface(state,options);CHECK(result.released&&result.delta.x==40);
 // The existing queue may coalesce adjacent moves, but retains press/release order.
 fresh(context);state={};tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseMove,30,30),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 CHECK(g_frame.events.size()==3);result=drag_surface(state,options);CHECK(result.released&&result.delta.x==40);
}
void stationary_and_threshold(const std::shared_ptr<core::Context>& context){
 fresh(context);DragSurfaceState state;
 tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseUp,20,20)});
 CHECK(g_frame.justPressed&&g_frame.justReleased&&left_press_available()&&left_release_available());
 const auto click=drag_surface(state,options);CHECK(click.pressed&&!click&&!state.pending);
 CHECK(left_press_available()&&left_release_available()&&!g_frame.mousePressConsumed&&!g_frame.mouseReleaseConsumed);
 fresh(context);tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseUp,21,21)});
 auto result=drag_surface(state,options);CHECK(result.pressed&&!result&&!state.pending&&!g_frame.mousePressConsumed);
 // Same held position is not movement, and a foreign right-button drag is not left input.
 fresh(context);tick(context,{event(EventType::MouseDown,20,20)},true);(void)drag_surface(state,options);
 tick(context,{},true,{20,20});CHECK(!drag_surface(state,options)&&state.pending);
 tick(context,{event(EventType::MouseUp,20,20)});CHECK(!drag_surface(state,options)&&!state.pending);
 fresh(context);tick(context,{event(EventType::MouseDown,20,20,1),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45,1)});
 result=drag_surface(state,options);CHECK(!result.pressed&&!result&&!state.pending);
}
void admission_and_cancel(const std::shared_ptr<core::Context>& context){
 const auto gesture=std::initializer_list<InputEvent>{event(EventType::MouseDown,20,20),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)};
 DragSurfaceState state;
 fresh(context);tick(context,{event(EventType::MouseDown,2,2),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 auto result=drag_surface(state,options);CHECK(!result.pressed&&!result&&!state.pending);
 // Hit bounds remain distinct from a larger valid content clip.
 fresh(context);tick(context,{event(EventType::MouseDown,2,2),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 g_frame.contentMin={0,0};g_frame.contentMax={200,200};
 result=drag_surface(state,options);CHECK(!result.pressed&&!result&&!state.pending);
 // A padded hit target is still the shared owner's explicit policy.
 fresh(context);tick(context,{event(EventType::MouseDown,7,7),event(EventType::MouseUp,60,45)});
 g_frame.contentMin={0,0};g_frame.contentMax={200,200};auto padded=options;padded.hit_padding=5;
 result=drag_surface(state,padded);CHECK(result.released);
 // Press outside a bounded modal cannot be laundered by its final inside point.
 fresh(context);tick(context,{event(EventType::MouseDown,2,2),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 g_frame.contentMin={0,0};g_frame.contentMax={200,200};g_frame.modalInput.begin({{10,10},{100,100}});
 result=drag_surface(state,padded);CHECK(!result.pressed&&!result&&!state.pending);
 fresh(context);tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseUp,180,190)});
 result=drag_surface(state,options);CHECK(result.released&&!result.hovered&&result.delta.x==160&&result.delta.y==170);
 for(bool release:{false,true}){
  fresh(context);tick(context,gesture);if(release)g_frame.mouseReleaseConsumed=true;else g_frame.mousePressConsumed=true;
  result=drag_surface(state,options);CHECK(!result.pressed&&!result&&!state.pending);
 }
 fresh(context);tick(context,gesture);g_frame.modalInput.begin({{10,10},{100,100}});g_frame.modalInput.require_owner_scope();
 CHECK(!mouse_press_position());
 result=drag_surface(state,options);CHECK(!result.pressed&&!result&&!state.pending);
 g_frame.modalInput.enter_owner_scope();result=drag_surface(state,options);CHECK(result.released);
 fresh(context);tick(context,{event(EventType::MouseDown,20,20)},true);(void)drag_surface(state,options);CHECK(state.pending);
 context->focus=false;tick(context,{event(EventType::MouseMove,60,45),{.type=EventType::FocusLost},event(EventType::MouseUp,60,45)});
 result=drag_surface(state,options);CHECK(!result&&!state.pending&&!g_frame.mouseDown&&!g_frame.justPressed&&!g_frame.justReleased);
 context->focus=true;fresh(context);tick(context,gesture);CHECK(drag_surface(state,options).released);
 fresh(context);tick(context,{event(EventType::MouseDown,20,20)},true);(void)drag_surface(state,options);
 tick(context,{{.type=EventType::FocusLost},event(EventType::MouseUp,60,45)});
 result=drag_surface(state,options);CHECK(!result&&!state.pending);
 fresh(context);tick(context,{event(EventType::MouseDown,25,25)},true);(void)drag_surface(state,options);
 tick(context,{{.type=EventType::FocusLost},event(EventType::MouseDown,20,20),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 result=drag_surface(state,options);CHECK(result.pressed&&result.released&&result.delta.x==40&&result.delta.y==25);
}
void context_ownership(const std::shared_ptr<core::Context>& a){
 auto b=std::make_shared<core::Context>();fresh(a);fresh(b);DragSurfaceState as,bs;
 tick(a,{event(EventType::MouseDown,20,20)},true);CHECK(drag_surface(as,options).pressed);
 tick(b,{});CHECK(!drag_surface(bs,options)&&!bs.pending);
 tick(a,{event(EventType::MouseMove,65,55),event(EventType::MouseUp,65,55)});
 const auto result=drag_surface(as,options);CHECK(result.released&&result.delta.x==45&&result.delta.y==35&&!bs.pending);
}
void queued_button_ownership(const std::shared_ptr<core::Context>& context){
 fresh(context);DragSurfaceState state;
 tick(context,{event(EventType::MouseDown,20,20)},false);(void)drag_surface(state,options);
 CHECK(state.pending&&g_frame.mouseDown&&g_contextMouseDownStates[context.get()].queued_edge_owned);
 tick(context,{event(EventType::MouseMove,60,45)},false);auto result=drag_surface(state,options);
 CHECK(result.dragging&&!result.released&&state.pending&&g_frame.mouseDown&&!g_frame.justPressed&&!g_frame.justReleased);
 tick(context,{},false,{70,50});result=drag_surface(state,options);
 CHECK(result.dragging&&state.pending&&!g_frame.justPressed&&!g_frame.justReleased);
 tick(context,{event(EventType::MouseUp,70,50)},false);result=drag_surface(state,options);
 CHECK(result.released&&!state.pending&&g_contextMouseDownStates[context.get()].queued_edge_owned);
 tick(context,{},true,{70,50});CHECK(!g_frame.justPressed&&!g_frame.justReleased&&!g_frame.mouseDown);
 tick(context,{},false,{70,50});CHECK(!g_frame.justPressed&&!g_frame.justReleased&&!g_frame.mouseDown);
 // Sampler-only clients still get their original press/release fallback.
 fresh(context);state={};tick(context,{},true,{20,20});(void)drag_surface(state,options);
 CHECK(g_frame.justPressed&&state.pending&&!g_contextMouseDownStates[context.get()].queued_edge_owned);
 tick(context,{},false,{20,20});(void)drag_surface(state,options);
 CHECK(g_frame.justReleased&&!state.pending&&!g_frame.mouseDown);
 // Right queued edges have the same context ownership and one edge each.
 fresh(context);tick(context,{event(EventType::MouseDown,20,20,1)},false);
 CHECK(g_frame.mouseRightDown&&g_frame.rightJustPressed&&g_contextRightMouseDownStates[context.get()].queued_edge_owned);
 tick(context,{event(EventType::MouseMove,60,45)},false);
 CHECK(g_frame.mouseRightDown&&!g_frame.rightJustPressed&&!g_frame.rightJustReleased);
 tick(context,{event(EventType::MouseUp,60,45,1)},false);
 CHECK(!g_frame.mouseRightDown&&g_frame.rightJustReleased&&g_contextRightMouseDownStates[context.get()].queued_edge_owned);
 // Real focus/pointer cancellation releases both queued owners immediately.
 for(const bool focusLoss:{false,true}){
  fresh(context);tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseDown,20,20,1)},false);
  context->focus=!focusLoss;context->pointer=false;tick(context,{event(EventType::MouseMove,80,80)},false);
  CHECK(!g_frame.mouseDown&&!g_frame.mouseRightDown);
  CHECK(g_contextMouseDownStates[context.get()].queued_edge_owned&&g_contextRightMouseDownStates[context.get()].queued_edge_owned);
  context->focus=context->pointer=true;tick(context,{},true);
  CHECK(!g_frame.mouseDown&&!g_frame.mouseRightDown&&!g_frame.justPressed&&!g_frame.rightJustPressed);
 }
 // A new context cannot inherit the last rendered context's held frame; the
 // exact existing cleanup statements also clear ownership for address reuse.
 fresh(context);tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseDown,20,20,1)},false);
 auto other=std::make_shared<core::Context>();tick(other,{},false);
 CHECK(!g_frame.mouseDown&&!g_frame.prevMouseDown&&!g_frame.justPressed&&!g_frame.mouseRightDown);
 tick(context,{event(EventType::MouseMove,60,45)},false);CHECK(g_frame.mouseDown&&!g_frame.justPressed);
 retire_pointer_state(context.get());CHECK(!g_contextMouseDownStates.contains(context.get())&&!g_contextRightMouseDownStates.contains(context.get()));
 tick(context,{},false);CHECK(!g_frame.mouseDown&&!g_frame.prevMouseDown&&!g_frame.justPressed&&!g_frame.mouseRightDown);
}
void pointer_trace_contract(const std::shared_ptr<core::Context>& context){
 core::cli::trace_menu_button0_rect=false;traceRecords.clear();g_pointerFrameTrace={};fresh(context);
 tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseUp,20,20)});(void)mouse_press_position();
 CHECK(traceRecords.empty()&&g_pointerFrameTrace.frames==0);
 core::cli::trace_menu_button0_rect=true;traceRecords.clear();g_pointerFrameTrace={};fresh(context);
 for(unsigned frame=0;frame<130;++frame){
  if(frame==0)tick(context,{event(EventType::MouseDown,20,20),InputEvent{.type=EventType::TextInput,.text="PRIVATE_TEXT_CANARY"}},false);
  else tick(context,{event(EventType::MouseMove,20.0f+frame*.1f,20)},false);
  (void)mouse_press_position();
 }
 CHECK(g_pointerFrameTrace.frames==120);
 CHECK(std::ranges::count_if(traceRecords,[](const auto& row){return row.tag=="PointerTrace.Gui";})==120);
 CHECK(std::ranges::none_of(traceRecords,[](const auto& row){return row.message.find("PRIVATE_TEXT_CANARY")!=std::string::npos;}));
 CHECK(traceRecords.size()<=360);
 g_pointerFrameTrace={};traceThrows=true;fresh(context);DragSurfaceState state;
 tick(context,{event(EventType::MouseDown,20,20),event(EventType::MouseMove,60,45),event(EventType::MouseUp,60,45)});
 CHECK(mouse_press_position());const auto drag=drag_surface(state,options);CHECK(drag.released&&drag.delta.x==40);
 traceThrows=false;core::cli::trace_menu_button0_rect=false;g_pointerFrameTrace={};traceRecords.clear();fresh(context);
}
int main(){auto context=std::make_shared<core::Context>();fast_sequences(context);stationary_and_threshold(context);admission_and_cancel(context);context_ownership(context);queued_button_ownership(context);pointer_trace_contract(context);}
'''.replace("TYPES", types).replace("CLEANUP_BUTTON_STATE", cleanup).replace("BUTTON_STATE", button_state).replace("TRACE_STATE", trace_state).replace("QUEUE", queue).replace("POINTS", points).replace("LIFECYCLE", lifecycle).replace("DRAG", drag)


class GuiQueuedDrag(unittest.TestCase):
    def test_actual_queue_frame_drag_debug_release(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC C++23 module consumer")
        code = fixture()
        key = hashlib.sha256(code.encode()).hexdigest()[:12]
        proof = ROOT / "Artifacts/Reports/gui-queued-button-owner-oct28" / key
        build = ROOT / "Artifacts/Intermediate/cmake/gui-queued-button-owner" / key
        proof.mkdir(parents=True, exist_ok=True)
        source = proof / "gui.queued_drag_contract.cpp"
        source.write_text(code, encoding="utf-8")
        portable = ROOT / "Engine/dep/EpochGui"
        modules = [portable / "modules/epoch.gui.ixx", portable / "modules/epoch.gui.input.ixx"]
        (proof / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.30)
project(GuiQueuedDrag LANGUAGES CXX)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
add_executable(queued_drag "{source.as_posix()}" "{(portable/'src/epochgui/input.cpp').as_posix()}")
target_compile_features(queued_drag PRIVATE cxx_std_23)
target_compile_options(queued_drag PRIVATE /W3 /WX /utf-8)
target_sources(queued_drag PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{portable.as_posix()}" FILES {" ".join('"'+p.as_posix()+'"' for p in modules)})
enable_testing()
add_test(NAME actual_queued_drag COMMAND queued_drag)
set_tests_properties(actual_queued_drag PROPERTIES TIMEOUT 15)
''', encoding="utf-8")
        commands = [["cmake", "-S", str(proof), "-B", str(build), "-G", "Visual Studio 17 2022", "-A", "x64"]]
        for config in ("Debug", "Release"):
            commands += [["cmake", "--build", str(build), "--config", config, "--parallel", "1"],
                ["ctest", "--test-dir", str(build), "-C", config, "--output-on-failure"]]
        results = []
        for index, command in enumerate(commands):
            completed = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=150)
            log = proof / f"lane-{index}.log"
            log.write_text(completed.stdout + completed.stderr, encoding="utf-8")
            results.append({"command": command, "exitCode": completed.returncode, "log": str(log)})
            self.assertEqual(completed.returncode, 0, completed.stdout[-9000:] + completed.stderr)
        (proof / "result.json").write_text(json.dumps({
            "scope": "Actual queue/filter/begin_frame/drag_surface with portable drag owner; no native OS window",
            "passed": True, "nativeAcceptance": False,
            "owners": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                [FRAME, GUI, API, *modules, portable/'src/epochgui/input.cpp']}, "commands": results,
            "cases": ["same-frame Down/Move/Up", "prior Down then queued Move/Up", "held Down+Move preserves origin",
                "stationary click edges unchanged/unconsumed", "below-threshold gesture is not a drag", "coalesced moves retain ordered edges",
                "origin clip and release outside content clip", "press/release consumed ownership", "modal owner gate",
                "native focus loss and queued cancellation", "two context queues keep drag state independent", "queued left/right ownership survives false sampler", "sampler-only fallback", "cleanup/address reuse clears ownership", "pointer/focus cancellation", "no repeated press"],
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
