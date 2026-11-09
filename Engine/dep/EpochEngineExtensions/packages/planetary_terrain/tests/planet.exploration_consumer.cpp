// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Real module/algorithm consumer; no viewer, renderer, worker or model starts.
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <stop_token>
import planet.terrain;
import planet.exploration;

namespace core = epoch::extensions::planetary_terrain::core;
namespace exploration = epoch::extensions::planetary_terrain::exploration;
namespace
{
    std::size_t assertions{};
    void require(bool value, const char* message)
    {
        ++assertions;
        if (!value) throw std::runtime_error(message);
    }
    double angle(core::DVec3 lhs, core::DVec3 rhs)
    {
        const auto cross = core::DVec3{lhs.y*rhs.z-lhs.z*rhs.y,
            lhs.z*rhs.x-lhs.x*rhs.z, lhs.x*rhs.y-lhs.y*rhs.x};
        return std::atan2(core::length(cross), std::clamp(lhs.x*rhs.x+lhs.y*rhs.y+lhs.z*rhs.z, -1.0, 1.0));
    }
    double dot(core::DVec3 lhs, core::DVec3 rhs)
    {
        return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z;
    }
    core::DVec3 unit(core::DVec3 value) { return value/core::length(value); }
    void same_state(const exploration::SessionState& lhs, const exploration::SessionState& rhs)
    {
        require(lhs.seed==rhs.seed && lhs.anchor==rhs.anchor && lhs.target==rhs.target
            && lhs.seconds==rhs.seconds && lhs.time_scale==rhs.time_scale
            && lhs.speed_factor==rhs.speed_factor && lhs.mode==rhs.mode
            && core::length(lhs.local_position-rhs.local_position)==0.0
            && core::length(lhs.forward-rhs.forward)==0.0 && core::length(lhs.up-rhs.up)==0.0
            && core::length(lhs.fall_velocity-rhs.fall_velocity)==0.0
            && lhs.gravity_enabled==rhs.gravity_enabled && lhs.level_assist==rhs.level_assist,
            "rejected or cancelled work changed the session");
    }
    void step(exploration::Simulation& simulation, const exploration::PilotInput& input = {})
    {
        require(exploration::advance(simulation, 1.0/60.0, input)==exploration::StepCode::advanced,
            "valid bounded tick rejected");
    }
    void guards_and_restore()
    {
        exploration::Simulation simulation{1337};
        const auto before = simulation.state();
        for (const auto delta : {-1.0, 0.0, 0.251, std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::quiet_NaN()})
        {
            require(exploration::advance(simulation,delta,{})==exploration::StepCode::invalid_input,
                "invalid timestep admitted");
            same_state(before,simulation.state());
        }
        for (unsigned axis=0;axis<3;++axis)
        {
            exploration::PilotInput input{};
            const auto invalid=std::numeric_limits<double>::quiet_NaN();
            if (axis==0) input.forward=invalid;
            else if (axis==1) input.right=invalid;
            else input.rise=invalid;
            require(exploration::advance(simulation,1.0/60.0,input)==exploration::StepCode::invalid_input,
                "nonfinite pilot input admitted");
            same_state(before,simulation.state());
        }
        std::stop_source stop; stop.request_stop();
        require(exploration::advance(simulation,1.0/60.0,{1,0,0,true},stop.get_token())
            ==exploration::StepCode::cancelled,"cancelled tick ran");
        same_state(before,simulation.state());
        simulation.look(std::numeric_limits<double>::quiet_NaN(),0);
        same_state(before,simulation.state());
        for (const auto scale : {-1.0,10001.0,std::numeric_limits<double>::infinity()})
        {
            bool rejected{};
            try { simulation.set_time_scale(scale); } catch(const std::invalid_argument&) { rejected=true; }
            require(rejected,"invalid orbital rate admitted");
            same_state(before,simulation.state());
        }
        auto invalid=before; invalid.anchor=6;
        require(!exploration::valid_state(invalid),"foreign body identity admitted");
        bool rejected{};
        try { simulation.restore(invalid); } catch(const std::invalid_argument&) { rejected=true; }
        require(rejected,"invalid saved state restored");
        same_state(before,simulation.state());
        simulation.look(0.3,-0.2);
        const auto saved=simulation.state();
        exploration::Simulation reopened{17}; reopened.restore(saved);
        require(exploration::valid_state(reopened.state()),"restored camera frame invalid");
        require(core::length(reopened.camera().position-simulation.camera().position)<1e-6,
            "local double camera restore changed world position");
        require(angle(reopened.camera().forward,simulation.camera().forward)<1e-12
            && angle(reopened.camera().up,simulation.camera().up)<1e-12,"restore lost camera attitude");
    }
    void orbital_carrier_and_moon()
    {
        core::Universe world{1337};
        require(world.bodies().size()==6 && world.body(5).parent==0
            && world.body(5).kind==core::BodyKind::moon,"reviewed moon identity is absent");
        for (const auto seconds : {0.0,123.0,90000.0,360000.0})
        {
            const auto& moon=world.body(5);
            const auto phase=moon.orbit_phase+std::remainder(seconds,moon.orbit_period)
                *(2.0*3.14159265358979323846/moon.orbit_period);
            const core::DVec3 relative{std::cos(phase)*moon.orbit_radius,
                std::sin(phase)*std::sin(moon.inclination)*moon.orbit_radius,
                std::sin(phase)*std::cos(moon.inclination)*moon.orbit_radius};
            require(core::length(world.center(5,seconds)-world.center(0,seconds)-relative)<1e-6,
                "moon no longer follows parent-relative orbit");
        }
        exploration::Simulation simulation{1337}; simulation.orbit(5);
        simulation.set_gravity(false); simulation.set_level_assist(false);
        simulation.set_time_scale(1000);
        const auto before=simulation.state(); const auto oldCenter=world.center(5,before.seconds);
        step(simulation);
        require(std::abs(simulation.seconds()-1000.0/60.0)<1e-12,"orbital time rate not applied");
        require(core::length(simulation.state().local_position-before.local_position)<1e-6,
            "idle camera detached from moving moon carrier");
        const auto expected=world.center(5,simulation.seconds())+before.local_position;
        require(core::length(simulation.camera().position-expected)<1e-6,"carrier transform lost precision");
        const auto half=simulation.interpolated_camera(0.5);
        require(core::length(half.position-expected)<1e-6,"interpolation applied carrier motion twice");
        require(core::length(world.center(5,simulation.seconds())-oldCenter)>100.0,"orbital advance did not move moon");
        simulation.set_time_scale(0);
        const auto paused=simulation.camera().position;const auto time=simulation.seconds();
        step(simulation,{1,0,0,false});
        require(simulation.seconds()==time && core::length(simulation.camera().position-paused)>0,
            "pausing orbital time also paused camera input");
    }
    void travel_and_radial_frame()
    {
        exploration::Simulation simulation;
        simulation.set_time_scale(0);simulation.select(1);simulation.look(0.9,0.3);simulation.cruise();
        for (int i=0;i<2400 && simulation.mode()==exploration::TravelMode::cruise;++i)
        {
            const auto old=simulation.camera();step(simulation);
            require(angle(old.forward,simulation.camera().forward)<=1.25/60+1e-7,"cruise turn exceeded original bound");
            require(angle(old.up,simulation.camera().up)<=1.25/60+1e-7,"cruise reset camera roll");
            require(std::abs(dot(simulation.camera().forward,simulation.camera().up))<1e-10,
                "cruise frame lost orthogonality");
        }
        require(simulation.mode()==exploration::TravelMode::flight && simulation.anchor()==1,"cruise never arrived");
        const auto arrived=simulation.camera();step(simulation);
        require(angle(arrived.forward,simulation.camera().forward)<1e-8
            && angle(arrived.up,simulation.camera().up)<1e-8,"arrival reset attitude");
        simulation.cruise();simulation.look(0.1,0);
        require(simulation.mode()==exploration::TravelMode::flight,"manual look did not stop cruise");
        const auto beforeGas=simulation.state();
        require(!simulation.land(4),"gas giant accepted landing");same_state(beforeGas,simulation.state());
        require(simulation.land(0),"solid planet landing refused");
        require(simulation.mode()==exploration::TravelMode::walk && std::abs(simulation.altitude()-1.8)<1e-5,
            "landing did not place the camera above sampled terrain");
        step(simulation,{1,1,0,false});
        require(std::abs(simulation.altitude()-1.8)<1e-5,"walking escaped radial surface");
        simulation.toggle_walk();
        auto state=simulation.state();const auto up=unit(state.local_position);
        state.local_position+=up*100.0;simulation.restore(state);
        const auto altitude=simulation.altitude();
        for(int i=0;i<120;++i)step(simulation);
        require(simulation.altitude()<altitude-5 && simulation.mode()==exploration::TravelMode::flight,
            "gravity failed or secretly activated walking");
        for(int i=0;i<1200;++i)step(simulation);
        require(simulation.altitude()>=1.79 && simulation.altitude()<2.0,"flight penetrated ground");
        for(int i=0;i<90;++i)step(simulation,{0,0,1,false});
        require(simulation.altitude()>10,"flight ascent failed against gravity");
        simulation.set_gravity(false);state=simulation.state();
        // An orthogonal rolled frame is deliberately not a global-Y camera.
        const auto forward=state.forward;const auto cosine=std::cos(1.1),sine=std::sin(1.1);
        const auto cross=core::DVec3{forward.y*state.up.z-forward.z*state.up.y,
            forward.z*state.up.x-forward.x*state.up.z,forward.x*state.up.y-forward.y*state.up.x};
        state.up=state.up*cosine+cross*sine+forward*(dot(forward,state.up)*(1.0-cosine));
        simulation.restore(state);simulation.set_level_assist(true);
        const auto localUp=simulation.radial_up();
        const auto horizon=unit(localUp-simulation.camera().forward*dot(localUp,simulation.camera().forward));
        const auto initial=angle(simulation.camera().up,horizon);
        for(int i=0;i<360;++i)
        {
            const auto old=simulation.camera();step(simulation);
            require(angle(old.up,simulation.camera().up)<=0.85/60+1e-7,"level assist exceeded original bound");
            require(angle(old.forward,simulation.camera().forward)<1e-8,"level assist erased manual pitch");
        }
        require(angle(simulation.camera().up,horizon)<initial*0.05,"radial horizon did not converge");
        const auto interpolated=simulation.interpolated_camera(0.5);
        require(core::finite(interpolated.position) && std::abs(dot(interpolated.forward,interpolated.up))<1e-10,
            "interpolated camera basis invalid");
    }
}
int main()
{
    try
    {
        guards_and_restore(); orbital_carrier_and_moon(); travel_and_radial_frame();
        std::ofstream result{"exploration-result.json"};
        result << "{\"cpu_module_pass\":true,\"assertions\":" << assertions
            << ",\"original_algorithms\":true,\"caller_owned_tick\":true,\"moon_parent_orbit\":true,"
            "\"radial_camera\":true,\"cancel_before_tick\":true,\"native_integration\":false}\n";
        return result ? 0 : 2;
    }
    catch(const std::exception& error)
    {
        std::ofstream failure{"exploration-failure.txt"}; failure << error.what();return 1;
    }
}
