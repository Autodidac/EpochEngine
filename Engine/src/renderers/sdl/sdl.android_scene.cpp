// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <string_view>
#include <utility>
#include <vector>
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include <src/render/render.atmosphere_shader.hpp>
#include <src/render/render.cloud_shader.hpp>

module core.context;
import core.logger;
import render.lighting;
import render.preview_atmosphere;
import render.preview_grid;

#include "sdl.android_scene.hpp"

namespace epochengine::sdlcontext::android_scene
{
namespace
{
    [[nodiscard]] GLuint compile_android_preview_shader(GLenum stage, std::string_view shared,
        std::string_view body, const char* logTag)
    {
        using epochengine::rendercloudshader::gles_header;
        const auto shader = glCreateShader(stage);
        if (!shader) return 0;
        const std::array sources{gles_header.data(), shared.data(), body.data()};
        const std::array lengths{GLint(gles_header.size()), GLint(shared.size()), GLint(body.size())};
        glShaderSource(shader, body.empty() ? 2 : 3, sources.data(), lengths.data());
        glCompileShader(shader);
        GLint compiled{};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled) return shader;
        std::array<char, 2048> log{};
        glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
        epochengine::logger::error(logTag, log.data());
        glDeleteShader(shader);
        return 0;
    }

    [[nodiscard]] GLuint make_android_fullscreen_program(std::string_view fragmentShared,
        std::string_view fragmentBody, const char* logTag)
    {
        const auto vertex = compile_android_preview_shader(GL_VERTEX_SHADER,
            epochengine::rendercloudshader::vertex_source, {}, logTag);
        const auto pixel = vertex ? compile_android_preview_shader(GL_FRAGMENT_SHADER,
            fragmentShared, fragmentBody, logTag) : 0;
        const auto program = vertex && pixel ? glCreateProgram() : 0;
        if (program)
        {
            glAttachShader(program, vertex);
            glAttachShader(program, pixel);
            glLinkProgram(program);
        }
        if (vertex) glDeleteShader(vertex);
        if (pixel) glDeleteShader(pixel);
        if (!program) return 0;
        GLint linked{};
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked) return program;
        std::array<char, 2048> log{};
        glGetProgramInfoLog(program, log.size(), nullptr, log.data());
        epochengine::logger::error(logTag, log.data());
        glDeleteProgram(program);
        return 0;
    }

    class AndroidPlanetarySky final
    {
    public:
        void release() noexcept
        {
            if (program_) glDeleteProgram(program_);
            if (triangle_) glDeleteVertexArrays(1, &triangle_);
        }

        void draw(const epochengine::previewgrid::PlanetAtmosphereView& atmosphere,
            int width, int height, bool space)
        {
            if (failed_) return;
            if (!program_ && !initialize())
            {
                failed_ = true;
                epochengine::logger::error("SDL.Sky", "Android sky shader initialization failed.");
                return;
            }

            GLint previousArray{};
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousArray);
            std::array<GLint, 6> previousBlend{};
            glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlend[0]);
            glGetIntegerv(GL_BLEND_DST_RGB, &previousBlend[1]);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlend[2]);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlend[3]);
            glGetIntegerv(GL_BLEND_EQUATION_RGB, &previousBlend[4]);
            glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &previousBlend[5]);
            glBindVertexArray(triangle_);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
            glUseProgram(program_);
            const auto vector = [&](std::size_t uniform, epochengine::previewgrid::Vec3 value) {
                glUniform3f(locations_[uniform], value.x, value.y, value.z);
            };
            vector(0, atmosphere.observer);
            vector(1, atmosphere.forward);
            vector(2, atmosphere.right);
            vector(3, atmosphere.up);
            vector(4, atmosphere.sun);
            glUniform1f(locations_[5], atmosphere.thickness);
            glUniform1i(locations_[6], atmosphere.sun_visible);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDisable(GL_BLEND);
            glBlendFuncSeparate(previousBlend[0], previousBlend[1], previousBlend[2], previousBlend[3]);
            glBlendEquationSeparate(previousBlend[4], previousBlend[5]);
            glBindVertexArray(static_cast<GLuint>(previousArray));
            if (glGetError() != GL_NO_ERROR)
            {
                failed_ = true;
                epochengine::logger::error("SDL.Sky", "Android sky draw failed.");
                return;
            }
            if (width != loggedWidth_ || height != loggedHeight_)
            {
                loggedWidth_ = width;
                loggedHeight_ = height;
                using namespace epochengine;
                const auto& eye = atmosphere.observer;
                logger::get("SDL.Sky").logf(logger::LogLevel::INFO, std::source_location::current(),
                    "Android planetary sky drawn: viewport={}x{} radius_ratio={:.6f} thickness={:.6f} solar_disk={} space={}.",
                    width, height, std::hypot(eye.x, eye.y, eye.z), atmosphere.thickness,
                    atmosphere.sun_visible, space);
            }
        }

    private:
        [[nodiscard]] bool initialize()
        {
            constexpr std::string_view body = R"glsl(in vec2 vUv;
out vec4 outColor;
void main() {
    outColor=skyAtmosphere(vUv*2.0-1.0);
    if (outColor.a<=0.0) discard;
}
)glsl";
            program_ = make_android_fullscreen_program(
                epochengine::renderatmosphereshader::fragment_shared, body, "SDL.Sky");
            if (!program_) return false;
            glGenVertexArrays(1, &triangle_);
            constexpr std::array names{"uSkyEye", "uSkyForward", "uSkyRight", "uSkyUp",
                "uSkySun", "uSkyThickness", "uSkySunVisible"};
            for (std::size_t i = 0; i < names.size(); ++i)
                locations_[i] = glGetUniformLocation(program_, names[i]);
            return triangle_ != 0;
        }

        GLuint program_{}, triangle_{};
        std::array<GLint, 7> locations_{};
        int loggedWidth_{}, loggedHeight_{};
        bool failed_{};
    };

    using PreviewVertex = epochengine::previewgrid::Vertex;
    using PreviewGeometry = std::vector<PreviewVertex>;
    class AndroidCloudVolume final
    {
    public:
        void release() noexcept
        {
            if (trace_.name) glDeleteProgram(trace_.name);
            if (resolve_.name) glDeleteProgram(resolve_.name);
            if (triangle_) glDeleteVertexArrays(1, &triangle_);
            glDeleteFramebuffers(framebuffers_.size(), framebuffers_.data());
            glDeleteTextures(textures_.size(), textures_.data());
            *this = {};
        }

        [[nodiscard]] bool begin(const epochengine::previewgrid::PublishedSceneFrame& scene,
            float aspect, int width, int height,
            const std::optional<epochengine::previewgrid::PlanetAtmosphereView>& atmosphere)
        {
            namespace preview = epochengine::previewgrid;
            if (failed_ || scene.clouds.empty() || std::ranges::any_of(scene.markers,
                [](const auto& marker) { return marker.wireframe; })) return false;
            const auto volume = scene.clouds.size() == 1
                ? preview::cloud_volume_view(scene.clouds.front(), scene.camera) : std::nullopt;
            // Scene admission and viewport size can change on the next frame.
            if (!volume)
            {
                return false;
            }
            constexpr std::uint64_t maxTargetPixels = 4'194'304;
            // Bound either axis so GLSL's signed texel mapping cannot overflow.
            constexpr int maxTargetExtent = 32'768;
            GLint samples{};
            glGetIntegerv(GL_SAMPLES, &samples);
            if (width <= 0 || height <= 0 || width > maxTargetExtent || height > maxTargetExtent
                || std::uint64_t(width) * height > maxTargetPixels || samples > 0)
            {
                return false;
            }

            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &textureUnit_);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
            glActiveTexture(GL_TEXTURE0);
            if (!trace_.name)
            {
                using namespace epochengine::rendercloudshader;
                trace_ = make_pipeline(fragment_source);
                resolve_ = make_pipeline(composite_source);
                glGenVertexArrays(1, &triangle_);
                glGenFramebuffers(framebuffers_.size(), framebuffers_.data());
                glGenTextures(textures_.size(), textures_.data());
            }
            // Small changes to the fitted clipping interval do not change the
            // view. Round outward and use the exact same interval for opaque
            // depth production and the cloud's depth decoder.
            pendingCamera_ = scene.camera;
            pendingCamera_.nearPlane = outward_depth(pendingCamera_.nearPlane, false);
            pendingCamera_.farPlane = outward_depth(pendingCamera_.farPlane, true);
            const auto& camera = pendingCamera_;
            projectionView_ = preview::multiply(preview::projection_for(nullptr, aspect, camera),
                preview::look_at(camera.eye, camera.target, camera.up));
            forward_ = preview::normalize(preview::subtract(camera.target, camera.eye));
            right_ = preview::normalize(preview::cross(forward_, camera.up));
            up_ = preview::cross(right_, forward_);
            orthographic_ = camera.projection == epochengine::render_camera::ProjectionKind::orthographic;
            const float halfHeight = orthographic_ ? (std::max)(0.025f, camera.orthographicVerticalSize * 0.5f)
                : std::tan(camera.fovRadians * 0.5f);
            projection_ = {halfHeight * width / height, halfHeight};
            volume_ = *volume;
            atmosphereThickness_ = 0.0f;
            if (atmosphere && atmosphere->thickness > 0.0f)
            {
                const auto& observer = atmosphere->observer;
                const auto& center = volume_.center;
                const float distance = std::hypot(observer.x, observer.y, observer.z);
                const float delta = std::hypot(observer.x + center[0], observer.y + center[1], observer.z + center[2]);
                // The sky view is in planet radii and the cloud center points in
                // the opposite direction. Pair only views of the same body.
                if (delta <= 1e-5f * (std::max)(1.0f, distance))
                    atmosphereThickness_ = atmosphere->thickness;
            }
            region_ = trace_region(width, height);
            grid_ = trace_grid(width, height, region_);
            light_ = cloud_lighting(scene);
            if (!trace_.name || !resolve_.name || !triangle_ || !prepare_targets(width, height))
            {
                restore();
                return refuse("The Android cloud pass could not initialize its shaders or framebuffers.");
            }
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[opaque]);
            return true;
        }

        [[nodiscard]] const epochengine::previewgrid::Mat4& projection_view() const noexcept { return projectionView_; }

        void restore() const noexcept
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_));
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_));
            glBindVertexArray(static_cast<GLuint>(vertexArray_));
            glActiveTexture(static_cast<GLenum>(textureUnit_));
        }

        [[nodiscard]] bool composite(int x, int y)
        {
            const auto start = Clock::now();
            const auto& window = grid_.window;
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[accumulation]);
            glViewport(window.x, window.y, window.width, window.height);
            glDisable(GL_SCISSOR_TEST);
            glBindVertexArray(triangle_);
            const GLfloat clear[4]{};
            glClearBufferfv(GL_COLOR, 0, clear);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures_[depth]);
            glUseProgram(trace_.name);
            glUniform1i(trace_[opaqueDepth], 0);
            set_scene_uniforms(trace_);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            const bool traced = glGetError() == GL_NO_ERROR;
            if (traced)
            {
                // Use a color-only FBO so that sampling its sibling's depth
                // texture does not feed back into an attached draw resource.
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers_[resolved]);
                glViewport(0, 0, width_, height_);
                glScissor(region_.x, region_.y, region_.width, region_.height);
                glEnable(GL_SCISSOR_TEST);
                glEnable(GL_BLEND);
                glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textures_[cloudColor]);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, textures_[depth]);
                glUseProgram(resolve_.name);
                glUniform1i(resolve_[cloudTexture], 0);
                glUniform1i(resolve_[opaqueDepth], 1);
                glUniform1i(resolve_[encodeSrgb], GL_TRUE);
                glUniform1i(resolve_[repairEdges], GL_FALSE);
                set_scene_uniforms(resolve_);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glDisable(GL_SCISSOR_TEST);
            }

            if (!traced || glGetError() != GL_NO_ERROR)
            {
                restore();
                return refuse("The Android cloud trace or resolve failed; returning to opaque scene rendering.");
            }
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers_[resolved]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_));
            glBlitFramebuffer(0, 0, width_, height_, x, y, x + width_, y + height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            restore();
            if (glGetError() != GL_NO_ERROR)
                return refuse("The Android cloud pass could not present its opaque color attachment.");
            presentPending_ = true;
            if (!drawLogged_)
            {
                using namespace epochengine;
                drawLogged_ = true;
                const double elapsed = std::chrono::duration<double, std::milli>(
                    Clock::now() - start).count();
                logger::get("SDL.Clouds").logf(logger::LogLevel::INFO, std::source_location::current(),
                    "Android density presented: viewport={}x{} trace={}x{} volumes=1 submit_ms={:.3f} crop={},{},{},{}.",
                    width_, height_, window.width, window.height, elapsed, region_.x, region_.y, region_.width, region_.height);
            }
            return true;
        }

        void pause_presentation() noexcept
        {
            lastPresent_ = {};
            presentPending_ = false;
        }

        void observe_present()
        {
            if (!std::exchange(presentPending_, false))
            {
                lastPresent_ = {};
                return;
            }
            const bool full = region_.width == width_ && region_.height == height_;
            const auto& center = volume_.center;
            const double height = std::hypot(double(center[0]), double(center[1]), double(center[2])) - 1.0;
            const Layer layer = height < volume_.base ? Layer::below
                : height < volume_.base + volume_.thickness ? Layer::inside : Layer::above;
            if (modeSamples_ < 8u && (modeSamples_ == 0u || full != reportedFull_ || layer != reportedLayer_))
            {
                reportedFull_ = full;
                reportedLayer_ = layer;
                ++modeSamples_;
                const char* label = layer == Layer::below ? "below" : layer == Layer::inside ? "inside" : "above";
                using namespace epochengine;
                logger::get("SDL.Clouds").logf(logger::LogLevel::INFO, std::source_location::current(),
                    "Android density grid presented: mode={} altitude_m={:.3f} viewport={}x{} lattice={}x{} rays={} layer={}.",
                    full ? "full" : "crop", height * volume_.radius_meters, width_, height_, grid_.width, grid_.height,
                    grid_.window.width * grid_.window.height, label);
            }
            if (presentLogged_) return;
            const auto now = Clock::now();
            const auto prior = std::exchange(lastPresent_, now);
            if (prior == Clock::time_point{}) return;
            presentIntervals_[presentCount_++] = std::chrono::duration<double, std::milli>(now - prior).count();
            if (presentCount_ < presentIntervals_.size()) return;
            std::ranges::sort(presentIntervals_);
            double sum{};
            for (const auto interval : presentIntervals_) sum += interval;
            using namespace epochengine;
            logger::get("SDL.Clouds").logf(logger::LogLevel::INFO, std::source_location::current(),
                "Android density present intervals: samples={} mean_ms={:.3f} p95_ms={:.3f}.",
                presentCount_, sum / presentCount_, presentIntervals_[(presentCount_ * 95u + 99u) / 100u - 1u]);
            presentLogged_ = true;
        }

    private:
        enum class Layer { above, inside, below };
        enum Target : unsigned { opaque, accumulation, resolved, targetCount };
        enum Texture : unsigned { opaqueColor, depth, cloudColor, textureCount };
        enum Uniform : unsigned {
            opaqueDepth, traceSize, cloudTexture, encodeSrgb, repairEdges,
            forward, right, up, center, sun, sunColor, ambient, worldToBody,
            projection, nearFar, wind, evolution, orthographic, radius, worldRadius,
            base, thickness, noiseScale, coverage, extinction, lowerExtinction,
            occluder, seed, atmosphereThickness, uniformCount
        };
        static constexpr std::array<const char*, uniformCount> uniforms_{
            "uDepth", "uTraceSize", "uCloud", "uEncodeSrgb", "uRepairEdges",
            "uForward", "uRight", "uUp", "uCenter", "uSun", "uSunColor", "uAmbient", "uWorldToBody",
            "uProjection", "uNearFar", "uWind", "uEvolution", "uOrthographic", "uRadius", "uWorldRadius",
            "uBase", "uThickness", "uNoiseScale", "uCoverage", "uExtinction", "uLowerExtinction",
            "uViewOccluderRadius", "uSeed", "uCloudAtmosphereThickness"
        };
        struct Pipeline final
        {
            GLuint name{};
            std::array<GLint, uniformCount> locations{};
            [[nodiscard]] GLint operator[](Uniform uniform) const noexcept { return locations[uniform]; }
        };
        using Clock = std::chrono::steady_clock;
        using LightUniforms = std::array<std::array<float, 3>, 3>;
        struct TraceRegion final { int x{}, y{}, width{}, height{}; };
        struct TraceGrid final { TraceRegion window{}; int width{}, height{}; };
        Pipeline trace_{}, resolve_{};
        GLuint triangle_{};
        std::array<GLuint, targetCount> framebuffers_{};
        std::array<GLuint, textureCount> textures_{};
        GLint draw_{}, read_{}, textureUnit_{}, vertexArray_{};
        int width_{}, height_{};
        epochengine::previewgrid::Mat4 projectionView_{};
        epochengine::previewgrid::Camera pendingCamera_{};
        epochengine::previewgrid::CloudVolumeView volume_{};
        float atmosphereThickness_{};
        epochengine::previewgrid::Vec3 forward_{}, right_{}, up_{};
        std::array<float, 2> projection_{};
        TraceRegion region_{};
        TraceGrid grid_{};
        LightUniforms light_{};
        Clock::time_point lastPresent_{};
        std::array<double, 30> presentIntervals_{};
        std::size_t presentCount_{};
        unsigned modeSamples_{};
        Layer reportedLayer_{Layer::above};
        bool failed_{}, drawLogged_{}, presentPending_{}, presentLogged_{}, orthographic_{}, reportedFull_{};

        static constexpr int maxTracePixels = 16'384;

        [[nodiscard]] static double trace_scale(int width, int height) noexcept
        {
            return (std::min)(0.5, std::sqrt(maxTracePixels / (double(width) * height)));
        }

        [[nodiscard]] static TraceGrid trace_grid(int width, int height, TraceRegion region) noexcept
        {
            const double scale = trace_scale(width, height);
            // The edge case with one column or row still fits the ray limit.
            const int columns = (std::clamp)(int(width * scale), 1, maxTracePixels);
            const int rows = (std::clamp)(int(height * scale), 1, maxTracePixels / columns);
            const auto window = [](int first, int count, int pixels, int samples) {
                const auto preceding = [&](int pixel) {
                    return (std::max)(0, int(((2LL * pixel + 2) * samples - pixels - 1) / (2LL * pixels)));
                };
                // Include both resolve taps and cover their full fragment quads.
                const int begin = preceding(first) & ~1;
                const int end = (std::min)(samples, (preceding(first + count - 1) + 3) & ~1);
                return std::array{begin, end - begin};
            };
            const auto [x, visibleColumns] = window(region.x, region.width, width, columns);
            const auto [y, visibleRows] = window(region.y, region.height, height, rows);
            return {{x, y, visibleColumns, visibleRows}, columns, rows};
        }

        [[nodiscard]] TraceRegion trace_region(int width, int height) const noexcept
        {
            namespace preview = epochengine::previewgrid;
            const TraceRegion full{0, 0, width, height};
            const preview::Vec3 center{volume_.center[0], volume_.center[1], volume_.center[2]};
            const double forwardDistance = preview::dot(center, forward_);
            const double radius = 1.0 + volume_.base + volume_.thickness;
            if (orthographic_ || !(forwardDistance > radius) || !(projection_[0] > 0.0f && projection_[1] > 0.0f))
                return full;

            // Round the sphere's projected tangent bounds outward. Leave two
            // full-grid intervals for interpolation at the cloud boundary.
            constexpr double alignment = 32.0;
            const double padding = 2.0 / trace_scale(width, height);
            const double denominator = (forwardDistance - radius) * (forwardDistance + radius);
            const auto extent = [&](double across, double projection, int pixels) {
                const double spread = radius * std::sqrt(across * across + denominator);
                const double centerSlope = across * forwardDistance;
                const double slopeToPixel = pixels * 0.5 / (projection * denominator);
                const double low = pixels * 0.5 + (centerSlope - spread) * slopeToPixel;
                const double high = pixels * 0.5 + (centerSlope + spread) * slopeToPixel;
                if (!std::isfinite(low) || !std::isfinite(high)) return std::array{0, pixels};
                return std::array{
                    int((std::clamp)(alignment * std::floor((low - padding) / alignment), 0.0, double(pixels))),
                    int((std::clamp)(alignment * std::ceil((high + padding) / alignment), 0.0, double(pixels)))};
            };
            const auto [left, right] = extent(preview::dot(center, right_), projection_[0], width);
            const auto [bottom, top] = extent(preview::dot(center, up_), projection_[1], height);
            const int cropWidth = right - left;
            const int cropHeight = top - bottom;
            if (cropWidth <= 0 || cropHeight <= 0 || 5LL * cropWidth * cropHeight > 4LL * width * height)
                return full;
            return {left, bottom, cropWidth, cropHeight};
        }

        [[nodiscard]] static float outward_depth(float depth, bool farPlane) noexcept
        {
            int power{};
            constexpr float buckets = 8.0f;
            const float scaled = std::frexp(depth, &power) * buckets;
            const float boundary = farPlane ? std::ceil(scaled) : std::floor(scaled);
            const float rounded = std::ldexp(boundary / buckets, power);
            return std::isfinite(rounded) && rounded > 0.0f ? rounded : depth;
        }

        [[nodiscard]] bool refuse(const char* message)
        {
            failed_ = true;
            epochengine::logger::error("SDL.Clouds", message);
            return false;
        }

        [[nodiscard]] static Pipeline make_pipeline(std::string_view body)
        {
            Pipeline pipeline{};
            pipeline.name = make_android_fullscreen_program(
                epochengine::rendercloudshader::cloud_trace_source, body, "SDL.Clouds");
            if (!pipeline.name) return {};
            for (std::size_t i = 0; i < uniforms_.size(); ++i)
                pipeline.locations[i] = glGetUniformLocation(pipeline.name, uniforms_[i]);
            return pipeline;
        }

        [[nodiscard]] bool prepare_targets(int width, int height) noexcept
        {
            if (width == width_ && height == height_) return true;
            const auto texture = [&](unsigned id, GLint internal, int w, int h, GLenum format, GLenum type) {
                glBindTexture(GL_TEXTURE_2D, textures_[id]);
                glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            };
            texture(opaqueColor, GL_RGBA8, width, height, GL_RGBA, GL_UNSIGNED_BYTE);
            texture(depth, GL_DEPTH_COMPONENT24, width, height, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
            texture(cloudColor, GL_RGBA8, grid_.width, grid_.height, GL_RGBA, GL_UNSIGNED_BYTE);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[opaque]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures_[opaqueColor], 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures_[depth], 0);
            const bool opaqueReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[resolved]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures_[opaqueColor], 0);
            const bool resolvedReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[accumulation]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures_[cloudColor], 0);
            const bool cloudReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            if (glGetError() != GL_NO_ERROR || !opaqueReady || !resolvedReady || !cloudReady) return false;
            width_ = width;
            height_ = height;
            return true;
        }

        [[nodiscard]] static LightUniforms cloud_lighting(
            const epochengine::previewgrid::PublishedSceneFrame& scene) noexcept
        {
            using namespace epochengine;
            const auto& frame = scene.solids->lighting;
            previewgrid::Vec3 direction{0, 1, 0};
            std::array<float, 3> color{};
            float strongest{};
            for (const auto& light : frame.lights)
                if (light.desc.enabled && light.desc.kind == lighting::LightKind::Directional
                    && light.desc.intensity > strongest)
                {
                    strongest = light.desc.intensity;
                    direction = {-light.desc.direction.x, -light.desc.direction.y, -light.desc.direction.z};
                    color = {light.desc.color.r * strongest, light.desc.color.g * strongest,
                        light.desc.color.b * strongest};
                }
            direction = previewgrid::normalize(direction);
            return {{{direction.x, direction.y, direction.z}, color,
                {frame.environment.ambient.r, frame.environment.ambient.g, frame.environment.ambient.b}}};
        }

        void set_scene_uniforms(const Pipeline& pipeline) const noexcept
        {
            using namespace epochengine;
            namespace preview = previewgrid;
            const auto& camera = pendingCamera_;
            const auto vector = [&](Uniform uniform, preview::Vec3 v) { glUniform3f(pipeline[uniform], v.x, v.y, v.z); };
            glUniform2f(pipeline[nearFar], camera.nearPlane, camera.farPlane);
            glUniform1i(pipeline[orthographic], orthographic_);
            glUniform2fv(pipeline[projection], 1, projection_.data());
            glUniform2i(pipeline[traceSize], grid_.width, grid_.height);
            vector(forward, forward_);
            vector(right, right_);
            vector(up, up_);
            glUniform3fv(pipeline[sun], 1, light_[0].data());
            glUniform3fv(pipeline[sunColor], 1, light_[1].data());
            glUniform3fv(pipeline[ambient], 1, light_[2].data());

            const auto& volume = volume_;
            vector(center, {volume.center[0], volume.center[1], volume.center[2]});
            glUniformMatrix3fv(pipeline[worldToBody], 1, GL_FALSE, volume.world_to_body.data());
            glUniform1f(pipeline[radius], volume.radius_meters);
            glUniform1f(pipeline[worldRadius], volume.radius_world);
            glUniform1f(pipeline[base], volume.base);
            glUniform1f(pipeline[thickness], volume.thickness);
            glUniform1f(pipeline[occluder], volume.view_occluder_radius);
            glUniform1f(pipeline[atmosphereThickness], atmosphereThickness_);
            glUniform1f(pipeline[noiseScale], volume.noise_scale);
            glUniform1f(pipeline[coverage], volume.coverage);
            glUniform1f(pipeline[extinction], volume.extinction);
            glUniform1f(pipeline[lowerExtinction], volume.lower_extinction);
            glUniform1ui(pipeline[seed], volume.seed);
            glUniform2f(pipeline[wind], volume.wind[0], volume.wind[1]);
            glUniform2f(pipeline[evolution], volume.evolution[0], volume.evolution[1]);
        }
    };

    // SDL's 2D geometry drops Z. Draw the existing opaque scene batch into
    // the SDL window's GLES depth buffer before SDL composites the overlays.
    class AndroidSceneDepth final
    {
    public:
        void release() noexcept
        {
            if (context_ && context_ == SDL_GL_GetCurrentContext())
            {
                clouds_.release();
                sky_.release();
                if (buffer_) glDeleteBuffers(1, &buffer_);
                if (program_) glDeleteProgram(program_);
            }
            *this = {};
        }

        void pause_presentation() noexcept { clouds_.pause_presentation(); }
        void observe_present() { clouds_.observe_present(); }

        [[nodiscard]] bool draw(const Surface& surface, const epochengine::core::RenderViewport& viewport,
            const epochengine::previewgrid::Mat4& mvp,
            const epochengine::previewgrid::PublishedSceneFrame& scene, bool densityClouds)
        {
            const auto& geometry = scene.solids->geometry;
            if (geometry->empty() || (failed_ && context_ == SDL_GL_GetCurrentContext())
                || SDL_GetRenderTarget(surface.renderer)
                || geometry->size() > static_cast<std::size_t>((std::numeric_limits<GLsizei>::max)())
                || !SDL_FlushRenderer(surface.renderer) || SDL_GL_GetCurrentWindow() != surface.window)
                return false;

            const auto context = SDL_GL_GetCurrentContext();
            if (!context) return false;
            if (context_ != context)
            {
                release();
                context_ = context;
                GLint depthBits{};
                glGetIntegerv(GL_DEPTH_BITS, &depthBits);
                if (depthBits < 16 || !build_program()) return disable();
                epochengine::logger::get("SDL").logf(epochengine::logger::LogLevel::INFO,
                    std::source_location::current(), "Android opaque scene depth active ({} bits).", depthBits);
            }

            float left{}, top{}, right{}, bottom{};
            if (!SDL_RenderCoordinatesToWindow(surface.renderer, static_cast<float>(viewport.x),
                    static_cast<float>(viewport.y), &left, &top)
                || !SDL_RenderCoordinatesToWindow(surface.renderer, static_cast<float>(viewport.x + viewport.width),
                    static_cast<float>(viewport.y + viewport.height), &right, &bottom)
                || surface.width <= 0 || surface.height <= 0 || !std::isfinite(left)
                || !std::isfinite(top) || !std::isfinite(right) || !std::isfinite(bottom))
                return false;
            const auto pixel = [](float coordinate, int logical, int drawable) {
                return static_cast<GLint>(std::lround((std::clamp)(coordinate, 0.0f,
                    static_cast<float>(logical)) * drawable / logical));
            };
            const auto x = pixel(left, surface.width, surface.framebufferWidth);
            const auto y = surface.framebufferHeight - pixel(bottom, surface.height, surface.framebufferHeight);
            const auto width = pixel(right, surface.width, surface.framebufferWidth) - x;
            const auto height = surface.framebufferHeight - pixel(top, surface.height, surface.framebufferHeight) - y;
            if (width <= 0 || height <= 0) return false;

            const float aspect = static_cast<float>(viewport.width) / viewport.height;
            const auto atmosphere = scene.camera.projection == epochengine::render_camera::ProjectionKind::perspective
                ? epochengine::previewgrid::planet_atmosphere_view(scene, aspect) : std::nullopt;
            const bool cloudTarget = densityClouds && clouds_.begin(scene, aspect, width, height, atmosphere);
            glBindBuffer(GL_ARRAY_BUFFER, buffer_);
            if (uploaded_.lock() != geometry)
            {
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(geometry->size() * sizeof(PreviewVertex)),
                    geometry->data(), GL_STATIC_DRAW);
                if (glGetError() != GL_NO_ERROR)
                {
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    if (cloudTarget) clouds_.restore();
                    return disable();
                }
                uploaded_ = geometry;
            }

            const int drawX = cloudTarget ? 0 : x;
            const int drawY = cloudTarget ? 0 : y;
            glViewport(drawX, drawY, width, height);
            glScissor(drawX, drawY, width, height);
            glEnable(GL_SCISSOR_TEST);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glClearDepthf(1.0f);
            const bool clearBackground = cloudTarget || atmosphere.has_value();
            if (clearBackground)
            {
                const auto background = epochengine::previewgrid::scene_clear_color(&scene);
                glClearColor(background[0], background[1], background[2], background[3]);
            }
            glClear(clearBackground ? GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT : GL_DEPTH_BUFFER_BIT);
            if (atmosphere) sky_.draw(*atmosphere, width, height, scene.space_background);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glFrontFace(GL_CW);
            glUseProgram(program_);
            glUniformMatrix4fv(mvp_, 1, GL_FALSE, (cloudTarget ? clouds_.projection_view() : mvp).data());
            bind_vertices(position_, offsetof(PreviewVertex, position));
            bind_vertices(color_, offsetof(PreviewVertex, color));
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(geometry->size()));

            // The SDL flush invalidated its viewport, scissor, blend and
            // program cache. Restore depth/culling and the two private inputs,
            // which its 2D renderer does not reset between draw batches.
            glDisableVertexAttribArray(position_);
            glDisableVertexAttribArray(color_);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glFrontFace(GL_CCW);
            if (glGetError() != GL_NO_ERROR)
            {
                if (cloudTarget) clouds_.restore();
                return disable();
            }
            if (cloudTarget && !clouds_.composite(x, y))
                return draw(surface, viewport, mvp, scene, false);
            return true;
        }

    private:
        static constexpr GLuint position_ = 6;
        static constexpr GLuint color_ = 7;
        SDL_GLContext context_{};
        GLuint program_{};
        GLuint buffer_{};
        GLint mvp_ = -1;
        bool failed_{};
        std::weak_ptr<const PreviewGeometry> uploaded_{};
        AndroidCloudVolume clouds_{};
        AndroidPlanetarySky sky_{};

        [[nodiscard]] bool disable()
        {
            failed_ = true;
            epochengine::logger::error("SDL", "Android scene depth unavailable. Using SDL geometry.");
            return false;
        }

        static void bind_vertices(GLuint attribute, std::size_t offset) noexcept
        {
            glEnableVertexAttribArray(attribute);
            glVertexAttribPointer(attribute, 3, GL_FLOAT, GL_FALSE, sizeof(PreviewVertex),
                reinterpret_cast<const void*>(offset));
        }

        [[nodiscard]] static GLuint compile_shader(GLenum type, const char* source) noexcept
        {
            const auto shader = glCreateShader(type);
            if (!shader) return 0;
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            GLint compiled{};
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (compiled) return shader;
            glDeleteShader(shader);
            return 0;
        }

        [[nodiscard]] bool build_program() noexcept
        {
            const auto vertex = compile_shader(GL_VERTEX_SHADER,
                "attribute vec3 a_position; attribute vec3 a_color; uniform mat4 u_mvp;"
                "varying mediump vec3 v_color;"
                "void main(){gl_Position=u_mvp*vec4(a_position,1.0);v_color=a_color;}");
            const auto fragment = compile_shader(GL_FRAGMENT_SHADER,
                "varying mediump vec3 v_color; void main(){gl_FragColor=vec4(clamp(v_color,0.0,1.0),1.0);}");
            if (vertex && fragment) program_ = glCreateProgram();
            if (program_)
            {
                glAttachShader(program_, vertex);
                glAttachShader(program_, fragment);
                glBindAttribLocation(program_, position_, "a_position");
                glBindAttribLocation(program_, color_, "a_color");
                glLinkProgram(program_);
            }
            if (vertex) glDeleteShader(vertex);
            if (fragment) glDeleteShader(fragment);
            if (!program_) return false;
            GLint linked{};
            glGetProgramiv(program_, GL_LINK_STATUS, &linked);
            if (!linked) return false;
            mvp_ = glGetUniformLocation(program_, "u_mvp");
            glGenBuffers(1, &buffer_);
            return mvp_ >= 0 && buffer_ != 0;
        }
    };

    AndroidSceneDepth sceneDepth{};
}

bool draw(const Surface& surface, const core::RenderViewport& viewport,
    const previewgrid::Mat4& mvp, const previewgrid::PublishedSceneFrame& scene, bool densityClouds)
{
    return sceneDepth.draw(surface, viewport, mvp, scene, densityClouds);
}

void release() noexcept { sceneDepth.release(); }
void pause_presentation() noexcept { sceneDepth.pause_presentation(); }
void observe_present() { sceneDepth.observe_present(); }
}
