"""Selected-model picker and actual primary action dispatch, without inference."""
from pathlib import Path
import hashlib
import json
import time
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run
from Tools.source.owned_source_text import read_owned_source

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/application/editor.frame_model_manager.inl"


class ModelManagerPrimaryActions(unittest.TestCase):
    def test_selected_model_actions_precede_settings_and_keep_callbacks(self):
        owner = read_owned_source(OWNER)
        picker_start = owner.index("                const auto chooseRegisteredGroup =")
        action_start = owner.index("                const std::string target = nativeModelId.empty()")
        self.assertLess(owner.index("                chooseRegisteredGroup(true);", picker_start), action_start)
        self.assertLess(action_start, owner.index("                    auto [draftIt, inserted] = editor.aiNativeOffloadDraft"))
        self.assertLess(action_start, owner.index('gui::section_heading("Load a saved model group'))
        self.assertLess(action_start, owner.index('gui::section_heading("Model assignments")'))
        self.assertEqual(owner.count("const std::array nativeActions{"), 1)
        picker = definition(owner, "                const auto chooseRegisteredGroup =", True)
        actions = owner[action_start:owner.index("                for (const auto& model : nativeState.models)", action_start)]
        layout = definition(owner, "            const gui::InlineButtonRowOptions managerActionLayout", True)
        code = CODE.replace("ACTUAL_PICKER", picker).replace("ACTUAL_ACTIONS", actions).replace("ACTUAL_LAYOUT", layout)
        report = ROOT / "Artifacts/Reports/model-manager-primary-actions-nov06" / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / "consumer.cpp"
        source.write_text(code, encoding="utf-8")
        records = compile_and_run(self, source, report, includes=[ROOT / "Engine/include"],
                                  windows_flags=("/W4", "/WX"))
        (report / "result.json").write_text(json.dumps({"passed":True,
            "scope":"Actual selected-model picker, fitted action row and dispatch with recording GUI/native/approval/dialog ports. No GUI/model/native inference.",
            "owner_sha256":hashlib.sha256(OWNER.read_bytes()).hexdigest(),"configurations":records},indent=2)+"\n",encoding="utf-8")


CODE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
#include "epoch.edition.hpp"
namespace gui {
enum class SemanticTone {warning};enum class InlineButtonSizing {FitContent};enum class InlineButtonAlignment {Center};
struct Vec2{float x,y;};
struct InlineButtonSpec{std::string_view label;float width{};bool enabled=true;};
struct InlineButtonRowOptions{InlineButtonSizing sizing;InlineButtonAlignment alignment;bool wrap;float available_width;};
struct SelectBoxOptions{std::string_view id,placeholder,selected;std::vector<std::string_view>options;Vec2 size;bool fit_content;InlineButtonAlignment alignment;unsigned max_visible_options;bool enabled;};
struct SelectBoxResult{bool changed;std::optional<std::size_t>selected_index;};
std::optional<std::size_t>choice,clicked;std::vector<std::string>labels;std::array<bool,6>enabled;
bool centered{},fitted{},wrapped{};float width{};
void section_heading(std::string_view,SemanticTone){}void semantic_block(std::string_view,SemanticTone,float){}void label(std::string_view){}
SelectBoxResult select_box(const SelectBoxOptions&options){auto selected=choice;choice.reset();return {selected.has_value()&&options.enabled,options.enabled?selected:std::nullopt};}
template<std::size_t N>std::optional<std::size_t>inline_button_row(const std::array<InlineButtonSpec,N>&buttons,float height,float,const InlineButtonRowOptions&options){
 static_assert(N==6);assert(height==30);labels.clear();for(std::size_t i=0;i<N;++i){labels.emplace_back(buttons[i].label);enabled[i]=buttons[i].enabled;assert(buttons[i].width==0);}
 centered=options.alignment==InlineButtonAlignment::Center;fitted=options.sizing==InlineButtonSizing::FitContent;wrapped=options.wrap;width=options.available_width;
 auto result=clicked;clicked.reset();return result&&*result<N&&buttons[*result].enabled?result:std::nullopt;
}
}
namespace native {
struct Behavior{bool variant{};bool flagged()const{return variant;}};
struct Model{std::string id;bool active{};Behavior behavior;};struct State{std::vector<Model>models;std::string model="flash";bool busy{};};
std::string operation,target;bool accepted=true;void reset(){operation.clear();target.clear();accepted=true;}
bool note(std::string name,std::string id={}){operation=std::move(name);target=std::move(id);return accepted;}
bool refresh(){return note("refresh");}bool install(std::string id){return note("install",std::move(id));}
bool load(std::string id,bool selected){assert(!selected);return note("load_additional",std::move(id));}
bool use_loaded(std::string id){return note("use_loaded",std::move(id));}bool prepare(std::string id){return note("prepare",std::move(id));}
bool unload(std::string id){return note("unload",std::move(id));}void cancel_setup(){(void)note("cancel");}
bool import_model_profiles(std::string path){return note("import",std::move(path));}
}
namespace epochengine::ai {bool confirmed=true;bool is_model_use_confirmed(){return confirmed;}namespace model_behavior{constexpr std::string_view warning="variant warning";}}
namespace ai=epochengine::ai;
namespace editor_ai_development_panel {
enum class ModelTransport{local_inference};bool allow=true;
struct Panel{bool select_model_transport(ModelTransport){return allow;}};
}
namespace platform::filesystem {
struct Dialog{bool accepted{},cancelled{};std::filesystem::path path;std::string diagnostic;};Dialog next;
Dialog open_file_dialog(const wchar_t*,const wchar_t*,const wchar_t*,const std::filesystem::path&){return next;}
}
struct Editor{
 bool packageModelInstall{},aiModelManagerAutoRescanPending{};int aiMethodSelection=0;
 std::string aiModelPendingUnregister,aiModelManagerFeedback;std::uint64_t aiChatUserModelSelectionRevision{};
 std::unique_ptr<editor_ai_development_panel::Panel>aiDevelopmentPanel;
};unsigned chatSwitches{};bool select_native_chat_transport(Editor&){++chatSwitches;return true;}
void draw(Editor&editor,const native::State&nativeState,std::string&nativeModelId,bool modelRequestActive=false,float managerWidth=720){
 std::vector<std::size_t>modelDisplayOrder;std::vector<std::string>labels;
 for(std::size_t i=0;i<nativeState.models.size();++i){modelDisplayOrder.push_back(i);labels.push_back(nativeState.models[i].id);}
 ACTUAL_LAYOUT
 ACTUAL_PICKER
 chooseRegisteredGroup(false);chooseRegisteredGroup(true);
 ACTUAL_ACTIONS
}
int main(){
 Editor editor;native::State state{{{"9b",false,{}},{"flash",true,{}}},"flash",false};std::string selected;
 gui::choice=0;gui::clicked=3;draw(editor,state,selected);
 assert(selected=="9b"&&native::operation=="prepare"&&native::target=="9b");
 assert(editor.aiChatUserModelSelectionRevision==2);
 assert(gui::labels==std::vector<std::string>({"Rescan","Install Only","Load Additional","Load & Use","Unload Selected","Import Model Profile..."}));
 for(float width:{160.f,320.f,720.f}){draw(editor,state,selected,false,width);assert(gui::fitted&&gui::centered&&gui::wrapped&&gui::width==width);}
 native::reset();gui::clicked=1;draw(editor,state,selected);assert(native::operation=="install"&&native::target=="9b");
 const auto revision=editor.aiChatUserModelSelectionRevision;
 native::reset();gui::clicked=2;draw(editor,state,selected);assert(native::operation=="load_additional"&&chatSwitches==0&&editor.aiChatUserModelSelectionRevision==revision);
 editor.aiMethodSelection=-1;gui::clicked=2;draw(editor,state,selected);assert(chatSwitches==1);editor.aiMethodSelection=0;
 state.models[0].active=true;gui::clicked=3;draw(editor,state,selected);assert(native::operation=="use_loaded"&&native::target=="9b");
 gui::clicked=4;draw(editor,state,selected);assert(native::operation=="unload"&&native::target=="9b");
 state.busy=true;gui::clicked=4;draw(editor,state,selected);assert(native::operation=="cancel"&&gui::labels[4]=="Cancel Setup");state.busy=false;
 for(bool active:{true,false}){native::reset();gui::clicked=3;draw(editor,state,selected,active);assert(active?native::operation.empty():native::operation=="use_loaded");}
 editor.packageModelInstall=true;native::reset();gui::clicked=3;draw(editor,state,selected);assert(native::operation.empty());editor.packageModelInstall=false;
 native::reset();editor.aiModelManagerAutoRescanPending=true;gui::clicked=0;draw(editor,state,selected);assert(native::operation.empty());editor.aiModelManagerAutoRescanPending=false;
 gui::clicked=0;draw(editor,state,selected);assert(native::operation=="refresh");
 platform::filesystem::next={true,false,std::filesystem::path{"Assets/profile/manifest.json"},{}};gui::clicked=5;draw(editor,state,selected);
 assert(native::operation=="import"&&native::target=="Assets/profile/manifest.json"&&editor.aiModelManagerFeedback.find("Import queued")!=std::string::npos);
 platform::filesystem::next={false,true,{},{}};native::reset();gui::clicked=5;draw(editor,state,selected);
 assert(native::operation.empty()&&editor.aiModelManagerFeedback=="Profile import cancelled.");
}
'''

if __name__ == "__main__":
    unittest.main()
