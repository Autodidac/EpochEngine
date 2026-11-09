"""Actual project view controls and completion binding; CPU recording ports only."""
from pathlib import Path
import hashlib
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.packages.test_planetary_stream_completion import condition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "Engine/src/epoch/implementation/epoch.project_play_scene.inl"


class RuntimeEnvironmentControls(unittest.TestCase):
    def test_actual_controls_cancel_and_wait_for_the_exact_view_cut(self):
        source = OWNER.read_text(encoding="utf-8")
        methods = "\n".join(definition(source, marker) for marker in (
            "            [[nodiscard]] bool runtime_water_visibility_pending(",
            "            void set_runtime_water_visible(",
            "            void draw_runtime_environment_controls("))
        marker = "if((!m_planetaryPaused||runtime_water_visibility_pending()) &&"
        outer = "(!m_planetaryPaused||runtime_water_visibility_pending()) &&" + condition(source, marker)
        commit = definition(source, "                                    {m_meshes[m_planetaryEntity]=")
        code = r'''
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace timing {using Clock=std::chrono::steady_clock;}
namespace project_planetary {
struct Settings {std::string mode{"planet"};unsigned body_index{};bool ocean_surface{true},stream_lod{true};
 friend bool operator==(const Settings&,const Settings&)=default;};
bool admitted=true;
std::optional<Settings> active_body_stream_settings(const Settings&s,unsigned body){
 if(!admitted)return {};auto value=s;value.body_index=body;return value;
}
bool stream_view_reusable(int a,int b){return a==b;}
}
namespace gui {
enum class InlineButtonSizing{FitContent};enum class InlineButtonAlignment{Center};
struct InlineButtonSpec{std::string_view label;float width{};bool enabled{true};};
struct Row{InlineButtonSizing sizing{};InlineButtonAlignment alignment{};bool wrap{};float available_width{};};
std::optional<std::size_t>click;std::array<std::string,2>labels;std::array<bool,2>enabled;Row recorded;
template<std::size_t N>std::optional<std::size_t>inline_button_row(const std::array<InlineButtonSpec,N>&items,float,float,Row options){
 static_assert(N==2);recorded=options;for(std::size_t i=0;i<N;++i){labels[i]=items[i].label;enabled[i]=items[i].enabled;}
 auto result=std::exchange(click,{});return result&&items[*result].enabled?result:std::nullopt;
}
}
struct Entity{std::string type;bool shown{true};bool visible()const{return shown;}};
struct Scheduler{std::vector<int>cancels;void cancel(int ticket){cancels.push_back(ticket);}};
struct Result{struct {bool ok{true},cancelled{};std::optional<int>mesh_frame{1};}operation;
 bool snapshot_ready{true};std::shared_ptr<int>snapshot=std::make_shared<int>(7),patch_snapshot=std::make_shared<int>(8);};
struct Runtime{
 std::optional<project_planetary::Settings>m_planetarySettings{project_planetary::Settings{}},
  m_meshPlanetarySettings{project_planetary::Settings{}},m_planetaryRequestSettings{project_planetary::Settings{}};
 bool m_runtimeWaterVisible{true},m_runtimeCloudsVisible{true},m_hasPlanetRecipe{true},m_planetaryPublishedOcean{true};
 bool m_planetaryContinue{},m_planetaryFailed{},m_planetaryWaterSourceRefused{},m_planetaryPaused{},m_explorationController{};
 std::string m_runtimeViewStatus;std::unordered_map<unsigned,int>m_waterCaches{{0,1}};std::vector<Entity>m_entities;
 std::shared_ptr<Scheduler>m_planetaryScheduler=std::make_shared<Scheduler>();
 std::optional<int>m_planetaryPending{1},m_planetaryWaterPending{2};int m_planetaryCancellation{3},m_planetaryWaterCancellation{4};
 std::uint64_t m_planetaryWaterClockGeneration{5};std::shared_ptr<int>m_planetaryWaterPublishedSource=std::make_shared<int>(1);
 std::optional<double>m_planetaryWaterPublishedSeconds{2};timing::Clock::time_point m_planetaryNextTick{},m_planetaryWaterNextTick{};
 unsigned m_planetaryRequestBody{},m_planetaryEntity{7};std::optional<unsigned>m_planetaryLodBody;
 struct{unsigned anchor{};}m_explorationFrame;int m_planetaryView{1};
 std::unordered_map<unsigned,std::shared_ptr<int>>m_meshes;std::optional<int>m_planetaryMeshFrame;
 std::shared_ptr<int>m_planetaryWaterSource;
 void reset_planetary_water(){m_planetaryWaterSource.reset();m_planetaryWaterPublishedSource.reset();}
 ACTUAL_METHODS
 bool accept(Result result,bool admitted=true){const int view=1;
  if(ACTUAL_OUTER){if(result.operation.ok&&!result.operation.cancelled&&result.snapshot_ready&&result.snapshot&&admitted)
    {ACTUAL_COMMIT return true;}}
  return false;
 }
};
#define CHECK(value) do{if(!(value))return __LINE__;}while(false)
int main(){
 Runtime r;const auto authored=*r.m_meshPlanetarySettings;const auto epoch=r.m_planetaryWaterClockGeneration;
 r.m_planetaryPaused=true;r.m_planetarySettings->stream_lod=false;r.m_meshPlanetarySettings->stream_lod=false;
 r.m_planetaryRequestSettings=r.m_planetarySettings;
 r.set_runtime_water_visible(false);
 CHECK(!r.m_runtimeWaterVisible&&!r.m_planetarySettings->ocean_surface&&r.m_planetaryPublishedOcean&&r.runtime_water_visibility_pending());
 CHECK(r.m_planetaryPending&&r.m_planetaryWaterPending&&r.m_planetaryScheduler->cancels==std::vector<int>({3,4}));
 CHECK(r.m_waterCaches.empty()&&r.m_planetaryWaterClockGeneration==epoch+1&&r.m_planetaryContinue&&!r.m_planetaryFailed);
 CHECK(r.m_meshPlanetarySettings->ocean_surface&&!r.m_planetarySettings->stream_lod&&!r.m_meshPlanetarySettings->stream_lod);
 CHECK(!r.accept(Result{})); // a cancelled, old wet cut cannot win
 r.draw_runtime_environment_controls(190);CHECK(gui::labels[1]=="Planet Water: turning off");
 CHECK(gui::recorded.wrap&&gui::recorded.available_width==190&&gui::recorded.alignment==gui::InlineButtonAlignment::Center);
 r.m_planetaryRequestSettings=r.m_planetarySettings;
 auto failed=Result{};failed.operation.ok=false;CHECK(!r.accept(failed)&&r.runtime_water_visibility_pending());
 auto cancelled=Result{};cancelled.operation.cancelled=true;CHECK(!r.accept(cancelled)&&r.runtime_water_visibility_pending());
 CHECK(!r.accept(Result{},false)&&r.runtime_water_visibility_pending());
 CHECK(r.accept(Result{})&&!r.runtime_water_visibility_pending()&&!r.m_planetaryPublishedOcean);
 r.draw_runtime_environment_controls(190);CHECK(gui::labels[1]=="Planet Water: Off");
 CHECK(r.m_runtimeViewStatus=="Water view updated. Terrain settings are unchanged.");
 CHECK(r.m_meshPlanetarySettings->ocean_surface&&!r.m_meshPlanetarySettings->stream_lod);
 // Rapid off/on leaves the active job owned, but only the last setting may publish.
 Runtime rapid;rapid.set_runtime_water_visible(false);rapid.m_planetaryRequestSettings=rapid.m_planetarySettings;
 rapid.set_runtime_water_visible(true);CHECK(rapid.m_planetaryPending&&rapid.m_planetaryWaterPending&&!rapid.runtime_water_visibility_pending());
 CHECK(!rapid.accept(Result{}));CHECK(rapid.m_planetaryPublishedOcean&&rapid.m_planetaryScheduler->cancels.size()==4);
 rapid.m_planetaryRequestSettings=rapid.m_planetarySettings;CHECK(rapid.accept(Result{}));
 // Generic projects do not need a planetary flight controller for these controls.
 Runtime plain;plain.m_planetarySettings.reset();plain.m_meshPlanetarySettings.reset();plain.m_hasPlanetRecipe=false;
 plain.m_entities={{"Water",true}};gui::click=1;plain.draw_runtime_environment_controls(160);
 CHECK(!plain.m_runtimeWaterVisible&&plain.m_waterCaches.empty()&&!gui::enabled[0]&&gui::enabled[1]);
 CHECK(plain.m_planetaryScheduler->cancels.empty());
 Runtime clouds;gui::click=0;clouds.draw_runtime_environment_controls(160);CHECK(!clouds.m_runtimeCloudsVisible);
 gui::click=0;clouds.draw_runtime_environment_controls(160);CHECK(clouds.m_runtimeCloudsVisible);
 Runtime invalid;project_planetary::admitted=false;invalid.set_runtime_water_visible(false);project_planetary::admitted=true;
 CHECK(invalid.m_runtimeWaterVisible&&invalid.m_planetarySettings->ocean_surface&&invalid.m_planetaryScheduler->cancels.empty());
 // A failed publication keeps the requested/actual distinction visible.
 Runtime rejected;rejected.set_runtime_water_visible(false);rejected.m_planetaryFailed=true;
 rejected.draw_runtime_environment_controls(160);CHECK(gui::labels[1]=="Planet Water: update failed"&&rejected.m_planetaryPublishedOcean);
}
'''.replace("ACTUAL_METHODS", methods).replace("ACTUAL_OUTER", outer).replace("ACTUAL_COMMIT", commit)
        report = ROOT / "Artifacts/Reports/runtime-environment-controls-nov07" / str(time.time_ns())
        report.mkdir(parents=True)
        cpp = report / "contract.cpp"
        cpp.write_text(code, encoding="utf-8")
        records = compile_and_run(self, cpp, report)
        (report / "result.json").write_text(json.dumps({"passed": True,
            "scope": "Actual view controls, cancellation requests, completion binding and accepted publication block; recorded source/provider/GUI/scheduler boundaries, no native rendering or provider run",
            "owner_sha256": hashlib.sha256(OWNER.read_bytes()).hexdigest(), "records": records}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
