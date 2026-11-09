#include "epoch/planet/simulation.hpp"
#include <algorithm>
#include <stdexcept>

namespace epoch::planet {
void Camera::orthonormalize() {
    forward=normalized(forward,{0,0,-1});
    auto r=cross(forward,up);if(length(r)<1e-8) r=tangent(forward);
    r=normalized(r);up=normalized(cross(r,forward));
}
Simulation::Simulation(std::uint32_t seed):Simulation(seed,{}) {}
Simulation::Simulation(std::uint32_t seed,WorldConfig config):Simulation(seed,config,{}) {}
Simulation::Simulation(std::uint32_t seed,WorldConfig config,std::optional<std::size_t> body_scope)
    :world_(seed,config),body_scope_(body_scope) {
    if(body_scope_&&*body_scope_>=world_.bodies().size())throw std::out_of_range("Scoped body");
    orbit(body_scope_.value_or(0));
}
void Simulation::reset_interpolation() { previous_camera_=camera_;previous_seconds_=seconds_; }
Camera Simulation::interpolated_camera(double alpha) const {
    alpha=std::isfinite(alpha)?std::clamp(alpha,0.0,1.0):1.0;
    Camera result;
    // Interpolate in the current carrier's local frame, not old absolute orbital coordinates.
    // The scene uses the current simulation time, so its carrier motion must not be interpolated twice.
    const auto center=world_.center(anchor_,seconds_);
    const auto old_local=previous_camera_.position-world_.center(anchor_,previous_seconds_);
    result.position=center+mix(old_local,camera_.position-center,alpha);
    result.forward=normalized(mix(previous_camera_.forward,camera_.forward,alpha),camera_.forward);
    result.up=normalized(mix(previous_camera_.up,camera_.up,alpha),camera_.up);
    result.orthonormalize();return result;
}
DVec3 Simulation::gravity() const {
    if(!gravity_enabled_||mode_!=TravelMode::flight) return {};
    DVec3 acceleration{};
    for(const auto& body:world_.bodies()) {
        if(body_scope_&&body.id!=*body_scope_)continue;
        const auto offset=camera_.position-world_.center(body.id,seconds_);
        const auto radius=std::max(length(offset),body.radius);
        const auto altitude=radius-body.radius;
        const double outer=std::max(body.radius*0.15,body.atmosphere_height*4.0);
        const double inner=std::min(outer*0.4,std::max(5000.0*body.geography_scale,body.atmosphere_height*0.5));
        const double influence=1.0-smooth(inner,outer,altitude);
        const double ratio=body.radius/radius;
        acceleration-=normalized(offset)*(body.surface_gravity*ratio*ratio*influence);
    }
    return acceleration;
}
void Simulation::track_forward(DVec3 desired,double dt) {
    desired=normalized(desired,camera_.forward);
    auto axis=cross(camera_.forward,desired);
    const auto cosine=std::clamp(dot(camera_.forward,desired),-1.0,1.0);
    const auto angle=std::atan2(length(axis),cosine);
    if(angle<1e-10) return;
    // A half-turn has infinitely many axes. Retaining camera up chooses a continuous one.
    axis=length(axis)>1e-8?normalized(axis):camera_.up;
    const auto turn=std::min(angle*(1.0-std::exp(-3.8*dt)),1.25*dt);
    camera_.forward=rotate(camera_.forward,axis,turn);
    camera_.up=rotate(camera_.up,axis,turn);camera_.orthonormalize();
}
void Simulation::align_horizon(DVec3 up,double strength,double dt) {
    if(!level_assist_||strength<=0) return;
    const auto projected=up-camera_.forward*dot(up,camera_.forward);
    const auto amount=length(projected);
    // Looking straight at a planet has no unique horizon. Never manufacture a roll there.
    if(amount<0.08) return;
    const auto target=projected/amount;
    const auto cosine=std::clamp(dot(camera_.up,target),-1.0,1.0);
    auto angle=std::atan2(dot(cross(camera_.up,target),camera_.forward),cosine);
    if(cosine<-0.999999) angle=pi;
    const auto weight=strength*smooth(0.08,0.35,amount);
    angle=std::clamp(angle*(1.0-std::exp(-2.4*weight*dt)),-0.85*dt,0.85*dt);
    camera_.up=rotate(camera_.up,camera_.forward,angle);camera_.orthonormalize();
}
DVec3 Simulation::radial_up() const { return normalized(camera_.position-world_.center(anchor_,seconds_)); }
double Simulation::sea_altitude() const { return length(camera_.position-world_.center(anchor_,seconds_))-world_.body(anchor_).radius; }
double Simulation::altitude() const {
    const auto local=camera_.position-world_.center(anchor_,seconds_);
    return length(local)-world_.ground_radius(anchor_,normalized(local));
}
void Simulation::select(std::size_t index) {
    if(index>=world_.bodies().size()||(body_scope_&&index!=*body_scope_))throw std::out_of_range("Target body");
    target_=index;
}
void Simulation::orbit(std::size_t index) {
    select(index);anchor_=index;mode_=TravelMode::flight;speed_=0;
    const auto center=world_.center(index,seconds_);
    auto d=normalized(-center);d=normalized(d+DVec3{0,0.28,0});
    camera_.position=center+d*(world_.body(index).radius*2.65);
    camera_.forward=-d;camera_.up={0,1,0};camera_.orthonormalize();fall_velocity_={};reset_interpolation();
}
bool Simulation::land(std::size_t index) {
    if(world_.body(index).kind==BodyKind::gas) return false;
    select(index);anchor_=index;mode_=TravelMode::walk;
    const auto center=world_.center(index,seconds_);
    DVec3 d=normalized(camera_.position-center);
    if(length(camera_.position-center)>world_.body(index).radius*10) d=normalized(-center);
    // Prefer dry land; this action is an explicit quick-travel control, not the manual flight path.
    for(int i=0;i<160&&world_.body(index).ocean;++i) {
        const auto sample=sample_surface(world_.body(index),d);
        const auto scale=world_.body(index).geography_scale;
        if(sample.height>180*scale&&sample.height<2500*scale&&sample.temperature>0.25) break;
        d=normalized(rotate(d,normalized(DVec3{0.17,1,0.23}),0.073)+DVec3{0.001,-0.002,0.001});
    }
    camera_.position=center+d*(world_.ground_radius(index,d)+1.8);
    const auto east=tangent(d);
    camera_.forward=normalized(east+d*0.06);camera_.up=d;camera_.orthonormalize();speed_=0;fall_velocity_={};reset_interpolation();
    return true;
}
void Simulation::toggle_walk() {
    if(mode_==TravelMode::walk) { mode_=TravelMode::flight;fall_velocity_={};return; }
    if(world_.body(anchor_).kind==BodyKind::gas) return;
    if(altitude()<500) {
        mode_=TravelMode::walk;const auto d=radial_up();
        camera_.position=world_.center(anchor_,seconds_)+d*(world_.ground_radius(anchor_,d)+1.8);
        fall_velocity_={};reset_interpolation();
    }
}
void Simulation::cruise() { mode_=TravelMode::cruise;fall_velocity_={}; }
void Simulation::adjust_speed(double factor) {
    if(std::isfinite(factor)&&factor>0) speed_factor_=std::clamp(speed_factor_*factor,0.01,100.0);
}
void Simulation::set_time_scale(double scale) {
    if(!std::isfinite(scale)||scale<0||scale>10'000) throw std::invalid_argument("Time scale outside [0,10000]");
    time_scale_=scale;
}
void Simulation::look(double yaw,double pitch,double roll) {
    if(!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(roll)) return;
    mode_=mode_==TravelMode::cruise?TravelMode::flight:mode_;
    const auto up=camera_.up; // No yaw-axis discontinuity when the nearest-body label changes.
    camera_.forward=rotate(camera_.forward,up,yaw);
    camera_.up=rotate(camera_.up,up,yaw);camera_.orthonormalize();
    // Pitch is relative to camera right, with no pole singularity or global-Euler lock.
    const auto r=camera_.right();
    camera_.forward=rotate(camera_.forward,r,pitch);camera_.up=rotate(camera_.up,r,pitch);
    camera_.orthonormalize();
    if(roll!=0.0&&mode_!=TravelMode::walk) {
        camera_.up=rotate(camera_.up,camera_.forward,roll);
        camera_.orthonormalize();
    }
    previous_camera_.forward=camera_.forward;previous_camera_.up=camera_.up;
}
DVec3 Simulation::resolve_motion(DVec3 from,DVec3 to) const {
    const auto delta=to-from;const auto distance=length(delta);if(distance<1e-12) return to;
    const auto direction=delta/distance;double first=distance;
    for(const auto& b:world_.bodies()) {
        if(body_scope_&&b.id!=*body_scope_)continue;
        const auto center=world_.center(b.id,seconds_);const auto local=from-center;
        double near_t=0,far_t=0;
        const auto shell=b.radius+std::max(100.0*b.geography_scale,b.height_scale*1.5)+2.0;
        if(!ray_sphere(local,direction,shell,near_t,far_t)||near_t>first) continue;
        const double start=std::max(0.0,near_t),end=std::min(first,far_t);
        if(end<start) continue;
        const auto clearance=[&](double t) {
            const auto p=local+direction*t;return length(p)-world_.ground_radius(b.id,normalized(p))-1.8;
        };
        double previous=start;
        // Swept collision catches entering/leaving a body in a single high-speed frame.
        const auto samples=static_cast<int>(std::clamp(std::ceil((end-start)/8.0),1.0,32.0));
        for(int s=1;s<=samples;++s) {
            const double t=std::lerp(start,end,static_cast<double>(s)/samples);
            if(clearance(t)<0) {
                double lo=previous,hi=t;
                for(int j=0;j<32;++j) { const auto mid=(lo+hi)*0.5;if(clearance(mid)>0) lo=mid;else hi=mid; }
                first=std::min(first,lo);break;
            }
            previous=t;
        }
    }
    auto result=from+direction*first;
    const auto nearest=body_scope_.value_or(world_.nearest(result,seconds_));const auto center=world_.center(nearest,seconds_);
    const auto local=result-center,up=normalized(local);
    const double ground=world_.ground_radius(nearest,up)+1.8;
    if(length(local)<ground) result=center+up*ground;
    return result;
}
void Simulation::step(double dt,const PilotInput& input) {
    if(!std::isfinite(dt)||dt<=0||dt>0.25) throw std::invalid_argument("Simulation step must be in (0,0.25]");
    if(!std::isfinite(input.forward)||!std::isfinite(input.right)||!std::isfinite(input.rise)) throw std::invalid_argument("Non-finite pilot input");
    previous_camera_=camera_;previous_seconds_=seconds_;
    const auto old_center=world_.center(anchor_,seconds_);
    seconds_+=dt*time_scale_;
    camera_.position+=world_.center(anchor_,seconds_)-old_center;
    const auto start=camera_.position;
    if(mode_==TravelMode::cruise) {
        const auto dest=world_.center(target_,seconds_);
        const auto offset=start-dest;
        const double remaining=length(offset)-world_.body(target_).radius*2.65;
        if(remaining<=0.05) {
            // Preserve the arrival attitude, including roll. No orbit() or frame reset.
            anchor_=target_;mode_=TravelMode::flight;fall_velocity_={};
        } else {
            const auto toward=normalized(-offset);
            track_forward(toward,dt);
            // Exponential position convergence has no hard 100-m arrival discontinuity.
            const auto move=std::min({remaining,remaining*(-std::expm1(-0.8*dt)),2e8*dt});
            camera_.position=resolve_motion(start,start+toward*move);
            if(length(camera_.position-start)<move*0.05&&move>1.0) mode_=TravelMode::flight;
        }
    } else {
        const auto up=radial_up();auto forward=camera_.forward,right=camera_.right();
        if(mode_==TravelMode::walk) { forward=normalized(forward-up*dot(forward,up),tangent(up));right=normalized(cross(forward,up)); }
        auto move=forward*std::clamp(input.forward,-1.0,1.0)+right*std::clamp(input.right,-1.0,1.0)+up*(mode_==TravelMode::flight?std::clamp(input.rise,-1.0,1.0):0.0);
        if(length(move)>1) move=normalized(move);
        double rate=mode_==TravelMode::walk?5.5:std::clamp(std::max(0.0,altitude())*0.65+18.0,18.0,6e7);
        rate*=speed_factor_*(input.boost?(mode_==TravelMode::walk?2.5:8.0):1.0);
        if(mode_==TravelMode::walk) {
            const auto center=world_.center(anchor_,seconds_);
            const auto direction=normalized(start+move*rate*dt-center);
            camera_.position=center+direction*(world_.ground_radius(anchor_,direction)+1.8);
            // Parallel transport the whole frame before leveling; preserve manual pitch.
            const auto axis=cross(up,direction);const auto sine=length(axis);
            if(sine>1e-12) {
                const auto angle=std::atan2(sine,dot(up,direction));
                camera_.forward=rotate(camera_.forward,axis/sine,angle);
                camera_.up=rotate(camera_.up,axis/sine,angle);
            }
            align_horizon(direction,1.0,dt);
        } else {
            const auto acceleration=gravity();
            // Flight assist supplies drag, not hover. Gravity still settles the craft onto terrain;
            // the 18 m/s minimum vertical thrust can lift off against Verdant's terminal descent.
            constexpr double drag=0.8;
            const auto decay=std::exp(-drag*dt);
            fall_velocity_=fall_velocity_*decay+acceleration*((1.0-decay)/drag);
            const auto desired=start+(move*rate+fall_velocity_)*dt;
            camera_.position=resolve_motion(start,desired);
            if(length(camera_.position-desired)>1e-5) {
                const auto normal=radial_up();
                const auto into=dot(fall_velocity_,normal);
                if(into<0) fall_velocity_-=normal*into;
            }
            const auto& body=world_.body(anchor_);
            const double outer=std::max(body.radius*0.15,body.atmosphere_height*4.0);
            align_horizon(radial_up(),1.0-smooth(outer*0.15,outer,std::max(0.0,sea_altitude())),dt);
        }
    }
    camera_.orthonormalize();
    speed_=length(camera_.position-start)/dt;
    const auto nearest=body_scope_.value_or(world_.nearest(camera_.position,seconds_));
    if(nearest!=anchor_) {
        const auto current_distance=length(camera_.position-world_.center(anchor_,seconds_))-world_.body(anchor_).radius;
        const auto next_distance=length(camera_.position-world_.center(nearest,seconds_))-world_.body(nearest).radius;
        // Avoid rapid carrier/label switching at equal-distance boundaries in space.
        if(next_distance+std::max(1000.0*world_.length_scale(),std::abs(current_distance)*0.03)<current_distance) anchor_=nearest;
    }
}
SessionState Simulation::state() const {
    return {world_.seed(),static_cast<std::uint32_t>(anchor_),static_cast<std::uint32_t>(target_),seconds_,time_scale_,speed_factor_,
            camera_.position-world_.center(anchor_,seconds_),camera_.forward,camera_.up,mode_,fall_velocity_,gravity_enabled_,level_assist_,world_.config()};
}
bool valid_state(const SessionState& s) noexcept {
    return valid_world_config(s.world)&&s.anchor<6&&s.target<6&&std::isfinite(s.seconds)&&s.seconds>=0&&s.seconds<1e12&&
        std::isfinite(s.time_scale)&&s.time_scale>=0&&s.time_scale<=10000&&
        std::isfinite(s.speed_factor)&&s.speed_factor>=0.01&&s.speed_factor<=100&&
        finite(s.local_position)&&length(s.local_position)>(s.world==WorldConfig{}?100.0:0.0)&&length(s.local_position)<1e14&&
        finite(s.forward)&&finite(s.up)&&length(s.forward)>0.9&&length(s.forward)<1.1&&
        length(s.up)>0.9&&length(s.up)<1.1&&length(cross(s.forward,s.up))>0.9&&
        finite(s.fall_velocity)&&length(s.fall_velocity)<1000.0&&
        static_cast<unsigned>(s.mode)<=static_cast<unsigned>(TravelMode::cruise);
}
void Simulation::restore(const SessionState& s) {
    if(!valid_state(s)||(body_scope_&&(s.anchor!=*body_scope_||s.target!=*body_scope_)))
        throw std::invalid_argument("Invalid or incompatible saved session");
    Universe replacement(s.seed,s.world);world_=std::move(replacement);
    anchor_=s.anchor;target_=s.target;seconds_=s.seconds;time_scale_=s.time_scale;speed_factor_=s.speed_factor;mode_=s.mode;
    camera_={world_.center(anchor_,seconds_)+s.local_position,s.forward,s.up};camera_.orthonormalize();speed_=0;
    fall_velocity_=s.fall_velocity;gravity_enabled_=s.gravity_enabled;level_assist_=s.level_assist;reset_interpolation();
}
std::size_t FixedClock::advance(double elapsed,const std::function<void(double)>& step) {
    if(!std::isfinite(elapsed)||elapsed<0) throw std::invalid_argument("Invalid elapsed time");
    accumulator_+=std::min(elapsed,0.25);std::size_t count=0;
    while(accumulator_+1e-12>=step_seconds&&count<15) { step(step_seconds);accumulator_-=step_seconds;++count; }
    accumulator_=std::max(0.0,accumulator_);return count;
}
} // namespace epoch::planet
