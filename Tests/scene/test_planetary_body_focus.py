"""Actual Editor body focus/generation framing in orbit and selected FPS controls; no GUI."""
import os, unittest

class PlanetaryBodyFocus(unittest.TestCase):
    def test_actual_body_and_generation_framing(self):
        if os.name != "nt":
            self.skipTest("Installed MSVC actual-module lane")
        from pathlib import Path
        import json, re, subprocess, sys
        ROOT=Path(__file__).resolve().parents[2]
        sys.path.insert(0,str(ROOT))
        from Tests.packages.test_planetary_mesh_admission import owner_function
        from Tests.build.test_opengl_sprite_stream import definition
        P=ROOT/'Artifacts/Reports/planetary-body-focus78-nov07/regression';P.mkdir(parents=True,exist_ok=True)
        src=(ROOT/'Engine/src/editor/application/editor.application_asset_browsing.inl').read_text()
        functions='\n'.join(owner_function(src,m)for m in (
         '[[nodiscard]] std::optional<project_planetary::MeshGenerationIdentity>',
         '[[nodiscard]] bool editor_planetary_stream_identity_matches(',
         '[[nodiscard]] bool select_editor_planetary_body(',
         '[[nodiscard]] std::optional<scene_interaction::FocusTarget> editor_planetary_body_focus('))
        helper=owner_function((ROOT/'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(),'        [[nodiscard]] bool frame_editor_scene_target(')
        generation=definition((ROOT/'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(),'                if(const auto body=editor_planetary_body_focus(state,*planet))')
        dispatch=owner_function((ROOT/'Engine/src/editor/application/editor.application_scripting_renderers.inl').read_text(),'            if (toolId == "focus_selection")')
        dispatch=dispatch[dispatch.index('{')+1:dispatch.index('#if defined(_WIN32)')]
        code=r'''
        #include <algorithm>
        #include <array>
        #include <cmath>
        #include <cstdint>
        #include <cstdio>
        #include <filesystem>
        #include <optional>
        #include <string>
        #include <string_view>
        #include <vector>
        import scene.snapshot;
        import scene.interaction;
        import project.planetary;
        import render.ray;
        import render.preview_grid;
        namespace probe {
        namespace scene=epochengine::scene;namespace scene_interaction=epochengine::scene_interaction;
        namespace ray=epochengine::ray;
        namespace previewgrid=epochengine::previewgrid;
        namespace project_planetary {
        using namespace epochengine::project_planetary;
        bool current=true;
        bool source_generation_is_current(const std::filesystem::path&,const SourceGeneration&){return current;}
        }
        enum class EditorMainSurface{Scene,ForestFactory};
        struct EditorEntity{std::string name="Planet",category="Planetary Terrain";std::uint64_t sceneObjectId=1;
        std::array<float,3>position{},rotation{},scale{1,1,1};
        std::optional<std::array<double,3>>positionAnchor;
        std::optional<scene::SceneMeshSnapshot>meshAsset;
        struct Region{std::uint64_t terrain_object_id{};};std::optional<Region>forestRegion;};
        struct EditorState{
        std::string projectRoot="project";std::uint64_t selectedEntityId{};std::vector<EditorEntity>entities;
        struct Attachment{std::string content_hash="source";bool source_attached()const{return true;}}planetaryAttachment;
        struct Policy{std::string selected_root="project";project_planetary::SourceGeneration source;
        project_planetary::Settings settings;std::optional<project_planetary::MeshGenerationIdentity>generated_mesh;
        std::vector<project_planetary::GeneratedBody>generated_bodies;};
        std::optional<Policy>planetaryMeshPolicy;
        struct PlanetaryBodySelection{std::uint64_t entity{};std::array<std::uint64_t,4>mesh_key{};std::uint32_t body{};};
        std::optional<PlanetaryBodySelection>planetaryBodySelection;
        EditorMainSurface mainSurface{EditorMainSurface::Scene};bool forestTerrainPainting{};
        scene_interaction::SceneInteractionQuery sceneInteraction;std::array<double,3>sceneInteractionOrigin{};
        std::vector<std::string>logs;
        };
        std::filesystem::path resolve_editor_path(const std::filesystem::path&p){return p;}
        void select_editor_entity(EditorState&e,std::size_t i){e.selectedEntityId=e.entities.at(i).sceneObjectId;}
        FUNCTIONS
        HELPER
        bool editor_has_selection(const EditorState&e){return e.selectedEntityId!=0;}
        void push_editor_log(EditorState&e,const std::string&s){e.logs.push_back(s);}
        std::optional<std::size_t>editor_entity_index(const EditorState&e,std::uint64_t id){for(std::size_t i=0;i<e.entities.size();++i)if(e.entities[i].sceneObjectId==id)return i;return {};}
        bool ensure_editor_scene_interaction(EditorState&){return true;}
        bool ensure_editor_forest_focus_mesh(EditorState&,const EditorEntity&){return true;}
        bool focus_forest_paint_region(const void*,const EditorEntity&,const EditorEntity&){return false;}
        void actual_focus(EditorState&state,const void*ctx){DISPATCH (void)focused;}
        void actual_generation_frame(EditorState&state,const void*ctx){const auto*planet=&state.entities[0];GENERATION}
        }
        int main(int argc,char**argv){using namespace probe;using namespace epochengine::previewgrid;
        bool anchored=argc>1&&std::string_view(argv[1])=="anchor";
        bool generation=argc>1&&std::string_view(argv[1])=="generation";
        bool fps=generation||(argc>1&&std::string_view(argv[1])=="fps");
        EditorState e;e.entities.emplace_back();auto&obj=e.entities[0];
        obj.position={3421880.25f,0,6306490};
        obj.meshAsset=scene::SceneMeshSnapshot{"Assets/Mesh/planet.epoch_mesh",{1,2,3,4},5828,18048,
        scene::SceneMeshCoordinates{637100,{1025656447.6465962,51685790.584686734,1291455547.2228227}}};
        if(anchored)obj.positionAnchor=std::array<double,3>{1e9+.125,-2e9+.25,3e9+.5};
        e.planetaryMeshPolicy.emplace();auto&p=*e.planetaryMeshPolicy;p.source.content_hash="source";
        p.generated_mesh=editor_planetary_mesh_identity(obj);
        p.generated_bodies={{.id=0,.name="VERDANT",.minimum={-10.000000953674316f,-10.002590179443359f,-10.006898880004883f},.maximum={10.000000953674316f,10.000000953674316f,10.000000953674316f}}};
        int context{};if(fps)set_camera_mode(&context,CameraMode::FPS);
        for(const auto mode:{"planet","universe"})for(unsigned repeat=0;repeat<2;++repeat){
        p.settings.mode=mode;if(!select_editor_planetary_body(e,0,0))return 1;
        const auto body=editor_planetary_body_focus(e,obj);if(!body)return 2;
        if(generation)actual_generation_frame(e,&context);else actual_focus(e,&context);const auto view=capture_camera_rig_snapshot(&context);
        const std::array<double,3> expected{double(obj.position[0])+(anchored?(*obj.positionAnchor)[0]:0),
        double(obj.position[1])+(anchored?(*obj.positionAnchor)[1]:0)-824.5,
        double(obj.position[2])+(anchored?(*obj.positionAnchor)[2]:0)-2197.5};
        const auto absolute=absolute_camera_target_for(&context);double error{};
        for(unsigned a=0;a<3;++a)error=(std::max)(error,std::abs(absolute[a]-expected[a]));
        std::fprintf(stderr,"mode=%s repeat=%u anchor=%u focus={%.9f,%.9f,%.9f} radius=%.9f distance=%.9f error=%.9f\n",mode,repeat,anchored,absolute[0],absolute[1],absolute[2],body->radius,camera_distance_for(&context),error);
        const auto camera=camera_for(&context);const auto eye=camera_world_eye(camera);double eyeDistance{};
        for(unsigned a=0;a<3;++a)eyeDistance+=(eye[a]-expected[a])*(eye[a]-expected[a]);
        std::fprintf(stderr,"fps=%u actual_mode=%u eye_distance=%.9f\n",fps,unsigned(view.mode),std::sqrt(eyeDistance));
        if((!fps&&error>2)||std::sqrt(eyeDistance)<3e7)return 3;
        if(fps&&view.mode!=CameraMode::FPS)return 4;
        obj.position[0]+=1024;obj.rotation={0,0,0};
        }
        }
        '''.replace('FUNCTIONS',functions).replace('HELPER',helper).replace('DISPATCH',dispatch).replace('GENERATION',generation)
        # Camera target uses the actual rig's origin plus local focus.
        code=code.replace('absolute_camera_target_for(&context)','std::array<double,3>{view.navigation_origin[0]+view.focus.x,view.navigation_origin[1]+view.focus.y,view.navigation_origin[2]+view.focus.z}')
        (P/'consumer.cpp').write_text(code)
        owners=(ROOT/'Engine/modules',ROOT/'Engine/dep/EpochPlatformEngine/modules')
        modules={}
        for owner in owners:
         for path in owner.rglob('*.ixx'):
          m=re.search(r'^export module\s+([\w.:]+);',path.read_text(encoding='utf-8-sig'),re.M)
          if m:modules[m[1]]=path
        pending=re.findall(r'^\s*import\s+([\w.:]+);',code,re.M)+['authoring.texture_detail'];closure=set()
        while pending:
         name=pending.pop()
         if name in closure:continue
         closure.add(name);pending+=re.findall(r'^(?:export\s+)?import\s+([\w.:]+);',modules[name].read_text(encoding='utf-8-sig'),re.M)
        bases=' '.join('"'+p.as_posix()+'"'for p in owners)
        files=' '.join('"'+modules[n].as_posix()+'"'for n in sorted(closure))
        (P/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
        project(PlanetBodyFocus LANGUAGES CXX)
        add_executable(focus consumer.cpp "{(ROOT/'Engine/src/render/render.preview_camera.cpp').as_posix()}" "{(ROOT/'Engine/src/render/render.preview_lighting.cpp').as_posix()}" "{(ROOT/'Engine/src/assets/texture/asset.texture_artifact.cpp').as_posix()}" "{(ROOT/'Engine/src/authoring/texture/authoring.texture_document.cpp').as_posix()}" "{(ROOT/'Engine/src/authoring/texture/authoring.texture_residency.cpp').as_posix()}")
        target_compile_features(focus PRIVATE cxx_std_23)
        target_compile_options(focus PRIVATE /EHsc /utf-8)
        target_sources(focus PRIVATE FILE_SET CXX_MODULES BASE_DIRS {bases} FILES {files})
        target_include_directories(focus PRIVATE "{(ROOT/'Engine/include').as_posix()}")
        ''')
        results=[]
        commands=[['cmake','-S',str(P),'-B',str(P/'cmake'),'-G','Visual Studio 17 2022','-A','x64']]
        for config in ('Debug','Release'):
         commands.append(['cmake','--build',str(P/'cmake'),'--config',config,'--parallel','1','--','/nr:false','/p:UseMultiToolTask=false','/p:CL_MPCount=1'])
        for i,c in enumerate(commands):
         r=subprocess.run(c,cwd=ROOT,capture_output=True,text=True,timeout=180,env=dict(os.environ,MSBUILDDISABLENODEREUSE='1'))
         (P/f'phase-{i}.log').write_text(r.stdout+r.stderr);results.append({'command':c,'exit':r.returncode})
         self.assertEqual(r.returncode,0,(r.stdout+r.stderr)[-7000:])
        for config in ('Debug','Release'):
         for case in ('plain','fps','generation'):
          r=subprocess.run([str(P/f'cmake/{config}/focus.exe'),case],cwd=P,capture_output=True,text=True,timeout=20)
          (P/f'{config}-{case}.log').write_text(r.stdout+r.stderr);results.append({'config':config,'case':case,'exit':r.returncode,'output':r.stdout+r.stderr})
          self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        (P/'result.json').write_text(json.dumps({'passed':True,'scope':'Actual select/body-bounds/focus dispatch/generation callback and camera modules; recording scene readiness and source-current ports; CPU only, not native acceptance.','results':results},indent=2))

if __name__ == "__main__":
    unittest.main()
