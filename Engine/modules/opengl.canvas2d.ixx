/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <vector>

#include "../include/engine.config.hpp"

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#  include <glad/glad.h>
#endif

export module opengl.canvas2d;

import render.canvas2d;
import render.canvas2d_cpu;
import render.canvas2d_evidence;
import render.canvas2d_presentation;
import render.device;
import render.device_opengl_family;
import render.texture_residency;

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
import opengl.platform;
import opengl.quad;
import opengl.state;
import opengl.textures;

export namespace epochengine::openglcanvas2d
{
    struct PresentationBinding final
    {
        OpenGLFamilyRenderDevice* device{};
        opengltextures::BackendData* backend{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return device != nullptr && backend != nullptr
                && device->backend() == RendererBackendKind::opengl;
        }
    };

    namespace detail
    {
        inline void restore_capability(GLenum capability, bool enabled) noexcept
        {
            if (enabled)
                glEnable(capability);
            else
                glDisable(capability);
        }

        struct ScopedPresentationState final
        {
            GLint draw_framebuffer{};
            GLint read_framebuffer{};
            GLint viewport[4]{};
            GLint scissor_box[4]{};
            GLint program{};
            GLint vertex_array{};
            GLint array_buffer{};
            GLint element_buffer{};
            GLint active_texture{GL_TEXTURE0};
            GLint active_texture_binding{};
            GLint texture_zero_binding{};
            GLint active_sampler_binding{};
            GLint sampler_zero_binding{};
            GLint blend_source_rgb{};
            GLint blend_destination_rgb{};
            GLint blend_source_alpha{};
            GLint blend_destination_alpha{};
            GLint blend_equation_rgb{};
            GLint blend_equation_alpha{};
            GLint depth_function{};
            GLint cull_face_mode{};
            GLint front_face{};
            GLint polygon_mode[2]{GL_FILL, GL_FILL};
            GLfloat blend_color[4]{};
            GLfloat clear_color[4]{};
            GLboolean depth_write{GL_TRUE};
            GLboolean color_write[4]{GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
            bool blend_enabled{};
            bool depth_enabled{};
            bool cull_enabled{};
            bool scissor_enabled{};
            bool stencil_enabled{};
#if defined(GL_FRAMEBUFFER_SRGB)
            bool framebuffer_srgb_enabled{};
#endif
#if defined(GL_RASTERIZER_DISCARD)
            bool rasterizer_discard_enabled{};
#endif
            bool sampler_objects_available{};

            ScopedPresentationState() noexcept
            {
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
                glGetIntegerv(GL_VIEWPORT, viewport);
                glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
                glGetIntegerv(GL_CURRENT_PROGRAM, &program);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertex_array);
                glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer);
                glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_buffer);
                glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &active_texture_binding);
                sampler_objects_available = GLAD_GL_VERSION_3_3 != 0
                    && glBindSampler != nullptr;
                if (sampler_objects_available)
                    glGetIntegerv(GL_SAMPLER_BINDING, &active_sampler_binding);
                if (active_texture != GL_TEXTURE0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_zero_binding);
                    if (sampler_objects_available)
                        glGetIntegerv(GL_SAMPLER_BINDING, &sampler_zero_binding);
                    glActiveTexture(static_cast<GLenum>(active_texture));
                }
                else
                {
                    texture_zero_binding = active_texture_binding;
                    if (sampler_objects_available)
                        sampler_zero_binding = active_sampler_binding;
                }
                glGetIntegerv(GL_BLEND_SRC_RGB, &blend_source_rgb);
                glGetIntegerv(GL_BLEND_DST_RGB, &blend_destination_rgb);
                glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_source_alpha);
                glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_destination_alpha);
                glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_equation_rgb);
                glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_equation_alpha);
                glGetFloatv(GL_BLEND_COLOR, blend_color);
                glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
                glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
                glGetIntegerv(GL_CULL_FACE_MODE, &cull_face_mode);
                glGetIntegerv(GL_FRONT_FACE, &front_face);
                glGetIntegerv(GL_POLYGON_MODE, polygon_mode);
                glGetBooleanv(GL_COLOR_WRITEMASK, color_write);
                glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color);
                blend_enabled = glIsEnabled(GL_BLEND) == GL_TRUE;
                depth_enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
                cull_enabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
                scissor_enabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
                stencil_enabled = glIsEnabled(GL_STENCIL_TEST) == GL_TRUE;
#if defined(GL_FRAMEBUFFER_SRGB)
                framebuffer_srgb_enabled = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;
#endif
#if defined(GL_RASTERIZER_DISCARD)
                rasterizer_discard_enabled = glIsEnabled(GL_RASTERIZER_DISCARD) == GL_TRUE;
#endif
            }

            ScopedPresentationState(const ScopedPresentationState&) = delete;
            ScopedPresentationState& operator=(const ScopedPresentationState&) = delete;

            ~ScopedPresentationState() noexcept
            {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_framebuffer));
                glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_framebuffer));
                glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
                glUseProgram(static_cast<GLuint>(program));
                glBindVertexArray(static_cast<GLuint>(vertex_array));
                glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(array_buffer));
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(element_buffer));

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_zero_binding));
                if (sampler_objects_available)
                    glBindSampler(0, static_cast<GLuint>(sampler_zero_binding));
                if (active_texture != GL_TEXTURE0)
                {
                    glActiveTexture(static_cast<GLenum>(active_texture));
                    glBindTexture(
                        GL_TEXTURE_2D,
                        static_cast<GLuint>(active_texture_binding));
                    if (sampler_objects_available)
                    {
                        glBindSampler(
                            static_cast<GLuint>(active_texture - GL_TEXTURE0),
                            static_cast<GLuint>(active_sampler_binding));
                    }
                }

                glBlendFuncSeparate(
                    static_cast<GLenum>(blend_source_rgb),
                    static_cast<GLenum>(blend_destination_rgb),
                    static_cast<GLenum>(blend_source_alpha),
                    static_cast<GLenum>(blend_destination_alpha));
                glBlendEquationSeparate(
                    static_cast<GLenum>(blend_equation_rgb),
                    static_cast<GLenum>(blend_equation_alpha));
                glBlendColor(
                    blend_color[0], blend_color[1],
                    blend_color[2], blend_color[3]);
                glDepthFunc(static_cast<GLenum>(depth_function));
                glDepthMask(depth_write);
                glCullFace(static_cast<GLenum>(cull_face_mode));
                glFrontFace(static_cast<GLenum>(front_face));
                glPolygonMode(GL_FRONT, static_cast<GLenum>(polygon_mode[0]));
                glPolygonMode(GL_BACK, static_cast<GLenum>(polygon_mode[1]));
                glColorMask(
                    color_write[0], color_write[1],
                    color_write[2], color_write[3]);
                glClearColor(
                    clear_color[0], clear_color[1],
                    clear_color[2], clear_color[3]);

                restore_capability(GL_BLEND, blend_enabled);
                restore_capability(GL_DEPTH_TEST, depth_enabled);
                restore_capability(GL_CULL_FACE, cull_enabled);
                restore_capability(GL_SCISSOR_TEST, scissor_enabled);
                restore_capability(GL_STENCIL_TEST, stencil_enabled);
#if defined(GL_FRAMEBUFFER_SRGB)
                restore_capability(GL_FRAMEBUFFER_SRGB, framebuffer_srgb_enabled);
#endif
#if defined(GL_RASTERIZER_DISCARD)
                restore_capability(GL_RASTERIZER_DISCARD, rasterizer_discard_enabled);
#endif
            }
        };

        struct ScopedReadbackState final
        {
            GLint read_framebuffer{};
            GLint read_buffer{GL_BACK};
            GLint pixel_pack_buffer{};
            GLint pack_alignment{4};
            GLint pack_row_length{};
            GLint pack_skip_rows{};
            GLint pack_skip_pixels{};

            ScopedReadbackState() noexcept
            {
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
                glGetIntegerv(GL_READ_BUFFER, &read_buffer);
                glGetIntegerv(
                    GL_PIXEL_PACK_BUFFER_BINDING,
                    &pixel_pack_buffer);
                glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
                glGetIntegerv(GL_PACK_ROW_LENGTH, &pack_row_length);
                glGetIntegerv(GL_PACK_SKIP_ROWS, &pack_skip_rows);
                glGetIntegerv(GL_PACK_SKIP_PIXELS, &pack_skip_pixels);
            }

            ScopedReadbackState(const ScopedReadbackState&) = delete;
            ScopedReadbackState& operator=(const ScopedReadbackState&) = delete;

            ~ScopedReadbackState() noexcept
            {
                glBindFramebuffer(
                    GL_READ_FRAMEBUFFER,
                    static_cast<GLuint>(read_framebuffer));
                glReadBuffer(static_cast<GLenum>(read_buffer));
                glBindBuffer(
                    GL_PIXEL_PACK_BUFFER,
                    static_cast<GLuint>(pixel_pack_buffer));
                glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
                glPixelStorei(GL_PACK_ROW_LENGTH, pack_row_length);
                glPixelStorei(GL_PACK_SKIP_ROWS, pack_skip_rows);
                glPixelStorei(GL_PACK_SKIP_PIXELS, pack_skip_pixels);
            }
        };

        [[nodiscard]] inline canvas2d::evidence::PixelEvidenceResult
            evidence_failure(
                const canvas2d::cpu::Image& reference,
                canvas2d::evidence::PixelEvidenceCode code) noexcept
        {
            canvas2d::evidence::PixelEvidenceResult result{};
            result.code = code;
            result.extent = reference.extent;
            result.reference_hash =
                canvas2d::evidence::canonical_image_hash(reference);
            return result;
        }

        struct ScopedTextureParameters final
        {
            GLint minimum_filter{GL_LINEAR};
            GLint maximum_filter{GL_LINEAR};
            GLint address_u{GL_CLAMP_TO_EDGE};
            GLint address_v{GL_CLAMP_TO_EDGE};

            ScopedTextureParameters() noexcept
            {
                glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &minimum_filter);
                glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &maximum_filter);
                glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &address_u);
                glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &address_v);
            }

            ScopedTextureParameters(const ScopedTextureParameters&) = delete;
            ScopedTextureParameters& operator=(const ScopedTextureParameters&) = delete;

            ~ScopedTextureParameters() noexcept
            {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minimum_filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maximum_filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address_u);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address_v);
            }
        };

        [[nodiscard]] inline bool valid_compose_bounds(
            const canvas2d::presentation::NativePresentationPacket& packet,
            const OpenGLFamilyTextureRecord& record) noexcept
        {
            if (!packet.texture || packet.compose == nullptr
                || packet.frame_sequence == 0 || packet.content_hash == 0
                || packet.image.extent.empty()
                || packet.image.format != TextureFormat::rgba8_unorm
                || packet.image.origin != canvas2d::presentation::PixelOrigin::top_left
                || packet.image.color_space != canvas2d::SpriteColorSpace::linear
                || packet.image.alpha_encoding
                    != canvas2d::cpu::AlphaEncoding::premultiplied)
                return false;
            const auto& compose = *packet.compose;
            const auto& viewport = compose.viewport;
            const auto& destination = viewport.clipped_destination;
            const auto& visible = viewport.visible_canvas;
            constexpr std::uint32_t maximumGlSize =
                static_cast<std::uint32_t>((std::numeric_limits<GLsizei>::max)());
            if (!compose || !compose.requires_offscreen_canvas
                || compose.destination != canvas2d::ComposeTargetKind::presentation_surface
                || viewport.output_surface.empty() || destination.empty()
                || viewport.output_surface.width > maximumGlSize
                || viewport.output_surface.height > maximumGlSize
                || record.desc.width != viewport.render_extent.width
                || record.desc.height != viewport.render_extent.height
                || record.desc.format != TextureFormat::rgba8_unorm
                || !std::isfinite(visible.x) || !std::isfinite(visible.y)
                || !std::isfinite(visible.width) || !std::isfinite(visible.height)
                || visible.x < 0.0f || visible.y < 0.0f
                || visible.width <= 0.0f || visible.height <= 0.0f)
            {
                return false;
            }
            return packet.surface.valid_for(viewport.output_surface)
                && packet.image.extent == viewport.render_extent
                && visible.x + visible.width
                    <= static_cast<float>(record.desc.width) + 0.001f
                && visible.y + visible.height
                    <= static_cast<float>(record.desc.height) + 0.001f;
        }

        [[nodiscard]] inline OpenGLFamilyNativeTextureAllocation
            contract_allocate_texture(
                void*, RendererBackendKind,
                const TextureDesc&, std::uint32_t) noexcept
        {
            return {91u, true};
        }

        [[nodiscard]] inline bool contract_upload_texture(
            void*, RendererBackendKind,
            const OpenGLFamilyTextureRecord&,
            const TextureUploadDesc&) noexcept
        {
            return true;
        }

        inline void contract_destroy_texture(
            void*, RendererBackendKind,
            const OpenGLFamilyTextureRecord&) noexcept
        {
        }
    }

    [[nodiscard]] inline bool present_native_canvas2d(
        void* user,
        const canvas2d::presentation::NativePresentationPacket& packet)
    {
        auto* const binding = static_cast<PresentationBinding*>(user);
        if (!binding || !static_cast<bool>(*binding)
            || !packet.texture || packet.compose == nullptr)
            return false;

        const OpenGLFamilyTextureRecord* const record =
            binding->device->resolve_texture(packet.texture);
        if (!record || !record->native_work_order_ready()
            || !detail::valid_compose_bounds(packet, *record))
        {
            return false;
        }

        auto& backend = *binding->backend;
        openglcontext::PlatformGL::ScopedContext context_guard;
        if (!opengltextures::activate_backend_context(
                backend,
                context_guard,
                "present Canvas2D"))
        {
            return false;
        }

        const auto platform_context =
            opengltextures::detail::to_platform_context(backend.glState);
        const void* const context_key =
            opengltextures::platform_context_key(platform_context);
        if (!context_key || context_key != record->native_context_key)
            return false;
        {
            std::lock_guard<std::mutex> gpu_lock(backend.gpuMutex);
            const auto stored = backend.native_textures.find(
                opengltextures::NativeTextureKey{
                    context_key,
                    record->texture_object});
            if (stored == backend.native_textures.end() || !stored->second.active)
                return false;
        }
        if (glIsTexture(record->texture_object) != GL_TRUE)
            return false;

        auto& gl_state = opengltextures::state_for_platform_context(
            backend,
            platform_context,
            nullptr);
        detail::ScopedPresentationState state_guard{};
        if (!openglquad::ensure_quad_pipeline(gl_state))
            return false;

        const auto& compose = *packet.compose;
        const auto& surface = packet.surface;
        const auto& surface_viewport = surface.viewport;
        const auto& viewport = compose.viewport;
        const auto& destination = viewport.clipped_destination;
        const auto& visible = viewport.visible_canvas;
        const float output_width = static_cast<float>(viewport.output_surface.width);
        const float output_height = static_cast<float>(viewport.output_surface.height);
        const float canvas_width = static_cast<float>(record->desc.width);
        const float canvas_height = static_cast<float>(record->desc.height);

        const GLint gl_surface_y = static_cast<GLint>(surface.framebuffer_extent.height)
            - static_cast<GLint>(surface_viewport.y)
            - static_cast<GLint>(surface_viewport.height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glViewport(
            surface_viewport.x,
            gl_surface_y,
            static_cast<GLsizei>(surface_viewport.width),
            static_cast<GLsizei>(surface_viewport.height));
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            surface_viewport.x,
            gl_surface_y,
            static_cast<GLsizei>(surface_viewport.width),
            static_cast<GLsizei>(surface_viewport.height));
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_BLEND);
#if defined(GL_FRAMEBUFFER_SRGB)
        glDisable(GL_FRAMEBUFFER_SRGB);
#endif
#if defined(GL_RASTERIZER_DISCARD)
        glDisable(GL_RASTERIZER_DISCARD);
#endif
        glDepthMask(GL_FALSE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (compose.clear_letterbox)
        {
            glClearColor(
                (std::clamp)(compose.letterbox_color.r, 0.0f, 1.0f),
                (std::clamp)(compose.letterbox_color.g, 0.0f, 1.0f),
                (std::clamp)(compose.letterbox_color.b, 0.0f, 1.0f),
                (std::clamp)(compose.letterbox_color.a, 0.0f, 1.0f));
            glClear(GL_COLOR_BUFFER_BIT);
        }

        auto& pipeline = gl_state;
        glUseProgram(pipeline.shader);
        glBindVertexArray(pipeline.vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, record->texture_object);
        if (state_guard.sampler_objects_available)
            glBindSampler(0, 0);
        detail::ScopedTextureParameters texture_parameters{};
        const GLint filter = compose.presentation_filter == FilterMode::nearest
            ? GL_NEAREST
            : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const float u0 = visible.x / canvas_width;
        const float du = visible.width / canvas_width;
        const float v0 = (visible.y + visible.height) / canvas_height;
        const float dv = -visible.height / canvas_height;
        if (pipeline.uUVRegionLoc >= 0)
            glUniform4f(pipeline.uUVRegionLoc, u0, v0, du, dv);
        if (pipeline.uSamplerLoc >= 0)
            glUniform1i(pipeline.uSamplerLoc, 0);

        const float center_x = static_cast<float>(destination.x)
            + static_cast<float>(destination.width) * 0.5f;
        const float center_y = static_cast<float>(destination.y)
            + static_cast<float>(destination.height) * 0.5f;
        const float ndc_x = center_x / output_width * 2.0f - 1.0f;
        const float ndc_y = 1.0f - center_y / output_height * 2.0f;
        const float ndc_width = static_cast<float>(destination.width)
            / output_width * 2.0f;
        const float ndc_height = static_cast<float>(destination.height)
            / output_height * 2.0f;
        if (pipeline.uTransformLoc >= 0)
            glUniform4f(
                pipeline.uTransformLoc,
                ndc_x,
                ndc_y,
                ndc_width,
                ndc_height);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        return true;
    }

    [[nodiscard]] inline canvas2d::evidence::PixelEvidenceResult
        compare_native_canvas2d(
            PresentationBinding& binding,
            const canvas2d::cpu::Image& reference,
            std::uint64_t referenceHash,
            canvas2d::presentation::PresentationSurface surface,
            const canvas2d::evidence::PixelEvidencePolicy& policy = {})
    {
        using namespace canvas2d::evidence;
        if (!static_cast<bool>(binding) || !reference.valid()
            || !surface.valid_for(reference.extent))
        {
            return detail::evidence_failure(
                reference,
                PixelEvidenceCode::readback_unavailable);
        }

        auto& backend = *binding.backend;
        openglcontext::PlatformGL::ScopedContext contextGuard;
        if (!opengltextures::activate_backend_context(
                backend,
                contextGuard,
                "compare Canvas2D pixels"))
        {
            return detail::evidence_failure(
                reference,
                PixelEvidenceCode::readback_unavailable);
        }

        const std::uint64_t pixelCount =
            static_cast<std::uint64_t>(reference.extent.width)
                * reference.extent.height;
        if (!policy.valid() || pixelCount == 0
            || pixelCount > policy.maximum_pixels
            || pixelCount > (std::numeric_limits<std::size_t>::max)())
        {
            return detail::evidence_failure(
                reference,
                policy.valid()
                    ? PixelEvidenceCode::capacity_exceeded
                    : PixelEvidenceCode::invalid_policy);
        }

        try
        {
            std::vector<canvas2d::cpu::Rgba8> pixels(
                static_cast<std::size_t>(pixelCount));
            detail::ScopedReadbackState stateGuard{};
            GLboolean doubleBuffered = GL_TRUE;
            glGetBooleanv(GL_DOUBLEBUFFER, &doubleBuffered);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadBuffer(doubleBuffered == GL_TRUE ? GL_BACK : GL_FRONT);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ROW_LENGTH, 0);
            glPixelStorei(GL_PACK_SKIP_ROWS, 0);
            glPixelStorei(GL_PACK_SKIP_PIXELS, 0);

            const auto& viewport = surface.viewport;
            const GLint readY =
                static_cast<GLint>(surface.framebuffer_extent.height)
                - static_cast<GLint>(viewport.y)
                - static_cast<GLint>(viewport.height);
            glFinish();
            glReadPixels(
                viewport.x,
                readY,
                static_cast<GLsizei>(viewport.width),
                static_cast<GLsizei>(viewport.height),
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                pixels.data());

            return compare_pixels(
                reference,
                referenceHash,
                PixelReadbackView{
                    reference.extent,
                    reference.extent.width,
                    pixels,
                    PixelOrigin::bottom_left},
                policy);
        }
        catch (const std::bad_alloc&)
        {
            return detail::evidence_failure(
                reference,
                PixelEvidenceCode::allocation_failure);
        }
    }

    [[nodiscard]] inline canvas2d::presentation::NativePresentationHooks
        make_native_presentation_hooks(PresentationBinding& binding) noexcept
    {
        return {&binding, &present_native_canvas2d};
    }

    class Presenter final
    {
    public:
        Presenter(
            OpenGLFamilyRenderDevice& device,
            std::uint64_t backend_epoch,
            texture_residency::ResidencyLimits limits = {}) noexcept
            : binding_{&device, &opengltextures::get_opengl_backend()},
              presenter_{
                  device,
                  backend_epoch,
                  make_native_presentation_hooks(binding_),
                  limits}
        {
        }

        Presenter(const Presenter&) = delete;
        Presenter& operator=(const Presenter&) = delete;
        Presenter(Presenter&&) = delete;
        Presenter& operator=(Presenter&&) = delete;

        [[nodiscard]] canvas2d::presentation::PresentationResult present(
            const canvas2d::Canvas2DFramePlan& frame,
            const canvas2d::cpu::RasterResult& raster,
            const canvas2d::presentation::PresentationPolicy& policy = {})
        {
            return presenter_.present(frame, raster, policy);
        }

        [[nodiscard]] canvas2d::presentation::PresentationResult present(
            const canvas2d::Canvas2DFramePlan& frame,
            const canvas2d::cpu::RasterResult& raster,
            canvas2d::presentation::PresentationSurface surface,
            const canvas2d::presentation::PresentationPolicy& policy = {})
        {
            return presenter_.present(frame, raster, surface, policy);
        }

        [[nodiscard]] canvas2d::presentation::PresentationResult rasterize_and_present(
            const canvas2d::Canvas2DFramePlan& frame,
            const canvas2d::cpu::ResourceBindings& resources = {},
            const canvas2d::cpu::RasterLimits& raster_limits = {},
            const canvas2d::cpu::RasterPolicy& raster_policy = {},
            const canvas2d::presentation::PresentationPolicy& presentation_policy = {})
        {
            return presenter_.rasterize_and_present(
                frame,
                resources,
                raster_limits,
                raster_policy,
                presentation_policy);
        }

        [[nodiscard]] canvas2d::evidence::PixelEvidenceResult
            compare_native(
                const canvas2d::cpu::Image& reference,
                std::uint64_t referenceHash,
                canvas2d::presentation::PresentationSurface surface,
                const canvas2d::evidence::PixelEvidencePolicy& policy = {})
        {
            return compare_native_canvas2d(
                binding_, reference, referenceHash, surface, policy);
        }

        [[nodiscard]] bool reset_backend_epoch(std::uint64_t epoch) noexcept
        {
            return presenter_.reset_backend_epoch(epoch);
        }

        void retire_all() noexcept
        {
            presenter_.retire_all();
        }

        [[nodiscard]] const canvas2d::presentation::PresentationMetrics&
            metrics() const noexcept
        {
            return presenter_.metrics();
        }

    private:
        PresentationBinding binding_{};
        canvas2d::presentation::Canvas2DPresenter presenter_;
    };

    [[nodiscard]] inline bool no_context_runtime_contract()
    {
        OpenGLFamilyRenderDevice device{RendererBackendKind::opengl};
        device.set_native_texture_hooks(OpenGLFamilyNativeTextureHooks{
            .allocate = detail::contract_allocate_texture,
            .upload = detail::contract_upload_texture,
            .destroy = detail::contract_destroy_texture
        });
        TextureDesc desc{};
        desc.width = 4;
        desc.height = 4;
        desc.format = TextureFormat::rgba8_unorm;
        const TextureHandle texture = device.create_texture(desc);
        if (!texture || !device.texture_ready(texture))
            return false;

        canvas2d::ComposeRequest request{};
        request.viewport = {
            {4, 4}, {8, 8}, canvas2d::ViewportPolicy::integer_scale, true};
        const canvas2d::FinalComposePlan compose =
            canvas2d::make_final_compose_plan(request);
        PresentationBinding binding{
            &device,
            &opengltextures::get_opengl_backend()};
        const auto hooks = make_native_presentation_hooks(binding);
        const canvas2d::presentation::NativePresentationPacket packet{
            texture,
            &compose,
            canvas2d::presentation::full_surface(compose.viewport.output_surface),
            canvas2d::presentation::ImageContract{
                {4, 4},
                TextureFormat::rgba8_unorm,
                canvas2d::presentation::PixelOrigin::top_left,
                canvas2d::SpriteColorSpace::linear,
                canvas2d::cpu::AlphaEncoding::premultiplied},
            1,
            1};
        const bool refused = hooks.ready()
            && !hooks.present(hooks.user, packet);

        canvas2d::cpu::Image reference{};
        reference.extent = compose.viewport.output_surface;
        reference.pixels.assign(
            static_cast<std::size_t>(reference.extent.width)
                * reference.extent.height,
            canvas2d::cpu::Rgba8{0, 0, 0, 255});
        const auto evidence = compare_native_canvas2d(
            binding,
            reference,
            canvas2d::evidence::canonical_image_hash(reference),
            canvas2d::presentation::full_surface(reference.extent));
        const bool evidenceRefused = evidence.code
            == canvas2d::evidence::PixelEvidenceCode::readback_unavailable;

        device.destroy(texture);
        return refused && evidenceRefused
            && device.resolve_texture(texture) == nullptr;
    }
}
#endif
