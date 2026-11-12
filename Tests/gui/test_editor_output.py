"""Actual Output classification/cache, retained log and dock header with GUI ports."""
from pathlib import Path
from Tests.support.cpp import compile_and_run
import hashlib,json,subprocess,unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]
APP=ROOT/'Engine/src/editor/application'

class EditorOutput(unittest.TestCase):
 def test_actual_output_filter_and_header_debug_release(self):
  owner=(APP/'editor.application_pane_routing.inl').read_text(encoding='utf-8')
  helpers='\n'.join(definition(owner,signature) for signature in (
   '        void push_editor_log(',
   '        [[nodiscard]] static bool editor_output_contains(',
   '        [[nodiscard]] static EditorWorkspaceTab editor_output_category(',
   '        [[nodiscard]] static const std::vector<std::string>& filtered_editor_output(',
   '        static void record_editor_run_output('))
  dock=(APP/'editor.frame_bottom_docks.inl').read_text(encoding='utf-8')
  header=dock[dock.index('        if (editor.dockStatusTab =='):dock.index('        auto dockLine =')]
  choice=dock[dock.index('        if (!editor.outputShowDetails'):dock.index('        else switch (editor.dockStatusTab)')]
  core=(ROOT/'Engine/modules/editor/editor.core.ixx').read_text(encoding='utf-8')
  enum=definition(core,'    export enum class EditorWorkspaceTab',True).replace('export ','',1)
  code=r'''
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#define CHECK(c) do{if(!(c))return __LINE__;}while(false)
ENUM
struct EditorState{
 std::string aiDiagnosticEngineRoot,projectId="project",outputSearch,outputFilterSearch,outputLastRunDetail;
 std::vector<std::string>logLines,outputFilteredLines;
 std::uint64_t outputLogRevision{1},outputFilterRevision{},outputScrollToEndGeneration{};
 EditorWorkspaceTab dockStatusTab{EditorWorkspaceTab::Output},outputFilterTab{EditorWorkspaceTab::Output};
 bool outputGeneralOnly{},outputFilterGeneral{},outputShowDetails{},outputFollowTail{true};int mainSurface{};
};
unsigned archived{};
namespace editor_ai_session_diagnostics{
std::filesystem::path fallback_engine_root(){return{};}
bool record_engine(const std::filesystem::path&,std::string_view,std::string_view,std::string_view){++archived;return true;}}
HELPERS
namespace epochengine{template<class...T>std::string format_text(const char*,const T&...){return "recorded-output-id";}}
int editor_main_surface_index(int value){return value;}
namespace gui{
struct Vec2{float x{},y{};};Vec2 cursor;
struct SegmentedButtonSpec{std::string_view label;float width{};bool active{};};
enum class TextInputKeyboardPolicy{focused};struct EditBoxResult{bool changed{};};
std::optional<std::string>search;std::optional<std::string_view>select;bool detailsClick{};
std::vector<std::string>filterLabels,shown;unsigned details{},panels{},rows{};float widest{};
Vec2 cursor_position(){return cursor;}void set_cursor(Vec2 p){cursor=p;}
float segmented_button_row_width(std::span<const SegmentedButtonSpec>items,float gap){float w{};
 for(const auto&i:items)w+=i.width>0?i.width:float(i.label.size())*7.f+20.f;
 return w+(items.empty()?0.f:float(items.size()-1)*gap);}
std::optional<std::size_t>segmented_button_row(std::span<const SegmentedButtonSpec>items,float,float gap){
 ++rows;widest=std::max(widest,segmented_button_row_width(items,gap));cursor.y+=31;
 std::optional<std::size_t>chosen;for(std::size_t i=0;i<items.size();++i){filterLabels.emplace_back(items[i].label);
 if(select&&items[i].label==*select)chosen=i;}return chosen;}
EditBoxResult edit_box(std::string&value,Vec2,std::size_t,bool,TextInputKeyboardPolicy,bool,std::string_view){
 cursor.y+=30;if(search){value=*search;search.reset();return{true};}return{};}
bool button_selected(std::string_view,Vec2,bool){cursor.y+=30;const bool clicked=detailsClick;detailsClick=false;return clicked;}
struct ScrollTextPanelOptions{std::string_view id;Vec2 size;const std::vector<std::string>&lines;bool incremental_line_measurements;
 std::size_t max_line_chars;bool selectable,stick_to_bottom;std::uint64_t scroll_to_end_generation;bool show_scroll_to_end;};
struct ScrollTextPanelResult{bool at_end{true};};
bool full_lines{},cached_measurements{};
ScrollTextPanelResult scroll_text_panel(const ScrollTextPanelOptions&o){shown=o.lines;full_lines=o.max_line_chars==0;cached_measurements=o.incremental_line_measurements;++panels;return{};}
void fresh(){cursor={};filterLabels.clear();shown.clear();panels=rows=0;widest=0;}
}
void actual_output(EditorState&editor,float width=900){gui::fresh();
const gui::Vec2 console_render_pos{},console_render_size{width,500};
HEADER
CHOICE
else ++gui::details;
}
int main(){
 EditorState state;const std::vector<std::string>input={"[project] Build ready","[assets] Texture ready", "[AI] Model ready", "[context] Focus",
 "[update] Nothing new","plain event","[unclosed event","[run] C:/Project with spaces/failure.log","[assets] caf\xc3\xa9 texture\\n"};
 for(const auto&line:input)push_editor_log(state,line);CHECK(archived==input.size());
 actual_output(state);CHECK(gui::shown==input&&gui::filterLabels.size()==6&&gui::filterLabels[1]=="General"&&gui::full_lines&&gui::cached_measurements);
 for(const auto&line:input){unsigned categories=editor_output_category(line)==EditorWorkspaceTab::Output;
  for(const auto category:{EditorWorkspaceTab::Project,EditorWorkspaceTab::Assets,EditorWorkspaceTab::AI,EditorWorkspaceTab::Systems})categories+=editor_output_category(line)==category;
  CHECK(categories==1);}
 gui::select="General";actual_output(state);gui::select.reset();CHECK(state.outputGeneralOnly&&gui::shown.size()==3);
 gui::search="uPdAtE";actual_output(state);CHECK(gui::shown.size()==1&&gui::shown[0]==input[4]);
 const auto*cache=state.outputFilteredLines.data();const auto revision=state.outputFilterRevision;actual_output(state);
 CHECK(state.outputFilteredLines.data()==cache&&state.outputFilterRevision==revision);
 gui::search="";gui::select="Project";actual_output(state);gui::select.reset();
 CHECK(!state.outputGeneralOnly&&gui::shown.size()==2&&gui::shown[1]==input[7]);
 gui::detailsClick=true;const auto oldDetails=gui::details;actual_output(state);CHECK(state.outputShowDetails&&gui::details==oldDetails+1&&gui::panels==0);
 gui::search="missing";actual_output(state);CHECK(!state.outputShowDetails&&gui::shown.empty());
 gui::select="Assets";gui::search="CAF\xc3\xa9";actual_output(state);gui::select.reset();CHECK(gui::shown.size()==1&&gui::shown[0]==input[8]);
 gui::search="texture\\n";actual_output(state);CHECK(gui::shown.size()==1&&gui::shown[0]==input[8]);
 for(float width:{100.f,180.f,320.f,900.f}){gui::select="All";gui::search="";actual_output(state,width);gui::select.reset();
  CHECK(gui::filterLabels.size()==6&&gui::widest<=width-16&&gui::shown==input);}
 const std::string detail="Run failed; C:/Project with spaces/Artifacts/Reports/"+std::string(1500,'x')+".log";
 record_editor_run_output(state,detail);const auto recorded=state.logLines.size();
 CHECK(state.logLines.back()=="[run] "+detail);record_editor_run_output(state,detail);CHECK(state.logLines.size()==recorded);
 record_editor_run_output(state,"");record_editor_run_output(state,detail);CHECK(state.logLines.size()==recorded+1);
 EditorState bounded;for(unsigned i=0;i<520;++i)push_editor_log(bounded,std::to_string(i));
 CHECK(bounded.logLines.size()==512&&bounded.logLines.front()=="8"&&bounded.logLines.back()=="519");
 return 0;
}
'''.replace('ENUM',enum).replace('HELPERS',helpers).replace('HEADER',header).replace('CHOICE',choice)
  proof=ROOT/'Artifacts/Reports/editor-output-oct29'/hashlib.sha256(code.encode()).hexdigest()[:12];proof.mkdir(parents=True,exist_ok=True)
  cpp=proof/'output.cpp';cpp.write_text(code,encoding='utf-8')
  results=compile_and_run(self,cpp,proof)
  (proof/'result.json').write_text(json.dumps({'passed':True,'cases':results,'scope':'Actual log/category/search/cache/Run-detail owner and actual Output header/render choice with named GUI/archive ports; no native GUI/disk archive claim.'},indent=2),encoding='utf-8')

if __name__=='__main__':unittest.main()
