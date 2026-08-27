module;

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <include/engine.config.hpp>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <wingdi.h>
#endif

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
#include "../modules/sfml.compat.hpp"
#endif

module core.context;

import context.multiplexer;
import context.type;

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
import gui.engine;
import input.engine;
import atlas.manager;
import atlas.texture;
import context.commandqueue;
import core.logger;
import image.loader;
import package.registry;
import render.arcade;
import render.preview_grid;
import render.canvas2d_limits;
import render.canvas2d_presentation;
import render.canvas2d_runtime;
import render.device;
import render.device_sfml;
import sfml.state;
import sfml.textures;

namespace
{
    std::unique_ptr<sf::RenderWindow> s_window{};
    epochengine::sfml_compat::ArcadePreviewSurface s_arcadePreviewSurface{};
    int s_width = 0;
    int s_height = 0;
    bool s_logged_activate_failure = false;

#if defined(_WIN32)
    HWND s_hostWindow = nullptr;
    HWND s_childWindow = nullptr;
    HWND s_dockParent = nullptr;
    HDC s_hdc = nullptr;
    HGLRC s_glContext = nullptr;
#endif

    enum class CanvasSceneStatus : std::uint8_t
    {
        missing_scene,
        presented,
        refused
    };

    class SfmlCanvas2DPresenter final
    {
    public:
        SfmlCanvas2DPresenter(
            const epochengine::core::Context* owner,
            std::uint64_t backendEpoch)
            : owner_(owner),
              execution_limits_(
                  epochengine::canvas2d::limits::for_backend(
                      epochengine::RendererBackendKind::sfml3)),
              presenter_(
                  device_,
                  backendEpoch,
                  {this, &SfmlCanvas2DPresenter::dispatch_present},
                  execution_limits_.residency)
        {
        }

        [[nodiscard]] CanvasSceneStatus render(
            const std::shared_ptr<epochengine::core::Context>& ctx,
            const epochengine::core::RenderViewport& viewport)
        {
            if (!context_ready(ctx.get()))
                return refuse("missing_or_stale_context");

            const auto prepared = session_.prepare(
                owner_,
                {
                    static_cast<std::uint32_t>(viewport.width),
                    static_cast<std::uint32_t>(viewport.height)},
                execution_limits_.canvas,
                execution_limits_.raster);
            if (prepared.code
                == epochengine::canvas2d::runtime::PrepareCode::missing_scene)
            {
                presenter_.retire_all();
                refusal_logged_ = false;
                return CanvasSceneStatus::missing_scene;
            }
            if ((prepared.code
                    != epochengine::canvas2d::runtime::PrepareCode::ready
                && prepared.code
                    != epochengine::canvas2d::runtime::PrepareCode::reused)
                || !prepared.frame || !prepared.raster)
            {
                return refuse(
                    epochengine::canvas2d::runtime::prepare_code_name(
                        prepared.code));
            }

            const sf::Vector2u framebuffer = s_window->getSize();
            if (framebuffer.x == 0u || framebuffer.y == 0u)
                return refuse("invalid_framebuffer_extent");

            const epochengine::canvas2d::presentation::PresentationSurface surface{
                {framebuffer.x, framebuffer.y},
                {
                    viewport.x,
                    viewport.y,
                    static_cast<std::uint32_t>(viewport.width),
                    static_cast<std::uint32_t>(viewport.height)}};
            try
            {
                const auto result = presenter_.present(
                    *prepared.frame,
                    *prepared.raster,
                    surface);
                if (result.code
                    != epochengine::canvas2d::presentation::PresentationCode::presented)
                {
                    return refuse(
                        epochengine::canvas2d::presentation::presentation_code_name(
                            result.code));
                }
            }
            catch (...)
            {
                return refuse("presentation_allocation_failure");
            }

            refusal_logged_ = false;
            return CanvasSceneStatus::presented;
        }

        void retire() noexcept
        {
            presenter_.retire_all();
            session_.reset();
            owner_ = nullptr;
            refusal_logged_ = false;
        }

        [[nodiscard]] const epochengine::core::Context* owner() const noexcept
        {
            return owner_;
        }

    private:
        [[nodiscard]] static bool dispatch_present(
            void* user,
            const epochengine::canvas2d::presentation::NativePresentationPacket& packet)
        {
            auto* const self = static_cast<SfmlCanvas2DPresenter*>(user);
            return self && self->present_native(packet);
        }

        [[nodiscard]] bool context_ready(
            const epochengine::core::Context* ctx) const noexcept
        {
            const auto& state = epochengine::sfmlcontext::state::s_sfmlstate;
            return owner_ && ctx == owner_ && s_window && s_window->isOpen()
                && state.get_sfml_window() == s_window.get() && state.running
                && !state.shouldClose && !state.window.get_should_close();
        }

        [[nodiscard]] bool present_native(
            const epochengine::canvas2d::presentation::NativePresentationPacket& packet)
        {
            if (packet.texture.value == 0u || packet.compose == nullptr
                || !context_ready(owner_))
                return false;

            const epochengine::SfmlTextureRecord* const record =
                device_.resolve_texture(packet.texture);
            if (!record || !record->texture || !record->ready
                || record->desc.width != packet.image.extent.width
                || record->desc.height != packet.image.extent.height)
            {
                return false;
            }

            const sf::Vector2u framebuffer = s_window->getSize();
            if (framebuffer.x != packet.surface.framebuffer_extent.width
                || framebuffer.y != packet.surface.framebuffer_extent.height)
            {
                return false;
            }

            const auto& compose = *packet.compose;
            const auto& destination = compose.viewport.clipped_destination;
            const auto& visible = compose.viewport.visible_canvas;
            if (destination.empty() || destination.x < 0 || destination.y < 0
                || visible.width <= 0.0f || visible.height <= 0.0f)
            {
                return false;
            }

            const float invWidth = 1.0f / static_cast<float>(framebuffer.x);
            const float invHeight = 1.0f / static_cast<float>(framebuffer.y);
            const sf::FloatRect normalizedViewport =
                epochengine::sfml_compat::float_rect(
                    packet.surface.viewport.x * invWidth,
                    packet.surface.viewport.y * invHeight,
                    packet.surface.viewport.width * invWidth,
                    packet.surface.viewport.height * invHeight);

            const float outputWidth =
                static_cast<float>(packet.surface.viewport.width);
            const float outputHeight =
                static_cast<float>(packet.surface.viewport.height);
            const sf::Color white(255u, 255u, 255u, 255u);
            const float left = static_cast<float>(destination.x);
            const float top = static_cast<float>(destination.y);
            const float right = left + static_cast<float>(destination.width);
            const float bottom = top + static_cast<float>(destination.height);
            const float u0 = visible.x;
            const float v0 = visible.y;
            const float u1 = visible.x + visible.width;
            const float v1 = visible.y + visible.height;
            sf::VertexArray content(sf::PrimitiveType::Triangles);
            content.append(sf::Vertex({left, top}, white, {u0, v0}));
            content.append(sf::Vertex({right, top}, white, {u1, v0}));
            content.append(sf::Vertex({right, bottom}, white, {u1, v1}));
            content.append(sf::Vertex({left, top}, white, {u0, v0}));
            content.append(sf::Vertex({right, bottom}, white, {u1, v1}));
            content.append(sf::Vertex({left, bottom}, white, {u0, v1}));

            sf::VertexArray clearSurface(sf::PrimitiveType::Triangles);
            sf::RenderStates clearStates{};
            if (compose.clear_letterbox)
            {
                const auto toUnorm8 = [](float value) noexcept -> std::uint8_t
                {
                    return static_cast<std::uint8_t>(std::lround(
                        (std::clamp)(value, 0.0f, 1.0f) * 255.0f));
                };
                const sf::Color letterbox(
                    toUnorm8(compose.letterbox_color.r),
                    toUnorm8(compose.letterbox_color.g),
                    toUnorm8(compose.letterbox_color.b),
                    toUnorm8(compose.letterbox_color.a));
                clearSurface.append(sf::Vertex({0.0f, 0.0f}, letterbox));
                clearSurface.append(sf::Vertex({outputWidth, 0.0f}, letterbox));
                clearSurface.append(sf::Vertex(
                    {outputWidth, outputHeight}, letterbox));
                clearSurface.append(sf::Vertex({0.0f, 0.0f}, letterbox));
                clearSurface.append(sf::Vertex(
                    {outputWidth, outputHeight}, letterbox));
                clearSurface.append(sf::Vertex({0.0f, outputHeight}, letterbox));
                clearStates.blendMode = sf::BlendNone;
            }

            record->texture->setSmooth(
                compose.presentation_filter
                    != epochengine::FilterMode::nearest);
            sf::RenderStates states{};
            states.texture = record->texture.get();
#if EPOCH_SFML_HAS_V3_API
            states.blendMode = sf::BlendMode(
                sf::BlendMode::Factor::One,
                sf::BlendMode::Factor::OneMinusSrcAlpha,
                sf::BlendMode::Equation::Add,
                sf::BlendMode::Factor::One,
                sf::BlendMode::Factor::OneMinusSrcAlpha,
                sf::BlendMode::Equation::Add);
#else
            states.blendMode = sf::BlendMode(
                sf::BlendMode::One,
                sf::BlendMode::OneMinusSrcAlpha,
                sf::BlendMode::Add,
                sf::BlendMode::One,
                sf::BlendMode::OneMinusSrcAlpha,
                sf::BlendMode::Add);
#endif

            const sf::View previousView = s_window->getView();
            sf::View canvasView{epochengine::sfml_compat::float_rect(
                0.0f,
                0.0f,
                outputWidth,
                outputHeight)};
            canvasView.setViewport(normalizedViewport);
            s_window->setView(canvasView);
            if (compose.clear_letterbox)
                s_window->draw(clearSurface, clearStates);
            s_window->draw(content, states);
            s_window->setView(previousView);
            return true;
        }

        [[nodiscard]] CanvasSceneStatus refuse(std::string_view reason)
        {
            if (!refusal_logged_)
            {
                epochengine::logger::warn(
                    "SFML.Canvas2D",
                    std::string("Presentation refused: ") + std::string(reason));
                refusal_logged_ = true;
            }
            return CanvasSceneStatus::refused;
        }

        const epochengine::core::Context* owner_{};
        epochengine::canvas2d::limits::NativeExecutionLimits execution_limits_{};
        epochengine::SfmlRenderDevice device_{};
        epochengine::canvas2d::runtime::SceneRasterSession session_{};
        epochengine::canvas2d::presentation::Canvas2DPresenter presenter_;
        bool refusal_logged_{};
    };

    std::unique_ptr<SfmlCanvas2DPresenter> s_canvas2d_presenter{};
    std::atomic<std::uint64_t> s_next_canvas2d_backend_epoch{1};

    [[nodiscard]] CanvasSceneStatus render_canvas2d_scene(
        const std::shared_ptr<epochengine::core::Context>& ctx,
        const epochengine::core::RenderViewport& viewport)
    {
        if (!ctx)
            return CanvasSceneStatus::refused;
        if (s_canvas2d_presenter && s_canvas2d_presenter->owner() != ctx.get())
        {
            s_canvas2d_presenter->retire();
            s_canvas2d_presenter.reset();
        }
        if (!s_canvas2d_presenter)
        {
            try
            {
                std::uint64_t epoch = s_next_canvas2d_backend_epoch.fetch_add(
                    1,
                    std::memory_order_relaxed);
                if (epoch == 0u)
                {
                    epoch = s_next_canvas2d_backend_epoch.fetch_add(
                        1,
                        std::memory_order_relaxed);
                }
                s_canvas2d_presenter =
                    std::make_unique<SfmlCanvas2DPresenter>(
                        ctx.get(),
                        epoch);
            }
            catch (...)
            {
                return CanvasSceneStatus::refused;
            }
        }
        return s_canvas2d_presenter->render(ctx, viewport);
    }

    void release_canvas2d_presenter() noexcept
    {
        if (s_canvas2d_presenter)
            s_canvas2d_presenter->retire();
        s_canvas2d_presenter.reset();
    }

    std::uint32_t default_add_texture(
        epochengine::TextureAtlas&,
        std::string,
        const epochengine::ImageData&) noexcept
    {
        return 0u;
    }

    std::uint32_t default_add_atlas(const epochengine::TextureAtlas& atlas) noexcept
    {
        const int idx = atlas.get_index();
        return static_cast<std::uint32_t>(idx >= 0 ? idx + 1 : 1);
    }

    void bind_default_input(const std::shared_ptr<epochengine::core::Context>& ctx)
    {
        ctx->is_key_held = [](epochengine::input::Key key) { return epochengine::input::is_key_held(key); };
        ctx->is_key_down = [](epochengine::input::Key key) { return epochengine::input::is_key_down(key); };
        ctx->get_mouse_position = [](int& x, int& y)
        {
            x = epochengine::input::mouseX.load(std::memory_order_relaxed);
            y = epochengine::input::mouseY.load(std::memory_order_relaxed);
        };
        ctx->is_mouse_button_held = [](epochengine::input::MouseButton button) { return epochengine::input::is_mouse_button_held(button); };
        ctx->is_mouse_button_down = [](epochengine::input::MouseButton button) { return epochengine::input::is_mouse_button_down(button); };
    }

    [[nodiscard]] sf::Color to_sfml_color(const epochengine::previewgrid::Vec3& color) noexcept
    {
        const auto clamp_channel = [](float value) noexcept -> std::uint8_t
        {
            const float scaled = (std::clamp)(value, 0.0f, 1.0f) * 255.0f;
            return static_cast<std::uint8_t>(scaled);
        };

        return sf::Color(
            clamp_channel(color.x),
            clamp_channel(color.y),
            clamp_channel(color.z));
    }

    [[nodiscard]] bool project_preview_vertex(
        const epochengine::previewgrid::Mat4& mvp,
        const epochengine::previewgrid::Vec3& position,
        const epochengine::core::RenderViewport& viewport,
        sf::Vector2f& out) noexcept
    {
        const auto clip = epochengine::previewgrid::transform_point(mvp, position);
        if (clip.w <= 1.0e-4f)
            return false;

        const float invW = 1.0f / clip.w;
        const float ndcX = clip.x * invW;
        const float ndcY = clip.y * invW;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
            return false;

        out.x = static_cast<float>(viewport.x)
            + ((ndcX * 0.5f) + 0.5f) * static_cast<float>(viewport.width);
        out.y = static_cast<float>(viewport.y)
            + ((-ndcY * 0.5f) + 0.5f) * static_cast<float>(viewport.height);
        return true;
    }

    [[nodiscard]] constexpr bool sfml_arcade_sampled_preview_contract() noexcept
    {
        constexpr auto route = epochengine::previewgrid::object_preview_geometry_route(
            epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen);
        return route.sampled_surface
            && !route.solid_scene
            && !route.marker_wire
            && epochengine::render_arcade::kScreenSceneNode.sampled_render_surface
            && !epochengine::render_arcade::kScreenSceneNode.diagnostic_overlay
            && epochengine::package_registry::engine_arcade_render_texture_width() > 0u
            && epochengine::package_registry::engine_arcade_render_texture_height() > 0u;
    }

    static_assert(sfml_arcade_sampled_preview_contract());

    [[nodiscard]] bool update_arcade_preview_surface() noexcept
    {
        if (!s_window)
            return false;

        const auto descriptor = epochengine::render_arcade::make_screen_render_texture_desc();
        if (!s_arcadePreviewSurface.ensure(*s_window, descriptor.width, descriptor.height)
            || !s_arcadePreviewSurface.begin_update(*s_window))
        {
            return false;
        }

        epochengine::render_arcade::emit_arcade_attract_pattern(
            static_cast<int>(descriptor.width),
            static_cast<int>(descriptor.height),
            s_arcadePreviewSurface.frame_number(),
            [](const epochengine::render_arcade::ArcadePreviewRect& rect)
            {
                s_arcadePreviewSurface.fill(
                    rect.x,
                    rect.y,
                    rect.width,
                    rect.height,
                    rect.color);
            });
        return s_arcadePreviewSurface.end_update(*s_window);
    }

    void render_engine_arcade_sampled_surface_preview(
        const std::shared_ptr<epochengine::core::Context>& ctx,
        const epochengine::previewgrid::Mat4& mvp,
        const epochengine::core::RenderViewport& viewport)
    {
        if (!ctx || !s_window)
            return;

        const auto markers = epochengine::previewgrid::sampled_render_surface_markers_for(ctx.get());
        if (markers.empty() || !update_arcade_preview_surface())
            return;

        const sf::Texture* const texture = s_arcadePreviewSurface.texture();
        if (!texture)
            return;

        for (const auto& marker : markers)
        {
            if (marker.primitive != epochengine::previewgrid::ObjectPreviewPrimitive::EngineArcadeScreen)
                continue;

            const float halfX = (std::max)(std::abs(marker.scale.x) * 0.5f, 0.25f);
            const float halfY = (std::max)(std::abs(marker.scale.y) * 0.5f, 0.18f);
            const float z =
                epochengine::render_arcade::screen_sample_plane_z(
                    marker.position.z, marker.scale.z);
            const epochengine::previewgrid::Vec3 world[4]{
                { marker.position.x - halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y - halfY, z },
                { marker.position.x + halfX, marker.position.y + halfY, z },
                { marker.position.x - halfX, marker.position.y + halfY, z }
            };
            sf::Vector2f projected[4]{};
            bool visible = true;
            for (std::size_t i = 0; i < 4u; ++i)
            {
                if (!project_preview_vertex(mvp, world[i], viewport, projected[i]))
                {
                    visible = false;
                    break;
                }
                projected[i].x -= static_cast<float>(viewport.x);
                projected[i].y -= static_cast<float>(viewport.y);
            }
            if (!visible)
                continue;

            const float width = static_cast<float>(s_arcadePreviewSurface.width());
            const float height = static_cast<float>(s_arcadePreviewSurface.height());
            const sf::Color white(255u, 255u, 255u, 255u);
            sf::VertexArray surface(sf::PrimitiveType::Triangles);
            surface.append(sf::Vertex(projected[0], white, sf::Vector2f(0.0f, height)));
            surface.append(sf::Vertex(projected[1], white, sf::Vector2f(width, height)));
            surface.append(sf::Vertex(projected[2], white, sf::Vector2f(width, 0.0f)));
            surface.append(sf::Vertex(projected[0], white, sf::Vector2f(0.0f, height)));
            surface.append(sf::Vertex(projected[2], white, sf::Vector2f(width, 0.0f)));
            surface.append(sf::Vertex(projected[3], white, sf::Vector2f(0.0f, 0.0f)));

            sf::RenderStates sampledStates{};
            sampledStates.texture = texture;
            s_window->draw(surface, sampledStates);
        }
    }

    void render_scene_preview(const std::shared_ptr<epochengine::core::Context>& ctx)
    {
        if (!ctx || !s_window)
            return;

        const auto viewport = ctx->scene_viewport();
        if (!viewport.valid() || ctx->scene_preview_mode() != epochengine::core::ScenePreviewMode::Editor)
            return;


        const CanvasSceneStatus canvasStatus = render_canvas2d_scene(ctx, viewport);
        if (canvasStatus != CanvasSceneStatus::missing_scene)
            return;
        const auto windowSize = s_window->getSize();
        if (windowSize.x == 0u || windowSize.y == 0u)
            return;

        const float invWidth = 1.0f / static_cast<float>(windowSize.x);
        const float invHeight = 1.0f / static_cast<float>(windowSize.y);
        const float viewportLeft = (std::clamp)(viewport.x * invWidth, 0.0f, 1.0f);
        const float viewportTop = (std::clamp)(viewport.y * invHeight, 0.0f, 1.0f);
        const float viewportWidth = (std::clamp)(viewport.width * invWidth, 0.0f, 1.0f - viewportLeft);
        const float viewportHeight = (std::clamp)(viewport.height * invHeight, 0.0f, 1.0f - viewportTop);

        const auto previousView = s_window->getView();
        sf::View previewView{ epochengine::sfml_compat::float_rect(
            0.0f,
            0.0f,
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)) };
        previewView.setViewport(epochengine::sfml_compat::float_rect(
            viewportLeft,
            viewportTop,
            viewportWidth,
            viewportHeight));
        s_window->setView(previewView);

        const auto clearColor = epochengine::previewgrid::kClearColor;
        sf::RectangleShape background{};
        background.setPosition(sf::Vector2f(0.0f, 0.0f));
        background.setSize(sf::Vector2f(
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)));
        background.setFillColor(sf::Color(
            static_cast<std::uint8_t>(clearColor[0] * 255.0f),
            static_cast<std::uint8_t>(clearColor[1] * 255.0f),
            static_cast<std::uint8_t>(clearColor[2] * 255.0f),
            static_cast<std::uint8_t>(clearColor[3] * 255.0f)));
        sf::RenderStates renderStates{};
        s_window->draw(background, renderStates);

        const auto camera = epochengine::previewgrid::camera_for(ctx.get());
        const float aspect = viewport.height > 0
            ? (viewport.width / static_cast<float>(viewport.height))
            : 1.0f;
        const auto proj = epochengine::previewgrid::projection_for(ctx.get(), aspect, camera);
        const auto view = epochengine::previewgrid::look_at(
            camera.eye,
            camera.target,
            camera.up);
        const auto mvp = epochengine::previewgrid::multiply(proj, view);
        const auto gridGeometry = epochengine::previewgrid::grid_geometry_for(ctx.get());
        const auto& vertices = gridGeometry->vertices;
        const auto& indices = gridGeometry->indices;
        sf::VertexArray lines(sf::PrimitiveType::Lines);

        for (std::size_t i = 0; i + 1 < indices.size(); i += 2)
        {
            const auto firstIndex = static_cast<std::size_t>(indices[i]);
            const auto secondIndex = static_cast<std::size_t>(indices[i + 1]);
            if (firstIndex >= vertices.size() || secondIndex >= vertices.size())
                continue;

            sf::Vector2f a{};
            sf::Vector2f b{};
            if (!project_preview_vertex(mvp, vertices[firstIndex].position, viewport, a)
                || !project_preview_vertex(mvp, vertices[secondIndex].position, viewport, b))
            {
                continue;
            }

            a.x -= static_cast<float>(viewport.x);
            a.y -= static_cast<float>(viewport.y);
            b.x -= static_cast<float>(viewport.x);
            b.y -= static_cast<float>(viewport.y);

            lines.append(sf::Vertex(a, to_sfml_color(vertices[firstIndex].color)));
            lines.append(sf::Vertex(b, to_sfml_color(vertices[firstIndex].color)));
        }

        if (lines.getVertexCount() > 0)
            s_window->draw(lines, renderStates);

        sf::VertexArray solids(sf::PrimitiveType::Triangles);
        const auto solidVertices = epochengine::previewgrid::object_solid_vertices_for(ctx.get());
        for (std::size_t i = 0; i + 2 < solidVertices.size(); i += 3)
        {
            if (!epochengine::previewgrid::clockwise_solid_triangle_faces_camera(
                    solidVertices[i].position,
                    solidVertices[i + 1].position,
                    solidVertices[i + 2].position,
                    camera.eye))
            {
                continue;
            }
            sf::Vector2f a{};
            sf::Vector2f b{};
            sf::Vector2f c{};
            if (!project_preview_vertex(mvp, solidVertices[i].position, viewport, a)
                || !project_preview_vertex(mvp, solidVertices[i + 1].position, viewport, b)
                || !project_preview_vertex(mvp, solidVertices[i + 2].position, viewport, c))
            {
                continue;
            }

            a.x -= static_cast<float>(viewport.x);
            a.y -= static_cast<float>(viewport.y);
            b.x -= static_cast<float>(viewport.x);
            b.y -= static_cast<float>(viewport.y);
            c.x -= static_cast<float>(viewport.x);
            c.y -= static_cast<float>(viewport.y);

            const auto color = to_sfml_color(solidVertices[i].color);
            solids.append(sf::Vertex(a, color));
            solids.append(sf::Vertex(b, color));
            solids.append(sf::Vertex(c, color));
        }
        if (solids.getVertexCount() > 0)
            s_window->draw(solids, renderStates);

        render_engine_arcade_sampled_surface_preview(ctx, mvp, viewport);

        const auto markerVertices = epochengine::previewgrid::look_marker_vertices_for(ctx.get());
        const std::size_t markerCount = epochengine::previewgrid::look_marker_vertex_count_for(ctx.get());
        if (markerCount > 0)
        {
            sf::VertexArray markerLines(sf::PrimitiveType::Lines);
            for (std::size_t i = 0; i + 1 < markerCount; i += 2)
            {
                sf::Vector2f a{};
                sf::Vector2f b{};
                if (!project_preview_vertex(mvp, markerVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, markerVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                a.x -= static_cast<float>(viewport.x);
                a.y -= static_cast<float>(viewport.y);
                b.x -= static_cast<float>(viewport.x);
                b.y -= static_cast<float>(viewport.y);

                markerLines.append(sf::Vertex(a, to_sfml_color(markerVertices[i].color)));
                markerLines.append(sf::Vertex(b, to_sfml_color(markerVertices[i].color)));
            }

            if (markerLines.getVertexCount() > 0)
                s_window->draw(markerLines, renderStates);
        }

        const auto objectVertices = epochengine::previewgrid::object_marker_vertices_for(ctx.get());
        if (!objectVertices.empty())
        {
            sf::VertexArray objectLines(sf::PrimitiveType::Lines);
            for (std::size_t i = 0; i + 1 < objectVertices.size(); i += 2)
            {
                sf::Vector2f a{};
                sf::Vector2f b{};
                if (!project_preview_vertex(mvp, objectVertices[i].position, viewport, a)
                    || !project_preview_vertex(mvp, objectVertices[i + 1].position, viewport, b))
                {
                    continue;
                }

                a.x -= static_cast<float>(viewport.x);
                a.y -= static_cast<float>(viewport.y);
                b.x -= static_cast<float>(viewport.x);
                b.y -= static_cast<float>(viewport.y);

                objectLines.append(sf::Vertex(a, to_sfml_color(objectVertices[i].color)));
                objectLines.append(sf::Vertex(b, to_sfml_color(objectVertices[i].color)));
            }

            if (objectLines.getVertexCount() > 0)
                s_window->draw(objectLines, renderStates);
        }

        s_window->setView(previousView);
    }

    void apply_view_size() noexcept
    {
        if (!s_window)
            return;

        const sf::Vector2u size(
            static_cast<unsigned>((std::max)(1, s_width)),
            static_cast<unsigned>((std::max)(1, s_height)));
        s_window->setSize(size);
        s_window->setView(sf::View(epochengine::sfml_compat::float_rect(
            0.0f,
            0.0f,
            static_cast<float>(size.x),
            static_cast<float>(size.y))));
    }

    void refresh_dimensions(const std::shared_ptr<epochengine::core::Context>& ctx) noexcept
    {
#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) != FALSE)
        {
            RECT client{};
            if (::GetClientRect(s_childWindow, &client))
            {
                s_width = (std::max)(1, static_cast<int>(client.right - client.left));
                s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));
            }
        }
        else if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE)
        {
            RECT client{};
            if (::GetClientRect(s_hostWindow, &client))
            {
                s_width = (std::max)(1, static_cast<int>(client.right - client.left));
                s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));
            }
        }
#else
        if (s_window)
        {
            const auto size = s_window->getSize();
            s_width = (std::max)(1, static_cast<int>(size.x));
            s_height = (std::max)(1, static_cast<int>(size.y));
        }
#endif

        if (ctx)
        {
            ctx->width = s_width;
            ctx->height = s_height;
            ctx->framebufferWidth = s_width;
            ctx->framebufferHeight = s_height;
            if (ctx->windowData)
                ctx->windowData->set_size(s_width, s_height);
        }

        epochengine::sfmlcontext::state::s_sfmlstate.set_dimensions(s_width, s_height);
    }

    void sync_docked_child_size(const std::shared_ptr<epochengine::core::Context>& ctx) noexcept
    {
#if defined(_WIN32)
        if (!s_childWindow || ::IsWindow(s_childWindow) == FALSE)
        {
            return;
        }

        RECT client{};
        UINT positionFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
        if (ctx && ctx->windowData && ctx->windowData->backend_ready())
            positionFlags |= SWP_SHOWWINDOW;
        else
            positionFlags |= SWP_HIDEWINDOW;
        const HWND childParent = ::GetParent(s_childWindow);
        if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE && childParent == s_hostWindow)
        {
            if (!::GetClientRect(s_hostWindow, &client))
                return;
        }
        else
        {
            if (!::GetClientRect(s_childWindow, &client))
                return;
            positionFlags |= SWP_NOMOVE;
        }

        const int width = (std::max)(1, static_cast<int>(client.right - client.left));
        const int height = (std::max)(1, static_cast<int>(client.bottom - client.top));
        if (width == s_width && height == s_height)
            return;

        s_width = width;
        s_height = height;

        ::SetWindowPos(
            s_childWindow,
            nullptr,
            0,
            0,
            width,
            height,
            positionFlags);

        if (s_window)
            s_window->setSize(sf::Vector2u(static_cast<unsigned>(width), static_cast<unsigned>(height)));

        apply_view_size();
        refresh_dimensions(ctx);
        if (ctx && ctx->onResize)
            ctx->onResize(width, height);
#else
        (void)ctx;
#endif
    }

    void request_host_shutdown(const std::shared_ptr<epochengine::core::Context>& ctx) noexcept
    {
        epochengine::sfmlcontext::state::s_sfmlstate.mark_should_close(true);
        epochengine::sfmlcontext::state::s_sfmlstate.running = false;
        if (ctx && ctx->windowData)
            ctx->windowData->set_should_close(true);
    }

    void sfml_initialize_adapter()
    {
        auto ctx = epochengine::core::get_current_render_context();
        if (!ctx)
            return;

        ctx->init_failed = false;
        s_width = (std::max)(1, ctx->width);
        s_height = (std::max)(1, ctx->height);

#if defined(_WIN32)
        s_hostWindow = ctx->get_hwnd();
#endif

        const std::string title = (ctx->windowData && !ctx->windowData->titleNarrow.empty())
            ? ctx->windowData->titleNarrow
            : (ctx->backendName.empty() ? "SFML" : ctx->backendName);

        s_window = epochengine::sfml_compat::make_render_window(
            epochengine::sfml_compat::video_mode(
                static_cast<unsigned>(s_width),
                static_cast<unsigned>(s_height),
                32u),
            title,
            sf::ContextSettings{});

        if (!s_window || !s_window->isOpen())
        {
            ctx->init_failed = true;
            return;
        }

        s_window->setVerticalSyncEnabled(false);
        s_window->setFramerateLimit(0);
        s_window->setKeyRepeatEnabled(false);
        (void)s_window->setActive(true);
        (void)s_window->setActive(false);

#if defined(_WIN32)
        s_childWindow = static_cast<HWND>(epochengine::sfml_compat::native_handle(*s_window));
        if (!s_childWindow)
        {
            ctx->init_failed = true;
            return;
        }

        if (s_hostWindow && ::IsWindow(s_hostWindow) != FALSE)
        {
            const HWND dockParent = ::GetParent(s_hostWindow);
            const HWND liveDockParent = dockParent ? dockParent : s_hostWindow;
            s_dockParent = liveDockParent;
            ::SetParent(s_childWindow, liveDockParent);

            LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
            style &= ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW);
            style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
            ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);
            epochengine::core::MakeDockable(s_childWindow, liveDockParent);

            RECT client{};
            const HWND sizeSource = s_hostWindow ? s_hostWindow : liveDockParent;
            ::GetClientRect(sizeSource, &client);
            s_width = (std::max)(1, static_cast<int>(client.right - client.left));
            s_height = (std::max)(1, static_cast<int>(client.bottom - client.top));

            ::SetWindowPos(
                s_childWindow,
                nullptr,
                0,
                0,
                s_width,
                s_height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_HIDEWINDOW);

            // Keep SFML's render target size aligned with the dock slot before the
            // first display so startup does not depend on a later resize event.
            s_window->setSize(sf::Vector2u(static_cast<unsigned>(s_width), static_cast<unsigned>(s_height)));
            ::RedrawWindow(
                s_childWindow,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

            if (s_hostWindow != s_childWindow)
                ::ShowWindow(s_hostWindow, SW_HIDE);
        }

        if (!s_window->setActive(true))
        {
            ctx->init_failed = true;
            return;
        }

        s_hdc = s_childWindow ? ::GetDC(s_childWindow) : nullptr;
        s_glContext = ::wglGetCurrentContext();
        (void)s_window->setActive(false);

        const HWND primaryWindow = s_childWindow ? s_childWindow : s_hostWindow;
        ctx->hdc = s_hdc;
        ctx->hglrc = s_glContext;
        ctx->hwnd = primaryWindow;
        ctx->native_window = s_childWindow ? s_childWindow : s_hostWindow;
        ctx->native_drawable = s_hdc;
        ctx->native_gl_context = s_glContext;
#endif

        apply_view_size();
        refresh_dimensions(ctx);
        if (ctx->onResize)
            ctx->onResize(s_width, s_height);

        if (ctx->windowData)
        {
#if defined(_WIN32)
            HWND previousHwnd = ctx->windowData->hwnd;
            HDC previousHdc = ctx->windowData->hdc;
            if (previousHwnd && previousHwnd != s_childWindow)
            {
                ctx->windowData->ownsNativeDc = false;
                if (previousHdc)
                    ::ReleaseDC(previousHwnd, previousHdc);
            }
#endif
            ctx->windowData->sfml_window = s_window.get();
#if defined(_WIN32)
            ctx->windowData->hwnd = s_childWindow ? s_childWindow : s_hostWindow;
            ctx->windowData->host_hwnd = s_hostWindow;
            ctx->windowData->hwndChild = s_childWindow;
            ctx->windowData->hdc = s_hdc;
            ctx->windowData->ownsNativeDc = false;
            ctx->windowData->ownsNativeGlContext = false;
#endif
            ctx->windowData->set_size(s_width, s_height);
        }

        auto& state = epochengine::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = s_window.get();
        state.set_dimensions(s_width, s_height);
        state.mark_should_close(false);
        state.running = true;

#if defined(_WIN32)
        const HWND focusWindow = s_childWindow ? s_childWindow : s_hostWindow;
        if ((!s_dockParent || ::IsWindow(s_dockParent) == FALSE)
            && focusWindow
            && ::IsWindow(focusWindow) != FALSE)
        {
            ::SetFocus(focusWindow);
            s_window->requestFocus();
        }
        if (s_dockParent && ::IsWindow(s_dockParent) != FALSE)
            ::PostMessageW(s_dockParent, WM_SIZE, 0, 0);
#endif

        epochengine::atlasmanager::register_backend_uploader(
            epochengine::core::ContextType::SFML,
            [](const epochengine::TextureAtlas& atlas)
            {
                epochengine::sfmlcontext::ensure_uploaded(atlas);
            });
    }

    void sfml_cleanup_adapter()
    {
        epochengine::atlasmanager::unregister_backend_uploader(epochengine::core::ContextType::SFML);
        release_canvas2d_presenter();
        if (s_window && s_window->isOpen())
            s_arcadePreviewSurface.reset(s_window.get());
        else
            s_arcadePreviewSurface.reset();
        epochengine::sfmlcontext::clear_gpu_atlases();

        auto& state = epochengine::sfmlcontext::state::s_sfmlstate;
        state.window.sfml_window = nullptr;
        state.running = false;
        state.mark_should_close(false);

#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) != FALSE)
        {
            ::ShowWindow(s_childWindow, SW_HIDE);
            if (::GetParent(s_childWindow))
            {
                LONG_PTR style = ::GetWindowLongPtrW(s_childWindow, GWL_STYLE);
                style &= ~static_cast<LONG_PTR>(WS_CHILD);
                style |= WS_POPUP;
                ::SetWindowLongPtrW(s_childWindow, GWL_STYLE, style);
                ::SetParent(s_childWindow, nullptr);
                ::SetWindowPos(
                    s_childWindow,
                    nullptr,
                    0,
                    0,
                    0,
                    0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_HIDEWINDOW);
            }
        }
        if (s_hostWindow && s_hostWindow != s_childWindow && ::IsWindow(s_hostWindow) != FALSE)
            ::ShowWindow(s_hostWindow, SW_HIDE);
#endif

        if (s_window && s_window->isOpen())
            s_window->close();
        s_window.reset();

#if defined(_WIN32)
        if (s_hdc && s_childWindow)
            ::ReleaseDC(s_childWindow, s_hdc);
        s_hostWindow = nullptr;
        s_childWindow = nullptr;
        s_dockParent = nullptr;
        s_hdc = nullptr;
        s_glContext = nullptr;
#endif
        s_width = 0;
        s_height = 0;
        s_logged_activate_failure = false;
    }

    bool sfml_process_adapter(
        std::shared_ptr<epochengine::core::Context> ctx,
        epochengine::core::CommandQueue& queue)
    {
        if (!ctx || !s_window || !s_window->isOpen())
            return false;

        auto& state = epochengine::sfmlcontext::state::s_sfmlstate;
        const bool closeRequested =
            state.shouldClose
            || state.window.get_should_close()
            || (ctx->windowData && ctx->windowData->get_should_close());
        if (closeRequested)
        {
            request_host_shutdown(ctx);
            queue.clear();
            return false;
        }

#if defined(_WIN32)
        if (s_childWindow && ::IsWindow(s_childWindow) == FALSE)
            return false;
#endif

        if (!s_window->setActive(true))
        {
            if (!s_logged_activate_failure)
            {
                epochengine::logger::error(
                    "Context.SFML",
                    std::string("SFML setActive failed. size=")
                        + std::to_string(s_width)
                        + "x"
                        + std::to_string(s_height));
                s_logged_activate_failure = true;
            }
            return false;
        }

#if EPOCH_SFML_HAS_V3_API
        while (const auto event = s_window->pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                request_host_shutdown(ctx);
                (void)s_window->setActive(false);
                return false;
            }

            if (event->is<sf::Event::Resized>())
            {
                refresh_dimensions(ctx);
                apply_view_size();
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }
#else
        sf::Event event{};
        while (s_window->pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                request_host_shutdown(ctx);
                (void)s_window->setActive(false);
                return false;
            }

            if (event.type == sf::Event::Resized)
            {
                refresh_dimensions(ctx);
                apply_view_size();
                if (ctx->onResize)
                    ctx->onResize(ctx->framebufferWidth, ctx->framebufferHeight);
            }
        }
#endif

        sync_docked_child_size(ctx);
        refresh_dimensions(ctx);

        epochengine::atlasmanager::process_pending_uploads(epochengine::core::ContextType::SFML);
        s_window->resetGLStates();

        const auto clearColor = epochengine::core::clear_color_for_context(epochengine::core::ContextType::SFML);
        s_window->clear(sf::Color(
            static_cast<std::uint8_t>(clearColor[0] * 255.0f),
            static_cast<std::uint8_t>(clearColor[1] * 255.0f),
            static_cast<std::uint8_t>(clearColor[2] * 255.0f),
            static_cast<std::uint8_t>(clearColor[3] * 255.0f)));

        s_window->resetGLStates();
        const bool overlayPriority = ctx->gui_overlay_priority();
        (void)queue.drain();
        if (!overlayPriority)
        {
            s_window->resetGLStates();
            (void)epochengine::gui::render_deferred_batch(ctx.get());
        }
        s_window->resetGLStates();
        render_scene_preview(ctx);
        s_window->resetGLStates();
        (void)queue.drain();
        s_window->resetGLStates();
        (void)epochengine::gui::render_deferred_batch(ctx.get());
        s_window->resetGLStates();
        (void)epochengine::gui::render_top_layer_batch(ctx.get());
        s_window->display();
        (void)s_window->setActive(false);
        return s_window->isOpen();
    }
}

namespace epochengine::core::detail
{
    void register_sfml_backend()
    {
        auto ctx = std::make_shared<Context>();
        ctx->type = ContextType::SFML;
        ctx->backendName = "SFML";
        ctx->initialize = sfml_initialize_adapter;
        ctx->cleanup = sfml_cleanup_adapter;
        ctx->process = sfml_process_adapter;
        ctx->clear = nullptr;
        ctx->present = nullptr;
        ctx->get_width = []() { return s_width; };
        ctx->get_height = []() { return s_height; };
        ctx->draw_sprite = epochengine::sfmlcontext::draw_sprite;
        ctx->add_texture = &default_add_texture;
        ctx->add_atlas = +[](const epochengine::TextureAtlas& atlas)
        {
            return default_add_atlas(atlas);
        };
        bind_default_input(ctx);
        AddContextForBackend(ContextType::SFML, std::move(ctx));
    }
}

#endif
