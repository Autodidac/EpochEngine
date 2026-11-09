"""Actual completion admission expressions and real moving-view policy; no GPU."""
from pathlib import Path
from Tests.platform.test_child_close_router import definition
from Tests.support.cpp import compile_and_run
import hashlib
import json
import os
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


def condition(text, marker):
    start = text.index(marker) + len(marker)
    depth, end = 1, start
    while depth:
        depth += (text[end] == '(') - (text[end] == ')')
        end += 1
    return text[start:end - 1]


class PlanetaryStreamCompletion(unittest.TestCase):
    def test_existing_request_gate_admits_forest_terrain_paint_only(self):
        path=ROOT/'Engine/src/editor/application/editor.application_packages_creation.inl'
        source=path.read_text(encoding='utf-8')
        pump=definition(source,'        void pump_planetary_stream(')
        marker='if (planetary_project_busy(editor)'
        guard='if (planetary_project_busy(editor)'+condition(pump,marker)+') return;'
        code=r'''#include <cstdint>
#include <optional>
#include <initializer_list>
enum class EditorMainSurface {Scene,ForestFactory,Game2D,Gui,Automation,AssetGeneration};
struct EditorState{
 EditorMainSurface mainSurface{EditorMainSurface::Scene};bool forestTerrainPainting{};
 std::optional<int>planetaryAttachmentPending,planetaryGeneratePending,planetaryStreamPending,
  planetaryFeatureReviewPending,planetaryFeatureAdmissionPending,waterAttachmentPending;
 bool planetaryStreamFailed{},projectBuildPending{},scriptBuildPending{},submitted{};
 std::uint64_t planetaryStreamNextTick{};
};
BUSY
void actual_request_gate(EditorState&editor){const std::uint64_t tick=1000;GUARD editor.submitted=true;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 EditorState scene;actual_request_gate(scene);CHECK(scene.submitted);
 EditorState paint;paint.mainSurface=EditorMainSurface::ForestFactory;paint.forestTerrainPainting=true;
 actual_request_gate(paint);CHECK(paint.submitted);
 auto preview=paint;preview.submitted=false;preview.forestTerrainPainting=false;actual_request_gate(preview);CHECK(!preview.submitted);
 for(const auto surface:{EditorMainSurface::Game2D,EditorMainSurface::Gui,EditorMainSurface::Automation,EditorMainSurface::AssetGeneration}){
  auto other=paint;other.submitted=false;other.mainSurface=surface;actual_request_gate(other);CHECK(!other.submitted);}
 for(unsigned reason=0;reason<10;++reason){auto blocked=paint;blocked.submitted=false;
  if(reason==0)blocked.planetaryAttachmentPending=1;if(reason==1)blocked.planetaryGeneratePending=1;
  if(reason==2)blocked.planetaryStreamPending=1;if(reason==3)blocked.planetaryFeatureReviewPending=1;
  if(reason==4)blocked.planetaryFeatureAdmissionPending=1;if(reason==5)blocked.waterAttachmentPending=1;
  if(reason==6)blocked.planetaryStreamFailed=true;if(reason==7)blocked.projectBuildPending=true;
  if(reason==8)blocked.scriptBuildPending=true;if(reason==9)blocked.planetaryStreamNextTick=1001;
  actual_request_gate(blocked);CHECK(!blocked.submitted);}
}
'''.replace('BUSY',definition(source,'        [[nodiscard]] bool planetary_project_busy(')).replace('GUARD',guard)
        proof=ROOT/'Artifacts/Reports/forest-paint-lod-gate-nov06';proof.mkdir(parents=True,exist_ok=True)
        cpp=proof/'consumer.cpp';cpp.write_text(code,encoding='utf-8')
        runs=compile_and_run(self,cpp,proof,windows_flags=('/W4','/WX'))
        (proof/'result.json').write_text(json.dumps({'passed':True,'scope':'Actual existing single-job request guard admits Scene and Forest terrain painting only; no provider/renderer launched',
            'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'runs':runs},indent=2)+'\n')

    def test_completed_provider_workers_and_time_are_request_bound(self):
        path=ROOT/'Engine/src/project/project.planetary.cpp'
        source=path.read_text(encoding='utf-8')
        start=source.index('            const auto performance=read_small(run/"generated/terrain.performance",4096u);')
        end=source.index('            // Advisory bounded evidence',start)
        block=source[start:end]
        code=r'''#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
std::string receipt;
std::string read_small(const std::filesystem::path&,unsigned){return receipt;}
std::string status(std::uint64_t request,double seconds){
 const std::filesystem::path run{"project"};
 struct {std::string message{"Planet LOD ready"};double generation_seconds;} operation{{},seconds};
 operation.message="Planet LOD ready";
 struct {std::uint64_t request_id;} view{request};
 BLOCK
 return operation.message;
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){
 receipt="epoch.planet.stream-performance.v1\nrequest_id=42\nworker_limit=4\npeak_active_workers=4\nworker_jobs_started=12\n";
 CHECK(status(42,.2054)=="Planet LOD ready\nLast LOD build: 4/4 workers, 205 ms provider");
 CHECK(status(43,.2054)=="Planet LOD ready");
 CHECK(status(42,std::numeric_limits<double>::infinity())=="Planet LOD ready\nLast LOD build: 4/4 workers");
 CHECK(status(42,-1)=="Planet LOD ready\nLast LOD build: 4/4 workers");
 CHECK(status(42,3601)=="Planet LOD ready\nLast LOD build: 4/4 workers");
 receipt+="worker_limit=4\n";CHECK(status(42,.2)=="Planet LOD ready");
 receipt="epoch.planet.stream-performance.v1\nrequest_id=42\nworker_limit=4\npeak_active_workers=5\nworker_jobs_started=12\n";
 CHECK(status(42,.2)=="Planet LOD ready");
 receipt="epoch.planet.stream-performance.v1\nrequest_id=42\nworker_limit=4\npeak_active_workers=0\nworker_jobs_started=12\n";
 CHECK(status(42,.2)=="Planet LOD ready");
 receipt="epoch.planet.stream-performance.v1\nrequest_id=42\nworker_limit=4\npeak_active_workers=0\nworker_jobs_started=0\n";
 CHECK(status(42,.0001)=="Planet LOD ready\nLast LOD build: 0/4 workers, 0 ms provider");
 receipt="epoch.planet.stream-performance.v1\nrequest_id=42\nworker_limit=4\npeak_active_workers=2\nworker_jobs_started=12";
 CHECK(status(42,.2)=="Planet LOD ready");
 receipt="epoch.planet.stream-performance.v2\nrequest_id=42\nworker_limit=4\npeak_active_workers=2\nworker_jobs_started=12\n";
 CHECK(status(42,.2)=="Planet LOD ready");
}
'''.replace('BLOCK',block)
        proof=ROOT/'Artifacts/Reports/planet-lod-worker-display-nov01'
        proof.mkdir(parents=True,exist_ok=True)
        cpp=proof/'consumer.cpp';cpp.write_text(code,encoding='utf-8')
        runs=compile_and_run(self,cpp,proof,windows_flags=('/W4','/WX'))
        (proof/'result.json').write_text(json.dumps({'passed':True,'scope':'Actual request-bound provider receipt parser and completed-work label; no graphics, no active-thread counts',
            'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'runs':runs},indent=2)+'\n')

    def test_actual_worker_exact_source_pre_and_post_admission(self):
        path=ROOT/'Engine/src/editor/application/editor.application_packages_creation.inl'
        source=path.read_text(encoding='utf-8')
        worker=definition(source,'[root, settings, view, source=editor.planetaryStreamSource,')
        pump=definition(source,'        void pump_planetary_stream(')
        # The only full disk/source verifier in this frame owner is inside its
        # submitted worker. No cached bool authorizes execution or publication.
        self.assertEqual(pump.count('project_planetary::mesh_generation_is_current('),worker.count('project_planetary::mesh_generation_is_current('))
        self.assertNotIn('editor_planetary_mesh_policy_current(',pump)
        code=r'''#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
namespace project_planetary {
struct Settings{int seed{7};};struct View{unsigned request_id{1};};
struct Source{std::string hash{"approved"};};struct MeshIdentity{std::string key{"saved"};};
struct StreamResult{struct Operation{bool ok{},cancelled{};std::string message;}operation;bool snapshot_ready{};std::shared_ptr<int> snapshot;};
unsigned verifies{},launches{};bool changedDuring{},providerFails{};std::string diskSource{"approved"},diskMesh{"saved"};
bool mesh_generation_is_current(const std::filesystem::path&,const Source& source,const Settings& settings,const MeshIdentity& mesh){
 ++verifies;return source.hash==diskSource&&mesh.key==diskMesh&&settings.seed==7;
}
StreamResult stream_step(const std::filesystem::path&,Settings,View,std::stop_token){
 ++launches;if(changedDuring)diskSource="changed";
 return {{!providerFails,false,{}},true,std::make_shared<int>(1)};
}
}
struct Editor{project_planetary::Source planetaryStreamSource;project_planetary::MeshIdentity planetaryStreamMesh;};
auto make_worker(Editor& editor){const std::filesystem::path root{"project"};const project_planetary::Settings settings;const project_planetary::View view;
 return WORKER;
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(false)
int main(){using namespace project_planetary;Editor editor;
 auto worker=make_worker(editor);auto result=worker({});CHECK(result.operation.ok&&result.snapshot&&verifies==2&&launches==1);
 verifies=launches=0;diskSource="changed";result=worker({});CHECK(!result.operation.ok&&!result.snapshot&&verifies==1&&launches==0);
 diskSource="approved";verifies=launches=0;changedDuring=true;result=worker({});
 CHECK(!result.operation.ok&&!result.snapshot_ready&&!result.snapshot&&verifies==2&&launches==1);
 changedDuring=false;diskSource="approved";verifies=launches=0;std::stop_source cancelled;cancelled.request_stop();result=worker(cancelled.get_token());
 CHECK(result.operation.cancelled&&!result.snapshot&&verifies==0&&launches==0);
 verifies=launches=0;providerFails=true;result=worker({});CHECK(!result.operation.ok&&verifies==1&&launches==1);providerFails=false;
 verifies=launches=0;diskMesh="changed";result=worker({});CHECK(!result.operation.ok&&verifies==1&&launches==0);diskMesh="saved";
 // Selection mutation cannot rewrite an already captured approved request.
 editor.planetaryStreamSource.hash="other";result=worker({});CHECK(result.operation.ok);
}
'''.replace('WORKER',worker)
        proof=ROOT/'Artifacts/Reports/terrain-selection-lod-oct29/worker-admission'
        proof.mkdir(parents=True,exist_ok=True)
        cpp=proof/'worker.cpp';cpp.write_text(code,encoding='utf-8')
        result=compile_and_run(self,cpp,proof)
        (proof/'result.json').write_text(json.dumps(result,indent=2)+'\n')

    def test_moving_view_and_actual_completion_guards(self):
        runtime_path = ROOT / 'Engine/src/epoch/implementation/epoch.project_play_scene.inl'
        editor_path = ROOT / 'Engine/src/editor/application/editor.application_packages_creation.inl'
        runtime, editor = runtime_path.read_text(), editor_path.read_text()
        outer = condition(runtime, 'if((!m_planetaryPaused||runtime_water_visibility_pending()) &&')
        outer = '(!m_planetaryPaused||runtime_water_visibility_pending()) &&' + outer
        successful = condition(runtime, 'if(result.operation.ok &&!result.operation.cancelled')
        successful = 'result.operation.ok &&!result.operation.cancelled' + successful
        source = condition(runtime, 'if(m_meshPlanetarySource\n')
        source = 'm_meshPlanetarySource\n' + source
        # The disk/source verifier is a recorded port. Preserve its actual guard
        # and call arguments; filesystem verification has its own owner consumer.
        source = source[:source.index('&&result.snapshot->vertices()')]
        source = source.replace('project_planetary::source_generation_is_current', 'source_is_current')
        begin = editor.index('const bool current = enabled &&', editor.index('void pump_planetary_stream('))
        current = editor[begin + len('const bool current = '):editor.index(';', begin)]
        success = condition(editor, 'else if (current && streamed.operation.ok &&')
        success = 'current && streamed.operation.ok &&' + success
        cancelled = condition(editor, 'if (current && (streamed.waiting_for_worker||streamed.operation.cancelled)')
        cancelled = 'current && (streamed.waiting_for_worker||streamed.operation.cancelled)' + cancelled
        source_editor = condition(editor, 'if (editor_planetary_stream_identity_matches(editor,*entityAt)')
        source_editor = 'editor_planetary_stream_identity_matches(editor,*entityAt)' + source_editor
        source_editor = source_editor[:source_editor.index('&&streamed.snapshot->vertices()')]
        source_editor = source_editor.replace('project_planetary::mesh_generation_is_current', 'generation_is_current')
        stationary = definition(runtime, '            [[nodiscard]] static bool planetary_view_changed(')
        time_reusable = definition(editor, '        [[nodiscard]] bool planetary_preview_time_reusable(')
        def statement(text, marker):
            start = text.index(marker)
            return text[start:text.index(';', start) + 1]
        editor_backoff = statement(editor, "editor.planetaryStreamNextTick = tick + 250'000'000ull")
        editor_ready = statement(editor, 'if (!editor.planetaryStreamFailed && (streamed.pending != 0u || changed))')
        runtime_backoff = statement(runtime, 'm_planetaryNextTick=now+std::chrono::milliseconds{250}')
        runtime_ready = statement(runtime, 'if(!m_planetaryFailed&&(result.pending!=0u||planetary_view_changed(view,m_planetaryView)))')
        runtime_retarget = statement(runtime, 'if(result.operation.ok&&!result.operation.cancelled')
        editor_retarget = definition(editor, 'else if (enabled && viewValid && streamed.operation.ok && !streamed.operation.cancelled')
        expressions = dict(runtime_outer=outer, runtime_success=successful, runtime_source=source,
                           editor_current=current, editor_success=success,
                           editor_cancelled=cancelled, editor_source=source_editor)
        proof = ROOT / 'Artifacts/Reports/planet-moving-completion-oct27'
        build = ROOT / 'Artifacts/Intermediate/cmake/planet-moving-completion'
        proof.mkdir(parents=True, exist_ok=True)
        code = r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
import project.planetary;
import planet.streaming;
namespace project_planetary=epochengine::project_planetary;
namespace planet=project_planetary;
namespace stream=epoch::extensions::planetary_terrain::streaming;
namespace epochengine::timeline {
enum class PlaybackDirection {reverse=-1,forward=1};
struct TimelineState {double playhead_seconds{},playback_rate{1.0};bool playing{};PlaybackDirection direction{PlaybackDirection::forward};};
}
TIME_REUSABLE
#define CHECK(x) do {if(!(x)) return __LINE__;}while(false)
struct Source {bool current{true};std::filesystem::path project_root{"project"};std::string project_manifest{"manifest"},content_hash{"hash"};};
bool source_is_current(const std::filesystem::path&,const Source& value){return value.current;}
std::filesystem::path resolve_editor_path(const std::filesystem::path& value){return value;}
bool generation_is_current(const std::filesystem::path&,const Source& value,const planet::Settings&,const planet::MeshGenerationIdentity&){return value.current;}
struct MeshRef {std::string artifact_key{"fixed"};};
struct Entity {std::uint64_t sceneObjectId{7};std::array<float,3> position{},rotation{},scale{1,1,1};MeshRef* meshAsset{};};
struct Lifecycle {
 struct Evidence {struct {struct {std::uint64_t generation{4};} selected_project;} generations;};
 Evidence evidence()const{return {};}
};
struct Editor {
 std::string planetaryOperationRoot{"project"},projectRoot{"project"},planetaryOperationProjectKey{"identity"};
 std::uint64_t planetaryOperationProjectGeneration{4},planetaryStreamEntity{7},planetaryStreamSequence{1};
 Lifecycle projectLifecycle;
 std::array<float,3> planetaryStreamPosition{},planetaryStreamRotation{},planetaryStreamScale{1,1,1};
 planet::Settings planetaryStreamSettings;
 Source planetaryStreamSource;
 planet::MeshGenerationIdentity planetaryStreamMesh;
 std::string planetaryStreamFallbackKey{"fixed"};
 planet::StreamView planetaryStreamView;
 epochengine::timeline::TimelineState timelineState;
 bool planetaryStreamTimelinePlaying{};
 epochengine::timeline::PlaybackDirection planetaryStreamTimelineDirection{epochengine::timeline::PlaybackDirection::forward};
 double planetaryStreamTimelineRate{1.0};
 bool source_current{true};
 bool identity_current{true};
 struct Policy {Source source;std::optional<planet::MeshGenerationIdentity> generated_mesh{planet::MeshGenerationIdentity{}};};
 std::optional<Policy> planetaryMeshPolicy{Policy{}};
 bool planetaryStreamFailed{};
 bool planetaryStreamContinue{};
 std::uint64_t planetaryStreamNextTick{};
};
std::string planetary_project_key(const Editor&){return "identity";}
bool editor_planetary_mesh_policy_current(const Editor& editor){return editor.source_current;}
bool editor_planetary_stream_identity_matches(const Editor& editor,const Entity&){return editor.identity_current&&editor.planetaryMeshPolicy&&editor.planetaryMeshPolicy->generated_mesh.has_value();}
struct Fixture {
 bool m_planetaryPaused{},m_explorationController{true};
 bool runtime_water_visibility_pending()const{return false;} // normal unchanged visibility lane
 std::uint32_t m_planetaryRequestBody{};
 struct {std::uint32_t anchor{};}m_explorationFrame;
 std::optional<planet::Settings> m_planetarySettings{planet::Settings{}},m_planetaryRequestSettings{planet::Settings{}};
 planet::StreamView m_planetaryView,view;
 std::uint64_t m_planetarySequence{1};
 planet::StreamResult result;
 std::optional<Source> m_meshPlanetarySource{Source{}};
 std::string m_projectRoot{"project"};
 Editor editor;
 MeshRef mesh;
 Entity entity{.meshAsset=&mesh};
 bool enabled{true},viewValid{true},changed{true};
 planet::StreamView desiredView;
 planet::Settings actualSettings;
 planet::StreamResult streamed;
 bool m_planetaryFailed{};
 std::chrono::steady_clock::time_point m_planetaryNextTick;
 Fixture(){m_planetaryView.camera={0,0,10.00001};m_planetaryView.request_id=1;
  view=m_planetaryView;view.camera[0]+=0.0001;view.request_id=0;
  result.operation.ok=true;result.request_id=1;
  result.verified_source=planet::SourceGeneration{"project","manifest","hash"};
  editor.planetaryStreamView=m_planetaryView;desiredView=view;
  streamed=result;}
 STATIONARY
 bool runtime_accepts(){return (RUNTIME_OUTER)&&(RUNTIME_SUCCESS)&&(RUNTIME_SOURCE);}
 bool editor_accepts(){const auto* entityAt=&entity;const auto* admittedSettings=&actualSettings;
  const bool current=(EDITOR_CURRENT);
  return !(EDITOR_CANCELLED)&&(EDITOR_SUCCESS)&&(EDITOR_SOURCE);}
 std::uint64_t editor_deadline(){const std::uint64_t tick=1'000'000'000;
  EDITOR_BACKOFF
  if(editor_accepts()){EDITOR_READY}
  EDITOR_RETARGET
  return editor.planetaryStreamNextTick/1'000'000;
 }
 std::uint64_t runtime_deadline(){const auto now=std::chrono::steady_clock::time_point{std::chrono::milliseconds{1000}};
  RUNTIME_BACKOFF
  if(runtime_accepts()){RUNTIME_READY}
  else if(!m_planetaryPaused){RUNTIME_RETARGET}
  return std::chrono::duration_cast<std::chrono::milliseconds>(m_planetaryNextTick.time_since_epoch()).count();
 }
};
int main(){
 // Exercise the real provider cut, including incomplete cold roots and
 // parent fallback during refinement. Its six-face coverage is independent
 // of which valid camera consumes a completed snapshot.
 stream::State coverage;stream::Request coverageRequest;
 coverageRequest.cells_per_tile=2;coverageRequest.camera={0,0,26.5};
 coverageRequest.lod.max_level=0;coverageRequest.lod.max_leaves=6;
 CHECK(stream::update(coverage,coverageRequest)==stream::Status::ready);
 for(unsigned root=0;root<6u;++root){CHECK(stream::draw_cut(coverage).empty());
  const auto work=stream::next_work(coverage);CHECK(work);
  CHECK(stream::complete(coverage,*work,stream::build(coverage,*work))==stream::Status::ready);}
 const auto all_faces=[&](){std::array<double,6> areas{};
  const auto cut=stream::draw_cut(coverage);if(cut.empty())return false;
  for(const auto key:cut){if(key.face>=6u)return false;areas[key.face]+=std::ldexp(1.0,-2*int(key.level));}
  return std::ranges::all_of(areas,[](double area){return std::abs(area-1.0)<1e-12;});};
 CHECK(all_faces()&&stream::stats(coverage).complete_coverage);
 coverageRequest.camera={0,0,10.001};coverageRequest.lod.max_level=8;coverageRequest.lod.max_leaves=96;
 CHECK(stream::update(coverage,coverageRequest)==stream::Status::ready&&all_faces());
 for(unsigned patch=0;patch<32u;++patch){const auto work=stream::next_work(coverage);if(!work)break;
  CHECK(stream::complete(coverage,*work,stream::build(coverage,*work))==stream::Status::ready);CHECK(all_faces());}
 Fixture good;CHECK(good.runtime_accepts()&&good.editor_accepts());
 CHECK(good.editor_deadline()==1000&&good.runtime_deadline()==1000);
 // A moving camera gets its next cut immediately even if the old cut finished.
 // A stationary, complete cut keeps the existing contention backoff.
 {Fixture f;f.changed=false;f.desiredView=f.editor.planetaryStreamView;f.view=f.m_planetaryView;
  CHECK(f.editor_deadline()==1250&&f.runtime_deadline()==1250);}
 {Fixture f;f.result.pending=f.streamed.pending=32;
  CHECK(f.editor_deadline()==1000&&f.runtime_deadline()==1000);
  f.editor.planetaryStreamFailed=f.m_planetaryFailed=true;
  CHECK(f.editor_deadline()==1250&&f.runtime_deadline()==1250);}
 {Fixture f;f.result.pending=f.streamed.pending=32;f.result.operation.cancelled=f.streamed.operation.cancelled=true;
  CHECK(f.editor_deadline()==1250&&f.runtime_deadline()==1250);}
 {Fixture f;f.result.pending=f.streamed.pending=32;f.result.request_id=f.streamed.request_id=2;
  CHECK(f.editor_deadline()==1250&&f.runtime_deadline()==1250);}
 // An incompatible-FOV output is refused, but the successful retired step must not add
 // another250ms before requesting the current camera. Failures keep backoff.
 {Fixture f;f.view.vertical_fov+=0.01;f.desiredView=f.view;
  CHECK(!f.runtime_accepts()&&!f.editor_accepts());
  CHECK(f.runtime_deadline()==1000&&f.editor_deadline()==1000);
  f.result.operation.ok=f.streamed.operation.ok=false;
  CHECK(f.runtime_deadline()==1250&&f.editor_deadline()==1250);}
 {Fixture f;f.view.vertical_fov+=0.01;f.desiredView=f.view;
  f.result.operation.cancelled=f.streamed.operation.cancelled=true;
  CHECK(f.runtime_deadline()==1250&&f.editor_deadline()==1250);}
 {Fixture f;f.view.vertical_fov+=0.01;f.desiredView=f.view;
  f.m_planetaryPaused=true;f.enabled=false;
  CHECK(f.runtime_deadline()==1250&&f.editor_deadline()==1250);}
 CHECK(Fixture::planetary_view_changed(good.view,good.m_planetaryView));
 // Continuous movement crosses the former stationary threshold every step.
 // Each bounded completed cut remains useful, then the next request advances.
 unsigned published{};
 for(unsigned step=0;step<200;++step){Fixture moving;
  moving.m_planetaryView.camera[0]=step*0.0001;moving.view=moving.m_planetaryView;
  moving.view.camera[0]+=0.0001;moving.view.request_id=0;
  moving.editor.planetaryStreamView=moving.m_planetaryView;moving.desiredView=moving.view;
  CHECK(moving.runtime_accepts()&&moving.editor_accepts());++published;}
 CHECK(published==200);
 // Orbital movement exceeded the old .25-radius reuse ceiling every worker
 // completion, so all200 safe cuts were previously discarded until landing.
 for(unsigned step=0;step<200;++step){Fixture flying;
  flying.m_planetaryView.camera={double(step),0,26.5};flying.view=flying.m_planetaryView;
  flying.view.camera[0]+=1.0;flying.editor.planetaryStreamView=flying.m_planetaryView;
  flying.desiredView=flying.view;flying.result.pending=flying.streamed.pending=32;
  CHECK(flying.runtime_accepts()&&flying.editor_accepts());
  CHECK(flying.runtime_deadline()==1000&&flying.editor_deadline()==1000);}
 {Fixture f;f.m_planetaryRequestBody=1;CHECK(!f.runtime_accepts());}
 {Fixture f;f.m_explorationFrame.anchor=1;CHECK(!f.runtime_accepts());}
 {Fixture f;f.m_planetarySettings->seed++;f.actualSettings.seed++;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.m_meshPlanetarySource->content_hash="changed";f.editor.planetaryMeshPolicy->source.content_hash="changed";CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.m_meshPlanetarySource.reset();CHECK(!f.runtime_accepts());}
 {Fixture f;f.result.verified_source.reset();CHECK(!f.runtime_accepts());}
 {Fixture f;f.result.verified_source->project_root="foreign";CHECK(!f.runtime_accepts());}
 {Fixture f;f.result.verified_source->project_manifest="changed";CHECK(!f.runtime_accepts());}
 {Fixture f;f.result.verified_source->content_hash="changed";CHECK(!f.runtime_accepts());}
 {Fixture f;f.editor.identity_current=false;CHECK(!f.editor_accepts());}
 {Fixture f;f.editor.planetaryMeshPolicy.reset();CHECK(!f.editor_accepts());}
 {Fixture f;f.editor.planetaryMeshPolicy->generated_mesh.reset();CHECK(!f.editor_accepts());}
 {Fixture f;f.streamed.operation.ok=false;CHECK(!f.editor_accepts());}
 {Fixture f;f.editor.planetaryMeshPolicy->source.project_root="foreign";CHECK(!f.editor_accepts());}
 {Fixture f;f.editor.planetaryMeshPolicy->source.project_manifest="changed";CHECK(!f.editor_accepts());}
 {Fixture f;f.editor.planetaryStreamMesh.vertex_count++;CHECK(!f.editor_accepts());}
 {Fixture f;f.m_planetaryPaused=true;f.enabled=false;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.result.operation.cancelled=true;f.streamed.operation.cancelled=true;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.result.request_id=2;f.streamed.request_id=2;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.m_planetarySequence=2;f.editor.planetaryStreamSequence=2;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.editor.projectRoot="foreign";CHECK(!f.editor_accepts());}
 {Fixture f;f.editor.planetaryOperationProjectKey="foreign";CHECK(!f.editor_accepts());}
 {Fixture f;f.editor.planetaryOperationProjectGeneration++;CHECK(!f.editor_accepts());}
 {Fixture f;f.entity.sceneObjectId++;CHECK(!f.editor_accepts());}
 {Fixture f;f.entity.position[0]=1;CHECK(!f.editor_accepts());}
 {Fixture f;f.entity.rotation[0]=1;CHECK(!f.editor_accepts());}
 {Fixture f;f.entity.scale[0]=2;CHECK(!f.editor_accepts());}
 {Fixture f;f.mesh.artifact_key="replaced";CHECK(!f.editor_accepts());}
 {Fixture f;f.view.camera[0]=0.01;f.desiredView=f.view;CHECK(f.runtime_accepts()&&f.editor_accepts());}
 {Fixture f;f.view.viewport_height++;f.desiredView=f.view;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.view.vertical_fov+=0.01;f.desiredView=f.view;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.view.camera[0]=std::numeric_limits<double>::quiet_NaN();f.desiredView=f.view;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 {Fixture f;f.m_planetaryView.camera[2]=std::numeric_limits<double>::infinity();f.editor.planetaryStreamView=f.m_planetaryView;CHECK(!f.runtime_accepts()&&!f.editor_accepts());}
 for(auto bad:{0.0,0.09,3.01,std::numeric_limits<double>::infinity()}){auto a=good.view;a.vertical_fov=bad;CHECK(!planet::stream_view_reusable(a,good.m_planetaryView));}
 for(unsigned bad:{0u,63u,16385u}){auto a=good.view;a.viewport_height=bad;CHECK(!planet::stream_view_reusable(a,good.m_planetaryView));}
 for(auto bad:{-1.0,1e12,std::numeric_limits<double>::quiet_NaN()}){auto a=good.view;a.simulation_seconds=bad;CHECK(!planet::stream_view_reusable(a,good.m_planetaryView));}
 {auto a=good.view,b=good.m_planetaryView;a.simulation_seconds=10;b.simulation_seconds=0;
  CHECK(planet::stream_view_reusable(a,b));} // Body-local geometry is time independent.
 {auto a=good.view,b=good.m_planetaryView;b.camera={0,0,12};a.camera={0.2,0,12};CHECK(planet::stream_view_reusable(a,b));
  a.camera[0]=0.251;CHECK(planet::stream_view_reusable(a,b));}
 {auto a=good.view,b=good.m_planetaryView;b.camera={1e10,1e10,1e10};a.camera=b.camera;
  CHECK(planet::stream_view_reusable(a,b));a.camera[0]+=1;CHECK(!planet::stream_view_reusable(a,b));}
 return 0;
}
'''
        code = code.replace('STATIONARY', stationary).replace('TIME_REUSABLE',time_reusable)
        for key, value in dict(editor_backoff=editor_backoff, editor_ready=editor_ready,
                               runtime_backoff=runtime_backoff, runtime_ready=runtime_ready,
                               editor_retarget=editor_retarget, runtime_retarget=runtime_retarget).items():
            code = code.replace(key.upper(), value)
        for key, value in expressions.items():
            code = code.replace(key.upper(), value)
        (proof / 'completion.contract.cpp').write_text(code, encoding='utf-8')
        modules = ['project.planetary', 'project.planetary_snapshot', 'asset.mesh_artifact', 'asset.texture_artifact', 'asset.cloud_volume', 'water.wave']
        files = ' '.join('"' + (ROOT / 'Engine/modules' / (name + '.ixx')).as_posix() + '"' for name in modules)
        (proof / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
project(PlanetMovingCompletion LANGUAGES CXX)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("{(ROOT/'Engine/dep/EpochEngineExtensions/packages/planetary_terrain').as_posix()}" provider)
add_executable(completion completion.contract.cpp)
target_link_libraries(completion PRIVATE EpochEngineExtensions::PlanetaryCore)
target_compile_features(completion PRIVATE cxx_std_23)
target_sources(completion PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(ROOT/'Engine/modules').as_posix()}" FILES {files})
if(MSVC)
 target_compile_options(completion PRIVATE /utf-8 /permissive- /EHsc)
endif()
enable_testing()
add_test(NAME moving_completion COMMAND completion)
''', encoding='utf-8')
        configure = ['cmake', '-S', str(proof), '-B', str(build)]
        configure += (['-G', 'Visual Studio 17 2022', '-A', 'x64',
                       '-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false']
                      if os.name == 'nt' else ['-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release'])
        commands = [('configure', configure)]
        for config in (('Debug', 'Release') if os.name == 'nt' else ('Release',)):
            commands += [(config + '-build', ['cmake', '--build', str(build), '--config', config, '--parallel', '1', '--target', 'completion']
                         + (['--', '/nr:false'] if os.name == 'nt' else [])),
                         (config + '-test', ['ctest', '--test-dir', str(build), '-C', config, '--output-on-failure'])]
        results = []
        for phase, command in commands:
            run = subprocess.run(command, cwd=proof, capture_output=True, text=True, timeout=120,
                                 env=dict(os.environ, MSBUILDDISABLENODEREUSE='1'))
            (proof / (phase + '.log')).write_text(run.stdout + run.stderr, encoding='utf-8')
            results.append(dict(phase=phase, exit=run.returncode, command=command))
            (proof / 'result.json').write_text(json.dumps(dict(
                scope='Actual completion admission expressions, recorded identity/source-verifier ports, real module view policy; no renderer/process execution',
                source={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in [runtime_path, editor_path, ROOT/'Engine/modules/project.planetary.ixx']},
                expressions=expressions, phases=results, native=False), indent=2) + '\n', encoding='utf-8')
            self.assertEqual(run.returncode, 0, (run.stdout + run.stderr)[-10000:])


if __name__ == '__main__':
    unittest.main()
