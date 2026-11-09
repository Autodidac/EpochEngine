"""Actual cloud packet/time authority and native precision; no GPU/model launch."""
from pathlib import Path
import json,os,re,subprocess,time,unittest
from Tests.build.test_opengl_sprite_stream import definition
ROOT=Path(__file__).resolve().parents[2]

class CloudVolumeRendering(unittest.TestCase):
    def test_actual_packets_time_rebase_and_native_precision(self):
        if os.name != 'nt':self.skipTest('Installed MSVC module consumer lane')
        repo=ROOT;root=ROOT
        report=ROOT/'Artifacts/Reports/cloud-volume-rendering/packets';report.mkdir(parents=True,exist_ok=True)
        native=(root/'Engine/modules/opengl.cloud_volume.ixx').read_text(encoding='utf-8')
        packed=definition(native,'    struct PackedVolume final',True)+'\n'+definition(native,'    [[nodiscard]] inline std::optional<PackedVolume> pack_volume(')
        depth=re.search(r'float viewDepth\(float z\)\{[^}]+\}',native).group(0).replace('1.0','1.0f')
        density_owner='\n'.join(definition(native,marker) for marker in (
            'float filteredNoise(', 'float cloudDensity('))
        # Execute the actual GLSL density/filter expressions with float literal
        # semantics; the noise oracle is the already shared CPU lattice owner.
        density_owner=re.sub(r'(?<![\w.])(\d+\.\d+)(?![\w.])',r'\1f',density_owner)
        code=r'''
        #include <algorithm>
        #include <array>
        #include <cmath>
        #include <limits>
        #include <memory>
        #include <optional>
        #include <span>
        #include <vector>
        import asset.cloud_volume;
        import render.preview_grid;
        import render.lighting;
        namespace epochengine::openglcloud {
        PACKED_OWNER
        }
        #define CHECK(value) do {if(!(value))return __LINE__;}while(false)
        namespace depth_shader {
        struct {float x{},y{};}uNearFar;
        bool uOrthographic{};
        float mix(float a,float b,float t){return a*(1.0f-t)+b*t;}
        DEPTH_OWNER
        }
        namespace density_shader {
        using uint=unsigned;
        struct vec3{float x{},y{},z{};};
        vec3 operator*(vec3 v,float s){return {v.x*s,v.y*s,v.z*s};}
        vec3 operator/(vec3 v,float s){return {v.x/s,v.y/s,v.z/s};}
        vec3 operator+(vec3 v,float s){return {v.x+s,v.y+s,v.z+s};}
        struct Matrix{}uWorldToBody;
        vec3 operator*(Matrix,vec3 v){return v;}
        struct{float x{},y{};}uWind;
        float uBase{},uThickness{},uNoiseScale{},uCoverage{};
        unsigned uSeed{};
        float length(vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
        float clamp(float v,float a,float b){return std::clamp(v,a,b);}
        float mix(float a,float b,float v){return a+(b-a)*v;}
        float smoothstep(float a,float b,float v){v=clamp((v-a)/(b-a),0.0f,1.0f);return v*v*(3.0f-2.0f*v);}
        float noiseValue(vec3 v,unsigned seed){return static_cast<float>(epochengine::asset::cloud_volume::detail::noise({v.x,v.y,v.z},seed));}
        DENSITY_OWNER
        }
        int main(){
         using namespace epochengine;namespace p=previewgrid;namespace c=asset::cloud_volume;
         constexpr double earth=6'371'000.0;
         p::CloudVolume cloud{.center={0,-earth,0},.radius=earth,.recipe={.seed=91,
          .base_altitude_meters=2000,.thickness_meters=1500,.horizontal_scale_meters=50000,
          .coverage=.65f,.extinction_per_meter=.001f,.wind_period_seconds=600}};
         p::Camera camera;camera.eye={0,0,0};camera.target={0,0,-1};camera.nearPlane=.1f;camera.farPlane=1e8f;
         CHECK(p::valid_cloud_volume(cloud));auto initial=openglcloud::pack_volume(cloud,camera);CHECK(initial);
         CHECK(initial->radius_meters==earth&&initial->center[1]==-1.0f&&initial->thickness>0.0002f);
         CHECK(float(1+initial->base+initial->thickness)>float(1+initial->base));
         auto tiny=cloud;tiny.radius=1e9;tiny.recipe.thickness_meters=1;tiny.recipe.horizontal_scale_meters=1e6;
         CHECK(p::valid_cloud_volume(tiny)&&!openglcloud::pack_volume(tiny,camera));
         tiny.recipe.thickness_meters=10000;CHECK(openglcloud::pack_volume(tiny,camera));
         auto underflow=cloud;underflow.radius=1e-50;underflow.meters_per_world_unit=earth/underflow.radius;
         CHECK(p::valid_cloud_volume(underflow)&&!openglcloud::pack_volume(underflow,camera));
         auto scaled=cloud;scaled.radius*=2;scaled.center[1]*=2;scaled.meters_per_world_unit=.5;
         const auto scaledPack=openglcloud::pack_volume(scaled,camera);CHECK(scaledPack);
         CHECK(scaledPack->radius_meters==initial->radius_meters&&scaledPack->center==initial->center&&scaledPack->thickness==initial->thickness);
         auto bad=cloud;bad.rotation[0]=-1;CHECK(!p::valid_cloud_volume(bad));
         bad=cloud;bad.time_seconds=std::nan("");CHECK(!p::valid_cloud_volume(bad));
         const std::array seven{cloud,cloud,cloud,cloud,cloud,cloud,cloud};CHECK(!p::admitted_cloud_volumes(seven));
         p::SceneFrameRequest first,second;first.clouds={cloud};second=first;second.clouds[0].time_seconds=42;
         CHECK(p::same_scene_frame_request(first,second));second.clouds[0].recipe.coverage=.2f;
         CHECK(!p::same_scene_frame_request(first,second));
         int context{};lighting::LightingFrame lights;const std::array<double,3> origin{1e15,0,0};
         CHECK(p::publish_scene_frame(&context,{},camera,lights,{},origin,std::span{&cloud,1}));
         const auto original=p::scene_frame_for(&context);CHECK(original&&original->clouds[0].time_seconds==0);
         auto moved=cloud;moved.center[0]-=256;moved.time_seconds=20;
         auto movedOrigin=origin;movedOrigin[0]+=256;
         CHECK(p::advance_scene_frame_camera(&context,camera,movedOrigin,std::span<const p::CloudVolume>{&moved,1}));
         const auto advanced=p::scene_frame_for(&context);
         CHECK(advanced->solids==original->solids&&advanced->wire==original->wire);
         CHECK(advanced->clouds[0].center==cloud.center&&advanced->clouds[0].time_seconds==20);
         CHECK(original->clouds[0].time_seconds==0&&advanced->revision>original->revision);
         const auto wind=openglcloud::pack_volume(advanced->clouds[0],camera);CHECK(wind&&wind->sine!=initial->sine);
         // A paused sample is stable. Reverse seeks restore the previous density pose.
         CHECK(p::advance_scene_frame_camera(&context,camera,movedOrigin,std::span<const p::CloudVolume>{&moved,1}));
         const auto paused=p::scene_frame_for(&context);CHECK(openglcloud::pack_volume(paused->clouds[0],camera)->sine==wind->sine);
         moved.time_seconds=0;CHECK(p::advance_scene_frame_camera(&context,camera,movedOrigin,std::span<const p::CloudVolume>{&moved,1}));
         const auto rewound=p::scene_frame_for(&context);CHECK(openglcloud::pack_volume(rewound->clouds[0],camera)->sine==initial->sine);
         const auto retained=rewound;CHECK(!p::advance_scene_frame_camera(&context,camera,movedOrigin,std::span<const p::CloudVolume>{&bad,1}));
         CHECK(p::scene_frame_for(&context)==retained);
         CHECK(p::advance_scene_frame_camera(&context,camera,movedOrigin));CHECK(!p::scene_frame_for(&context)->clouds.empty());
         CHECK(p::advance_scene_frame_camera(&context,camera,movedOrigin,std::span<const p::CloudVolume>{}));CHECK(p::scene_frame_for(&context)->clouds.empty());
         CHECK(p::publish_scene_frame(&context,{},camera,lights,{},origin,std::span{&cloud,1}));
         const auto keep=p::scene_frame_for(&context);CHECK(!p::publish_scene_frame(&context,{},camera,lights,{},origin,seven));CHECK(p::scene_frame_for(&context)==keep);
         // Compile the actual GLSL depth expression with float-literal semantics.
         // Far-plane clear depth remains finite even at a1e9 clip ratio.
         depth_shader::uNearFar={.1f,1e8f};
         CHECK(std::isfinite(depth_shader::viewDepth(1.0f))&&depth_shader::viewDepth(1.0f)==1e8f);
         CHECK(std::abs(depth_shader::viewDepth(0.0f)-.1f)<1e-6f);
         for(float depth:{0.0f,.25f,.5f,.9f,.999999f,1.0f}){
          const double expected=.1*1e8/(1e8*(1.0-double(depth))+.1*double(depth));
          CHECK(std::abs(depth_shader::viewDepth(depth)-expected)<std::max(1e-5,expected*1e-6));
         }
         depth_shader::uOrthographic=true;CHECK(depth_shader::viewDepth(0.0f)==.1f&&depth_shader::viewDepth(1.0f)==1e8f);
         p::cleanup_context(&context);return 0;
        }
        '''.replace('PACKED_OWNER',packed).replace('DEPTH_OWNER',depth).replace('DENSITY_OWNER',density_owner)
        code=code.replace('p::cleanup_context(&context);return 0;',r'''
         // CPU/native shape policy agrees at ground, inside the deck and orbit
         // pixel footprints, including an empty weather region.
         density_shader::uBase=static_cast<float>(cloud.recipe.base_altitude_meters/earth);
         density_shader::uThickness=static_cast<float>(cloud.recipe.thickness_meters/earth);
         density_shader::uNoiseScale=static_cast<float>(earth/cloud.recipe.horizontal_scale_meters);
         density_shader::uCoverage=cloud.recipe.coverage;density_shader::uSeed=cloud.recipe.seed;
         density_shader::uWind={1.0f,0.0f};
         for(unsigned index=0;index<128u;++index)for(double footprint:{0.0,100.0,50000.0,1e7}){
          const double y=1.0-2.0*(index+0.5)/128.0,angle=index*2.399963229728653;
          const double horizontal=std::sqrt(1-y*y),distance=earth+2750;
          const c::Point point{horizontal*std::cos(angle)*distance,y*distance,horizontal*std::sin(angle)*distance};
          const auto expected=c::density(cloud.recipe,earth,point,0.0,footprint);
          const auto actual=density_shader::cloudDensity({static_cast<float>(point[0]/earth),static_cast<float>(point[1]/earth),static_cast<float>(point[2]/earth)},static_cast<float>(footprint/earth));
          CHECK(std::isfinite(actual)&&std::abs(actual-expected)<0.003);
         }
         CHECK(density_shader::cloudDensity({0.0f,1.0f,0.0f},0.0f)==0.0f);
         p::cleanup_context(&context);return 0;
        ''')
        (report/'cloud.packet_contract.cpp').write_text(code,encoding='utf-8')
        module_map={}
        owners=[repo/'Engine/modules',repo/'Engine/dep/EpochPlatformEngine/modules']
        for owner in owners:
         for path in owner.glob('*.ixx'):
          match=re.search(r'^export module\s+([\w.:]+);',path.read_text(encoding='utf-8-sig'),re.M)
          if match:module_map[match.group(1)]=path
        pending=['render.preview_grid','authoring.texture_detail'];closure=set()
        while pending:
         name=pending.pop()
         if name in closure:continue
         closure.add(name);pending+=re.findall(r'^(?:export\s+)?import\s+([\w.:]+);',module_map[name].read_text(encoding='utf-8-sig'),re.M)
        paths='\n'.join('"'+module_map[name].as_posix()+'"' for name in sorted(closure))
        bases=' '.join('"'+owner.as_posix()+'"' for owner in owners)
        sources=' '.join('"'+(repo/p).as_posix()+'"' for p in ['Engine/src/render/render.preview_camera.cpp','Engine/src/assets/texture/asset.texture_artifact.cpp','Engine/src/authoring/texture/authoring.texture_document.cpp','Engine/src/authoring/texture/authoring.texture_residency.cpp'])
        (report/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.30)
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
        project(CloudPacketContract LANGUAGES CXX)
        add_executable(cloud_packet cloud.packet_contract.cpp {sources})
        target_compile_features(cloud_packet PRIVATE cxx_std_23)
        target_include_directories(cloud_packet PRIVATE "{(repo/'Engine/include').as_posix()}")
        target_sources(cloud_packet PRIVATE FILE_SET CXX_MODULES BASE_DIRS {bases} FILES {paths})
        if(MSVC)
         target_compile_options(cloud_packet PRIVATE /utf-8 /W3 /WX /UNDEBUG /Z7)
         target_compile_definitions(cloud_packet PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        enable_testing()
        add_test(NAME cloud_packet_authority COMMAND cloud_packet)
        set_tests_properties(cloud_packet_authority PROPERTIES TIMEOUT 15)
        add_executable(cloud_density "{(repo/'Tests/assets/asset.cloud_volume_contract.cpp').as_posix()}")
        target_compile_features(cloud_density PRIVATE cxx_std_23)
        target_sources(cloud_density PRIVATE FILE_SET CXX_MODULES BASE_DIRS "{(repo/'Engine/modules').as_posix()}" FILES "{(repo/'Engine/modules/asset.cloud_volume.ixx').as_posix()}")
        if(MSVC)
         target_compile_options(cloud_density PRIVATE /utf-8 /W3 /WX /UNDEBUG /Z7)
        endif()
        add_test(NAME cloud_density_ground_orbit COMMAND cloud_density)
        set_tests_properties(cloud_density_ground_orbit PROPERTIES TIMEOUT 15)
        ''',encoding='utf-8')
        build=repo/'Artifacts/Intermediate/tests/cloud-volume-rendering';commands=[['cmake','-S',str(report),'-B',str(build),'-G','Visual Studio 17 2022','-A','x64','-DCMAKE_VS_GLOBALS=VcpkgEnabled=false;VcpkgManifestInstall=false;VcpkgAutoLink=false']]
        for config in ['Debug','Release']:
         commands.extend([['cmake','--build',str(build),'--config',config,'--parallel','1','--','/nr:false','/p:CL_MPCount=1'],['ctest','--test-dir',str(build),'-C',config,'--output-on-failure']])
        records=[]
        for index,command in enumerate(commands):
         start=time.monotonic();result=subprocess.run(command,cwd=report,capture_output=True,text=True,timeout=240,env=dict(os.environ,MSBUILDDISABLENODEREUSE='1'))
         (report/f'lane-{index}.log').write_text(result.stdout+result.stderr,encoding='utf-8');records.append(dict(command=command,exit=result.returncode,seconds=time.monotonic()-start))
         print(index,result.returncode,(result.stdout+result.stderr)[-4500:])
         (report/'result.json').write_text(json.dumps(dict(pass_all=all(r['exit']==0 for r in records),complete=len(records)==len(commands),records=records),indent=2),encoding='utf-8')
         self.assertEqual(result.returncode,0,(result.stdout+result.stderr)[-10000:])

if __name__=="__main__":unittest.main()
