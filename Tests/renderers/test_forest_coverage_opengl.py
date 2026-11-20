"""Forest coverage shader source and actual scoped upload against simulated GL."""
from pathlib import Path
import json
import re
import shutil
import subprocess
import time
import unittest

from Tests.support.cpp import compile_and_run
from Tests.support.embedded_glsl import opengl_scene_fragment
from Tests.build.test_opengl_sprite_stream import definition

ROOT = Path(__file__).resolve().parents[2]
PREVIEW = ROOT / 'Engine/modules/opengl/opengl.preview.ixx'


class ForestCoverageOpenGL(unittest.TestCase):
    def test_exact_glsl330_compiles_offline(self):
        compiler = ROOT / 'Artifacts/Dependencies/vcpkg_installed/x64-windows/tools/shaderc/glslc.exe'
        if not compiler.is_file():
            compiler = Path(shutil.which('glslc') or '')
        if not compiler.is_file():
            self.skipTest('Installed glslc required; no tool download or graphics launch')
        shaders = re.findall(r'R"\((#version 330 core.*?)\)"', PREVIEW.read_text(encoding='utf-8'), re.S)
        report = ROOT / 'Artifacts/Reports/forest-coverage-opengl' / ('shader-' + str(time.time_ns()))
        report.mkdir(parents=True)
        results = []
        for stage, source in (('vert', shaders[0]), ('frag', opengl_scene_fragment())):
            path = report / ('forest.' + stage)
            path.write_text(source, encoding='utf-8')
            command = [str(compiler), '--target-env=opengl', '-fauto-map-locations',
                       '-fauto-bind-uniforms', '-c', str(path), '-o', str(path) + '.spv']
            result = subprocess.run(command, cwd=report, capture_output=True, text=True, timeout=30)
            (report / (stage + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
            results.append(dict(stage=stage, exit=result.returncode, command=command))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        (report / 'result.json').write_text(json.dumps(results, indent=2), encoding='utf-8')

    def test_extracted_coverage_function_cpu(self):
        include = ROOT / 'Artifacts/Dependencies/vcpkg_installed/x64-windows/include'
        if not (include / 'glm/glm.hpp').is_file():
            self.skipTest('Installed GLM required; no dependency download')
        shader = re.findall(r'R"\((#version 330 core.*?)\)"', PREVIEW.read_text(encoding='utf-8'), re.S)[1]
        function = definition(shader, 'vec3 forestCoverage(')
        # Syntax bridge only: GLM swizzles are methods; C++ float literals need f.
        function = re.sub(r'\.(xyz|rgb|xy|zw)\b', r'.\1()', function)
        function = re.sub(r'\b(?:\d+\.\d+(?:e[+-]?\d+)?|\d+e[+-]?\d+)\b', lambda m: m[0] + 'f', function)
        editor = (ROOT / 'Engine/src/editor/application/editor.application_scene_preview.inl').read_text(encoding='utf-8')
        publication = editor.split('const auto publishLasso = ', 1)[1].split('if (highlighted)', 1)[0]
        # Use the production presentation expressions, including draft/selected
        # opacity, rather than copying today's palette into the test.
        fill = publication.split('.fill = ', 1)[1].split(',\n', 1)[0].replace('std::array', 'vec4')
        edge = publication.split('.edge = ', 1)[1].split('};', 1)[0].replace('std::array', 'vec4')
        colors = ('vec4 authored_fill(bool highlighted,bool drafting){return ' + fill + ';}\n'
                  'vec4 authored_edge(bool highlighted){return ' + edge + ';}\n')
        report = ROOT / 'Artifacts/Reports/forest-coverage-opengl' / ('fragment-cpu-' + str(time.time_ns()))
        report.mkdir(parents=True)
        cpp = report / 'consumer.cpp'
        cpp.write_text(r'''
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <cassert>
using namespace glm;
vec3 vDrawPosition{},dx{.001f,0,0},dy{0,.001f,0};
int uPaintTable{},uPaintRows{};
std::vector<std::array<vec4,135>> rows;
vec3 dFdx(vec3){return dx;}vec3 dFdy(vec3){return dy;}
vec4 texelFetch(int,ivec2 index,int){return rows.at(index.y).at(index.x);}
''' + function + colors + r'''
std::array<vec4,135> square(float half,vec4 fill){
 std::array<vec4,135> row{};
 row[0]={0,0,1,4};row[1]={1,0,0,0};row[2]={0,1,0,0};row[3]={0,0,1,0};
 row[4]={-half,-half,half,half};row[5]=fill;row[6]={0,1,0,1};
 row[7]={-half,-half,0,0};row[8]={half,-half,0,0};row[9]={half,half,0,0};row[10]={-half,half,0,0};return row;
}
bool close(vec3 a,vec3 b){return length(a-b)<1e-5f;}
int main(){
 const vec3 base{.2f,.2f,.2f};rows.push_back(square(.1f,{1,0,0,.5f}));
 vDrawPosition={0,0,1};assert(close(forestCoverage(base),base));
 uPaintRows=1;assert(close(forestCoverage(base),{.6f,.1f,.1f}));
 vDrawPosition={.2f,0,1};assert(close(forestCoverage(base),base));
 vDrawPosition={.1f,0,1};assert(close(forestCoverage(base),{0,1,0}));
 vDrawPosition={0,0,-1};assert(close(forestCoverage(base),base));
 vDrawPosition={0,0,2};assert(close(forestCoverage(base),base));
 rows.push_back(square(.2f,{0,0,1,.5f}));uPaintRows=2;vDrawPosition={0,0,1};
 assert(close(forestCoverage(base),{.6f,.1f,.1f}));
 rows[0]=square(.0001f,{1,0,0,.5f});dx={.000001f,0,0};dy={0,.000001f,0};
 assert(close(forestCoverage(base),{.6f,.1f,.1f}));
 rows[0][0].w=2;assert(close(forestCoverage(base),{.1f,.1f,.6f}));
 // The same actual chart and fragment sample must retain distinct authored
 // land and sea input colors. Coverage never replaces the material wholesale.
 const vec3 land{.32f,.42f,.12f},sea{.02f,.09f,.18f};
 dx={.001f,0,0};dy={0,.001f,0};uPaintRows=1;rows.resize(1);
 for(bool highlighted:{false,true})for(bool drafting:{false,true}){
  const vec4 fill=authored_fill(highlighted,drafting),edge=authored_edge(highlighted);
  assert(fill.a>0 && fill.a<1 && edge.a>0 && edge.a<1);
  rows[0]=square(.1f,fill);rows[0][6]=edge;vDrawPosition={0,0,1};
  const auto paintedLand=forestCoverage(land),paintedSea=forestCoverage(sea);
  const vec3 fillRgb{fill.x,fill.y,fill.z},edgeRgb{edge.x,edge.y,edge.z};
  assert(close(paintedLand,land*(1-fill.a)+fillRgb*fill.a));
  assert(close(paintedSea,sea*(1-fill.a)+fillRgb*fill.a));
  assert(close(paintedLand-paintedSea,(land-sea)*(1-fill.a)));
  assert(length(paintedLand-paintedSea)>.1f);
  // Fully covered outline just inside the polygon still retains the base
  // contribution through both independently authored alpha composites.
  vDrawPosition={.0999999f,0,1};
  const auto edgedLand=forestCoverage(land),edgedSea=forestCoverage(sea);
  assert(close(edgedLand,paintedLand*(1-edge.a)+edgeRgb*edge.a));
  assert(close(edgedSea,paintedSea*(1-edge.a)+edgeRgb*edge.a));
  assert(close(edgedLand-edgedSea,(land-sea)*(1-fill.a)*(1-edge.a)));
  assert(length(edgedLand-edgedSea)>.005f);
  // Exterior antialiasing is outline-only: there is no fill outside the lasso.
  vDrawPosition={.10125f,0,1};
  assert(close(forestCoverage(land)-forestCoverage(sea),(land-sea)*(1-edge.a*.5f)));
  vDrawPosition={.104f,0,1};
  assert(close(forestCoverage(land),land) && close(forestCoverage(sea),sea));
  uPaintRows=0;vDrawPosition={0,0,1};
  assert(close(forestCoverage(land),land) && close(forestCoverage(sea),sea));
  uPaintRows=1;
 }
}
''', encoding='utf-8')
        records = compile_and_run(self, cpp, report, includes=(include,))
        (report / 'result.json').write_text(json.dumps({'scope': 'Extracted GLSL coverage math compiled with GLM syntax bridge, actual editor lasso fill/edge expressions and explicit derivative/texture inputs; distinct land/sea interior, outline, exterior and disabled coverage; not driver execution', 'commands': records}, indent=2), encoding='utf-8')

    def test_shader_depth_and_legacy_scope(self):
        source = PREVIEW.read_text(encoding='utf-8')
        shaders = re.findall(r'R"\((#version (?:330 core|120).*?)\)"', source, re.S)
        vertex, fragment, legacy_vertex, legacy_fragment = shaders[:4]
        self.assertIn('vDrawPosition = uAtmospherePass ? vec3(screen,0.0) : aPos;', vertex)
        self.assertIn('vec4(forestCoverage(vColor), 1.0)', fragment)
        self.assertIn('min(uPaintRows, 65)', fragment)
        self.assertIn('count > 128', fragment)
        self.assertIn('radiusSquared < 0.25', fragment)
        self.assertIn('denominator <= 0.0', fragment)
        self.assertIn('distanceToEdge', fragment)
        self.assertLess(fragment.index('dFdx(vDrawPosition)'), fragment.index('for (int row'))
        self.assertNotIn('gl_FragDepth', fragment)
        self.assertNotIn('discard', definition(fragment, 'vec3 forestCoverage('))
        self.assertNotIn('discard', fragment.split('outColor = vec4(forestCoverage(vColor), 1.0);', 1)[1])
        self.assertNotIn('Paint', legacy_vertex + legacy_fragment)
        self.assertIn('gl_FragColor = vec4(vColor, 1.0);', legacy_fragment)
        destroy = definition(source, '    inline void destroy_scene_preview_pipeline(')
        self.assertIn('glDeleteTextures(1, &state.scenePaintTable)', destroy)
        self.assertIn('state.scenePaintTexels.clear()', destroy)
        wire = definition(source, '    inline void render_mesh_wire_preview(')
        self.assertIn('glUniform1i(state.scenePaintRowsLoc, 0)', wire)

    def test_actual_scope_restores_state_and_limits_uploads(self):
        source = PREVIEW.read_text(encoding='utf-8')
        scope = definition(source, '    struct ScopedForestCoverage final', True)
        textures = (ROOT / 'Engine/modules/opengl/opengl.textures.ixx').read_text(encoding='utf-8')
        native_scope = definition(textures, '    struct ScopedNativeTextureState final', True)
        report = ROOT / 'Artifacts/Reports/forest-coverage-opengl' / str(time.time_ns())
        report.mkdir(parents=True)
        cpp = report / 'consumer.cpp'
        cpp.write_text(r'''
#include <array>
#include <vector>
#include <memory>
#include <optional>
#include <map>
#include <cassert>
#include <utility>
using GLint=int; using GLuint=unsigned; using GLenum=unsigned; using GLsizei=int;
enum : GLenum {GL_ACTIVE_TEXTURE=1,GL_TEXTURE0=100,GL_SAMPLER_BINDING=2,GL_UNPACK_SWAP_BYTES=3,
 GL_TEXTURE_BINDING_2D=4,GL_UNPACK_ALIGNMENT=5,GL_UNPACK_ROW_LENGTH=6,GL_PIXEL_UNPACK_BUFFER_BINDING=7,
 GL_UNPACK_SKIP_PIXELS=8,GL_UNPACK_SKIP_ROWS=9,GL_PIXEL_UNPACK_BUFFER=10,GL_TEXTURE_2D=11,
 GL_TEXTURE_MIN_FILTER=12,GL_TEXTURE_MAG_FILTER=13,GL_NEAREST=14,GL_TEXTURE_BASE_LEVEL=15,
 GL_TEXTURE_MAX_LEVEL=16,GL_RGBA32F=17,GL_RGBA=18,GL_FLOAT=19,GL_FALSE=0,GL_NO_ERROR=0};
std::map<GLenum,GLint> settings; std::map<unsigned,GLint> bindings,samplers,uniforms;
int active=GL_TEXTURE0+3,uploads=0,failUpload=0,error=0;
void glGetIntegerv(GLenum name,GLint* value){
 *value=name==GL_ACTIVE_TEXTURE?active:name==GL_TEXTURE_BINDING_2D?bindings[active]:
 name==GL_SAMPLER_BINDING?samplers[active-GL_TEXTURE0]:settings[name];}
void glActiveTexture(GLenum value){active=value;}
void glBindSampler(GLuint unit,GLuint value){samplers[unit]=value;}
void glBindTexture(GLenum,GLuint value){bindings[active]=value;}
void glBindBuffer(GLenum,GLuint value){settings[GL_PIXEL_UNPACK_BUFFER_BINDING]=value;}
void glPixelStorei(GLenum name,GLint value){settings[name]=value;}
void glGenTextures(GLsizei,GLuint* value){*value=91;}
void glUniform1i(GLint name,GLint value){uniforms[name]=value;}
void glTexParameteri(GLenum,GLenum,GLint){}
GLenum glGetError(){return std::exchange(error,0);}
void glTexImage2D(GLenum,int,GLint,GLsizei width,GLsizei height,int,GLenum,GLenum,const void*){
 assert(width==135 && height>0 && height<=65);assert(settings[GL_PIXEL_UNPACK_BUFFER_BINDING]==0);
 assert(settings[GL_UNPACK_ALIGNMENT]==1 && settings[GL_UNPACK_ROW_LENGTH]==0);
 assert(settings[GL_UNPACK_SKIP_PIXELS]==0 && settings[GL_UNPACK_SKIP_ROWS]==0 && settings[GL_UNPACK_SWAP_BYTES]==0);
 ++uploads;error=failUpload;
}
namespace epochengine::openglstate {
struct OpenGL4State {GLint scenePaintRowsLoc=21,scenePaintTableLoc=22;GLuint scenePaintTable{};
 std::vector<std::array<float,4>> scenePaintTexels;};}
namespace epochengine::previewgrid {
constexpr std::size_t kMeshPaintCoverageRowTexels=135,kMaximumMeshPaintCoverageRows=65;
struct Paint {std::vector<int>lassos{1};};
struct Marker {std::shared_ptr<Paint>paintView=std::make_shared<Paint>();};
struct SolidDrawMarker {Marker marker;std::shared_ptr<std::vector<int>>textured;};
struct SolidDrawFrame {std::vector<SolidDrawMarker>markers{1};std::shared_ptr<std::vector<int>>geometry=std::make_shared<std::vector<int>>(3);};
std::vector<std::array<float,4>> packed(135);
auto pack_mesh_paint_coverage(const Marker&){return packed;}
}
namespace epochengine::opengltextures {
inline void clear_gl_errors(){while(glGetError()!=0){}}
''' + native_scope + '\n}\n' + scope + r'''
int main(){
 settings={{GL_UNPACK_ALIGNMENT,8},{GL_UNPACK_ROW_LENGTH,37},{GL_UNPACK_SKIP_PIXELS,4},
 {GL_UNPACK_SKIP_ROWS,2},{GL_UNPACK_SWAP_BYTES,1},{GL_PIXEL_UNPACK_BUFFER_BINDING,9}};
 bindings={{GL_TEXTURE0,17},{GL_TEXTURE0+3,27}};samplers={{0,42}};
 const auto oldSettings=settings,oldBindings=bindings,oldSamplers=samplers;
 epochengine::openglstate::OpenGL4State state;
 epochengine::previewgrid::SolidDrawFrame frame;
 auto restored=[&]{assert(active==GL_TEXTURE0+3 && settings==oldSettings && bindings==oldBindings && samplers==oldSamplers && uniforms[21]==0);};
 {ScopedForestCoverage guard{state,frame};assert(uniforms[21]==1 && uniforms[22]==0 && uploads==1 && samplers[0]==0);}
 restored();
 {ScopedForestCoverage guard{state,frame};assert(uniforms[21]==1 && uploads==1);}
 restored();
 frame.markers.push_back({});{ScopedForestCoverage guard{state,frame};assert(uniforms[21]==0 && uploads==1);}restored();
 frame.markers.resize(1);frame.markers[0].textured=std::make_shared<std::vector<int>>(3);
 {ScopedForestCoverage guard{state,frame};assert(uniforms[21]==0 && uploads==1);}restored();
 frame.markers[0].textured.reset();
 epochengine::previewgrid::packed[0][0]=2;failUpload=1;
 {ScopedForestCoverage guard{state,frame};assert(uniforms[21]==0 && uploads==2);}restored();
 assert(state.scenePaintTexels[0][0]==0);failUpload=0;
 {ScopedForestCoverage guard{state,frame};assert(uniforms[21]==1 && uploads==3);}restored();
 epochengine::previewgrid::packed.resize(135*66);
 {ScopedForestCoverage guard{state,frame};assert(uniforms[21]==0 && uploads==3);}restored();
 state.scenePaintRowsLoc=-1;{ScopedForestCoverage guard{state,frame};assert(uploads==3);}restored();
}
''', encoding='utf-8')
        records = compile_and_run(self, cpp, report)
        (report / 'result.json').write_text(json.dumps({'scope': 'Actual upload scope with simulated GL calls; no graphics execution or GLSL compiler proof', 'commands': records}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    unittest.main()
