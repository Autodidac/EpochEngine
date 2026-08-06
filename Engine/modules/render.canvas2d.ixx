// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

export module render.canvas2d;

import render.device;

export namespace epochengine::canvas2d
{
    inline constexpr std::uint32_t invalid_index =
        (std::numeric_limits<std::uint32_t>::max)();
    inline constexpr std::uint32_t empty_tile =
        (std::numeric_limits<std::uint32_t>::max)();

    enum class ResultCode : std::uint8_t
    {
        success,
        partial,
        invalid_limits,
        invalid_canvas_extent,
        invalid_surface_extent,
        invalid_viewport_policy,
        invalid_camera,
        invalid_handle,
        stale_handle,
        invalid_material,
        material_key_collision,
        invalid_sprite,
        duplicate_sort_key,
        invalid_tile_set,
        invalid_tile_layer,
        invalid_tile_chunk,
        capacity_exceeded,
        arithmetic_overflow,
        allocation_failure
    };

    [[nodiscard]] constexpr bool succeeded(ResultCode code) noexcept
    {
        return code == ResultCode::success || code == ResultCode::partial;
    }

    struct UInt2 final
    {
        std::uint32_t x{};
        std::uint32_t y{};

        friend constexpr bool operator==(UInt2, UInt2) noexcept = default;
    };

    struct Int2 final
    {
        std::int32_t x{};
        std::int32_t y{};

        friend constexpr bool operator==(Int2, Int2) noexcept = default;
    };

    struct Float2 final
    {
        float x{};
        float y{};

        friend constexpr bool operator==(Float2, Float2) noexcept = default;
    };

    struct RectI final
    {
        std::int32_t x{};
        std::int32_t y{};
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return width == 0 || height == 0;
        }

        friend constexpr bool operator==(RectI, RectI) noexcept = default;
    };

    struct RectF final
    {
        float x{};
        float y{};
        float width{};
        float height{};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return width <= 0.0f || height <= 0.0f;
        }

        friend constexpr bool operator==(RectF, RectF) noexcept = default;
    };

    struct LinearColor final
    {
        float r{1.0f};
        float g{1.0f};
        float b{1.0f};
        float a{1.0f};

        friend constexpr bool operator==(LinearColor, LinearColor) noexcept = default;
    };

    [[nodiscard]] inline bool finite(Float2 value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y);
    }

    [[nodiscard]] inline bool finite(RectF value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y)
            && std::isfinite(value.width) && std::isfinite(value.height);
    }

    [[nodiscard]] inline bool finite(LinearColor value) noexcept
    {
        return std::isfinite(value.r) && std::isfinite(value.g)
            && std::isfinite(value.b) && std::isfinite(value.a);
    }

    [[nodiscard]] constexpr bool checked_multiply(
        std::uint64_t lhs,
        std::uint64_t rhs,
        std::uint64_t& result) noexcept
    {
        if (lhs != 0 && rhs > (std::numeric_limits<std::uint64_t>::max)() / lhs)
            return false;
        result = lhs * rhs;
        return true;
    }

    [[nodiscard]] constexpr bool checked_add(
        std::uint64_t lhs,
        std::uint64_t rhs,
        std::uint64_t& result) noexcept
    {
        if (rhs > (std::numeric_limits<std::uint64_t>::max)() - lhs)
            return false;
        result = lhs + rhs;
        return true;
    }

    struct CanvasExtent final
    {
        std::uint32_t width{320};
        std::uint32_t height{180};

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return width == 0 || height == 0;
        }

        friend constexpr bool operator==(CanvasExtent, CanvasExtent) noexcept = default;
    };

    struct CanvasLimits final
    {
        std::uint32_t maximum_canvas_dimension{65'536};
        std::uint32_t maximum_surface_dimension{131'072};
        std::uint64_t maximum_canvas_pixels{268'435'456};
        std::uint64_t maximum_surface_pixels{536'870'912};
        float maximum_camera_zoom{65'536.0f};
        float maximum_pixels_per_world_unit{65'536.0f};

        friend constexpr bool operator==(const CanvasLimits&, const CanvasLimits&) noexcept = default;
    };

    struct ExtentValidation final
    {
        ResultCode code{ResultCode::invalid_canvas_extent};
        std::uint64_t pixel_count{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    [[nodiscard]] constexpr bool valid(const CanvasLimits& limits) noexcept
    {
        return limits.maximum_canvas_dimension != 0
            && limits.maximum_surface_dimension != 0
            && limits.maximum_canvas_pixels != 0
            && limits.maximum_surface_pixels != 0
            && limits.maximum_camera_zoom > 0.0f
            && limits.maximum_pixels_per_world_unit > 0.0f;
    }

    [[nodiscard]] constexpr ExtentValidation validate_canvas_extent(
        CanvasExtent extent,
        const CanvasLimits& limits = {}) noexcept
    {
        if (!valid(limits) || extent.empty()
            || extent.width > limits.maximum_canvas_dimension
            || extent.height > limits.maximum_canvas_dimension)
        {
            return {ResultCode::invalid_canvas_extent, 0};
        }

        std::uint64_t pixels{};
        if (!checked_multiply(extent.width, extent.height, pixels))
            return {ResultCode::arithmetic_overflow, 0};
        if (pixels > limits.maximum_canvas_pixels)
            return {ResultCode::capacity_exceeded, pixels};
        return {ResultCode::success, pixels};
    }

    [[nodiscard]] constexpr ExtentValidation validate_surface_extent(
        CanvasExtent extent,
        const CanvasLimits& limits = {}) noexcept
    {
        if (!valid(limits) || extent.empty()
            || extent.width > limits.maximum_surface_dimension
            || extent.height > limits.maximum_surface_dimension)
        {
            return {ResultCode::invalid_surface_extent, 0};
        }

        std::uint64_t pixels{};
        if (!checked_multiply(extent.width, extent.height, pixels))
            return {ResultCode::arithmetic_overflow, 0};
        if (pixels > limits.maximum_surface_pixels)
            return {ResultCode::capacity_exceeded, pixels};
        return {ResultCode::success, pixels};
    }

    enum class ViewportPolicy : std::uint8_t
    {
        resize_canvas,
        letterbox,
        integer_scale,
        fractional_scale,
        fill_crop,
        stretch
    };

    struct ViewportRequest final
    {
        CanvasExtent logical_canvas{};
        CanvasExtent output_surface{1280, 720};
        ViewportPolicy policy{ViewportPolicy::integer_scale};
        bool center{true};
    };

    struct ViewportPlan final
    {
        ResultCode code{ResultCode::invalid_canvas_extent};
        CanvasExtent logical_canvas{};
        CanvasExtent render_extent{};
        CanvasExtent output_surface{};
        RectI destination{};
        RectI clipped_destination{};
        RectF visible_canvas{};
        std::array<RectI, 4> letterbox_bars{};
        std::uint8_t letterbox_bar_count{};
        float scale_x{};
        float scale_y{};
        bool preserves_aspect{};
        bool exact_integer_scale{};
        bool canvas_resized{};
        bool content_clipped{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    [[nodiscard]] constexpr RectI intersect_surface(
        RectI rectangle,
        CanvasExtent surface) noexcept
    {
        const std::int64_t left = (std::max)(std::int64_t{0}, std::int64_t{rectangle.x});
        const std::int64_t top = (std::max)(std::int64_t{0}, std::int64_t{rectangle.y});
        const std::int64_t right = (std::min)(
            static_cast<std::int64_t>(surface.width),
            static_cast<std::int64_t>(rectangle.x) + rectangle.width);
        const std::int64_t bottom = (std::min)(
            static_cast<std::int64_t>(surface.height),
            static_cast<std::int64_t>(rectangle.y) + rectangle.height);
        if (right <= left || bottom <= top)
            return {};
        return {
            static_cast<std::int32_t>(left),
            static_cast<std::int32_t>(top),
            static_cast<std::uint32_t>(right - left),
            static_cast<std::uint32_t>(bottom - top)
        };
    }

    inline void append_letterbox_bar(
        ViewportPlan& plan,
        RectI bar) noexcept
    {
        if (!bar.empty() && plan.letterbox_bar_count < plan.letterbox_bars.size())
            plan.letterbox_bars[plan.letterbox_bar_count++] = bar;
    }

    [[nodiscard]] inline ViewportPlan plan_viewport(
        const ViewportRequest& request,
        const CanvasLimits& limits = {}) noexcept
    {
        ViewportPlan plan{};
        plan.logical_canvas = request.logical_canvas;
        plan.render_extent = request.logical_canvas;
        plan.output_surface = request.output_surface;

        const auto canvas_validation = validate_canvas_extent(request.logical_canvas, limits);
        if (!canvas_validation)
        {
            plan.code = canvas_validation.code;
            return plan;
        }
        const auto surface_validation = validate_surface_extent(request.output_surface, limits);
        if (!surface_validation)
        {
            plan.code = surface_validation.code;
            return plan;
        }

        const float surface_width = static_cast<float>(request.output_surface.width);
        const float surface_height = static_cast<float>(request.output_surface.height);
        const float canvas_width = static_cast<float>(request.logical_canvas.width);
        const float canvas_height = static_cast<float>(request.logical_canvas.height);

        if (request.policy == ViewportPolicy::resize_canvas)
        {
            const auto resized_validation = validate_canvas_extent(
                request.output_surface,
                limits);
            if (!resized_validation)
            {
                plan.code = resized_validation.code;
                return plan;
            }
            plan.render_extent = request.output_surface;
            plan.destination = {0, 0, request.output_surface.width, request.output_surface.height};
            plan.clipped_destination = plan.destination;
            plan.visible_canvas = {0.0f, 0.0f, surface_width, surface_height};
            plan.scale_x = 1.0f;
            plan.scale_y = 1.0f;
            plan.preserves_aspect = true;
            plan.exact_integer_scale = true;
            plan.canvas_resized = request.logical_canvas != request.output_surface;
            plan.code = ResultCode::success;
            return plan;
        }

        if (request.policy == ViewportPolicy::stretch)
        {
            plan.destination = {0, 0, request.output_surface.width, request.output_surface.height};
            plan.clipped_destination = plan.destination;
            plan.visible_canvas = {0.0f, 0.0f, canvas_width, canvas_height};
            plan.scale_x = surface_width / canvas_width;
            plan.scale_y = surface_height / canvas_height;
            plan.preserves_aspect = std::abs(plan.scale_x - plan.scale_y) <= 1.0e-6f;
            plan.exact_integer_scale =
                std::floor(plan.scale_x) == plan.scale_x
                && std::floor(plan.scale_y) == plan.scale_y;
            plan.code = ResultCode::success;
            return plan;
        }

        if (request.policy != ViewportPolicy::letterbox
            && request.policy != ViewportPolicy::integer_scale
            && request.policy != ViewportPolicy::fractional_scale
            && request.policy != ViewportPolicy::fill_crop)
        {
            plan.code = ResultCode::invalid_viewport_policy;
            return plan;
        }

        const bool fill_crop = request.policy == ViewportPolicy::fill_crop;
        const float fit_scale = fill_crop
            ? (std::max)(surface_width / canvas_width, surface_height / canvas_height)
            : (std::min)(surface_width / canvas_width, surface_height / canvas_height);
        float uniform_scale = fit_scale;
        if (request.policy == ViewportPolicy::integer_scale
            || (request.policy == ViewportPolicy::letterbox && fit_scale >= 1.0f))
        {
            uniform_scale = (std::max)(1.0f, std::floor(fit_scale));
        }

        if (!std::isfinite(uniform_scale) || uniform_scale <= 0.0f)
        {
            plan.code = ResultCode::invalid_surface_extent;
            return plan;
        }

        const auto destination_width = static_cast<std::uint32_t>(
            (std::max)(1.0f, std::floor(canvas_width * uniform_scale + 1.0e-5f)));
        const auto destination_height = static_cast<std::uint32_t>(
            (std::max)(1.0f, std::floor(canvas_height * uniform_scale + 1.0e-5f)));
        const std::int32_t destination_x = request.center
            ? static_cast<std::int32_t>(
                (static_cast<std::int64_t>(request.output_surface.width)
                    - destination_width) / 2)
            : 0;
        const std::int32_t destination_y = request.center
            ? static_cast<std::int32_t>(
                (static_cast<std::int64_t>(request.output_surface.height)
                    - destination_height) / 2)
            : 0;

        plan.destination = {
            destination_x,
            destination_y,
            destination_width,
            destination_height
        };
        plan.clipped_destination = intersect_surface(plan.destination, request.output_surface);
        plan.scale_x = static_cast<float>(destination_width) / canvas_width;
        plan.scale_y = static_cast<float>(destination_height) / canvas_height;
        plan.preserves_aspect = true;
        plan.exact_integer_scale =
            std::floor(plan.scale_x) == plan.scale_x
            && plan.scale_x == plan.scale_y;
        plan.content_clipped = plan.clipped_destination != plan.destination;

        if (plan.clipped_destination.empty())
        {
            plan.code = ResultCode::invalid_surface_extent;
            return plan;
        }

        const float clipped_left = static_cast<float>(
            plan.clipped_destination.x - plan.destination.x);
        const float clipped_top = static_cast<float>(
            plan.clipped_destination.y - plan.destination.y);
        plan.visible_canvas = {
            clipped_left / plan.scale_x,
            clipped_top / plan.scale_y,
            static_cast<float>(plan.clipped_destination.width) / plan.scale_x,
            static_cast<float>(plan.clipped_destination.height) / plan.scale_y
        };

        const std::int32_t right = plan.clipped_destination.x
            + static_cast<std::int32_t>(plan.clipped_destination.width);
        const std::int32_t bottom = plan.clipped_destination.y
            + static_cast<std::int32_t>(plan.clipped_destination.height);
        append_letterbox_bar(plan, {
            0,
            0,
            request.output_surface.width,
            static_cast<std::uint32_t>((std::max)(0, plan.clipped_destination.y))
        });
        append_letterbox_bar(plan, {
            0,
            bottom,
            request.output_surface.width,
            static_cast<std::uint32_t>((std::max)(
                0,
                static_cast<std::int32_t>(request.output_surface.height) - bottom))
        });
        append_letterbox_bar(plan, {
            0,
            plan.clipped_destination.y,
            static_cast<std::uint32_t>((std::max)(0, plan.clipped_destination.x)),
            plan.clipped_destination.height
        });
        append_letterbox_bar(plan, {
            right,
            plan.clipped_destination.y,
            static_cast<std::uint32_t>((std::max)(
                0,
                static_cast<std::int32_t>(request.output_surface.width) - right)),
            plan.clipped_destination.height
        });
        plan.code = ResultCode::success;
        return plan;
    }

    enum class PixelSnapMode : std::uint8_t
    {
        none,
        camera,
        sprites,
        camera_and_sprites
    };

    enum class CanvasYAxis : std::uint8_t
    {
        down,
        up
    };

    struct CameraState final
    {
        Float2 center{};
        float rotation_radians{};
        float zoom{1.0f};
        float pixels_per_world_unit{1.0f};
        PixelSnapMode pixel_snap{PixelSnapMode::camera_and_sprites};
        CanvasYAxis y_axis{CanvasYAxis::down};

        friend constexpr bool operator==(const CameraState&, const CameraState&) noexcept = default;
    };

    struct CameraPlan final
    {
        ResultCode code{ResultCode::invalid_camera};
        CameraState requested{};
        Float2 snapped_center{};
        Float2 output_pixels_per_world_unit{};
        float canvas_pixels_per_world_unit{};
        float world_units_per_canvas_pixel{};
        CanvasExtent render_extent{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    [[nodiscard]] inline bool valid(
        const CameraState& camera,
        const CanvasLimits& limits = {}) noexcept
    {
        return valid(limits)
            && finite(camera.center)
            && std::isfinite(camera.rotation_radians)
            && std::isfinite(camera.zoom)
            && std::isfinite(camera.pixels_per_world_unit)
            && camera.zoom > 0.0f
            && camera.zoom <= limits.maximum_camera_zoom
            && camera.pixels_per_world_unit > 0.0f
            && camera.pixels_per_world_unit <= limits.maximum_pixels_per_world_unit;
    }

    [[nodiscard]] constexpr bool snaps_camera(PixelSnapMode mode) noexcept
    {
        return mode == PixelSnapMode::camera || mode == PixelSnapMode::camera_and_sprites;
    }

    [[nodiscard]] constexpr bool snaps_sprites(PixelSnapMode mode) noexcept
    {
        return mode == PixelSnapMode::sprites || mode == PixelSnapMode::camera_and_sprites;
    }

    [[nodiscard]] inline CameraPlan plan_camera(
        const CameraState& camera,
        const ViewportPlan& viewport,
        const CanvasLimits& limits = {}) noexcept
    {
        CameraPlan plan{};
        plan.requested = camera;
        plan.render_extent = viewport.render_extent;
        if (!viewport || !valid(camera, limits))
            return plan;

        plan.canvas_pixels_per_world_unit = camera.pixels_per_world_unit * camera.zoom;
        if (!std::isfinite(plan.canvas_pixels_per_world_unit)
            || plan.canvas_pixels_per_world_unit <= 0.0f)
        {
            return plan;
        }
        plan.world_units_per_canvas_pixel = 1.0f / plan.canvas_pixels_per_world_unit;
        plan.snapped_center = camera.center;
        if (snaps_camera(camera.pixel_snap))
        {
            const float quantum = plan.world_units_per_canvas_pixel;
            plan.snapped_center.x = std::round(camera.center.x / quantum) * quantum;
            plan.snapped_center.y = std::round(camera.center.y / quantum) * quantum;
        }
        plan.output_pixels_per_world_unit = {
            plan.canvas_pixels_per_world_unit * viewport.scale_x,
            plan.canvas_pixels_per_world_unit * viewport.scale_y
        };
        plan.code = ResultCode::success;
        return plan;
    }

    [[nodiscard]] inline Float2 world_to_canvas(
        Float2 world,
        const CameraPlan& camera) noexcept
    {
        if (!camera || !finite(world))
            return {std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::quiet_NaN()};
        const float dx = world.x - camera.snapped_center.x;
        const float dy = world.y - camera.snapped_center.y;
        const float cosine = std::cos(camera.requested.rotation_radians);
        const float sine = std::sin(camera.requested.rotation_radians);
        const float local_x = cosine * dx + sine * dy;
        float local_y = -sine * dx + cosine * dy;
        if (camera.requested.y_axis == CanvasYAxis::up)
            local_y = -local_y;
        return {
            static_cast<float>(camera.render_extent.width) * 0.5f
                + local_x * camera.canvas_pixels_per_world_unit,
            static_cast<float>(camera.render_extent.height) * 0.5f
                + local_y * camera.canvas_pixels_per_world_unit
        };
    }

    [[nodiscard]] inline Float2 canvas_to_world(
        Float2 canvas,
        const CameraPlan& camera) noexcept
    {
        if (!camera || !finite(canvas))
            return {std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::quiet_NaN()};
        float local_x = (canvas.x - static_cast<float>(camera.render_extent.width) * 0.5f)
            * camera.world_units_per_canvas_pixel;
        float local_y = (canvas.y - static_cast<float>(camera.render_extent.height) * 0.5f)
            * camera.world_units_per_canvas_pixel;
        if (camera.requested.y_axis == CanvasYAxis::up)
            local_y = -local_y;
        const float cosine = std::cos(camera.requested.rotation_radians);
        const float sine = std::sin(camera.requested.rotation_radians);
        return {
            camera.snapped_center.x + cosine * local_x - sine * local_y,
            camera.snapped_center.y + sine * local_x + cosine * local_y
        };
    }

    struct SpriteHandle final
    {
        std::uint32_t index{invalid_index};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        friend constexpr bool operator==(SpriteHandle, SpriteHandle) noexcept = default;
        friend constexpr auto operator<=>(SpriteHandle, SpriteHandle) noexcept = default;
    };

    struct SpriteIdentityMetrics final
    {
        std::uint32_t capacity{};
        std::uint32_t slots{};
        std::uint32_t live{};
        std::uint64_t allocations{};
        std::uint64_t retirements{};
        std::uint64_t rejected_allocations{};
        std::uint64_t stale_rejections{};
    };

    class SpriteIdentityRegistry final
    {
    public:
        explicit SpriteIdentityRegistry(std::uint32_t capacity = 65'536) noexcept
            : capacity_(capacity)
        {
        }

        [[nodiscard]] std::optional<SpriteHandle> create()
        {
            if (capacity_ == 0)
            {
                ++metrics_.rejected_allocations;
                return std::nullopt;
            }

            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                Slot& slot = slots_[index];
                if (!slot.live)
                {
                    slot.live = true;
                    ++metrics_.live;
                    ++metrics_.allocations;
                    return SpriteHandle{index, slot.generation};
                }
            }

            if (slots_.size() >= capacity_)
            {
                ++metrics_.rejected_allocations;
                return std::nullopt;
            }

            try
            {
                slots_.push_back(Slot{});
            }
            catch (const std::bad_alloc&)
            {
                ++metrics_.rejected_allocations;
                return std::nullopt;
            }
            catch (const std::length_error&)
            {
                ++metrics_.rejected_allocations;
                return std::nullopt;
            }
            ++metrics_.live;
            ++metrics_.allocations;
            return SpriteHandle{
                static_cast<std::uint32_t>(slots_.size() - 1),
                slots_.back().generation
            };
        }

        [[nodiscard]] ResultCode retire(SpriteHandle handle) noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
            {
                ++metrics_.stale_rejections;
                return ResultCode::invalid_handle;
            }
            Slot& slot = slots_[handle.index];
            if (!slot.live || slot.generation != handle.generation)
            {
                ++metrics_.stale_rejections;
                return ResultCode::stale_handle;
            }
            slot.live = false;
            slot.generation = next_generation(slot.generation);
            --metrics_.live;
            ++metrics_.retirements;
            return ResultCode::success;
        }

        [[nodiscard]] bool alive(SpriteHandle handle) const noexcept
        {
            return handle.valid()
                && handle.index < slots_.size()
                && slots_[handle.index].live
                && slots_[handle.index].generation == handle.generation;
        }

        [[nodiscard]] SpriteIdentityMetrics metrics() const noexcept
        {
            SpriteIdentityMetrics result = metrics_;
            result.capacity = capacity_;
            result.slots = static_cast<std::uint32_t>(slots_.size());
            return result;
        }

    private:
        struct Slot final
        {
            std::uint32_t generation{1};
            bool live{true};
        };

        [[nodiscard]] static constexpr std::uint32_t next_generation(
            std::uint32_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }

        std::uint32_t capacity_{};
        std::vector<Slot> slots_{};
        SpriteIdentityMetrics metrics_{};
    };

    enum class SpriteAlphaMode : std::uint8_t
    {
        opaque,
        mask,
        straight,
        premultiplied,
        additive
    };

    enum class SpriteColorSpace : std::uint8_t
    {
        linear,
        srgb
    };

    enum class SpriteSourceKind : std::uint8_t
    {
        solid_color,
        texture,
        render_surface
    };
    struct LogicalTextureReference final
    {
        std::uint64_t asset_key{};
        std::uint64_t artifact_revision{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return asset_key != 0 && artifact_revision != 0;
        }

        friend constexpr bool operator==(
            const LogicalTextureReference&,
            const LogicalTextureReference&) noexcept = default;
    };


    struct SpriteSamplerDeclaration final
    {
        FilterMode min_filter{FilterMode::nearest};
        FilterMode mag_filter{FilterMode::nearest};
        AddressMode address_u{AddressMode::clamp_to_edge};
        AddressMode address_v{AddressMode::clamp_to_edge};
        float maximum_anisotropy{1.0f};
        bool mipmapped{};

        friend constexpr bool operator==(
            const SpriteSamplerDeclaration&,
            const SpriteSamplerDeclaration&) noexcept = default;
    };

    struct SpriteMaterialDeclaration final
    {
        std::uint32_t stable_key{1};
        SpriteSourceKind source{SpriteSourceKind::texture};
        TextureHandle texture{};
        SamplerHandle resolved_sampler{};
        MaterialHandle resolved_material{};
        LogicalTextureReference logical_texture{};
        SpriteSamplerDeclaration sampler{};
        SpriteAlphaMode alpha{SpriteAlphaMode::premultiplied};
        SpriteColorSpace color_space{SpriteColorSpace::srgb};
        float alpha_cutoff{0.5f};
        bool writes_color{true};

        friend constexpr bool operator==(
            const SpriteMaterialDeclaration&,
            const SpriteMaterialDeclaration&) noexcept = default;
    };

    [[nodiscard]] inline bool valid(
        const SpriteSamplerDeclaration& sampler) noexcept
    {
        const bool valid_filter = [](FilterMode value) noexcept
        {
            return value == FilterMode::nearest || value == FilterMode::linear;
        }(sampler.min_filter) && (sampler.mag_filter == FilterMode::nearest
            || sampler.mag_filter == FilterMode::linear);
        const bool valid_address = [](AddressMode value) noexcept
        {
            return value == AddressMode::clamp_to_edge
                || value == AddressMode::repeat
                || value == AddressMode::mirrored_repeat;
        }(sampler.address_u) && (sampler.address_v == AddressMode::clamp_to_edge
            || sampler.address_v == AddressMode::repeat
            || sampler.address_v == AddressMode::mirrored_repeat);
        return valid_filter && valid_address
            && std::isfinite(sampler.maximum_anisotropy)
            && sampler.maximum_anisotropy >= 1.0f
            && sampler.maximum_anisotropy <= 16.0f;
    }

    [[nodiscard]] inline bool valid(
        const SpriteMaterialDeclaration& material) noexcept
    {
        const bool valid_source = material.source == SpriteSourceKind::solid_color
            || material.source == SpriteSourceKind::texture
            || material.source == SpriteSourceKind::render_surface;
        const bool valid_alpha = material.alpha == SpriteAlphaMode::opaque
            || material.alpha == SpriteAlphaMode::mask
            || material.alpha == SpriteAlphaMode::straight
            || material.alpha == SpriteAlphaMode::premultiplied
            || material.alpha == SpriteAlphaMode::additive;
        const bool valid_color_space = material.color_space == SpriteColorSpace::linear
            || material.color_space == SpriteColorSpace::srgb;
        if (!valid_source || !valid_alpha || !valid_color_space
            || material.stable_key == 0 || !valid(material.sampler)
            || !std::isfinite(material.alpha_cutoff)
            || material.alpha_cutoff < 0.0f || material.alpha_cutoff > 1.0f)
        {
            return false;
        }
        if (material.source == SpriteSourceKind::texture
            && !static_cast<bool>(material.logical_texture))
        {
            return false;
        }
        if (material.source == SpriteSourceKind::render_surface && !material.texture)
            return false;
        return material.alpha != SpriteAlphaMode::mask
            || (material.alpha_cutoff > 0.0f && material.alpha_cutoff < 1.0f);
    }

    [[nodiscard]] constexpr bool same_material_intent(
        const SpriteMaterialDeclaration& lhs,
        const SpriteMaterialDeclaration& rhs) noexcept
    {
        if (lhs.stable_key != rhs.stable_key
            || lhs.source != rhs.source
            || lhs.logical_texture != rhs.logical_texture
            || lhs.sampler != rhs.sampler
            || lhs.alpha != rhs.alpha
            || lhs.color_space != rhs.color_space
            || lhs.alpha_cutoff != rhs.alpha_cutoff
            || lhs.writes_color != rhs.writes_color)
        {
            return false;
        }
        if (lhs.source == SpriteSourceKind::render_surface)
            return lhs.texture == rhs.texture;
        return true;
    }

    enum class SpritePhase : std::uint8_t
    {
        background,
        world,
        foreground,
        overlay
    };

    struct SpriteSortKey final
    {
        SpritePhase phase{SpritePhase::world};
        std::int32_t layer{};
        std::int32_t order{};
        std::int32_t depth{};
        std::uint64_t stable_sequence{};
        SpriteHandle sprite{};
        std::int32_t sort_group{};

        friend constexpr bool operator==(const SpriteSortKey&, const SpriteSortKey&) noexcept = default;
    };

    [[nodiscard]] constexpr bool less(SpriteSortKey lhs, SpriteSortKey rhs) noexcept
    {
        return std::tuple{
            lhs.phase,
            lhs.layer,
            lhs.sort_group,
            lhs.order,
            lhs.depth,
            lhs.stable_sequence,
            lhs.sprite.index,
            lhs.sprite.generation
        } < std::tuple{
            rhs.phase,
            rhs.layer,
            rhs.sort_group,
            rhs.order,
            rhs.depth,
            rhs.stable_sequence,
            rhs.sprite.index,
            rhs.sprite.generation
        };
    }

    struct SpriteTransform final
    {
        Float2 position{};
        Float2 size{1.0f, 1.0f};
        Float2 pivot{0.5f, 0.5f};
        float rotation_radians{};

        friend constexpr bool operator==(const SpriteTransform&, const SpriteTransform&) noexcept = default;
    };

    struct SpriteSubmission final
    {
        SpriteHandle sprite{};
        SpriteMaterialDeclaration material{};
        SpriteTransform transform{};
        RectF source_uv{0.0f, 0.0f, 1.0f, 1.0f};
        LinearColor tint{};
        SpritePhase phase{SpritePhase::world};
        std::int32_t layer{};
        std::int32_t order{};
        std::int32_t depth{};
        std::uint64_t stable_sequence{1};
        std::uint32_t clip_key{};
        bool flip_x{};
        bool flip_y{};
        bool hidden{};
        std::int32_t sort_group{};
    };

    [[nodiscard]] constexpr SpriteSortKey sort_key(
        const SpriteSubmission& submission) noexcept
    {
        return {
            submission.phase,
            submission.layer,
            submission.order,
            submission.depth,
            submission.stable_sequence,
            submission.sprite,
            submission.sort_group
        };
    }

    [[nodiscard]] inline ResultCode validate(
        const SpriteSubmission& submission) noexcept
    {
        if (!submission.sprite.valid())
            return ResultCode::invalid_handle;
        if (!valid(submission.material))
            return ResultCode::invalid_material;
        if (!finite(submission.transform.position)
            || !finite(submission.transform.size)
            || !finite(submission.transform.pivot)
            || !std::isfinite(submission.transform.rotation_radians)
            || submission.transform.size.x <= 0.0f
            || submission.transform.size.y <= 0.0f
            || !finite(submission.source_uv)
            || submission.source_uv.empty()
            || submission.source_uv.x < 0.0f
            || submission.source_uv.y < 0.0f
            || submission.source_uv.x + submission.source_uv.width > 1.0f
            || submission.source_uv.y + submission.source_uv.height > 1.0f
            || !finite(submission.tint)
            || submission.stable_sequence == 0)
        {
            return ResultCode::invalid_sprite;
        }
        return ResultCode::success;
    }

    struct SpriteVertex final
    {
        Float2 position{};
        Float2 uv{};
        LinearColor color{};

        friend constexpr bool operator==(const SpriteVertex&, const SpriteVertex&) noexcept = default;
    };

    struct SpriteBatchKey final
    {
        SpriteMaterialDeclaration material{};
        std::uint32_t clip_key{};

        friend constexpr bool operator==(
            const SpriteBatchKey& lhs,
            const SpriteBatchKey& rhs) noexcept
        {
            return lhs.clip_key == rhs.clip_key
                && same_material_intent(lhs.material, rhs.material);
        }
    };

    struct CompiledSpriteBatch final
    {
        SpriteBatchKey key{};
        std::uint32_t first_vertex{};
        std::uint32_t vertex_count{};
        std::uint32_t first_index{};
        std::uint32_t index_count{};
        std::uint32_t sprite_count{};
        SpriteSortKey first_sort_key{};
        SpriteSortKey last_sort_key{};
    };

    struct SpriteBatchLimits final
    {
        std::uint32_t maximum_submissions{131'072};
        std::uint32_t maximum_materials{4'096};
        std::uint32_t maximum_batches{8'192};
        std::uint32_t maximum_sprites_per_batch{16'384};
        std::uint32_t maximum_vertices{524'288};
        std::uint32_t maximum_indices{786'432};

        friend constexpr bool operator==(const SpriteBatchLimits&, const SpriteBatchLimits&) noexcept = default;
    };

    [[nodiscard]] constexpr bool valid(const SpriteBatchLimits& limits) noexcept
    {
        return limits.maximum_submissions != 0
            && limits.maximum_materials != 0
            && limits.maximum_batches != 0
            && limits.maximum_sprites_per_batch != 0
            && limits.maximum_vertices >= 4
            && limits.maximum_indices >= 6;
    }

    struct ProjectSettings final
    {
        CanvasExtent logical_canvas{1280, 720};
        float pixels_per_world_unit{100.0f};
        ViewportPolicy viewport_policy{ViewportPolicy::integer_scale};
        FilterMode presentation_filter{FilterMode::nearest};
        PixelSnapMode pixel_snap{PixelSnapMode::camera_and_sprites};
        std::uint32_t maximum_sprites_per_batch{2048};
        std::uint32_t maximum_batches{256};
        std::uint32_t tile_chunk_extent{32};

        friend constexpr bool operator==(
            const ProjectSettings&,
            const ProjectSettings&) noexcept = default;
    };

    [[nodiscard]] inline ResultCode validate(
        const ProjectSettings& settings,
        const CanvasLimits& canvas_limits = {}) noexcept
    {
        if (!validate_canvas_extent(settings.logical_canvas, canvas_limits))
            return ResultCode::invalid_canvas_extent;
        if (!std::isfinite(settings.pixels_per_world_unit)
            || settings.pixels_per_world_unit <= 0.0f
            || settings.pixels_per_world_unit > canvas_limits.maximum_pixels_per_world_unit)
        {
            return ResultCode::invalid_camera;
        }
        if (settings.viewport_policy != ViewportPolicy::resize_canvas
            && settings.viewport_policy != ViewportPolicy::letterbox
            && settings.viewport_policy != ViewportPolicy::integer_scale
            && settings.viewport_policy != ViewportPolicy::fractional_scale
            && settings.viewport_policy != ViewportPolicy::fill_crop
            && settings.viewport_policy != ViewportPolicy::stretch)
        {
            return ResultCode::invalid_viewport_policy;
        }
        if (settings.presentation_filter != FilterMode::nearest
            && settings.presentation_filter != FilterMode::linear)
        {
            return ResultCode::invalid_material;
        }
        if (settings.maximum_sprites_per_batch == 0
            || settings.maximum_sprites_per_batch > 65'536u
            || settings.maximum_batches == 0
            || settings.maximum_batches > 4'096u)
        {
            return ResultCode::invalid_limits;
        }
        switch (settings.tile_chunk_extent)
        {
        case 8u: case 16u: case 32u: case 64u: case 128u: case 256u:
            return ResultCode::success;
        default:
            return ResultCode::invalid_tile_layer;
        }
    }

    [[nodiscard]] constexpr ViewportRequest make_viewport_request(
        const ProjectSettings& settings,
        CanvasExtent output_surface) noexcept
    {
        return {settings.logical_canvas, output_surface, settings.viewport_policy, true};
    }

    struct SpriteBatchDiagnostics final
    {
        std::uint64_t input_submissions{};
        std::uint64_t hidden_submissions{};
        std::uint64_t accepted_submissions{};
        std::uint64_t emitted_sprites{};
        std::uint64_t emitted_vertices{};
        std::uint64_t emitted_indices{};
        std::uint64_t emitted_batches{};
        std::uint64_t material_count{};
        std::uint64_t invalid_handle_rejections{};
        std::uint64_t invalid_material_rejections{};
        std::uint64_t invalid_sprite_rejections{};
        std::uint64_t material_collision_rejections{};
        std::uint64_t duplicate_sort_key_rejections{};
        std::uint64_t submission_limit_rejections{};
        std::uint64_t material_limit_rejections{};
        std::uint64_t batch_limit_rejections{};
        std::uint64_t geometry_limit_rejections{};
    };

    struct SpriteBatchCompilation final
    {
        ResultCode code{ResultCode::success};
        std::vector<SpriteVertex> vertices{};
        std::vector<std::uint32_t> indices{};
        std::vector<CompiledSpriteBatch> batches{};
        std::vector<SpriteHandle> draw_order{};
        SpriteBatchDiagnostics diagnostics{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return succeeded(code);
        }
    };

    namespace detail
    {
        struct AcceptedSprite final
        {
            const SpriteSubmission* submission{};
            SpriteSortKey key{};
            std::uint32_t input_index{};
        };

        struct MaterialRecord final
        {
            std::uint32_t stable_key{};
            SpriteMaterialDeclaration declaration{};
        };

        [[nodiscard]] inline Float2 snap_sprite_position(
            Float2 position,
            PixelSnapMode mode) noexcept
        {
            if (!snaps_sprites(mode))
                return position;
            return {std::round(position.x), std::round(position.y)};
        }

        inline void append_quad(
            const SpriteSubmission& submission,
            PixelSnapMode snap_mode,
            SpriteBatchCompilation& output)
        {
            const Float2 position = snap_sprite_position(
                submission.transform.position,
                snap_mode);
            const float left = -submission.transform.pivot.x * submission.transform.size.x;
            const float top = -submission.transform.pivot.y * submission.transform.size.y;
            const float right = left + submission.transform.size.x;
            const float bottom = top + submission.transform.size.y;
            const float cosine = std::cos(submission.transform.rotation_radians);
            const float sine = std::sin(submission.transform.rotation_radians);

            const auto transform = [&](float x, float y) noexcept -> Float2
            {
                return {
                    position.x + cosine * x - sine * y,
                    position.y + sine * x + cosine * y
                };
            };

            float u0 = submission.source_uv.x;
            float v0 = submission.source_uv.y;
            float u1 = submission.source_uv.x + submission.source_uv.width;
            float v1 = submission.source_uv.y + submission.source_uv.height;
            if (submission.flip_x)
                std::swap(u0, u1);
            if (submission.flip_y)
                std::swap(v0, v1);

            const std::uint32_t base = static_cast<std::uint32_t>(output.vertices.size());
            output.vertices.push_back({transform(left, top), {u0, v0}, submission.tint});
            output.vertices.push_back({transform(right, top), {u1, v0}, submission.tint});
            output.vertices.push_back({transform(right, bottom), {u1, v1}, submission.tint});
            output.vertices.push_back({transform(left, bottom), {u0, v1}, submission.tint});
            output.indices.insert(output.indices.end(), {
                base, base + 1, base + 2,
                base, base + 2, base + 3
            });
        }
    }

    [[nodiscard]] inline SpriteBatchCompilation compile_sprite_batches(
        std::span<const SpriteSubmission> submissions,
        const SpriteBatchLimits& limits = {},
        PixelSnapMode snap_mode = PixelSnapMode::none)
    {
        SpriteBatchCompilation output{};
        output.diagnostics.input_submissions = submissions.size();
        if (!valid(limits))
        {
            output.code = ResultCode::invalid_limits;
            return output;
        }

        try
        {
            const std::size_t accepted_capacity = (std::min)(
                submissions.size(),
                static_cast<std::size_t>(limits.maximum_submissions));
            std::vector<detail::AcceptedSprite> accepted{};
            std::vector<detail::MaterialRecord> materials{};
            accepted.reserve(accepted_capacity);
            materials.reserve((std::min)(
                accepted_capacity,
                static_cast<std::size_t>(limits.maximum_materials)));

            for (std::size_t input_index = 0; input_index < submissions.size(); ++input_index)
            {
                const SpriteSubmission& submission = submissions[input_index];
                if (submission.hidden)
                {
                    ++output.diagnostics.hidden_submissions;
                    continue;
                }
                if (accepted.size() >= limits.maximum_submissions)
                {
                    ++output.diagnostics.submission_limit_rejections;
                    continue;
                }

                const ResultCode validation = validate(submission);
                if (validation != ResultCode::success)
                {
                    if (validation == ResultCode::invalid_handle)
                        ++output.diagnostics.invalid_handle_rejections;
                    else if (validation == ResultCode::invalid_material)
                        ++output.diagnostics.invalid_material_rejections;
                    else
                        ++output.diagnostics.invalid_sprite_rejections;
                    continue;
                }

                const auto material = std::find_if(
                    materials.begin(),
                    materials.end(),
                    [&](const detail::MaterialRecord& record)
                    {
                        return record.stable_key == submission.material.stable_key;
                    });
                if (material != materials.end())
                {
                    if (!same_material_intent(material->declaration, submission.material))
                    {
                        ++output.diagnostics.material_collision_rejections;
                        continue;
                    }
                }
                else
                {
                    if (materials.size() >= limits.maximum_materials)
                    {
                        ++output.diagnostics.material_limit_rejections;
                        continue;
                    }
                    materials.push_back({submission.material.stable_key, submission.material});
                }

                accepted.push_back({
                    &submission,
                    sort_key(submission),
                    static_cast<std::uint32_t>(input_index)
                });
            }

            std::sort(
                accepted.begin(),
                accepted.end(),
                [](const detail::AcceptedSprite& lhs, const detail::AcceptedSprite& rhs)
                {
                    if (less(lhs.key, rhs.key))
                        return true;
                    if (less(rhs.key, lhs.key))
                        return false;
                    return lhs.input_index < rhs.input_index;
                });

            output.vertices.reserve((std::min)(
                static_cast<std::size_t>(limits.maximum_vertices),
                accepted.size() * std::size_t{4}));
            output.indices.reserve((std::min)(
                static_cast<std::size_t>(limits.maximum_indices),
                accepted.size() * std::size_t{6}));
            output.batches.reserve((std::min)(
                static_cast<std::size_t>(limits.maximum_batches),
                accepted.size()));
            output.draw_order.reserve(accepted.size());

            std::optional<SpriteSortKey> previous_key{};
            for (const detail::AcceptedSprite& accepted_sprite : accepted)
            {
                const SpriteSubmission& submission = *accepted_sprite.submission;
                if (previous_key && *previous_key == accepted_sprite.key)
                {
                    ++output.diagnostics.duplicate_sort_key_rejections;
                    continue;
                }
                previous_key = accepted_sprite.key;

                if (output.vertices.size() + 4 > limits.maximum_vertices
                    || output.indices.size() + 6 > limits.maximum_indices)
                {
                    ++output.diagnostics.geometry_limit_rejections;
                    continue;
                }

                const SpriteBatchKey batch_key{submission.material, submission.clip_key};
                const bool needs_batch = output.batches.empty()
                    || output.batches.back().key != batch_key
                    || output.batches.back().sprite_count >= limits.maximum_sprites_per_batch;
                if (needs_batch)
                {
                    if (output.batches.size() >= limits.maximum_batches)
                    {
                        ++output.diagnostics.batch_limit_rejections;
                        continue;
                    }
                    output.batches.push_back({
                        batch_key,
                        static_cast<std::uint32_t>(output.vertices.size()),
                        0,
                        static_cast<std::uint32_t>(output.indices.size()),
                        0,
                        0,
                        accepted_sprite.key,
                        accepted_sprite.key
                    });
                }

                detail::append_quad(submission, snap_mode, output);
                CompiledSpriteBatch& batch = output.batches.back();
                batch.vertex_count += 4;
                batch.index_count += 6;
                ++batch.sprite_count;
                batch.last_sort_key = accepted_sprite.key;
                output.draw_order.push_back(submission.sprite);
                ++output.diagnostics.emitted_sprites;
            }

            output.diagnostics.accepted_submissions = accepted.size();
            output.diagnostics.emitted_vertices = output.vertices.size();
            output.diagnostics.emitted_indices = output.indices.size();
            output.diagnostics.emitted_batches = output.batches.size();
            output.diagnostics.material_count = materials.size();

            const std::uint64_t rejected =
                output.diagnostics.invalid_handle_rejections
                + output.diagnostics.invalid_material_rejections
                + output.diagnostics.invalid_sprite_rejections
                + output.diagnostics.material_collision_rejections
                + output.diagnostics.duplicate_sort_key_rejections
                + output.diagnostics.submission_limit_rejections
                + output.diagnostics.material_limit_rejections
                + output.diagnostics.batch_limit_rejections
                + output.diagnostics.geometry_limit_rejections;
            output.code = rejected == 0 ? ResultCode::success : ResultCode::partial;
            return output;
        }
        catch (const std::bad_alloc&)
        {
            output.vertices.clear();
            output.indices.clear();
            output.batches.clear();
            output.draw_order.clear();
            output.code = ResultCode::allocation_failure;
            return output;
        }
        catch (const std::length_error&)
        {
            output.vertices.clear();
            output.indices.clear();
            output.batches.clear();
            output.draw_order.clear();
            output.code = ResultCode::capacity_exceeded;
            return output;
        }
    }

    struct TileSetHandle final
    {
        std::uint32_t index{invalid_index};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        friend constexpr bool operator==(TileSetHandle, TileSetHandle) noexcept = default;
        friend constexpr auto operator<=>(TileSetHandle, TileSetHandle) noexcept = default;
    };

    struct TileLayerHandle final
    {
        std::uint32_t index{invalid_index};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != invalid_index && generation != 0;
        }

        friend constexpr bool operator==(TileLayerHandle, TileLayerHandle) noexcept = default;
        friend constexpr auto operator<=>(TileLayerHandle, TileLayerHandle) noexcept = default;
    };

    struct TileValidationLimits final
    {
        std::uint32_t maximum_tile_extent{4'096};
        std::uint32_t maximum_tiles_per_set{1'048'576};
        std::uint32_t maximum_layer_dimension_tiles{1'048'576};
        std::uint64_t maximum_cells_per_layer{1'073'741'824};
        std::uint32_t maximum_chunk_extent{512};
        std::uint32_t maximum_cells_per_chunk{65'536};
        std::uint32_t maximum_animation_frames{1'024};

        friend constexpr bool operator==(
            const TileValidationLimits&,
            const TileValidationLimits&) noexcept = default;
    };

    [[nodiscard]] constexpr bool valid(const TileValidationLimits& limits) noexcept
    {
        return limits.maximum_tile_extent != 0
            && limits.maximum_tiles_per_set != 0
            && limits.maximum_layer_dimension_tiles != 0
            && limits.maximum_cells_per_layer != 0
            && limits.maximum_chunk_extent != 0
            && limits.maximum_cells_per_chunk != 0
            && limits.maximum_animation_frames != 0;
    }

    struct TileSetDescriptor final
    {
        TileSetHandle handle{};
        TextureHandle texture{};
        CanvasExtent texture_extent{256, 256};
        UInt2 tile_extent{16, 16};
        UInt2 grid{16, 16};
        UInt2 margin{};
        UInt2 spacing{};
        std::uint32_t tile_count{256};
        std::uint32_t first_global_tile{};
        SpriteMaterialDeclaration material{};

        friend constexpr bool operator==(const TileSetDescriptor&, const TileSetDescriptor&) noexcept = default;
    };

    struct TileSetValidation final
    {
        ResultCode code{ResultCode::invalid_tile_set};
        std::uint64_t grid_capacity{};
        UInt2 required_texture_extent{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    [[nodiscard]] inline TileSetValidation validate(
        const TileSetDescriptor& descriptor,
        const TileValidationLimits& limits = {}) noexcept
    {
        TileSetValidation result{};
        if (!valid(limits) || !descriptor.handle.valid() || !descriptor.texture
            || !valid(descriptor.material)
            || descriptor.material.source == SpriteSourceKind::solid_color
            || descriptor.material.texture != descriptor.texture
            || descriptor.texture_extent.empty()
            || descriptor.tile_extent.x == 0 || descriptor.tile_extent.y == 0
            || descriptor.tile_extent.x > limits.maximum_tile_extent
            || descriptor.tile_extent.y > limits.maximum_tile_extent
            || descriptor.grid.x == 0 || descriptor.grid.y == 0
            || descriptor.tile_count == 0
            || descriptor.tile_count > limits.maximum_tiles_per_set)
        {
            return result;
        }

        if (!checked_multiply(descriptor.grid.x, descriptor.grid.y, result.grid_capacity))
        {
            result.code = ResultCode::arithmetic_overflow;
            return result;
        }
        if (descriptor.tile_count > result.grid_capacity)
            return result;

        std::uint64_t last_global_tile{};
        if (!checked_add(
                descriptor.first_global_tile,
                descriptor.tile_count - 1,
                last_global_tile)
            || last_global_tile > (std::numeric_limits<std::uint32_t>::max)())
        {
            result.code = ResultCode::arithmetic_overflow;
            return result;
        }

        std::uint64_t width{};
        std::uint64_t height{};
        std::uint64_t horizontal_spacing{};
        std::uint64_t vertical_spacing{};
        if (!checked_multiply(descriptor.grid.x, descriptor.tile_extent.x, width)
            || !checked_multiply(descriptor.grid.y, descriptor.tile_extent.y, height)
            || !checked_multiply(descriptor.grid.x - 1, descriptor.spacing.x, horizontal_spacing)
            || !checked_multiply(descriptor.grid.y - 1, descriptor.spacing.y, vertical_spacing)
            || !checked_add(width, horizontal_spacing, width)
            || !checked_add(height, vertical_spacing, height)
            || !checked_add(width, std::uint64_t{2} * descriptor.margin.x, width)
            || !checked_add(height, std::uint64_t{2} * descriptor.margin.y, height))
        {
            result.code = ResultCode::arithmetic_overflow;
            return result;
        }
        if (width > descriptor.texture_extent.width
            || height > descriptor.texture_extent.height
            || width > (std::numeric_limits<std::uint32_t>::max)()
            || height > (std::numeric_limits<std::uint32_t>::max)())
        {
            return result;
        }
        result.required_texture_extent = {
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height)
        };
        result.code = ResultCode::success;
        return result;
    }

    struct TileLayerDescriptor final
    {
        TileLayerHandle handle{};
        TileSetHandle tile_set{};
        UInt2 extent_tiles{64, 64};
        UInt2 chunk_extent{32, 32};
        Float2 world_origin{};
        Float2 tile_world_extent{1.0f, 1.0f};
        Float2 parallax{1.0f, 1.0f};
        SpritePhase phase{SpritePhase::world};
        std::int32_t layer{};
        float opacity{1.0f};
        bool visible{true};
        bool collision_source{};

        friend constexpr bool operator==(
            const TileLayerDescriptor&,
            const TileLayerDescriptor&) noexcept = default;
    };

    struct TileLayerValidation final
    {
        ResultCode code{ResultCode::invalid_tile_layer};
        std::uint64_t cell_count{};
        UInt2 chunk_grid{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    [[nodiscard]] inline TileLayerValidation validate(
        const TileLayerDescriptor& descriptor,
        const TileValidationLimits& limits = {}) noexcept
    {
        TileLayerValidation result{};
        if (!valid(limits) || !descriptor.handle.valid() || !descriptor.tile_set.valid()
            || descriptor.extent_tiles.x == 0 || descriptor.extent_tiles.y == 0
            || descriptor.extent_tiles.x > limits.maximum_layer_dimension_tiles
            || descriptor.extent_tiles.y > limits.maximum_layer_dimension_tiles
            || descriptor.chunk_extent.x == 0 || descriptor.chunk_extent.y == 0
            || descriptor.chunk_extent.x > limits.maximum_chunk_extent
            || descriptor.chunk_extent.y > limits.maximum_chunk_extent
            || !finite(descriptor.world_origin)
            || !finite(descriptor.tile_world_extent)
            || descriptor.tile_world_extent.x <= 0.0f
            || descriptor.tile_world_extent.y <= 0.0f
            || !finite(descriptor.parallax)
            || !std::isfinite(descriptor.opacity)
            || descriptor.opacity < 0.0f || descriptor.opacity > 1.0f)
        {
            return result;
        }
        if (!checked_multiply(
                descriptor.extent_tiles.x,
                descriptor.extent_tiles.y,
                result.cell_count))
        {
            result.code = ResultCode::arithmetic_overflow;
            return result;
        }
        if (result.cell_count > limits.maximum_cells_per_layer)
        {
            result.code = ResultCode::capacity_exceeded;
            return result;
        }
        const std::uint64_t chunk_columns =
            (std::uint64_t{descriptor.extent_tiles.x}
                + descriptor.chunk_extent.x - 1)
            / descriptor.chunk_extent.x;
        const std::uint64_t chunk_rows =
            (std::uint64_t{descriptor.extent_tiles.y}
                + descriptor.chunk_extent.y - 1)
            / descriptor.chunk_extent.y;
        if (chunk_columns > (std::numeric_limits<std::uint32_t>::max)()
            || chunk_rows > (std::numeric_limits<std::uint32_t>::max)())
        {
            result.code = ResultCode::arithmetic_overflow;
            return result;
        }
        result.chunk_grid = {
            static_cast<std::uint32_t>(chunk_columns),
            static_cast<std::uint32_t>(chunk_rows)
        };
        result.code = ResultCode::success;
        return result;
    }

    enum class TileTransform : std::uint8_t
    {
        identity,
        flip_x,
        flip_y,
        flip_xy,
        rotate_90,
        rotate_180,
        rotate_270
    };

    struct TileCell final
    {
        std::uint32_t tile{empty_tile};
        TileTransform transform{TileTransform::identity};
        LinearColor tint{};
        std::uint16_t animation_frame{};
        std::uint16_t flags{};

        [[nodiscard]] constexpr bool occupied() const noexcept
        {
            return tile != empty_tile;
        }
    };

    struct TileChunkDescriptor final
    {
        TileLayerHandle layer{};
        UInt2 coordinate{};
        UInt2 extent{};
        std::uint64_t revision{1};
        std::span<const TileCell> cells{};
    };

    struct TileChunkValidation final
    {
        ResultCode code{ResultCode::invalid_tile_chunk};
        std::uint64_t expected_cells{};
        std::uint64_t occupied_cells{};
        std::uint64_t invalid_cells{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    [[nodiscard]] inline TileChunkValidation validate(
        const TileChunkDescriptor& chunk,
        const TileLayerDescriptor& layer,
        const TileSetDescriptor& tile_set,
        const TileValidationLimits& limits = {}) noexcept
    {
        TileChunkValidation result{};
        const TileLayerValidation layer_validation = validate(layer, limits);
        const TileSetValidation set_validation = validate(tile_set, limits);
        if (!layer_validation || !set_validation
            || !chunk.layer.valid() || chunk.layer != layer.handle
            || layer.tile_set != tile_set.handle
            || chunk.revision == 0
            || chunk.coordinate.x >= layer_validation.chunk_grid.x
            || chunk.coordinate.y >= layer_validation.chunk_grid.y
            || chunk.extent.x == 0 || chunk.extent.y == 0
            || chunk.extent.x > layer.chunk_extent.x
            || chunk.extent.y > layer.chunk_extent.y)
        {
            return result;
        }

        const std::uint64_t origin_x =
            std::uint64_t{chunk.coordinate.x} * layer.chunk_extent.x;
        const std::uint64_t origin_y =
            std::uint64_t{chunk.coordinate.y} * layer.chunk_extent.y;
        if (origin_x + chunk.extent.x > layer.extent_tiles.x
            || origin_y + chunk.extent.y > layer.extent_tiles.y
            || !checked_multiply(chunk.extent.x, chunk.extent.y, result.expected_cells))
        {
            result.code = ResultCode::arithmetic_overflow;
            return result;
        }
        if (result.expected_cells > limits.maximum_cells_per_chunk)
        {
            result.code = ResultCode::capacity_exceeded;
            return result;
        }
        if (chunk.cells.size() != result.expected_cells)
            return result;

        for (const TileCell& cell : chunk.cells)
        {
            if (!cell.occupied())
                continue;
            ++result.occupied_cells;
            const bool valid_transform = cell.transform >= TileTransform::identity
                && cell.transform <= TileTransform::rotate_270;
            if (cell.tile >= tile_set.tile_count
                || cell.animation_frame >= limits.maximum_animation_frames
                || !finite(cell.tint)
                || !valid_transform)
            {
                ++result.invalid_cells;
            }
        }
        if (result.invalid_cells != 0)
            return result;
        result.code = ResultCode::success;
        return result;
    }

    [[nodiscard]] constexpr std::uint64_t tile_stable_sequence(
        TileLayerHandle layer,
        UInt2 chunk_coordinate,
        std::uint32_t local_cell_index) noexcept
    {
        std::uint64_t value = 1469598103934665603ull;
        const auto mix = [&](std::uint64_t part) constexpr
        {
            value ^= part;
            value *= 1099511628211ull;
        };
        mix(layer.index);
        mix(layer.generation);
        mix(chunk_coordinate.x);
        mix(chunk_coordinate.y);
        mix(local_cell_index);
        return value == 0 ? 1 : value;
    }

    enum class ComposeTargetKind : std::uint8_t
    {
        presentation_surface,
        render_target
    };

    struct ComposeResourceBindings final
    {
        RenderTextureAssetHandles canvas{};
        RenderTargetHandle destination{};
    };

    struct ComposeRequest final
    {
        ViewportRequest viewport{};
        CameraState camera{};
        ComposeTargetKind destination{ComposeTargetKind::presentation_surface};
        TextureFormat color_format{TextureFormat::rgba8_unorm};
        FilterMode presentation_filter{FilterMode::nearest};
        LinearColor clear_color{0.03f, 0.04f, 0.06f, 1.0f};
        LinearColor letterbox_color{0.0f, 0.0f, 0.0f, 1.0f};
        std::uint32_t sprite_batch_count{};
        std::uint32_t sprite_count{};
        std::uint32_t tile_layer_count{};
        bool clear_canvas{true};
        bool clear_letterbox{true};
    };

    struct FinalComposePlan final
    {
        ResultCode code{ResultCode::invalid_canvas_extent};
        ViewportPlan viewport{};
        CameraPlan camera{};
        RenderTextureAssetPlan canvas_target{};
        ComposeTargetKind destination{ComposeTargetKind::presentation_surface};
        FilterMode presentation_filter{FilterMode::nearest};
        LinearColor clear_color{};
        LinearColor letterbox_color{};
        std::uint32_t sprite_batch_count{};
        std::uint32_t sprite_count{};
        std::uint32_t tile_layer_count{};
        bool clear_canvas{};
        bool clear_letterbox{};
        bool requires_offscreen_canvas{true};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return code == ResultCode::success;
        }
    };

    [[nodiscard]] inline FinalComposePlan make_final_compose_plan(
        const ComposeRequest& request,
        const CanvasLimits& limits = {}) noexcept
    {
        FinalComposePlan plan{};
        plan.viewport = plan_viewport(request.viewport, limits);
        if (!plan.viewport)
        {
            plan.code = plan.viewport.code;
            return plan;
        }
        plan.camera = plan_camera(request.camera, plan.viewport, limits);
        if (!plan.camera)
        {
            plan.code = plan.camera.code;
            return plan;
        }
        if (!finite(request.clear_color) || !finite(request.letterbox_color)
            || request.color_format == TextureFormat::unknown
            || (request.presentation_filter != FilterMode::nearest
                && request.presentation_filter != FilterMode::linear))
        {
            plan.code = ResultCode::invalid_material;
            return plan;
        }

        RenderTextureAssetDesc target{};
        target.width = plan.viewport.render_extent.width;
        target.height = plan.viewport.render_extent.height;
        target.color_format = request.color_format;
        target.depth_format = TextureFormat::unknown;
        target.has_depth = false;
        target.sampled_after_render = true;
        target.usage = RenderTextureUsage::ui_surface;
        target.debug_name = "Canvas2D.Compose";
        plan.canvas_target = make_render_texture_asset_plan(target);
        plan.destination = request.destination;
        plan.presentation_filter = request.presentation_filter;
        plan.clear_color = request.clear_color;
        plan.letterbox_color = request.letterbox_color;
        plan.sprite_batch_count = request.sprite_batch_count;
        plan.sprite_count = request.sprite_count;
        plan.tile_layer_count = request.tile_layer_count;
        plan.clear_canvas = request.clear_canvas;
        plan.clear_letterbox = request.clear_letterbox;
        plan.requires_offscreen_canvas = request.viewport.policy != ViewportPolicy::resize_canvas
            || request.destination == ComposeTargetKind::render_target;
        plan.code = ResultCode::success;
        return plan;
    }

    [[nodiscard]] constexpr ResultCode validate_compose_resources(
        const FinalComposePlan& plan,
        const ComposeResourceBindings& resources) noexcept
    {
        if (!plan)
            return plan.code;
        if (plan.requires_offscreen_canvas
            && !resources.canvas.satisfies(plan.canvas_target.backend_requirements))
        {
            return ResultCode::invalid_handle;
        }
        if (plan.destination == ComposeTargetKind::render_target
            && !resources.destination)
        {
            return ResultCode::invalid_handle;
        }
        return ResultCode::success;
    }

    struct Canvas2DSubmission final
    {
        ProjectSettings project{};
        CameraState camera{};
        std::span<const SpriteSubmission> sprites{};
        std::span<const TileSetDescriptor> tile_sets{};
        std::span<const TileLayerDescriptor> tile_layers{};
        std::span<const TileChunkDescriptor> tile_chunks{};
        ComposeTargetKind destination{ComposeTargetKind::presentation_surface};
        TextureFormat color_format{TextureFormat::rgba8_unorm};
        LinearColor clear_color{0.03f, 0.04f, 0.06f, 1.0f};
        LinearColor letterbox_color{0.0f, 0.0f, 0.0f, 1.0f};
        std::uint64_t frame_sequence{1};
    };

    struct Canvas2DSubmissionDiagnostics final
    {
        std::uint64_t submitted_sprites{};
        std::uint64_t submitted_tile_sets{};
        std::uint64_t submitted_tile_layers{};
        std::uint64_t submitted_tile_chunks{};
        std::uint64_t accepted_tile_sets{};
        std::uint64_t accepted_tile_layers{};
        std::uint64_t accepted_tile_chunks{};
        std::uint64_t occupied_tiles{};
        std::uint64_t invalid_tile_sets{};
        std::uint64_t invalid_tile_layers{};
        std::uint64_t invalid_tile_chunks{};
        std::uint64_t missing_tile_set_references{};
        std::uint64_t missing_tile_layer_references{};
    };

    struct Canvas2DFramePlan final
    {
        ResultCode code{ResultCode::invalid_canvas_extent};
        std::uint64_t frame_sequence{};
        SpriteBatchCompilation sprites{};
        FinalComposePlan compose{};
        Canvas2DSubmissionDiagnostics diagnostics{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return succeeded(code) && static_cast<bool>(compose);
        }
    };

    [[nodiscard]] inline Canvas2DFramePlan compile_canvas2d_submission(
        const Canvas2DSubmission& submission,
        CanvasExtent output_surface,
        const CanvasLimits& canvas_limits = {},
        const TileValidationLimits& tile_limits = {})
    {
        Canvas2DFramePlan output{};
        output.frame_sequence = submission.frame_sequence;
        output.diagnostics.submitted_sprites = submission.sprites.size();
        output.diagnostics.submitted_tile_sets = submission.tile_sets.size();
        output.diagnostics.submitted_tile_layers = submission.tile_layers.size();
        output.diagnostics.submitted_tile_chunks = submission.tile_chunks.size();

        output.code = validate(submission.project, canvas_limits);
        if (output.code != ResultCode::success || submission.frame_sequence == 0)
        {
            if (submission.frame_sequence == 0)
                output.code = ResultCode::invalid_sprite;
            return output;
        }

        CameraState camera = submission.camera;
        camera.pixels_per_world_unit = submission.project.pixels_per_world_unit;
        camera.pixel_snap = submission.project.pixel_snap;
        if (!valid(camera, canvas_limits))
        {
            output.code = ResultCode::invalid_camera;
            return output;
        }

        SpriteBatchLimits batch_limits{};
        batch_limits.maximum_sprites_per_batch = submission.project.maximum_sprites_per_batch;
        batch_limits.maximum_batches = submission.project.maximum_batches;
        output.sprites = compile_sprite_batches(
            submission.sprites,
            batch_limits,
            submission.project.pixel_snap);
        if (!output.sprites)
        {
            output.code = output.sprites.code;
            return output;
        }

        const auto find_tile_set = [&](TileSetHandle handle) -> const TileSetDescriptor*
        {
            const auto it = std::find_if(
                submission.tile_sets.begin(),
                submission.tile_sets.end(),
                [&](const TileSetDescriptor& candidate) { return candidate.handle == handle; });
            return it == submission.tile_sets.end() ? nullptr : &*it;
        };
        const auto find_tile_layer = [&](TileLayerHandle handle) -> const TileLayerDescriptor*
        {
            const auto it = std::find_if(
                submission.tile_layers.begin(),
                submission.tile_layers.end(),
                [&](const TileLayerDescriptor& candidate) { return candidate.handle == handle; });
            return it == submission.tile_layers.end() ? nullptr : &*it;
        };

        for (const TileSetDescriptor& tile_set : submission.tile_sets)
        {
            if (validate(tile_set, tile_limits))
                ++output.diagnostics.accepted_tile_sets;
            else
                ++output.diagnostics.invalid_tile_sets;
        }
        for (const TileLayerDescriptor& layer : submission.tile_layers)
        {
            const TileSetDescriptor* tile_set = find_tile_set(layer.tile_set);
            if (!tile_set)
            {
                ++output.diagnostics.missing_tile_set_references;
                continue;
            }
            if (validate(layer, tile_limits) && validate(*tile_set, tile_limits))
                ++output.diagnostics.accepted_tile_layers;
            else
                ++output.diagnostics.invalid_tile_layers;
        }
        for (const TileChunkDescriptor& chunk : submission.tile_chunks)
        {
            const TileLayerDescriptor* layer = find_tile_layer(chunk.layer);
            if (!layer)
            {
                ++output.diagnostics.missing_tile_layer_references;
                continue;
            }
            const TileSetDescriptor* tile_set = find_tile_set(layer->tile_set);
            if (!tile_set)
            {
                ++output.diagnostics.missing_tile_set_references;
                continue;
            }
            const TileChunkValidation chunk_validation = validate(
                chunk,
                *layer,
                *tile_set,
                tile_limits);
            if (chunk_validation)
            {
                ++output.diagnostics.accepted_tile_chunks;
                output.diagnostics.occupied_tiles += chunk_validation.occupied_cells;
            }
            else
            {
                ++output.diagnostics.invalid_tile_chunks;
            }
        }

        ComposeRequest compose{};
        compose.viewport = make_viewport_request(submission.project, output_surface);
        compose.camera = camera;
        compose.destination = submission.destination;
        compose.color_format = submission.color_format;
        compose.presentation_filter = submission.project.presentation_filter;
        compose.clear_color = submission.clear_color;
        compose.letterbox_color = submission.letterbox_color;
        compose.sprite_batch_count = static_cast<std::uint32_t>(output.sprites.batches.size());
        compose.sprite_count = static_cast<std::uint32_t>(output.sprites.draw_order.size());
        compose.tile_layer_count = static_cast<std::uint32_t>(output.diagnostics.accepted_tile_layers);
        output.compose = make_final_compose_plan(compose, canvas_limits);
        if (!output.compose)
        {
            output.code = output.compose.code;
            return output;
        }

        const bool partial = output.sprites.code == ResultCode::partial
            || output.diagnostics.invalid_tile_sets != 0
            || output.diagnostics.invalid_tile_layers != 0
            || output.diagnostics.invalid_tile_chunks != 0
            || output.diagnostics.missing_tile_set_references != 0
            || output.diagnostics.missing_tile_layer_references != 0;
        output.code = partial ? ResultCode::partial : ResultCode::success;
        return output;
    }

    [[nodiscard]] constexpr bool canvas2d_constexpr_contract() noexcept
    {
        const CanvasLimits limits{};
        const auto extent = validate_canvas_extent({320, 180}, limits);
        const auto invalid_extent = validate_canvas_extent({0, 180}, limits);
        const SpriteHandle first{3, 2};
        const SpriteHandle second{4, 1};
        const SpriteSortKey first_key{
            SpritePhase::world, 1, 2, 0, 10, first
        };
        const SpriteSortKey second_key{
            SpritePhase::world, 1, 2, 0, 11, second
        };
        return valid(limits)
            && extent
            && extent.pixel_count == 57'600
            && !invalid_extent
            && first.valid()
            && less(first_key, second_key)
            && !less(second_key, first_key)
            && tile_stable_sequence({1, 1}, {2, 3}, 4) != 0;
    }

    [[nodiscard]] inline bool canvas2d_runtime_contract()
    {
        const ViewportPlan integer_view = plan_viewport({
            {320, 180},
            {1920, 1080},
            ViewportPolicy::integer_scale,
            true
        });
        const ViewportPlan letterbox_view = plan_viewport({
            {320, 180},
            {1280, 800},
            ViewportPolicy::letterbox,
            true
        });
        const ViewportPlan fill_view = plan_viewport({
            {320, 180},
            {800, 800},
            ViewportPolicy::fill_crop,
            true
        });
        if (!integer_view || !letterbox_view || !fill_view
            || integer_view.scale_x != 6.0f
            || integer_view.destination != RectI{0, 0, 1920, 1080}
            || letterbox_view.scale_x != 4.0f
            || letterbox_view.destination != RectI{0, 40, 1280, 720}
            || letterbox_view.letterbox_bar_count != 2
            || !fill_view.content_clipped
            || fill_view.clipped_destination != RectI{0, 0, 800, 800}
            || fill_view.letterbox_bar_count != 0)
        {
            return false;
        }

        const CameraPlan camera = plan_camera(
            {{0.49f, 1.51f}, 0.0f, 1.0f, 1.0f,
             PixelSnapMode::camera_and_sprites, CanvasYAxis::down},
            integer_view);
        if (!camera || camera.snapped_center != Float2{0.0f, 2.0f})
            return false;

        SpriteIdentityRegistry identities{2};
        const auto first = identities.create();
        const auto second = identities.create();
        if (!first || !second || !identities.alive(*first)
            || identities.create().has_value()
            || identities.retire(*first) != ResultCode::success
            || identities.alive(*first))
        {
            return false;
        }
        const auto replacement = identities.create();
        if (!replacement || replacement->index != first->index
            || replacement->generation == first->generation)
        {
            return false;
        }

        ProjectSettings project_settings{};
        project_settings.logical_canvas = {320, 180};
        project_settings.maximum_sprites_per_batch = 1;
        if (validate(project_settings) != ResultCode::success
            || make_viewport_request(project_settings, {1920, 1080}).policy
                != ViewportPolicy::integer_scale)
        {
            return false;
        }

        SpriteMaterialDeclaration material{};
        material.stable_key = 7;
        material.logical_texture = {7, 1};
        material.source = SpriteSourceKind::texture;
        material.texture = TextureHandle{1};
        SpriteMaterialDeclaration invalid_material = material;
        invalid_material.source = static_cast<SpriteSourceKind>(255);
        SpriteMaterialDeclaration invalid_sampler = material;
        invalid_sampler.sampler.address_u = static_cast<AddressMode>(255);
        if (valid(invalid_material) || valid(invalid_sampler.sampler)
            || valid(invalid_sampler))
        {
            return false;
        }
        SpriteMaterialDeclaration material_rebound = material;
        material_rebound.texture = TextureHandle{99};
        material_rebound.resolved_sampler = SamplerHandle{3};
        material_rebound.resolved_material = MaterialHandle{4};
        const std::array<SpriteSubmission, 2> submissions{{
            {*replacement, material, {{10.0f, 20.0f}, {8.0f, 8.0f}},
             {0.0f, 0.0f, 1.0f, 1.0f}, {}, SpritePhase::world, 0, 1, 0, 2},
            {*second, material_rebound, {{30.0f, 40.0f}, {8.0f, 8.0f}},
             {0.0f, 0.0f, 1.0f, 1.0f}, {}, SpritePhase::world, 0, 0, 0, 1}
        }};
        const SpriteBatchCompilation compilation = compile_sprite_batches(
            submissions,
            {},
            PixelSnapMode::sprites);
        if (!compilation || compilation.code != ResultCode::success
            || compilation.vertices.size() != 8
            || compilation.indices.size() != 12
            || compilation.batches.size() != 1
            || compilation.draw_order != std::vector<SpriteHandle>{*second, *replacement})
        {
            return false;
        }

        SpriteBatchLimits split_limits{};
        split_limits.maximum_sprites_per_batch = project_settings.maximum_sprites_per_batch;
        split_limits.maximum_batches = project_settings.maximum_batches;
        const SpriteBatchCompilation split_compilation = compile_sprite_batches(
            submissions,
            split_limits,
            PixelSnapMode::sprites);
        if (!split_compilation || split_compilation.batches.size() != 2
            || split_compilation.draw_order != compilation.draw_order)
        {
            return false;
        }

        TileSetDescriptor tile_set{};
        tile_set.handle = {0, 1};
        tile_set.texture = material.texture;
        tile_set.material = material;
        const TileSetValidation tile_set_validation = validate(tile_set);
        if (!tile_set_validation
            || tile_set_validation.required_texture_extent != UInt2{256, 256})
        {
            return false;
        }

        TileLayerDescriptor layer{};
        layer.handle = {0, 1};
        layer.tile_set = tile_set.handle;
        const TileLayerValidation layer_validation = validate(layer);
        if (!layer_validation || layer_validation.chunk_grid != UInt2{2, 2})
            return false;

        std::array<TileCell, 4> cells{};
        cells[0].tile = 0;
        cells[1].tile = 1;
        const TileChunkDescriptor chunk{
            layer.handle,
            {0, 0},
            {2, 2},
            1,
            cells
        };
        const TileChunkValidation chunk_validation = validate(chunk, layer, tile_set);
        if (!chunk_validation || chunk_validation.occupied_cells != 2)
            return false;

        ComposeRequest compose{};
        compose.viewport = {{320, 180}, {1920, 1080}, ViewportPolicy::integer_scale, true};
        compose.camera = {};
        compose.presentation_filter = project_settings.presentation_filter;
        compose.sprite_batch_count = static_cast<std::uint32_t>(compilation.batches.size());
        compose.sprite_count = static_cast<std::uint32_t>(compilation.draw_order.size());
        compose.tile_layer_count = 1;
        const FinalComposePlan final_plan = make_final_compose_plan(compose);
        ComposeRequest invalid_compose = compose;
        invalid_compose.presentation_filter = static_cast<FilterMode>(255);
        if (make_final_compose_plan(invalid_compose).code
            != ResultCode::invalid_material)
        {
            return false;
        }

        const std::array<TileSetDescriptor, 1> tile_sets{tile_set};
        const std::array<TileLayerDescriptor, 1> tile_layers{layer};
        const std::array<TileChunkDescriptor, 1> tile_chunks{chunk};
        Canvas2DSubmission frame{};
        frame.project = project_settings;
        frame.camera = compose.camera;
        frame.sprites = submissions;
        frame.tile_sets = tile_sets;
        frame.tile_layers = tile_layers;
        frame.tile_chunks = tile_chunks;
        const Canvas2DFramePlan frame_plan = compile_canvas2d_submission(
            frame,
            {1920, 1080});
        return final_plan && frame_plan
            && final_plan.presentation_filter == FilterMode::nearest
            && final_plan.canvas_target.render_target.width == 320
            && final_plan.canvas_target.render_target.height == 180
            && final_plan.sprite_count == 2
            && final_plan.tile_layer_count == 1
            && frame_plan.code == ResultCode::success
            && frame_plan.sprites.batches.size() == 2
            && frame_plan.diagnostics.accepted_tile_chunks == 1
            && frame_plan.diagnostics.occupied_tiles == 2;
    }

    static_assert(canvas2d_constexpr_contract());
}
