"""Actual runtime ocean worker and clock admission; CPU ports, no graphics/models."""
from pathlib import Path
import hashlib
import json
import unittest

from Tests.platform.test_child_close_router import definition
from Tests.packages.test_planetary_stream_completion import condition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / 'Engine/src/epoch/implementation/epoch.project_play_scene.inl'

CODE = r'''
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <thread>
#include <unordered_map>
#include <vector>
namespace timing {using Clock=std::chrono::steady_clock;}
namespace epochengine::core::time {CLOCK_STATS;}
TIMELINE
namespace platform::child_process {PRESENTATION;VALID_PRESENTATION}
namespace core {namespace time=epochengine::core::time;struct Context {double seconds{};std::optional<double>simulation_sample_time()const{return std::isfinite(seconds)?std::optional<double>{seconds}:std::nullopt;}};}
namespace asset::mesh {
struct Vertex {};
struct Artifact {int value{71};std::vector<int> vertices()const{return {1,2,3};}std::vector<int> indices()const{return {0,1,2};}};
}
namespace project_planetary_snapshot {
struct Snapshot {struct {double water_radius_meters{6371000};std::vector<int> water_vertices{1};struct {unsigned request_id{17};}identity;}metadata;std::shared_ptr<const asset::mesh::Artifact> artifact;};
struct Limits {std::uint64_t maximum_transient_bytes{70ull*1024*1024},maximum_retained_bytes{180ull*1024*1024};};
}
namespace water::wave {
struct Tile {double value{};double seconds()const{return value;}};
struct Evaluation {std::shared_ptr<Tile> tile;explicit operator bool()const{return bool(tile);}};
struct Provider {virtual ~Provider()=default;virtual Evaluation evaluate(double,std::uint64_t,std::stop_token)=0;};
}
unsigned source_checks{},water_checks{},resamples{};
bool planet_current=true,water_current=true,cancel_in_resample{};
std::stop_source* cancellation_port{};
namespace project_planetary {
struct SourceGeneration {};
struct Settings {std::uint32_t body_index{2};bool ocean_surface{true};};
bool source_generation_is_current(const std::filesystem::path&,const SourceGeneration&,std::stop_token) {++source_checks;return planet_current;}
auto snapshot_transition_limits(const Settings&) {return project_planetary_snapshot::Limits{};}
}
namespace package_source {
struct Attachment {std::string content_hash;bool source_attached()const{return !content_hash.empty();}};
Attachment load_project_attachment(const std::filesystem::path&,std::string_view,std::stop_token) {
 ++water_checks;return {water_current?"water-approved":"changed"};
}
}
namespace project_planetary_snapshot {
std::shared_ptr<const asset::mesh::Artifact> resample_water(const Snapshot&,const water::wave::Tile&,Limits,std::stop_token) {
 ++resamples;if(cancel_in_resample)cancellation_port->request_stop();return std::make_shared<asset::mesh::Artifact>();
}
}
struct FakeProvider:water::wave::Provider {
 std::atomic<unsigned> calls{};bool fail{},wrong_time{},replace_planet{},replace_water{};std::uint64_t budget{};
 std::atomic<unsigned> active{},maximum_active{};std::atomic<bool> block{};
 water::wave::Evaluation evaluate(double seconds,std::uint64_t maximum,std::stop_token stop)override {
  const auto running=++active;maximum_active.store((std::max)(maximum_active.load(),running));
  struct Retire {std::atomic<unsigned>& value;~Retire(){--value;}}retire{active};
  while(block.load()&&!stop.stop_requested())std::this_thread::sleep_for(std::chrono::milliseconds{1});
  ++calls;budget=maximum;if(replace_planet)planet_current=false;if(replace_water)water_current=false;
  return {fail?nullptr:std::make_shared<water::wave::Tile>(seconds+(wrong_time?1.0:0.0))};
 }
};
struct SchedulerPort {
 unsigned cancels{};std::stop_source cancellation;
 int cancel(int){++cancels;cancellation.request_stop();return 0;}
 void Shutdown(){cancellation.request_stop();}SchedulerPort& graph(){return *this;}
 template<class T>struct Submission {std::future<T> completion;int cancellation{};};
 template<class F>auto submit_cancellable(std::string_view,F function) {
  cancellation=std::stop_source{};const auto stop=cancellation.get_token();
  return Submission<decltype(function(stop))>{std::async(std::launch::async,[function,stop]{return function(stop);}),1};
 }
};
namespace project_planetary_runtime {struct Binding {std::string water_source_hash{"water-approved"};};auto admitted_project_binding(){return std::optional<Binding>{Binding{}};}}
struct ControllerPort {std::shared_ptr<FakeProvider> provider=std::make_shared<FakeProvider>();auto water_provider(){return provider;}};
namespace logger {enum class LogLevel{INFO};struct Sink {template<class...T>void log(T&&...){}};Sink& get(std::string_view){static Sink sink;return sink;}}
struct Runtime {
 RESULT_TYPE
 WORKER
 std::shared_ptr<SchedulerPort> m_planetaryScheduler=std::make_shared<SchedulerPort>();
 std::optional<std::future<PlanetaryWaterResult>> m_planetaryWaterPending;int m_planetaryWaterCancellation{};
 std::optional<std::future<int>> m_planetaryPending;int m_planetaryCancellation{};
 struct FramePublicationPort {std::optional<int> pending;int cancellation{};};
 std::unordered_map<const void*,FramePublicationPort> m_planetaryFramePublications;
 std::shared_ptr<ControllerPort> m_explorationController=std::make_shared<ControllerPort>();
 struct {double seconds{},time_scale{1};std::uint32_t anchor{2};}m_explorationFrame;
 std::optional<epochengine::timeline::TimelineState> m_runtimePresentation;
 std::optional<double> m_planetaryWaterObservedSeconds,m_planetaryWaterObservedScale;
 timing::Clock::time_point m_planetaryWaterObservedTick{},m_planetaryWaterNextTick{};
 timing::Clock::time_point m_planetaryWaterEvidenceTick{};
 int m_planetaryWaterClockDirection{};std::uint64_t m_planetaryWaterClockGeneration{};
 std::shared_ptr<const project_planetary_snapshot::Snapshot> m_planetaryWaterSource,m_planetaryWaterPublishedSource;
 std::optional<project_planetary::Settings> m_planetarySettings{project_planetary::Settings{}};
 std::optional<project_planetary::SourceGeneration> m_meshPlanetarySource{project_planetary::SourceGeneration{}};
 std::optional<double> m_planetaryWaterPublishedSeconds;
 std::unordered_map<unsigned,std::shared_ptr<const asset::mesh::Artifact>> m_meshes;
 unsigned m_planetaryEntity{7};std::string m_projectRoot{"owned"},m_planetaryWaterStatus;bool m_planetaryWaterSourceRefused{};
 void ensure_planetary_scheduler(){}bool planetary_mesh_frame_fits(const std::shared_ptr<const asset::mesh::Artifact>&){return true;}
 CLOCK
 APPLY_TIME
 ADVANCE_TIME
 void observe_planetary_water_clock(timing::Clock::time_point now){observe_planetary_water_clock(now,m_explorationFrame.seconds,m_explorationFrame.time_scale);}
 RESET
 PUMP
 void retire(){RETIRE}
 bool accepts(const PlanetaryWaterResult& result)const {return COMPLETION;}
};
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main() {
 auto base=std::make_shared<project_planetary_snapshot::Snapshot>();
 auto provider=std::make_shared<FakeProvider>();const project_planetary::Settings settings;
 auto run=[&](std::stop_token stop={}) {return Runtime::sample_planetary_water("owned",{},base,provider,settings,3.25,"water-approved",7,1024,stop);};
 auto result=run();CHECK(result.mesh&&result.source_current&&!result.cancelled&&result.seconds==3.25&&result.clock_generation==7);
 CHECK(result.source==base&&result.body==2&&provider->budget==64ull*1024*1024&&source_checks==2&&water_checks==2&&resamples==1);
 CHECK(std::isfinite(result.worker_seconds)&&result.worker_seconds>=0.0);
 source_checks=water_checks=resamples=0;planet_current=false;result=run();CHECK(!result.mesh&&!result.source_current&&provider->calls==1&&source_checks==1&&water_checks==0);
 planet_current=true;provider->replace_water=true;result=run();CHECK(!result.mesh&&!result.source_current&&water_checks==2);
 water_current=true;provider->replace_water=false;provider->fail=true;result=run();CHECK(!result.mesh&&result.source_current&&!result.cancelled);
 provider->fail=false;provider->wrong_time=true;const auto before=resamples;result=run();CHECK(!result.mesh&&result.source_current&&resamples==before);
 provider->wrong_time=false;std::stop_source stop;stop.request_stop();const auto calls=provider->calls.load();result=run(stop.get_token());CHECK(result.cancelled&&!result.mesh&&provider->calls==calls);
 std::stop_source during;cancellation_port=&during;cancel_in_resample=true;result=run(during.get_token());CHECK(result.cancelled&&!result.mesh&&!result.source_current);cancel_in_resample=false;
 const auto count=provider->calls.load();result=Runtime::sample_planetary_water("owned",{},base,provider,settings,1,"water-approved",7,180ull*1024*1024,{});
 CHECK(!result.mesh&&result.source_current&&provider->calls==count);
 Runtime runtime;auto now=timing::Clock::time_point{};runtime.observe_planetary_water_clock(now);
 for(unsigned i=0;i<100;++i){now+=std::chrono::milliseconds{16};runtime.m_explorationFrame.seconds+=0.016;runtime.observe_planetary_water_clock(now);}
 CHECK(runtime.m_planetaryWaterClockGeneration==0&&runtime.m_planetaryScheduler->cancels==0);
 runtime.m_explorationFrame.time_scale=0;runtime.observe_planetary_water_clock(now);CHECK(runtime.m_planetaryWaterClockGeneration==1);
 for(unsigned i=0;i<10;++i){now+=std::chrono::milliseconds{16};runtime.observe_planetary_water_clock(now);}CHECK(runtime.m_planetaryWaterClockGeneration==1);
 runtime.m_explorationFrame.time_scale=1;runtime.observe_planetary_water_clock(now);CHECK(runtime.m_planetaryWaterClockGeneration==2);
 for(unsigned i=0;i<30;++i){now+=std::chrono::milliseconds{16};runtime.m_explorationFrame.seconds-=0.016;runtime.observe_planetary_water_clock(now);}
 CHECK(runtime.m_planetaryWaterClockGeneration==3);
 runtime.m_explorationFrame.seconds+=1000;runtime.observe_planetary_water_clock(now);const auto epoch=runtime.m_planetaryWaterClockGeneration;CHECK(epoch==4);
 result=run();runtime.m_planetaryWaterSource=base;result.clock_generation=epoch;CHECK(runtime.accepts(result));
 result.clock_generation=epoch-1;CHECK(!runtime.accepts(result));result.clock_generation=epoch;
 result.body=1;CHECK(!runtime.accepts(result));result.body=2;
 result.source=std::make_shared<project_planetary_snapshot::Snapshot>();CHECK(!runtime.accepts(result));result.source=base;
 runtime.m_explorationFrame.anchor=1;CHECK(!runtime.accepts(result));
 // An actual pending runtime job prevents another evaluation, including after
 // source replacement/cancellation. The provider permits exactly one caller.
 Runtime queued;queued.m_planetaryWaterSource=base;
 auto live=queued.m_explorationController->provider;live->block=true;
 queued.pump_planetary_water();for(unsigned i=0;i<1000&&!live->active;++i)std::this_thread::sleep_for(std::chrono::milliseconds{1});
 CHECK(live->active==1);queued.pump_planetary_water();CHECK(live->maximum_active==1);
 queued.reset_planetary_water();queued.m_planetaryWaterSource=std::make_shared<project_planetary_snapshot::Snapshot>();
 for(unsigned i=0;i<1000&&queued.m_planetaryWaterPending;++i){queued.pump_planetary_water();std::this_thread::sleep_for(std::chrono::milliseconds{1});if(live->active&&live->calls>0)break;}
 CHECK(live->maximum_active==1);queued.retire();CHECK(live->active==0&&!queued.m_planetaryWaterPending&&!queued.m_planetaryScheduler);
 Runtime publishing;int context_id{};
 auto scheduler=publishing.m_planetaryScheduler;
 publishing.m_planetaryFramePublications.emplace(&context_id,Runtime::FramePublicationPort{1,7});
 publishing.retire();
 CHECK(scheduler->cancels==1&&publishing.m_planetaryFramePublications.empty()&&!publishing.m_planetaryScheduler);
 // Paused simulation requests its exact quantized sample once, then stays idle.
 Runtime paused;paused.m_planetaryWaterSource=base;paused.m_explorationFrame.seconds=3.25;paused.m_explorationFrame.time_scale=0;
 auto idle=paused.m_explorationController->provider;paused.pump_planetary_water();
 for(unsigned i=0;i<1000&&paused.m_planetaryWaterPending;++i){std::this_thread::sleep_for(std::chrono::milliseconds{1});paused.pump_planetary_water();}
 CHECK(paused.m_planetaryWaterPublishedSeconds==3.25&&!paused.m_planetaryWaterPending&&idle->calls==1);
 for(unsigned i=0;i<100;++i)paused.pump_planetary_water();CHECK(idle->calls==1&&paused.m_planetaryWaterStatus=="Ocean: paused.");
 auto far=std::make_shared<project_planetary_snapshot::Snapshot>();far->metadata.water_vertices.clear();paused.reset_planetary_water();paused.m_planetaryWaterSource=far;
 paused.pump_planetary_water();CHECK(!paused.m_planetaryWaterPending&&idle->calls==1&&paused.m_planetaryWaterStatus.find("nearby")!=std::string::npos);
 auto old=std::make_shared<project_planetary_snapshot::Snapshot>();old->metadata.water_radius_meters=0;paused.m_planetaryWaterSource=old;
 paused.pump_planetary_water();CHECK(!paused.m_planetaryWaterPending&&idle->calls==1&&paused.m_planetaryWaterStatus.find("source update")!=std::string::npos);
 // Actual TimelineState transport drives FFT samples from the existing context
 // clock. Reverse returns to the same sample without negative physics steps.
 Runtime visual;core::Context context;context.seconds=10;visual.m_planetaryWaterSource=base;
 CHECK(visual.apply_runtime_presentation({4,120,1,false,true},&context));
 context.seconds=11;visual.advance_runtime_presentation(&context);CHECK(visual.m_runtimePresentation->playhead_seconds==5);
 CHECK(visual.apply_runtime_presentation({5,120,1,true,true},&context));
 context.seconds=12;visual.advance_runtime_presentation(&context);CHECK(visual.m_runtimePresentation->playhead_seconds==4);
 CHECK(visual.m_explorationFrame.seconds==0&&context.seconds==12); // Visual time does not rewind physics.
 CHECK(visual.apply_runtime_presentation({3.25,120,1,true,false},&context));
 context.seconds=90;visual.advance_runtime_presentation(&context);CHECK(visual.m_runtimePresentation->playhead_seconds==3.25);
 visual.pump_planetary_water();
 for(unsigned i=0;i<1000&&visual.m_planetaryWaterPending;++i){std::this_thread::sleep_for(std::chrono::milliseconds{1});visual.pump_planetary_water();}
 CHECK(visual.m_planetaryWaterPublishedSeconds==3.25&&visual.m_planetaryWaterStatus.find("Editor timeline paused")!=std::string::npos);
 const auto idle_calls=visual.m_explorationController->provider->calls.load();
 for(unsigned i=0;i<100;++i)visual.pump_planetary_water();CHECK(visual.m_explorationController->provider->calls==idle_calls);
 const auto time_epoch=visual.m_planetaryWaterClockGeneration;
 CHECK(visual.apply_runtime_presentation({8,120,2,true,true},&context));CHECK(visual.m_planetaryWaterClockGeneration>time_epoch);
 context.seconds=90.5;visual.advance_runtime_presentation(&context);CHECK(visual.m_runtimePresentation->playhead_seconds==7);
 CHECK(!visual.apply_runtime_presentation({-1,120,1,false,false},&context));
 context.seconds=std::numeric_limits<double>::quiet_NaN();CHECK(!visual.apply_runtime_presentation({1,120,1,false,false},&context));
 // A real command cancels the old branch while retaining the one-provider-call
 // invariant. Only the new paused cursor may replace the previous surface.
 Runtime seeking;core::Context seekContext;seeking.m_planetaryWaterSource=base;
 auto seekProvider=seeking.m_explorationController->provider;seekProvider->block=true;
 seeking.pump_planetary_water();for(unsigned i=0;i<1000&&!seekProvider->active;++i)std::this_thread::sleep_for(std::chrono::milliseconds{1});
 CHECK(seekProvider->active==1);CHECK(seeking.apply_runtime_presentation({9.5,120,1,true,false},&seekContext));
 seekProvider->block=false;
 for(unsigned i=0;i<1000&&seeking.m_planetaryWaterPublishedSeconds!=9.5;++i){seeking.pump_planetary_water();std::this_thread::sleep_for(std::chrono::milliseconds{1});}
 CHECK(seeking.m_planetaryWaterPublishedSeconds==9.5&&seekProvider->maximum_active==1);seeking.retire();
}
'''


class PlanetaryWaterClock(unittest.TestCase):
    def test_actual_runtime_worker_clock_and_completion(self):
        source = OWNER.read_text(encoding='utf-8')
        result = definition(source, '            struct PlanetaryWaterResult final') + ';'
        worker = definition(source, '            [[nodiscard]] static PlanetaryWaterResult sample_planetary_water(')
        clock = definition(source, '            void observe_planetary_water_clock(')
        apply_time = definition(source, '            bool apply_runtime_presentation(')
        advance_time = definition(source, '            void advance_runtime_presentation(')
        reset = definition(source, '            void reset_planetary_water(')
        pump = definition(source, '            void pump_planetary_water(')
        shutdown = definition(source, '            void shutdown_planetary_runtime(')
        retire = definition(shutdown, '                try')[len('                try'):]
        completion = 'result.source==m_planetaryWaterSource&&m_planetarySettings' + condition(
            source, 'if(result.source==m_planetaryWaterSource&&m_planetarySettings')
        code = CODE.replace('RESULT_TYPE', result).replace('WORKER', worker).replace(' CLOCK\n', clock+'\n').replace('COMPLETION', completion)
        timeline=(ROOT/'Engine/modules/timeline.system.ixx').read_text(encoding='utf-8')
        timeline=timeline[timeline.index('export namespace epochengine::timeline'):timeline.index('    inline void clamp_view_config')].replace('export namespace','namespace',1)+'}\n'
        stats=(ROOT/'Engine/dep/EpochPlatformEngine/modules/core.time.ixx').read_text(encoding='utf-8')
        protocol=(ROOT/'Engine/modules/platform.child_process.ixx').read_text(encoding='utf-8')
        code=code.replace('CLOCK_STATS',definition(stats,'    struct simulation_stats')).replace('TIMELINE',timeline)
        code=code.replace('VALID_PRESENTATION',definition(protocol,'    [[nodiscard]] inline bool valid_runtime_presentation(')).replace('PRESENTATION',definition(protocol,'    struct RuntimePresentation final'))
        code=code.replace('APPLY_TIME',apply_time).replace('ADVANCE_TIME',advance_time)
        code = code.replace('RESET', reset).replace('PUMP', pump).replace('RETIRE', retire)
        report = ROOT / 'Artifacts/Reports/planetary-water-clock-nov01' / hashlib.sha256(code.encode()).hexdigest()[:12]
        report.mkdir(parents=True, exist_ok=True)
        fixture = report / 'clock.cpp'
        fixture.write_text(code, encoding='utf-8')
        proof = compile_and_run(self, fixture, report)
        (report / 'result.json').write_text(json.dumps(proof, indent=2) + '\n')

    def test_actual_editor_sender_is_explicit_and_identity_bound(self):
        editor=(ROOT/'Engine/src/editor/application/editor.application_project_runtime.inl').read_text(encoding='utf-8')
        inspector=(ROOT/'Engine/src/editor/application/editor.frame_inspector.inl').read_text(encoding='utf-8')
        self.assertEqual(inspector.count('(void)request_project_runtime_presentation(editor);'),2)
        for path in ('editor.frame_bootstrap.inl','editor.application_runtime_diagnostics.inl'):
            self.assertNotIn('request_project_runtime_presentation(', (ROOT/'Engine/src/editor/application'/path).read_text(encoding='utf-8'))
        actual=definition(editor,'        [[nodiscard]] bool request_project_runtime_presentation(')
        code=r'''
#include <optional>
#include <string>
#include <string_view>
namespace epochengine::timeline {enum class PlaybackDirection{reverse,forward};}
namespace platform::child_process {
struct RuntimePresentation{double seconds,duration_seconds,rate;bool reverse,playing;};
RuntimePresentation captured{};unsigned calls{};bool sent=true;
bool request_runtime_presentation(int process,int lease,int image,std::string_view correlation,RuntimePresentation sample){
 ++calls;captured=sample;return sent&&process==4&&lease==8&&image==12&&correlation=="owned";}}
struct EditorState{std::optional<int>projectRuntimeSnapshot{1};bool projectRuntimeInEditor{true};int projectRuntimeWindow{1},projectRuntimeProcess{4};
 struct {int executable{12};std::string correlation{"owned"};}projectRuntimeLaunch;
 struct {double playhead_seconds{5},duration_seconds{120},playback_rate{2};epochengine::timeline::PlaybackDirection direction{epochengine::timeline::PlaybackDirection::reverse};bool playing{true};}timelineState;};
bool current=true;bool project_runtime_identity_current(const EditorState&,int){return current;}int project_runtime_control_lease(const EditorState&){return 8;}
ACTUAL
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){EditorState editor;namespace p=platform::child_process;
 CHECK(request_project_runtime_presentation(editor));CHECK(p::calls==1&&p::captured.seconds==5&&p::captured.duration_seconds==120&&p::captured.rate==2&&p::captured.reverse&&p::captured.playing);
 current=false;CHECK(!request_project_runtime_presentation(editor)&&p::calls==1);current=true;
 editor.projectRuntimeInEditor=false;CHECK(!request_project_runtime_presentation(editor)&&p::calls==1);editor.projectRuntimeInEditor=true;
 editor.projectRuntimeWindow=0;CHECK(!request_project_runtime_presentation(editor)&&p::calls==1);editor.projectRuntimeWindow=1;
 editor.projectRuntimeSnapshot.reset();CHECK(!request_project_runtime_presentation(editor)&&p::calls==1);editor.projectRuntimeSnapshot=1;
 p::sent=false;CHECK(!request_project_runtime_presentation(editor)&&p::calls==2);
}
'''.replace('ACTUAL',actual)
        report=ROOT/'Artifacts/Reports/planetary-water-clock-nov01'/hashlib.sha256(code.encode()).hexdigest()[:12]
        report.mkdir(parents=True,exist_ok=True);fixture=report/'sender.cpp';fixture.write_text(code,encoding='utf-8')
        proof=compile_and_run(self,fixture,report);(report/'result.json').write_text(json.dumps(proof,indent=2)+'\n')


if __name__ == '__main__':
    unittest.main()
