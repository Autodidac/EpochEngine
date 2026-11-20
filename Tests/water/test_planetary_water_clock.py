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
#include <array>
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>
namespace timing {using Clock=std::chrono::steady_clock;}
namespace epochengine::core::time {CLOCK_STATS;}
TIMELINE
namespace platform::child_process {PRESENTATION;VALID_PRESENTATION}
namespace core {namespace time=epochengine::core::time;struct Context {double seconds{};std::optional<double>simulation_sample_time()const{return std::isfinite(seconds)?std::optional<double>{seconds}:std::nullopt;}};}
namespace asset::mesh {
struct Vertex {};
struct SphericalWaterRange {float specular_scale{};};
struct SphericalWaterAppearance {std::vector<SphericalWaterRange> ranges;int artifact_key{71};};
struct Artifact {int key()const{return 71;}std::vector<int> vertices()const{return {1,2,3};}std::vector<int> indices()const{return {0,1,2};}};
}
namespace previewgrid {
WATER_VIEW;
struct Frame {struct Marker {std::shared_ptr<const asset::mesh::Artifact> mesh;};std::vector<Marker> markers;};
std::shared_ptr<Frame> published;
std::shared_ptr<const Frame> scene_frame_for(const void*){return published;}
}
namespace water::wave {
struct Tile {double value{};double seconds()const{return value;}};
struct Evaluation {std::shared_ptr<const Tile> tile;explicit operator bool()const{return bool(tile);}};
struct Provider {virtual ~Provider()=default;virtual Evaluation evaluate(double,std::uint64_t,std::stop_token)=0;};
}
namespace project_planetary_snapshot {
struct Snapshot {struct {double water_radius_meters{6371000};std::vector<int> water_vertices{1};struct {unsigned request_id{17};}identity;
 std::shared_ptr<const asset::mesh::SphericalWaterAppearance> water_appearance;}metadata;std::shared_ptr<const asset::mesh::Artifact> artifact;
 std::uint64_t bytes{4096u};std::optional<double> water_sample_seconds;};
struct SelectedCut {std::shared_ptr<const Snapshot> source;};
struct Limits {std::uint64_t maximum_transient_bytes{70ull*1024*1024},maximum_retained_bytes{180ull*1024*1024};};
std::uint64_t retained_snapshot_bytes(const Snapshot& snapshot){return snapshot.bytes;}
struct TileKey {unsigned face{},level{},x{},y{};};
struct WaterLatticeProvider {};
struct WetTransitionFailure {std::string_view reason;std::optional<TileKey> patch;};
enum class InitialCut {coarse_roots,selected_patches};
struct RetainedSampleProof {
 unsigned calls{};
 std::uint64_t external_bytes{};
 std::shared_ptr<const water::wave::Tile> tile;
 std::shared_ptr<const WaterLatticeProvider> basis;
};
RetainedSampleProof retained_sample;
bool refuse_basis{};
std::optional<std::uint64_t> wet_output_budget;
unsigned wet_advances{};
std::vector<unsigned> wet_materializations;
struct Transition {
 explicit Transition(Limits){}
 struct Draw {std::shared_ptr<const Snapshot> snapshot;};
 std::vector<Draw> draws;
 std::shared_ptr<const Snapshot> previous,target;
 unsigned steps{};
 bool admit(const std::shared_ptr<const Snapshot>& snapshot,std::stop_token={},InitialCut=InitialCut::coarse_roots) {
  if(!previous){previous=snapshot;draws={{snapshot}};}else target=snapshot;
  return bool(snapshot);
 }
 bool settled()const{return steps==7;}
 bool needs_retained_water_sample()const {
  return previous&&target&&previous->water_sample_seconds&&target->water_sample_seconds
   &&previous->water_sample_seconds!=target->water_sample_seconds;
 }
 bool prepare_retained_water(unsigned,std::shared_ptr<const water::wave::Tile> tile,
  std::shared_ptr<const WaterLatticeProvider> basis,std::uint64_t external,std::stop_token stop) {
  retained_sample={retained_sample.calls+1,external,std::move(tile),std::move(basis)};
  return !stop.stop_requested()&&!refuse_basis&&retained_sample.tile
   &&retained_sample.tile->seconds()==target->water_sample_seconds;
 }
 bool advance_wet(double,bool,std::stop_token stop,WetTransitionFailure*) {
  if(stop.stop_requested())return false;
  if(wet_output_budget)++wet_advances;
  ++steps;return true;
 }
 const auto& draw()const{return draws;}
 std::shared_ptr<const asset::mesh::Artifact> materialize_wet(unsigned,double,std::stop_token,
  std::shared_ptr<const asset::mesh::SphericalWaterAppearance>* appearance,std::uint64_t external,WetTransitionFailure* failure)const {
  *failure={};
  if(wet_output_budget){
   if(external>*wet_output_budget){*failure={"output_retained_budget",{}};return {};}
   wet_materializations.push_back(steps);
  }
  *appearance=std::make_shared<asset::mesh::SphericalWaterAppearance>();
  return std::make_shared<asset::mesh::Artifact>();
 }
};
}
unsigned source_checks{},water_checks{},resamples{};
bool planet_current=true,water_current=true,cancel_in_resample{},cancel_in_source_check{};
bool recipe_current=true;
std::stop_source* cancellation_port{};
namespace project_planetary {
struct SourceGeneration {bool operator==(const SourceGeneration&)const=default;};
struct ShorelineSourceReview {};
unsigned review_count{},recheck_count{},reject_recheck{},stop_recheck{};
struct Settings {std::uint32_t body_index{2},cells_per_tile{16};bool ocean_surface{true},stream_lod{true};bool operator==(const Settings&)const=default;};
PACKAGED_SHORELINE;
using GeneratedMeshFrame=int;
struct StreamView {bool reusable{true};};
bool stream_view_reusable(const StreamView& view,const StreamView&){return view.reusable;}
struct StreamCutFrame {
 std::shared_ptr<const asset::mesh::Artifact> mesh;
 std::shared_ptr<const asset::mesh::SphericalWaterAppearance> appearance;
};
WET_CURSOR;
struct StreamResult {
 struct {bool ok{true},cancelled{};std::string message;std::optional<GeneratedMeshFrame> mesh_frame;}operation;
 bool snapshot_ready{true};
 unsigned pending{};
 std::uint64_t request_id{};
 std::shared_ptr<const asset::mesh::Artifact> snapshot;
 std::shared_ptr<const asset::mesh::SphericalWaterAppearance> water_appearance;
 std::shared_ptr<const project_planetary_snapshot::Snapshot> patch_snapshot;
 std::optional<SourceGeneration> verified_source{SourceGeneration{}};
 std::string verified_recipe{"recipe-one"};
 std::optional<double> water_sample_seconds,water_edge_sample_seconds;
 std::vector<StreamCutFrame> wet_transition;
 std::optional<StreamWetCutCursor> wet_cursor;
};
bool source_generation_is_current(const std::filesystem::path&,const SourceGeneration&,std::stop_token stop) {
 ++source_checks;
 if(cancel_in_source_check)cancellation_port->request_stop();
 return planet_current&&!stop.stop_requested();
}
bool stream_recipe_is_current(const std::filesystem::path&,std::string_view stamp) {return recipe_current&&stamp=="recipe-one";}
auto snapshot_transition_limits(const Settings& settings) {
 return settings.stream_lod?project_planetary_snapshot::Limits{}:project_planetary_snapshot::Limits{0u,0u};
}
}
namespace package_source {
struct Attachment {std::string content_hash;bool source_attached()const{return !content_hash.empty();}};
Attachment load_project_attachment(const std::filesystem::path&,std::string_view,std::stop_token) {
 ++water_checks;return {water_current?"water-approved":"changed"};
}
}
namespace project_planetary {
std::shared_ptr<const ShorelineSourceReview> review_shoreline_sources(const std::filesystem::path& root,
 const SourceGeneration& generation,std::string_view hash,std::stop_token stop) {
 ++review_count;
 if(!source_generation_is_current(root,generation,stop))return {};
 const auto water=package_source::load_project_attachment(root,"fft_ocean",stop);
 return water.source_attached()&&water.content_hash==hash&&!stop.stop_requested()
  ?std::make_shared<const ShorelineSourceReview>():nullptr;
}
bool shoreline_sources_are_current(const std::filesystem::path&,const SourceGeneration&,
 const ShorelineSourceReview&,std::stop_token stop) {
 ++recheck_count;
 if(recheck_count==stop_recheck)cancellation_port->request_stop();
 return recheck_count!=reject_recheck&&planet_current&&water_current&&!stop.stop_requested();
}
}
namespace project_planetary_snapshot {
const water::wave::Tile* resample_tile{};
std::shared_ptr<const asset::mesh::Artifact> resample_water(const Snapshot&,const water::wave::Tile& tile,Limits,std::stop_token,
 std::shared_ptr<const asset::mesh::SphericalWaterAppearance>* appearance) {
 ++resamples;resample_tile=&tile;
 if(cancel_in_resample)cancellation_port->request_stop();
 *appearance=std::make_shared<const asset::mesh::SphericalWaterAppearance>();
 return std::make_shared<asset::mesh::Artifact>();
}
}
struct FakeProvider:water::wave::Provider {
 std::atomic<unsigned> calls{};bool fail{},wrong_time{},replace_planet{},replace_water{},cancel_in_evaluate{};std::uint64_t budget{};
 std::shared_ptr<const water::wave::Tile> last_tile;
 std::atomic<unsigned> active{},maximum_active{};std::atomic<bool> block{};
 water::wave::Evaluation evaluate(double seconds,std::uint64_t maximum,std::stop_token stop)override {
  const auto running=++active;maximum_active.store((std::max)(maximum_active.load(),running));
  struct Retire {std::atomic<unsigned>& value;~Retire(){--value;}}retire{active};
  while(block.load()&&!stop.stop_requested())std::this_thread::sleep_for(std::chrono::milliseconds{1});
  ++calls;budget=maximum;if(replace_planet)planet_current=false;if(replace_water)water_current=false;
  if(cancel_in_evaluate)cancellation_port->request_stop();
  last_tile=fail?nullptr:std::make_shared<water::wave::Tile>(seconds+(wrong_time?1.0:0.0));
  return {last_tile};
 }
};
struct SchedulerPort {
 unsigned cancels{};std::stop_source cancellation;
 int cancel(int){++cancels;cancellation.request_stop();return 0;}
 void Shutdown(){cancellation.request_stop();}SchedulerPort& graph(){return *this;}
 template<class T>struct Submission {std::future<T> completion;int cancellation{};};
 template<class F>auto submit_cancellable(std::string_view,F function) {
  cancellation=std::stop_source{};const auto stop=cancellation.get_token();
  return Submission<decltype(function(stop))>{std::async(std::launch::async,[function=std::move(function),stop]()mutable{return function(stop);}),1};
 }
};
namespace project_planetary_runtime {struct Binding {std::string water_source_hash{"water-approved"};};auto admitted_project_binding(){return std::optional<Binding>{Binding{}};}}
struct ControllerPort {
 std::shared_ptr<FakeProvider> provider=std::make_shared<FakeProvider>();
 std::shared_ptr<const project_planetary_snapshot::WaterLatticeProvider> lattice=
  std::make_shared<const project_planetary_snapshot::WaterLatticeProvider>();
 auto water_provider(){return provider;}
 auto water_lattice_provider(){return lattice;}
};
namespace render_water_evidence {bool enabled(){return false;}}
namespace editor_tasks {struct TaskCancelled {};}
namespace logger {enum class LogLevel{INFO,WARN};struct Sink {template<class...T>void log(T&&...){} template<class...T>void logf(T&&...){}};Sink& get(std::string_view){static Sink sink;return sink;}
 struct WetBatchRecord {unsigned count{},first_step{};std::size_t frames{};bool source_current{};double queue_ms{};};
 WetBatchRecord wet_batch_record;
 template<class...T>void infof_loc(std::string_view,std::source_location,std::string_view format,T&&... args){
  if constexpr(sizeof...(T)==13){
   if(format.starts_with("Shoreline batch timing:")){
    const auto values=std::tuple{args...};
    wet_batch_record={wet_batch_record.count+1,std::get<1>(values),std::get<2>(values),std::get<3>(values),std::get<5>(values)};
   }
  }
 }}
struct Runtime {
 RESULT_TYPE
 CUT_TYPE
 PLAYBACK_TYPE
 WORKER
 PREPARE_CUT
 DISPLAY_BYTES
 CONTINUATION_FUNCTIONS
 PLAYBACK_FUNCTIONS
 std::shared_ptr<SchedulerPort> m_planetaryScheduler=std::make_shared<SchedulerPort>();
 std::optional<std::future<PlanetaryWaterResult>> m_planetaryWaterPending;int m_planetaryWaterCancellation{};
 std::optional<std::future<project_planetary::StreamResult>> m_planetaryPending;int m_planetaryCancellation{};
 std::optional<InitialPlanetaryWaterCut> m_planetaryInitialWaterCut;
 std::optional<PlanetaryCutPlayback> m_planetaryCutPlayback;
 bool m_planetaryPreparingInitialWater{},m_planetaryPreparingWetBatch{},m_planetaryPaused{},m_planetaryContinue{},m_planetaryFailed{};
 bool m_planetaryPublishedOcean{};
 std::optional<std::uint32_t> m_planetaryLodBody;
 std::uint64_t m_planetaryRequestWaterClock{};
 std::uint32_t m_planetaryRequestBody{2};
 project_planetary::StreamView m_planetaryView{};
 std::string m_planetaryStatus,m_runtimeViewStatus;
 timing::Clock::time_point m_planetaryNextTick{};
 bool runtime_water_visibility_pending()const{return false;}
 static bool planetary_view_changed(const project_planetary::StreamView&,const project_planetary::StreamView&){return false;}
 std::optional<int> m_groundCoverPending;int m_groundCoverCancellation{};
 struct FrameRequest {struct {std::uint64_t terrain_request{};}water_source;};
 struct FramePublicationPort {
  std::optional<int> pending;int cancellation{};
  std::optional<FrameRequest> request;bool admitted{};
 };
 std::unordered_map<const void*,FramePublicationPort> m_planetaryFramePublications;
 std::shared_ptr<ControllerPort> m_explorationController=std::make_shared<ControllerPort>();
 struct {double seconds{},time_scale{1};std::uint32_t anchor{2};}m_explorationFrame;
 std::optional<epochengine::timeline::TimelineState> m_runtimePresentation;
 std::optional<double> m_planetaryWaterObservedSeconds,m_planetaryWaterObservedScale;
 timing::Clock::time_point m_planetaryWaterObservedTick{},m_planetaryWaterNextTick{};
 timing::Clock::time_point m_planetaryWaterEvidenceTick{};
 int m_planetaryWaterClockDirection{};std::uint64_t m_planetaryWaterClockGeneration{};
 std::shared_ptr<const project_planetary_snapshot::Snapshot> m_planetaryWaterSource,m_planetaryWaterPublishedSource;
 std::shared_ptr<const asset::mesh::SphericalWaterAppearance> m_planetaryWaterAppearance;
 std::optional<int> m_planetaryMeshFrame;
 std::optional<project_planetary::Settings> m_planetarySettings{project_planetary::Settings{}};
 std::optional<project_planetary::Settings> m_planetaryRequestSettings{m_planetarySettings};
 std::optional<project_planetary::SourceGeneration> m_meshPlanetarySource{project_planetary::SourceGeneration{}};
 std::shared_ptr<const project_planetary::PackagedShoreline> m_packagedShoreline;
 double m_packagedWaterElapsed{};
 std::optional<double> m_planetaryWaterPublishedSeconds;
 bool m_planetaryWaterCatchUp{};
 std::unordered_map<unsigned,std::shared_ptr<const asset::mesh::Artifact>> m_meshes;
 unsigned m_planetaryEntity{7};std::string m_projectRoot{"owned"},m_planetaryWaterStatus;bool m_planetaryWaterSourceRefused{};
 void ensure_planetary_scheduler(){}
 bool planetary_mesh_frame_fits(const std::shared_ptr<const asset::mesh::Artifact>&,const std::optional<int>&){return true;}
 std::optional<previewgrid::SphericalWaterView> water_detail_for_cut(unsigned body_id,
     std::array<double,3> transform_scale={1,1,1}) const {
  const auto m_planetaryLodBody=2u;
  const struct {unsigned id;double radius;} body{body_id,0.6371};
  const struct {struct {std::array<double,3> scale;} transform;} authored{{transform_scale}};
  const auto* entity=&authored;
  const auto scale=transform_scale[0];
  const struct {double meters_to_preview;} preview{1e-7};
  const auto* m_explorationPreview=&preview;
  const double sceneUnits=1.;
  const std::array<double,3> atmosphereCenter{1.,2.,3.};
  struct {std::shared_ptr<const asset::mesh::Artifact> mesh=std::make_shared<asset::mesh::Artifact>();
    std::shared_ptr<const asset::mesh::SphericalWaterAppearance> sphericalWater;
    std::optional<previewgrid::SphericalWaterView> waterDetail;} marker;
  WATER_MARKER
  return marker.waterDetail;
 }
 CLOCK
 APPLY_TIME
 ADVANCE_TIME
 void observe_planetary_water_clock(timing::Clock::time_point now){observe_planetary_water_clock(now,m_explorationFrame.seconds,m_explorationFrame.time_scale);}
 RESET
 ADOPT
 CLOCK_SAMPLE
 PUMP
 void pump_planetary_water(){
  const auto now=timing::Clock::now();
  const auto [seconds,scale]=planetary_water_clock_sample();
  observe_planetary_water_clock(now,seconds,scale);
  pump_planetary_water(now,seconds,scale);
 }
 void pick_initial_cut(project_planetary::StreamResult& result){const bool initialWater=false;STAGE_CUT}
 void queue_initial_cut(project_planetary::StreamView view={}){
  const auto now=timing::Clock::now();
  pump_planetary_cut_playback(view,now);
  QUEUE_CUT
 }
 void retire(){RETIRE}
 bool accepts(const PlanetaryWaterResult& result)const {return COMPLETION;}
};
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main() {
 {
  Runtime playback;
  const auto previous=std::make_shared<const asset::mesh::Artifact>();
  const auto phase=std::make_shared<const asset::mesh::Artifact>();
  const auto settled=std::make_shared<const asset::mesh::Artifact>();
  const auto prepare=[&]{
   project_planetary::StreamResult target;
   target.snapshot=settled;target.patch_snapshot=std::make_shared<const project_planetary_snapshot::Snapshot>();
   target.operation.mesh_frame=12;target.water_sample_seconds=3.5;
   target.wet_transition.push_back({.mesh=phase});
   playback.m_meshes[7]=previous;playback.m_planetaryMeshFrame=4;
   playback.m_planetaryNextTick={};
   playback.m_planetaryCutPlayback=Runtime::PlanetaryCutPlayback{std::move(target),previous,{},4};
  };
  const auto now=timing::Clock::now();
  prepare();
  CHECK(playback.pump_planetary_cut_playback({},now)&&playback.m_meshes[7]==phase);
  CHECK(playback.m_planetaryMeshFrame==12&&playback.m_planetaryCutPlayback->next==1);
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{49})&&playback.m_meshes[7]==phase);
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{50})&&playback.m_meshes[7]==settled);
  CHECK(!playback.m_planetaryCutPlayback&&playback.m_planetaryWaterPublishedSeconds==3.5);
  CHECK(playback.m_planetaryWaterCatchUp);
  prepare();
  CHECK(playback.pump_planetary_cut_playback({},now)&&playback.m_meshes[7]==phase);
  ++playback.m_planetaryWaterClockGeneration;
  CHECK(!playback.pump_planetary_cut_playback({},now)&&playback.m_meshes[7]==previous);
  CHECK(playback.m_planetaryMeshFrame==4&&!playback.m_planetaryCutPlayback&&playback.m_planetaryContinue);
 }
 {
  Runtime playback;
  auto previous=std::make_shared<project_planetary_snapshot::Snapshot>();
  auto incoming=std::make_shared<project_planetary_snapshot::Snapshot>();
  previous->artifact=std::make_shared<asset::mesh::Artifact>();
  incoming->artifact=std::make_shared<asset::mesh::Artifact>();
  previous->water_sample_seconds=3.0;incoming->water_sample_seconds=3.5;
  const auto start=[&] {
   project_planetary::StreamResult result;
   result.request_id=18;result.patch_snapshot=incoming;result.snapshot=incoming->artifact;
   result.water_sample_seconds=3.5;result.operation.mesh_frame=12;
   Runtime::prepare_planetary_wet_transition(result,previous,std::make_shared<const project_planetary::ShorelineSourceReview>(),
       *playback.m_planetarySettings,previous->bytes,{},playback.m_explorationController->provider,
       playback.m_explorationController->lattice,{});
   playback.m_meshes[7]=previous->artifact;
   playback.m_planetaryMeshFrame=4;playback.m_planetaryNextTick={};
   playback.m_planetaryCutPlayback.emplace(Runtime::PlanetaryCutPlayback{std::move(result),previous->artifact,{},4});
  };
  const auto now=timing::Clock::now();
  start();
  CHECK(playback.m_planetaryCutPlayback->target.wet_transition.size()==4);
  CHECK(playback.m_planetaryCutPlayback->target.wet_cursor->steps==4);
  int context{};
  CHECK(playback.pump_planetary_cut_playback({},now,&context));
  const auto first=playback.m_meshes[7];
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{60},&context));
  CHECK(playback.m_meshes[7]==first&&playback.m_planetaryCutPlayback->next==1);
  previewgrid::published=std::make_shared<previewgrid::Frame>();
  previewgrid::published->markers.push_back({first});
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{60},&context));
  CHECK(playback.m_meshes[7]==first&&playback.m_planetaryCutPlayback->next==1);
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{110},&context));
  CHECK(playback.m_meshes[7]!=first&&playback.m_planetaryCutPlayback->next==2);
  previewgrid::published.reset();
  for(unsigned i=3;i<=5;++i)
   CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{60*i}));
  CHECK(playback.m_planetaryPreparingWetBatch&&playback.m_planetaryPending);
  CHECK(playback.m_planetaryPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{360}));
  CHECK(!playback.m_planetaryPending&&playback.m_planetaryCutPlayback->next==1);
  CHECK(playback.m_planetaryCutPlayback->target.wet_transition.size()==2);
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{410}));
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{460}));
  CHECK(!playback.m_planetaryCutPlayback&&playback.m_meshes[7]==incoming->artifact);
  // Preparing the next batch overlaps the final committed phase's hold.
  // A ready future must leave that phase visible until the hold expires.
  start();
  for(unsigned i=0;i<3;++i)
   CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{60*i}));
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{180},&context));
  const auto last=playback.m_meshes[7];
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{181},&context));
  CHECK(!playback.m_planetaryPending&&playback.m_meshes[7]==last);
  previewgrid::published=std::make_shared<previewgrid::Frame>();
  previewgrid::published->markers.push_back({last});
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{185},&context));
  CHECK(playback.m_planetaryPreparingWetBatch&&playback.m_planetaryPending&&playback.m_meshes[7]==last);
  CHECK(playback.m_planetaryPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{234},&context));
  CHECK(!playback.m_planetaryPending&&playback.m_planetaryCutPlayback->next==0&&playback.m_meshes[7]==last);
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{235},&context));
  CHECK(playback.m_planetaryCutPlayback->next==1&&playback.m_meshes[7]!=last);
  playback.restore_planetary_cut_playback();
  previewgrid::published.reset();
  start();
  for(unsigned i=0;i<=4;++i)
   CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{60*i}));
  CHECK(playback.m_planetaryPreparingWetBatch&&playback.m_planetaryPending);
  playback.m_planetaryFramePublications[&context]={1,2,Runtime::FrameRequest{{18u}},true};
  ++playback.m_planetaryWaterClockGeneration;
  playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{300});
  CHECK(!playback.m_planetaryCutPlayback&&playback.m_meshes[7]==previous->artifact);
  CHECK(!playback.m_planetaryFramePublications[&context].request&&!playback.m_planetaryFramePublications[&context].admitted);
  if(playback.m_planetaryPending)
   CHECK(playback.m_planetaryPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  CHECK(!playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{350}));
  CHECK(!playback.m_planetaryPending&&!playback.m_planetaryPreparingWetBatch&&playback.m_planetaryContinue);
  playback.m_planetaryRequestWaterClock=playback.m_planetaryWaterClockGeneration;
  start();
  for(unsigned i=0;i<4;++i)
   CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{60*i}));
  recipe_current=false;
  CHECK(playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{240}));
  CHECK(playback.m_planetaryPending->wait_for(std::chrono::seconds{2})==std::future_status::ready);
  CHECK(!playback.pump_planetary_cut_playback({},now+std::chrono::milliseconds{300}));
  CHECK(!playback.m_planetaryCutPlayback&&playback.m_meshes[7]==previous->artifact&&playback.m_planetaryFailed);
  recipe_current=true;
  source_checks=water_checks=0;
 }
 auto base=std::make_shared<project_planetary_snapshot::Snapshot>();
 auto provider=std::make_shared<FakeProvider>();const project_planetary::Settings settings;
 auto run=[&](std::stop_token stop={}) {return Runtime::sample_planetary_water("owned",{},base,provider,settings,3.25,"water-approved",7,1024,stop);};
 auto result=run();CHECK(result.mesh&&result.appearance&&result.source_current&&!result.cancelled&&result.seconds==3.25&&result.clock_generation==7);
 CHECK(result.source==base&&result.body==2&&provider->budget==64ull*1024*1024&&source_checks==2&&water_checks==2&&resamples==1);
 CHECK(std::isfinite(result.worker_seconds)&&result.worker_seconds>=0.0);
 source_checks=water_checks=resamples=0;planet_current=false;result=run();CHECK(!result.mesh&&!result.source_current&&provider->calls==1&&source_checks==1&&water_checks==0);
 planet_current=true;provider->replace_water=true;result=run();CHECK(!result.mesh&&!result.appearance&&!result.source_current&&water_checks==2);
 water_current=true;provider->replace_water=false;provider->fail=true;result=run();CHECK(!result.mesh&&result.source_current&&!result.cancelled);
 provider->fail=false;provider->wrong_time=true;const auto before=resamples;result=run();CHECK(!result.mesh&&result.source_current&&resamples==before);
 provider->wrong_time=false;std::stop_source stop;stop.request_stop();const auto calls=provider->calls.load();result=run(stop.get_token());CHECK(result.cancelled&&!result.mesh&&provider->calls==calls);
 std::stop_source during_admission;cancellation_port=&during_admission;cancel_in_source_check=true;
 result=run(during_admission.get_token());CHECK(result.cancelled&&!result.mesh&&!result.source_current&&provider->calls==calls);
 cancel_in_source_check=false;
 std::stop_source during;cancellation_port=&during;cancel_in_resample=true;result=run(during.get_token());CHECK(result.cancelled&&!result.mesh&&!result.source_current);cancel_in_resample=false;
 const auto count=provider->calls.load();result=Runtime::sample_planetary_water("owned",{},base,provider,settings,1,"water-approved",7,180ull*1024*1024,{});
 CHECK(!result.mesh&&result.source_current&&provider->calls==count);
 {
  Runtime packaged;
  const auto tick=timing::Clock::now();
  auto shoreline=std::make_shared<project_planetary::PackagedShoreline>();
  shoreline->selection.source=base;
  shoreline->fft_source="water-approved";
  shoreline->sample_seconds=3.25;
  packaged.m_packagedShoreline=shoreline;
  packaged.m_packagedWaterElapsed=0.5;
  packaged.m_meshPlanetarySource.reset();
  packaged.adopt_planetary_water_source(base,shoreline->sample_seconds);
  const auto sources_before=source_checks;
  const auto [seconds,scale]=packaged.planetary_water_clock_sample();
  CHECK(seconds==3.75&&scale==1.0);
  packaged.pump_planetary_water(tick,seconds,scale);
  CHECK(packaged.m_planetaryWaterPending->wait_for(std::chrono::seconds{1})==std::future_status::ready);
  packaged.pump_planetary_water(tick+std::chrono::milliseconds{100},seconds,scale);
  CHECK(packaged.m_planetaryWaterPublishedSeconds==3.75&&source_checks==sources_before);
  packaged.retire();
 }
 {
  using namespace project_planetary_snapshot;
  auto earlier=std::make_shared<Snapshot>();
  auto incoming=std::make_shared<Snapshot>();
  earlier->artifact=std::make_shared<asset::mesh::Artifact>();
  incoming->artifact=std::make_shared<asset::mesh::Artifact>();
  earlier->water_sample_seconds=incoming->water_sample_seconds=6.25;
  auto wave=std::make_shared<FakeProvider>();
  const auto basis=std::make_shared<const WaterLatticeProvider>();
  const auto start=[&](bool sample_edge,std::uint64_t external,std::stop_token stop={}) {
   project_planetary::StreamResult cut;
   cut.patch_snapshot=incoming;
   if(sample_edge)cut.water_edge_sample_seconds=incoming->water_sample_seconds;
   else {cut.snapshot=incoming->artifact;cut.water_sample_seconds=incoming->water_sample_seconds;}
   return Runtime::prepare_initial_planetary_water_cut("owned",settings,
    {std::move(cut),wave,"water-approved",earlier,basis},7,earlier->bytes+external,stop);
  };
  retained_sample={};
  const auto before_review=project_planetary::review_count;
  const auto before_recheck=project_planetary::recheck_count;
  auto matched=start(false,1024);
  CHECK(matched.operation.ok&&matched.wet_cursor&&wave->calls==0&&retained_sample.calls==0);
  CHECK(project_planetary::review_count==before_review+1&&project_planetary::recheck_count==before_recheck+1);
  const auto source_reads=source_checks,water_reads=water_checks;
  const auto completed=Runtime::continue_planetary_wet_cut("owned",settings,matched,earlier->bytes,{},std::nullopt);
  CHECK(completed.operation.ok&&!completed.wet_cursor&&completed.wet_transition.size()==2);
  CHECK(logger::wet_batch_record.count==0);
  CHECK(source_checks==source_reads&&water_checks==water_reads);
  CHECK(project_planetary::review_count==before_review+1&&project_planetary::recheck_count==before_recheck+3);
  project_planetary::reject_recheck=project_planetary::recheck_count+2;
  const auto queued_at=timing::Clock::now()-std::chrono::milliseconds{5};
  const auto stale_batch=Runtime::continue_planetary_wet_cut("owned",settings,matched,earlier->bytes,{},queued_at);
  CHECK(!stale_batch.operation.ok&&!stale_batch.operation.cancelled&&!stale_batch.wet_cursor&&stale_batch.wet_transition.empty());
  CHECK(logger::wet_batch_record.count==1&&logger::wet_batch_record.first_step==4
   &&logger::wet_batch_record.frames==2&&!logger::wet_batch_record.source_current&&logger::wet_batch_record.queue_ms>=5.0);
  std::stop_source stop_recheck;
  cancellation_port=&stop_recheck;
  project_planetary::stop_recheck=project_planetary::recheck_count+2;
  const auto cancelled_batch=Runtime::continue_planetary_wet_cut("owned",settings,matched,earlier->bytes,stop_recheck.get_token(),std::nullopt);
  CHECK(!cancelled_batch.operation.ok&&cancelled_batch.operation.cancelled&&!cancelled_batch.wet_cursor&&cancelled_batch.wet_transition.empty());
  CHECK(source_checks==source_reads&&water_checks==water_reads);
  cancellation_port=nullptr;

  // Force two real cursor rollovers. Neither may undo an advance or skip a frame.
  const auto staged_mesh=std::make_shared<const asset::mesh::Artifact>();
  const auto staged_appearance=std::make_shared<const asset::mesh::SphericalWaterAppearance>();
  wet_output_budget=2u*Runtime::planetary_cut_mesh_bytes(staged_mesh,staged_appearance)-1u;
  auto rolling=start(false,0u);
  CHECK(rolling.wet_transition.size()==2&&rolling.wet_cursor&&rolling.wet_cursor->steps==2);
  CHECK(rolling.wet_cursor->materialization_pending&&wet_advances==3);
  rolling=Runtime::continue_planetary_wet_cut("owned",settings,std::move(rolling),earlier->bytes,{},queued_at);
  CHECK(rolling.wet_transition.size()==2&&rolling.wet_cursor&&rolling.wet_cursor->steps==4);
  CHECK(rolling.wet_cursor->materialization_pending&&wet_advances==5);
  CHECK(logger::wet_batch_record.count==2&&logger::wet_batch_record.first_step==2
   &&logger::wet_batch_record.frames==2&&logger::wet_batch_record.source_current);
  rolling=Runtime::continue_planetary_wet_cut("owned",settings,std::move(rolling),earlier->bytes,{},std::nullopt);
  CHECK(rolling.wet_transition.size()==2&&!rolling.wet_cursor&&rolling.operation.ok&&wet_advances==7);
  CHECK((wet_materializations==std::vector<unsigned>{1,2,3,4,5,6}));
  wet_output_budget.reset();

  earlier=std::make_shared<Snapshot>(*earlier);
  earlier->water_sample_seconds=6.0;
  constexpr auto tight=179ull*1024*1024;
  retained_sample={};
  auto lazy=start(false,tight);
  CHECK(lazy.operation.ok&&lazy.wet_cursor&&wave->calls==1&&retained_sample.calls==1);
  CHECK(retained_sample.tile==wave->last_tile&&retained_sample.tile->seconds()==6.25&&retained_sample.basis==basis);
  CHECK(retained_sample.external_bytes==tight);
  CHECK(wave->budget==Limits{}.maximum_retained_bytes-tight-earlier->bytes-incoming->bytes);

  auto saturated=start(false,Limits{}.maximum_retained_bytes-earlier->bytes-incoming->bytes);
  CHECK(saturated.operation.ok&&saturated.snapshot==incoming->artifact&&wave->calls==1);
  CHECK(!saturated.wet_cursor&&saturated.wet_transition.empty());

  retained_sample={};
  const auto before_edges=resamples;
  auto edges=start(true,2048);
  CHECK(edges.operation.ok&&edges.wet_cursor&&edges.snapshot!=incoming->artifact);
  CHECK(wave->calls==2&&resamples==before_edges+1&&retained_sample.calls==1);
  CHECK(retained_sample.tile==wave->last_tile&&retained_sample.tile.get()==resample_tile);
  CHECK(retained_sample.external_bytes==2048+Runtime::planetary_cut_mesh_bytes(edges.snapshot,edges.water_appearance));

  wave->wrong_time=true;
  auto wrong_clock=start(false,1024);
  CHECK(wrong_clock.operation.ok&&wrong_clock.snapshot==incoming->artifact);
  CHECK(!wrong_clock.wet_cursor&&wrong_clock.wet_transition.empty());
  wave->wrong_time=false;
  refuse_basis=true;
  auto missing_basis=start(false,1024);
  CHECK(missing_basis.operation.ok&&missing_basis.snapshot==incoming->artifact);
  CHECK(!missing_basis.wet_cursor&&missing_basis.wet_transition.empty());
  refuse_basis=false;

  wave->replace_water=true;
  auto stale_water=start(false,1024);
  CHECK(!stale_water.operation.ok&&!stale_water.snapshot_ready&&!stale_water.wet_cursor&&stale_water.wet_transition.empty());
  wave->replace_water=false;water_current=true;
  wave->replace_planet=true;
  auto stale_terrain=start(false,1024);
  CHECK(!stale_terrain.operation.ok&&!stale_terrain.snapshot_ready&&!stale_terrain.wet_cursor&&stale_terrain.wet_transition.empty());
  wave->replace_planet=false;planet_current=true;
  const auto before_admission_cancel=wave->calls.load();
  std::stop_source stop_admission;
  cancellation_port=&stop_admission;
  cancel_in_source_check=true;
  auto cancelled_admission=start(false,1024,stop_admission.get_token());
  CHECK(!cancelled_admission.operation.ok&&cancelled_admission.operation.cancelled&&wave->calls==before_admission_cancel);
  CHECK(!cancelled_admission.wet_cursor&&cancelled_admission.wet_transition.empty());
  cancel_in_source_check=false;
  std::stop_source stop_retained;
  cancellation_port=&stop_retained;
  wave->cancel_in_evaluate=true;
  auto cancelled=start(false,1024,stop_retained.get_token());
  CHECK(cancelled.operation.cancelled&&!cancelled.wet_cursor&&cancelled.wet_transition.empty());
  cancellation_port=nullptr;
 }
 Runtime mixed;
 auto mutableFft=mixed.m_explorationController->provider;
 mutableFft->block=true;
 auto visible=std::make_shared<const asset::mesh::Artifact>();
 mixed.m_meshes[mixed.m_planetaryEntity]=visible;
 mixed.m_planetaryWaterSource=base;
 mixed.m_explorationFrame.seconds=7.87;
 mixed.pump_planetary_water();
 for(unsigned i=0;i<1000&&!mutableFft->active;++i)std::this_thread::sleep_for(std::chrono::milliseconds{1});
 CHECK(mutableFft->active==1);
 auto cut=std::make_shared<project_planetary_snapshot::Snapshot>();
 const auto raw_cut=[&]{return project_planetary::StreamResult{
  .snapshot=std::make_shared<const asset::mesh::Artifact>(),
  .water_appearance=std::make_shared<const asset::mesh::SphericalWaterAppearance>(),
  .patch_snapshot=cut,.water_edge_sample_seconds=3.2875};};
 auto pending=raw_cut();
 mixed.pick_initial_cut(pending);
 CHECK(mixed.m_planetaryInitialWaterCut&&!mixed.m_planetaryInitialWaterCut->stream.snapshot
  &&!mixed.m_planetaryInitialWaterCut->stream.water_sample_seconds);
 mixed.queue_initial_cut();
 CHECK(!mixed.m_planetaryPending&&mixed.m_planetaryWaterPending);
 mutableFft->block=false;
 for(unsigned i=0;i<1000&&mixed.m_planetaryWaterPending;++i){
  mixed.pump_planetary_water();std::this_thread::sleep_for(std::chrono::milliseconds{1});}
 CHECK(!mixed.m_planetaryWaterPending&&mixed.m_meshes[mixed.m_planetaryEntity]==visible);
 mixed.m_planetarySettings->stream_lod=false;
 mixed.m_planetaryRequestSettings=mixed.m_planetarySettings;
 mixed.queue_initial_cut();
 CHECK(mixed.m_planetaryPreparingInitialWater&&!mixed.m_planetaryInitialWaterCut&&mixed.m_planetaryPending);
 mixed.pump_planetary_water();
 CHECK(!mixed.m_planetaryWaterPending&&mixed.m_meshes[mixed.m_planetaryEntity]==visible);
 const auto sampled=mixed.m_planetaryPending->get();
 CHECK(sampled.operation.ok&&sampled.snapshot_ready&&sampled.snapshot&&sampled.water_appearance);
 CHECK(sampled.water_appearance->artifact_key==sampled.snapshot->key()
  &&sampled.patch_snapshot==cut&&sampled.water_sample_seconds==3.2875&&!sampled.water_edge_sample_seconds);
 CHECK(mutableFft->calls==2&&mutableFft->maximum_active==1);
 Runtime stale;
 auto outdated=raw_cut();
 stale.pick_initial_cut(outdated);
 ++stale.m_planetaryWaterClockGeneration;
 stale.queue_initial_cut();
 CHECK(!stale.m_planetaryInitialWaterCut&&!stale.m_planetaryPending&&stale.m_planetaryContinue);
 Runtime full;
 auto retained=std::make_shared<project_planetary_snapshot::Snapshot>();
 retained->bytes=project_planetary_snapshot::Limits{}.maximum_retained_bytes;
 full.m_planetaryWaterSource=retained;
 full.m_meshes[full.m_planetaryEntity]=visible;
 auto exceeds_budget=raw_cut();
 full.pick_initial_cut(exceeds_budget);
 full.queue_initial_cut();
 CHECK(full.m_planetaryFailed&&full.m_planetaryStatus.find("memory budget")!=std::string::npos);
 CHECK(!full.m_planetaryInitialWaterCut&&!full.m_planetaryPending&&!full.m_planetaryPreparingInitialWater);
 CHECK(full.m_meshes[full.m_planetaryEntity]==visible&&full.m_explorationController->provider->calls==0);
 Runtime unlinked;
 unlinked.m_explorationController->provider.reset();
 auto unsupported=raw_cut();
 unlinked.pick_initial_cut(unsupported);
 CHECK(!unlinked.m_planetaryInitialWaterCut&&unsupported.snapshot&&unsupported.water_appearance);
 auto legacy=raw_cut();legacy.water_edge_sample_seconds.reset();
 mixed.pick_initial_cut(legacy);
 CHECK(!mixed.m_planetaryInitialWaterCut&&legacy.snapshot&&legacy.water_appearance);
 auto rejected=raw_cut();
 provider->wrong_time=true;
 const auto failed=Runtime::prepare_initial_planetary_water_cut("owned",settings,
  {std::move(rejected),provider,"water-approved"},7,1024,{});
 CHECK(!failed.operation.ok&&!failed.snapshot_ready&&!failed.water_sample_seconds);
 provider->wrong_time=false;
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
 publishing.m_groundCoverPending=1;publishing.m_groundCoverCancellation=8;
 publishing.retire();
 CHECK(scheduler->cancels==2&&publishing.m_planetaryFramePublications.empty()&&!publishing.m_planetaryScheduler);
 // Paused simulation requests its exact quantized sample once, then stays idle.
 Runtime paused;paused.m_planetaryWaterSource=base;paused.m_explorationFrame.seconds=3.25;paused.m_explorationFrame.time_scale=0;
 auto idle=paused.m_explorationController->provider;paused.pump_planetary_water();
 for(unsigned i=0;i<1000&&paused.m_planetaryWaterPending;++i){std::this_thread::sleep_for(std::chrono::milliseconds{1});paused.pump_planetary_water();}
 CHECK(paused.m_planetaryWaterPublishedSeconds==3.25&&!paused.m_planetaryWaterPending&&idle->calls==1);
 for(unsigned i=0;i<100;++i)paused.pump_planetary_water();CHECK(idle->calls==1&&paused.m_planetaryWaterStatus=="Ocean: paused.");
 // Each verified stream cut already contains the requested frozen sample.
 // A new geometry identity alone must not queue another FFT evaluation.
 Runtime adoption;
 const auto observed=timing::Clock::time_point{std::chrono::seconds{1}};
 adoption.observe_planetary_water_clock(observed,1.0,0.5);
 const auto cutTick=observed+std::chrono::milliseconds{20};
 adoption.observe_planetary_water_clock(cutTick,1.01,0.5);
 adoption.m_planetaryWaterAppearance=std::make_shared<const asset::mesh::SphericalWaterAppearance>();
 adoption.adopt_planetary_water_source(base,1.0);
 CHECK(adoption.m_planetaryWaterObservedSeconds==1.01&&adoption.m_planetaryWaterObservedScale==0.5
  &&adoption.m_planetaryWaterObservedTick==cutTick&&adoption.m_planetaryWaterClockDirection==1);
 CHECK(adoption.water_detail_for_cut(2));
 adoption.reset_planetary_water();
 CHECK(!adoption.m_planetaryWaterObservedSeconds&&!adoption.m_planetaryWaterObservedScale
  &&adoption.m_planetaryWaterObservedTick==timing::Clock::time_point{}&&adoption.m_planetaryWaterClockDirection==0);
 // Resume an admitted wet cut in bounded phase steps while the clock runs.
 // Keep one provider call, and return to the normal absolute clock on arrival.
 {
  Runtime catchUp;
  const auto tick=timing::Clock::now();
  catchUp.adopt_planetary_water_source(base,7.4833333333333121);
  catchUp.m_planetaryWaterCatchUp=true;
  catchUp.pump_planetary_water(tick,18.05,1.0);
  const auto provider=catchUp.m_explorationController->provider;
  CHECK(catchUp.m_planetaryWaterPending->wait_for(std::chrono::seconds{1})==std::future_status::ready);
  CHECK(provider->last_tile->seconds()==8.2);
  catchUp.pump_planetary_water(tick+std::chrono::milliseconds{100},18.15,1.0);
  CHECK(catchUp.m_planetaryWaterPublishedSeconds==8.2&&catchUp.m_planetaryWaterCatchUp);
  CHECK(catchUp.m_planetaryWaterPending->wait_for(std::chrono::seconds{1})==std::future_status::ready);
  CHECK(provider->last_tile->seconds()==8.95&&provider->maximum_active==1);
  catchUp.retire();
 }
 {
  Runtime reverse;
  const auto tick=timing::Clock::now();
  reverse.adopt_planetary_water_source(base,18.03);
  reverse.m_planetaryWaterCatchUp=true;
  reverse.pump_planetary_water(tick,16.8,-1.0);
  CHECK(reverse.m_planetaryWaterPending->wait_for(std::chrono::seconds{1})==std::future_status::ready);
  CHECK(reverse.m_explorationController->provider->last_tile->seconds()==17.3);
  reverse.pump_planetary_water(tick+std::chrono::milliseconds{100},16.75,-1.0);
  CHECK(reverse.m_planetaryWaterPending->wait_for(std::chrono::seconds{1})==std::future_status::ready);
  reverse.pump_planetary_water(tick+std::chrono::milliseconds{200},16.75,-1.0);
  CHECK(reverse.m_planetaryWaterPublishedSeconds==16.75&&!reverse.m_planetaryWaterCatchUp);
  reverse.retire();
 }
 Runtime frozen;frozen.m_explorationFrame.time_scale=0;
 const auto frozenProvider=frozen.m_explorationController->provider;
 for(unsigned revision=0;revision<2;++revision){
  auto baked=std::make_shared<project_planetary_snapshot::Snapshot>();
  frozen.adopt_planetary_water_source(baked,0.0);
  frozen.pump_planetary_water();
  CHECK(!frozen.m_planetaryWaterPending&&frozenProvider->calls==0);
  CHECK(frozen.m_planetaryWaterPublishedSource==baked&&frozen.m_planetaryWaterPublishedSeconds==0.0);
  CHECK(frozen.m_planetaryWaterStatus=="Ocean: paused.");
 }
 // A different time and a legacy cut without a time receipt still evaluate.
 frozen.adopt_planetary_water_source(base,0.125);
 frozen.pump_planetary_water();CHECK(frozen.m_planetaryWaterPending);
 for(unsigned i=0;i<1000&&frozen.m_planetaryWaterPending;++i){std::this_thread::sleep_for(std::chrono::milliseconds{1});frozen.pump_planetary_water();}
 CHECK(frozenProvider->calls==1&&frozen.m_planetaryWaterPublishedSeconds==0.0);
 frozen.adopt_planetary_water_source(base,std::nullopt);
 CHECK(!frozen.m_planetaryWaterPublishedSeconds);
 const auto incomingAppearance=std::make_shared<asset::mesh::SphericalWaterAppearance>();
 frozen.m_planetaryWaterAppearance=incomingAppearance;
 frozen.pump_planetary_water();CHECK(frozen.m_planetaryWaterPending);
 const auto liveBeforeResample=frozen.water_detail_for_cut(2);
 CHECK(liveBeforeResample&&liveBeforeResample->seconds==0.0);
 CHECK(!frozen.water_detail_for_cut(1)&&!frozen.water_detail_for_cut(2,{1,1,2}));
 for(unsigned i=0;i<1000&&frozen.m_planetaryWaterPending;++i){std::this_thread::sleep_for(std::chrono::milliseconds{1});frozen.pump_planetary_water();}
 CHECK(frozenProvider->calls==2&&frozen.m_planetaryWaterPublishedSeconds==0.0);
 CHECK(frozen.water_detail_for_cut(2)==liveBeforeResample);
 frozen.m_planetaryWaterObservedSeconds=1.25;
 frozen.m_planetaryWaterPublishedSeconds=0.125;
 CHECK(frozen.water_detail_for_cut(2)==liveBeforeResample);
 frozen.m_planetaryWaterObservedSeconds.reset();CHECK(!frozen.water_detail_for_cut(2));
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
 seeking.adopt_planetary_water_source(base,1.0);
 seeking.m_planetaryWaterCatchUp=true;
 auto seekProvider=seeking.m_explorationController->provider;seekProvider->block=true;
 seeking.pump_planetary_water();for(unsigned i=0;i<1000&&!seekProvider->active;++i)std::this_thread::sleep_for(std::chrono::milliseconds{1});
 CHECK(seekProvider->active==1);CHECK(seeking.apply_runtime_presentation({9.5,120,1,true,false},&seekContext));
 CHECK(!seeking.m_planetaryWaterCatchUp);
 seekProvider->block=false;
 for(unsigned i=0;i<1000&&seeking.m_planetaryWaterPublishedSeconds!=9.5;++i){seeking.pump_planetary_water();std::this_thread::sleep_for(std::chrono::milliseconds{1});}
 CHECK(seeking.m_planetaryWaterPublishedSeconds==9.5&&seekProvider->maximum_active==1);seeking.retire();
}
'''


class PlanetaryWaterClock(unittest.TestCase):
    def test_actual_runtime_worker_clock_and_completion(self):
        source = OWNER.read_text(encoding='utf-8')
        result = definition(source, '            struct PlanetaryWaterResult final') + ';'
        cut_type = definition(source, '            struct InitialPlanetaryWaterCut final') + ';'
        worker = definition(source, '            [[nodiscard]] static PlanetaryWaterResult sample_planetary_water(')
        prepare = definition(source, '            [[nodiscard]] static project_planetary::StreamResult prepare_initial_planetary_water_cut(')
        display = definition(source, '            [[nodiscard]] std::uint64_t planetary_water_display_bytes(')
        clock = definition(source, '            void observe_planetary_water_clock(')
        apply_time = definition(source, '            bool apply_runtime_presentation(')
        advance_time = definition(source, '            void advance_runtime_presentation(')
        reset = definition(source, '            void reset_planetary_water(')
        adopt = definition(source, '            void adopt_planetary_water_source(')
        pump = definition(source, '            void pump_planetary_water(')
        clock_sample = definition(source, '            [[nodiscard]] std::pair<double,double> planetary_water_clock_sample(')
        marker = '                                     const bool transition_supported='
        stage = source[source.index(marker):source.index('                                     if(!initialWater&&m_explorationController')]
        stage += definition(source, 'if(!initialWater&&m_explorationController')
        queue = definition(source, '                if(m_planetaryInitialWaterCut)\n')
        marker = definition(source, '                    if(body.id==m_planetaryLodBody&&marker.mesh')
        preview = (ROOT / 'Engine/modules/render/render.preview_grid.ixx').read_text(encoding='utf-8')
        water_view = definition(preview, '    export struct SphericalWaterView final').replace('export ', '', 1)
        shutdown = definition(source, '            void shutdown_planetary_runtime(')
        retire = definition(shutdown, '                try')[len('                try'):]
        completion = 'result.source==m_planetaryWaterSource&&m_planetarySettings' + condition(
            source, 'if(result.source==m_planetaryWaterSource&&m_planetarySettings')
        code = CODE.replace('RESULT_TYPE', result).replace('WORKER', worker).replace(' CLOCK\n', clock+'\n').replace('COMPLETION', completion)
        timeline=(ROOT/'Engine/modules/timeline/timeline.system.ixx').read_text(encoding='utf-8')
        timeline=timeline[timeline.index('export namespace epochengine::timeline'):timeline.index('    inline void clamp_view_config')].replace('export namespace','namespace',1)+'}\n'
        stats=(ROOT/'Engine/dep/EpochPlatformEngine/modules/core.time.ixx').read_text(encoding='utf-8')
        protocol=(ROOT/'Engine/modules/platform/platform.child_process.ixx').read_text(encoding='utf-8')
        code=code.replace('CLOCK_STATS',definition(stats,'    struct simulation_stats')).replace('TIMELINE',timeline)
        code=code.replace('VALID_PRESENTATION',definition(protocol,'    [[nodiscard]] inline bool valid_runtime_presentation(')).replace('PRESENTATION',definition(protocol,'    struct RuntimePresentation final'))
        code=code.replace('APPLY_TIME',apply_time).replace('ADVANCE_TIME',advance_time)
        code=code.replace('CUT_TYPE',cut_type).replace('PREPARE_CUT',prepare).replace('DISPLAY_BYTES',display)
        code=code.replace('PLAYBACK_TYPE',definition(source,'            struct PlanetaryCutPlayback final')+';')
        code=code.replace('PLAYBACK_FUNCTIONS','\n'.join(definition(source,marker) for marker in (
            '            void publish_planetary_stream_cut(', '            bool pump_planetary_cut_playback(',
            '            [[nodiscard]] bool planetary_wet_frame_committed(',
            '            [[nodiscard]] bool planetary_wet_frames_fit(', '            void queue_planetary_wet_batch(',
            '            void restore_planetary_cut_playback(')))
        code=code.replace('CLOCK_SAMPLE',clock_sample).replace('STAGE_CUT',stage).replace('QUEUE_CUT',queue)
        code = code.replace('RESET', reset).replace('ADOPT', adopt).replace('PUMP', pump).replace('RETIRE', retire)
        code = code.replace('WATER_VIEW', water_view).replace('WATER_MARKER', marker)
        types=(ROOT/'Engine/modules/project/project.planetary.ixx').read_text(encoding='utf-8')
        code=code.replace('WET_CURSOR',definition(types,'    struct StreamWetCutCursor final'))
        code=code.replace('PACKAGED_SHORELINE',definition(types,'    struct PackagedShoreline final'))
        code=code.replace('CONTINUATION_FUNCTIONS','\n'.join(definition(source,marker,marker.endswith(' final')) for marker in (
            '            static void prepare_planetary_wet_transition(',
            '            [[nodiscard]] static std::uint64_t planetary_cut_mesh_bytes(',
            '            static void report_planetary_wet_batch(',
            '            struct ShorelineBatchTiming final',
            '            static void prepare_planetary_wet_batch(',
            '            [[nodiscard]] static bool shoreline_inputs_are_current(',
            '            [[nodiscard]] static project_planetary::StreamResult continue_planetary_wet_cut(')))
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
