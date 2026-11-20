"""Execute actual mesh texture sampler scope with GL recording ports; no GPU."""
from pathlib import Path
import unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.cpp import compile_and_run
ROOT=Path(__file__).resolve().parents[2]
class MeshTextureSampler(unittest.TestCase):
    def test_inherited_sampler_is_neutralized_and_restored(self):
        source=(ROOT/'Engine/modules/opengl/opengl.preview.ixx').read_text(encoding='utf-8')
        scope=definition(source,'        struct TextureZeroScope final',True)
        code=r'''
#include <cstdlib>
#include <stdexcept>
using GLint=int;using GLenum=unsigned;using GLuint=unsigned;
enum {GL_ACTIVE_TEXTURE=1,GL_CULL_FACE_MODE,GL_FRONT_FACE,GL_TEXTURE0=10,GL_TEXTURE_BINDING_2D=20,GL_SAMPLER_BINDING,GL_TEXTURE_2D};
int active=12,cull=31,winding=32,samplers[3]={51,52,53},textures[3]={61,62,63};
int GLAD_GL_VERSION_3_3=1;int calls=0;
void check(bool value){if(!value)std::exit(7);}
void glGetIntegerv(GLenum what,GLint*out){switch(what){case GL_ACTIVE_TEXTURE:*out=active;break;case GL_CULL_FACE_MODE:*out=cull;break;case GL_FRONT_FACE:*out=winding;break;case GL_TEXTURE_BINDING_2D:*out=textures[active-10];break;case GL_SAMPLER_BINDING:*out=samplers[active-10];break;default:std::exit(8);}}
void glActiveTexture(GLenum value){active=int(value);}void glFrontFace(GLenum value){winding=int(value);}void glCullFace(GLenum value){cull=int(value);}void glBindTexture(GLenum,GLuint value){textures[active-10]=int(value);}
void bindSampler(GLuint unit,GLuint value){samplers[unit]=int(value);++calls;}auto glBindSampler=&bindSampler;
SCOPE
int main(){
 for(int available:{0,1}){GLAD_GL_VERSION_3_3=available;calls=0;
  try{TextureZeroScope scope;check(active==10);check(samplers[0]==(available?0:51));
   glBindTexture(GL_TEXTURE_2D,100);glFrontFace(99);glCullFace(98);glActiveTexture(11);throw 1;
  }catch(int){}
  check(active==12&&textures[0]==61&&textures[1]==62&&samplers[0]==51&&samplers[2]==53&&cull==31&&winding==32);
  check(calls==(available?2:0));
 }
 glBindSampler=nullptr;{TextureZeroScope scope;check(samplers[0]==51);}check(active==12);
}
'''.replace('SCOPE',scope)
        report=ROOT/'Artifacts/Reports/mesh-texture-sampler-nov02';report.mkdir(parents=True,exist_ok=True)
        unit=report/'contract.cpp';unit.write_text(code,encoding='utf-8');compile_and_run(self,unit,report)
