"""Actual cloud packet/time authority and native precision; no GPU/model launch."""
from pathlib import Path
import json,os,re,subprocess,time,unittest
from Tests.build.test_opengl_sprite_stream import definition
from Tools.build.generate_vulkan_cloud_shader import extract_cloud_trace
ROOT=Path(__file__).resolve().parents[2]
CLOUD_SHADER_OWNER=ROOT/'Engine/src/render/render.cloud_shader.hpp'


def desktop_cloud_shader_source():
    source=CLOUD_SHADER_OWNER.read_text(encoding='utf-8')
    return re.sub(r'^#ifdef GL_ES\n.*?(?:^#else\n(.*?))?^#endif\n',
                  lambda branch:branch.group(1) or '',source,flags=re.S|re.M)

class CloudVolumeRendering(unittest.TestCase):
    def test_portable_weather_lattice_matches_cpu(self):
        from Tests.support.cpp import compile_and_run
        cloud = extract_cloud_trace(CLOUD_SHADER_OWNER.read_text(encoding="utf-8"))
        shader = definition(cloud, "uint hashWord(") + "\n" + definition(cloud, "float lattice(")
        # API 31 SwiftShader 4 clamps larger source literals to INT_MAX.
        for literal in re.findall(r'\b0x([0-9a-fA-F]+)[uU]\b', shader):
            self.assertLessEqual(int(literal, 16), 0x7fffffff)
        source = (ROOT / "Engine/modules/asset.cloud_volume.ixx").read_text(encoding="utf-8")
        cpu = definition(source, "inline std::uint32_t hash(") + "\n" + definition(source, "inline double lattice(")
        fixture = ROOT / "Artifacts/Reports/cloud-lattice" / str(time.time_ns())
        fixture.mkdir(parents=True)
        unit = fixture / "cloud.lattice.cpp"
        unit.write_text("""
#include <cmath>
#include <cstdint>
#include <initializer_list>
namespace cpu {
""" + cpu + """
}
using uint = std::uint32_t;
struct ivec3 { int x, y, z; };
""" + shader + """
int main() {
    for (int y = -75; y < 108; ++y) {
        for (int x = -43; x < 46; ++x) {
            for (uint seed : {0u, 2989955058u, 0xffffffffu}) {
                if (std::abs(lattice({x, y, -22}, seed) - cpu::lattice(x, y, -22, seed)) > 6e-8)
                    return 1;
            }
        }
    }
}
""", encoding="utf-8")
        compile_and_run(self, unit, fixture)

    def test_android_cloud_frame_can_recover_without_recreating_context(self):
        from Tests.support.cpp import compile_and_run
        owner = ROOT / 'Engine/src/renderers/sdl/sdl.android_scene.cpp'
        source = owner.read_text(encoding='utf-8')
        start = source.index('namespace preview = epochengine::previewgrid;', source.index('bool begin('))
        end = source.index('glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING', start)
        # Exercise admission on one persistent pass with a stand-in for GLES sample count.
        # The next GL query marks entry into rendering; no framebuffer is needed here.
        code = r'''
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>
namespace epochengine::previewgrid {
struct Marker { bool wireframe{}; };
struct Scene { std::vector<bool> clouds{true}; std::vector<Marker> markers; int camera{}; };
std::optional<int> cloud_volume_view(bool admitted, int) {
    return admitted ? std::optional{1} : std::nullopt;
}
}
using GLint = int;
constexpr int GL_SAMPLES = 0;
int sampleCount{};
void glGetIntegerv(int, GLint* result) { *result = sampleCount; }
struct Pass {
    bool failed_{};
    bool refuse(const char*) { failed_ = true; return false; }
    bool begin(const epochengine::previewgrid::Scene& scene, int width, int height) {
        ADMISSION
        return true;
    }
};
int main() {
    Pass pass;
    epochengine::previewgrid::Scene scene;
    if (!pass.begin(scene, 800, 600)) return 1;
    scene.clouds.push_back(true);
    if (pass.begin(scene, 800, 600)) return 2;
    scene.clouds = {false};
    if (pass.begin(scene, 800, 600)) return 3;
    scene.clouds = {true};
    if (pass.begin(scene, 4000, 4000)) return 4;
    sampleCount = 4;
    if (pass.begin(scene, 800, 600)) return 5;
    sampleCount = 0;
    if (!pass.begin(scene, 800, 600)) return 6;
    pass.failed_ = true;
    return pass.begin(scene, 800, 600) ? 7 : 0;
}
'''.replace('ADMISSION', source[start:end])
        report = ROOT / 'Artifacts/Reports/android-cloud-admission' / str(time.time_ns())
        report.mkdir(parents=True)
        fixture = report / 'cloud.admission.cpp'
        fixture.write_text(code, encoding='utf-8')
        compile_and_run(self, fixture, report)

    def test_actual_preview_pass_readback_restores_gl_state(self):
        from Tests.support.cpp import compile_and_run
        import hashlib
        owner=ROOT/'Engine/modules/opengl/opengl.preview.ixx'
        source=owner.read_text(encoding='utf-8')
        start=source.index('    namespace detail\n    {\n        // A radius ratio')
        end=source.index('    inline void draw_planetary_sky(',start)
        implementation=source[start:end].replace('std::chrono::steady_clock','ProbeClock')
        render=source[end:]
        checkpoints=('"read("clear")"', '"read("sky")"', '"read("plain-opaque")"', '"read("textured-and-sampled")"',
                     '"read(cloudRendered?"cloud-complete":"cloud-refused")"', '"read("scene-helpers")"')
        positions=[render.index(stage[1:-1]) for stage in checkpoints]
        self.assertEqual(positions,sorted(positions))
        code=r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <type_traits>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
namespace core {
struct Context {};
namespace env {
std::string pixel;
std::optional<std::string> get(std::string_view name){
 if(name=="EPOCH_OPENGL_PREVIEW_SAMPLE_PIXEL")return pixel;
 if(name=="EPOCH_OPENGL_PREVIEW_SAMPLE_DWELL_MS")return "1500";
 if(name=="EPOCH_OPENGL_PREVIEW_SAMPLE_RADIUS")return "1.000841";
 return std::nullopt;
}}
}
namespace render_camera {enum class ProjectionKind{perspective,orthographic};}
namespace previewgrid {
struct Vec{float x{},y{},z{};};
struct CloudVolume{
 std::array<double,3> center{};double radius{},meters_per_world_unit{1.0};
 struct {double view_occluder_radius_meters{};std::uint32_t seed{};} recipe;
};
struct SolidDrawFrame{std::shared_ptr<const int> geometry;};
struct PublishedSceneFrame {
 struct {Vec eye,target,up;float fovRadians{},nearPlane{},farPlane{};
  render_camera::ProjectionKind projection{};} camera;
 std::vector<CloudVolume> clouds;
 std::shared_ptr<const SolidDrawFrame> solids;
 std::array<double,3> render_origin{};
 std::uint64_t revision{};
};
}
struct ProbeClock{
 using time_point=std::chrono::steady_clock::time_point;
 inline static time_point value{};
 static time_point now(){return value;}
};
namespace logger {
enum class LogLevel{INFO};
struct Log {unsigned records{};template<class... T> void logf(LogLevel,std::source_location,const char*,T...){++records;}};
Log& get(const char*){static Log result;return result;}
}
using GLint=int;using GLuint=unsigned;using GLenum=unsigned;using GLubyte=unsigned char;using GLfloat=float;
enum : GLenum {GL_NONE,GL_READ_FRAMEBUFFER_BINDING,GL_DRAW_FRAMEBUFFER_BINDING,GL_DRAW_BUFFER0,
 GL_READ_FRAMEBUFFER,GL_DRAW_FRAMEBUFFER,GL_READ_BUFFER,GL_PIXEL_PACK_BUFFER_BINDING,GL_PACK_ALIGNMENT,
 GL_PACK_ROW_LENGTH,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_SWAP_BYTES,GL_PIXEL_PACK_BUFFER,
 GL_SAMPLES,GL_DEPTH_ATTACHMENT,GL_DEPTH,GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,GL_FRONT_AND_BACK,
 GL_RGBA,GL_UNSIGNED_BYTE,GL_DEPTH_COMPONENT,GL_FLOAT,GL_FALSE=0};
int activeRead=1,activeDraw=2,drawBuffer=19,packBuffer=55,samples=0;
std::array readBuffers{0,15,16};
std::array pack{4,21,11,12,1};
unsigned reads{};
void glGetIntegerv(GLenum name,GLint* out){
 if(name>=GL_PACK_ALIGNMENT&&name<=GL_PACK_SWAP_BYTES){*out=pack[name-GL_PACK_ALIGNMENT];return;}
 switch(name){
 case GL_READ_FRAMEBUFFER_BINDING:*out=activeRead;return;
 case GL_DRAW_FRAMEBUFFER_BINDING:*out=activeDraw;return;
 case GL_DRAW_BUFFER0:*out=drawBuffer;return;
 case GL_READ_BUFFER:*out=readBuffers[activeRead];return;
 case GL_PIXEL_PACK_BUFFER_BINDING:*out=packBuffer;return;
 case GL_SAMPLES:*out=samples;return;
 default:std::abort();
 }
}
void glBindFramebuffer(GLenum target,GLuint fbo){if(target!=GL_READ_FRAMEBUFFER)std::abort();activeRead=int(fbo);}
void glReadBuffer(GLenum selector){readBuffers[activeRead]=int(selector);}
void glBindBuffer(GLenum target,GLuint buffer){if(target!=GL_PIXEL_PACK_BUFFER)std::abort();packBuffer=int(buffer);}
void glPixelStorei(GLenum name,GLint value){pack.at(name-GL_PACK_ALIGNMENT)=value;}
void glGetFramebufferAttachmentParameteriv(GLenum target,GLenum attachment,GLenum parameter,GLint* result){
 if(target!=GL_DRAW_FRAMEBUFFER||attachment!=GL_DEPTH_ATTACHMENT||parameter!=GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE)std::abort();
 *result=24;
}
void glReadPixels(int x,int y,int w,int h,GLenum format,GLenum type,void* destination){
 constexpr std::array<std::array<int,2>,6> expected{{{60,60},{35,40},{85,40},{35,80},{85,80},{29,52}}};
 if(w!=1||h!=1||activeRead!=activeDraw||readBuffers[activeRead]!=drawBuffer||packBuffer
  ||pack!=std::array{1,0,0,0,0}||expected.at(reads/2u)!=std::array{x,y})std::abort();
 if(reads++%2u){if(format!=GL_DEPTH_COMPONENT||type!=GL_FLOAT)std::abort();*static_cast<float*>(destination)=0.9f;}
 else if(format!=GL_RGBA||type!=GL_UNSIGNED_BYTE)std::abort();
}
IMPLEMENTATION
int main(){
 core::Context context;
 previewgrid::PublishedSceneFrame scene;
 scene.camera.target={0,0,-1};
 scene.camera.up={0,1,0};
 scene.solids=std::make_shared<const previewgrid::SolidDrawFrame>();
 const double radius=detail::configured_preview_probe_radius();
 const detail::PreviewProbeViewport viewport{10,20,100,80};
 const auto claim=[&](){return detail::claim_preview_probe(&context,scene,viewport,radius);};
 auto& log=logger::get("");
 CHECK(radius==1.000841&&detail::configured_preview_probe_radius()==radius&&log.records==1);
 CHECK(!claim()&&!claim()&&log.records==2);
 scene.clouds.push_back({{0,0,-7000000},6371000,1.0,{6349751,1337}});
 CHECK(!claim()&&!claim()&&log.records==3);
 scene.clouds[0].center[2]=-6376359;
 CHECK(!claim()&&!claim()&&log.records==4);
 const auto next=[&](){
  ProbeClock::value+=std::chrono::milliseconds(125);
  ++scene.revision;
  return claim();
 };
 for(unsigned i=0;i<4;++i)CHECK(!next());
 // Actual movement resets the dwell, even while still inside the radius band.
 scene.camera.eye.x+=10;scene.camera.target.x+=10;
 CHECK(!next()&&!next());
 // A separate heading change also resets it.
 scene.camera.target.x+=0.02f;
 CHECK(!next());
 for(unsigned i=0;i<5;++i){
  scene.camera.eye.x+=0.05f;scene.camera.target.x+=0.05f;
  scene.camera.nearPlane+=0.001f;scene.camera.farPlane+=100.0f;
  scene.camera.fovRadians+=0.0001f;
  scene.solids=std::make_shared<const previewgrid::SolidDrawFrame>();
  CHECK(!next());
 }
 // Rebasing changes both camera and cloud coordinates, preserving the pose.
 scene.camera.eye.x+=20;scene.camera.target.x+=20;scene.clouds[0].center[0]+=20;
 CHECK(detail::configured_preview_probe_dwell()==std::chrono::milliseconds(1500));
 for(unsigned i=0;i<6;++i)CHECK(!next());
 CHECK(next()==&scene.clouds[0]&&!claim()&&log.records==4);
 const detail::PreviewPassProbe probe{viewport,11};
 const auto restored=[](){return activeRead==1&&activeDraw==2&&readBuffers==std::array{0,15,16}
  &&packBuffer==55&&pack==std::array{4,21,11,12,1};};
 CHECK(probe.read("clear")&&restored()&&reads==10&&log.records==9);
 reads=0;core::env::pixel="19,32";
 CHECK(probe.read("targeted")&&restored()&&reads==12&&log.records==15);
 reads=0;core::env::pixel="100,32";
 CHECK(probe.read("out-of-bounds")&&restored()&&reads==10&&log.records==20);
 samples=4;
 CHECK(!probe.read("unsupported")&&restored()&&reads==10&&log.records==21);
}
'''.replace('IMPLEMENTATION',implementation)
        report=ROOT/'Artifacts/Reports/opengl-preview-pass-probe'/hashlib.sha256(code.encode()).hexdigest()[:16]
        report.mkdir(parents=True,exist_ok=True)
        fixture=report/'preview.pass_probe.cpp'
        fixture.write_text(code,encoding='utf-8')
        records=compile_and_run(self,fixture,report)
        (report/'result.json').write_text(json.dumps(records,indent=2),encoding='utf-8')

    def test_actual_cloud_light_is_eclipsed_by_planet(self):
        from Tests.support.cpp import compile_and_run
        import hashlib
        owner=CLOUD_SHADER_OWNER
        native=desktop_cloud_shader_source()
        fragment=extract_cloud_trace(native)
        illumination=definition(fragment,'vec3 cloudIllumination(')
        self.assertIn('vec3 illumination=cloudIllumination(samplePosition,ray,footprint,extinction);',fragment)
        illumination=re.sub(r'(?<![\w.])(\d+\.\d+)(?![\w.])',r'\1f',illumination)
        code=r'''
#include <algorithm>
#include <cmath>
#include <initializer_list>
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
struct vec3 {
 float x,y,z;
 explicit vec3(float v):x(v),y(v),z(v){}
 vec3(float a,float b,float c):x(a),y(b),z(c){}
};
vec3 operator+(vec3 a,vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
vec3 operator-(vec3 a,float b){return {a.x-b,a.y-b,a.z-b};}
vec3 operator*(vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
vec3 operator*(float a,vec3 b){return b*a;}
vec3 operator*(vec3 a,vec3 b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
float dot(vec3 a,vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float inversesqrt(float value){return 1.0f/std::sqrt(value);}
vec3 sqrt(vec3 a){return {std::sqrt(a.x),std::sqrt(a.y),std::sqrt(a.z)};}
vec3 max(vec3 a,vec3 b){return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
vec3 clamp(vec3 a,vec3 lo,vec3 hi){
 return {std::clamp(a.x,lo.x,hi.x),std::clamp(a.y,lo.y,hi.y),std::clamp(a.z,lo.z,hi.z)};
}
vec3 mix(vec3 a,vec3 b,float weight){
 return {std::lerp(a.x,b.x,weight),std::lerp(a.y,b.y,weight),std::lerp(a.z,b.z,weight)};
}
float smoothstep(float start,float end,float value){
 const float t=std::clamp((value-start)/(end-start),0.0f,1.0f);
 return t*t*(3.0f-2.0f*t);
}
using std::clamp;using std::exp;using std::max;using std::pow;using std::sqrt;
vec3 uSun{0,1,0},uAmbient{.05f,.08f,.12f},uSunColor{1,.9f,.8f};
float uExtinction=.001f,uLowerExtinction=.00275f,uNoiseScale=500.0f,uBase=.001f,uThickness=.001f,uRadius=6371000.0f;
unsigned probes{};
bool finiteDeck{},separatedDecks{},upperCore{};
float cloudExtinction(vec3 p,float,float){
 ++probes;
 if(separatedDecks){
  const float height=(std::sqrt(dot(p,p))-1.0f-uBase)/uThickness;
  if(height<.52f)return .2f*uExtinction;
  if(upperCore&&height>.72f&&height<.84f)return uExtinction;
  return 0.0f;
 }
 return !finiteDeck||p.y<1.0018f?.2f*uExtinction:0.0f;
}
SHADER_OWNER
bool same(vec3 a,vec3 b){
 return std::abs(a.x-b.x)<1e-6f&&std::abs(a.y-b.y)<1e-6f&&std::abs(a.z-b.z)<1e-6f;
}
// Independent oracle measures the squared closest distance along the forward ray.
bool planet_blocks_sun(vec3 p,vec3 sun){
 const double t=-(double(p.x)*sun.x+double(p.y)*sun.y+double(p.z)*sun.z);
 if(t<=0.0)return false;
 const double x=p.x+t*sun.x,y=p.y+t*sun.y,z=p.z+t*sun.z;
 return x*x+y*y+z*z<=1.0;
}
vec3 sun_at(float vertical){return {std::sqrt(1.0f-vertical*vertical),vertical,0};}
int main(){
 const vec3 viewRay{0,0,-1},cloud{0,1.00125f,0};
 const float equalExtinction=.2f*uExtinction;
 CHECK(!planet_blocks_sun(cloud,uSun));
 const auto day=cloudIllumination(cloud,viewRay,.02f,equalExtinction);
 CHECK(day.x>uAmbient.x&&day.y>uAmbient.y&&day.z>uAmbient.z&&probes==3);
 finiteDeck=true;
 const auto underside=cloudIllumination({0,1.0009f,0},viewRay,.02f,equalExtinction);
 const auto top=cloudIllumination({0,1.0017f,0},viewRay,.02f,equalExtinction);
 CHECK(top.x>underside.x&&top.y>underside.y&&top.z>underside.z);
 finiteDeck=false;
 uSun={0,-1,0};probes=0;
 CHECK(planet_blocks_sun(cloud,uSun));
 CHECK(same(cloudIllumination(cloud,viewRay,.02f,equalExtinction),uAmbient)&&probes==0);
 uSun=sun_at(-.025f);
 CHECK(planet_blocks_sun({0,1,0},uSun)&&!planet_blocks_sun(cloud,uSun));
 const auto twilight=cloudIllumination(cloud,viewRay,.02f,equalExtinction);
 CHECK(twilight.x>uAmbient.x&&twilight.x<day.x&&probes==3);
 CHECK((twilight.x-uAmbient.x)/(day.x-uAmbient.x)<.14f);
 CHECK((twilight.z-uAmbient.z)/(day.z-uAmbient.z)<(twilight.x-uAmbient.x)/(day.x-uAmbient.x));
 uSun={uSun.y,uSun.x,0};
 CHECK(same(cloudIllumination({cloud.y,0,0},viewRay,.02f,equalExtinction),twilight));
 float previous=uAmbient.x;
 for(float elevation:{-.08f,-.054f,-.050f,-.046f,-.025f}){
  uSun=sun_at(elevation);
  const float brightness=cloudIllumination(cloud,viewRay,.02f,equalExtinction).x;
  CHECK(brightness>=previous&&brightness<=twilight.x);
  if(elevation==-.054f||elevation==-.050f||elevation==-.046f)
   CHECK(brightness>previous&&brightness<twilight.x);
  previous=brightness;
 }
 CHECK(std::abs(previous-twilight.x)<1e-6f);
 // A compact upper core casts a shadow through the empty inter-deck gap.
 // The legacy uniform taps miss it from this lower altitude.
 const vec3 lowerCore{0,1.0011f,0};
 separatedDecks=true;
 probes=0;uSun={0,1,0};
 const auto clearOverhead=cloudIllumination(lowerCore,viewRay,.02f,equalExtinction);
 upperCore=true;
 const auto shadedOverhead=cloudIllumination(lowerCore,viewRay,.02f,equalExtinction);
 CHECK(probes==6u&&shadedOverhead.x+0.08f<clearOverhead.x);
 // The two-deck estimate blends in above the local horizon without a jump.
 uSun=sun_at(-.00001f);
 const auto before=cloudIllumination(lowerCore,viewRay,.02f,equalExtinction);
 uSun=sun_at(.00001f);
 const auto after=cloudIllumination(lowerCore,viewRay,.02f,equalExtinction);
 CHECK(std::abs(after.x-before.x)<0.0001f);
 separatedDecks=false;uSun={0,1,0};probes=0;
 const auto neutral=cloudIllumination(cloud,viewRay,.000004f,equalExtinction);
 const auto opening=cloudIllumination(cloud,viewRay,.000004f,.8f*uExtinction);
 const auto blocked=cloudIllumination(cloud,viewRay,.000004f,.05f*uExtinction);
 CHECK(probes==9u&&opening.x>neutral.x&&blocked.x<neutral.x);
 CHECK(same(cloudIllumination(cloud,viewRay,.02f,.8f*uExtinction),day));
 const vec3 upper{0,1.0017f,0};
 CHECK(same(cloudIllumination(upper,viewRay,.000004f,equalExtinction),
            cloudIllumination(upper,viewRay,.000004f,.8f*uExtinction)));
 return 0;
}
'''.replace('SHADER_OWNER',illumination)
        report=ROOT/'Artifacts/Reports/cloud-volume-rendering/sun-eclipse'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'cloud.sun_eclipse.cpp';source.write_text(code,encoding='utf-8')
        receipt=dict(owner=owner.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(owner.read_bytes()).hexdigest(),passed=False)
        result=report/'result.json';result.write_text(json.dumps(receipt,indent=2),encoding='utf-8')
        receipt['records']=compile_and_run(self,source,report,windows_flags=('/W4','/WX'),linux_flags=('-Wall','-Wextra','-Werror'))
        receipt['passed']=True;result.write_text(json.dumps(receipt,indent=2),encoding='utf-8')

    def test_actual_half_resolution_rays_match_opaque_depth_texels(self):
        from Tests.support.cpp import compile_and_run
        import hashlib
        owner=CLOUD_SHADER_OWNER
        native=desktop_cloud_shader_source()
        fragment=native.split('inline constexpr std::string_view fragment_source=R"glsl(',1)[1].split(')glsl";',1)[0]
        tracer=extract_cloud_trace(native)
        composite=native.split('inline constexpr std::string_view composite_source=R"glsl(',1)[1].split(')glsl";',1)[0]
        body=definition(fragment,'void main(){').split('{',1)[1]
        setup=body[:body.index('// Derivatives must')]
        sampling=(body[body.index('float depth='):body.index('vec4 value=traceCloud(')]
                  +tracer[tracer.index('float rayProjection='):tracer.index('vec2 outer,inner;')])
        coordinates='struct {vec2 xy;} gl_FragCoord{fragmentPosition};'
        actual=(definition(tracer,'float viewDepth(')+'\nSnapshot sample(vec2 fragmentPosition){'
                +coordinates+setup+sampling+'return {origin,ray,opaque};}')
        compose=definition(composite,'void main(){').split('{',1)[1].split('if(uEncodeSrgb',1)[0]
        actual+='\nvec4 composite_sample(vec2 vUv){'+compose+'return value;}'
        sky_guard='if(targetZ<1.0&&sourceZ>=1.0){brightSkyTap=brightSkyTap||value.a>0.00001;continue;}'
        self.assertIn(sky_guard,compose)
        actual+='\nvec4 old_composite_sample(vec2 vUv){'+compose.replace(sky_guard,'')+'return value;}'
        # Negative control restores the old half-resolution ray location while
        # retaining the same full-resolution depth texel, which caused holes.
        self.assertIn('sampleUv*2.0-1.0',setup)
        legacy=setup.replace('sampleUv*2.0-1.0','vUv*2.0-1.0')+'(void)sampleUv;'
        actual+=('\nSnapshot legacy_sample(vec2 fragmentPosition,vec2 vUv){'
                 +coordinates+legacy+sampling+'return {origin,ray,opaque};}')
        actual=re.sub(r'(?<![\w.])(\d+\.\d+(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)(?![\w.])',r'\1f',actual)
        code=r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
struct ivec2;
struct vec2 {float x{},y{};vec2()=default;vec2(float a,float b):x(a),y(b){};explicit vec2(const ivec2&);};
struct ivec2 {int x{},y{};ivec2()=default;ivec2(int a,int b):x(a),y(b){};explicit ivec2(int a):x(a),y(a){};explicit ivec2(vec2 a):x(static_cast<int>(a.x)),y(static_cast<int>(a.y)){};};
vec2::vec2(const ivec2& a):x(static_cast<float>(a.x)),y(static_cast<float>(a.y)){}
vec2 operator*(vec2 a,vec2 b){return {a.x*b.x,a.y*b.y};}
vec2 operator/(vec2 a,vec2 b){return {a.x/b.x,a.y/b.y};}
vec2 operator*(vec2 a,float b){return {a.x*b,a.y*b};}
vec2 operator+(vec2 a,float b){return {a.x+b,a.y+b};}
vec2 operator-(vec2 a,float b){return {a.x-b,a.y-b};}
ivec2 operator+(ivec2 a,ivec2 b){return {a.x+b.x,a.y+b.y};}
ivec2 operator-(ivec2 a,ivec2 b){return {a.x-b.x,a.y-b.y};}
ivec2 operator*(ivec2 a,int b){return {a.x*b,a.y*b};}
ivec2 operator%(ivec2 a,ivec2 b){return {a.x%b.x,a.y%b.y};}
bool operator==(ivec2 a,ivec2 b){return a.x==b.x&&a.y==b.y;}
ivec2 clamp(ivec2 v,ivec2 a,ivec2 b){return {std::clamp(v.x,a.x,b.x),std::clamp(v.y,a.y,b.y)};}
vec2 floor(vec2 v){return {std::floor(v.x),std::floor(v.y)};}
vec2 fract(vec2 v){return {v.x-std::floor(v.x),v.y-std::floor(v.y)};}
struct vec3 {float x{},y{},z{};vec3()=default;explicit vec3(float v):x(v),y(v),z(v){};vec3(float a,float b,float c):x(a),y(b),z(c){};};
vec3 operator+(vec3 a,vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
vec3 operator-(vec3 a,vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
vec3 operator*(vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
vec3& operator/=(vec3& a,float b){a={a.x/b,a.y/b,a.z/b};return a;}
vec3& operator-=(vec3& a,vec3 b){a=a-b;return a;}
float dot(vec3 a,vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
vec3 cross(vec3 a,vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
vec3 normalize(vec3 a){return a*(1.0f/std::sqrt(dot(a,a)));}
float mix(float a,float b,float t){return a*(1.0f-t)+b*t;}
using std::abs;using std::exp;using std::max;
vec2 uProjection{},uNearFar{2.665f,13.5f};bool uOrthographic{};
vec3 uForward{},uRight{},uUp{},uCenter{};float uWorldRadius=1.0f;
constexpr int uDepth=1,uCloud=2;
ivec2 dimensions{};
ivec2 low_size(){return {(dimensions.x+1)/2,(dimensions.y+1)/2};}
ivec2 textureSize(int texture,int){return texture==uCloud?low_size():dimensions;}
struct vec4 {
 float r{},g{},b{},a{};
 explicit vec4(float single):r(single),g(single),b(single),a(single){}
 vec4(float red,float green,float blue,float alpha):r(red),g(green),b(blue),a(alpha){}
};
vec4 operator*(vec4 v,float s){return {v.r*s,v.g*s,v.b*s,v.a*s};}
vec4 operator/(vec4 v,float s){return {v.r/s,v.g/s,v.b/s,v.a/s};}
vec4& operator+=(vec4& v,vec4 other){v={v.r+other.r,v.g+other.g,v.b+other.b,v.a+other.a};return v;}
vec4 texelFetch(int,ivec2,int);
bool recordComposite{};
std::vector<ivec2> cloudTaps,depthTaps;
bool syntheticDepth{},supplyOpaqueTap{};
ivec2 opaqueTarget{},opaqueBacking{};
float opaqueDepth{};
bool uRepairEdges{};
float cloudAlpha{1.0f},exactAlpha{},tracedDepth{};
unsigned exactTraces{};
ivec2 tracedPixel{};
vec4 traceExactOpaque(ivec2 target,ivec2,float depth){
 ++exactTraces;tracedPixel=target;tracedDepth=depth;return {0,0,0,exactAlpha};
}
struct Snapshot {vec3 origin,ray;float opaque;};
SHADER_OWNERS
Snapshot expected_ray(ivec2 pixel){
 const float x=2.0f*(static_cast<float>(pixel.x)+.5f)/static_cast<float>(dimensions.x)-1.0f;
 const float y=2.0f*(static_cast<float>(pixel.y)+.5f)/static_cast<float>(dimensions.y)-1.0f;
 const auto offset=uRight*(x*uProjection.x)+uUp*(y*uProjection.y);
 return {uOrthographic?offset-uCenter:vec3{0,0,0}-uCenter,
  uOrthographic?uForward:normalize(uForward+offset),0};
}
// Double geometric oracle supplies the depth at each exact full-size pixel.
double sphere_entry(const Snapshot& s,double radius){
 const std::array<double,3> o{s.origin.x,s.origin.y,s.origin.z};
 std::array<double,3> d{s.ray.x,s.ray.y,s.ray.z};double norm{};
 for(double v:d)norm+=v*v;
 norm=std::sqrt(norm);
 for(double& v:d)v/=norm;
 double t{};for(unsigned a=0;a<3;++a)t-=o[a]*d[a];
 double square{};for(unsigned a=0;a<3;++a){const double v=o[a]+d[a]*t;square+=v*v;}
 if(square>=radius*radius)return std::numeric_limits<double>::infinity();
 return t-std::sqrt(radius*radius-square);
}
vec4 texelFetch(int texture,ivec2 pixel,int){
 if(texture==uCloud){
  cloudTaps.push_back(pixel);
  return {static_cast<float>(pixel.x),static_cast<float>(pixel.y),0,cloudAlpha};
 }
 if(recordComposite)depthTaps.push_back(pixel);
 if(syntheticDepth){
  const bool opaque=pixel==opaqueTarget||(supplyOpaqueTap&&pixel==opaqueBacking);
  return {opaque?opaqueDepth:1.0f,0,0,0};
 }
 const auto s=expected_ray(pixel);const double distance=sphere_entry(s,1.0);
 if(!std::isfinite(distance)||distance<=0)return {1.0f,0,0,0};
 const double z=distance*dot(s.ray,uForward),n=uNearFar.x,f=uNearFar.y;
 return {static_cast<float>(uOrthographic?(z-n)/(f-n):f*(z-n)/(z*(f-n))),0,0,0};
}
vec2 pixel_center(ivec2 pixel){return {static_cast<float>(pixel.x)+.5f,static_cast<float>(pixel.y)+.5f};}
// Rational nearest-texel definition for the old low-pixel center. No float
// projection or shader parity expression is shared with this oracle.
ivec2 backing_pixel(ivec2 low){
 const auto extent=low_size();
 return {(2*low.x+1)*dimensions.x/(2*extent.x),(2*low.y+1)*dimensions.y/(2*extent.y)};
}
bool same(vec3 a,vec3 b){return std::abs(a.x-b.x)<0.000001f&&std::abs(a.y-b.y)<0.000001f&&std::abs(a.z-b.z)<0.000001f;}
int main(){
 constexpr double earth=6371000.0,outer=1.0+10080.0/earth;
 uCenter={static_cast<float>(-4790643.376/earth),static_cast<float>(-2463881.276/earth),static_cast<float>(74775030.767/earth)};
 uForward=normalize({-.025679787f,-.065139893f,.997545660f});
 uRight=normalize(cross(uForward,{.439057f,.895744f,.0697949f}));uUp=cross(uRight,uForward);
 unsigned allLegacyHoles{};
 for(const auto size:{ivec2{1898,1000},ivec2{1899,1001},ivec2{1898,1001},ivec2{1899,1000}}){
  dimensions=size;const int lowWidth=(size.x+1)/2,lowHeight=(size.y+1)/2;
  uProjection={std::tan(.45f)*static_cast<float>(size.x)/static_cast<float>(size.y),std::tan(.45f)};
  const float centerZ=dot(uCenter,uForward);
  const int cx=static_cast<int>((dot(uCenter,uRight)/(centerZ*uProjection.x)+1)*.5f*static_cast<float>(lowWidth));
  const int cy=static_cast<int>((dot(uCenter,uUp)/(centerZ*uProjection.y)+1)*.5f*static_cast<float>(lowHeight));
  unsigned checked{},legacyHoles{};
  for(int y=cy-55;y<=cy+55;++y)for(int x=cx-55;x<=cx+55;++x){
   const auto position=pixel_center({x,y});
   const auto pixel=backing_pixel({x,y});
   const auto wanted=expected_ray(pixel),actual=sample(position),old=legacy_sample(position,position/vec2{low_size()});
   CHECK(same(actual.origin,wanted.origin)&&same(actual.ray,wanted.ray));
   const double terrain=sphere_entry(actual,1.0),entry=sphere_entry(actual,outer);
   if(!std::isfinite(terrain)||terrain<=0)continue;
   ++checked;CHECK(actual.opaque>entry); // Opaque sphere cannot cover its own atmosphere.
   const double oldEntry=sphere_entry(old,outer);
   if(std::isfinite(oldEntry)&&old.opaque<oldEntry)++legacyHoles;
  }
  CHECK(checked>500);if(size.x%2==0&&size.y%2==0)CHECK(legacyHoles>50);allLegacyHoles+=legacyHoles;
  std::printf("viewport=%dx%d checked=%u old_false_holes=%u aligned_false_holes=0\n",size.x,size.y,checked,legacyHoles);
  // Orthographic origins need the same pixel alignment, not only directions.
  uOrthographic=true;
  for(ivec2 low:{ivec2{117,217},ivec2{683,125}}){
   const auto actual=sample(pixel_center(low)),wanted=expected_ray(backing_pixel(low));
   CHECK(same(actual.origin,wanted.origin)&&same(actual.ray,wanted.ray));
  }
  uOrthographic=false;
  // The actual compositor recovers each traced sample's exact opaque depth
  // address, including clamped edge neighbors of a resized viewport.
  const ivec2 anchoredLow{cx,cy},anchoredTarget=backing_pixel(anchoredLow);
  recordComposite=true;
  for(ivec2 target:{ivec2{0,0},size-ivec2{1},anchoredTarget,anchoredTarget+ivec2{1,1}}){
   cloudTaps.clear();depthTaps.clear();
   const auto value=composite_sample(pixel_center(target)/vec2{size});
   CHECK(cloudTaps.size()==4&&depthTaps.size()==5&&depthTaps[0]==target);
   for(unsigned index=0;index<cloudTaps.size();++index){
    const auto low=cloudTaps[index];
    CHECK(low.x>=0&&low.y>=0&&low.x<lowWidth&&low.y<lowHeight);
    CHECK(depthTaps[index+1]==backing_pixel(low));
   }
   if(target==anchoredTarget)CHECK(value.r==static_cast<float>(cx)&&value.g==static_cast<float>(cy)&&value.a==1.0f);
  }
  recordComposite=false;
  // A one-pixel opaque silhouette can fall between all four backing rays.
  // Its neighboring sky clouds must not survive the nearest-sample fallback.
  syntheticDepth=true;opaqueTarget=anchoredTarget-ivec2{1};opaqueBacking=anchoredTarget;
  const auto silhouette=pixel_center(opaqueTarget)/vec2{size};
  const auto sky=pixel_center(anchoredTarget+ivec2{2})/vec2{size};
  for(float depth:{.75f,std::nextafter(1.0f,0.0f)}){
   opaqueDepth=depth;supplyOpaqueTap=false;
   CHECK(old_composite_sample(silhouette).a==1.0f);
   CHECK(composite_sample(silhouette).a==0.0f);
   CHECK(composite_sample(sky).a==1.0f);
   supplyOpaqueTap=true;
   const auto kept=composite_sample(silhouette);
   CHECK(kept.a==1.0f&&kept.r==static_cast<float>(cx)&&kept.g==static_cast<float>(cy));
   uRepairEdges=true;supplyOpaqueTap=false;exactTraces=0;exactAlpha=.7845f;
   CHECK(composite_sample(silhouette).a==exactAlpha&&exactTraces==1u
         &&tracedPixel==opaqueTarget&&tracedDepth==opaqueDepth);
   exactAlpha=0.0f;
   CHECK(composite_sample(silhouette).a==0.0f&&exactTraces==2u);
   supplyOpaqueTap=true;
   CHECK(composite_sample(silhouette).a==1.0f&&exactTraces==2u);
   supplyOpaqueTap=false;cloudAlpha=0.0f;
   CHECK(composite_sample(silhouette).a==0.0f&&exactTraces==2u);
   cloudAlpha=1.0f;uRepairEdges=false;
  }
  syntheticDepth=false;
  unsigned legacyBoundaryTies{};
  for(int full:{size.x,size.y}){
   const int low=(full+1)/2;
   for(int x=0;x<low;++x){
    // Force the two precision boundaries in this negative control under /O2.
    const volatile float uv=(static_cast<float>(x)+.5f)/static_cast<float>(low);
    const volatile float rayPosition=uv*static_cast<float>(full);
    const volatile double texturePosition=static_cast<double>(uv)*full;
    const int rayTexel=static_cast<int>(rayPosition);
    const int textureTexel=static_cast<int>(texturePosition);
    legacyBoundaryTies+=rayTexel!=textureTexel?1u:0u;
   }
  }
  CHECK(size.x%2==1&&size.y%2==1?legacyBoundaryTies==0:legacyBoundaryTies>100);
  std::printf("viewport=%dx%d old_normalized_boundary_ties=%u mismatched_composite_taps=0\n",size.x,size.y,legacyBoundaryTies);
 }
 CHECK(allLegacyHoles>200);return 0;
}
'''.replace('SHADER_OWNERS',actual)
        report=ROOT/'Artifacts/Reports/cloud-volume-rendering/depth-texel-provenance'/hashlib.sha256(code.encode()).hexdigest()[:16]
        report.mkdir(parents=True,exist_ok=True)
        source=report/'cloud.depth_ray.cpp';source.write_text(code,encoding='utf-8')
        receipt=dict(owner=owner.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(native.encode()).hexdigest(),passed=False)
        result=report/'result.json';result.write_text(json.dumps(receipt,indent=2),encoding='utf-8')
        receipt['records']=compile_and_run(self,source,report,windows_flags=('/W4','/WX'),linux_flags=('-Wall','-Wextra','-Werror'))
        receipt['passed']=True;result.write_text(json.dumps(receipt,indent=2),encoding='utf-8')

    def test_actual_mobile_depth_mapping_matches_trace_centers(self):
        from Tests.support.cpp import compile_and_run
        source=CLOUD_SHADER_OWNER.read_text(encoding='utf-8')
        center=definition(source,'ivec2 cloudTraceDepthPixel(').replace('ivec2','int')
        preceding=definition(source,'ivec2 cloudPrecedingTracePixel(').replace('ivec2','int')
        start=source.index(' ivec2 base=clamp(cloudPrecedingTracePixel(')
        end=source.index('\n fraction=vec2(',start)
        # GLSL integer/vector arithmetic here runs independently on each axis.
        bracket=source[start:end].replace('ivec2','int').replace('vec2','float')
        sdl=(ROOT/'Engine/src/renderers/sdl/sdl.android_scene.cpp').read_text(encoding='utf-8')
        self.assertIn('cloudTraceDepthPixel(tracePixel,depthSize,uTraceSize)', source)
        self.assertIn('glViewport(window.x, window.y, window.width, window.height)', sdl)
        self.assertIn('texture(cloudColor, GL_RGBA8, grid_.width, grid_.height,', sdl)
        grid=sdl[sdl.index('    static constexpr int maxTracePixels'):
                 sdl.index('[[nodiscard]] TraceRegion trace_region')]
        edge=re.sub(r'\b(\d+\.\d+)\b',r'\1f',definition(source,'float cloudTraceEdgeFraction('))
        code=r'''
#include <algorithm>
#include <array>
#include <cmath>
using std::clamp;
using std::max;
using std::min;
CENTER
PRECEDING
EDGE
struct Sample {int base,low,high; float fraction;};
Sample sample(int targetPixel,int depthSize,int cloudSize){
BRACKET
 return {base,low,high,fraction};
}
struct TraceRegion {int x{},y{},width{},height{};};
struct TraceGrid {TraceRegion window{};int width{},height{};};
GRID
int main(){
 constexpr std::array extents{std::array{1,1},std::array{9,1},std::array{18,18},
  std::array{13,6},std::array{128,63},std::array{1080,107},std::array{2211,221},
  std::array{16384,16383}};
 for(auto [pixels,cloud]:extents){
  int expectedBase=0;
  for(int p=0;p<pixels;++p){
   while(expectedBase+1<cloud && cloudTraceDepthPixel(expectedBase+1,pixels,cloud)<=p)
    ++expectedBase;
   const auto actual=sample(p,pixels,cloud);
   if(actual.base!=expectedBase || actual.low!=cloudTraceDepthPixel(expectedBase,pixels,cloud)
      || actual.high!=cloudTraceDepthPixel(min(expectedBase+1,cloud-1),pixels,cloud)
      || actual.low<0 || actual.high>=pixels || actual.fraction<0.0f || actual.fraction>1.0f)
    return 1;
   if(p>actual.low && p<actual.high
      && std::abs(std::lerp(float(actual.low),float(actual.high),actual.fraction)-float(p))>0.002f)
    return 2;
  }
 }
 const auto coversTaps=[](int from,int count,int pixels,int full,int offset,int extent){
  for(int p=from;p<from+count;++p){
   const int base=sample(p,pixels,full).base;
   if(base<offset||min(base+1,full-1)>=offset+extent)return false;
  }
  return true;
 };
 for(const auto [w,h]:{std::array{1080,2211},std::array{2211,1080},std::array{3,2161},
                      std::array{1,32768},std::array{32768,1},std::array{17,13}}){
  const auto full=trace_grid(w,h,{0,0,w,h});
  if(full.window.x||full.window.y||full.window.width!=full.width||full.window.height!=full.height
     ||full.width*full.height>maxTracePixels)return 7;
  for(const auto region:{TraceRegion{0,0,w,max(1,8*h/10)},TraceRegion{w/2,h/2,max(1,w/3),max(1,h/3)},
                        TraceRegion{w-1,h-1,1,1},TraceRegion{0,0,1,1}}){
   const auto grid=trace_grid(w,h,region);
   const auto window=grid.window;
   if(grid.width!=full.width||grid.height!=full.height||((window.x|window.y)&1)
      ||window.width<1||window.height<1||window.x+window.width>grid.width||window.y+window.height>grid.height)return 9;
   if(!coversTaps(region.x,region.width,w,grid.width,window.x,window.width)
      ||!coversTaps(region.y,region.height,h,grid.height,window.y,window.height))return 8;
  }
 }
 float previous=0.0f;
 for(int i=0;i<=1000;++i){
  const auto f=float(i)*0.001f;
  const auto weight=cloudTraceEdgeFraction(f);
  if(weight<previous||weight>1.000001f||std::abs(weight-f)>0.038f)return 3;
  if(f>=0.25f&&f<=0.75f&&weight!=f)return 4;
  if(std::abs(weight+cloudTraceEdgeFraction(1.0f-f)-1.0f)>0.000001f)return 5;
  previous=weight;
 }
 if(cloudTraceEdgeFraction(0.0f)!=0.0f||cloudTraceEdgeFraction(1.0f)!=1.0f
   ||cloudTraceEdgeFraction(0.0001f)>0.000001f)return 6;
}
'''.replace('CENTER',center).replace('PRECEDING',preceding).replace('EDGE',edge).replace('BRACKET',bracket).replace('GRID',grid)
        report=ROOT/'Artifacts/Reports/cloud-volume-rendering/mobile-depth-map'
        report.mkdir(parents=True,exist_ok=True)
        fixture=report/'cloud.mobile_depth_map.cpp'
        fixture.write_text(code,encoding='utf-8')
        records=compile_and_run(self,fixture,report,windows_flags=('/W4','/WX'),
                                linux_flags=('-Wall','-Wextra','-Werror'))
        (report/'result.json').write_text(json.dumps(records,indent=2),encoding='utf-8')

    def test_actual_shader_shell_intervals_ignore_terrain_clip_planes(self):
        from Tests.support.cpp import compile_and_run
        import hashlib
        owner=CLOUD_SHADER_OWNER
        native=owner.read_text(encoding='utf-8')
        fragment=extract_cloud_trace(native)
        sphere=definition(fragment,'bool sphere(')
        depth=definition(fragment,'float viewDepth(')
        begin=fragment.index('float rayProjection=')
        empty='if(total<=0.0)return vec4(0.0);'
        end=fragment.index(empty,begin)+len(empty)
        intervals=fragment[begin:end]
        march_end=fragment.index('\n#ifdef GL_ES\n if(depth==1.0')
        march=fragment[fragment.index('float stepLength=cloudStepLength('):march_end]
        air_path=definition(fragment,'float cloudForegroundTransmission(')
        self.assertIn('float entry=firstLength>0.0?first.x:second.x;',fragment)
        self.assertIn('if(depth==1.0&&!uOrthographic&&uCloudAtmosphereThickness>0.0)',fragment)
        # Only GLSL syntax is lowered. The actual native clipping/intersection
        # expressions execute as float, including the sky-depth decision.
        sphere=sphere.replace('out vec2 interval','vec2& interval').replace('interval=interval.yx;','std::swap(interval.x,interval.y);\n ')
        intervals=intervals.replace('return vec4(0.0);','return {};')
        owners='\n'.join((sphere,depth,'Result trace(vec3 origin,vec3 ray,float depth){\n'+intervals+
            '\nreturn {first,second,firstLength,secondLength,total};\n}',
            definition(fragment,'float cloudStepLength('),definition(fragment,'vec2 cloudStepSegment('),air_path,
            'float march(float firstLength,float total,vec3 origin={}){\n'
            ' const vec2 first{0.0f,firstLength},second{4.0f,4.0f+total-firstLength};\n'
            ' const vec3 ray{1.0f,0.0f,0.0f},originDx{},originDy{},rayDx{},rayDy{};\n'
            +march+'\n return transmittance;\n}'))
        owners=re.sub(r'(?<![\w.])(\d+\.\d+(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)(?![\w.])',r'\1f',owners)
        code=r'''
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#define CHECK(value) do {if(!(value))return __LINE__;}while(false)
struct vec2 {float x{},y{};vec2()=default;explicit vec2(float s):x(s),y(s){};vec2(float a,float b):x(a),y(b){};};
struct vec3 {float x{},y{},z{};};
vec3 operator+(vec3 a,vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
vec3 operator-(vec3 a,vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
vec3 operator*(vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
vec3 operator*(float s,vec3 a){return a*s;}
void operator+=(vec3& a,vec3 b){a=a+b;}
float dot(vec3 a,vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float length(vec3 a){return std::sqrt(dot(a,a));}
vec3 normalized(vec3 a){return a*(1.0f/std::sqrt(dot(a,a)));}
using std::sqrt;using std::min;using std::max;using std::clamp;using std::exp;
float mix(float a,float b,float t){return a*(1.0f-t)+b*t;}
float smoothstep(float a,float b,float value){
 const float t=clamp((value-a)/(b-a),0.0f,1.0f);
 return t*t*(3.0f-2.0f*t);
}
vec2 uNearFar{.01f,10.0f};bool uOrthographic{};
vec3 uForward{0,0,-1};float uWorldRadius=1.0f;
float uBase=7200.0f/6371000.0f,uThickness=2880.0f/6371000.0f;
float uCloudAtmosphereThickness=100000.0f/6371000.0f;
float uViewOccluderRadius{};
float uExtinction=1.0f,uRadius=1.0f;
unsigned densitySamples{};
float firstSample{},lastSample{};
float cloudExtinction(vec3 position,float,float){
 if(densitySamples++==0)firstSample=position.x;
 lastSample=position.x;
 const float density=position.x<2.0f?.82f+.16f*std::sin(11.0f*position.x):.28f+.18f*std::sin(17.0f*position.x);
 return density*uExtinction;
}
vec3 cloudIllumination(vec3,vec3,float,float){return {1,1,1};}
struct Result {vec2 first{},second{};float firstLength{},secondLength{},total{};};
SHADER_OWNERS
// Independent double geometric oracle: intersect a line at its closest point
// with concentric shell boundaries, then subtract the empty inner interval.
Result expected(vec3 o,vec3 d,double opaque=std::numeric_limits<double>::infinity()){
 const std::array<double,3> origin{o.x,o.y,o.z};
 std::array<double,3> direction{d.x,d.y,d.z};
 double norm{};for(double v:direction)norm+=v*v;norm=std::sqrt(norm);
 for(double& v:direction)v/=norm;
 double along{};for(unsigned a=0;a<3;++a)along-=origin[a]*direction[a];
 double perpendicular{};for(unsigned a=0;a<3;++a){const double v=origin[a]+direction[a]*along;perpendicular+=v*v;}
 const double solid=uViewOccluderRadius;
 if(solid>0&&perpendicular<solid*solid){
  const double half=std::sqrt(solid*solid-perpendicular);
  if(along+half>0)opaque=std::min(opaque,std::max(0.0,along-half));
 }
 const double inner=1.0+uBase,outer=inner+uThickness;
 if(perpendicular>=outer*outer)return {};
 const double outerHalf=std::sqrt(outer*outer-perpendicular);
 const double entry=std::max(0.0,along-outerHalf),exit=std::min(opaque,along+outerHalf);
 if(exit<=entry)return {};
 Result result;result.first={static_cast<float>(entry),static_cast<float>(exit)};
 if(perpendicular<inner*inner){
  const double innerHalf=std::sqrt(inner*inner-perpendicular),in=along-innerHalf,out=along+innerHalf;
  if(out>entry&&in<exit){result.first.y=static_cast<float>(std::min(exit,in));
   result.second={static_cast<float>(std::max(entry,out)),static_cast<float>(exit)};}
 }
 result.firstLength=std::max(0.0f,result.first.y-result.first.x);
 result.secondLength=std::max(0.0f,result.second.y-result.second.x);
 result.total=result.firstLength+result.secondLength;return result;
}
bool close(const Result& a,const Result& b,float tolerance=0.00008f){
 if(std::abs(a.total-b.total)>tolerance)return false;
 for(auto pair:{std::array{a.firstLength,b.firstLength},std::array{a.secondLength,b.secondLength}})
  if(std::abs(pair[0]-pair[1])>tolerance)return false;
 if(a.firstLength>0&&b.firstLength>0&&(std::abs(a.first.x-b.first.x)>tolerance||std::abs(a.first.y-b.first.y)>tolerance))return false;
 if(a.secondLength>0&&b.secondLength>0&&(std::abs(a.second.x-b.second.x)>tolerance||std::abs(a.second.y-b.second.y)>tolerance))return false;
 return true;
}
float encoded_depth(float rayDistance,vec3 ray){
 const float z=rayDistance*dot(ray,uForward)*uWorldRadius,n=uNearFar.x,f=uNearFar.y;
 return uOrthographic?(z-n)/(f-n):f*(z-n)/(z*(f-n));
}
int main(){
 const vec3 origin{0,0,3},ray{0,0,-1};
 // Terrain near planes can jump while this camera/cloud shell is unchanged.
 // Neither those planes nor a short terrain far plane erase a sky volume.
 const auto both=expected(origin,ray);CHECK(both.firstLength>0&&both.secondLength>0);
 for(vec2 planes:{vec2{.01f,10.0f},vec2{2.2f,10.0f},vec2{.01f,1.2f}}){
  uNearFar=planes;CHECK(close(trace(origin,ray,1.0f),both));
 }
 // Screen-edge rays reconstruct opaque view depth through their forward dot.
 const auto oblique=normalized({.2f,.1f,-1.0f});const auto angled=expected(origin,oblique);
 CHECK(angled.firstLength>0&&angled.secondLength>0);
 uNearFar={2.7f,10.0f};CHECK(close(trace(origin,oblique,1.0f),angled));
 // Rotating around to the other hemisphere preserves ray/shell direction.
 uForward={0,0,1};CHECK(close(trace({0,0,-3},uForward,1.0f),expected({0,0,-3},uForward)));
 // A viewer inside the shell must retain density immediately in front of them.
 const vec3 inside{0,0,1.0f+uBase+uThickness*.5f};uNearFar={.01f,10.0f};
 const auto fromInside=trace(inside,uForward,1.0f);
 CHECK(fromInside.total>uThickness*.4f&&close(fromInside,expected(inside,uForward)));
 // A grazing ray crossing only the outer shell has one continuous interval.
 uForward=ray;const vec3 grazing{1.0f+uBase+uThickness*.5f,0,3};
 const auto tangent=trace(grazing,ray,1.0f);CHECK(tangent.firstLength>0&&tangent.secondLength==0);
 CHECK(close(tangent,expected(grazing,ray)));
 CHECK(trace({1.0f+uBase+uThickness+.01f,0,3},ray,1.0f).total==0);
 // Real opaque geometry still clips clouds: before the volume, halfway into
 // its near side, and at the planet surface (which hides the far-side clouds).
 for(bool orthographic:{false,true}){
  uOrthographic=orthographic;uNearFar={.01f,10.0f};
  for(float distance:{1.5f,both.first.x+uThickness*.5f,2.0f}){
   const auto clipped=trace(origin,ray,encoded_depth(distance,ray));
   CHECK(close(clipped,expected(origin,ray,distance)));CHECK(clipped.secondLength==0);
  }
 }
 uOrthographic=false;
 const float distance=angled.first.x+uThickness*.25f;
 CHECK(close(trace(origin,oblique,encoded_depth(distance,oblique)),expected(origin,oblique,distance)));
 // The admitted body's relief bound clips only the rear interval at orbit.
 uViewOccluderRadius=(6371000.0f-1.18f*18000.0f-9.0f)/6371000.0f;
 uNearFar={.01f,10.0f};
 const auto orbit=trace(origin,ray,1.0f);
 CHECK(orbit.firstLength>0&&orbit.secondLength==0&&close(orbit,expected(origin,ray)));
 // The recorded 6.376 Mm view was below the cloud deck, looking inward.
 const vec3 belowDeck{0,0,6376359.0f/6371000.0f};
 CHECK(expected(belowDeck,ray).total==0&&trace(belowDeck,ray,1.0f).total==0);
 CHECK(trace({0,0,.995f},ray,1.0f).total==0);
 uForward={0,0,1};
 CHECK(trace(belowDeck,uForward,1.0f).total>0
  &&close(trace(belowDeck,uForward,1.0f),expected(belowDeck,uForward)));
 uForward=ray;
 CHECK(close(trace(grazing,ray,1.0f),tangent));
 CHECK(close(trace(origin,ray,encoded_depth(distance,ray)),expected(origin,ray,distance)));
 // Below the deck, the first nonempty interval starts after the clear gap.
 // A long skyward ray should lose contrast while the overhead cloud stays solid.
 const vec3 coastEye{0,0,1},horizon{1,0,0},zenith{0,0,1};
 const auto coastRay=trace(coastEye,horizon,1.0f);
 const auto vertical=trace(coastEye,zenith,1.0f);
 CHECK(coastRay.firstLength==0.0f&&coastRay.secondLength>0.0f&&coastRay.second.x>uBase*10.0f);
 const float distant=cloudForegroundTransmission(coastEye,horizon,coastRay.second.x);
 CHECK(distant>0.0f&&distant<0.8f);
 CHECK(vertical.firstLength==0.0f&&vertical.secondLength>0.0f);
 CHECK(cloudForegroundTransmission(coastEye,zenith,vertical.second.x)==1.0f);
 CHECK(cloudForegroundTransmission(inside,horizon,coastRay.second.x)==1.0f);
 CHECK(cloudForegroundTransmission(origin,ray,both.first.x)==1.0f);
 // Crossing an adaptive step boundary must not redistribute the ray's
 // earlier samples. Test both a contiguous ray and one with an empty gap.
 uThickness=1.0f;
 for(float seam:{.935f,2.0f}){
  const float below=march(std::min(seam,1.99999f),1.99999f);
  const float above=march(seam,2.00001f);
  CHECK(std::abs(above-below)<.0002f);
 }
 // Sliding the front segment's endpoint across a bin midpoint must not
 // switch a full bin's weight from one side of the empty gap to the other.
 CHECK(std::abs(march(.95001f,2.0f)-march(.94999f,2.0f))<.0002f);
 const auto integral=[](float start,float finish,float bias,float wave,float frequency){
  return bias*(finish-start)+wave*(std::cos(frequency*start)-std::cos(frequency*finish))/frequency;
 };
 const float optical=integral(0.0f,.935f,.82f,.16f,11.0f)+integral(4.0f,5.065f,.28f,.18f,17.0f);
 CHECK(std::abs(march(.935f,2.0f)-std::exp(-optical))<.012f);
#if defined(GL_ES)
 // The same 24 bins move toward the eye in a long in-cloud view. Check
 // both ends of the sampling interval to catch an incomplete remap.
 uThickness=5760.0f/6371000.0f;
 uExtinction=0;
 constexpr float rayLength=.02f;
 densitySamples=0;
 CHECK(march(rayLength,rayLength,{0,0,1.0f+uBase-uThickness})==1.0f);
 CHECK(densitySamples==24&&std::abs(firstSample-rayLength/48.0f)<1e-7f);
 densitySamples=0;
 CHECK(march(rayLength,rayLength,{0,0,1.0f+uBase+.5f*uThickness})==1.0f);
 constexpr float lastBegin=23.0f/24.0f;
 CHECK(densitySamples==24&&firstSample<rayLength/200.0f);
 CHECK(std::abs(lastSample-rayLength*(1.0f+lastBegin*lastBegin)*.5f)<1e-7f);
#endif
 return 0;
}
'''.replace('SHADER_OWNERS',owners)
        report=ROOT/'Artifacts/Reports/cloud-volume-rendering/shell-intervals'
        report.mkdir(parents=True,exist_ok=True)
        source=report/'cloud.shell_intervals.cpp';source.write_text(code,encoding='utf-8')
        receipt=dict(owner=owner.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(native.encode()).hexdigest(),passed=False)
        result=report/'result.json';result.write_text(json.dumps(receipt,indent=2),encoding='utf-8')
        receipt['records']=compile_and_run(self,source,report,windows_flags=('/W4','/WX'),linux_flags=('-Wall','-Wextra','-Werror'))
        gles=report/'gles';gles.mkdir(exist_ok=True)
        receipt['gles_records']=compile_and_run(self,source,gles,
            windows_flags=('/W4','/WX','/DGL_ES'),linux_flags=('-Wall','-Wextra','-Werror','-DGL_ES'))
        receipt['passed']=True;result.write_text(json.dumps(receipt,indent=2),encoding='utf-8')

    def test_actual_packets_time_rebase_and_native_precision(self):
        if os.name != 'nt':self.skipTest('Installed MSVC module consumer lane')
        repo=ROOT;root=ROOT
        report=ROOT/'Artifacts/Reports/cloud-volume-rendering/packets';report.mkdir(parents=True,exist_ok=True)
        native=desktop_cloud_shader_source()
        depth=re.search(r'float viewDepth\(float z\)\{[^}]+\}',native).group(0).replace('1.0','1.0f')
        density_owner='\n'.join(definition(native,marker) for marker in (
            'float filteredNoise(', 'float cloudDensity(', 'float cloudExtinction('))
        # Execute the actual GLSL density/filter expressions with float literal
        # semantics; the noise oracle is the already shared CPU lattice owner.
        density_owner=re.sub(r'(?<![\w.])(\d+\.\d+)(?![\w.])',r'\1f',density_owner)
        code=r'''
        #include <algorithm>
        #include <array>
        #include <cmath>
        #include <cstdio>
        #include <limits>
        #include <memory>
        #include <optional>
        #include <span>
        #include <vector>
        import asset.cloud_volume;
        import render.preview_grid;
        import render.lighting;
        #define CHECK(value) do {if(!(value))return __LINE__;}while(false)
        namespace depth_shader {
        struct {float x{},y{};}uNearFar;
        bool uOrthographic{};
        float mix(float a,float b,float t){return a*(1.0f-t)+b*t;}
        DEPTH_OWNER
        }
        namespace density_shader {
        using uint=unsigned;
        using std::abs;
        using std::sin;
        struct vec3{float x{},y{},z{};};
        vec3 uCenter{0.0f,-1.0f,0.0f};
        vec3 operator*(vec3 v,float s){return {v.x*s,v.y*s,v.z*s};}
        vec3 operator/(vec3 v,float s){return {v.x/s,v.y/s,v.z/s};}
        vec3 operator+(vec3 v,float s){return {v.x+s,v.y+s,v.z+s};}
        vec3 operator+(vec3 a,vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
        struct Matrix{bool tilted{};}uWorldToBody;
        vec3 operator*(Matrix m,vec3 v){return m.tilted?vec3{v.y,-v.x,v.z}:v;}
        struct{float x{},y{};}uWind,uEvolution;
        float uBase{},uThickness{},uNoiseScale{},uCoverage{},uExtinction{},uLowerExtinction{};
        unsigned uSeed{};
        float dot(vec3 a,vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
        float length(vec3 v){return std::sqrt(dot(v,v));}
        float clamp(float v,float a,float b){return std::clamp(v,a,b);}
        float max(float a,float b){return std::max(a,b);}
        float min(float a,float b){return std::min(a,b);}
        float mix(float a,float b,float v){return a+(b-a)*v;}
        vec3 mix(vec3 a,vec3 b,float v){return a*(1.0f-v)+b*v;}
        float smoothstep(float a,float b,float v){v=clamp((v-a)/(b-a),0.0f,1.0f);return v*v*(3.0f-2.0f*v);}
        float noiseValue(vec3 v,unsigned seed){return static_cast<float>(epochengine::asset::cloud_volume::detail::noise({v.x,v.y,v.z},seed));}
        DENSITY_OWNER
        }
        int main(){
         using namespace epochengine;namespace p=previewgrid;namespace c=asset::cloud_volume;
         constexpr double earth=6'371'000.0;
         p::CloudVolume cloud{.center={0,-earth,0},.radius=earth,.recipe={.seed=91,
          .base_altitude_meters=2000,.thickness_meters=1500,.horizontal_scale_meters=50000,
          .coverage=.65f,.extinction_per_meter=.001f,.wind_period_seconds=600,.view_occluder_radius_meters=6349751.0}};
         p::Camera camera;camera.eye={0,0,0};camera.target={0,0,-1};camera.nearPlane=.1f;camera.farPlane=1e8f;
         CHECK(p::valid_cloud_volume(cloud));auto initial=p::cloud_volume_view(cloud,camera);CHECK(initial);
         CHECK(initial->radius_meters==earth&&initial->center[1]==-1.0f&&initial->thickness>0.0002f);
         CHECK(initial->seed==cloud.recipe.seed&&initial->coverage==cloud.recipe.coverage);
         CHECK(initial->extinction==cloud.recipe.extinction_per_meter&&initial->lower_extinction>initial->extinction);
         CHECK(float(1+initial->base+initial->thickness)>float(1+initial->base));
         const double inner=cloud.recipe.view_occluder_radius_meters/earth;
         CHECK(initial->view_occluder_radius<=inner
           &&std::nextafter(initial->view_occluder_radius,1.0f)>inner);
         auto tiny=cloud;tiny.radius=1e9;tiny.recipe.thickness_meters=1;tiny.recipe.horizontal_scale_meters=1e6;
         CHECK(p::valid_cloud_volume(tiny)&&!p::cloud_volume_view(tiny,camera));
         tiny.recipe.thickness_meters=10000;CHECK(p::cloud_volume_view(tiny,camera));
         auto underflow=cloud;underflow.radius=1e-50;underflow.meters_per_world_unit=earth/underflow.radius;
         CHECK(p::valid_cloud_volume(underflow)&&!p::cloud_volume_view(underflow,camera));
         auto scaled=cloud;scaled.radius*=2;scaled.center[1]*=2;scaled.meters_per_world_unit=.5;
         const auto scaledPack=p::cloud_volume_view(scaled,camera);CHECK(scaledPack);
         CHECK(scaledPack->radius_meters==initial->radius_meters&&scaledPack->center==initial->center&&scaledPack->thickness==initial->thickness);
         CHECK(scaledPack->view_occluder_radius==initial->view_occluder_radius);
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
         const auto wind=p::cloud_volume_view(advanced->clouds[0],camera);
         CHECK(wind&&wind->wind!=initial->wind&&wind->evolution!=initial->evolution);
         CHECK(wind->view_occluder_radius==initial->view_occluder_radius);
         // A paused sample is stable. Reverse seeks restore the previous density pose.
         CHECK(p::advance_scene_frame_camera(&context,camera,movedOrigin,std::span<const p::CloudVolume>{&moved,1}));
         const auto paused=p::scene_frame_for(&context);
         CHECK(p::cloud_volume_view(paused->clouds[0],camera)->wind==wind->wind);
         CHECK(p::cloud_volume_view(paused->clouds[0],camera)->evolution==wind->evolution);
         moved.time_seconds=0;CHECK(p::advance_scene_frame_camera(&context,camera,movedOrigin,std::span<const p::CloudVolume>{&moved,1}));
         const auto rewound=p::scene_frame_for(&context);
         CHECK(p::cloud_volume_view(rewound->clouds[0],camera)->wind==initial->wind);
         CHECK(p::cloud_volume_view(rewound->clouds[0],camera)->evolution==initial->evolution);
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
        '''.replace('DEPTH_OWNER',depth).replace('DENSITY_OWNER',density_owner)
        code=code.replace('p::cleanup_context(&context);return 0;',r'''
         // CPU and native density agree across pixel footprints and wind phases,
         // including regional advection after the wind period wraps.
         density_shader::uBase=static_cast<float>(cloud.recipe.base_altitude_meters/earth);
         density_shader::uThickness=static_cast<float>(cloud.recipe.thickness_meters/earth);
         density_shader::uNoiseScale=static_cast<float>(earth/cloud.recipe.horizontal_scale_meters);
         density_shader::uCoverage=cloud.recipe.coverage;density_shader::uSeed=cloud.recipe.seed;
         density_shader::uExtinction=cloud.recipe.extinction_per_meter;
         density_shader::uLowerExtinction=static_cast<float>(c::lower_deck_extinction(cloud.recipe.extinction_per_meter));
         for(double seconds:{0.0,30.0,90.0,137.0,301.0,137.0+cloud.recipe.wind_period_seconds}){
          auto animated=cloud;animated.time_seconds=seconds;
          const auto packet=p::cloud_volume_view(animated,camera);CHECK(packet);
          density_shader::uWind={packet->wind[0],packet->wind[1]};
          density_shader::uEvolution={packet->evolution[0],packet->evolution[1]};
          for(unsigned index=0;index<128u;++index)for(double height:{.08,.3,.52,.75,.94})for(double footprint:{0.0,100.0,50000.0,1e7}){
           const double y=1.0-2.0*(index+0.5)/128.0,angle=index*2.399963229728653;
           const double horizontal=std::sqrt(1-y*y),distance=earth+cloud.recipe.base_altitude_meters+height*cloud.recipe.thickness_meters;
           const c::Point point{horizontal*std::cos(angle)*distance,y*distance,horizontal*std::sin(angle)*distance};
           const density_shader::vec3 local{static_cast<float>(point[0]/earth),static_cast<float>(point[1]/earth),static_cast<float>(point[2]/earth)};
           // Isolate a metre-scale error from rounding a planet radius to the
           // shader's float representation. Retain a tight density comparison
           // against the CPU oracle at the actual represented radius.
           const double represented=density_shader::length(local)*earth;
           CHECK(std::abs(represented-distance)<1.4);
           const double localLength=std::hypot(double(local.x),double(local.y),double(local.z));
           const double scale=represented/localLength;
           const c::Point representedPoint{local.x*scale,local.y*scale,local.z*scale};
           for(double rayFootprint : {0.0,4000.0}){
            const double observerRadiusRatio=std::array{1.0,1.004,2.8}[index%3];
            density_shader::uCenter={0.0f,static_cast<float>(-observerRadiusRatio),0.0f};
            const double observerAltitude=(double(density_shader::length(density_shader::uCenter))-1.0)*earth;
            const auto expected=c::density(cloud.recipe,earth,representedPoint,seconds,footprint,rayFootprint,observerAltitude);
            const float pixel=static_cast<float>(footprint/earth),bin=static_cast<float>(rayFootprint/earth);
            const auto actual=density_shader::cloudDensity(local,pixel,bin);
            const auto lowerLimit=earth+cloud.recipe.base_altitude_meters+0.52*cloud.recipe.thickness_meters;
            const auto extinction=c::extinction_from_density(expected,cloud.recipe.extinction_per_meter,represented<lowerLimit);
            CHECK(std::abs(density_shader::cloudExtinction(local,pixel,bin)-extinction)<0.000007f);
            if(!std::isfinite(actual)||std::abs(actual-expected)>=0.0015){
             std::fprintf(stderr,"density mismatch time=%.0f sample=%u h=%.2f pixel=%.0f ray=%.0f float=%.7f metric=%.7f\n",
              seconds,index,height,footprint,rayFootprint,actual,expected);
             return __LINE__;
            }
            density_shader::uWorldToBody.tilted=true;
            const auto tilted=density_shader::cloudDensity({-local.y,local.x,local.z},pixel,bin);
            density_shader::uWorldToBody.tilted=false;
            CHECK(std::isfinite(tilted)&&std::abs(tilted-expected)<0.0015);
           }
          }
         }
         CHECK(density_shader::cloudDensity({0.0f,1.0f,0.0f},0.0f,0.0f)==0.0f);
         density_shader::uCoverage=0.0f;
         CHECK(density_shader::cloudDensity({0.0f,static_cast<float>((earth+2750.0)/earth),0.0f},0.0f,0.0f)==0.0f);
         p::cleanup_context(&context);return 0;
        ''')
        (report/'cloud.packet_contract.cpp').write_text(code,encoding='utf-8')
        module_map={}
        owners=[repo/'Engine/modules',repo/'Engine/dep/EpochPlatformEngine/modules']
        for owner in owners:
         for path in owner.rglob('*.ixx'):
          match=re.search(r'^export module\s+([\w.:]+);',path.read_text(encoding='utf-8-sig'),re.M)
          if match:module_map[match.group(1)]=path
        pending=['render.preview_grid','authoring.texture_detail'];closure=set()
        while pending:
         name=pending.pop()
         if name in closure:continue
         closure.add(name);pending+=re.findall(r'^(?:export\s+)?import\s+([\w.:]+);',module_map[name].read_text(encoding='utf-8-sig'),re.M)
        paths='\n'.join('"'+module_map[name].as_posix()+'"' for name in sorted(closure))
        bases=' '.join('"'+owner.as_posix()+'"' for owner in owners)
        sources=' '.join('"'+(repo/p).as_posix()+'"' for p in ['Engine/src/render/render.preview_camera.cpp', 'Engine/src/render/render.preview_lighting.cpp','Engine/src/assets/texture/asset.texture_artifact.cpp','Engine/src/authoring/texture/authoring.texture_document.cpp', 'Engine/src/authoring/texture/authoring.texture_snapshot_codec.cpp','Engine/src/authoring/texture/authoring.texture_residency.cpp'])
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
