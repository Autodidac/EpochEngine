"""Production reconnect guard and publication method; no runtime or model load."""
from pathlib import Path
import unittest
from Tools.source.owned_source_text import definition
from Tests.support.cpp import compile_and_run, dependency_include

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / 'Engine/src/ai/native'


class ModelReconnect(unittest.TestCase):
    def test_actual_discovery_buttons_match_original_filename(self):
        owner=(ROOT/'Engine/src/editor/application/editor.application_models_attachments.inl').read_text(encoding='utf-8')
        block=owner[owner.index('                const auto selected = std::ranges::find_if(state.models,'):]
        block=block[:block.index('\n            }\n        }')]
        body=r'''
#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace gui {
struct InlineButtonSpec {std::string_view label;bool enabled;};
std::size_t count{};int click=-1;bool reconnectEnabled{};
std::optional<std::size_t> inline_button_row(std::span<const InlineButtonSpec> actions,float,float,int){
 count=actions.size();reconnectEnabled=count==2&&actions[1].enabled;
 if(click>=0&&static_cast<std::size_t>(click)<actions.size()&&actions[click].enabled)return click;
 return {};
}
}
namespace native {unsigned calls{};bool reconnect_model_file(std::string,std::string){++calls;return true;}}
struct Model {std::string id,local_weights_filename;bool active{};};
struct State {std::vector<Model> models;bool busy{};};
struct File {std::string id;std::filesystem::path path;bool complete=true;};
struct Editor {std::string aiRegisteredModelId,aiRegisteredModelPath;int aiModelManagerSection=2,aiModelManagerTab=3;};
std::string nativeModelId="hauhau";
void render(Editor& editor,const State& state,const File& file){const int layout=0;
BLOCK
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 Editor editor;State state{{{"hauhau","Hauhau-IQ3_M.gguf",false}},false};
 File matching{"new", "D:/models/Hauhau-IQ3_M.gguf"};gui::click=1;
 render(editor,state,{"flash","D:/models/Flash.gguf"});CHECK(gui::count==1&&native::calls==0);
 render(editor,state,{"edge","D:/models/model-skeleton.gguf"});CHECK(gui::count==1&&native::calls==0);
 render(editor,state,matching);CHECK(gui::count==2&&gui::reconnectEnabled&&native::calls==1);
 state.models[0].active=true;render(editor,state,matching);CHECK(!gui::reconnectEnabled&&native::calls==1);
 state.models[0].active=false;state.busy=true;render(editor,state,matching);CHECK(!gui::reconnectEnabled&&native::calls==1);
 state.busy=false;matching.complete=false;render(editor,state,matching);CHECK(!gui::reconnectEnabled&&native::calls==1);
 matching.complete=true;state.models[0].local_weights_filename.clear();render(editor,state,matching);CHECK(gui::count==1&&native::calls==1);
 state.models.clear();render(editor,state,matching);CHECK(gui::count==1&&native::calls==1);
 gui::click=0;render(editor,state,matching);CHECK(editor.aiRegisteredModelId=="new"&&editor.aiModelManagerSection==0);
 return 0;
}
'''.replace('BLOCK',block)
        report=ROOT/'Artifacts/Reports/model-reconnect-ui-20261115'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'contract.cpp';source.write_text(body,encoding='utf-8')
        compile_and_run(self,source,report)

    def test_actual_metadata_reconnect_preserves_profile(self):
        owner = (NATIVE/'ai.backend_model_native_model_manager.cpp').read_text(encoding='utf-8')
        helpers = '\n'.join(definition(owner, marker) for marker in (
            '[[nodiscard]] std::string trim_path_text(',
            '[[nodiscard]] std::expected<std::uint64_t, std::string> local_gguf_set_bytes(',
            '[[nodiscard]] std::expected<std::filesystem::path, std::string> normalize_local_gguf_path(',
            '[[nodiscard]] std::expected<std::filesystem::path, std::string> validate_model_reconnection('))
        method = definition(owner, 'std::expected<nlohmann::json, core::Error> NativeModelManager::reconnect_model_file(')
        report = ROOT/'Artifacts/Reports/model-reconnect-20261115'
        report.mkdir(parents=True, exist_ok=True)
        body = r'''
#include <algorithm>
#include <cstdio>
#include <expected>
#include <map>
#include <mutex>
#include <regex>
#include "ai.backend_model_runtime_model.cpp"
#include "ai.backend_core_error.cpp"
namespace epoch_ai::model {
HELPERS
class NativeModelManager {public:
std::mutex mutex_, slots_mutex_;
std::vector<RuntimeModelProfile> profiles_;
std::map<std::string,int> slots_;
nlohmann::json install_state_={{"state","idle"}},runtime_install_state_={{"state","idle"}};
unsigned saves{};bool failSave{};
std::optional<RuntimeModelProfile> find_locked(std::string_view id){for(const auto& p:profiles_)if(p.id==id)return p;return {};}
void save_profile(const RuntimeModelProfile&){if(failSave)throw std::runtime_error("disk refused");++saves;}
std::expected<nlohmann::json,core::Error> reconnect_model_file(std::string_view,const std::filesystem::path&);
};
METHOD
}
void number(std::ofstream& out,std::uint64_t value,unsigned width){for(unsigned i=0;i<width;++i)out.put(static_cast<char>((value>>(8*i))&255));}
void text(std::ofstream& out,std::string_view value){number(out,value.size(),8);out.write(value.data(),value.size());}
void gguf(const std::filesystem::path& file,std::string_view arch="qwen35",unsigned version=3){
 std::ofstream out(file,std::ios::binary);out.write("GGUF",4);number(out,version,4);number(out,1,8);number(out,2,8);
 text(out,"general.architecture");number(out,8,4);text(out,arch);
 text(out,std::string(arch)+".block_count");number(out,4,4);number(out,32,4);
}
#define CHECK(x) do {if(!(x))return __LINE__;}while(false)
int main(int argc,char** argv){
 if(argc!=2)return 1;namespace fs=std::filesystem;using namespace epoch_ai::model;
 const fs::path root=argv[1];fs::create_directories(root/"old");fs::create_directories(root/"new");
 RuntimeModelProfile original;original.id="selected";original.display_name="My model";original.source=ModelSourceKind::local_gguf;
 original.local_path=root/"old/model.gguf";original.context_tokens=8192;original.capability_tier="high";
 original.behavior_labels={"uncensored"};original.threads=4;original.gpu_layers="7";
 const auto selected=root/"new/model.gguf";gguf(selected);
 NativeModelManager manager;manager.profiles_={original};
 CHECK(!manager.reconnect_model_file("unknown",selected)&&manager.saves==0);
 manager.slots_[original.id]=1;CHECK(!manager.reconnect_model_file(original.id,selected)&&manager.saves==0);manager.slots_.clear();
 manager.install_state_["state"]="installing";CHECK(!manager.reconnect_model_file(original.id,selected));manager.install_state_["state"]="idle";
 gguf(original.local_path);CHECK(!manager.reconnect_model_file(original.id,selected));fs::remove(original.local_path);
 gguf(root/"new/other.gguf");CHECK(!manager.reconnect_model_file(original.id,root/"new/other.gguf"));
 gguf(selected,"clip");CHECK(!manager.reconnect_model_file(original.id,selected));
 gguf(selected,"qwen35",9);CHECK(!manager.reconnect_model_file(original.id,selected));gguf(selected);
 manager.failSave=true;CHECK(!manager.reconnect_model_file(original.id,selected));CHECK(manager.profiles_[0].local_path==original.local_path);manager.failSave=false;
 manager.profiles_[0].estimated_bytes=1000000;CHECK(!manager.reconnect_model_file(original.id,selected));manager.profiles_[0]=original;
 const auto result=manager.reconnect_model_file(original.id,selected);CHECK(result&&manager.saves==1&&!result->at("loaded").get<bool>());
 auto expected=to_json(original);expected["local_path"]=fs::absolute(selected).lexically_normal().string();
 CHECK(to_json(manager.profiles_[0])==expected);
 manager.profiles_[0]=original;manager.profiles_[0].local_path=root/"old/split-00001-of-00002.gguf";
 const auto first=root/"new/split-00001-of-00002.gguf",second=root/"new/split-00002-of-00002.gguf";
 gguf(first);CHECK(!manager.reconnect_model_file(original.id,first));
 {std::ofstream out(second);out<<"not a GGUF";}CHECK(!manager.reconnect_model_file(original.id,first));
 gguf(second);CHECK(manager.reconnect_model_file(original.id,first));
 return 0;
}
'''.replace('HELPERS', helpers).replace('METHOD', method)
        source=report/'contract.cpp'
        source.write_text(body,encoding='utf-8')
        compile_and_run(self,source,report,args=lambda config:[report/config/'fixture'],
                        includes=[NATIVE,dependency_include(ROOT)])


if __name__ == '__main__':
    unittest.main()
