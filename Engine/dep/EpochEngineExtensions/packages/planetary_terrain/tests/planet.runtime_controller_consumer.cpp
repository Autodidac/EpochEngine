// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include "epoch/extensions/planetary_terrain/planet.runtime_bridge.hpp"
import planet.exploration_runtime;
import planet.exploration;
import planet.terrain;
import asset.mesh_artifact;
import project.planetary_snapshot;
namespace host=epochengine::project_planetary_runtime;
namespace adapter=epoch::extensions::planetary_terrain::exploration_runtime;
namespace core=epoch::extensions::planetary_terrain::core;
namespace snapshots=epochengine::project_planetary_snapshot;
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
#if defined(EPOCH_PLANET_WATER_CONTRACT)
    void check_linked_wet_cuts(host::Binding binding,const core::Universe& world)
    {
        using Status=host::TerrainStatus;
        binding.universe=false;
        auto controller=adapter::make_controller(binding);
        require(bool(controller),"wet single-body controller unavailable");
        const host::TerrainProfile profile{4u,17u,180u,64u,210.0,0.15,{}};
        const std::string recipe_key(64,'d');
        auto terrain=controller->create_wet_terrain_provider(profile,recipe_key);
        auto waves=controller->water_provider();
        require(terrain&&waves,"linked wet terrain or its FFT provider unavailable");
        constexpr double wave_seconds=32.06666666666613;
        constexpr std::uint64_t tile_bytes=4u*1024u*1024u;
        const auto sample=waves->evaluate(wave_seconds,tile_bytes);
        require(sample&&sample.tile&&sample.tile->seconds()==wave_seconds,"linked wet test did not obtain its exact FFT sample");

        // Use the established Verdant shoreline fixture at two flight heights.
        const auto coast=core::tile_direction({4096u,131041u,0u,0u,17u},0.5,0.5);
        const auto up=core::tangent(coast);
        const auto center=world.center(0,0.0);
        const auto surface=world.ground_radius(0,coast);
        const auto camera_at=[&](double height) {
            const auto camera=center+coast*((std::max)(world.body(0).radius,surface)+height);
            return std::array{camera.x,camera.y,camera.z};
        };
        host::TerrainView view{1u,camera_at(130.0),{-coast.x,-coast.y,-coast.z},
            {up.x,up.y,up.z},0.0,1.0471975511965976,1080.0/2211.0,2211u};
        const auto unpublished=[](const host::TerrainBatch& batch) {
            return !batch.artifact&&!batch.aquatic_source&&!batch.water_appearance;
        };
        const auto missing=terrain->step(view,{});
        require(missing.status==Status::invalid&&unpublished(missing),"wet terrain accepted a missing FFT sample");
        view.water_sample=sample.tile;
        const auto settled=[&] {
            for(unsigned step=0;step<24u;++step)
            {
                ++view.request_id;
                const auto batch=terrain->step(view,{});
                require(batch.request_id==view.request_id&&batch.built<=16u,"linked wet work exceeded its batch");
                if(batch.status==Status::ready)
                {
                    require(!batch.pending&&batch.artifact&&batch.aquatic_source&&batch.water_appearance
                        &&batch.draw_patches>=6u,"linked wet terrain published an incomplete cut");
                    const auto& source=*batch.aquatic_source;
                    require(source.artifact&&source.metadata.identity.artifact_key==source.artifact->key()
                        &&source.metadata.identity.request_id==batch.request_id
                        &&source.metadata.identity.source_hash==binding.source_hash
                        &&source.metadata.identity.recipe_key==recipe_key
                        &&source.metric_origin_meters==batch.source_origin_meters
                        &&snapshots::valid_metadata(source.metadata,*source.artifact,{.artifact=profile.artifact}),
                        "linked wet source did not preserve complete coverage or exact identity");
                    require(source.water_sample_seconds==view.water_sample->seconds()
                        &&source.water_sample_seconds!=view.seconds&&source.metadata.water_radius_meters>0.0
                        &&!source.metadata.water_vertices.empty()&&source.metadata.water_appearance
                        &&source.metadata.water_appearance->artifact_key==source.artifact->key()
                        &&batch.water_appearance->artifact_key==batch.artifact->key()
                        &&!batch.water_appearance->ranges.empty(),"linked wet cut lost its independent phase or water appearance");
                    return batch;
                }
                if(batch.status!=Status::pending||!unpublished(batch))
                    throw std::runtime_error("wet footprint stopped with status "
                        +std::to_string(static_cast<unsigned>(batch.status))+" at "+std::string{batch.failure});
            }
            throw std::runtime_error("linked wet terrain did not settle inside the step bound");
        };
        const auto near=settled();
        const auto stale=terrain->step(view,{});
        require(stale.status==Status::invalid&&unpublished(stale),"stale wet request attempted publication");
        ++view.request_id;
        std::stop_source stopped;
        stopped.request_stop();
        const auto cancelled=terrain->step(view,stopped.get_token());
        require(cancelled.status==Status::cancelled&&unpublished(cancelled),"cancelled wet request attempted publication");
        const auto retained=terrain->step(view,{});
        require(retained.status==Status::ready&&retained.artifact==near.artifact
            &&retained.aquatic_source==near.aquatic_source&&retained.water_appearance==near.water_appearance,
            "cancelled wet request discarded the complete displayed cut");

        view.world_camera_meters=camera_at(6500.0);
        const auto raised=settled();
        require(raised.artifact->key()!=near.artifact->key()&&raised.generation>near.generation
            &&raised.aquatic_source->water_sample_seconds==near.aquatic_source->water_sample_seconds,
            "wet topology replacement did not keep the exact displayed phase");

        const auto next=waves->evaluate(wave_seconds+0.25,tile_bytes);
        require(next&&next.tile&&next.tile->seconds()==wave_seconds+0.25,"FFT provider did not advance the sample");
        view.water_sample=next.tile;
        const auto advanced=settled();
        require(!advanced.built&&advanced.artifact->key()!=raised.artifact->key()
            &&advanced.source_origin_meters==raised.source_origin_meters
            &&advanced.aquatic_source->water_sample_seconds==wave_seconds+0.25
            &&raised.aquatic_source->water_sample_seconds==wave_seconds,
            "new FFT phase rebuilt terrain or mutated the previously presented cut");
    }
#endif
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
        require(!first->create_dry_terrain_provider({}),"wet or universe controller granted dry single-body terrain");
        auto ground_cover=second->ground_cover_provider();
        second.reset();
        require(bool(ground_cover),"admitted runtime has no ground cover provider");
        const core::Universe cover_world{binding.seed};
        std::optional<host::GroundCoverDrawPatch> grass;
        std::array<double,3> grass_direction{};
        for(unsigned index=0;index<64u&&!grass;++index)
        {
            const double y=-0.8+1.6*(index+0.5)/64.0;
            const double angle=index*2.399963229728653;
            const double planar=std::sqrt(1.0-y*y);
            const core::DVec3 direction{planar*std::cos(angle),y,planar*std::sin(angle)};
            const auto terrain=core::sample_surface(cover_world.body(0),direction);
            if(terrain.height<20.0||terrain.moisture<0.45||terrain.temperature<0.30)continue;
            grass_direction={direction.x,direction.y,direction.z};
            auto candidate=ground_cover->build_patch(0,grass_direction,{});
            if(candidate&&candidate->mesh)grass=std::move(candidate);
        }
        require(grass&&grass->mesh->vertices().size()<=3456u,"ground cover did not yield bounded drawable geometry");
        const auto repeated_grass=ground_cover->build_patch(0,grass_direction,{});
        require(repeated_grass&&repeated_grass->mesh&&repeated_grass->mesh->key()==grass->mesh->key()
            &&repeated_grass->body_local_anchor_meters==grass->body_local_anchor_meters,"ground cover patch is unstable");
        std::stop_source grass_stop;
        grass_stop.request_stop();
        require(!ground_cover->build_patch(0,grass_direction,grass_stop.get_token())
            &&!ground_cover->build_patch(99,grass_direction,{}),"ground cover bypassed body or cancellation gate");
        const std::array verdant_capture{3755514.6242091493,3753616.1154924748,3520829.416996601};
        const auto capture_patch=ground_cover->build_patch(0,verdant_capture,{});
        const auto neighborhood=ground_cover->build_neighborhood(0,verdant_capture,{});
        require(capture_patch&&capture_patch->mesh&&neighborhood&&neighborhood->size()==9u
            &&neighborhood->front().mesh&&neighborhood->front().mesh->key()==capture_patch->mesh->key()
            &&neighborhood->front().body_local_anchor_meters==capture_patch->body_local_anchor_meters,
            "ground cover neighborhood lost the exact Verdant home patch");
        for(const auto& patch:*neighborhood)
            require(patch.mesh&&patch.mesh->vertices().size()<=3456u
                &&std::ranges::count_if(*neighborhood,[&patch](const auto& other) {
                    return patch.body_local_anchor_meters==other.body_local_anchor_meters;
                })==1,"ground cover neighborhood repeated a patch or exceeded its mesh budget");
        const auto cancelled_neighborhood=ground_cover->build_neighborhood(0,verdant_capture,grass_stop.get_token());
        const auto unadmitted_neighborhood=ground_cover->build_neighborhood(99,verdant_capture,{});
        const auto non_temperate_neighborhood=ground_cover->build_neighborhood(1,verdant_capture,{});
        require(cancelled_neighborhood&&cancelled_neighborhood->empty()
            &&unadmitted_neighborhood&&unadmitted_neighborhood->empty()
            &&non_temperate_neighborhood&&non_temperate_neighborhood->empty(),
            "an admitted provider returned partial or unhandled grass neighborhood results");
        const auto visible=ground_cover->build_visible_neighborhood(0,verdant_capture,2.8,{});
        require(visible&&visible->size()>9u&&visible->size()<=host::maximum_visible_ground_cover_patches,
            "ground cover view did not extend beyond the original square");
        const auto original_direction=core::normalized(core::DVec3{verdant_capture[0],verdant_capture[1],verdant_capture[2]});
        const auto nearby=original_direction+core::tangent(original_direction)
            *(host::ground_cover_view_update_meters/cover_world.body(0).radius);
        const auto moved_cover=ground_cover->build_visible_neighborhood(0,{nearby.x,nearby.y,nearby.z},2.8,{});
        require(moved_cover&&moved_cover->size()>9u,"moving the view retired visible grass");
        const auto recovered=ground_cover->build_visible_neighborhood(0,verdant_capture,2.8,{});
        require(recovered&&recovered->size()==visible->size(),"return to a grass pose lost retained patches");
        for(std::size_t i=0u;i<visible->size();++i)
        {
            const auto& before=(*visible)[i];
            const auto& after=(*recovered)[i];
            require(before.mesh&&after.mesh&&before.mesh->key()==after.mesh->key()
                &&before.body_local_anchor_meters==after.body_local_anchor_meters,"moving the view reseeded grass");
        }
        const auto above=ground_cover->build_visible_neighborhood(0,verdant_capture,32.0,{});
        const auto stopped_cover=ground_cover->build_visible_neighborhood(0,verdant_capture,2.8,grass_stop.get_token());
        require(above&&above->empty()&&stopped_cover&&stopped_cover->empty(),
            "grass view published faded or cancelled geometry");
        second=host::create_controller(binding);
        require(bool(second),"second controller could not be reopened");
#if defined(EPOCH_PLANET_WATER_CONTRACT)
        auto waves=first->water_provider();require(bool(waves),"admitted runtime has no real Water provider");
        const auto sample=waves->evaluate(3.25,4u*1024u*1024u);
        require(bool(sample)&&sample.tile&&sample.tile->seconds()==3.25,"runtime FFT did not sample exact time");
        require(waves->evaluate(3.25,4u*1024u*1024u).reused,"runtime FFT did not reuse unchanged tile");
        std::stop_source waterStop;waterStop.request_stop();
        require(waves->evaluate(4.0,4u*1024u*1024u,waterStop.get_token()).code==epochengine::water::wave::Code::cancelled,
            "runtime Water cancellation lost");
        check_linked_wet_cuts(binding,cover_world);
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
        auto surface=host::create_controller(binding);
        const core::Universe spawnWorld{binding.seed};
        std::optional<core::DVec3> dry,wet;
        for (std::uint8_t face=0;face<6;++face)
            for(int x=0;x<11;++x)for(int y=0;y<11;++y)
            {
                const auto direction=core::cube_direction(face,double(x)/5.0-1.0,double(y)/5.0-1.0);
                const auto elevation=core::sample_height(spawnWorld.body(0),direction);
                if(!dry&&elevation>10)dry=direction;
                if(!wet&&elevation<-10)wet=direction;
            }
        require(bool(dry)&&bool(wet),"surface-start fixtures did not find dry and wet ground");
        const auto localStart=*dry*spawnWorld.ground_radius(0,*dry);
        const auto localHeading=core::tangent(*dry);
        host::SurfaceStart start{.body=0,.body_local_position_meters={localStart.x,localStart.y,localStart.z},
            .body_local_forward={localHeading.x,localHeading.y,localHeading.z}};
        const auto initialSession=surface->encode_session();
        const auto submerged=*wet*spawnWorld.ground_radius(0,*wet);
        auto invalidStart=start;invalidStart.body_local_position_meters={submerged.x,submerged.y,submerged.z};
        require(surface->start_on_surface(invalidStart).code==host::Code::invalid_input,"submerged authored start admitted");
        invalidStart=start;invalidStart.body=4;
        require(surface->start_on_surface(invalidStart).code==host::Code::invalid_input,"gas giant authored start admitted");
        invalidStart=start;for(auto& axis:invalidStart.body_local_position_meters)axis*=2;
        require(surface->start_on_surface(invalidStart).code==host::Code::invalid_input&&surface->encode_session()==initialSession,
            "distant authored start admitted or rejected start changed the session");
        const auto surfaceStart=surface->start_on_surface(start);
        const auto expectedEye=spawnWorld.center(0,0)+localStart+*dry*1.8;
        require(surfaceStart.code==host::Code::ready&&surfaceStart.frame.mode==host::TravelMode::walk
            &&surfaceStart.frame.anchor==0&&surfaceStart.frame.target==0&&host::valid_frame(surfaceStart.frame),"valid authored start refused");
        require(distance(surfaceStart.frame.camera,{expectedEye.x,expectedEye.y,expectedEye.z})<.001
            &&distance(surfaceStart.frame.forward,start.body_local_forward)<1e-8,"authored start moved away or lost heading");
        const auto savedStart=surface->encode_session();
        auto restoredStart=host::create_controller(binding);
        require(restoredStart->restore_session(savedStart)
            &&distance(restoredStart->capture().frame.camera,surfaceStart.frame.camera)<.001,"walking start did not restore");

        auto flight=host::create_controller(binding);
        require(bool(flight),"flight-start controller unavailable");
        const auto flightBefore=flight->capture().frame;
        const auto flightLocal=*wet*(spawnWorld.body(0).radius+132.125);
        const host::FlightStart authoredFlight{.body=0,
            .body_local_position_meters={flightLocal.x,flightLocal.y,flightLocal.z},
            .body_local_forward={0,-.6,-.8},.body_local_up={.6,.64,-.48}};
        const auto flightStart=flight->start_in_flight(authoredFlight);
        require(flightStart.code==host::Code::ready&&host::valid_frame(flightStart.frame)
            &&flightStart.frame.mode==host::TravelMode::flight&&flightStart.frame.anchor==0u
            &&flightStart.frame.target==0u,"valid authored flight pose refused");
        const auto flightEye=spawnWorld.center(0,flightStart.frame.seconds)+flightLocal;
        require(distance(flightStart.frame.camera,{flightEye.x,flightEye.y,flightEye.z})<1e-6
            &&distance(flightStart.frame.forward,authoredFlight.body_local_forward)<1e-12
            &&distance(flightStart.frame.up,authoredFlight.body_local_up)<1e-12
            &&flightStart.frame.seconds==flightBefore.seconds&&flightStart.frame.time_scale==flightBefore.time_scale
            &&flightStart.frame.gravity_enabled==flightBefore.gravity_enabled
            &&flightStart.frame.level_assist==flightBefore.level_assist,"authored flight eye or rolled basis changed");
        const auto sampledFlight=advance(*flight).frame;
        require(distance(sampledFlight.camera,flightStart.frame.camera)<1e-6
            &&distance(sampledFlight.forward,flightStart.frame.forward)<1e-12
            &&distance(sampledFlight.up,flightStart.frame.up)<1e-12,"fresh flight interpolation sampled the old orbit");
        const auto flightSession=flight->encode_session();
        require(!flightSession.empty(),"authored flight was not serializable");

        auto waterContact=host::create_controller(binding);
        require(bool(waterContact),"water contact controller unavailable");
        const double seaRadius=spawnWorld.body(0).radius;
        const auto nearWater=*wet*(seaRadius+2.0);
        const auto waterHeading=core::tangent(*wet);
        const auto aboveWater=waterContact->start_in_flight({.body=0,
            .body_local_position_meters={nearWater.x,nearWater.y,nearWater.z},
            .body_local_forward={waterHeading.x,waterHeading.y,waterHeading.z},
            .body_local_up={wet->x,wet->y,wet->z}});
        require(aboveWater.code==host::Code::ready&&std::abs(aboveWater.frame.altitude-0.2)<1e-6,
            "water clearance above the surface is incorrect");
        const auto atWater=advance(*waterContact,{.elapsed_seconds=0.05,.rise=-1.0,.time_scale=0.0}).frame;
        const auto waterEye=spawnWorld.center(0,atWater.seconds)+*wet*(seaRadius+1.8);
        require(atWater.mode==host::TravelMode::flight&&std::abs(atWater.altitude)<1e-6
            &&distance(atWater.camera,{waterEye.x,waterEye.y,waterEye.z})<1e-6,
            "water contact should read zero while preserving camera clearance");

        for(unsigned invalid=0;invalid<6u;++invalid)
        {
            auto bad=authoredFlight;
            if(invalid==0u)bad.body=99u;
            else if(invalid==1u)bad.body_local_position_meters[0]=std::numeric_limits<double>::infinity();
            else if(invalid==2u)bad.body_local_forward={};
            else if(invalid==3u)bad.body_local_up=bad.body_local_forward;
            else if(invalid==4u)bad.body_local_up[1]=std::numeric_limits<double>::quiet_NaN();
            else
            {
                const auto inside=*wet*(spawnWorld.body(0).radius+.75);
                bad.body_local_position_meters={inside.x,inside.y,inside.z};
            }
            require(flight->start_in_flight(bad).code==host::Code::invalid_input
                &&flight->encode_session()==flightSession,"invalid flight pose replaced the active session");
        }
        auto restoredFlight=host::create_controller(binding);
        require(restoredFlight&&restoredFlight->restore_session(flightSession),"saved flight pose did not restore");
        const auto restoredFlightFrame=restoredFlight->capture().frame;
        require(distance(restoredFlightFrame.camera,flightStart.frame.camera)<1e-6
            &&distance(restoredFlightFrame.forward,flightStart.frame.forward)<1e-12
            &&distance(restoredFlightFrame.up,flightStart.frame.up)<1e-12,"flight restore changed the authored pose");
        for(unsigned tick=0;tick<3;++tick)(void)advance(*surface,{.elapsed_seconds=1.0/60.0,.forward=1});
        require(distance(surface->capture().frame.camera,surfaceStart.frame.camera)>.1,"authored start could not walk");

        auto collision=host::create_controller(binding);
        require(collision->start_on_surface(start).code==host::Code::ready,"collision start refused");
        const auto localEye=[](const host::Frame& frame){
            const auto& center=frame.bodies[0].center;
            return std::array{frame.camera[0]-center[0],frame.camera[1]-center[1],frame.camera[2]-center[2]};
        };
        const auto boxCenter=localStart+localHeading*2.5+*dry*.8;
        const auto localRight=core::normalized(core::cross(*dry,localHeading));
        host::WalkBox obstacle{.body=0,.center_meters={boxCenter.x,boxCenter.y,boxCenter.z},
            .half_extent_meters={.85,.8,.07},
            .rotation={localRight.x,dry->x,localHeading.x,localRight.y,dry->y,localHeading.y,localRight.z,dry->z,localHeading.z}};
        require(collision->set_walk_boxes(std::array{obstacle}),"rotated beacon box refused");
        auto malformedBox=obstacle;malformedBox.rotation[0]=std::numeric_limits<double>::infinity();
        require(!collision->set_walk_boxes(std::array{malformedBox}),"invalid box replaced admitted collider");
        const auto blocked=advance(*collision,{.elapsed_seconds=.25,.forward=1,.boost=true,.speed_multiplier=100}).frame;
        require(distance(localEye(blocked),localEye(surfaceStart.frame))<.1&&blocked.seconds>surfaceStart.frame.seconds,
            "swept walking tunneled through the thin box or stopped simulation time");
        // An exact tick boundary renders the preceding physics pose.
        const auto around=advance(*collision,{.elapsed_seconds=2.0/60.0,.right=1}).frame;
        require(distance(localEye(around),localEye(blocked))>.1,"walking could not move around the obstacle");
        require(collision->start_on_surface(start).code==host::Code::ready&&collision->set_walk_boxes({}),"collision removal refused");
        const auto clearWalk=advance(*collision,{.elapsed_seconds=2.0/60.0,.forward=1,.boost=true}).frame;
        require(distance(localEye(clearWalk),localEye(surfaceStart.frame))>2.6,"removing the box left a stale obstacle");
        // Exercise the actual host projection with real original controller
        // frames, never float-subtract astronomical centers in the consumer.
        auto projectionController=host::create_controller(binding);
        auto projectionFrame=projectionController->capture().frame;
        const auto earthProjection=host::preview_frame(projectionFrame);
        require(earthProjection&&earthProjection->body_count==6u&&earthProjection->anchor==0u,"Earth projection refused");
        require(earthProjection->bodies[0].cloud_density&&earthProjection->bodies[2].cloud_density
            &&!earthProjection->bodies[5].cloud_density&&!earthProjection->bodies[0].cloud_mesh,
            "cloud density did not follow atmosphere-bearing body scope or duplicated mesh lobes");
        require(!earthProjection->bodies[1].cloud_density
            &&earthProjection->bodies[1].atmosphere_height==55'000.0*earthProjection->meters_to_preview
            &&earthProjection->bodies[5].atmosphere_height==0.0,
            "clear Ember atmosphere or airless Selene was lost during projection");
        auto invalidAtmosphere=projectionFrame;
        invalidAtmosphere.bodies[0].atmosphere_height_meters=std::numeric_limits<double>::infinity();
        require(!host::preview_frame(invalidAtmosphere),"non-finite atmosphere height entered the host");
        const auto retainedClouds=projectionFrame.bodies[0].cloud_density;
        require(retainedClouds&&retainedClouds->view_occluder_radius_meters==6'349'751.0
            &&earthProjection->bodies[0].cloud_density==retainedClouds,
            "metric solid radius changed in exploration preview");
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
        require(frame.mode==host::TravelMode::walk&&std::abs(frame.altitude)<1e-5,"landing altitude is not at the surface");
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
        require(customFrame.mode==host::TravelMode::walk&&std::abs(customFrame.altitude)<1e-7,
            "custom landing altitude is not at the surface");
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
        const auto checkSunDirection=[](const host::Frame& frame) {
            require(frame.sunlight&&host::valid_sunlight(*frame.sunlight),"enabled sun frame missing");
            const auto bodies=std::span{frame.bodies}.first(frame.body_count);
            const auto carrier=std::ranges::find_if(bodies,[&](const auto& body){return body.id==frame.anchor;});
            require(carrier!=bodies.end(),"solar frame missing active body");
            const auto& center=carrier->center;
            const auto length=std::hypot(center[0],center[1],center[2]);
            for(unsigned axis=0;axis<3;++axis)
                require(std::abs(frame.sunlight->direction_to_sun[axis]+center[axis]/length)<1e-15,
                    "solar direction did not use the active body's center");
        };
        for(std::uint32_t body:std::array{0u,5u,3u})
        {
            host::Input choose;choose.command=host::Command::orbit;choose.selected_body=body;
            auto illuminated=advance(*solar,choose).frame;
            checkSunDirection(illuminated);
            const auto orbitFrame=illuminated;
            illuminated=advance(*solar,{.command=host::Command::land}).frame;
            checkSunDirection(illuminated);
            require(illuminated.seconds==orbitFrame.seconds&&illuminated.anchor==orbitFrame.anchor
                &&distance(illuminated.camera,orbitFrame.camera)>100.0
                &&illuminated.sunlight->direction_to_sun==orbitFrame.sunlight->direction_to_sun,
                "moving the camera at a fixed scene time moved the sunlight");
            for(unsigned instant=0;instant<3;++instant)
            {
                checkSunDirection(illuminated);
                require(illuminated.sunlight->color==std::array<float,3>{1,1,1}&&illuminated.sunlight->intensity==1.12f,
                    "original solar diffuse radiance changed");
                const auto later=advance(*solar,{.elapsed_seconds=.25,.time_scale=1000}).frame;
                checkSunDirection(later);
                require(later.seconds>illuminated.seconds
                    &&distance(later.sunlight->direction_to_sun,illuminated.sunlight->direction_to_sun)>1e-7,
                    "orbital time did not advance sunlight");
                illuminated=later;
            }
        }
        auto singleSolarBinding=solarBinding;singleSolarBinding.universe=false;singleSolarBinding.initial_body=5;
        const auto singleSolar=adapter::make_controller(singleSolarBinding);
        require(bool(singleSolar),"scoped Moon solar controller unavailable");
        const auto singleSolarFrame=singleSolar->capture().frame;
        checkSunDirection(singleSolarFrame);
        require(singleSolarFrame.body_count==1u&&singleSolarFrame.bodies[0].id==5,
            "scoped solar frame published hidden bodies");
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
            if(selected==0u)
            {
                const host::TerrainProfile profile{8u,9u,48u,64u,210.0,0.15,{}};
                auto rejected=profile;
                rejected.maximum_level=21u;
                require(!planet->create_dry_terrain_provider(rejected),"linked terrain accepted an unsupported level");
                auto terrain=planet->create_dry_terrain_provider(profile);
                require(bool(terrain),"dry single-body controller refused the linked provider");
                require(planet->create_dry_terrain_provider(profile)!=terrain,"separate terrain sessions share mutable residence");
                const auto center=cover_world.center(0,0.0);
                const auto radius=cover_world.body(0).radius;
                host::TerrainView view{0u,{center.x+radius*1.0004,center.y,center.z},
                    {-1.0,0.0,0.0},{0.0,1.0,0.0},0.0,1.0471975511965976,1080.0/2211.0,2211u};
                require(terrain->step(view,{}).status==host::TerrainStatus::invalid,"unidentified terrain work accepted");
                const auto settled=[&] {
                    for(unsigned step=0;step<24;++step)
                    {
                        ++view.request_id;
                        const auto batch=terrain->step(view,{});
                        require(batch.request_id==view.request_id&&batch.built<=16u,"terrain work exceeded its batch");
                        if(batch.status==host::TerrainStatus::ready)
                        {
                            require(batch.artifact&&batch.draw_patches>=6u&&batch.source_origin_meters!=std::array<double,3>{},
                                "linked terrain offered a partial or nonmetric cut");
                            if(!batch.pending)return batch;
                        }
                        else require(batch.status==host::TerrainStatus::pending&&!batch.artifact,
                            "linked terrain failed while building admitted footprint");
                    }
                    throw std::runtime_error("linked terrain did not settle inside the step bound");
                };
                const auto near=settled();
                require(near.maximum_level>0,"near linked terrain has only coarse roots");
                require(terrain->step(view,{}).status==host::TerrainStatus::invalid,"old terrain request replayed");
                ++view.request_id;
                const auto reused=terrain->step(view,{});
                require(reused.status==host::TerrainStatus::ready&&!reused.built&&reused.artifact==near.artifact,
                    "unchanged footprint rebuilt its selected cut");
                ++view.request_id;
                std::stop_source stopped;
                stopped.request_stop();
                const auto cancelled=terrain->step(view,stopped.get_token());
                require(cancelled.status==host::TerrainStatus::cancelled&&!cancelled.artifact,
                    "cancelled terrain request attempted publication");
                const auto retained=terrain->step(view,{});
                require(retained.artifact==near.artifact,"cancellation discarded the admitted cut");
                view.world_camera_meters={center.x,center.y,center.z+radius*3.0};
                view.forward={0.0,0.0,-1.0};
                const auto far=settled();
                require(far.artifact->key()!=near.artifact->key()&&far.generation>near.generation
                    &&far.maximum_level<near.maximum_level&&far.source_origin_meters!=near.source_origin_meters,
                    "new flight footprint did not move terrain detail");
            }
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
            host::FlightStart scopedFlight{.body=selected,
                .body_local_position_meters={spawnWorld.body(selected).radius+123456.25,0,0},
                .body_local_forward={0,0,-1},.body_local_up={0,1,0}};
            const auto scopedStart=planet->start_in_flight(scopedFlight);
            require(scopedStart.code==host::Code::ready&&scopedStart.frame.body_count==1u
                &&scopedStart.frame.anchor==selected&&scopedStart.frame.target==selected,
                "selected Planet flight lost its bound body");
            const auto scopedFlightSession=planet->encode_session();
            scopedFlight.body=selected==0u?5u:0u;
            require(planet->start_in_flight(scopedFlight).code==host::Code::invalid_input
                &&planet->encode_session()==scopedFlightSession,"Planet flight accepted a foreign body");
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
            <<",\"six_real_body_meshes\":true,\"moon_orbits\":true,\"session_codec\":true,\"native_host\":false";
#if defined(EPOCH_PLANET_WATER_CONTRACT)
        result<<",\"linked_wet_cut_phases\":true";
#endif
        result<<"}\n";
        return result?0:2;
    }
    catch(const std::exception& error){std::ofstream file{"controller-failure.txt"};file<<error.what();return 1;}
}
