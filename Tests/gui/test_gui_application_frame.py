"""CPU contract for the project GUI owner callback and input/paint order."""
from pathlib import Path
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run, windows_sdk_includes

ROOT = Path(__file__).resolve().parents[2]


class GuiApplicationFrame(unittest.TestCase):
    def test_poll_after_input_with_a_borrowed_owner_view(self):
        runtime = (ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl").read_text(encoding="utf-8")
        owners = "\n".join(definition(runtime, marker) for marker in (
            "            static bool set_application_widget_text(",
            "            static bool get_application_widget_text(",
            "            void frame_gui_application(",
        ))
        source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <epoch.engine.hpp>

namespace project = epochengine::project;
enum class Phase { clear, begin, paint, keyboard, action, poll, end, present, capture };
std::vector<Phase> phases;

namespace core {
enum class ContextType { OpenGL, Software };
struct Context {
    ContextType type{ContextType::Software};
    void clear_safe() { phases.push_back(Phase::clear); }
    void present_safe() { phases.push_back(Phase::present); }
};
}
namespace gui {
struct Vec2 { float x{}, y{}; };
enum class EventType { KeyDown, TextInput };
struct InputEvent { EventType type{}; bool ctrl_down{}, alt_down{}; int key{}; std::string text; };
void begin_frame(const std::shared_ptr<core::Context>&, float, Vec2, bool) { phases.push_back(Phase::begin); }
void begin_window(std::string_view, Vec2, Vec2, bool, bool) {}
void wrapped_label(std::string_view, float) {}
void end_window() {}
void end_frame() { phases.push_back(Phase::end); }
}
namespace project_gui {
enum class PointerDelivery { ordered };
enum class TextScaling { canvas, desktop };
}
namespace project_gui_runtime {
struct Float2 { float x{}, y{}; };
enum class RuntimeCode { ready, no_change, rejected };
enum class EventKind { activated, other };
struct RuntimeSession {
    struct Widget { unsigned id; std::string_view name; };
    struct Artifact { std::array<Widget,1> widgets{{{1,"Status"}}}; } authored;
    std::string value{"Idle"};
    const Artifact* artifact() const { return &authored; }
    std::optional<std::string_view> text(unsigned id) const {
        if (id != 1) return std::nullopt;
        return value;
    }
    RuntimeCode set_text(unsigned id, std::string text) {
        if (id != 1) return RuntimeCode::rejected;
        value = std::move(text);
        return RuntimeCode::ready;
    }
};
}
struct Adapter {
    struct Event { project_gui_runtime::EventKind kind; std::string_view action; };
    struct Frame { std::vector<Event> events; };
    bool pendingInput{true};
    std::string paintedValue;
    Frame render(project_gui_runtime::RuntimeSession& runtime, project_gui_runtime::Float2,
        auto command, project_gui::PointerDelivery delivery, project_gui::TextScaling text,
        project_gui_runtime::Float2) {
        assert(delivery == project_gui::PointerDelivery::ordered);
        assert(text == project_gui::TextScaling::desktop);
        phases.push_back(Phase::paint);
        paintedValue = runtime.value;
        if (!pendingInput) return {};
        pendingInput = false;
        assert(command({.type=gui::EventType::TextInput,.text="1"}));
        return {{ {project_gui_runtime::EventKind::other,"ignored"},
                  {project_gui_runtime::EventKind::activated,"stop"} }};
    }
};
struct Host {
    project::GuiApplication* m_guiApplication{};
    std::unique_ptr<project_gui_runtime::RuntimeSession> m_projectGuiRuntime{std::make_unique<project_gui_runtime::RuntimeSession>()};
    std::unique_ptr<Adapter> m_projectGuiAdapter{std::make_unique<Adapter>()};
    std::string m_projectGuiStatus;
    void finish_runtime_screenshot(core::Context*) { phases.push_back(Phase::capture); }
FRAME_OWNERS
};

struct PollingApplication : project::GuiApplication {
    std::thread::id owner{std::this_thread::get_id()};
    unsigned polls{};
    bool stopped{};
    bool handle(project::GuiApplicationEvent event, project::GuiApplicationView) override {
        assert(std::this_thread::get_id() == owner);
        if (event.kind == project::GuiApplicationEventKind::text) {
            assert(event.value == "1");
            phases.push_back(Phase::keyboard);
        } else {
            assert(event.kind == project::GuiApplicationEventKind::action && event.value == "stop");
            phases.push_back(Phase::action);
            stopped = true;
        }
        return true;
    }
    void on_frame(project::GuiApplicationView view) override {
        assert(std::this_thread::get_id() == owner && stopped);
        phases.push_back(Phase::poll);
        std::string_view status;
        assert(view.get_text(view.context,"Status",status));
        assert(status == (polls == 0 ? "Idle" : "Stopped"));
        ++polls;
        assert(view.set_text(view.context,"Status","Stopped"));
    }
};
struct ExistingApplication : project::GuiApplication {
    bool handle(project::GuiApplicationEvent, project::GuiApplicationView) override { return true; }
};
int main() {
    auto context = std::make_shared<core::Context>();
    PollingApplication application;
    Host host{.m_guiApplication=&application};
    host.frame_gui_application(context,.016f,{},false,{520.f,440.f},{});
    assert((phases == std::vector{Phase::clear,Phase::begin,Phase::paint,Phase::keyboard,
        Phase::action,Phase::poll,Phase::end,Phase::present,Phase::capture}));
    assert(application.polls == 1 && host.m_projectGuiAdapter->paintedValue == "Idle");
    phases.clear();
    context->type = core::ContextType::OpenGL;
    host.frame_gui_application(context,.016f,{},false,{520.f,440.f},{});
    assert((phases == std::vector{Phase::begin,Phase::paint,Phase::poll,Phase::end,
        Phase::present,Phase::capture}));
    assert(application.polls == 2 && host.m_projectGuiAdapter->paintedValue == "Stopped");
    host.m_projectGuiRuntime.reset();
    host.frame_gui_application(context,.016f,{},false,{520.f,440.f},{});
    assert(application.polls == 2);

    ExistingApplication legacy;
    Host unchanged{.m_guiApplication=&legacy};
    unchanged.frame_gui_application(context,.016f,{},false,{520.f,440.f},{});
    assert(unchanged.m_projectGuiRuntime->value == "Idle");
}
'''.replace("FRAME_OWNERS", owners)
        report = ROOT / "Artifacts/Reports/gui-application-frame" / str(time.time_ns())
        report.mkdir(parents=True)
        fixture = report / "contract.cpp"
        fixture.write_text(source, encoding="utf-8")
        includes = [ROOT / "Engine/include"]
        kits = windows_sdk_includes(Path("C:/Program Files (x86)/Windows Kits/10"))
        if kits:
            includes.extend(kits[-1] / folder for folder in ("um", "shared"))
        records = compile_and_run(self, fixture, report, includes=includes, windows_flags=("/DNOMINMAX",))
        (report / "records.json").write_text(json.dumps(records, indent=2), encoding="utf-8")
