/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

export module render.lighting;

import render.math;

export namespace epochengine::lighting
{
    using Vec3 = epochengine::render_math::Float3;
    using Color3 = epochengine::render_math::LinearRgb;

    enum class LightKind : std::uint8_t
    {
        Directional,
        Point,
        Spot
    };

    struct LightHandle final
    {
        std::uint32_t index{(std::numeric_limits<std::uint32_t>::max)()};
        std::uint32_t generation{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return index != (std::numeric_limits<std::uint32_t>::max)() && generation != 0;
        }

        friend constexpr bool operator==(const LightHandle&, const LightHandle&) noexcept = default;
    };

    struct LightDesc final
    {
        LightKind kind{LightKind::Directional};
        Color3 color{1.0f, 1.0f, 1.0f};
        float intensity{1.0f};
        Vec3 position{};
        Vec3 direction{0.0f, -1.0f, 0.0f};
        float range{10.0f};
        float innerConeCosine{0.90f};
        float outerConeCosine{0.75f};
        bool enabled{true};
    };

    struct LightValidation final
    {
        bool valid{};
        bool finite{};
        bool positiveRange{};
        bool orderedSpotCone{};
    };

    [[nodiscard]] inline LightValidation validate(const LightDesc& light) noexcept
    {
        const bool validKind =
            light.kind == LightKind::Directional ||
            light.kind == LightKind::Point ||
            light.kind == LightKind::Spot;
        const bool finite =
            std::isfinite(light.color.r) &&
            std::isfinite(light.color.g) &&
            std::isfinite(light.color.b) &&
            std::isfinite(light.intensity) &&
            std::isfinite(light.position.x) &&
            std::isfinite(light.position.y) &&
            std::isfinite(light.position.z) &&
            std::isfinite(light.direction.x) &&
            std::isfinite(light.direction.y) &&
            std::isfinite(light.direction.z) &&
            std::isfinite(light.range) &&
            std::isfinite(light.innerConeCosine) &&
            std::isfinite(light.outerConeCosine);
        const bool positiveRange =
            light.kind == LightKind::Directional ||
            light.range > 0.0f;
        const bool orderedSpotCone =
            light.kind != LightKind::Spot ||
            (light.innerConeCosine >= light.outerConeCosine &&
             light.innerConeCosine <= 1.0f &&
             light.outerConeCosine >= -1.0f);
        return {
            validKind && finite && positiveRange && orderedSpotCone && light.intensity >= 0.0f,
            finite,
            positiveRange,
            orderedSpotCone
        };
    }

    [[nodiscard]] inline LightDesc sanitize(LightDesc light) noexcept
    {
        light.color = {
            (std::max)(0.0f, light.color.r),
            (std::max)(0.0f, light.color.g),
            (std::max)(0.0f, light.color.b)
        };
        light.intensity = (std::max)(0.0f, light.intensity);
        light.direction = epochengine::render_math::normalize(light.direction, {0.0f, -1.0f, 0.0f});
        light.range = (std::max)(0.001f, light.range);
        light.innerConeCosine = (std::clamp)(light.innerConeCosine, -1.0f, 1.0f);
        light.outerConeCosine = (std::clamp)(light.outerConeCosine, -1.0f, light.innerConeCosine);
        return light;
    }

    [[nodiscard]] inline Vec3 direction_from_euler_degrees(Vec3 rotationDegrees) noexcept
    {
        constexpr float kDegreesToRadians = 0.01745329251994329577f;
        const float pitch = rotationDegrees.x * kDegreesToRadians;
        const float yaw = rotationDegrees.y * kDegreesToRadians;
        const float cosPitch = std::cos(pitch);
        return epochengine::render_math::normalize(
            Vec3{
                cosPitch * std::sin(yaw),
                std::sin(pitch),
                -cosPitch * std::cos(yaw)
            },
            {0.0f, -1.0f, 0.0f});
    }
    struct LightingEnvironment final
    {
        Color3 ambient{0.03f, 0.03f, 0.03f};
    };

    struct FrameLight final
    {
        LightHandle handle{};
        LightDesc desc{};
    };

    struct LightingFrame final
    {
        std::uint64_t revision{};
        LightingEnvironment environment{};
        std::vector<FrameLight> lights{};
    };

    struct LightingMetrics final
    {
        std::size_t slotCount{};
        std::size_t activeCount{};
        std::size_t directionalCount{};
        std::size_t pointCount{};
        std::size_t spotCount{};
        std::size_t disabledCount{};
        std::uint64_t revision{};
    };

    class LightManager final
    {
    public:
        explicit LightManager(std::size_t capacity = 64) noexcept
            : capacity_((std::max)(std::size_t{1}, capacity))
        {
        }
        [[nodiscard]] LightHandle create(LightDesc desc)
        {
            if (!validate(desc).valid || active_count() >= capacity_)
            {
                return {};
            }
            desc = sanitize(desc);

            std::uint32_t index{};
            if (!freeSlots_.empty())
            {
                index = freeSlots_.back();
                freeSlots_.pop_back();
            }
            else
            {
                index = static_cast<std::uint32_t>(slots_.size());
                slots_.push_back({});
            }

            Slot& slot = slots_[index];
            if (slot.generation == 0)
            {
                slot.generation = 1;
            }
            slot.desc = desc;
            slot.occupied = true;
            ++revision_;
            return {index, slot.generation};
        }

        [[nodiscard]] bool destroy(LightHandle handle) noexcept
        {
            Slot* slot = resolve(handle);
            if (slot == nullptr)
            {
                return false;
            }

            slot->occupied = false;
            slot->generation = next_generation(slot->generation);
            freeSlots_.push_back(handle.index);
            ++revision_;
            return true;
        }

        [[nodiscard]] bool update(LightHandle handle, LightDesc desc) noexcept
        {
            Slot* slot = resolve(handle);
            if (slot == nullptr || !validate(desc).valid)
            {
                return false;
            }

            slot->desc = sanitize(desc);
            ++revision_;
            return true;
        }

        [[nodiscard]] std::optional<LightDesc> get(LightHandle handle) const noexcept
        {
            const Slot* slot = resolve(handle);
            if (slot == nullptr)
            {
                return std::nullopt;
            }
            return slot->desc;
        }

        [[nodiscard]] bool contains(LightHandle handle) const noexcept
        {
            return resolve(handle) != nullptr;
        }

        [[nodiscard]] LightingFrame build_frame() const
        {
            LightingFrame frame{};
            frame.revision = revision_;
            frame.environment = environment_;
            frame.lights.reserve(slots_.size() - freeSlots_.size());
            for (std::uint32_t index = 0; index < slots_.size(); ++index)
            {
                const Slot& slot = slots_[index];
                if (slot.occupied && slot.desc.enabled)
                {
                    frame.lights.push_back({
                        {index, slot.generation},
                        slot.desc
                    });
                }
            }
            return frame;
        }

        [[nodiscard]] LightingMetrics metrics() const noexcept
        {
            LightingMetrics result{};
            result.slotCount = slots_.size();
            result.revision = revision_;

            for (const Slot& slot : slots_)
            {
                if (!slot.occupied)
                {
                    continue;
                }
                ++result.activeCount;
                if (!slot.desc.enabled)
                {
                    ++result.disabledCount;
                }
                switch (slot.desc.kind)
                {
                case LightKind::Directional: ++result.directionalCount; break;
                case LightKind::Point: ++result.pointCount; break;
                case LightKind::Spot: ++result.spotCount; break;
                }
            }
            return result;
        }
        void set_environment(LightingEnvironment environment) noexcept
        {
            const auto finiteNonnegative = [](float value) noexcept
            {
                return std::isfinite(value) ? (std::max)(0.0f, value) : 0.0f;
            };
            environment.ambient = {
                finiteNonnegative(environment.ambient.r),
                finiteNonnegative(environment.ambient.g),
                finiteNonnegative(environment.ambient.b)
            };
            environment_ = environment;
            ++revision_;
        }
        [[nodiscard]] LightingEnvironment environment() const noexcept
        {
            return environment_;
        }



        [[nodiscard]] std::uint64_t revision() const noexcept
        {
            return revision_;
        }

    private:
        struct Slot final
        {
            LightDesc desc{};
            std::uint32_t generation{1};
            bool occupied{};
        };

        [[nodiscard]] static constexpr std::uint32_t next_generation(std::uint32_t generation) noexcept
        {
            ++generation;
            return generation == 0 ? 1 : generation;
        }

        [[nodiscard]] Slot* resolve(LightHandle handle) noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
            {
                return nullptr;
            }
            Slot& slot = slots_[handle.index];
            return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
        }

        [[nodiscard]] const Slot* resolve(LightHandle handle) const noexcept
        {
            if (!handle.valid() || handle.index >= slots_.size())
            {
                return nullptr;
            }
            const Slot& slot = slots_[handle.index];
            return slot.occupied && slot.generation == handle.generation ? &slot : nullptr;
        }

        [[nodiscard]] std::size_t active_count() const noexcept
        {
            return slots_.size() - freeSlots_.size();
        }

        std::vector<Slot> slots_{};
        std::vector<std::uint32_t> freeSlots_{};
        std::size_t capacity_{64};
        LightingEnvironment environment_{};
        std::uint64_t revision_{};
    };

    struct SurfaceSample final
    {
        Vec3 position{};
        Vec3 normal{0.0f, 1.0f, 0.0f};
        Vec3 viewDirection{0.0f, 0.0f, 1.0f};
        Color3 albedo{1.0f, 1.0f, 1.0f};
        Color3 specularColor{1.0f, 1.0f, 1.0f};
        float shininess{32.0f};
    };

    struct LightingResult final
    {
        Color3 ambient{};
        Color3 diffuse{};
        Color3 specular{};
        Color3 combined{};
        std::size_t evaluatedLights{};
    };

    [[nodiscard]] inline float distance_attenuation(float distance, float range) noexcept
    {
        const float normalized = (std::clamp)(distance / (std::max)(range, 0.001f), 0.0f, 1.0f);
        const float window = 1.0f - normalized * normalized;
        return window * window / (std::max)(1.0f, distance * distance);
    }

    [[nodiscard]] inline float spot_attenuation(const LightDesc& light, Vec3 surfaceToLight) noexcept
    {
        const Vec3 lightToSurface = epochengine::render_math::scale(surfaceToLight, -1.0f);
        const float cosine = epochengine::render_math::dot(epochengine::render_math::normalize(light.direction), epochengine::render_math::normalize(lightToSurface));
        const float width = (std::max)(0.0001f, light.innerConeCosine - light.outerConeCosine);
        const float factor = (std::clamp)((cosine - light.outerConeCosine) / width, 0.0f, 1.0f);
        return factor * factor * (3.0f - 2.0f * factor);
    }

    [[nodiscard]] inline LightingResult evaluate_reference_raster_lighting(
        std::span<const FrameLight> lights,
        const SurfaceSample& sample) noexcept
    {
        LightingResult result{};
        const Vec3 normal = epochengine::render_math::normalize(sample.normal);
        const Vec3 view = epochengine::render_math::normalize(sample.viewDirection, {0.0f, 0.0f, 1.0f});
        const float shininess = (std::clamp)(sample.shininess, 1.0f, 2048.0f);

        for (const FrameLight& frameLight : lights)
        {
            const LightDesc& light = frameLight.desc;
            if (!light.enabled || light.intensity <= 0.0f)
            {
                continue;
            }

            ++result.evaluatedLights;
            const Color3 radiance = epochengine::render_math::scale(light.color, light.intensity);

            Vec3 surfaceToLight{};
            float attenuation = 1.0f;
            if (light.kind == LightKind::Directional)
            {
                surfaceToLight = epochengine::render_math::normalize(epochengine::render_math::scale(light.direction, -1.0f));
            }
            else
            {
                const Vec3 delta = epochengine::render_math::subtract(light.position, sample.position);
                const float distance = epochengine::render_math::length(delta);
                if (distance > light.range)
                {
                    continue;
                }
                surfaceToLight = epochengine::render_math::normalize(delta);
                attenuation = distance_attenuation(distance, light.range);
                if (light.kind == LightKind::Spot)
                {
                    attenuation *= spot_attenuation(light, surfaceToLight);
                }
            }

            const float diffuseAmount = (std::max)(0.0f, epochengine::render_math::dot(normal, surfaceToLight));
            if (diffuseAmount <= 0.0f || attenuation <= 0.0f)
            {
                continue;
            }

            const Color3 incident = epochengine::render_math::scale(radiance, attenuation);
            result.diffuse = epochengine::render_math::add(
                result.diffuse,
                epochengine::render_math::scale(epochengine::render_math::multiply(sample.albedo, incident), diffuseAmount));

            const Vec3 halfVector = epochengine::render_math::normalize(epochengine::render_math::add(surfaceToLight, view), normal);
            const float specularAmount = std::pow(
                (std::max)(0.0f, epochengine::render_math::dot(normal, halfVector)),
                shininess);
            result.specular = epochengine::render_math::add(
                result.specular,
                epochengine::render_math::scale(epochengine::render_math::multiply(sample.specularColor, incident), specularAmount));
        }

        result.combined = epochengine::render_math::add(epochengine::render_math::add(result.ambient, result.diffuse), result.specular);
        return result;
    }

    [[nodiscard]] inline LightingResult evaluate_reference_raster_lighting(
        const LightingFrame& frame,
        const SurfaceSample& sample) noexcept
    {
        LightingResult result = evaluate_reference_raster_lighting(
            std::span<const FrameLight>{frame.lights.data(), frame.lights.size()},
            sample);
        result.ambient = epochengine::render_math::multiply(sample.albedo, frame.environment.ambient);
        result.combined = epochengine::render_math::add(
            result.ambient,
            epochengine::render_math::add(result.diffuse, result.specular));
        return result;
    }
}