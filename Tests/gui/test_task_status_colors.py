"""Execute existing task menu, toolbar and scheduler row presentation without graphics."""
from pathlib import Path
import hashlib
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
BOOTSTRAP = ROOT / "Engine/src/editor/application/editor.frame_bootstrap.inl"
MENU = ROOT / "Engine/src/editor/application/editor.frame_command_menus.inl"
PANEL = ROOT / "Engine/src/editor/systems/editor.systems_panel.cpp"
API = ROOT / "Engine/modules/gui.engine.ixx"
SCHEDULER = ROOT / "Engine/modules/editor.task_scheduler.ixx"
GRAPH = ROOT / "Engine/modules/taskgraph.dotsystem.ixx"


def statement(source, marker):
    start = source.index(marker)
    return source[start:source.index(";", start) + 1]


def fixture():
    api, graph, scheduler = (p.read_text(encoding="utf-8") for p in (API, GRAPH, SCHEDULER))
    bootstrap, menu, panel = (p.read_text(encoding="utf-8") for p in (BOOTSTRAP, MENU, PANEL))
    tone = definition(api, "    export enum class SemanticTone", True).replace("export ", "")
    graph_types = "\n".join(definition(graph, marker, True) for marker in (
        "    enum class NodeState", "    struct NodeDiagnostic", "    struct GraphEdgeDiagnostic final",
        "    struct GraphSnapshot"))
    activity = definition(scheduler, "    struct ActivitySnapshot final", True)
    activity += "\n" + definition(scheduler, "    [[nodiscard]] inline ActivitySnapshot activity_snapshot(")
    row_tone = definition(panel, "        [[nodiscard]] constexpr gui::SemanticTone scheduler_row_tone(")
    start = menu.index("            const auto show_task_status =", menu.index('open_dropdown("Tasks"'))
    menu_body = menu[start:menu.index('            menu_item("Open Tasks & Threads"', start)]
    row_label = statement(panel, "                const std::string label = row.row->label")
    start = panel.index("                if (gui::menu_item({.label = label")
    row_control = panel[start:panel.index("            {", start)]
    toolbar = statement(bootstrap, "        toolbarStatusItems[toolbarTaskIndex].tone =")
    start = bootstrap.index("        toolbarStatusItems[toolbarTaskIndex].tooltip =")
    tooltip = bootstrap[start:bootstrap.index("        toolbarStatusItems[toolbarFpsIndex].tone =", start)]
    labels = []
    for marker in ('epochengine::format_text("Thread {} / Queued {} / Idle {}"',
                   'epochengine::format_text("{}T / {}Q / {}I"'):
        start = bootstrap.index(marker)
        end = bootstrap.index("toolbarActivity.idle_workers)", start) + len("toolbarActivity.idle_workers)")
        labels.append(bootstrap[start:end])
    return PREFIX + "\nnamespace gui {\n" + tone + GUI_PORTS + "\n}\n" + \
        "namespace epochengine::taskgraph {\n" + graph_types + "\n}\n" + \
        "namespace taskgraph=epochengine::taskgraph;\nnamespace editor_tasks {\n" + activity + "\n}\n" + \
        PORTS + row_tone + "\nvoid build_menu(Editor& editor, std::size_t toolbarRunning, std::size_t toolbarQueued, const editor_tasks::ActivitySnapshot& toolbarActivity) {\n" + menu_body + "\n}\n" + \
        "void toolbar_status(std::size_t toolbarRunning,std::size_t toolbarQueued) {\n" + toolbar + tooltip + "\n}\n" + \
        "std::string toolbar_label(std::size_t toolbarRunning,std::size_t toolbarQueued, const editor_tasks::ActivitySnapshot& toolbarActivity,bool compact) { return compact ? " + labels[1] + " : " + labels[0] + ";}\n" + \
        "bool task_row(const RowView& row, bool selected, float width) {\n" + row_label + row_control + "{ return true; } return false; }\n" + CHECKS


PREFIX = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#pragma warning(push)
#pragma warning(disable:4702)
#include "core.format_text.hpp"
#pragma warning(pop)
#include <gui/system_workspace.hpp>
namespace gui_lib=epochengine::gui_lib;
'''

GUI_PORTS = r'''
struct Vec2 {float x{},y{};};
struct MenuItemOptions { std::string_view label{}; Vec2 size{}; SemanticTone tone{SemanticTone::neutral}; bool selected{}; };
struct Recorded {std::string label{};Vec2 size{};SemanticTone tone{};bool selected{};};
Recorded observed{};
bool menu_item(MenuItemOptions options) {observed={std::string(options.label),options.size,options.tone,options.selected};return true;}
'''

PORTS = r'''
using Tone=gui::SemanticTone;
using Status=gui_lib::system_workspace::RowStatus;
struct Scheduler {
 taskgraph::GraphSnapshot graph{}; unsigned reads{}; std::size_t requested_nodes{999};
 taskgraph::GraphSnapshot snapshot(std::size_t maximum) {++reads;requested_nodes=maximum;return graph;}
};
enum class TopMenu {None,Tasks};
enum class EditorMainSurface {Systems};
struct Editor {Scheduler* taskScheduler{};TopMenu openMenu{TopMenu::Tasks};};
EditorMainSurface opened{};std::string origin{};
void open_editor_surface(EditorMainSurface surface,std::string_view reason) {opened=surface;origin=reason;}
struct Command {std::string label;Tone tone;std::function<void()> action;};
std::vector<Command> commands;
void menu_item(std::string_view label,std::function<void()> action,gui::MenuItemOptions options={}) {commands.push_back({std::string(label),options.tone,std::move(action)});}
struct ToolbarItem {Tone tone{};std::string_view tooltip{};};
constexpr std::size_t toolbarTaskIndex=0;
std::array<ToolbarItem,1> toolbarStatusItems{};
struct Row {std::string label;Status status;};struct RowView {const Row* row;};
'''

CHECKS = r'''
int main() {
 Scheduler scheduler;
 scheduler.graph.workerCount=4;
 auto activity=editor_tasks::activity_snapshot(scheduler.graph);
 assert(activity.idle_workers==4&&activity.running_tasks==0);
 Editor editor{&scheduler};
 build_menu(editor,0,0,activity);
 assert(commands.size()==6&&scheduler.reads==1&&scheduler.requested_nodes==0);
 assert(commands[0].label=="Active work: 0"&&commands[0].tone==Tone::muted);
 assert(commands[1].label=="Queued work: 0"&&commands[1].tone==Tone::muted);
 assert(commands[2].label=="Idle Editor workers: 4 / 4"&&commands[2].tone==Tone::muted);
 assert(commands[3].tone==Tone::muted&&commands[4].tone==Tone::muted&&commands[5].tone==Tone::neutral);
 toolbar_status(0,0);assert(toolbarStatusItems[0].tone==Tone::muted);
 assert(toolbarStatusItems[0].tooltip.find("Planet workers: Planet LOD")!=std::string::npos);
 assert(toolbar_label(0,0,activity,false)=="Thread 0 / Queued 0 / Idle 4");
 assert(toolbar_label(0,0,activity,true)=="0T / 0Q / 4I");
 scheduler.graph.runningCount=2;scheduler.graph.queueCount=3;scheduler.graph.pendingCount=1;
 scheduler.graph.outstandingCount=6;scheduler.graph.completedCount=7;scheduler.graph.failedCount=2;scheduler.graph.cancelledCount=5;
 activity=editor_tasks::activity_snapshot(scheduler.graph);
 assert(activity.idle_workers==2&&activity.running_tasks==2&&activity.queued_tasks==3&&activity.waiting_tasks==1);
 commands.clear();build_menu(editor,activity.running_tasks,activity.queued_tasks+activity.waiting_tasks,activity);
 assert(commands[0].label=="Active work: 2"&&commands[0].tone==Tone::info);
 assert(commands[1].label=="Queued work: 4"&&commands[1].tone==Tone::warning);
 assert(commands[2].label=="Idle Editor workers: 2 / 4");
 assert(commands[3].label=="Completed Editor tasks: 7"&&commands[3].tone==Tone::success);
 assert(commands[4].label=="Failed Editor tasks: 2"&&commands[4].tone==Tone::error);
 assert(commands[5].label=="Cancelled Editor tasks: 5"&&commands[5].tone==Tone::neutral);
 commands[4].action();assert(editor.openMenu==TopMenu::None&&opened==EditorMainSurface::Systems&&origin=="Tasks status");
 toolbar_status(0,4);assert(toolbarStatusItems[0].tone==Tone::warning);
 toolbar_status(2,4);assert(toolbarStatusItems[0].tone==Tone::info);
 assert(toolbar_label(2,4,activity,false)=="Thread 2 / Queued 4 / Idle 2");
 assert(toolbar_label(2,4,activity,true)=="2T / 4Q / 2I");
 // A model job can be active while all Editor workers are sleeping.
 scheduler.graph.runningCount=0;activity=editor_tasks::activity_snapshot(scheduler.graph);
 commands.clear();build_menu(editor,1,0,activity);
 assert(commands[0].label=="Active work: 1"&&commands[2].label=="Idle Editor workers: 4 / 4");
 editor.taskScheduler=nullptr;commands.clear();build_menu(editor,0,0,{});
 assert(commands.size()==3&&commands[2].label=="Idle Editor workers: 0 / 0");
 constexpr std::array statuses{Status::unknown,Status::healthy,Status::idle,Status::queued,Status::running,Status::waiting,Status::blocked,Status::failed,Status::cancelled,Status::disabled};
 constexpr std::array tones{Tone::muted,Tone::success,Tone::muted,Tone::warning,Tone::info,Tone::warning,Tone::warning,Tone::error,Tone::neutral,Tone::muted};
 for(std::size_t i=0;i<statuses.size();++i)for(bool selected:{false,true}) {
  Row row{"Actual task",statuses[i]};assert(task_row({&row},selected,220));
  assert(gui::observed.label=="Actual task  |  "+std::string(gui_lib::system_workspace::row_status_name(row.status)));
  assert(gui::observed.tone==tones[i]&&gui::observed.selected==selected);
  assert(gui::observed.size.x==220&&gui::observed.size.y==28);
 }
 return 0;
}
'''


class TaskStatusColors(unittest.TestCase):
    def test_actual_menu_toolbar_and_scheduler_rows(self):
        report = ROOT / "Artifacts/Reports/task-status-colors-nov01" / str(time.time_ns())
        report.mkdir(parents=True)
        owners = [BOOTSTRAP, MENU, PANEL, API, SCHEDULER, GRAPH, Path(__file__)]
        hashes = lambda: {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in owners}
        before = hashes()
        source = report / "consumer.cpp"
        source.write_text(fixture(), encoding="utf-8")
        records = compile_and_run(self, source, report, includes=(ROOT / "Engine/include", ROOT / "Engine/dep/EpochGui/include"), windows_flags=("/W4", "/WX"))
        self.assertEqual(before, hashes(), "Task presentation source changed during CPU proof")
        (report / "result.json").write_text(json.dumps({"passed": True, "source_sha256": before,
            "invocations": records, "graphics_started": False,
            "ports": "Recording GUI/menu actions and a read-only scheduler snapshot; actual production count, label and semantic-tone bodies."}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
