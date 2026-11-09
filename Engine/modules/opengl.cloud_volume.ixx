// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <include/engine.config.hpp>
#if defined(EPOCH_USING_OPENGL) && EPOCH_USING_OPENGL == 1
#include <glad/glad.h>
#endif

export module opengl.cloud_volume;
import asset.cloud_volume;
import render.preview_grid;
import render.lighting;
import opengl.state;
import core.logger;

#if defined(EPOCH_USING_OPENGL) && EPOCH_USING_OPENGL == 1
export namespace epochengine::openglcloud
{
    struct PackedVolume final
    {
        std::array<float,3> center{};
        std::array<float,9> world_to_body{};
        float radius_meters{},radius_world{},base{},thickness{},noise_scale{},cosine{},sine{};
    };
    // This packing owner is shared by the native pass and its CPU contracts.
    // Reject a valid metric recipe that the float shader cannot represent;
    // silently drawing a zero-thickness shell would falsely accept the frame.
    [[nodiscard]] inline std::optional<PackedVolume> pack_volume(
        const previewgrid::CloudVolume& cloud,const previewgrid::Camera& camera) noexcept
    {
        if(!previewgrid::valid_cloud_volume(cloud))return {};
        const auto radius=cloud.radius*cloud.meters_per_world_unit;
        if(cloud.recipe.thickness_meters/radius<8.0*std::numeric_limits<float>::epsilon())return {};
        PackedVolume result;
        for(unsigned axis=0;axis<3u;++axis)
        {
            const double eye=axis==0?camera.eye.x:axis==1?camera.eye.y:camera.eye.z;
            const auto coordinate=(cloud.center[axis]-eye)/cloud.radius;
            if(!std::isfinite(coordinate)||std::abs(coordinate)>1e12)return {};
            result.center[axis]=static_cast<float>(coordinate);
        }
        if(cloud.radius>(std::numeric_limits<float>::max)()
            ||cloud.radius<(std::numeric_limits<float>::min)())return {};
        result.radius_world=static_cast<float>(cloud.radius);result.radius_meters=static_cast<float>(radius);
        result.base=static_cast<float>(cloud.recipe.base_altitude_meters/radius);
        result.thickness=static_cast<float>(cloud.recipe.thickness_meters/radius);
        result.noise_scale=static_cast<float>(radius/cloud.recipe.horizontal_scale_meters);
        const double angle=-std::fmod(cloud.time_seconds,cloud.recipe.wind_period_seconds)
            /cloud.recipe.wind_period_seconds*6.28318530717958647692;
        result.cosine=static_cast<float>(std::cos(angle));result.sine=static_cast<float>(std::sin(angle));
        for(unsigned index=0;index<9u;++index)result.world_to_body[index]=static_cast<float>(cloud.rotation[index]);
        return result;
    }

    namespace detail
    {
        // A fullscreen triangle has no edge through the viewport and needs no
        // persistent geometry upload. All resources belong to the native context.
        inline constexpr std::string_view vertex_source=R"glsl(#version 330 core
out vec2 vUv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);vUv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)glsl";
        inline constexpr std::string_view fragment_source=R"glsl(#version 330 core
in vec2 vUv;out vec4 outColor;
uniform sampler2D uDepth;
uniform vec3 uForward,uRight,uUp,uCenter,uSun,uSunColor,uAmbient;
uniform mat3 uWorldToBody;
uniform vec2 uProjection,uNearFar,uWind;
uniform bool uOrthographic;
uniform float uRadius,uWorldRadius,uBase,uThickness,uNoiseScale,uCoverage,uExtinction;
uniform uint uSeed;
uint hashWord(uint v){v^=v>>16u;v*=0x7feb352du;v^=v>>15u;v*=0x846ca68bu;return v^(v>>16u);}
float lattice(ivec3 p,uint seed){uint v=uint(p.x)*0x8da6b343u^uint(p.y)*0xd8163841u^uint(p.z)*0xcb1ab31fu^seed;return float(hashWord(v)>>8u)/16777215.0;}
float noiseValue(vec3 p,uint seed){
 ivec3 c=ivec3(floor(p));vec3 f=fract(p);f=f*f*(3.0-2.0*f);
 return mix(mix(mix(lattice(c,seed),lattice(c+ivec3(1,0,0),seed),f.x),mix(lattice(c+ivec3(0,1,0),seed),lattice(c+ivec3(1,1,0),seed),f.x),f.y),
 mix(mix(lattice(c+ivec3(0,0,1),seed),lattice(c+ivec3(1,0,1),seed),f.x),mix(lattice(c+ivec3(0,1,1),seed),lattice(c+ivec3(1,1,1),seed),f.x),f.y),f.z);
}
float filteredNoise(vec3 p,uint seed,float footprint){
 float retained=1.0-smoothstep(0.5,1.0,footprint);
 return retained==0.0?0.5:mix(0.5,noiseValue(p,seed),retained);
}
float cloudDensity(vec3 local,float footprint){
 float height=(length(local)-1.0-uBase)/uThickness;if(height<=0.0||height>=1.0)return 0.0;
 vec3 q=uWorldToBody*local;q=vec3(uWind.x*q.x+uWind.y*q.z,q.y,-uWind.y*q.x+uWind.x*q.z)*uNoiseScale;
 footprint*=uNoiseScale;
 float weather=0.7*filteredNoise(q/32.0,uSeed^0x57454154u,footprint/32.0)
  +0.3*filteredNoise(q/16.0+9.0,uSeed^0x48455231u,footprint/16.0);
 float cover=smoothstep(0.0,0.20,weather-(1.0-uCoverage));if(cover==0.0)return 0.0;
 float shape=(2.0*filteredNoise(q,uSeed,footprint)+filteredNoise(q*2.0+17.0,uSeed+1013u,footprint*2.0))/3.0;
 float detailMask=clamp((shape-0.25)/0.50,0.0,1.0);
 return cover*detailMask*smoothstep(0.0,0.2,height)*(1.0-smoothstep(0.55,1.0,height));
}
bool sphere(vec3 o,vec3 d,float r,out vec2 interval){
 float b=dot(o,d),c=dot(o,o)-r*r,h=b*b-c;if(h<0.0)return false;
 float s=sqrt(h),q=-b-(b>=0.0?s:-s);interval=vec2(q,q==0.0?-b:c/q);
 if(interval.x>interval.y)interval=interval.yx;return true;
}
float viewDepth(float z){float n=uNearFar.x,f=uNearFar.y;return uOrthographic?mix(n,f,z):n*f/(f*(1.0-z)+n*z);}
void main(){
 vec2 p=vUv*2.0-1.0;vec3 origin=vec3(0.0),ray;
 if(uOrthographic){origin=uRight*p.x*uProjection.x+uUp*p.y*uProjection.y;ray=uForward;}
 else ray=normalize(uForward+uRight*p.x*uProjection.x+uUp*p.y*uProjection.y);
 origin/=uWorldRadius;origin-=uCenter;
 // Derivatives must be taken before ray-dependent discard/loop control. At
 // half resolution these are the actual accumulation-pixel footprints.
 vec3 originDx=dFdx(origin),originDy=dFdy(origin),rayDx=dFdx(ray),rayDy=dFdy(ray);
 float depth=texture(uDepth,vUv).r;
 float rayProjection=max(0.000001,dot(ray,uForward));
 float opaque=min(uNearFar.y,viewDepth(depth))/(uWorldRadius*rayProjection);
 vec2 outer,inner;if(!sphere(origin,ray,1.0+uBase+uThickness,outer))discard;
 outer=vec2(max(uNearFar.x/(uWorldRadius*rayProjection),outer.x),min(opaque,outer.y));if(outer.y<=outer.x)discard;
 vec2 first=outer,second=vec2(0.0);bool split=sphere(origin,ray,1.0+uBase,inner)&&inner.y>outer.x&&inner.x<outer.y;
 if(split){first=vec2(outer.x,min(outer.y,inner.x));second=vec2(max(outer.x,inner.y),outer.y);}
 float firstLength=max(0.0,first.y-first.x),secondLength=max(0.0,second.y-second.x),total=firstLength+secondLength;
 if(total<=0.0)discard;
 int count=int(clamp(ceil(total/max(uThickness/10.0,0.00000001)),16.0,48.0));
 float stepLength=total/float(count),transmittance=1.0;vec3 color=vec3(0.0);
 for(int i=0;i<64;++i){
  if(i>=count||transmittance<0.005)break;
  float offset=(float(i)+0.5)*stepLength,t=offset<firstLength?first.x+offset:second.x+(offset-firstLength);
  float footprint=max(length(originDx+rayDx*t),length(originDy+rayDy*t));
  vec3 samplePosition=origin+ray*t;float density=cloudDensity(samplePosition,footprint);if(density<=0.0001)continue;
  float alpha=1.0-exp(-density*uExtinction*stepLength*uRadius);
  // Two bounded light probes give self-shadowing without an unbounded shadow
  // traversal. Directional scene lighting also rotates with the existing sun.
  float shadow=0.0;
  for(int j=0;j<2;++j)shadow+=cloudDensity(samplePosition+uSun*uThickness*(float(j)+0.5)*0.5,footprint);
  float sunVisibility=exp(-shadow*uExtinction*uThickness*uRadius*0.5);
  float facing=max(0.0,dot(normalize(samplePosition),uSun));
  float phase=0.65+0.35*pow(max(0.0,dot(ray,uSun)),8.0);
  vec3 illumination=uAmbient+uSunColor*sunVisibility*(0.25+0.75*facing)*phase;
  color+=transmittance*alpha*illumination;transmittance*=1.0-alpha;
 }
 outColor=vec4(color,1.0-transmittance);
}
)glsl";
        inline constexpr std::string_view composite_source=R"glsl(#version 330 core
in vec2 vUv;out vec4 outColor;
uniform sampler2D uCloud,uDepth;
uniform vec2 uNearFar;uniform bool uOrthographic,uEncodeSrgb;
float viewDepth(float z){float n=uNearFar.x,f=uNearFar.y;return uOrthographic?mix(n,f,z):n*f/(f*(1.0-z)+n*z);}
vec3 srgb(vec3 x){return mix(12.92*x,1.055*pow(max(x,vec3(0.0)),vec3(1.0/2.4))-0.055,step(vec3(0.0031308),x));}
void main(){
 vec2 size=vec2(textureSize(uCloud,0)),p=vUv*size-0.5,base=floor(p),fraction=fract(p);
 float target=viewDepth(texture(uDepth,vUv).r),weightSum=0.0,nearestDifference=1e30;vec4 sum=vec4(0.0),nearest=vec4(0.0);
 for(int y=0;y<2;++y)for(int x=0;x<2;++x){
  vec2 uv=(base+vec2(x,y)+0.5)/size;vec4 value=texture(uCloud,uv);
  float difference=abs(viewDepth(texture(uDepth,uv).r)-target);
  if(difference<nearestDifference){nearestDifference=difference;nearest=value;}
  float spatial=(x==0?1.0-fraction.x:fraction.x)*(y==0?1.0-fraction.y:fraction.y);
  float weight=spatial*exp(-difference/max(0.01,abs(target)*0.01));sum+=value*weight;weightSum+=weight;
 }
 vec4 value=weightSum>0.00001?sum/weightSum:nearest;
 if(uEncodeSrgb&&value.a>0.00001)value.rgb=srgb(value.rgb/value.a)*value.a;
 outColor=value;
}
)glsl";
        [[nodiscard]] inline GLuint shader(GLenum stage,std::string_view source)
        {
            const auto handle=glCreateShader(stage);if(!handle)return 0;
            const auto* data=source.data();const auto size=static_cast<GLint>(source.size());
            glShaderSource(handle,1,&data,&size);glCompileShader(handle);
            GLint ok{};glGetShaderiv(handle,GL_COMPILE_STATUS,&ok);
            if(ok==GL_TRUE)return handle;
            std::array<GLchar,2048> message{};GLsizei used{};
            glGetShaderInfoLog(handle,static_cast<GLsizei>(message.size()),&used,message.data());
            logger::get("OpenGL.Clouds").log(logger::LogLevel::Error,std::string_view{message.data(),static_cast<std::size_t>((std::max)(0,used))},std::source_location::current());
            glDeleteShader(handle);return 0;
        }
        [[nodiscard]] inline GLuint program(std::string_view fragment)
        {
            const auto vertex=shader(GL_VERTEX_SHADER,vertex_source);
            const auto pixel=vertex?shader(GL_FRAGMENT_SHADER,fragment):0;
            if(!vertex||!pixel){if(vertex)glDeleteShader(vertex);return 0;}
            const auto handle=glCreateProgram();
            if(handle){glAttachShader(handle,vertex);glAttachShader(handle,pixel);glLinkProgram(handle);}
            glDeleteShader(vertex);glDeleteShader(pixel);
            GLint ok{};if(handle)glGetProgramiv(handle,GL_LINK_STATUS,&ok);
            if(ok==GL_TRUE)return handle;
            if(handle){std::array<GLchar,2048> message{};GLsizei used{};
                glGetProgramInfoLog(handle,static_cast<GLsizei>(message.size()),&used,message.data());
                logger::get("OpenGL.Clouds").log(logger::LogLevel::Error,std::string_view{message.data(),static_cast<std::size_t>((std::max)(0,used))},std::source_location::current());glDeleteProgram(handle);}
            return 0;
        }
        struct ScopedState final
        {
            GLint draw{},read{},program{},vao{},active{},textures[2]{},samplers[2]{},viewport[4]{};
            GLint unpackBuffer{};
            GLint sourceRgb{},destinationRgb{},sourceAlpha{},destinationAlpha{},equationRgb{},equationAlpha{};
            GLboolean blend{},depth{},scissor{},cull{},srgb{},depthMask{},colorMask[4]{};
            ScopedState()
            {
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
                glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
                glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_VIEWPORT,viewport);
                glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
                glGetIntegerv(GL_BLEND_SRC_RGB,&sourceRgb);glGetIntegerv(GL_BLEND_DST_RGB,&destinationRgb);
                glGetIntegerv(GL_BLEND_SRC_ALPHA,&sourceAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&destinationAlpha);
                glGetIntegerv(GL_BLEND_EQUATION_RGB,&equationRgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&equationAlpha);
                blend=glIsEnabled(GL_BLEND);depth=glIsEnabled(GL_DEPTH_TEST);scissor=glIsEnabled(GL_SCISSOR_TEST);
                cull=glIsEnabled(GL_CULL_FACE);srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
                glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
                for(unsigned unit=0;unit<2u;++unit){glActiveTexture(GL_TEXTURE0+unit);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[unit]);glGetIntegerv(GL_SAMPLER_BINDING,&samplers[unit]);glBindSampler(unit,0);}
            }
            ~ScopedState()
            {
                for(unsigned unit=0;unit<2u;++unit){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(textures[unit]));glBindSampler(unit,static_cast<GLuint>(samplers[unit]));}
                glActiveTexture(static_cast<GLenum>(active));glUseProgram(static_cast<GLuint>(program));glBindVertexArray(static_cast<GLuint>(vao));
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(draw));glBindFramebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(read));
                glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glDepthMask(depthMask);
                glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER,static_cast<GLuint>(unpackBuffer));
                glBlendFuncSeparate(sourceRgb,destinationRgb,sourceAlpha,destinationAlpha);glBlendEquationSeparate(equationRgb,equationAlpha);
                const auto restore=[](GLenum name,GLboolean enabled){if(enabled)glEnable(name);else glDisable(name);};
                restore(GL_BLEND,blend);restore(GL_DEPTH_TEST,depth);restore(GL_SCISSOR_TEST,scissor);restore(GL_CULL_FACE,cull);restore(GL_FRAMEBUFFER_SRGB,srgb);
            }
        };
        inline void vector(GLuint program,const char* name,previewgrid::Vec3 value)
        {glUniform3f(glGetUniformLocation(program,name),value.x,value.y,value.z);}
        inline void scalar(GLuint program,const char* name,double value)
        {glUniform1f(glGetUniformLocation(program,name),static_cast<float>(value));}
        inline void projection(GLuint program,const previewgrid::Camera& camera)
        {
            glUniform2f(glGetUniformLocation(program,"uNearFar"),camera.nearPlane,camera.farPlane);
            glUniform1i(glGetUniformLocation(program,"uOrthographic"),camera.projection==render_camera::ProjectionKind::orthographic);
        }
    }

    inline void release(openglstate::CloudVolumeResources& resources) noexcept
    {
        if(resources.program)glDeleteProgram(resources.program);
        if(resources.composite_program)glDeleteProgram(resources.composite_program);
        if(resources.vao)glDeleteVertexArrays(1,&resources.vao);
        if(resources.depth_texture)glDeleteTextures(1,&resources.depth_texture);
        if(resources.color_texture)glDeleteTextures(1,&resources.color_texture);
        if(resources.framebuffer)glDeleteFramebuffers(1,&resources.framebuffer);
        resources={};
    }

    // No GL objects or maps outlive their owning native context. Allocation is
    // resize-only, bounded to96MiB; an unsupported target fails visibly once.
    [[nodiscard]] inline bool render(openglstate::CloudVolumeResources& resources,
        const previewgrid::PublishedSceneFrame& scene,int x,int y,int width,int height)
    {
        if(scene.clouds.empty())return true;
        if(resources.failed)return false;
        if(std::ranges::any_of(scene.markers,[](const auto& marker){return marker.wireframe;}))return true;
        const auto fail=[&](const char* message,bool permanent=false){
            if(resources.diagnostic!=message)logger::error("OpenGL.Clouds",message);
            resources.diagnostic=message;resources.failed=permanent;return false;
        };
        if(!scene.solids||!previewgrid::admitted_cloud_volumes(scene.clouds)||x<0||y<0||width<=0||height<=0
            ||width>16384||height>16384)return fail("Cloud density packet or viewport was refused.");
        if(!GLAD_GL_VERSION_3_3)return fail("Density clouds require native OpenGL3.3; no opaque replacement was drawn.",true);
        for(const auto& cloud:scene.clouds)if(!pack_volume(cloud,scene.camera))
            return fail("Cloud position or thickness is below this native shader's precision at the selected planet radius.");
        const std::uint64_t full=static_cast<std::uint64_t>(width)*height;
        const int lowWidth=(width+1)/2,lowHeight=(height+1)/2;
        if(full*4u+static_cast<std::uint64_t>(lowWidth)*lowHeight*8u>96ull*1024u*1024u)
            return fail("Cloud viewport exceeds its96MiB transient target budget.");
        const detail::ScopedState preserved;
        GLint samples{},depthBits{},maximumTexture{};
        glGetIntegerv(GL_SAMPLES,&samples);glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximumTexture);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,preserved.draw?GL_DEPTH_ATTACHMENT:GL_DEPTH,
            GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,&depthBits);
        if(samples>0||depthBits<=0||width>maximumTexture||height>maximumTexture)
            return fail("Density clouds need a single-sample scene depth target; this target is unsupported.");
        if(!resources.program)
        {
            resources.program=detail::program(detail::fragment_source);
            resources.composite_program=detail::program(detail::composite_source);
            glGenVertexArrays(1,&resources.vao);
            if(!resources.program||!resources.composite_program||!resources.vao)
            {release(resources);return fail("Native cloud shader/pipeline creation failed; see the shader diagnostic.",true);}
        }
        glActiveTexture(GL_TEXTURE0);
        if(resources.width!=width||resources.height!=height)
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
            if(!resources.depth_texture)glGenTextures(1,&resources.depth_texture);
            if(!resources.color_texture)glGenTextures(1,&resources.color_texture);
            if(!resources.framebuffer)glGenFramebuffers(1,&resources.framebuffer);
            glBindTexture(GL_TEXTURE_2D,resources.depth_texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT32F,width,height,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_NONE);
            glBindTexture(GL_TEXTURE_2D,resources.color_texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,lowWidth,lowHeight,0,GL_RGBA,GL_HALF_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,resources.framebuffer);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,resources.color_texture,0);
            if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            {release(resources);return fail("Native cloud accumulation framebuffer is incomplete.",true);}
            resources.width=width;resources.height=height;
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(preserved.draw));
        glBindTexture(GL_TEXTURE_2D,resources.depth_texture);
        glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,x,y,width,height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,resources.framebuffer);
        glViewport(0,0,lowWidth,lowHeight);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glDisable(GL_CULL_FACE);glDisable(GL_FRAMEBUFFER_SRGB);
        const GLfloat empty[4]{};glClearBufferfv(GL_COLOR,0,empty);
        glEnable(GL_BLEND);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(resources.program);glBindVertexArray(resources.vao);
        glUniform1i(glGetUniformLocation(resources.program,"uDepth"),0);detail::projection(resources.program,scene.camera);
        const auto forward=previewgrid::normalize(previewgrid::subtract(scene.camera.target,scene.camera.eye));
        const auto right=previewgrid::normalize(previewgrid::cross(forward,scene.camera.up));
        const auto up=previewgrid::cross(right,forward);
        detail::vector(resources.program,"uForward",forward);detail::vector(resources.program,"uRight",right);detail::vector(resources.program,"uUp",up);
        const double halfHeight=scene.camera.projection==render_camera::ProjectionKind::orthographic
            ?(std::max)(0.025,double(scene.camera.orthographicVerticalSize)*0.5):std::tan(scene.camera.fovRadians*0.5);
        glUniform2f(glGetUniformLocation(resources.program,"uProjection"),static_cast<float>(halfHeight*width/height),static_cast<float>(halfHeight));
        previewgrid::Vec3 sun{0,1,0},sunColor{};
        const auto& lightingFrame=scene.solids->lighting;
        float brightest{};
        for(const auto& light:lightingFrame.lights)if(light.desc.enabled&&light.desc.kind==lighting::LightKind::Directional
            &&light.desc.intensity>brightest)
        {brightest=light.desc.intensity;sun={-light.desc.direction.x,-light.desc.direction.y,-light.desc.direction.z};
            sunColor={light.desc.color.r*brightest,light.desc.color.g*brightest,light.desc.color.b*brightest};}
        sun=previewgrid::normalize(sun);detail::vector(resources.program,"uSun",sun);detail::vector(resources.program,"uSunColor",sunColor);
        detail::vector(resources.program,"uAmbient",{lightingFrame.environment.ambient.r,lightingFrame.environment.ambient.g,lightingFrame.environment.ambient.b});
        std::array<const previewgrid::CloudVolume*,6> ordered{};std::size_t count{};
        for(const auto& cloud:scene.clouds)ordered[count++]=&cloud;
        const auto distance=[&](const auto* cloud){double square{};for(unsigned axis=0;axis<3u;++axis){const double eye=axis==0?scene.camera.eye.x:axis==1?scene.camera.eye.y:scene.camera.eye.z;const auto delta=cloud->center[axis]-eye;square+=delta*delta;}return square;};
        std::sort(ordered.begin(),ordered.begin()+count,[&](const auto* a,const auto* b){return distance(a)>distance(b);});
        for(std::size_t index=0;index<count;++index)
        {
            const auto& cloud=*ordered[index];const auto& recipe=cloud.recipe;
            const auto packed=*pack_volume(cloud,scene.camera);
            detail::vector(resources.program,"uCenter",{packed.center[0],packed.center[1],packed.center[2]});
            glUniformMatrix3fv(glGetUniformLocation(resources.program,"uWorldToBody"),1,GL_FALSE,packed.world_to_body.data());
            detail::scalar(resources.program,"uRadius",packed.radius_meters);detail::scalar(resources.program,"uWorldRadius",packed.radius_world);
            detail::scalar(resources.program,"uBase",packed.base);detail::scalar(resources.program,"uThickness",packed.thickness);
            detail::scalar(resources.program,"uNoiseScale",packed.noise_scale);detail::scalar(resources.program,"uCoverage",recipe.coverage);
            detail::scalar(resources.program,"uExtinction",recipe.extinction_per_meter);glUniform1ui(glGetUniformLocation(resources.program,"uSeed"),recipe.seed);
            glUniform2f(glGetUniformLocation(resources.program,"uWind"),packed.cosine,packed.sine);
            glDrawArrays(GL_TRIANGLES,0,3);
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(preserved.draw));glViewport(x,y,width,height);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,resources.color_texture);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,resources.depth_texture);
        glUseProgram(resources.composite_program);detail::projection(resources.composite_program,scene.camera);
        glUniform1i(glGetUniformLocation(resources.composite_program,"uCloud"),0);glUniform1i(glGetUniformLocation(resources.composite_program,"uDepth"),1);
        GLint encoding{GL_LINEAR};GLboolean doubleBuffered{};glGetBooleanv(GL_DOUBLEBUFFER,&doubleBuffered);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,preserved.draw?GL_COLOR_ATTACHMENT0:(doubleBuffered?GL_BACK_LEFT:GL_FRONT_LEFT),GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
        const bool nativeSrgb=preserved.srgb&&encoding==GL_SRGB;
        if(nativeSrgb)glEnable(GL_FRAMEBUFFER_SRGB);
        glUniform1i(glGetUniformLocation(resources.composite_program,"uEncodeSrgb"),!nativeSrgb);
        glDrawArrays(GL_TRIANGLES,0,3);resources.failed=false;resources.diagnostic=nullptr;return true;
    }
}
#endif
