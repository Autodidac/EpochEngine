#pragma once
#include "epoch/planet/math.hpp"
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace epoch::planet {
struct WorldConfig {
    std::uint32_t reference_body=0;
    double reference_radius_meters=6'371'000.0;
    friend bool operator==(const WorldConfig&,const WorldConfig&)=default;
};
[[nodiscard]] inline bool valid_world_config(const WorldConfig& value) noexcept {
    return value.reference_body<=5u&&std::isfinite(value.reference_radius_meters)
        &&value.reference_radius_meters>=250.0&&value.reference_radius_meters<=100'000'000.0;
}
enum class BodyKind : std::uint8_t { temperate, desert, ice, volcanic, gas, moon };
struct Body {
    std::uint32_t id{},seed{};
    std::string_view name;
    BodyKind kind{};
    double radius{},height_scale{},atmosphere_height{},surface_gravity{};
    double orbit_radius{},orbit_period{},orbit_phase{},inclination{};
    int parent=-1;
    FVec3 atmosphere_color{};
    bool ocean{};
    double geography_scale=1.0;
};
struct SurfaceSample { double height{},temperature{},moisture{};FVec3 color{}; };
[[nodiscard]] std::uint32_t hash32(std::uint32_t x) noexcept;
[[nodiscard]] double noise3(DVec3 p,std::uint32_t seed) noexcept;
[[nodiscard]] double fbm(DVec3 p,std::uint32_t seed,int octaves=5) noexcept;
[[nodiscard]] double sample_height(const Body& body,DVec3 unit_direction) noexcept;
[[nodiscard]] SurfaceSample sample_surface(const Body& body,DVec3 unit_direction) noexcept;
[[nodiscard]] DVec3 surface_normal(const Body& body,DVec3 direction,double sample_spacing=3.0) noexcept;
[[nodiscard]] std::string_view kind_name(BodyKind kind) noexcept;
class Universe {
public:
    explicit Universe(std::uint32_t seed=1337);
    Universe(std::uint32_t seed,WorldConfig config);
    [[nodiscard]] const WorldConfig& config() const noexcept {return config_;}
    [[nodiscard]] double length_scale() const noexcept {return length_scale_;}
    [[nodiscard]] double scaled_sun_radius() const noexcept {return sun_radius*length_scale_;}
    [[nodiscard]] std::span<const Body> bodies() const noexcept { return bodies_; }
    [[nodiscard]] const Body& body(std::size_t index) const { return bodies_.at(index); }
    [[nodiscard]] std::uint32_t seed() const noexcept { return seed_; }
    [[nodiscard]] DVec3 center(std::size_t index,double seconds) const;
    [[nodiscard]] std::size_t nearest(DVec3 position,double seconds) const;
    [[nodiscard]] double ground_radius(std::size_t index,DVec3 direction) const;
    static constexpr double sun_radius=60'000'000.0;
private:
    std::uint32_t seed_;
    std::vector<Body> bodies_;
    WorldConfig config_{};
    double length_scale_=1.0;
};
} // namespace epoch::planet
