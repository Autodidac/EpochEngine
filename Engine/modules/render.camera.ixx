// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>

export module render.camera;

import render.math;

export namespace epochengine::render_camera
{
    using Vec3 = render_math::Float3;

    enum class ProjectionKind : std::uint8_t
    {
        perspective,
        orthographic
    };

    enum class ViewPurpose : std::uint8_t
    {
        editor,
        project_runtime,
        canvas_2d,
        asset_preview,
        render_to_texture,
        portal,
        mirror,
        capture
    };

    enum class ViewOrientation : std::uint8_t
    {
        free,
        front,
        back,
        left,
        right,
        top,
        bottom
    };

    struct ViewHandle final
    {
        std::uint64_t value{};
        std::uint32_t generation{};

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value != 0u && generation != 0u;
        }

        friend constexpr bool operator==(const ViewHandle&, const ViewHandle&) noexcept = default;
    };

    struct PerspectiveProjection final
    {
        float vertical_field_of_view_radians{0.90f};
        float near_plane{0.05f};
        float far_plane{4096.0f};

        friend constexpr bool operator==(const PerspectiveProjection&, const PerspectiveProjection&) noexcept = default;
    };

    struct OrthographicProjection final
    {
        float vertical_size{10.0f};
        float near_plane{0.01f};
        float far_plane{4096.0f};

        friend constexpr bool operator==(const OrthographicProjection&, const OrthographicProjection&) noexcept = default;
    };

    struct ClipPlane final
    {
        Vec3 normal{0.0f, 0.0f, 1.0f};
        float distance{};
        bool enabled{};

        friend constexpr bool operator==(const ClipPlane&, const ClipPlane&) noexcept = default;
    };

    struct ViewDescriptor final
    {
        ViewHandle handle{};
        std::uint64_t scene_id{};
        ViewPurpose purpose{ViewPurpose::editor};
        ProjectionKind projection{ProjectionKind::perspective};
        ViewOrientation orientation{ViewOrientation::free};
        Vec3 position{9.0f, 7.0f, 9.0f};
        Vec3 target{};
        Vec3 up{0.0f, 1.0f, 0.0f};
        PerspectiveProjection perspective{};
        OrthographicProjection orthographic{};
        std::array<ClipPlane, 4> clip_planes{};
        std::uint8_t clip_plane_count{};
        std::uint64_t source_revision{1u};
        bool editor_navigation_enabled{true};

        friend constexpr bool operator==(const ViewDescriptor&, const ViewDescriptor&) noexcept = default;
    };

    struct ResolvedView final
    {
        ViewDescriptor descriptor{};
        Vec3 forward{0.0f, 0.0f, -1.0f};
        Vec3 right{1.0f, 0.0f, 0.0f};
        Vec3 up{0.0f, 1.0f, 0.0f};
        float near_plane{0.05f};
        float far_plane{4096.0f};
        float vertical_field_of_view_radians{0.90f};
        float orthographic_vertical_size{10.0f};
        bool valid{};
    };

    struct OrientationBasis final
    {
        Vec3 forward{0.0f, 0.0f, -1.0f};
        Vec3 up{0.0f, 1.0f, 0.0f};
    };

    [[nodiscard]] constexpr std::string_view projection_name(ProjectionKind projection) noexcept
    {
        switch (projection)
        {
        case ProjectionKind::orthographic: return "Orthographic";
        case ProjectionKind::perspective:
        default: return "Perspective";
        }
    }

    [[nodiscard]] constexpr std::string_view orientation_name(ViewOrientation orientation) noexcept
    {
        switch (orientation)
        {
        case ViewOrientation::front: return "Front";
        case ViewOrientation::back: return "Back";
        case ViewOrientation::left: return "Left";
        case ViewOrientation::right: return "Right";
        case ViewOrientation::top: return "Top";
        case ViewOrientation::bottom: return "Bottom";
        case ViewOrientation::free:
        default: return "Free";
        }
    }

    [[nodiscard]] constexpr bool axis_locked(ViewOrientation orientation) noexcept
    {
        return orientation != ViewOrientation::free;
    }

    [[nodiscard]] constexpr OrientationBasis orientation_basis(ViewOrientation orientation) noexcept
    {
        switch (orientation)
        {
        case ViewOrientation::front:
            return {{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}};
        case ViewOrientation::back:
            return {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}};
        case ViewOrientation::left:
            return {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        case ViewOrientation::right:
            return {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        case ViewOrientation::top:
            return {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
        case ViewOrientation::bottom:
            return {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
        case ViewOrientation::free:
        default:
            return {{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}};
        }
    }

    [[nodiscard]] inline bool valid_projection(const PerspectiveProjection& projection) noexcept
    {
        constexpr float kPi = 3.14159265358979323846f;
        return std::isfinite(projection.vertical_field_of_view_radians)
            && projection.vertical_field_of_view_radians > 0.01f
            && projection.vertical_field_of_view_radians < (kPi - 0.01f)
            && std::isfinite(projection.near_plane)
            && std::isfinite(projection.far_plane)
            && projection.near_plane > 0.0f
            && projection.far_plane > projection.near_plane;
    }

    [[nodiscard]] inline bool valid_projection(const OrthographicProjection& projection) noexcept
    {
        return std::isfinite(projection.vertical_size)
            && projection.vertical_size > 0.0f
            && std::isfinite(projection.near_plane)
            && std::isfinite(projection.far_plane)
            && projection.near_plane >= 0.0f
            && projection.far_plane > projection.near_plane;
    }

    [[nodiscard]] inline bool valid_clip_plane(const ClipPlane& plane) noexcept
    {
        return !plane.enabled
            || (render_math::finite(plane.normal)
                && render_math::length_squared(plane.normal) > 1.0e-8f
                && std::isfinite(plane.distance));
    }

    [[nodiscard]] inline ResolvedView resolve(ViewDescriptor descriptor) noexcept
    {
        ResolvedView result{};
        result.descriptor = descriptor;

        if (!descriptor.handle
            || descriptor.source_revision == 0u
            || descriptor.scene_id == 0u
            || !render_math::finite(descriptor.position)
            || !render_math::finite(descriptor.target)
            || !render_math::finite(descriptor.up)
            || descriptor.clip_plane_count > descriptor.clip_planes.size())
        {
            return result;
        }

        for (std::size_t index = 0; index < descriptor.clip_plane_count; ++index)
        {
            if (!valid_clip_plane(descriptor.clip_planes[index]))
                return result;
        }

        Vec3 forward = render_math::subtract(descriptor.target, descriptor.position);
        Vec3 requestedUp = descriptor.up;
        if (axis_locked(descriptor.orientation))
        {
            const auto basis = orientation_basis(descriptor.orientation);
            forward = basis.forward;
            requestedUp = basis.up;
            result.descriptor.target = render_math::add(descriptor.position, forward);
            result.descriptor.up = requestedUp;
        }

        result.forward = render_math::normalize(forward, {0.0f, 0.0f, -1.0f});
        result.right = render_math::normalize(
            render_math::cross(result.forward, requestedUp),
            {1.0f, 0.0f, 0.0f});
        result.up = render_math::normalize(
            render_math::cross(result.right, result.forward),
            requestedUp);

        if (!render_math::finite(result.forward)
            || !render_math::finite(result.right)
            || !render_math::finite(result.up)
            || render_math::length_squared(result.forward) < 0.99f
            || render_math::length_squared(result.right) < 0.99f
            || render_math::length_squared(result.up) < 0.99f)
        {
            return result;
        }

        if (descriptor.projection == ProjectionKind::perspective)
        {
            if (!valid_projection(descriptor.perspective))
                return result;
            result.near_plane = descriptor.perspective.near_plane;
            result.far_plane = descriptor.perspective.far_plane;
            result.vertical_field_of_view_radians = descriptor.perspective.vertical_field_of_view_radians;
        }
        else
        {
            if (!valid_projection(descriptor.orthographic))
                return result;
            result.near_plane = descriptor.orthographic.near_plane;
            result.far_plane = descriptor.orthographic.far_plane;
            result.orthographic_vertical_size = descriptor.orthographic.vertical_size;
        }

        result.valid = true;
        return result;
    }

    [[nodiscard]] inline ViewDescriptor make_editor_view(
        ViewHandle handle,
        Vec3 position,
        Vec3 target,
        Vec3 up,
        ProjectionKind projection,
        ViewOrientation orientation,
        float orthographic_vertical_size,
        std::uint64_t source_revision) noexcept
    {
        ViewDescriptor descriptor{};
        descriptor.handle = handle;
        descriptor.scene_id = 1u;
        descriptor.purpose = ViewPurpose::editor;
        descriptor.projection = projection;
        descriptor.orientation = orientation;
        descriptor.position = position;
        descriptor.target = target;
        descriptor.up = up;
        descriptor.orthographic.vertical_size = (std::max)(0.01f, orthographic_vertical_size);
        descriptor.source_revision = (std::max)(std::uint64_t{1u}, source_revision);
        descriptor.editor_navigation_enabled = true;
        return descriptor;
    }

    [[nodiscard]] inline bool add_clip_plane(
        ViewDescriptor& descriptor,
        ClipPlane plane) noexcept
    {
        if (!valid_clip_plane(plane)
            || descriptor.clip_plane_count >= descriptor.clip_planes.size())
        {
            return false;
        }
        if (plane.enabled)
            plane.normal = render_math::normalize(plane.normal, {0.0f, 0.0f, 1.0f});
        descriptor.clip_planes[descriptor.clip_plane_count++] = plane;
        return true;
    }

    struct ContractChecks final
    {
        bool perspective{};
        bool orthographic{};
        bool axes{};
        bool invalid_identity_refused{};
        bool invalid_projection_refused{};
        bool clip_plane_bounded{};

        [[nodiscard]] constexpr bool passed() const noexcept
        {
            return perspective
                && orthographic
                && axes
                && invalid_identity_refused
                && invalid_projection_refused
                && clip_plane_bounded;
        }
    };

    [[nodiscard]] inline ContractChecks run_contract_checks() noexcept
    {
        ViewDescriptor perspectiveView = make_editor_view(
            {1u, 1u},
            {4.0f, 3.0f, 4.0f},
            {},
            {0.0f, 1.0f, 0.0f},
            ProjectionKind::perspective,
            ViewOrientation::free,
            10.0f,
            2u);
        const auto perspectiveResolved = resolve(perspectiveView);

        ViewDescriptor orthographicView = perspectiveView;
        orthographicView.projection = ProjectionKind::orthographic;
        orthographicView.orientation = ViewOrientation::top;
        orthographicView.orthographic.vertical_size = 24.0f;
        const auto orthographicResolved = resolve(orthographicView);

        bool axesPassed = true;
        for (const auto orientation : {
                ViewOrientation::front,
                ViewOrientation::back,
                ViewOrientation::left,
                ViewOrientation::right,
                ViewOrientation::top,
                ViewOrientation::bottom})
        {
            orthographicView.orientation = orientation;
            const auto axis = resolve(orthographicView);
            axesPassed = axesPassed
                && axis.valid
                && std::abs(render_math::dot(axis.forward, axis.up)) < 1.0e-5f
                && std::abs(render_math::dot(axis.forward, axis.right)) < 1.0e-5f;
        }

        ViewDescriptor invalidIdentity = perspectiveView;
        invalidIdentity.handle = {};
        ViewDescriptor invalidProjection = perspectiveView;
        invalidProjection.perspective.near_plane = 10.0f;
        invalidProjection.perspective.far_plane = 1.0f;

        ViewDescriptor clipped = perspectiveView;
        bool clipBounded = true;
        for (std::size_t index = 0; index < clipped.clip_planes.size(); ++index)
        {
            clipBounded = clipBounded && add_clip_plane(
                clipped,
                {{0.0f, 0.0f, 1.0f}, static_cast<float>(index), true});
        }
        clipBounded = clipBounded
            && !add_clip_plane(clipped, {{0.0f, 1.0f, 0.0f}, 0.0f, true});

        return ContractChecks{
            .perspective = perspectiveResolved.valid
                && perspectiveResolved.descriptor.projection == ProjectionKind::perspective,
            .orthographic = orthographicResolved.valid
                && orthographicResolved.orthographic_vertical_size == 24.0f,
            .axes = axesPassed,
            .invalid_identity_refused = !resolve(invalidIdentity).valid,
            .invalid_projection_refused = !resolve(invalidProjection).valid,
            .clip_plane_bounded = clipBounded
        };
    }
}
