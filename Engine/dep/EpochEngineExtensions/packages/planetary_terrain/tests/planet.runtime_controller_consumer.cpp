// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include "epoch/extensions/planetary_terrain/planet.runtime_bridge.hpp"
import planet.exploration_runtime;
import planet.exploration;
import planet.terrain;
import asset.mesh_artifact;
namespace host=epochengine::project_planetary_runtime;
namespace adapter=epoch::extensions::planetary_terrain::exploration_runtime;
namespace core=epoch::extensions::planetary_terrain::core;
// Standalone fixture port only. Actual host admission is a separate Engine
// integration gate; this consumer exercises the optional bridge after binding.
extern "C" bool epoch_bind_admitted_planetary_runtime(const char* root,const char* id) noexcept
{
    const auto binding=host::admitted_project_binding();
    return root&&id&&binding&&binding->project_root==root&&binding->project_id==id;
}
namespace
{
    std::size_t assertions{};
    void require(bool value,const char* message) {++assertions;if(!value)throw std::runtime_error(message);}
    std::unique_ptr<host::Controller> forbidden_factory(const host::Binding&) {throw std::runtime_error("foreign factory executed");}
    host::Result advance(host::Controller& controller,host::Input input={})
    {
        auto result=controller.advance(input);require(result.code==host::Code::ready&&host::valid_frame(result.frame),"controller frame invalid");return result;
    }
    std::string replace(std::string text,std::string_view before,std::string_view after)
    {
        const auto at=text.find(before);require(at!=std::string::npos,"fixture field absent");text.replace(at,before.size(),after);return text;
    }
    double distance(const std::array<double,3>& a,const std::array<double,3>& b)
    {
        return std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]);
    }
}
int main()
{
    try
    {
        host::Binding binding{"C:/admitted/project","Project01","sha256:"+std::string(64,'a'),host::physical_edition,1337,0,true};
        binding.water_source_hash="sha256:"+std::string(64,'c');
        binding.universe=true;
        require(!adapter::register_controller(binding),"registration before normal host binding admitted");
        require(!host::create_controller(binding),"unbound controller created");
        auto foreign=binding;foreign.edition=1u-host::physical_edition;
        require(!host::bind_admitted_project(foreign),"foreign physical edition bound");
        require(host::bind_admitted_project(binding)&&adapter::register_controller(binding),"exact linked provider refused");
        require(epoch_install_planetary_runtime(binding.project_root.c_str(),binding.project_id.c_str()),"C startup bridge refused exact admitted identity");
        require(!epoch_install_planetary_runtime("C:/foreign","Other")&&!epoch_install_planetary_runtime(nullptr,nullptr),"C bridge bypassed admission port");
        require(adapter::register_controller(binding),"same startup factory is not idempotent");
        require(!host::register_factory(binding,&forbidden_factory),"startup factory replaced");
        for(unsigned field=0;field<5;++field)
        {
            foreign=binding;
            if(field==0)foreign.project_root+="/other";
            else if(field==1)foreign.project_id="Other";
            else if(field==2)foreign.source_hash="sha256:"+std::string(64,'b');
            else if(field==3)foreign.edition=1u-host::physical_edition;
            else foreign.seed++;
            require(!host::bind_admitted_project(foreign)&&!host::register_factory(foreign,&forbidden_factory)
                &&!host::create_controller(foreign),"foreign exact binding admitted");
        }
        auto first=host::create_controller(binding),second=host::create_controller(binding);
        require(first&&second&&first.get()!=second.get(),"per-scene controllers share mutable instance");
#if defined(EPOCH_PLANET_WATER_CONTRACT)
        auto waves=first->water_provider();require(bool(waves),"admitted runtime has no real Water provider");
        const auto sample=waves->evaluate(3.25,4u*1024u*1024u);
        require(bool(sample)&&sample.tile&&sample.tile->seconds()==3.25,"runtime FFT did not sample exact time");
        require(waves->evaluate(3.25,4u*1024u*1024u).reused,"runtime FFT did not reuse unchanged tile");
        std::stop_source waterStop;waterStop.request_stop();
        require(waves->evaluate(4.0,4u*1024u*1024u,waterStop.get_token()).code==epochengine::water::wave::Code::cancelled,
            "runtime Water cancellation lost");
#endif
        // Explicit banking uses the same original camera and persistent session.
        auto banking=host::create_controller(binding);require(bool(banking),"banking controller unavailable");
        const auto unbanked=banking->capture().frame;
        host::Input bank;bank.roll=0.5;
        const auto banked=advance(*banking,bank).frame;
        require(distance(unbanked.camera,banked.camera)==0.0&&distance(unbanked.forward,banked.forward)<1e-12
            &&distance(unbanked.up,banked.up)>0.1,"roll changed position/direction or did not bank");
        const std::array right{unbanked.forward[1]*unbanked.up[2]-unbanked.forward[2]*unbanked.up[1],
            unbanked.forward[2]*unbanked.up[0]-unbanked.forward[0]*unbanked.up[2],
            unbanked.forward[0]*unbanked.up[1]-unbanked.forward[1]*unbanked.up[0]};
        require(banked.up[0]*right[0]+banked.up[1]*right[1]+banked.up[2]*right[2]>0.1,"positive roll did not bank right like Editor");
        const auto bankedSession=banking->encode_session();
        auto bankRestored=host::create_controller(binding);
        require(bankRestored->restore_session(bankedSession)
            &&distance(bankRestored->capture().frame.up,banked.up)<1e-12,"banking session restore lost up basis");
        bank.roll=-0.5;require(distance(advance(*banking,bank).frame.up,unbanked.up)<1e-12,"opposite roll did not reverse bank");
        const auto beforeInvalid=banking->encode_session();bank.roll=std::numeric_limits<double>::quiet_NaN();
        require(banking->advance(bank).code==host::Code::invalid_input&&banking->encode_session()==beforeInvalid,"non-finite roll mutated controller");
        host::Input walk;walk.command=host::Command::land;(void)advance(*banking,walk);
        const auto walking=banking->capture().frame;require(walking.mode==host::TravelMode::walk,"walk fixture did not land");bank.roll=0.5;
        require(distance(advance(*banking,bank).frame.up,walking.up)<1e-12,"manual roll changed grounded walk alignment");
        // Exercise the actual host projection with real original controller
        // frames, never float-subtract astronomical centers in the consumer.
        auto projectionController=host::create_controller(binding);
        auto projectionFrame=projectionController->capture().frame;
        const auto earthProjection=host::preview_frame(projectionFrame);
        require(earthProjection&&earthProjection->body_count==6u&&earthProjection->anchor==0u,"Earth projection refused");
        require(earthProjection->bodies[0].cloud_density&&earthProjection->bodies[2].cloud_density
            &&!earthProjection->bodies[5].cloud_density&&!earthProjection->bodies[0].cloud_mesh,
            "cloud density did not follow atmosphere-bearing body scope or duplicated mesh lobes");
        const auto retainedClouds=projectionFrame.bodies[0].cloud_density;
        auto cloudClock=host::create_controller(binding);
        for(unsigned tick=0;tick<4u;++tick)(void)advance(*cloudClock,{.elapsed_seconds=0.25});
        const auto cloudSample=cloudClock->capture().frame;
        require(cloudSample.bodies[0].cloud_wind_degrees>0.0,"simulation time did not move cloud wind");
        auto restoredCloudClock=host::create_controller(binding);
        require(restoredCloudClock->restore_session(cloudClock->encode_session())
            &&restoredCloudClock->capture().frame.bodies[0].cloud_wind_degrees==cloudSample.bodies[0].cloud_wind_degrees,
            "session restore lost cloud time");
        (void)advance(*projectionController,{.elapsed_seconds=0.25});
        require(projectionController->capture().frame.bodies[0].cloud_density==retainedClouds,
            "cloud time rebuilt immutable density recipe");
        auto invalidCloud=projectionFrame;
        invalidCloud.bodies[0].cloud_wind_degrees=std::numeric_limits<double>::infinity();
        require(!host::preview_frame(invalidCloud),"non-finite cloud wind entered the host");
        require(std::abs(earthProjection->meters_to_preview-10.0/6371000.0)<1e-20,"metric scale not derived from anchor radius");
        require(distance(earthProjection->bodies[0].center,{0,0,0})==0.0&&earthProjection->bodies[0].radius==10.0,"anchor not rebased to radius ten");
        require(std::abs(distance(earthProjection->bodies[5].center,{0,0,0})-42000000.0*earthProjection->meters_to_preview)<1e-10,"moon parent distance changed during projection");
        require(earthProjection->far_distance>64&&earthProjection->forward==projectionFrame.forward&&earthProjection->up==projectionFrame.up,"basis or system far bound lost");
        host::Input chooseMoon;chooseMoon.selected_body=5;chooseMoon.command=host::Command::orbit;
        projectionFrame=advance(*projectionController,chooseMoon).frame;
        const auto moonProjection=host::preview_frame(projectionFrame);
        require(moonProjection&&moonProjection->anchor==5&&moonProjection->bodies[5].radius==10.0,"moon anchor projection refused");
        require(distance(moonProjection->bodies[5].center,{0,0,0})==0.0,"moon anchor retained absolute center");
        require(std::abs(distance(moonProjection->bodies[0].center,{0,0,0})-42000000.0*moonProjection->meters_to_preview)<1e-10,"Earth/moon relative projection mismatch");
        const auto previousMoon=moonProjection->bodies[0].center;
        for(unsigned tick=0;tick<100;++tick)projectionFrame=advance(*projectionController,{.elapsed_seconds=0.25}).frame;
        const auto advancedProjection=host::preview_frame(projectionFrame);
        require(advancedProjection&&advancedProjection->bodies[0].center!=previousMoon,"time tick did not move parent-relative orbit");
        auto precision=projectionFrame;
        for(auto& body:precision.bodies)for(auto& coordinate:body.center)coordinate+=1e12;
        for(auto& coordinate:precision.camera)coordinate+=1e12;
        const auto translated=host::preview_frame(precision);
        require(translated&&distance(translated->camera,advancedProjection->camera)<1e-8,"camera lost precision by float conversion before rebasing");
        auto invalidProjection=projectionFrame;invalidProjection.anchor=99;
        require(!host::preview_frame(invalidProjection),"missing anchor admitted");
        invalidProjection=projectionFrame;invalidProjection.camera[0]=std::numeric_limits<double>::infinity();
        require(!host::preview_frame(invalidProjection),"nonfinite camera admitted");
        invalidProjection=projectionFrame;invalidProjection.bodies[5].radius=0;
        require(!host::preview_frame(invalidProjection),"zero-radius anchor admitted");
        invalidProjection=projectionFrame;invalidProjection.up=invalidProjection.forward;
        require(!host::preview_frame(invalidProjection),"parallel camera basis admitted");
        auto frame=first->capture().frame;
        require(host::valid_frame(frame)&&frame.body_count==6&&frame.bodies[5].name=="SELENE"&&frame.bodies[5].parent==0,
            "moon is not an actual original body");
        core::Universe world{binding.seed};
        for(unsigned body=0;body<frame.body_count;++body)
        {
            const auto& artifact=*frame.bodies[body].coarse_mesh;
            require(artifact.vertices().size()>=6u*81u&&artifact.indices().size()>=6u*8u*8u*6u,"real six-face body geometry absent");
            auto sourceBody=world.body(body);sourceBody.ocean=body==0u;
            const auto expected=core::build_tile(sourceBody,{0,0,static_cast<std::uint8_t>(body),0,0},{8,false,false});
            const auto point=(expected.anchor+core::DVec3{expected.vertices.front().position})*(10.0/sourceBody.radius);
            const auto actual=artifact.vertices().front().position;
            require(std::abs(actual[0]-point.x)<1e-5&&std::abs(actual[1]-point.y)<1e-5&&std::abs(actual[2]-point.z)<1e-5,
                "coarse body is not the reviewed terrain sampler");
            require(frame.bodies[body].radius==world.body(body).radius,"metric radius lost");
        }
        const auto oldMesh=frame.bodies[5].coarse_mesh;
        host::Input orbit;orbit.selected_body=5;orbit.command=host::Command::orbit;
        frame=advance(*first,orbit).frame;
        require(frame.anchor==5&&frame.target==5&&frame.bodies[5].coarse_mesh==oldMesh,"orbit command remeshed or lost moon");
        require(second->capture().frame.anchor==0,"one context changed another controller");
        host::Input faster;faster.time_scale=1000;faster.elapsed_seconds=1.0/60.0;
        const auto previousCenter=frame.bodies[5].center;
        frame=advance(*first,faster).frame;
        require(std::abs(frame.seconds-1000.0/60)<1e-9&&distance(previousCenter,frame.bodies[5].center)>100,
            "original orbital time/moon motion is disconnected");
        host::Input pause;pause.command=host::Command::toggle_time_pause;advance(*first,pause);
        const auto paused=first->capture().frame;
        host::Input move;move.elapsed_seconds=1.0/60;move.forward=1;
        advance(*first,move);frame=advance(*first,move).frame;
        require(frame.seconds==paused.seconds&&distance(frame.camera,paused.camera)>0,"paused orbital time froze camera");
        const auto session=first->encode_session();
        require(!session.empty()&&session.size()<16384&&session.find(binding.project_root)==std::string::npos,
            "session missing or bound to absolute source path");
        require(session==first->encode_session(),"same state encoding is nondeterministic");
        foreign=binding;foreign.project_root="C:/relocated/project";
        auto copied=adapter::make_controller(foreign);
        require(copied&&copied->restore_session(session),"relocated admitted project cannot restore same session");
        require(copied->capture().frame.anchor==5&&copied->capture().frame.seconds==frame.seconds,"restored moon/time changed");
        const auto stable=copied->encode_session();
        for(const auto& malformed:std::array{
            replace(session,"world=EpochPlanet0.2.3","world=Other"),replace(session,"project_id=Project01","project_id=Other"),
            replace(session,binding.source_hash,"sha256:"+std::string(64,'b')),replace(session,"seed=1337","seed=1338"),
            replace(session,"mode=0","mode=9"),replace(session,"anchor=5","anchor=99"),
            replace(session,"seconds=","seconds=nan"),session+"unknown=1\n",session.substr(0,session.size()-1),std::string(16385,'x')})
        {
            require(!copied->restore_session(malformed),"malformed or foreign session restored");
            require(copied->encode_session()==stable,"invalid restore changed previous state");
        }
        const auto before=first->encode_session();
        host::Input invalid;invalid.elapsed_seconds=-1;invalid.command=host::Command::orbit;
        require(first->advance(invalid).code==host::Code::invalid_input&&first->encode_session()==before,
            "invalid elapsed time changed camera");
        invalid={};invalid.yaw=std::numeric_limits<double>::quiet_NaN();
        require(first->advance(invalid).code==host::Code::invalid_input&&first->encode_session()==before,"nonfinite look admitted");
        std::stop_source stop;stop.request_stop();
        require(first->advance(orbit,stop.get_token()).code==host::Code::cancelled&&first->encode_session()==before,"cancelled command mutated state");
        auto landInput=host::Input{};landInput.selected_body=4;landInput.command=host::Command::land;
        require(first->advance(landInput).code==host::Code::invalid_input&&first->encode_session()==before,"gas landing changed prior session");
        landInput.selected_body=0;frame=advance(*first,landInput).frame;
        require(frame.mode==host::TravelMode::walk&&std::abs(frame.altitude-1.8)<1e-5,"original landing absent");
        require(copied->restore_session(first->encode_session())&&copied->capture().frame.mode==host::TravelMode::walk,
            "landed session lost walking mode");
        host::Input cruise;cruise.selected_body=1;cruise.command=host::Command::toggle_cruise;
        frame=advance(*first,cruise).frame;
        require(frame.mode==host::TravelMode::cruise&&copied->restore_session(first->encode_session())
            &&copied->capture().frame.mode==host::TravelMode::cruise&&copied->capture().frame.target==1,
            "cruise session lost travel mode or destination");
        auto invalidFrame=frame;invalidFrame.seconds=1e12;
        require(!host::valid_frame(invalidFrame),"out-of-range orbital time admitted");
        invalidFrame=frame;invalidFrame.up=invalidFrame.forward;
        require(!host::valid_frame(invalidFrame),"degenerate camera frame admitted");
        auto a=adapter::make_controller(binding),b=adapter::make_controller(binding);
        host::Input input;input.forward=1;input.elapsed_seconds=1.0/60;
        for(unsigned i=0;i<60;++i)advance(*a,input);
        input.elapsed_seconds=1.0/144;
        for(unsigned i=0;i<144;++i)advance(*b,input);
        require(distance(a->capture().frame.camera,b->capture().frame.camera)<1e-5,"fixed ticks vary with display cadence");
        // Custom physical size survives the real optional controller codec;
        // a session from another size must never silently change this binding.
        auto customBinding=binding;customBinding.reference_radius_meters=25000;
        auto custom=adapter::make_controller(customBinding),customCopy=adapter::make_controller(customBinding);
        require(custom&&customCopy,"custom world controller refused");
        auto customFrame=custom->capture().frame;
        require(host::valid_frame(customFrame)&&customFrame.bodies[0].radius==25000,"coarse runtime ignored world size");
        require(customFrame.bodies[0].coarse_mesh&&customFrame.bodies[5].coarse_mesh,"custom coarse worlds missing");
        host::Input customLand;customLand.command=host::Command::land;customLand.selected_body=0;
        customFrame=advance(*custom,customLand).frame;
        require(customFrame.mode==host::TravelMode::walk&&std::abs(customFrame.altitude-1.8)<1e-7,"custom landing changed player height");
        const auto sizedSession=custom->encode_session();
        require(sizedSession.starts_with("epoch.planet.exploration.v2\n")
            &&sizedSession.find("size_reference_body=0\nreference_radius_meters=25000\n")!=std::string::npos,
            "custom session lacks exact physical size");
        require(customCopy->restore_session(sizedSession),"custom session failed restore");
        const auto restoredCustom=customCopy->capture().frame;
        require(restoredCustom.bodies[0].radius==25000&&restoredCustom.mode==customFrame.mode
            &&distance(restoredCustom.camera,customFrame.camera)<1e-8
            &&distance(restoredCustom.forward,customFrame.forward)<1e-12,"custom session changed physical state");
        const auto unchanged=customCopy->encode_session();
        require(!customCopy->restore_session(session)&&customCopy->encode_session()==unchanged,"legacy session replaced custom world");
        require(!first->restore_session(sizedSession),"custom session changed default world");
        require(!customCopy->restore_session(replace(sizedSession,"reference_radius_meters=25000","reference_radius_meters=26000"))
            &&customCopy->encode_session()==unchanged,"foreign size session admitted");
        require(!customCopy->restore_session(replace(sizedSession,"size_reference_body=0","size_reference_body=6"))
            &&customCopy->encode_session()==unchanged,"invalid reference body admitted");
        auto invalidBinding=customBinding;invalidBinding.reference_radius_meters=249;
        require(!adapter::make_controller(invalidBinding),"invalid physical size constructed runtime");
        require(!first->capture().frame.sunlight&&!custom->capture().frame.sunlight,"legacy binding silently enabled solar light");
        auto solarBinding=customBinding;solarBinding.original_sunlight=true;
        auto solar=adapter::make_controller(solarBinding),solarCopy=adapter::make_controller(solarBinding);
        require(solar&&solarCopy,"solar controller refused exact binding");
        for(std::uint32_t body:std::array{0u,5u,3u})
        {
            host::Input choose;choose.command=host::Command::orbit;choose.selected_body=body;
            auto illuminated=advance(*solar,choose).frame;
            for(unsigned instant=0;instant<3;++instant)
            {
                require(illuminated.sunlight&&host::valid_sunlight(*illuminated.sunlight),"enabled sun frame missing");
                const auto length=std::hypot(illuminated.camera[0],illuminated.camera[1],illuminated.camera[2]);
                for(unsigned axis=0;axis<3;++axis)
                    require(std::abs(illuminated.sunlight->direction_to_sun[axis]+illuminated.camera[axis]/length)<1e-15,
                        "solar direction differs from original interpolated-camera shader");
                require(illuminated.sunlight->color==std::array<float,3>{1,1,1}&&illuminated.sunlight->intensity==1.12f,
                    "original solar diffuse radiance changed");
                illuminated=advance(*solar,{.elapsed_seconds=.25,.time_scale=1000}).frame;
            }
        }
        const auto solarSession=solar->encode_session();
        require(solarSession.starts_with("epoch.planet.exploration.v3\n")&&solarSession.find("original_sunlight=1\n")!=std::string::npos,
            "solar opt-in absent from session");
        require(solarCopy->restore_session(solarSession)&&solarCopy->capture().frame.sunlight,"solar session not restored");
        require(!solarCopy->restore_session(sizedSession)&&!customCopy->restore_session(solarSession),"solar mode changed across session binding");
        const auto solarStable=solarCopy->encode_session();
        require(!solarCopy->restore_session(replace(solarSession,"original_sunlight=1","original_sunlight=0"))
            &&solarCopy->encode_session()==solarStable,"false solar session admitted or changed prior state");
        auto invalidSolar=solar->capture().frame;invalidSolar.sunlight->direction_to_sun={0,0,0};
        require(!host::valid_frame(invalidSolar),"degenerate solar frame admitted");
        // Planet and Universe share one controller and simulation, but the
        // admitted recipe must decide their inventory, physics and sessions.
        auto planetBinding=binding;planetBinding.universe=false;
        require(planetBinding!=binding,"coverage scope absent from binding identity");
        require(!host::create_controller(planetBinding),"foreign coverage scope reused registered controller");
        for(std::uint32_t selected:std::array{0u,5u})
        {
            planetBinding.initial_body=selected;planetBinding.ocean_surface=false;planetBinding.water_source_hash.clear();
            auto planet=adapter::make_controller(planetBinding),planetCopy=adapter::make_controller(planetBinding);
            require(planet&&planetCopy,"selected Planet controller unavailable");
            const auto selectedFrame=planet->capture().frame;
            require(host::valid_frame(selectedFrame)&&selectedFrame.body_count==1u
                &&selectedFrame.anchor==selected&&selectedFrame.target==selected
                &&selectedFrame.bodies[0].id==selected&&selectedFrame.bodies[0].parent==-1,
                "Planet emitted extra bodies or retained an absent parent");
            require(!selectedFrame.bodies[1].coarse_mesh,"Planet built an unselected body");
            const auto selectedPreview=host::preview_frame(selectedFrame);
            require(selectedPreview&&selectedPreview->body_count==1u&&selectedPreview->anchor==selected,
                "nonzero selected-body projection refused");
            host::Input hidden;hidden.selected_body=selected==0u?5u:0u;hidden.command=host::Command::orbit;
            const auto stablePlanet=planet->encode_session();
            require(stablePlanet.starts_with("epoch.planet.exploration.v4\n")
                &&stablePlanet.find("universe=0\n")!=std::string::npos,"Planet session lacks scope");
            require(planet->advance(hidden).code==host::Code::invalid_input&&planet->encode_session()==stablePlanet,
                "Planet selected an absent body or mutated rejected input");
            require(planetCopy->restore_session(stablePlanet),"scoped Planet session did not restore");
            const auto restoredPlanet=planetCopy->encode_session();
            require(!planetCopy->restore_session(session)&&planetCopy->encode_session()==restoredPlanet,
                "Universe session crossed into Planet scope");
            require(!first->restore_session(stablePlanet),"Planet session crossed into Universe scope");
            require(!planetCopy->restore_session(replace(stablePlanet,"universe=0","universe=1"))
                &&planetCopy->encode_session()==restoredPlanet,"foreign explicit scope admitted");
            const auto foreignAnchor="anchor="+std::to_string(selected);
            require(!planetCopy->restore_session(replace(stablePlanet,foreignAnchor,"anchor=4"))
                &&planetCopy->encode_session()==restoredPlanet,"Planet restored an absent carrier");
            host::Input land;land.command=host::Command::land;
            const auto onSurface=advance(*planet,land).frame;
            require(onSurface.body_count==1u&&onSurface.anchor==selected&&onSurface.mode==host::TravelMode::walk,
                "selected Planet landing changed coverage");
        }
        namespace simulation=epoch::extensions::planetary_terrain::exploration;
        simulation::Simulation scoped{binding.seed,{},std::optional<std::size_t>{0u}},unscoped{binding.seed};
        auto nearMoon=scoped.state();nearMoon.time_scale=0;
        const auto moon=scoped.world().body(5u);
        nearMoon.local_position=scoped.world().center(5u,0)-scoped.world().center(0u,0)
            +core::DVec3{moon.radius+10000,0,0};
        scoped.restore(nearMoon);unscoped.restore(nearMoon);
        require(core::length(scoped.gravity())<1e-12&&core::length(unscoped.gravity())>0.01,
            "Planet retained an absent body's gravity");
        scoped.step(1.0/60,{});unscoped.step(1.0/60,{});
        require(scoped.anchor()==0u&&unscoped.anchor()==5u,"Planet automatically reanchored to an absent body");
        bool rejected{};try{scoped.select(5u);}catch(const std::out_of_range&){rejected=true;}
        require(rejected&&scoped.target()==0u,"scoped simulation selected an absent body");
        std::ofstream result{"controller-result.json"};result<<"{\"pass\":true,\"assertions\":"<<assertions
            <<",\"six_real_body_meshes\":true,\"moon_orbits\":true,\"session_codec\":true,\"native_host\":false}\n";
        return result?0:2;
    }
    catch(const std::exception& error){std::ofstream file{"controller-failure.txt"};file<<error.what();return 1;}
}
