"""Actual Model Manager idle entry, Save dispatch and native JSON roundtrip.

GUI editing/clicks and the native queue/host are recording ports. No window,
model, user profile or worker is started or changed by this CPU consumer.
"""
from pathlib import Path
import hashlib
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run, dependency_include

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/editor/application/editor.frame_model_manager.inl"


def fixture():
    owner = OWNER.read_text(encoding="utf-8")
    runtime = (ROOT / "Engine/src/ai/ai.native_runtime.cpp").read_text(encoding="utf-8")
    api = (ROOT / "Engine/src/ai/ai.native_runtime.hpp").read_text(encoding="utf-8")
    idle = owner[owner.index("                        auto& idleMinutesText ="):
                 owner.index('                        gui::wrapped_label("The idle timeout applies')]
    actions = owner[owner.index("                        const std::array actionButtons{"):
                    owner.index("                        if (const auto action=gui::inline_button_row(actionButtons")]
    dispatch = definition(owner, "                        if (const auto action=gui::inline_button_row(actionButtons")
    defaults = definition(owner, "                        if (native::save_model_tuning(selectedNativeId, defaults))")
    model_tuning = definition(api, "    struct ModelTuning final", True)
    json_owner = "\n".join(definition(runtime, marker) for marker in (
        "std::string string_field(", "ModelTuning model_tuning_from_json(",
        "epoch_ai::runtime::json model_tuning_json(", "bool save_model_tuning("))
    return r'''
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <nlohmann/json.hpp>
namespace epoch_ai::runtime {using json=nlohmann::json;}
namespace native {
MODEL_TUNING
struct Error {std::string message;};
struct Host {
 std::unordered_map<std::string,epoch_ai::runtime::json> profiles;
 unsigned writes{};
 std::expected<void,Error> set_native_model_tuning(const std::string& id,const epoch_ai::runtime::json& value){
  ++writes;profiles[id]=value;return {};
 }
} host;
bool accept_queue=true;unsigned queued{};std::string phase_text;
void phase(std::string text){phase_text=std::move(text);}
template<class Callback>bool queue(const std::string&,Callback callback){
 ++queued;if(!accept_queue)return false;callback(host,std::stop_token{});return true;
}
JSON_OWNER
}
namespace gui {
struct Vec2 {float x,y;};
enum class InlineButtonSizing {FitContent};
enum class InlineButtonAlignment {Center};
struct InlineButtonRowOptions {};
struct InlineButtonSpec {std::string_view label;float width{};bool enabled=true;};
struct EditBoxResult {bool changed{};};
bool suppressed=false;std::optional<std::string> typed;std::optional<std::size_t> clicked;
bool save_enabled{},discard_enabled{};std::string notice,label_text;Vec2 edit_size;std::size_t edit_limit{};
struct ScopedInputSuppression {
 bool previous;
 explicit ScopedInputSuppression(bool disabled):previous(suppressed){suppressed=suppressed||disabled;}
 ~ScopedInputSuppression(){suppressed=previous;}
};
void label(std::string_view text){label_text=text;}
void wrapped_label(std::string_view text,float){notice=text;}
EditBoxResult edit_box(std::string& text,Vec2 size,std::size_t limit,bool){
 edit_size=size;edit_limit=limit;
 if(typed&&!suppressed){text=typed->substr(0,limit);typed.reset();return {true};}
 typed.reset();return {};
}
template<std::size_t N>std::optional<std::size_t> inline_button_row(
 const std::array<InlineButtonSpec,N>& buttons,float,float,const InlineButtonRowOptions&){
 save_enabled=buttons[0].enabled;discard_enabled=buttons[1].enabled;
 // Return even a disabled click to exercise the owning dispatch's own guards.
 auto result=clicked;clicked.reset();return result;
}
}
struct Editor {
 std::unordered_map<std::string,native::ModelTuning> aiNativeTuningDraft;
 std::unordered_map<std::string,std::string> aiNativeIdleMinutesDraft;
 std::string aiModelManagerFeedback;
};
struct Entry {native::ModelTuning tuning;};
void render(Editor& editor,const std::string& selectedNativeId,const Entry& saved,
 bool canEdit=true,float managerWidth=720){
 const float managerControlWidth=(std::min)(managerWidth,720.0f);
 const gui::InlineButtonRowOptions managerActionLayout;
 auto& tune=editor.aiNativeTuningDraft.try_emplace(selectedNativeId,saved.tuning).first->second;
 const auto* selectedNativeIt=&saved;
 gui::notice.clear();
IDLE_ENTRY
ACTION_BUTTONS
DISPATCH
}
void hardware_defaults(Editor& editor,const std::string& selectedNativeId,native::ModelTuning defaults){
HARDWARE_DEFAULTS
}
#define CHECK(value) do {if(!(value)) return __LINE__;} while(false)
int main(){
 Editor editor;Entry first,second;first.tuning.idle_timeout_minutes=2;second.tuning.idle_timeout_minutes=77;
 first.tuning.context_tokens=81920;first.tuning.gpu_layers="30";first.tuning.threads=12;
 const auto baseline=native::model_tuning_json(first.tuning);
 // A narrow logical control remains exact at 150% scaling: entry is not a pixel/value mapping.
 for(float width:{160.0f,240.0f,480.0f,720.0f}){
  render(editor,"first",first,true,width);
  CHECK(gui::edit_size.x==(std::min)(width,240.0f)&&gui::edit_limit==16);
 }
 CHECK(editor.aiNativeIdleMinutesDraft.at("first")=="2");
 CHECK(gui::label_text.find("minutes")!=std::string::npos&&gui::label_text.find("0 keeps resident")!=std::string::npos);
 // Normal typing over multiple frames; the exact value reaches the real Save queue/serializer.
 gui::typed="2";render(editor,"first",first);
 gui::typed="20";render(editor,"first",first);
 CHECK(editor.aiNativeTuningDraft.at("first").idle_timeout_minutes==20&&gui::save_enabled);
 gui::clicked=0;render(editor,"first",first);
 CHECK(native::host.writes==1&&native::queued==1);
 CHECK(native::model_tuning_from_json(native::host.profiles.at("first")).idle_timeout_minutes==20);
 auto expected=baseline;expected["idle_timeout_minutes"]=20;
 CHECK(native::host.profiles.at("first")==expected);
 CHECK(editor.aiModelManagerFeedback.find("idle timeout applies when saved")!=std::string::npos);
 CHECK(native::phase_text.find("idle timeout applies now")!=std::string::npos);
 // Partial, malformed, fractional, signed, trailing and overflowing values never queue a save.
 for(const std::string invalid:{"","-1","+20","20.0","20 minutes"," 20","20 ","NaN","1441","4294967296","9999999999999999","\xEF\xBC\x92\xEF\xBC\x90"}){
  gui::typed=invalid;gui::clicked=0;render(editor,"first",first);
  CHECK(editor.aiNativeIdleMinutesDraft.at("first")==invalid);
  CHECK(editor.aiNativeTuningDraft.at("first").idle_timeout_minutes==20);
  CHECK(!gui::save_enabled&&gui::discard_enabled&&native::queued==1);
  CHECK(gui::notice.find("0 to 1440")!=std::string::npos);
  render(editor,"first",first);CHECK(editor.aiNativeIdleMinutesDraft.at("first")==invalid);
 }
 // Invalid first-model input is neither transferred to nor overwritten by a different profile.
 render(editor,"second",second);CHECK(editor.aiNativeIdleMinutesDraft.at("second")=="77");
 gui::typed="123";gui::clicked=0;render(editor,"second",second);
 CHECK(native::host.profiles.at("second")["idle_timeout_minutes"]==123);
 render(editor,"first",first);CHECK(!gui::save_enabled&&editor.aiNativeTuningDraft.at("first").idle_timeout_minutes==20);
 gui::clicked=1;render(editor,"first",first);
 CHECK(editor.aiNativeIdleMinutesDraft.at("first")=="2"&&editor.aiNativeTuningDraft.at("first").idle_timeout_minutes==2);
 CHECK(editor.aiNativeIdleMinutesDraft.at("second")=="123");
 // Inclusive limits and explicitly selected automatic/recommended policies retain idle editing.
 for(const char* policy:{"recommended","automatic","manual"})for(const char* valid:{"0","20","1440"}){
  editor.aiNativeTuningDraft.at("first").runtime_policy=policy;
  gui::typed=valid;gui::clicked=0;render(editor,"first",first);
  CHECK(gui::save_enabled);
  CHECK(native::host.profiles.at("first")["idle_timeout_minutes"]==std::stoul(valid));
 }
 const auto writes=native::host.writes;
 const auto saved_value=editor.aiNativeTuningDraft.at("first").idle_timeout_minutes;
 gui::typed="20";gui::clicked=0;render(editor,"first",first,false);
 CHECK(!gui::save_enabled&&!gui::discard_enabled&&native::host.writes==writes);
 CHECK(editor.aiNativeTuningDraft.at("first").idle_timeout_minutes==saved_value);
 // Failed queue leaves the exact draft available; hardware defaults reset only the selected entry.
 native::accept_queue=false;gui::typed="20";gui::clicked=0;render(editor,"first",first);
 CHECK(native::host.writes==writes&&editor.aiNativeIdleMinutesDraft.at("first")=="20");
 CHECK(editor.aiModelManagerFeedback.find("not queued")!=std::string::npos);
 native::accept_queue=true;auto defaults=first.tuning;defaults.idle_timeout_minutes=20;
 hardware_defaults(editor,"first",defaults);
 CHECK(editor.aiNativeIdleMinutesDraft.at("first")=="20");
 CHECK(editor.aiNativeTuningDraft.at("first").idle_timeout_minutes==20);
 CHECK(editor.aiNativeIdleMinutesDraft.at("second")=="123");
 return 0;
}
'''.replace("MODEL_TUNING", model_tuning).replace("JSON_OWNER", json_owner).replace(
        "IDLE_ENTRY", idle).replace("ACTION_BUTTONS", actions).replace("DISPATCH", dispatch).replace(
        "HARDWARE_DEFAULTS", defaults)


class ModelManagerIdleMinutes(unittest.TestCase):
    def test_actual_entry_validation_save_and_model_switch(self):
        report = ROOT / "Artifacts/Reports/model-manager-idle-minutes-oct29" / str(time.time_ns())
        report.mkdir(parents=True)
        source = report / "consumer.cpp"
        source.write_text(fixture(), encoding="utf-8")
        records = compile_and_run(self, source, report, includes=(dependency_include(ROOT),),
                                  windows_flags=("/W4", "/WX"), compile_timeout=90, run_timeout=10)
        (report / "result.json").write_text(json.dumps({
            "scope": "Actual UI/Save/JSON owner; recording GUI/queue/host ports; no native GUI/model proof",
            "owner_sha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(),
            "consumer_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "configurations": records,
        }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
