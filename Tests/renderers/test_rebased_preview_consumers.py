"""CPU proof of the actual backend snapshot reads and Vulkan mesh builders.

Graphics APIs are never linked. Recording legacy getters deliberately fail when
an admitted rebased frame is held; publishing a second frame cannot mix its
camera, vertices or draw counts into the retained first frame.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

from Tests.support.cpp import msvc_2022_toolsets

ROOT = Path(__file__).resolve().parents[2]


class RebasedPreviewConsumers(unittest.TestCase):
    def test_actual_snapshot_reads_and_vulkan_builders(self):
        paths = [
            'Engine/src/renderers/directx/directx.preview.cpp',
            'Engine/src/renderers/sdl/core.context_sdl.cpp',
            'Engine/src/renderers/sfml/core.context_sfml.cpp',
            'Engine/src/renderers/software/software.context_preview.cpp',
            'Engine/modules/raylib.context.ixx',
            'Engine/modules/sdl.context.ixx',
            'Engine/modules/sfml.context.ixx',
        ]
        readers = []
        for index, path in enumerate(paths):
            text = (ROOT/path).read_text(encoding='utf-8')
            statements = []
            for marker in ('const auto camera = matchedScene',
                           'const auto solidGeometry = matchedScene',
                           'const auto objectGeometry = matchedScene'):
                begin = text.index(marker)
                statements.append(text[begin:text.index(';', begin)+1])
            ctx = '&ctx' if '/directx/' in path or '/software/' in path else 'ctx.get()'
            setup = 'core::Context ctx;' if ctx == '&ctx' else 'auto ctx=std::make_shared<core::Context>();'
            acquisition_owner = (ROOT/'Engine/src/renderers/directx/directx.context.cpp').read_text(encoding='utf-8') if '/directx/' in path else text
            start = acquisition_owner.index('const auto matchedScene =')
            acquisition = acquisition_owner[start:acquisition_owner.index(';', start)+1]
            if '/directx/' in path:
                acquisition = acquisition.replace('ctx.get()', '&ctx')  # Caller uses shared_ptr; preview helper uses reference.
            readers.append('void reader'+str(index)+'(){'+setup+
                acquisition+'published=second;'+
                '\n'.join(statements)+
                'CHECK(camera.eye.x==11);CHECK(solidGeometry==first->solids->geometry);'
                'CHECK(objectGeometry==first->lines);}')
        vk_path = 'Engine/modules/vulkan.context_meshcube.ixx'
        vk = (ROOT/vk_path).read_text(encoding='utf-8')
        body = '\n'.join(definition(vk, marker).replace('export ', '') for marker in (
            '    export bool preview_geometry_matches(',
            '    [[nodiscard]] inline std::vector<Vertex> build_preview_vertices(',
            '    [[nodiscard]] inline std::vector<std::uint32_t> build_preview_indices(',
            '    export std::vector<Vertex> preview_vertices_for(',
            '    export std::vector<std::uint32_t> preview_indices_for(',
            '    export std::uint32_t preview_solid_index_count_for(',
            '    export std::uint32_t preview_arcade_screen_index_count_for('))
        commands_path = 'Engine/modules/vulkan.context_commands.ixx'
        commands = (ROOT/commands_path).read_text(encoding='utf-8')
        rebuild = definition(commands, '        if (frame.scene_visible && ctx)')
        start = commands.index('previewSceneFrame = previewgrid::scene_frame_for(')
        capture = commands[start:commands.index(';', start)+1]
        self.assertLess(start, commands.index('updateUniformBuffer(imageIndex, cam);'))
        descriptor_path = 'Engine/modules/vulkan.context_descriptor.ixx'
        descriptor = (ROOT/descriptor_path).read_text(encoding='utf-8')
        start = descriptor.index('const auto previewCamera =')
        camera_read = descriptor[start:descriptor.index(';', start)+1]
        module_path = 'Engine/modules/render.preview_grid.ixx'
        module = (ROOT/module_path).read_text(encoding='utf-8')
        packet = definition(module, '    export struct PublishedSceneFrame', True).replace('export ', '')
        cloud_recipe = (ROOT/'Engine/modules/asset.cloud_volume.ixx').read_text(encoding='utf-8')
        cloud = ('namespace asset::cloud_volume {\n' + definition(cloud_recipe, '    struct Recipe final', True) + '\n}\n')
        cloud += 'namespace previewgrid {\n' + definition(module, '    export struct CloudVolume final', True).replace('export ', '') + '\n}\n'
        arcade = (ROOT/'Engine/modules/render.arcade.ixx').read_text(encoding='utf-8')
        arcade_ports = '\n'.join(definition(arcade, marker, True) for marker in (
            '    struct ArcadeSceneNodeContract',
            '    inline constexpr ArcadeSceneNodeContract kCabinetBodySceneNode',
            '    inline constexpr ArcadeSceneNodeContract kScreenSceneNode',
        ))
        arcade_ports += '\n' + '\n'.join(definition(arcade, marker) for marker in (
            '    [[nodiscard]] constexpr float screen_sample_plane_z(',
            '    [[nodiscard]] constexpr bool screen_sample_plane_faces_viewer(',
        ))
        routes = '\n'.join(definition(module, marker, True).replace('export ', '') for marker in (
            '    export enum class ObjectPreviewPrimitive',
            '    export struct ObjectPreviewGeometryRoute',
        ))
        routes += '\n' + definition(module, '    export [[nodiscard]] constexpr ObjectPreviewGeometryRoute object_preview_geometry_route(').replace('export ', '')
        wire = definition(module, '    export struct MeshWireDrawFrame final', True).replace('export ', '')
        facing = definition(module, '    export [[nodiscard]] constexpr bool clockwise_solid_triangle_faces_camera(').replace('export ', '')
        visitor = 'template<class Emit>\n' + definition(module, '    inline void for_each_visible_marker_line(')

        cpp = r'''#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <span>
#include <string_view>
#include <vector>
#define CHECK(x) do{if(!(x))std::exit(__LINE__);}while(false)
namespace epochengine {
namespace core {struct Context{};}
@@CLOUD@@
namespace render_arcade {
@@ARCADE@@
}
namespace previewgrid {
struct Vec3{float x{},y{},z{};};
struct Vertex{Vec3 position,color;};
struct Camera{Vec3 eye;};
@@ROUTES@@
struct ObjectMarker{
    Vec3 position,scale;
    ObjectPreviewPrimitive primitive{ObjectPreviewPrimitive::EngineArcadeScreen};
    bool sampledRenderSurface{true};
};
struct SolidDrawFrame{std::shared_ptr<const std::vector<Vertex>> geometry;};
@@WIRE@@
@@FACING@@
@@VISITOR@@
struct GridGeometry{std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;};
@@PACKET@@
inline std::shared_ptr<const PublishedSceneFrame> published,first,second;
std::shared_ptr<const PublishedSceneFrame> scene_frame_for(const void*){return published;}
std::uint64_t preview_geometry_revision_for(const void*){return 7;}
unsigned legacyCalls{};
void forbidden(){++legacyCalls;}
Camera camera_for(const void*){forbidden();return {};}
auto object_solid_geometry_for(const void*){forbidden();return std::make_shared<const std::vector<Vertex>>();}
auto object_marker_geometry_for(const void*){forbidden();return std::make_shared<const std::vector<Vertex>>();}
auto object_mesh_wire_frame_for(const void*){forbidden();return std::make_shared<const MeshWireDrawFrame>();}
auto grid_geometry_for(const void*){forbidden();return std::make_shared<const GridGeometry>();}
std::array<Vertex,8> look_marker_vertices_for(const void*){forbidden();return {};}
std::size_t look_marker_vertex_count_for(const void*){forbidden();return 0;}
std::vector<ObjectMarker> sampled_render_surface_markers_for(const void*){forbidden();return {};}
std::vector<ObjectMarker> sampled_render_surface_markers_for(const PublishedSceneFrame&f){return f.markers;}
}
}
using namespace epochengine;
using namespace epochengine::previewgrid;
@@READERS@@
namespace vulkanconsumer {
struct Vertex{std::array<float,3> position,color;std::array<float,2> uv;};
constexpr std::uint32_t kArcadeScreenVertexCount=4,kArcadeScreenIndexCount=6;
std::array<float,3> preview_color_to_vulkan(const previewgrid::Vec3&c){return {c.x,c.y,c.z};}
@@BUILDERS@@
struct UploadOwner {
 std::shared_ptr<const PublishedSceneFrame> previewSceneFrame,uploadedPreviewSceneFrame;
 std::uint64_t previewGeometryRevision{};
 bool previewIndicesSequential{};
 unsigned indexCount{},solidIndexCount{},lineIndexCount{},arcadeScreenIndexOffset{},arcadeScreenIndexCount{},uploads{};
 void createVertexBuffer(){++uploads;}
 void createIndexBuffer(){++uploads;indexCount=11;}
 const core::Context* bound_context(){return nullptr;}
 void captureAndRead(){const auto*ctx=bound_context();
 @@CAPTURE@@
 published=second;
 @@CAMERA_READ@@
 CHECK(previewCamera.eye.x==11);
 }
 void update(){core::Context context;auto*ctx=&context;struct{bool scene_visible=true;}frame;
 @@REBUILD@@
 }
};
}
int main(){
 auto a=std::make_shared<PublishedSceneFrame>();a->camera.eye.x=11;a->revision=1;
 auto solids=std::make_shared<SolidDrawFrame>();
 solids->geometry=std::make_shared<const std::vector<previewgrid::Vertex>>(3,previewgrid::Vertex{{1,2,3},{1,1,1}});a->solids=solids;
 a->lines=std::make_shared<const std::vector<previewgrid::Vertex>>(2,previewgrid::Vertex{{4,5,6},{1,1,1}});
 a->markers.push_back({{7,8,9},{2,2,2}});a->wire=std::make_shared<const MeshWireDrawFrame>();
 auto b=std::make_shared<PublishedSceneFrame>(*a);b->camera.eye.x=99;b->revision=2;
 auto other=std::make_shared<SolidDrawFrame>();other->geometry=std::make_shared<const std::vector<previewgrid::Vertex>>(6);b->solids=other;
 first=a;second=b;
 @@RUN_READERS@@
 published=first;const auto retained=scene_frame_for(nullptr);published=second;
 auto vertices=vulkanconsumer::preview_vertices_for(nullptr,retained);
 auto indices=vulkanconsumer::preview_indices_for(nullptr,retained);
 CHECK(vertices.size()==9&&indices.size()==11);
 CHECK(vertices[0].position[0]==1&&vertices[7].position[0]==4);
 CHECK(vulkanconsumer::preview_solid_index_count_for(nullptr,retained)==3);
 CHECK(vulkanconsumer::preview_arcade_screen_index_count_for(nullptr,retained)==6);
 for(auto i:indices)CHECK(i<vertices.size());
 CHECK(legacyCalls==0);
 // Exact backend builders must replace every-edge fallback with front-face edges,
 // retain helper lines and keep index/vertex counts identical in both directions.
 auto wired=std::make_shared<PublishedSceneFrame>(*retained);
 auto wire=std::make_shared<MeshWireDrawFrame>();
 wire->depth_triangles=std::make_shared<const std::vector<previewgrid::Vertex>>(std::initializer_list<previewgrid::Vertex>{
  {{0,-1,-1},{1,0,0}},{{0,-1,1},{1,0,0}},{{0,1,0},{1,0,0}},
  {{0,-1,-1},{0,1,0}},{{0,1,0},{0,1,0}},{{0,-1,1},{0,1,0}}});
 wire->helper_lines=retained->lines;wired->wire=wire;
 auto visible=vulkanconsumer::preview_vertices_for(nullptr,wired);
 auto visibleIndices=vulkanconsumer::preview_indices_for(nullptr,wired);
 CHECK(visible.size()==15&&visibleIndices.size()==17);
 for(auto i:visibleIndices)CHECK(i<visible.size());
 CHECK(visible[7].color[0]==1&&visible[7].color[1]==0);
 wired->camera.eye.x=-11;
 auto reverse=vulkanconsumer::preview_vertices_for(nullptr,wired);
 CHECK(reverse.size()==15&&reverse[7].color[0]==0&&reverse[7].color[1]==1);
 CHECK(legacyCalls==0);
 vulkanconsumer::UploadOwner uploader;published=first;uploader.captureAndRead();CHECK(uploader.previewSceneFrame==first);uploader.previewSceneFrame=first;uploader.update();CHECK(uploader.uploads==2&&uploader.indexCount==11);
 uploader.update();CHECK(uploader.uploads==2);
 uploader.previewSceneFrame=std::make_shared<PublishedSceneFrame>(*first);
 uploader.update();CHECK(uploader.uploads==2);
 auto empty=std::make_shared<PublishedSceneFrame>(*first);auto emptySolids=std::make_shared<SolidDrawFrame>();
 emptySolids->geometry=std::make_shared<const std::vector<previewgrid::Vertex>>();empty->solids=emptySolids;
 empty->lines=emptySolids->geometry;empty->markers.clear(); // Equal revision deliberately tests pointer transition.
 uploader.previewSceneFrame=empty;uploader.update();CHECK(uploader.uploads==2&&uploader.indexCount==0);
 uploader.previewSceneFrame=first;uploader.update();CHECK(uploader.uploads==4&&uploader.indexCount==11);
 uploader.previewSceneFrame.reset();uploader.update();CHECK(uploader.uploads==6);
 return 0;
}
'''
        cpp = cpp.replace('@@WIRE@@',wire).replace('@@FACING@@',facing).replace('@@VISITOR@@',visitor).replace('@@PACKET@@', packet).replace('@@READERS@@', '\n'.join(readers)).replace('@@BUILDERS@@', body).replace('@@REBUILD@@', rebuild).replace('@@CAPTURE@@', capture).replace('@@CAMERA_READ@@', camera_read)
        cpp = cpp.replace('@@CLOUD@@', cloud).replace('@@ARCADE@@', arcade_ports).replace('@@ROUTES@@', routes)
        cpp = cpp.replace('@@RUN_READERS@@', '\n'.join('published=first;reader'+str(i)+'();' for i in range(len(readers))))
        proof = ROOT/'Artifacts/Reports/planetary-frame-precision-oct31/backend-consumers/cpu'/hashlib.sha256(cpp.encode()).hexdigest()[:12]
        proof.mkdir(parents=True, exist_ok=True)
        source = proof/'render.rebased_consumer_contract.cpp'
        source.write_text(cpp, encoding='utf-8')
        runs = compile_and_run(self, source, proof)
        (proof/'result.json').write_text(json.dumps({'scope':'Actual PublishedSceneFrame declaration, backend read expressions and Vulkan mesh/count/rebuild functions with recording legacy getters; not whole producer module or native backend proof','declaration_sha256':hashlib.sha256(packet.encode()).hexdigest(),'runs':runs,'sources':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in paths+[vk_path,module_path,commands_path,descriptor_path]},'native':False},indent=2),encoding='utf-8')


    def test_complete_sfml_functions_compile_with_installed_headers(self):
        if os.name != 'nt':
            self.skipTest('Installed MSVC/SFML graph compile-only check')
        sfml = ROOT/'Artifacts/Dependencies/vcpkg_installed/x64-windows/include'
        if not (sfml/'SFML/Graphics.hpp').is_file():
            self.skipTest('Installed SFML headers required; no dependency download')
        paths = ['Engine/modules/sfml.context.ixx', 'Engine/src/renderers/sfml/core.context_sfml.cpp']
        functions = [definition((ROOT/path).read_text(encoding='utf-8'), signature)
            for path,signature in zip(paths, ['        inline void render_scene_preview(', '    void render_scene_preview('])]
        ports = r"""#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
namespace epochengine {
namespace core {
enum class ScenePreviewMode{Editor};
struct RenderViewport{int x,y,width,height;bool valid()const;};
struct Context{RenderViewport scene_viewport()const;ScenePreviewMode scene_preview_mode()const;bool gui_overlay_priority()const;int frame_window_state()const;};
}
namespace previewgrid {
struct Vec3{float x,y,z;};struct Vertex{Vec3 position,color;};
struct Camera{Vec3 eye,target,up;};struct Mat4{};
struct GridGeometry{std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;};
struct SolidDrawFrame{std::shared_ptr<const std::vector<Vertex>> geometry;};
struct MeshWireDrawFrame{};
struct PublishedSceneFrame{Camera camera;std::shared_ptr<const SolidDrawFrame> solids;std::shared_ptr<const MeshWireDrawFrame> wire;std::shared_ptr<const std::vector<Vertex>> lines;};
std::shared_ptr<const MeshWireDrawFrame> object_mesh_wire_frame_for(const void*);
template<class Emit>void for_each_visible_marker_line(const MeshWireDrawFrame&,std::span<const Vertex>,Vec3,Emit){}
extern std::array<float,4> kClearColor;
std::shared_ptr<const PublishedSceneFrame> scene_frame_for(const void*);
Camera camera_for(const void*);Mat4 projection_for(const void*,float,const Camera&);Mat4 look_at(Vec3,Vec3,Vec3);Mat4 multiply(Mat4,Mat4);
std::shared_ptr<const GridGeometry> grid_geometry_for(const void*);
std::shared_ptr<const std::vector<Vertex>> object_solid_geometry_for(const void*),object_marker_geometry_for(const void*);
std::array<Vertex,8> look_marker_vertices_for(const void*);std::size_t look_marker_vertex_count_for(const void*);
bool clockwise_solid_triangle_faces_camera(Vec3,Vec3,Vec3,Vec3);
}
namespace sfml_compat {sf::FloatRect float_rect(float,float,float,float);}
namespace rendercontext {
struct Request{int width,height;core::RenderViewport viewport;bool visible,overlay;int state;};
struct Frame{bool scene_visible;core::RenderViewport scene;float normalized_left,normalized_top,normalized_width,normalized_height;};
Frame resolve_frame_plan(Request);
}
}
using namespace epochengine;
bool project_preview_vertex(const previewgrid::Mat4&,previewgrid::Vec3,const core::RenderViewport&,sf::Vector2f&);
sf::Color to_sfml_color(previewgrid::Vec3);
void render_engine_arcade_sampled_surface_preview(const std::shared_ptr<core::Context>&,const previewgrid::Mat4&,const core::RenderViewport&,const std::shared_ptr<const previewgrid::PublishedSceneFrame>&);
enum class CanvasSceneStatus{missing_scene};CanvasSceneStatus render_canvas2d_scene(const std::shared_ptr<core::Context>&,const core::RenderViewport&);
namespace module_path {struct{sf::RenderWindow*window;}sfmlcontext;
@@MODULE@@
}
namespace native_path {sf::RenderWindow*s_window;int s_width,s_height;
@@NATIVE@@
}
"""
        cpp = ports.replace('@@MODULE@@',functions[0]).replace('@@NATIVE@@',functions[1])
        proof = ROOT/'Artifacts/Reports/planetary-frame-precision-oct31/backend-consumers/sfml-scope-repair'/hashlib.sha256(cpp.encode()).hexdigest()[:12]
        proof.mkdir(parents=True,exist_ok=True)
        source = proof/'render.sfml_scope_contract.cpp';source.write_text(cpp,encoding='utf-8')
        tool = msvc_2022_toolsets()[-1]
        sdk = Path('C:/Program Files (x86)/Windows Kits/10');kit = sorted((sdk/'Include').glob('*'))[-1]
        temp = proof/'tmp';temp.mkdir(exist_ok=True)
        records=[]
        for config,flags in [('Debug',['/Od','/MDd','/D_DEBUG']),('Release',['/O2','/MD'])]:
            command=[str(tool/'bin/Hostx64/x64/cl.exe'),'/nologo','/std:c++latest','/EHsc','/utf-8','/Z7','/c',*flags,
                '/I'+str(tool/'include'),'/I'+str(kit/'ucrt'),'/I'+str(sfml),'/Fo'+str(proof/(config+'.obj')),str(source)]
            run=subprocess.run(command,cwd=proof,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),capture_output=True,text=True,timeout=60)
            (proof/(config+'-compile.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
            records.append({'configuration':config,'exit':run.returncode,'command':command})
            (proof/'result.json').write_text(json.dumps({'scope':'Complete actual SFML scene function bodies and real installed SFML3.0.2 types/API; declared engine ports; compile only, no link or runtime','runs':records,'sources':{path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in paths}},indent=2),encoding='utf-8')
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

if __name__ == '__main__':
    unittest.main()
