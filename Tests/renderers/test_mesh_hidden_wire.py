"""Actual preview GL state/pass owners with recording ports; no graphics driver."""
from pathlib import Path
import json
import time
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run

ROOT = Path(__file__).resolve().parents[2]


class MeshHiddenWire(unittest.TestCase):
    def test_matched_frame_gl_consumer_ownership(self):
        source = (ROOT / "Engine/modules/opengl.preview.ixx").read_text(encoding="utf-8")
        owner = definition(source, "    inline void render_scene_preview_conventional(")
        self.assertEqual(owner.count("scene_frame_for(ctx)"), 1)
        self.assertIn("matchedScene ? matchedScene->camera", owner)
        self.assertIn("matchedScene ? matchedScene->solids->geometry", owner)
        self.assertIn("matchedScene ? matchedScene->wire", owner)
        self.assertIn("matchedScene ? matchedScene->lines", owner)
        self.assertIn("render_mesh_base_color_preview(ctx,state,mvp,matchedScene)", owner)
        self.assertIn("viewportHeight,\n                matchedScene);", owner)
        self.assertEqual(owner.count("if (!matchedScene)"), 2)
        textured = definition(source, "    [[nodiscard]] inline bool render_mesh_base_color_preview(")
        self.assertIn("matchedScene ? matchedScene->solids", textured)
        sampled = definition(source, "    inline void render_engine_arcade_sampled_surface_preview(")
        self.assertIn("sampled_render_surface_markers_for(*matchedScene)", sampled)
        self.assertIn("sampled_render_surface_markers_for(ctx)", sampled)

    def test_actual_depth_wire_pass_and_state_restoration(self):
        path = ROOT / "Engine/modules/opengl.preview.ixx"
        source = path.read_text(encoding="utf-8")
        scope = definition(source, "        struct ScopedPreviewGLState final", True)
        draw = definition(source, "    inline void render_mesh_wire_preview(")
        code = r'''
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>
using GLenum=unsigned;using GLuint=unsigned;using GLint=int;using GLsizei=int;
using GLboolean=unsigned char;using GLfloat=float;using GLsizeiptr=std::ptrdiff_t;
constexpr GLboolean GL_FALSE=0,GL_TRUE=1;
enum : GLenum {GL_BLEND=2,GL_CULL_FACE,GL_SCISSOR_TEST,GL_DEPTH_TEST,GL_DEPTH_WRITEMASK,
 GL_COLOR_WRITEMASK,GL_POLYGON_OFFSET_FILL,GL_POLYGON_OFFSET_FACTOR,GL_POLYGON_OFFSET_UNITS,
 GL_DEPTH_FUNC,GL_VIEWPORT,GL_SCISSOR_BOX,GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,
 GL_ARRAY_BUFFER_BINDING,GL_ELEMENT_ARRAY_BUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,
 GL_READ_FRAMEBUFFER_BINDING,GL_ACTIVE_TEXTURE,GL_TEXTURE_BINDING_2D,GL_COLOR_CLEAR_VALUE,
 GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER,GL_DRAW_FRAMEBUFFER,GL_READ_FRAMEBUFFER,
 GL_TEXTURE_2D,GL_LESS,GL_LEQUAL,GL_EQUAL,GL_DYNAMIC_DRAW,GL_TRIANGLES,GL_LINES,
 GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,
 GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA,GL_FUNC_ADD,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE,
 GL_CULL_FACE_MODE,GL_FRONT_FACE,GL_POLYGON_MODE,GL_BACK,GL_FRONT,GL_CW,GL_CCW,GL_FRONT_AND_BACK,GL_FILL,GL_LINE};
struct State {std::set<GLenum> enabled;std::map<GLenum,std::vector<GLint>> integers;
 std::array<GLboolean,4> color{};GLboolean depth{};std::array<float,4> clear{};
 float offsetFactor{},offsetUnits{};bool operator==(const State&)const=default;};
State state;std::size_t uploads{};
struct Draw {GLenum topology;GLsizei count;std::array<GLboolean,4> color;bool depthWrite,depthTest,cull,offset,blend,wire;
 GLenum cullMode,winding,polygonMode,depthFunc;};
std::vector<Draw> draws;std::size_t failDraw{};bool wireEdges{};
GLboolean glIsEnabled(GLenum e){return state.enabled.contains(e);}
void glEnable(GLenum e){state.enabled.insert(e);}void glDisable(GLenum e){state.enabled.erase(e);}
void glGetBooleanv(GLenum e,GLboolean* v){if(e==GL_DEPTH_WRITEMASK)*v=state.depth;else std::copy(state.color.begin(),state.color.end(),v);}
void glGetIntegerv(GLenum e,GLint* v){const auto& a=state.integers.at(e);std::copy(a.begin(),a.end(),v);}
void glGetFloatv(GLenum e,GLfloat* v){if(e==GL_COLOR_CLEAR_VALUE)std::copy(state.clear.begin(),state.clear.end(),v);else *v=e==GL_POLYGON_OFFSET_FACTOR?state.offsetFactor:state.offsetUnits;}
void glUseProgram(GLuint v){state.integers[GL_CURRENT_PROGRAM]={int(v)};}
void glBindVertexArray(GLuint v){state.integers[GL_VERTEX_ARRAY_BINDING]={int(v)};}
void glBindBuffer(GLenum e,GLuint v){state.integers[e==GL_ARRAY_BUFFER?GL_ARRAY_BUFFER_BINDING:GL_ELEMENT_ARRAY_BUFFER_BINDING]={int(v)};}
void glBindFramebuffer(GLenum e,GLuint v){state.integers[e==GL_DRAW_FRAMEBUFFER?GL_DRAW_FRAMEBUFFER_BINDING:GL_READ_FRAMEBUFFER_BINDING]={int(v)};}
void glActiveTexture(GLenum v){state.integers[GL_ACTIVE_TEXTURE]={int(v)};}
void glBindTexture(GLenum,GLuint v){state.integers[GL_TEXTURE_BINDING_2D]={int(v)};}
void glViewport(GLint x,GLint y,GLsizei w,GLsizei h){state.integers[GL_VIEWPORT]={x,y,w,h};}
void glScissor(GLint x,GLint y,GLsizei w,GLsizei h){state.integers[GL_SCISSOR_BOX]={x,y,w,h};}
void glClearColor(float a,float b,float c,float d){state.clear={a,b,c,d};}
void glCullFace(GLenum e){state.integers[GL_CULL_FACE_MODE]={int(e)};}
void glFrontFace(GLenum e){state.integers[GL_FRONT_FACE]={int(e)};}
void glPolygonMode(GLenum,GLenum e){state.integers[GL_POLYGON_MODE]={int(e),int(e)};}
void glDepthFunc(GLenum e){state.integers[GL_DEPTH_FUNC]={int(e)};}
void glDepthMask(GLboolean v){state.depth=v;}
void glColorMask(GLboolean a,GLboolean b,GLboolean c,GLboolean d){state.color={a,b,c,d};}
void glPolygonOffset(float a,float b){state.offsetFactor=a;state.offsetUnits=b;}
void glUniformMatrix4fv(GLint,GLsizei,GLboolean,const float*){}
void glUniform1i(GLint,GLint value){wireEdges=bool(value);}
void glBlendFuncSeparate(GLenum sr,GLenum dr,GLenum sa,GLenum da){state.integers[GL_BLEND_SRC_RGB]={int(sr)};
 state.integers[GL_BLEND_DST_RGB]={int(dr)};state.integers[GL_BLEND_SRC_ALPHA]={int(sa)};state.integers[GL_BLEND_DST_ALPHA]={int(da)};}
void glBlendEquationSeparate(GLenum rgb,GLenum alpha){state.integers[GL_BLEND_EQUATION_RGB]={int(rgb)};state.integers[GL_BLEND_EQUATION_ALPHA]={int(alpha)};}
void glBufferData(GLenum,GLsizeiptr,const void*,GLenum){++uploads;}
void glDrawArrays(GLenum e,GLint,GLsizei n){draws.push_back({e,n,state.color,bool(state.depth),bool(glIsEnabled(GL_DEPTH_TEST)),bool(glIsEnabled(GL_CULL_FACE)),bool(glIsEnabled(GL_POLYGON_OFFSET_FILL)),bool(glIsEnabled(GL_BLEND)),wireEdges,GLenum(state.integers[GL_CULL_FACE_MODE][0]),GLenum(state.integers[GL_FRONT_FACE][0]),GLenum(state.integers[GL_POLYGON_MODE][0]),GLenum(state.integers[GL_DEPTH_FUNC][0])});if(failDraw==draws.size())throw std::runtime_error("recorded draw failure");}
namespace epochengine::previewgrid {
struct Vertex{std::array<float,3>position{},color{};};
struct MeshWireDrawFrame{std::shared_ptr<const std::vector<Vertex>>depth_triangles,helper_lines;};}
namespace epochengine::openglstate{struct OpenGL4State{GLuint sceneShader=1,sceneMarkerVao=2,sceneMarkerVbo=3;GLint sceneMvpLoc=4,sceneWireLoc=5;std::size_t meshTrianglesSubmitted{},meshDrawCallsSubmitted{},meshUploadBytesSubmitted{};};}
namespace actual {namespace detail {using Mat4=std::array<float,16>;SCOPE}
DRAW
}
#define CHECK(...) do{if(!(__VA_ARGS__)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#__VA_ARGS__);return 91;}}while(false)
int main(){
 state.enabled={GL_BLEND,GL_CULL_FACE,GL_SCISSOR_TEST,GL_POLYGON_OFFSET_FILL};state.color={1,0,1,0};state.depth=0;
 state.offsetFactor=4;state.offsetUnits=-3;state.clear={.1f,.2f,.3f,.4f};
 for(auto e:{GL_DEPTH_FUNC,GL_CURRENT_PROGRAM,GL_VERTEX_ARRAY_BINDING,GL_ARRAY_BUFFER_BINDING,
  GL_ELEMENT_ARRAY_BUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_FRAMEBUFFER_BINDING,GL_ACTIVE_TEXTURE,GL_TEXTURE_BINDING_2D,
  GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA})state.integers[e]={int(e+100)};
 state.integers[GL_DEPTH_FUNC]={GL_LESS};
 state.integers[GL_CULL_FACE_MODE]={GL_FRONT};state.integers[GL_FRONT_FACE]={GL_CCW};
 state.integers[GL_POLYGON_MODE]={GL_LINE,GL_LINE};
 state.integers[GL_VIEWPORT]={5,6,700,800};state.integers[GL_SCISSOR_BOX]={9,10,600,500};
 const auto before=state;epochengine::openglstate::OpenGL4State gpu;actual::detail::Mat4 mvp{};
 epochengine::previewgrid::MeshWireDrawFrame frame;
 frame.depth_triangles=std::make_shared<const std::vector<epochengine::previewgrid::Vertex>>(36);
 {actual::detail::ScopedPreviewGLState preserve;actual::render_mesh_wire_preview(frame,gpu,mvp);}
 CHECK(state==before&&draws.size()==2&&uploads==1);CHECK(gpu.meshTrianglesSubmitted==24&&gpu.meshDrawCallsSubmitted==2&&gpu.meshUploadBytesSubmitted==36*sizeof(epochengine::previewgrid::Vertex));CHECK(draws[0].topology==GL_TRIANGLES&&draws[0].count==36);
 CHECK(draws[0].color==std::array<GLboolean,4>{0,0,0,0}&&draws[0].depthWrite&&draws[0].depthTest&&draws[0].cull&&!draws[0].offset);
 CHECK(!draws[0].wire&&draws[0].depthFunc==GL_LEQUAL&&draws[1].wire&&draws[1].blend&&draws[1].depthFunc==GL_EQUAL);
 CHECK(draws[1].topology==GL_TRIANGLES&&draws[1].count==36&&draws[1].depthTest&&!draws[1].depthWrite&&draws[1].cull&&!draws[1].offset);
 CHECK(draws[1].color==std::array<GLboolean,4>{1,1,1,1});
 CHECK(draws[0].cullMode==GL_BACK&&draws[1].cullMode==GL_BACK);
 CHECK(draws[0].winding==GL_CW&&draws[1].winding==GL_CW);
 CHECK(draws[0].polygonMode==GL_FILL&&draws[1].polygonMode==GL_FILL&&!wireEdges);
 draws.clear();failDraw=2;bool caught{};
 try{actual::detail::ScopedPreviewGLState preserve;actual::render_mesh_wire_preview(frame,gpu,mvp);}catch(const std::runtime_error&){caught=true;}
 CHECK(caught&&state==before&&draws.size()==2&&!wireEdges);failDraw=0;draws.clear();
 gpu.sceneWireLoc=-1;
 {actual::detail::ScopedPreviewGLState preserve;actual::render_mesh_wire_preview(frame,gpu,mvp);}
 CHECK(state==before&&draws.size()==2&&draws[0].offset&&!draws[1].offset);
 CHECK(draws[1].polygonMode==GL_LINE&&!draws[1].wire&&draws[1].depthFunc==GL_LEQUAL);
 draws.clear();
 {actual::detail::ScopedPreviewGLState preserve;actual::render_mesh_wire_preview({},gpu,mvp);}
 CHECK(draws.empty()&&state==before);return 0;
}
'''.replace("SCOPE", scope).replace("DRAW\n", draw + "\n", 1)
        report = ROOT / "Artifacts/Reports/mesh-hidden-wire-oct31" / ("gl-state-" + str(time.time_ns()))
        report.mkdir(parents=True)
        unit = report / "contract.cpp"
        unit.write_text(code, encoding="utf-8")
        records = compile_and_run(self, unit, report)
        (report / "result.json").write_text(json.dumps({"scope": "Actual GL pass/state owners with recording ports; success, empty and interrupted draw restoration; no driver/pixels", "records": records}, indent=2), encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
