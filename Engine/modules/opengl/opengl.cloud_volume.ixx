// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>
#include <include/engine.config.hpp>
#if defined(EPOCH_USING_OPENGL) && EPOCH_USING_OPENGL == 1
#include <glad/glad.h>
#include "../../src/render/render.cloud_shader.hpp"
#endif

export module opengl.cloud_volume;
import render.preview_grid;
import render.lighting;
import opengl.state;
import core.logger;

#if defined(EPOCH_USING_OPENGL) && EPOCH_USING_OPENGL == 1
export namespace epochengine::openglcloud
{
    namespace detail
    {
        using rendercloudshader::cloud_trace_source;
        using rendercloudshader::composite_source;
        using rendercloudshader::fragment_source;
        using rendercloudshader::vertex_source;

        [[nodiscard]] inline GLuint shader(GLenum stage,std::string_view source,std::string_view body={})
        {
            const auto handle=glCreateShader(stage);if(!handle)return 0;
            constexpr auto preamble=rendercloudshader::desktop_header;
            const std::array data{preamble.data(),source.data(),body.data()};
            const std::array sizes{static_cast<GLint>(preamble.size()),static_cast<GLint>(source.size()),static_cast<GLint>(body.size())};
            glShaderSource(handle,body.empty()?2:3,data.data(),sizes.data());glCompileShader(handle);
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
            const auto pixel=vertex?shader(GL_FRAGMENT_SHADER,cloud_trace_source,fragment):0;
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
        inline void volume_uniforms(GLuint program,const previewgrid::CloudVolumeView& packed)
        {
            vector(program,"uCenter",{packed.center[0],packed.center[1],packed.center[2]});
            glUniformMatrix3fv(glGetUniformLocation(program,"uWorldToBody"),1,GL_FALSE,packed.world_to_body.data());
            scalar(program,"uRadius",packed.radius_meters);scalar(program,"uWorldRadius",packed.radius_world);
            scalar(program,"uBase",packed.base);scalar(program,"uThickness",packed.thickness);
            scalar(program,"uViewOccluderRadius",packed.view_occluder_radius);
            scalar(program,"uNoiseScale",packed.noise_scale);scalar(program,"uCoverage",packed.coverage);
            scalar(program,"uExtinction",packed.extinction);
            scalar(program,"uLowerExtinction",packed.lower_extinction);
            glUniform1ui(glGetUniformLocation(program,"uSeed"),packed.seed);
            glUniform2f(glGetUniformLocation(program,"uWind"),packed.wind[0],packed.wind[1]);
            glUniform2f(glGetUniformLocation(program,"uEvolution"),packed.evolution[0],packed.evolution[1]);
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
        for(const auto& cloud:scene.clouds)if(!previewgrid::cloud_volume_view(cloud,scene.camera))
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
        using Clock=std::chrono::steady_clock;
        const bool firstPass=!resources.program;
        const auto begin=firstPass?Clock::now():Clock::time_point{};
        if(firstPass)
        {
            resources.program=detail::program(detail::fragment_source);
            resources.composite_program=detail::program(detail::composite_source);
            glGenVertexArrays(1,&resources.vao);
            if(!resources.program||!resources.composite_program||!resources.vao)
            {release(resources);return fail("Native cloud shader/pipeline creation failed; see the shader diagnostic.",true);}
        }
        const auto pipelineReady=firstPass?Clock::now():begin;
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
        const auto targetsReady=firstPass?Clock::now():begin;
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
        glUniform1i(glGetUniformLocation(resources.program,"uDepth"),0);
        const auto forward=previewgrid::normalize(previewgrid::subtract(scene.camera.target,scene.camera.eye));
        const auto right=previewgrid::normalize(previewgrid::cross(forward,scene.camera.up));
        const auto up=previewgrid::cross(right,forward);
        const double halfHeight=scene.camera.projection==render_camera::ProjectionKind::orthographic
            ?(std::max)(0.025,double(scene.camera.orthographicVerticalSize)*0.5):std::tan(scene.camera.fovRadians*0.5);
        previewgrid::Vec3 sun{0,1,0},sunColor{};
        const auto& lightingFrame=scene.solids->lighting;
        float brightest{};
        for(const auto& light:lightingFrame.lights)if(light.desc.enabled&&light.desc.kind==lighting::LightKind::Directional
            &&light.desc.intensity>brightest)
        {brightest=light.desc.intensity;sun={-light.desc.direction.x,-light.desc.direction.y,-light.desc.direction.z};
            sunColor={light.desc.color.r*brightest,light.desc.color.g*brightest,light.desc.color.b*brightest};}
        sun=previewgrid::normalize(sun);
        const auto trace_uniforms=[&](GLuint program){
            detail::projection(program,scene.camera);
            detail::vector(program,"uForward",forward);detail::vector(program,"uRight",right);detail::vector(program,"uUp",up);
            glUniform2f(glGetUniformLocation(program,"uProjection"),static_cast<float>(halfHeight*width/height),static_cast<float>(halfHeight));
            detail::vector(program,"uSun",sun);detail::vector(program,"uSunColor",sunColor);
            detail::vector(program,"uAmbient",{lightingFrame.environment.ambient.r,lightingFrame.environment.ambient.g,lightingFrame.environment.ambient.b});
        };
        trace_uniforms(resources.program);
        std::array<const previewgrid::CloudVolume*,6> ordered{};std::size_t count{};
        for(const auto& cloud:scene.clouds)ordered[count++]=&cloud;
        const auto distance=[&](const auto* cloud){double square{};for(unsigned axis=0;axis<3u;++axis){const double eye=axis==0?scene.camera.eye.x:axis==1?scene.camera.eye.y:scene.camera.eye.z;const auto delta=cloud->center[axis]-eye;square+=delta*delta;}return square;};
        std::sort(ordered.begin(),ordered.begin()+count,[&](const auto* a,const auto* b){return distance(a)>distance(b);});
        previewgrid::CloudVolumeView singleVolume;
        for(std::size_t index=0;index<count;++index)
        {
            const auto& cloud=*ordered[index];
            const auto packed=*previewgrid::cloud_volume_view(cloud,scene.camera);
            detail::volume_uniforms(resources.program,packed);
            if(count==1u)singleVolume=packed;
            glDrawArrays(GL_TRIANGLES,0,3);
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(preserved.draw));glViewport(x,y,width,height);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,resources.color_texture);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,resources.depth_texture);
        glUseProgram(resources.composite_program);
        // One exact ray can replace one cloud. Retain the existing combined
        // resolve when there is more than one independently ordered body.
        glUniform1i(glGetUniformLocation(resources.composite_program,"uRepairEdges"),count==1u);
        if(count==1u)
        {
            trace_uniforms(resources.composite_program);
            detail::volume_uniforms(resources.composite_program,singleVolume);
        }
        else detail::projection(resources.composite_program,scene.camera);
        glUniform1i(glGetUniformLocation(resources.composite_program,"uCloud"),0);glUniform1i(glGetUniformLocation(resources.composite_program,"uDepth"),1);
        GLint encoding{GL_LINEAR};GLboolean doubleBuffered{};glGetBooleanv(GL_DOUBLEBUFFER,&doubleBuffered);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,preserved.draw?GL_COLOR_ATTACHMENT0:(doubleBuffered?GL_BACK_LEFT:GL_FRONT_LEFT),GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
        const bool nativeSrgb=preserved.srgb&&encoding==GL_SRGB;
        if(nativeSrgb)glEnable(GL_FRAMEBUFFER_SRGB);
        glUniform1i(glGetUniformLocation(resources.composite_program,"uEncodeSrgb"),!nativeSrgb);
        glDrawArrays(GL_TRIANGLES,0,3);
        if(firstPass)
        {
            const auto elapsed=[](Clock::duration duration){
                return std::to_string(std::chrono::duration<double,std::milli>(duration).count());
            };
            const auto submitted=Clock::now();
            logger::info("OpenGL.Clouds","First density pass CPU wall times, viewport "
                +std::to_string(width)+"x"+std::to_string(height)+", volumes="+std::to_string(count)
                +", pipeline_ms="+elapsed(pipelineReady-begin)
                +", targets_ms="+elapsed(targetsReady-pipelineReady)
                +", trace_and_composite_submit_ms="+elapsed(submitted-targetsReady));
        }
        resources.failed=false;resources.diagnostic=nullptr;return true;
    }
}
#endif
