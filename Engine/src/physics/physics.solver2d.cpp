/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

module physics.solver2d;

namespace epochengine::physics
{
    namespace
    {
        constexpr std::uint32_t hard_maximum_iterations = 64;
        constexpr std::uint32_t hard_maximum_steps_per_advance = 1u << 16;
        constexpr std::uint32_t hard_maximum_bodies = 1u << 18;
        constexpr std::uint32_t hard_maximum_queued_commands = 1u << 20;
        constexpr std::uint32_t hard_maximum_static_primitives = 1u << 18;
        constexpr std::uint32_t hard_maximum_contacts = 1u << 19;
        constexpr double maximum_body_mass = 1.0e12;
        constexpr double minimum_body_mass = 1.0e-9;
        constexpr double maximum_body_coefficient = 1.0e6;
        constexpr std::size_t no_index = (std::numeric_limits<std::size_t>::max)();

        [[nodiscard]] bool finite(double value) noexcept
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool finite(Vector2 value) noexcept
        {
            return finite(value.x) && finite(value.y);
        }

        [[nodiscard]] bool finite(const Vector3& value) noexcept
        {
            return finite(value.x) && finite(value.y) && finite(value.z);
        }

        [[nodiscard]] bool finite(const Quaternion& value) noexcept
        {
            return finite(value.x)
                && finite(value.y)
                && finite(value.z)
                && finite(value.w);
        }

        [[nodiscard]] bool valid_bounds(const Aabb2& value) noexcept
        {
            return finite(value.minimum)
                && finite(value.maximum)
                && value.minimum.x < value.maximum.x
                && value.minimum.y < value.maximum.y;
        }

        [[nodiscard]] bool valid_filter(
            const CollisionFilter2D& value) noexcept
        {
            return value.layer != 0;
        }

        [[nodiscard]] bool valid_shape(const Shape2D& value) noexcept
        {
            switch (value.kind)
            {
            case Shape2DKind::aabb:
                return finite(value.half_extents)
                    && value.half_extents.x > 0.0
                    && value.half_extents.y > 0.0
                    && value.radius == 0.0;
            case Shape2DKind::circle:
                return finite(value.radius)
                    && value.radius > 0.0
                    && value.half_extents == Vector2{};
            default:
                return false;
            }
        }

        [[nodiscard]] Vector2 shape_extents(const Shape2D& shape) noexcept
        {
            if (shape.kind == Shape2DKind::circle)
                return { shape.radius, shape.radius };
            return shape.half_extents;
        }

        [[nodiscard]] bool shape_inside_bounds(
            const Shape2D& shape,
            Vector2 position,
            const Aabb2& bounds) noexcept
        {
            const Vector2 extent = shape_extents(shape);
            return position.x - extent.x >= bounds.minimum.x
                && position.y - extent.y >= bounds.minimum.y
                && position.x + extent.x <= bounds.maximum.x
                && position.y + extent.y <= bounds.maximum.y;
        }

        [[nodiscard]] bool identity_rotation(
            const Quaternion& value) noexcept
        {
            return value.x == 0.0
                && value.y == 0.0
                && value.z == 0.0
                && value.w == 1.0;
        }

        [[nodiscard]] bool zero(const Vector3& value) noexcept
        {
            return value.x == 0.0
                && value.y == 0.0
                && value.z == 0.0;
        }

        [[nodiscard]] bool within_component_limit(
            Vector2 value,
            double limit) noexcept
        {
            return finite(value)
                && std::abs(value.x) <= limit
                && std::abs(value.y) <= limit;
        }

        [[nodiscard]] bool valid_body_descriptor(
            const Body2DDescriptor& value,
            const Solver2DConfiguration& configuration) noexcept
        {
            const BodyDescriptor& body = value.body;
            const BodyInitialState& state = value.initial_state;
            if ((body.motion != BodyMotionType::static_body
                    && body.motion != BodyMotionType::kinematic_body
                    && body.motion != BodyMotionType::dynamic_body)
                || !valid_shape(value.shape)
                || !valid_filter(value.filter)
                || !finite(body.mass_kilograms)
                || !finite(body.gravity_scale)
                || !finite(body.linear_damping)
                || !finite(body.angular_damping)
                || body.linear_damping < 0.0
                || body.angular_damping < 0.0
                || body.linear_damping > maximum_body_coefficient
                || body.angular_damping > maximum_body_coefficient
                || std::abs(body.gravity_scale) > maximum_body_coefficient
                || !finite(state.transform.position)
                || !finite(state.transform.orientation)
                || !identity_rotation(state.transform.orientation)
                || !finite(state.linear_velocity)
                || !finite(state.angular_velocity)
                || state.linear_velocity.z != 0.0
                || !zero(state.angular_velocity)
                || !within_component_limit(
                    { state.linear_velocity.x, state.linear_velocity.y },
                    configuration.maximum_linear_speed)
                || !shape_inside_bounds(
                    value.shape,
                    { state.transform.position.x, state.transform.position.y },
                    configuration.world_bounds))
            {
                return false;
            }

            if (body.motion == BodyMotionType::dynamic_body)
            {
                return body.mass_kilograms >= minimum_body_mass
                    && body.mass_kilograms <= maximum_body_mass;
            }

            return body.mass_kilograms == 0.0
                && (body.motion != BodyMotionType::static_body
                    || zero(state.linear_velocity));
        }

        [[nodiscard]] bool filters_allow(
            const CollisionFilter2D& first,
            const CollisionFilter2D& second) noexcept
        {
            return (first.mask & second.layer) != 0
                && (second.mask & first.layer) != 0;
        }

        [[nodiscard]] bool target_less(
            const ContactTarget2D& left,
            const ContactTarget2D& right) noexcept
        {
            if (left.kind != right.kind)
            {
                return static_cast<std::uint8_t>(left.kind)
                    < static_cast<std::uint8_t>(right.kind);
            }
            if (left.kind == ContactTarget2DKind::body)
            {
                if (left.body.index != right.body.index)
                    return left.body.index < right.body.index;
                return left.body.generation < right.body.generation;
            }
            return left.static_primitive_id < right.static_primitive_id;
        }

        [[nodiscard]] bool contact_key_less(
            const ContactRecord2D& left,
            const ContactRecord2D& right) noexcept
        {
            if (left.first != right.first)
                return target_less(left.first, right.first);
            return target_less(left.second, right.second);
        }

        [[nodiscard]] bool same_contact_key(
            const ContactRecord2D& left,
            const ContactRecord2D& right) noexcept
        {
            return left.first == right.first && left.second == right.second;
        }

        [[nodiscard]] ContactTarget2D body_target(BodyHandle body) noexcept
        {
            return {
                .kind = ContactTarget2DKind::body,
                .body = body
            };
        }

        [[nodiscard]] ContactTarget2D map_target(
            std::uint64_t stableId) noexcept
        {
            return {
                .kind = ContactTarget2DKind::static_map,
                .static_primitive_id = stableId
            };
        }

        [[nodiscard]] Vector2 add(Vector2 left, Vector2 right) noexcept
        {
            return { left.x + right.x, left.y + right.y };
        }

        [[nodiscard]] Vector2 subtract(Vector2 left, Vector2 right) noexcept
        {
            return { left.x - right.x, left.y - right.y };
        }

        [[nodiscard]] Vector2 multiply(Vector2 value, double scalar) noexcept
        {
            return { value.x * scalar, value.y * scalar };
        }

        [[nodiscard]] double dot(Vector2 left, Vector2 right) noexcept
        {
            return left.x * right.x + left.y * right.y;
        }

        [[nodiscard]] Aabb2 shape_bounds(
            const Shape2D& shape,
            Vector2 position) noexcept
        {
            const Vector2 extent = shape_extents(shape);
            return {
                .minimum = {
                    position.x - extent.x,
                    position.y - extent.y
                },
                .maximum = {
                    position.x + extent.x,
                    position.y + extent.y
                }
            };
        }

        [[nodiscard]] Shape2D map_shape(
            const StaticAabbPrimitive2D& primitive) noexcept
        {
            return Shape2D::box({
                (primitive.bounds.maximum.x - primitive.bounds.minimum.x) * 0.5,
                (primitive.bounds.maximum.y - primitive.bounds.minimum.y) * 0.5
            });
        }

        [[nodiscard]] Vector2 map_center(
            const StaticAabbPrimitive2D& primitive) noexcept
        {
            return {
                (primitive.bounds.minimum.x + primitive.bounds.maximum.x) * 0.5,
                (primitive.bounds.minimum.y + primitive.bounds.maximum.y) * 0.5
            };
        }

        struct Manifold final
        {
            Vector2 normal{};
            Vector2 point{};
            double penetration{};
        };

        [[nodiscard]] bool collide_aabb_aabb(
            Vector2 firstPosition,
            Vector2 firstHalf,
            Vector2 secondPosition,
            Vector2 secondHalf,
            Manifold& result) noexcept
        {
            const double deltaX = secondPosition.x - firstPosition.x;
            const double deltaY = secondPosition.y - firstPosition.y;
            const double overlapX = firstHalf.x + secondHalf.x
                - std::abs(deltaX);
            const double overlapY = firstHalf.y + secondHalf.y
                - std::abs(deltaY);
            if (!(overlapX > 0.0) || !(overlapY > 0.0))
                return false;

            if (overlapX <= overlapY)
            {
                result.normal = { deltaX < 0.0 ? -1.0 : 1.0, 0.0 };
                result.penetration = overlapX;
                const double firstFace = firstPosition.x
                    + result.normal.x * firstHalf.x;
                const double secondFace = secondPosition.x
                    - result.normal.x * secondHalf.x;
                result.point = {
                    (firstFace + secondFace) * 0.5,
                    std::clamp(
                        (firstPosition.y + secondPosition.y) * 0.5,
                        (std::max)(
                            firstPosition.y - firstHalf.y,
                            secondPosition.y - secondHalf.y),
                        (std::min)(
                            firstPosition.y + firstHalf.y,
                            secondPosition.y + secondHalf.y))
                };
            }
            else
            {
                result.normal = { 0.0, deltaY < 0.0 ? -1.0 : 1.0 };
                result.penetration = overlapY;
                const double firstFace = firstPosition.y
                    + result.normal.y * firstHalf.y;
                const double secondFace = secondPosition.y
                    - result.normal.y * secondHalf.y;
                result.point = {
                    std::clamp(
                        (firstPosition.x + secondPosition.x) * 0.5,
                        (std::max)(
                            firstPosition.x - firstHalf.x,
                            secondPosition.x - secondHalf.x),
                        (std::min)(
                            firstPosition.x + firstHalf.x,
                            secondPosition.x + secondHalf.x)),
                    (firstFace + secondFace) * 0.5
                };
            }
            return finite(result.normal)
                && finite(result.point)
                && finite(result.penetration);
        }

        [[nodiscard]] bool collide_circle_circle(
            Vector2 firstPosition,
            double firstRadius,
            Vector2 secondPosition,
            double secondRadius,
            Manifold& result) noexcept
        {
            const Vector2 delta = subtract(secondPosition, firstPosition);
            const double distanceSquared = dot(delta, delta);
            const double radius = firstRadius + secondRadius;
            if (!finite(distanceSquared)
                || distanceSquared >= radius * radius)
            {
                return false;
            }

            if (distanceSquared > 0.0)
            {
                const double distance = std::sqrt(distanceSquared);
                result.normal = multiply(delta, 1.0 / distance);
                result.penetration = radius - distance;
            }
            else
            {
                result.normal = { 1.0, 0.0 };
                result.penetration = radius;
            }
            result.point = add(
                firstPosition,
                multiply(result.normal, firstRadius - result.penetration * 0.5));
            return finite(result.point) && finite(result.penetration);
        }

        [[nodiscard]] bool collide_aabb_circle(
            Vector2 boxPosition,
            Vector2 halfExtents,
            Vector2 circlePosition,
            double radius,
            Manifold& result) noexcept
        {
            const Aabb2 bounds{
                .minimum = subtract(boxPosition, halfExtents),
                .maximum = add(boxPosition, halfExtents)
            };
            const Vector2 closest{
                std::clamp(circlePosition.x, bounds.minimum.x, bounds.maximum.x),
                std::clamp(circlePosition.y, bounds.minimum.y, bounds.maximum.y)
            };
            const Vector2 delta = subtract(circlePosition, closest);
            const double distanceSquared = dot(delta, delta);
            if (!finite(distanceSquared) || distanceSquared >= radius * radius)
                return false;

            if (distanceSquared > 0.0)
            {
                const double distance = std::sqrt(distanceSquared);
                result.normal = multiply(delta, 1.0 / distance);
                result.point = closest;
                result.penetration = radius - distance;
                return finite(result.penetration);
            }

            const double left = circlePosition.x - bounds.minimum.x;
            const double right = bounds.maximum.x - circlePosition.x;
            const double top = circlePosition.y - bounds.minimum.y;
            const double bottom = bounds.maximum.y - circlePosition.y;
            double nearest = left;
            result.normal = { -1.0, 0.0 };
            result.point = { bounds.minimum.x, circlePosition.y };
            if (right < nearest)
            {
                nearest = right;
                result.normal = { 1.0, 0.0 };
                result.point = { bounds.maximum.x, circlePosition.y };
            }
            if (top < nearest)
            {
                nearest = top;
                result.normal = { 0.0, -1.0 };
                result.point = { circlePosition.x, bounds.minimum.y };
            }
            if (bottom < nearest)
            {
                nearest = bottom;
                result.normal = { 0.0, 1.0 };
                result.point = { circlePosition.x, bounds.maximum.y };
            }
            result.penetration = radius + nearest;
            return finite(result.penetration);
        }

        [[nodiscard]] bool collide(
            const Shape2D& firstShape,
            Vector2 firstPosition,
            const Shape2D& secondShape,
            Vector2 secondPosition,
            Manifold& result) noexcept
        {
            if (firstShape.kind == Shape2DKind::aabb
                && secondShape.kind == Shape2DKind::aabb)
            {
                return collide_aabb_aabb(
                    firstPosition,
                    firstShape.half_extents,
                    secondPosition,
                    secondShape.half_extents,
                    result);
            }
            if (firstShape.kind == Shape2DKind::circle
                && secondShape.kind == Shape2DKind::circle)
            {
                return collide_circle_circle(
                    firstPosition,
                    firstShape.radius,
                    secondPosition,
                    secondShape.radius,
                    result);
            }
            if (firstShape.kind == Shape2DKind::aabb)
            {
                return collide_aabb_circle(
                    firstPosition,
                    firstShape.half_extents,
                    secondPosition,
                    secondShape.radius,
                    result);
            }

            const bool collided = collide_aabb_circle(
                secondPosition,
                secondShape.half_extents,
                firstPosition,
                firstShape.radius,
                result);
            if (collided)
            {
                result.normal.x = -result.normal.x;
                result.normal.y = -result.normal.y;
            }
            return collided;
        }

        [[nodiscard]] bool horizontal_overlap(
            Vector2 bodyPosition,
            Vector2 bodyExtent,
            const Aabb2& bounds) noexcept
        {
            return bodyPosition.x + bodyExtent.x > bounds.minimum.x
                && bodyPosition.x - bodyExtent.x < bounds.maximum.x;
        }

        [[nodiscard]] double slope_surface_y(
            const Aabb2& bounds,
            StaticPrimitive2DKind kind,
            double x) noexcept
        {
            const double width = bounds.maximum.x - bounds.minimum.x;
            const double height = bounds.maximum.y - bounds.minimum.y;
            const double t = std::clamp(
                (x - bounds.minimum.x) / width,
                0.0,
                1.0);
            return kind == StaticPrimitive2DKind::slope_up_right
                ? bounds.maximum.y - height * t
                : bounds.minimum.y + height * t;
        }

        [[nodiscard]] Vector2 slope_solid_normal(
            const Aabb2& bounds,
            StaticPrimitive2DKind kind) noexcept
        {
            const double width = bounds.maximum.x - bounds.minimum.x;
            const double height = bounds.maximum.y - bounds.minimum.y;
            Vector2 normal{
                kind == StaticPrimitive2DKind::slope_up_right
                    ? height / width
                    : -height / width,
                1.0
            };
            const double length = std::sqrt(dot(normal, normal));
            return multiply(normal, 1.0 / length);
        }

        [[nodiscard]] bool collide_static_primitive(
            const Shape2D& bodyShape,
            Vector2 bodyPosition,
            Vector2 previousPosition,
            Vector2 velocity,
            const StaticAabbPrimitive2D& primitive,
            double contactSlop,
            bool enforceApproach,
            Manifold& result) noexcept
        {
            if (primitive.kind == StaticPrimitive2DKind::solid_box)
            {
                return collide(
                    bodyShape,
                    bodyPosition,
                    map_shape(primitive),
                    map_center(primitive),
                    result);
            }

            const Vector2 extent = shape_extents(bodyShape);
            if (!horizontal_overlap(bodyPosition, extent, primitive.bounds))
                return false;

            if (primitive.kind == StaticPrimitive2DKind::one_way_up)
            {
                const double surfaceY = primitive.bounds.minimum.y;
                const double bodyBottom = bodyPosition.y + extent.y;
                const double previousBottom = previousPosition.y + extent.y;
                if (bodyBottom <= surfaceY
                    || bodyPosition.y - extent.y >= primitive.bounds.maximum.y)
                {
                    return false;
                }
                const double tolerance = (std::max)(1.0e-7, contactSlop * 4.0);
                if (enforceApproach
                    && (velocity.y < -tolerance
                        || previousBottom > surfaceY + tolerance))
                {
                    return false;
                }
                result.normal = { 0.0, 1.0 };
                result.point = {
                    std::clamp(
                        bodyPosition.x,
                        primitive.bounds.minimum.x,
                        primitive.bounds.maximum.x),
                    surfaceY
                };
                result.penetration = bodyBottom - surfaceY;
                return finite(result.point)
                    && finite(result.penetration)
                    && result.penetration > 0.0;
            }

            const double sampleX = std::clamp(
                bodyPosition.x,
                primitive.bounds.minimum.x,
                primitive.bounds.maximum.x);
            const double surfaceY = slope_surface_y(
                primitive.bounds,
                primitive.kind,
                sampleX);
            const double bodyBottom = bodyPosition.y + extent.y;
            if (bodyBottom <= surfaceY
                || bodyPosition.y - extent.y >= primitive.bounds.maximum.y)
            {
                return false;
            }

            const double previousX = std::clamp(
                previousPosition.x,
                primitive.bounds.minimum.x,
                primitive.bounds.maximum.x);
            const double previousSurfaceY = slope_surface_y(
                primitive.bounds,
                primitive.kind,
                previousX);
            const double tolerance = (std::max)(1.0e-7, contactSlop * 4.0);
            if (enforceApproach
                && previousPosition.y - extent.y
                    >= previousSurfaceY + tolerance)
            {
                return false;
            }

            result.normal = slope_solid_normal(
                primitive.bounds,
                primitive.kind);
            result.point = { sampleX, surfaceY };
            result.penetration =
                (bodyBottom - surfaceY) * result.normal.y;
            return finite(result.normal)
                && finite(result.point)
                && finite(result.penetration)
                && result.penetration > 0.0;
        }

        [[nodiscard]] Solver2DCode manager_code(ResultCode code) noexcept
        {
            switch (code)
            {
            case ResultCode::success:
                return Solver2DCode::success;
            case ResultCode::invalid_handle:
                return Solver2DCode::invalid_handle;
            case ResultCode::capacity_exceeded:
            case ResultCode::queue_full:
            case ResultCode::sequence_exhausted:
                return Solver2DCode::capacity_exceeded;
            case ResultCode::invalid_temporal_input:
            case ResultCode::temporal_mismatch:
            case ResultCode::temporal_regression:
            case ResultCode::unsupported_direction:
            case ResultCode::step_limit_exceeded:
                return Solver2DCode::invalid_temporal_input;
            case ResultCode::invalid_configuration:
                return Solver2DCode::invalid_configuration;
            case ResultCode::invalid_descriptor:
            case ResultCode::invalid_command:
            case ResultCode::unsupported_body_operation:
                return Solver2DCode::invalid_descriptor;
            case ResultCode::invalid_snapshot:
                return Solver2DCode::invalid_snapshot;
            default:
                return Solver2DCode::manager_rejected;
            }
        }

        struct WorkingBody final
        {
            BodyHandle handle{};
            BodyDescriptor descriptor{};
            BodyState state{};
            Shape2D shape{};
            CollisionFilter2D filter{};
            Vector2 previous_position{};
        };

        struct ContactCandidate final
        {
            ContactTarget2D first{};
            ContactTarget2D second{};
            std::size_t first_body{ no_index };
            std::size_t second_body{ no_index };
            std::size_t map_primitive{ no_index };
            Manifold manifold{};
        };

        [[nodiscard]] bool candidate_less(
            const ContactCandidate& left,
            const ContactCandidate& right) noexcept
        {
            if (left.first != right.first)
                return target_less(left.first, right.first);
            return target_less(left.second, right.second);
        }

        [[nodiscard]] double inverse_mass(const WorkingBody& body) noexcept
        {
            if (body.descriptor.motion != BodyMotionType::dynamic_body)
                return 0.0;
            return 1.0 / body.descriptor.mass_kilograms;
        }

        [[nodiscard]] Vector2 body_position(const WorkingBody& body) noexcept
        {
            return {
                body.state.transform.position.x,
                body.state.transform.position.y
            };
        }

        [[nodiscard]] Vector2 body_velocity(const WorkingBody& body) noexcept
        {
            return {
                body.state.linear_velocity.x,
                body.state.linear_velocity.y
            };
        }

        void set_body_position(WorkingBody& body, Vector2 value) noexcept
        {
            body.state.transform.position.x = value.x;
            body.state.transform.position.y = value.y;
        }

        void set_body_velocity(WorkingBody& body, Vector2 value) noexcept
        {
            body.state.linear_velocity.x = value.x;
            body.state.linear_velocity.y = value.y;
            body.state.linear_velocity.z = 0.0;
        }

        [[nodiscard]] bool clamp_to_world(
            WorkingBody& body,
            const Solver2DConfiguration& configuration,
            Solver2DMetrics& metrics) noexcept
        {
            Vector2 position = body_position(body);
            Vector2 velocity = body_velocity(body);
            const Vector2 extent = shape_extents(body.shape);
            const Vector2 minimum{
                configuration.world_bounds.minimum.x + extent.x,
                configuration.world_bounds.minimum.y + extent.y
            };
            const Vector2 maximum{
                configuration.world_bounds.maximum.x - extent.x,
                configuration.world_bounds.maximum.y - extent.y
            };
            if (minimum.x > maximum.x || minimum.y > maximum.y)
                return false;

            bool clamped = false;
            if (position.x < minimum.x)
            {
                position.x = minimum.x;
                if (velocity.x < 0.0)
                    velocity.x = 0.0;
                clamped = true;
            }
            else if (position.x > maximum.x)
            {
                position.x = maximum.x;
                if (velocity.x > 0.0)
                    velocity.x = 0.0;
                clamped = true;
            }
            if (position.y < minimum.y)
            {
                position.y = minimum.y;
                if (velocity.y < 0.0)
                    velocity.y = 0.0;
                clamped = true;
            }
            else if (position.y > maximum.y)
            {
                position.y = maximum.y;
                if (velocity.y > 0.0)
                    velocity.y = 0.0;
                clamped = true;
            }
            if (!finite(position) || !finite(velocity))
                return false;

            if (clamped)
                ++metrics.world_bounds_clamps;
            set_body_position(body, position);
            set_body_velocity(body, velocity);
            return true;
        }

        [[nodiscard]] bool valid_static_primitive(
            const StaticAabbPrimitive2D& primitive,
            const Solver2DConfiguration& configuration) noexcept
        {
            const bool validKind =
                primitive.kind == StaticPrimitive2DKind::solid_box
                || primitive.kind == StaticPrimitive2DKind::one_way_up
                || primitive.kind == StaticPrimitive2DKind::slope_up_right
                || primitive.kind == StaticPrimitive2DKind::slope_down_right;
            return primitive.stable_id != 0
                && valid_bounds(primitive.bounds)
                && valid_filter(primitive.filter)
                && validKind
                && primitive.bounds.minimum.x
                    >= configuration.world_bounds.minimum.x
                && primitive.bounds.minimum.y
                    >= configuration.world_bounds.minimum.y
                && primitive.bounds.maximum.x
                    <= configuration.world_bounds.maximum.x
                && primitive.bounds.maximum.y
                    <= configuration.world_bounds.maximum.y;
        }
        constexpr double snapshot_contact_tolerance = 1.0e-9;

        [[nodiscard]] bool snapshot_contact_equal(
            double left,
            double right) noexcept
        {
            const double scale = (std::max)({
                1.0,
                std::abs(left),
                std::abs(right)
            });
            return std::abs(left - right)
                <= snapshot_contact_tolerance * scale;
        }

        [[nodiscard]] bool snapshot_contact_equal(
            Vector2 left,
            Vector2 right) noexcept
        {
            return snapshot_contact_equal(left.x, right.x)
                && snapshot_contact_equal(left.y, right.y);
        }

        struct SnapshotContactGeometry final
        {
            Shape2D shape{};
            Vector2 position{};
            CollisionFilter2D filter{};
            BodyMotionType motion{ BodyMotionType::static_body };
            bool enabled{};
            std::optional<StaticAabbPrimitive2D> static_primitive{};
        };

        [[nodiscard]] bool resolve_snapshot_contact_geometry(
            const ContactTarget2D& target,
            const Solver2DStateSnapshot& state,
            SnapshotContactGeometry& result) noexcept
        {
            if (target.kind == ContactTarget2DKind::body)
            {
                if (!target.body
                    || target.body.index >= state.manager.body_slots.size()
                    || target.body.index >= state.collider_slots.size()
                    || target.static_primitive_id != 0)
                {
                    return false;
                }

                const BodySlotSnapshot& body =
                    state.manager.body_slots[target.body.index];
                const ColliderSlot2DSnapshot& collider =
                    state.collider_slots[target.body.index];
                if (!body.occupied
                    || !collider.occupied
                    || body.generation != target.body.generation
                    || collider.generation != target.body.generation)
                {
                    return false;
                }

                result = {
                    .shape = collider.shape,
                    .position = {
                        body.state.transform.position.x,
                        body.state.transform.position.y
                    },
                    .filter = collider.filter,
                    .motion = body.descriptor.motion,
                    .enabled = body.state.enabled
                };
                return true;
            }

            if (target.kind != ContactTarget2DKind::static_map
                || target.body
                || target.static_primitive_id == 0)
            {
                return false;
            }

            const auto found = std::lower_bound(
                state.static_primitives.begin(),
                state.static_primitives.end(),
                target.static_primitive_id,
                [](const auto& primitive, std::uint64_t id)
                {
                    return primitive.stable_id < id;
                });
            if (found == state.static_primitives.end()
                || found->stable_id != target.static_primitive_id)
            {
                return false;
            }

            result = {
                .shape = map_shape(*found),
                .position = map_center(*found),
                .filter = found->filter,
                .motion = BodyMotionType::static_body,
                .enabled = true,
                .static_primitive = *found
            };
            return true;
        }

        [[nodiscard]] bool valid_snapshot_contact_geometry(
            const ContactRecord2D& contact,
            const Solver2DStateSnapshot& state) noexcept
        {
            SnapshotContactGeometry first{};
            SnapshotContactGeometry second{};
            if (!resolve_snapshot_contact_geometry(contact.first, state, first)
                || !resolve_snapshot_contact_geometry(
                    contact.second,
                    state,
                    second)
                || !first.enabled
                || !second.enabled
                || !filters_allow(first.filter, second.filter)
                || (first.motion == BodyMotionType::static_body
                    && second.motion == BodyMotionType::static_body))
            {
                return false;
            }

            const double normalLengthSquared = dot(
                contact.normal,
                contact.normal);
            if (!finite(normalLengthSquared)
                || !snapshot_contact_equal(normalLengthSquared, 1.0))
            {
                return false;
            }

            Manifold expected{};
            const bool expectedCollision = second.static_primitive
                ? collide_static_primitive(
                    first.shape,
                    first.position,
                    first.position,
                    {},
                    *second.static_primitive,
                    0.0,
                    false,
                    expected)
                : collide(
                    first.shape,
                    first.position,
                    second.shape,
                    second.position,
                    expected);
            return expectedCollision
                && snapshot_contact_equal(contact.normal, expected.normal)
                && snapshot_contact_equal(contact.point, expected.point)
                && snapshot_contact_equal(
                    contact.penetration,
                    expected.penetration);
        }
    }

    struct Solver2D::Impl final
    {
        explicit Impl(Solver2DConfiguration value)
            : configuration(std::move(value))
            , manager(configuration.fixed_step)
            , gravity(configuration.initial_gravity)
        {
            reset_point = capture_state();
        }

        [[nodiscard]] Solver2DStateSnapshot capture_state() const
        {
            return {
                .manager = manager.snapshot(),
                .collider_slots = collider_slots,
                .static_primitives = static_primitives,
                .contacts = contacts,
                .gravity = gravity,
                .paused = paused,
                .next_contact_id = next_contact_id,
                .metrics = metrics
            };
        }

        [[nodiscard]] bool restore_state(
            const Solver2DStateSnapshot& state)
        {
            if (manager.restore(state.manager) != ResultCode::success)
                return false;
            collider_slots = state.collider_slots;
            static_primitives = state.static_primitives;
            contacts = state.contacts;
            gravity = state.gravity;
            paused = state.paused;
            next_contact_id = state.next_contact_id;
            metrics = state.metrics;
            return true;
        }

        [[nodiscard]] bool synchronize_colliders(
            const PhysicsManagerSnapshot& managerState)
        {
            if (collider_slots.size() != managerState.body_slots.size())
                return false;

            for (std::size_t index = 0;
                index < managerState.body_slots.size();
                ++index)
            {
                const BodySlotSnapshot& bodySlot = managerState.body_slots[index];
                ColliderSlot2DSnapshot& collider = collider_slots[index];
                if (!bodySlot.occupied)
                {
                    collider = {
                        .generation = bodySlot.generation
                    };
                    continue;
                }
                if (!collider.occupied
                    || collider.generation != bodySlot.generation)
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] Solver2DCode collect_contacts(
            const std::vector<WorkingBody>& bodies,
            std::vector<ContactCandidate>& result,
            Solver2DMetrics& workingMetrics) const
        {
            result.clear();
            for (std::size_t first = 0; first < bodies.size(); ++first)
            {
                const WorkingBody& firstBody = bodies[first];
                if (!firstBody.state.enabled)
                    continue;

                for (std::size_t second = first + 1;
                    second < bodies.size();
                    ++second)
                {
                    const WorkingBody& secondBody = bodies[second];
                    if (!secondBody.state.enabled
                        || !filters_allow(firstBody.filter, secondBody.filter)
                        || (firstBody.descriptor.motion
                                == BodyMotionType::static_body
                            && secondBody.descriptor.motion
                                == BodyMotionType::static_body))
                    {
                        continue;
                    }

                    ++workingMetrics.broadphase_pairs_tested;
                    Manifold manifold{};
                    if (!collide(
                            firstBody.shape,
                            body_position(firstBody),
                            secondBody.shape,
                            body_position(secondBody),
                            manifold))
                    {
                        continue;
                    }
                    if (result.size() >= configuration.maximum_contacts)
                        return Solver2DCode::contact_capacity_exceeded;
                    result.push_back({
                        .first = body_target(firstBody.handle),
                        .second = body_target(secondBody.handle),
                        .first_body = first,
                        .second_body = second,
                        .manifold = manifold
                    });
                }

                if (firstBody.descriptor.motion == BodyMotionType::static_body)
                    continue;
                for (std::size_t mapIndex = 0;
                    mapIndex < static_primitives.size();
                    ++mapIndex)
                {
                    const StaticAabbPrimitive2D& primitive =
                        static_primitives[mapIndex];
                    if (!filters_allow(firstBody.filter, primitive.filter))
                        continue;

                    ++workingMetrics.broadphase_pairs_tested;
                    Manifold manifold{};
                    if (!collide_static_primitive(
                            firstBody.shape,
                            body_position(firstBody),
                            firstBody.previous_position,
                            body_velocity(firstBody),
                            primitive,
                            configuration.contact_slop,
                            true,
                            manifold))
                    {
                        continue;
                    }
                    if (result.size() >= configuration.maximum_contacts)
                        return Solver2DCode::contact_capacity_exceeded;
                    result.push_back({
                        .first = body_target(firstBody.handle),
                        .second = map_target(primitive.stable_id),
                        .first_body = first,
                        .map_primitive = mapIndex,
                        .manifold = manifold
                    });
                }
            }

            std::sort(result.begin(), result.end(), candidate_less);
            workingMetrics.contact_manifolds_generated += result.size();
            return Solver2DCode::success;
        }

        [[nodiscard]] bool resolve_contact(
            ContactCandidate& contact,
            std::vector<WorkingBody>& bodies,
            Solver2DMetrics& workingMetrics) const noexcept
        {
            WorkingBody& first = bodies[contact.first_body];
            WorkingBody* second = contact.second_body == no_index
                ? nullptr
                : &bodies[contact.second_body];
            const double firstInverseMass = inverse_mass(first);
            const double secondInverseMass = second
                ? inverse_mass(*second)
                : 0.0;
            const double inverseMassSum = firstInverseMass + secondInverseMass;
            if (inverseMassSum == 0.0)
                return true;

            const double correctionMagnitude = (std::max)(
                contact.manifold.penetration - configuration.contact_slop,
                0.0) / inverseMassSum;
            if (correctionMagnitude > 0.0)
            {
                if (firstInverseMass > 0.0)
                {
                    set_body_position(first, subtract(
                        body_position(first),
                        multiply(
                            contact.manifold.normal,
                            correctionMagnitude * firstInverseMass)));
                }
                if (second && secondInverseMass > 0.0)
                {
                    set_body_position(*second, add(
                        body_position(*second),
                        multiply(
                            contact.manifold.normal,
                            correctionMagnitude * secondInverseMass)));
                }
                ++workingMetrics.position_corrections;
            }

            const Vector2 firstVelocity = body_velocity(first);
            const Vector2 secondVelocity = second
                ? body_velocity(*second)
                : Vector2{};
            const double relativeNormalVelocity = dot(
                subtract(secondVelocity, firstVelocity),
                contact.manifold.normal);
            if (relativeNormalVelocity < 0.0)
            {
                const double impulseMagnitude =
                    -relativeNormalVelocity / inverseMassSum;
                if (!finite(impulseMagnitude))
                    return false;
                if (firstInverseMass > 0.0)
                {
                    set_body_velocity(first, subtract(
                        firstVelocity,
                        multiply(
                            contact.manifold.normal,
                            impulseMagnitude * firstInverseMass)));
                }
                if (second && secondInverseMass > 0.0)
                {
                    set_body_velocity(*second, add(
                        secondVelocity,
                        multiply(
                            contact.manifold.normal,
                            impulseMagnitude * secondInverseMass)));
                }
                ++workingMetrics.velocity_corrections;
            }
            return finite(body_position(first))
                && finite(body_velocity(first))
                && (!second
                    || (finite(body_position(*second))
                        && finite(body_velocity(*second))));
        }

        [[nodiscard]] Solver2DCode rebuild_contact_records(
            const std::vector<ContactCandidate>& candidates,
            std::uint64_t stepIndex,
            std::vector<ContactRecord2D>& result,
            std::uint64_t& nextId) const
        {
            result.clear();
            result.reserve(candidates.size());
            for (const ContactCandidate& candidate : candidates)
            {
                ContactRecord2D key{
                    .first = candidate.first,
                    .second = candidate.second
                };
                const auto previous = std::lower_bound(
                    contacts.begin(),
                    contacts.end(),
                    key,
                    contact_key_less);

                ContactId2D id{};
                std::uint64_t beganStep = stepIndex;
                if (previous != contacts.end()
                    && same_contact_key(*previous, key))
                {
                    id = previous->id;
                    beganStep = previous->began_step;
                }
                else
                {
                    if (nextId == 0
                        || nextId == (std::numeric_limits<std::uint64_t>::max)())
                    {
                        return Solver2DCode::capacity_exceeded;
                    }
                    id.value = nextId++;
                }

                result.push_back({
                    .id = id,
                    .first = candidate.first,
                    .second = candidate.second,
                    .normal = candidate.manifold.normal,
                    .point = candidate.manifold.point,
                    .penetration = candidate.manifold.penetration,
                    .began_step = beganStep,
                    .last_step = stepIndex
                });
            }
            return Solver2DCode::success;
        }

        [[nodiscard]] Solver2DCode solve_step()
        {
            PhysicsManagerSnapshot managerState = manager.snapshot();
            if (!synchronize_colliders(managerState))
                return Solver2DCode::invalid_snapshot;

            std::vector<WorkingBody> workingBodies{};
            workingBodies.reserve(managerState.metrics.active_bodies);
            for (std::size_t index = 0;
                index < managerState.body_slots.size();
                ++index)
            {
                const BodySlotSnapshot& bodySlot = managerState.body_slots[index];
                if (!bodySlot.occupied)
                    continue;
                const ColliderSlot2DSnapshot& collider = collider_slots[index];
                workingBodies.push_back({
                    .handle = {
                        .index = static_cast<std::uint32_t>(index),
                        .generation = bodySlot.generation
                    },
                    .descriptor = bodySlot.descriptor,
                    .state = bodySlot.state,
                    .shape = collider.shape,
                    .filter = collider.filter
                });
            }

            Solver2DMetrics workingMetrics = metrics;
            const double deltaSeconds =
                static_cast<double>(configuration.fixed_step.fixed_step_ticks)
                * configuration.seconds_per_tick;
            for (WorkingBody& body : workingBodies)
            {
                body.previous_position = body_position(body);
                if (!body.state.enabled
                    || body.descriptor.motion == BodyMotionType::static_body)
                {
                    continue;
                }

                Vector2 velocity = body_velocity(body);
                if (body.descriptor.motion == BodyMotionType::dynamic_body)
                {
                    velocity = add(
                        velocity,
                        multiply(
                            gravity,
                            body.descriptor.gravity_scale * deltaSeconds));
                    const double damping = (std::max)(
                        0.0,
                        1.0 - body.descriptor.linear_damping * deltaSeconds);
                    velocity = multiply(velocity, damping);
                }
                if (!within_component_limit(
                        velocity,
                        configuration.maximum_linear_speed))
                {
                    return Solver2DCode::numerical_failure;
                }
                set_body_velocity(body, velocity);
                set_body_position(body, add(
                    body_position(body),
                    multiply(velocity, deltaSeconds)));
                if (!clamp_to_world(body, configuration, workingMetrics))
                    return Solver2DCode::numerical_failure;
            }

            std::vector<ContactCandidate> candidates{};
            candidates.reserve((std::min)(
                static_cast<std::size_t>(configuration.maximum_contacts),
                workingBodies.size() + static_primitives.size()));
            for (std::uint32_t iteration = 0;
                iteration < configuration.solver_iterations;
                ++iteration)
            {
                const Solver2DCode collectCode = collect_contacts(
                    workingBodies,
                    candidates,
                    workingMetrics);
                if (collectCode != Solver2DCode::success)
                    return collectCode;

                bool corrected = false;
                for (ContactCandidate& candidate : candidates)
                {
                    const std::uint64_t correctionCount =
                        workingMetrics.position_corrections
                        + workingMetrics.velocity_corrections;
                    if (!resolve_contact(
                            candidate,
                            workingBodies,
                            workingMetrics))
                    {
                        return Solver2DCode::numerical_failure;
                    }
                    corrected = corrected
                        || correctionCount
                            != workingMetrics.position_corrections
                                + workingMetrics.velocity_corrections;
                }
                for (WorkingBody& body : workingBodies)
                {
                    if (!clamp_to_world(body, configuration, workingMetrics))
                        return Solver2DCode::numerical_failure;
                }
                if (!corrected)
                    break;
            }

            const Solver2DCode finalCollectCode = collect_contacts(
                workingBodies,
                candidates,
                workingMetrics);
            if (finalCollectCode != Solver2DCode::success)
                return finalCollectCode;

            std::vector<ContactRecord2D> nextContacts{};
            std::uint64_t nextId = next_contact_id;
            const Solver2DCode contactCode = rebuild_contact_records(
                candidates,
                managerState.fixed_step_index,
                nextContacts,
                nextId);
            if (contactCode != Solver2DCode::success)
                return contactCode;

            for (const WorkingBody& body : workingBodies)
            {
                BodySlotSnapshot& slot = managerState.body_slots[body.handle.index];
                const bool changed =
                    slot.state.transform.position.x
                        != body.state.transform.position.x
                    || slot.state.transform.position.y
                        != body.state.transform.position.y
                    || slot.state.linear_velocity.x
                        != body.state.linear_velocity.x
                    || slot.state.linear_velocity.y
                        != body.state.linear_velocity.y;
                if (!changed)
                    continue;

                slot.state.transform.position.x = body.state.transform.position.x;
                slot.state.transform.position.y = body.state.transform.position.y;
                slot.state.linear_velocity.x = body.state.linear_velocity.x;
                slot.state.linear_velocity.y = body.state.linear_velocity.y;
                slot.state.linear_velocity.z = 0.0;
                ++slot.state.revision;
                slot.state.last_changed_at = managerState.current_time;
            }

            managerState.pending_commands.erase(
                std::remove_if(
                    managerState.pending_commands.begin(),
                    managerState.pending_commands.end(),
                    [&managerState](const BodyCommand& command)
                    {
                        return command.body.index
                                >= managerState.body_slots.size()
                            || !managerState.body_slots[command.body.index].occupied
                            || managerState.body_slots[command.body.index].generation
                                != command.body.generation;
                    }),
                managerState.pending_commands.end());
            managerState.metrics.queued_commands =
                static_cast<std::uint32_t>(
                    managerState.pending_commands.size());

            if (manager.restore(managerState) != ResultCode::success)
                return Solver2DCode::manager_rejected;

            contacts = std::move(nextContacts);
            next_contact_id = nextId;
            metrics = workingMetrics;
            metrics.active_bodies = managerState.metrics.active_bodies;
            metrics.static_primitives = static_cast<std::uint32_t>(
                static_primitives.size());
            metrics.active_contacts = static_cast<std::uint32_t>(
                contacts.size());
            metrics.peak_contacts = (std::max)(
                metrics.peak_contacts,
                metrics.active_contacts);
            ++metrics.fixed_steps_committed;
            return Solver2DCode::success;
        }

        Solver2DConfiguration configuration{};
        PhysicsManager manager;
        std::vector<ColliderSlot2DSnapshot> collider_slots{};
        std::vector<StaticAabbPrimitive2D> static_primitives{};
        std::vector<ContactRecord2D> contacts{};
        Vector2 gravity{};
        bool paused{};
        std::uint64_t next_contact_id{ 1 };
        Solver2DMetrics metrics{};
        Solver2DStateSnapshot reset_point{};
        mutable std::mutex mutex{};
    };

    Solver2DCode Solver2D::validate_configuration(
        const Solver2DConfiguration& configuration) noexcept
    {
        if (PhysicsManager::validate_configuration(configuration.fixed_step)
                != ResultCode::success
            || !finite(configuration.initial_gravity)
            || !valid_bounds(configuration.world_bounds)
            || !finite(configuration.seconds_per_tick)
            || configuration.seconds_per_tick <= 0.0
            || !finite(configuration.maximum_linear_speed)
            || configuration.maximum_linear_speed <= 0.0
            || !finite(configuration.maximum_acceleration)
            || configuration.maximum_acceleration <= 0.0
            || !within_component_limit(
                configuration.initial_gravity,
                configuration.maximum_acceleration)
            || !finite(configuration.contact_slop)
            || configuration.contact_slop < 0.0
            || configuration.fixed_step.maximum_steps_per_advance
                > hard_maximum_steps_per_advance
            || configuration.fixed_step.maximum_bodies
                > hard_maximum_bodies
            || configuration.fixed_step.maximum_queued_commands
                > hard_maximum_queued_commands
            || configuration.solver_iterations == 0
            || configuration.solver_iterations > hard_maximum_iterations
            || configuration.maximum_static_primitives == 0
            || configuration.maximum_static_primitives
                > hard_maximum_static_primitives
            || configuration.maximum_contacts == 0
            || configuration.maximum_contacts > hard_maximum_contacts)
        {
            return Solver2DCode::invalid_configuration;
        }

        const double deltaSeconds =
            static_cast<double>(configuration.fixed_step.fixed_step_ticks)
            * configuration.seconds_per_tick;
        const double width = configuration.world_bounds.maximum.x
            - configuration.world_bounds.minimum.x;
        const double height = configuration.world_bounds.maximum.y
            - configuration.world_bounds.minimum.y;
        if (!finite(deltaSeconds)
            || deltaSeconds <= 0.0
            || deltaSeconds > 1.0
            || !finite(width)
            || !finite(height)
            || width <= 0.0
            || height <= 0.0)
        {
            return Solver2DCode::invalid_configuration;
        }
        return Solver2DCode::success;
    }

    Solver2D::Solver2D(Solver2DConfiguration configuration)
    {
        if (validate_configuration(configuration)
            != Solver2DCode::success)
        {
            throw std::invalid_argument(
                "Solver2D received an invalid bounded configuration.");
        }
        impl_ = std::make_unique<Impl>(std::move(configuration));
    }

    Solver2D::~Solver2D() = default;
    Solver2D::Solver2D(Solver2D&&) noexcept = default;
    Solver2D& Solver2D::operator=(Solver2D&&) noexcept = default;

    Solver2DConfiguration Solver2D::configuration() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->configuration;
    }

    TemporalAddress Solver2D::current_time() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->manager.current_time();
    }

    bool Solver2D::paused() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->paused;
    }

    Vector2 Solver2D::gravity() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->gravity;
    }

    Solver2DMetrics Solver2D::metrics() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->metrics;
    }

    Solver2DCode Solver2D::set_gravity(Vector2 gravityValue)
    {
        std::scoped_lock lock{ impl_->mutex };
        if (!within_component_limit(
                gravityValue,
                impl_->configuration.maximum_acceleration))
        {
            return Solver2DCode::invalid_descriptor;
        }
        impl_->gravity = gravityValue;
        return Solver2DCode::success;
    }

    void Solver2D::set_paused(bool pausedValue) noexcept
    {
        std::scoped_lock lock{ impl_->mutex };
        impl_->paused = pausedValue;
    }

    SpawnBody2DResult Solver2D::spawn(
        const Body2DDescriptor& descriptor)
    {
        std::scoped_lock lock{ impl_->mutex };
        if (!valid_shape(descriptor.shape))
            return { .code = Solver2DCode::invalid_shape };
        if (!valid_filter(descriptor.filter))
            return { .code = Solver2DCode::invalid_filter };
        if (!valid_body_descriptor(descriptor, impl_->configuration))
            return { .code = Solver2DCode::invalid_descriptor };

        const CreateBodyResult created = impl_->manager.create_body(
            descriptor.body,
            descriptor.initial_state,
            impl_->manager.current_time());
        if (!created)
        {
            return {
                .code = manager_code(created.code),
                .manager_code = created.code
            };
        }

        const PhysicsManagerSnapshot managerState = impl_->manager.snapshot();
        if (impl_->collider_slots.size() < managerState.body_slots.size())
            impl_->collider_slots.resize(managerState.body_slots.size());
        for (std::size_t index = 0;
            index < managerState.body_slots.size();
            ++index)
        {
            if (!managerState.body_slots[index].occupied)
            {
                impl_->collider_slots[index] = {
                    .generation = managerState.body_slots[index].generation
                };
            }
        }
        impl_->collider_slots[created.body.index] = {
            .generation = created.body.generation,
            .occupied = true,
            .shape = descriptor.shape,
            .filter = descriptor.filter
        };
        impl_->metrics.active_bodies = managerState.metrics.active_bodies;
        return {
            .body = created.body
        };
    }

    bool Solver2D::contains(BodyHandle bodyHandle) const
    {
        std::scoped_lock lock{ impl_->mutex };
        if (!impl_->manager.contains(bodyHandle)
            || bodyHandle.index >= impl_->collider_slots.size())
        {
            return false;
        }
        const ColliderSlot2DSnapshot& collider =
            impl_->collider_slots[bodyHandle.index];
        return collider.occupied
            && collider.generation == bodyHandle.generation;
    }

    std::optional<Body2DSnapshot> Solver2D::body(
        BodyHandle bodyHandle) const
    {
        std::scoped_lock lock{ impl_->mutex };
        const std::optional<BodySnapshot> bodyState =
            impl_->manager.body(bodyHandle);
        if (!bodyState || bodyHandle.index >= impl_->collider_slots.size())
            return std::nullopt;
        const ColliderSlot2DSnapshot& collider =
            impl_->collider_slots[bodyHandle.index];
        if (!collider.occupied || collider.generation != bodyHandle.generation)
            return std::nullopt;
        return Body2DSnapshot{
            .body = *bodyState,
            .shape = collider.shape,
            .filter = collider.filter
        };
    }

    std::vector<Body2DSnapshot> Solver2D::bodies() const
    {
        std::scoped_lock lock{ impl_->mutex };
        const std::vector<BodySnapshot> managerBodies = impl_->manager.bodies();
        std::vector<Body2DSnapshot> result{};
        result.reserve(managerBodies.size());
        for (const BodySnapshot& bodyState : managerBodies)
        {
            if (bodyState.handle.index >= impl_->collider_slots.size())
                continue;
            const ColliderSlot2DSnapshot& collider =
                impl_->collider_slots[bodyState.handle.index];
            if (!collider.occupied
                || collider.generation != bodyState.handle.generation)
            {
                continue;
            }
            result.push_back({
                .body = bodyState,
                .shape = collider.shape,
                .filter = collider.filter
            });
        }
        return result;
    }

    EnqueueResult Solver2D::enqueue_destroy(
        BodyHandle bodyHandle,
        const TemporalAddress& executeAt)
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->manager.enqueue_destroy(bodyHandle, executeAt);
    }

    EnqueueResult Solver2D::enqueue_set_transform(
        BodyHandle bodyHandle,
        const Transform& transform,
        const TemporalAddress& executeAt)
    {
        std::scoped_lock lock{ impl_->mutex };
        if (!finite(transform.position)
            || !finite(transform.orientation)
            || !identity_rotation(transform.orientation)
            || bodyHandle.index >= impl_->collider_slots.size())
        {
            return { .code = ResultCode::invalid_command };
        }
        const ColliderSlot2DSnapshot& collider =
            impl_->collider_slots[bodyHandle.index];
        if (!collider.occupied
            || collider.generation != bodyHandle.generation)
        {
            return { .code = ResultCode::invalid_handle };
        }
        if (!shape_inside_bounds(
                collider.shape,
                { transform.position.x, transform.position.y },
                impl_->configuration.world_bounds))
        {
            return { .code = ResultCode::invalid_command };
        }
        return impl_->manager.enqueue_set_transform(
            bodyHandle,
            transform,
            executeAt);
    }

    EnqueueResult Solver2D::enqueue_set_velocity(
        BodyHandle bodyHandle,
        Vector2 velocity,
        const TemporalAddress& executeAt)
    {
        std::scoped_lock lock{ impl_->mutex };
        if (!within_component_limit(
                velocity,
                impl_->configuration.maximum_linear_speed))
        {
            return { .code = ResultCode::invalid_command };
        }
        return impl_->manager.enqueue_set_linear_velocity(
            bodyHandle,
            { velocity.x, velocity.y, 0.0 },
            executeAt);
    }

    EnqueueResult Solver2D::enqueue_set_enabled(
        BodyHandle bodyHandle,
        bool enabled,
        const TemporalAddress& executeAt)
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->manager.enqueue_set_enabled(
            bodyHandle,
            enabled,
            executeAt);
    }

    EnqueueResult Solver2D::enqueue_impulse(
        BodyHandle bodyHandle,
        Vector2 impulse,
        const TemporalAddress& executeAt)
    {
        std::scoped_lock lock{ impl_->mutex };
        if (!within_component_limit(
                impulse,
                impl_->configuration.maximum_linear_speed
                    * maximum_body_mass))
        {
            return { .code = ResultCode::invalid_command };
        }
        return impl_->manager.enqueue_apply_linear_impulse(
            bodyHandle,
            { impulse.x, impulse.y, 0.0 },
            executeAt);
    }

    Solver2DCode Solver2D::set_static_map(
        std::span<const StaticAabbPrimitive2D> primitives)
    {
        std::scoped_lock lock{ impl_->mutex };
        if (primitives.size() > impl_->configuration.maximum_static_primitives)
            return Solver2DCode::capacity_exceeded;

        std::vector<StaticAabbPrimitive2D> candidate{
            primitives.begin(), primitives.end()
        };
        for (const StaticAabbPrimitive2D& primitive : candidate)
        {
            if (!valid_static_primitive(primitive, impl_->configuration))
                return Solver2DCode::invalid_static_map;
        }
        std::sort(
            candidate.begin(),
            candidate.end(),
            [](const auto& left, const auto& right)
            {
                return left.stable_id < right.stable_id;
            });
        for (std::size_t index = 1; index < candidate.size(); ++index)
        {
            if (candidate[index - 1].stable_id == candidate[index].stable_id)
                return Solver2DCode::duplicate_static_id;
        }

        impl_->static_primitives = std::move(candidate);
        impl_->contacts.erase(
            std::remove_if(
                impl_->contacts.begin(),
                impl_->contacts.end(),
                [](const ContactRecord2D& contact)
                {
                    return contact.first.kind
                            == ContactTarget2DKind::static_map
                        || contact.second.kind
                            == ContactTarget2DKind::static_map;
                }),
            impl_->contacts.end());
        impl_->metrics.static_primitives = static_cast<std::uint32_t>(
            impl_->static_primitives.size());
        impl_->metrics.active_contacts = static_cast<std::uint32_t>(
            impl_->contacts.size());
        return Solver2DCode::success;
    }

    void Solver2D::clear_static_map() noexcept
    {
        std::scoped_lock lock{ impl_->mutex };
        impl_->static_primitives.clear();
        impl_->contacts.erase(
            std::remove_if(
                impl_->contacts.begin(),
                impl_->contacts.end(),
                [](const ContactRecord2D& contact)
                {
                    return contact.first.kind
                            == ContactTarget2DKind::static_map
                        || contact.second.kind
                            == ContactTarget2DKind::static_map;
                }),
            impl_->contacts.end());
        impl_->metrics.static_primitives = 0;
        impl_->metrics.active_contacts = static_cast<std::uint32_t>(
            impl_->contacts.size());
    }

    std::vector<StaticAabbPrimitive2D> Solver2D::static_map() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->static_primitives;
    }

    Solver2DAdvanceResult Solver2D::advance(
        const FixedStepRequest& request)
    {
        std::scoped_lock lock{ impl_->mutex };
        Solver2DAdvanceResult result{
            .current_time = impl_->manager.current_time(),
            .contacts = impl_->contacts
        };
        if (impl_->paused)
        {
            result.code = Solver2DCode::paused;
            return result;
        }

        const TemporalAddress current = impl_->manager.current_time();
        const auto& fixed = impl_->configuration.fixed_step;
        if (request.from != current
            || request.direction != TemporalDirection::forward
            || request.to.timeline != current.timeline
            || request.to.branch != current.branch
            || request.to.domain != current.domain
            || request.to.tick <= current.tick)
        {
            result.code = Solver2DCode::invalid_temporal_input;
            return result;
        }
        const std::int64_t distance = request.to.tick - current.tick;
        if (distance % fixed.fixed_step_ticks != 0)
        {
            result.code = Solver2DCode::invalid_temporal_input;
            return result;
        }
        const std::int64_t stepCount = distance / fixed.fixed_step_ticks;
        if (stepCount <= 0
            || stepCount > fixed.maximum_steps_per_advance)
        {
            result.code = Solver2DCode::invalid_temporal_input;
            return result;
        }

        const Solver2DStateSnapshot before = impl_->capture_state();
        for (std::int64_t step = 0; step < stepCount; ++step)
        {
            const TemporalAddress from = impl_->manager.current_time();
            TemporalAddress to = from;
            to.tick += fixed.fixed_step_ticks;
            const AdvanceResult managerAdvance = impl_->manager.advance({
                .from = from,
                .to = to,
                .direction = TemporalDirection::forward
            });
            if (!managerAdvance)
            {
                (void)impl_->restore_state(before);
                result = {
                    .code = manager_code(managerAdvance.code),
                    .manager_code = managerAdvance.code,
                    .current_time = before.manager.current_time,
                    .contacts = before.contacts
                };
                return result;
            }
            result.command_outcomes.insert(
                result.command_outcomes.end(),
                managerAdvance.command_outcomes.begin(),
                managerAdvance.command_outcomes.end());

            const Solver2DCode solveCode = impl_->solve_step();
            if (solveCode != Solver2DCode::success)
            {
                (void)impl_->restore_state(before);
                result = {
                    .code = solveCode,
                    .manager_code = solveCode == Solver2DCode::manager_rejected
                        ? ResultCode::invalid_snapshot
                        : ResultCode::success,
                    .current_time = before.manager.current_time,
                    .contacts = before.contacts
                };
                return result;
            }
            ++result.steps_committed;
        }

        result.current_time = impl_->manager.current_time();
        result.contacts = impl_->contacts;
        return result;
    }

    std::vector<ContactRecord2D> Solver2D::contacts() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->contacts;
    }

    void Solver2D::capture_reset_point()
    {
        std::scoped_lock lock{ impl_->mutex };
        impl_->reset_point = impl_->capture_state();
    }

    Solver2DCode Solver2D::reset()
    {
        std::scoped_lock lock{ impl_->mutex };
        return impl_->restore_state(impl_->reset_point)
            ? Solver2DCode::success
            : Solver2DCode::invalid_snapshot;
    }

    Solver2DSnapshot Solver2D::snapshot() const
    {
        std::scoped_lock lock{ impl_->mutex };
        return {
            .configuration = impl_->configuration,
            .state = impl_->capture_state(),
            .reset_point = impl_->reset_point
        };
    }

    Solver2DCode Solver2D::restore(const Solver2DSnapshot& snapshotValue)
    {
        std::scoped_lock lock{ impl_->mutex };
        if (snapshotValue.configuration != impl_->configuration)
            return Solver2DCode::invalid_configuration;

        const auto validateState = [this](
            const Solver2DStateSnapshot& state) -> bool
        {
            if (state.manager.configuration != impl_->configuration.fixed_step
                || state.collider_slots.size()
                    != state.manager.body_slots.size()
                || state.collider_slots.size()
                    > impl_->configuration.fixed_step.maximum_bodies
                || state.static_primitives.size()
                    > impl_->configuration.maximum_static_primitives
                || state.contacts.size()
                    > impl_->configuration.maximum_contacts
                || !within_component_limit(
                    state.gravity,
                    impl_->configuration.maximum_acceleration)
                || state.next_contact_id == 0)
            {
                return false;
            }

            PhysicsManager candidate{ impl_->configuration.fixed_step };
            if (candidate.restore(state.manager) != ResultCode::success)
                return false;

            std::uint32_t activeBodies = 0;
            for (std::size_t index = 0;
                index < state.manager.body_slots.size();
                ++index)
            {
                const BodySlotSnapshot& bodySlot = state.manager.body_slots[index];
                const ColliderSlot2DSnapshot& collider =
                    state.collider_slots[index];
                if (collider.generation != bodySlot.generation
                    || collider.occupied != bodySlot.occupied)
                {
                    return false;
                }
                if (!collider.occupied)
                    continue;
                ++activeBodies;
                if (!valid_shape(collider.shape)
                    || !valid_filter(collider.filter)
                    || !shape_inside_bounds(
                        collider.shape,
                        {
                            bodySlot.state.transform.position.x,
                            bodySlot.state.transform.position.y
                        },
                        impl_->configuration.world_bounds))
                {
                    return false;
                }
            }

            std::uint64_t previousStaticId = 0;
            for (const StaticAabbPrimitive2D& primitive :
                state.static_primitives)
            {
                if (!valid_static_primitive(
                        primitive,
                        impl_->configuration)
                    || primitive.stable_id <= previousStaticId)
                {
                    return false;
                }
                previousStaticId = primitive.stable_id;
            }

            std::uint64_t largestContactId = 0;
            for (std::size_t index = 0; index < state.contacts.size(); ++index)
            {
                const ContactRecord2D& contact = state.contacts[index];
                if (!contact.id
                    || !target_less(contact.first, contact.second)
                    || !finite(contact.normal)
                    || !finite(contact.point)
                    || !finite(contact.penetration)
                    || contact.penetration <= 0.0
                    || contact.began_step > contact.last_step
                    || contact.last_step > state.manager.fixed_step_index
                    || (index != 0
                        && !contact_key_less(
                            state.contacts[index - 1],
                            contact)))
                {
                    return false;
                }

                const auto validTarget = [&state](
                    const ContactTarget2D& target) -> bool
                {
                    if (target.kind == ContactTarget2DKind::body)
                    {
                        return target.body
                            && target.body.index < state.manager.body_slots.size()
                            && state.manager.body_slots[target.body.index].occupied
                            && state.manager.body_slots[target.body.index].generation
                                == target.body.generation
                            && target.static_primitive_id == 0;
                    }
                    if (target.kind != ContactTarget2DKind::static_map
                        || target.body
                        || target.static_primitive_id == 0)
                    {
                        return false;
                    }
                    const auto found = std::lower_bound(
                        state.static_primitives.begin(),
                        state.static_primitives.end(),
                        target.static_primitive_id,
                        [](const auto& primitive, std::uint64_t id)
                        {
                            return primitive.stable_id < id;
                        });
                    return found != state.static_primitives.end()
                        && found->stable_id == target.static_primitive_id;
                };
                if (!validTarget(contact.first)
                    || !validTarget(contact.second)
                    || (contact.first.kind == ContactTarget2DKind::static_map
                        && contact.second.kind
                            == ContactTarget2DKind::static_map)
                    || !valid_snapshot_contact_geometry(contact, state))
                {
                    return false;
                }
                largestContactId = (std::max)(
                    largestContactId,
                    contact.id.value);
                for (std::size_t previous = 0; previous < index; ++previous)
                {
                    if (state.contacts[previous].id == contact.id)
                        return false;
                }
            }
            if (largestContactId >= state.next_contact_id)
                return false;

            return state.metrics.active_bodies == activeBodies
                && state.metrics.static_primitives
                    == state.static_primitives.size()
                && state.metrics.active_contacts == state.contacts.size()
                && state.metrics.peak_contacts
                    >= state.metrics.active_contacts
                && state.metrics.fixed_steps_committed
                    == state.manager.fixed_step_index;
        };

        if (!validateState(snapshotValue.state)
            || !validateState(snapshotValue.reset_point))
        {
            return Solver2DCode::invalid_snapshot;
        }

        const Solver2DStateSnapshot before = impl_->capture_state();
        if (!impl_->restore_state(snapshotValue.state))
            return Solver2DCode::invalid_snapshot;
        impl_->reset_point = snapshotValue.reset_point;
        if (impl_->manager.snapshot() != snapshotValue.state.manager)
        {
            (void)impl_->restore_state(before);
            return Solver2DCode::invalid_snapshot;
        }
        return Solver2DCode::success;
    }
}
