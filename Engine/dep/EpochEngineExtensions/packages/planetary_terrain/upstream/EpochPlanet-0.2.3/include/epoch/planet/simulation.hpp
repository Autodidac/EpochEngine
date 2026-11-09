#pragma once
#include "epoch/planet/world.hpp"
#include <functional>
#include <optional>

namespace epoch::planet {
enum class TravelMode : std::uint8_t { flight, walk, cruise };
struct Camera {
    DVec3 position{},forward{0,0,-1},up{0,1,0};
    [[nodiscard]] DVec3 right() const { return normalized(cross(forward,up),{1,0,0}); }
    void orthonormalize();
};
struct PilotInput { double forward{},right{},rise{};bool boost{}; };
struct SessionState {
    std::uint32_t seed=1337;
    std::uint32_t anchor=0,target=0;
    double seconds{},time_scale=1,speed_factor=1;
    DVec3 local_position{},forward{},up{};
    TravelMode mode=TravelMode::flight;
    DVec3 fall_velocity{};
    bool gravity_enabled=true,level_assist=true;
    WorldConfig world{};
};
class Simulation {
public:
    explicit Simulation(std::uint32_t seed=1337);
    Simulation(std::uint32_t seed,WorldConfig config);
    Simulation(std::uint32_t seed,WorldConfig config,std::optional<std::size_t> body_scope);
    [[nodiscard]] const Universe& world() const noexcept { return world_; }
    [[nodiscard]] const Camera& camera() const noexcept { return camera_; }
    [[nodiscard]] double seconds() const noexcept { return seconds_; }
    [[nodiscard]] double time_scale() const noexcept { return time_scale_; }
    [[nodiscard]] double speed_factor() const noexcept { return speed_factor_; }
    [[nodiscard]] double speed() const noexcept { return speed_; }
    [[nodiscard]] std::size_t anchor() const noexcept { return anchor_; }
    [[nodiscard]] std::size_t target() const noexcept { return target_; }
    [[nodiscard]] TravelMode mode() const noexcept { return mode_; }
    [[nodiscard]] double altitude() const;
    [[nodiscard]] Camera interpolated_camera(double alpha) const;
    [[nodiscard]] DVec3 gravity() const;
    [[nodiscard]] double vertical_speed() const { return dot(fall_velocity_,radial_up()); }
    [[nodiscard]] bool gravity_enabled() const noexcept { return gravity_enabled_; }
    [[nodiscard]] bool level_assist() const noexcept { return level_assist_; }
    void set_gravity(bool enabled) { gravity_enabled_=enabled;if(!enabled) fall_velocity_={}; }
    void set_level_assist(bool enabled) { level_assist_=enabled; }
    [[nodiscard]] double sea_altitude() const;
    [[nodiscard]] DVec3 radial_up() const;
    void select(std::size_t index);
    void orbit(std::size_t index);
    [[nodiscard]] bool land(std::size_t index);
    void toggle_walk();
    void cruise();
    void stop_cruise() { if(mode_==TravelMode::cruise) mode_=TravelMode::flight; }
    void adjust_speed(double factor);
    void set_time_scale(double scale);
    void look(double yaw,double pitch,double roll=0.0);
    void step(double dt,const PilotInput& input);
    [[nodiscard]] SessionState state() const;
    void restore(const SessionState& state);
private:
    [[nodiscard]] DVec3 resolve_motion(DVec3 old_position,DVec3 desired) const;
    void align_horizon(DVec3 up,double strength,double dt);
    void track_forward(DVec3 desired,double dt);
    void reset_interpolation();
    Universe world_;
    std::optional<std::size_t> body_scope_{};
    Camera camera_,previous_camera_;
    double previous_seconds_{};
    DVec3 fall_velocity_{};
    bool gravity_enabled_=true,level_assist_=true;
    std::size_t anchor_{},target_{};
    double seconds_{},time_scale_=1,speed_factor_=1,speed_{};
    TravelMode mode_=TravelMode::flight;
};
class FixedClock {
public:
    static constexpr double step_seconds=1.0/60.0;
    std::size_t advance(double elapsed,const std::function<void(double)>& step);
    [[nodiscard]] double remainder() const noexcept { return accumulator_; }
private:
    double accumulator_{};
};
[[nodiscard]] bool valid_state(const SessionState& state) noexcept;
} // namespace epoch::planet
