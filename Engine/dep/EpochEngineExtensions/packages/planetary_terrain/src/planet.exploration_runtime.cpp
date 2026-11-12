// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
module planet.exploration_runtime;
import planet.exploration;
import planet.terrain;
import planet.coverage;
import planet.clouds;
import asset.cloud_volume;
import asset.mesh_artifact;
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
import water.fft;
#endif

// The existing Engine project host owns manifest, edition, package-source and
// settings admission. This linked adapter cannot replace that policy.
extern "C" bool epoch_bind_admitted_planetary_runtime(const char*, const char*) noexcept;

namespace epoch::extensions::planetary_terrain::exploration_runtime
{
    namespace host = epochengine::project_planetary_runtime;
    namespace exploration = epoch::extensions::planetary_terrain::exploration;
    namespace core = epoch::extensions::planetary_terrain::core;
    namespace coverage = epoch::extensions::planetary_terrain::coverage;
    namespace mesh = epochengine::asset::mesh;
    namespace
    {
        constexpr double fixed_step = 1.0/60.0;
        [[nodiscard]] std::array<double,3> vector(core::DVec3 value) { return {value.x,value.y,value.z}; }
        [[nodiscard]] std::shared_ptr<const mesh::Artifact> coarse_body(std::uint32_t seed,
            std::uint32_t index, bool ocean,core::WorldConfig config)
        {
            coverage::Request request;
            request.mode=coverage::Mode::planet;request.seed=seed;request.body_index=index;
            request.cells_per_tile=8;request.ocean_surface=ocean&&index==0u;
            request.world=config;
            const auto generated=coverage::build(request);
            if (!generated) return {};
            const core::Universe world{seed,config};const double scale=10.0/world.body(index).radius;
            std::vector<mesh::Vertex> vertices;std::vector<std::uint32_t> indices;
            for(const auto& patch:generated.patches)
            {
                const auto base=static_cast<std::uint32_t>(vertices.size());
                for(const auto& vertex:patch.mesh.vertices)
                {
                    const auto position=(patch.mesh.anchor+core::DVec3{vertex.position})*scale;
                    vertices.push_back({{static_cast<float>(position.x),static_cast<float>(position.y),static_cast<float>(position.z)},
                        {vertex.normal.x,vertex.normal.y,vertex.normal.z},{},
                        {vertex.color[0],vertex.color[1],vertex.color[2]}});
                }
                for(const auto indexValue:patch.mesh.indices)indices.push_back(base+indexValue);
            }
            return mesh::build_artifact(vertices,indices).artifact;
        }
        [[nodiscard]] bool valid_input(const host::Input& input)
        {
            return std::isfinite(input.elapsed_seconds)&&input.elapsed_seconds>=0.0&&input.elapsed_seconds<=0.25
                &&std::isfinite(input.forward)&&std::isfinite(input.right)&&std::isfinite(input.rise)
                &&std::isfinite(input.yaw)&&std::isfinite(input.pitch)&&std::isfinite(input.roll)
                &&(!input.selected_body||*input.selected_body<6u)
                &&(!input.time_scale||(std::isfinite(*input.time_scale)&&*input.time_scale>=0.0&&*input.time_scale<=10000.0))
                &&(!input.speed_multiplier||(std::isfinite(*input.speed_multiplier)&&*input.speed_multiplier>0.0))
                &&static_cast<unsigned>(input.command)<=static_cast<unsigned>(host::Command::toggle_level_assist);
        }
        [[nodiscard]] bool blocks_walk(const host::WalkBox& box, core::DVec3 from,
            core::DVec3 to, core::DVec3 radial_up) noexcept
        {
            // Sweep a conservative box-space envelope of the 1.8 m walking
            // capsule. The eye positions and box center share body-local metres.
            constexpr double eye_to_middle = 0.88, radius = 0.32, half_segment = 0.55;
            const auto from_mid = vector(from - radial_up * eye_to_middle);
            const auto to_mid = vector(to - radial_up * eye_to_middle);
            const auto up = vector(radial_up);
            double entry = 0.0, exit = 1.0;
            double from_depth = std::numeric_limits<double>::infinity(), to_depth = from_depth;
            bool intersects = true;
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                double start{}, finish{}, vertical{};
                for (std::size_t row = 0; row < 3; ++row)
                {
                    const auto basis = box.rotation[row * 3 + axis];
                    start += basis * (from_mid[row] - box.center_meters[row]);
                    finish += basis * (to_mid[row] - box.center_meters[row]);
                    vertical += basis * up[row];
                }
                const auto extent = box.half_extent_meters[axis] + radius + half_segment * std::abs(vertical);
                from_depth = (std::min)(from_depth, extent - std::abs(start));
                to_depth = (std::min)(to_depth, extent - std::abs(finish));
                const auto delta = finish - start;
                if (std::abs(delta) <= 1e-12)
                    intersects = intersects && std::abs(start) <= extent;
                else
                {
                    const auto first = (-extent - start) / delta;
                    const auto second = (extent - start) / delta;
                    entry = (std::max)(entry, (std::min)(first, second));
                    exit = (std::min)(exit, (std::max)(first, second));
                }
            }
            // Existing save data can start inside an authored box. Permit
            // movement that reduces the overlap so the player can get out.
            if (from_depth >= 0.0) return to_depth > from_depth + 1e-8;
            return intersects && entry <= exit;
        }
        class OriginalController final : public host::Controller
        {
            host::Binding binding_;
            exploration::Simulation simulation_;
            std::array<std::shared_ptr<const mesh::Artifact>,6> meshes_{};
            std::array<std::optional<epochengine::asset::cloud_volume::Recipe>,6> clouds_{};
            bool cloud_layers_ready_{true};
            double accumulator_{},last_nonzero_scale_{1.0};
            std::shared_ptr<epochengine::water::wave::Provider> water_{};
            std::vector<host::WalkBox> walk_boxes_{};
        public:
            explicit OriginalController(const host::Binding& binding):binding_(binding),simulation_(binding.seed,
                {binding.size_reference_body,binding.reference_radius_meters},
                binding.universe?std::optional<std::size_t>{}:std::optional<std::size_t>{binding.initial_body})
            {
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
                if(binding.ocean_surface)
                {
                    epochengine::water::fft::Settings settings;settings.seed=binding.seed;
                    water_=std::make_shared<epochengine::water::fft::Implementation>(settings);
                }
#endif
                simulation_.orbit(binding.initial_body);
                for(std::uint32_t index=0;index<meshes_.size();++index)
                    if(binding.universe||index==binding.initial_body)
                    {
                        meshes_[index]=coarse_body(binding.seed,index,binding.ocean_surface,simulation_.world().config());
                        clouds_[index]=clouds::density_recipe(simulation_.world().body(index));
                        if(clouds::supports(simulation_.world().body(index))&&!clouds_[index])cloud_layers_ready_=false;
                    }
            }
            [[nodiscard]] std::shared_ptr<epochengine::water::wave::Provider> water_provider() const override
            {return water_;}
            [[nodiscard]] bool set_walk_boxes(std::span<const host::WalkBox> boxes) override
            {
                if (boxes.size() > host::maximum_walk_boxes || std::ranges::any_of(boxes, [&](const auto& box) {
                    return !box.valid() || box.body >= simulation_.world().bodies().size()
                        || (!binding_.universe && box.body != binding_.initial_body)
                        || simulation_.world().body(box.body).kind == core::BodyKind::gas;
                })) return false;
                walk_boxes_.assign(boxes.begin(), boxes.end());
                return true;
            }
            [[nodiscard]] host::Result capture() const override
            {
                if(!cloud_layers_ready_)return {host::Code::provider_failure,{}};
                host::Frame frame;
                const auto camera=simulation_.interpolated_camera(accumulator_/fixed_step);
                frame.seed=binding_.seed;frame.anchor=static_cast<std::uint32_t>(simulation_.anchor());
                frame.target=static_cast<std::uint32_t>(simulation_.target());
                frame.camera=vector(camera.position);frame.forward=vector(camera.forward);frame.up=vector(camera.up);
                frame.seconds=simulation_.seconds();frame.time_scale=simulation_.time_scale();
                frame.speed_factor=simulation_.speed_factor();frame.altitude=simulation_.altitude();frame.speed=simulation_.speed();
                frame.gravity_enabled=simulation_.gravity_enabled();frame.level_assist=simulation_.level_assist();
                frame.mode=static_cast<host::TravelMode>(simulation_.mode());
                if(binding_.original_sunlight)
                {
                    const auto direction=core::DVec3{-camera.position.x,-camera.position.y,-camera.position.z};
                    const auto length=core::length(direction);
                    if(!std::isfinite(length)||length<=0.0)return {host::Code::invalid_frame,{}};
                    frame.sunlight=host::Sunlight{.direction_to_sun=vector(direction/length)};
                }
                for(std::uint32_t index=0;index<simulation_.world().bodies().size();++index)
                {
                    if(!binding_.universe&&index!=binding_.initial_body)continue;
                    const auto& body=simulation_.world().body(index);
                    frame.bodies[frame.body_count++]={body.id,binding_.universe?body.parent:-1,std::string{body.name},
                        vector(simulation_.world().center(index,frame.seconds)),body.radius,
                        body.kind!=core::BodyKind::gas,meshes_[index],{},clouds::wind_degrees(frame.seconds),clouds_[index]};
                }
                frame.valid=true;
                return host::valid_frame(frame)?host::Result{host::Code::ready,std::move(frame)}
                    :host::Result{host::Code::invalid_frame,{}};
            }
            [[nodiscard]] host::Result advance(const host::Input& input,std::stop_token stop) override
            {
                if(stop.stop_requested())return {host::Code::cancelled,{}};
                if(!valid_input(input)||(!binding_.universe&&input.selected_body
                    &&*input.selected_body!=binding_.initial_body))return {host::Code::invalid_input,{}};
                auto next=simulation_;double accumulated=accumulator_,lastScale=last_nonzero_scale_;
                if(input.selected_body)next.select(*input.selected_body);
                if(input.time_scale)next.set_time_scale(*input.time_scale);
                if(input.speed_multiplier)next.adjust_speed(*input.speed_multiplier);
                switch(input.command)
                {
                case host::Command::none:break;
                case host::Command::orbit:next.orbit(next.target());accumulated=0;break;
                case host::Command::land:if(!next.land(next.target()))return {host::Code::invalid_input,{}};accumulated=0;break;
                case host::Command::toggle_cruise:if(next.mode()==exploration::TravelMode::cruise)next.stop_cruise();else next.cruise();break;
                case host::Command::toggle_walk:next.toggle_walk();break;
                case host::Command::toggle_time_pause:
                    if(next.time_scale()>0){lastScale=next.time_scale();next.set_time_scale(0);}else next.set_time_scale(lastScale);break;
                case host::Command::cycle_time_rate:
                    next.set_time_scale(next.time_scale()<1?1:next.time_scale()<10?10:next.time_scale()<100?100:next.time_scale()<1000?1000:1);break;
                case host::Command::toggle_gravity:next.set_gravity(!next.gravity_enabled());break;
                case host::Command::toggle_level_assist:next.set_level_assist(!next.level_assist());break;
                }
                if(input.yaw!=0||input.pitch!=0||input.roll!=0)next.look(input.yaw,input.pitch,input.roll);
                if(input.forward!=0||input.right!=0||input.rise!=0)next.stop_cruise();
                accumulated+=input.elapsed_seconds;
                for(unsigned steps=0;accumulated+1e-12>=fixed_step&&steps<15u;++steps)
                {
                    if(stop.stop_requested())return {host::Code::cancelled,{}};
                    const exploration::PilotInput pilot{input.forward,input.right,input.rise,input.boost};
                    const bool check_boxes = next.mode() == exploration::TravelMode::walk && (pilot.forward != 0 || pilot.right != 0)
                        && std::ranges::any_of(walk_boxes_, [&](const auto& box) { return box.body == next.anchor(); });
                    auto before = check_boxes ? std::optional{next} : std::nullopt;
                    if(exploration::advance(next,fixed_step,pilot,stop)
                        !=exploration::StepCode::advanced)return {host::Code::cancelled,{}};
                    if (before)
                    {
                        const auto from = before->state().local_position;
                        const auto to = next.state().local_position;
                        const auto up = core::normalized(from);
                        if (std::ranges::any_of(walk_boxes_, [&](const auto& box) {
                            return box.body == next.anchor() && blocks_walk(box, from, to, up);
                        }))
                        {
                            next = std::move(*before);
                            if (exploration::advance(next, fixed_step, {}, stop) != exploration::StepCode::advanced)
                                return {host::Code::cancelled, {}};
                        }
                    }
                    accumulated-=fixed_step;
                }
                if(stop.stop_requested())return {host::Code::cancelled,{}};
                if(!exploration::valid_state(next.state()))return {host::Code::invalid_frame,{}};
                simulation_=std::move(next);accumulator_=(std::max)(0.0,accumulated);last_nonzero_scale_=lastScale;
                return capture();
            }
            [[nodiscard]] host::Result start_on_surface(const host::SurfaceStart& start) override
            {
                const auto& world = simulation_.world();
                if (start.body >= world.bodies().size()
                    || (!binding_.universe && start.body != binding_.initial_body))
                    return {host::Code::invalid_input, {}};
                const auto& body = world.body(start.body);
                const core::DVec3 local{start.body_local_position_meters[0], start.body_local_position_meters[1],
                    start.body_local_position_meters[2]};
                const core::DVec3 forward{start.body_local_forward[0], start.body_local_forward[1], start.body_local_forward[2]};
                const auto radius = core::length(local);
                if (body.kind == core::BodyKind::gas || !core::finite(local) || !core::finite(forward)
                    || !std::isfinite(radius) || radius <= 0.0)
                    return {host::Code::invalid_input, {}};

                const auto up = local / radius;
                const auto height = core::sample_height(body, up);
                const auto ground = world.ground_radius(start.body, up);
                if ((body.ocean && height <= 0.0) || std::abs(radius - ground) > 100.0)
                    return {host::Code::invalid_input, {}};

                auto state = simulation_.state();
                state.anchor = state.target = start.body;
                state.mode = exploration::TravelMode::walk;
                state.local_position = up * (ground + 1.8);
                const auto radial_heading = forward.x * up.x + forward.y * up.y + forward.z * up.z;
                state.forward = core::normalized(forward - up * radial_heading, core::tangent(up));
                state.up = up;
                state.fall_velocity = {};
                if (!exploration::valid_state(state)) return {host::Code::invalid_frame, {}};

                auto previous = simulation_;
                const auto previousAccumulator = accumulator_;
                simulation_.restore(state);
                accumulator_ = 0.0;
                auto result = capture();
                if (result.code != host::Code::ready)
                {
                    simulation_ = std::move(previous);
                    accumulator_ = previousAccumulator;
                }
                return result;
            }
            [[nodiscard]] std::string encode_session() const override
            {
                const auto state=simulation_.state();
                if(!exploration::valid_state(state))return {};
                const bool scoped=!binding_.universe;
                const bool solar=binding_.original_sunlight;
                const bool sized=scoped||solar||state.world!=core::WorldConfig{};
                std::string output=std::string{scoped?"epoch.planet.exploration.v4\n":solar?"epoch.planet.exploration.v3\n":sized?"epoch.planet.exploration.v2\n":"epoch.planet.exploration.v1\n"}
                    +"world=EpochPlanet0.2.3\nproject_id="+binding_.project_id
                    +"\nsource_hash="+binding_.source_hash+"\nedition="+std::to_string(binding_.edition)+"\n";
                const auto number=[&](std::string_view key,auto value) {
                    std::array<char,64> bytes{};
                    const auto converted=[&] {
                        if constexpr(std::is_floating_point_v<decltype(value)>)
                            return std::to_chars(bytes.data(),bytes.data()+bytes.size(),value,
                                std::chars_format::general,std::numeric_limits<decltype(value)>::max_digits10);
                        else return std::to_chars(bytes.data(),bytes.data()+bytes.size(),value);
                    }();
                    if(converted.ec!=std::errc{})return false;
                    output.append(key);output+='=';output.append(bytes.data(),converted.ptr);output+='\n';return true;
                };
                const auto vec=[&](std::string_view key,core::DVec3 value) {
                    return number(std::string{key}+"_x",value.x)&&number(std::string{key}+"_y",value.y)&&number(std::string{key}+"_z",value.z);
                };
                if(!number("seed",state.seed)||!number("initial_body",binding_.initial_body)
                    ||!number("ocean_surface",binding_.ocean_surface?1u:0u)
                    ||(scoped&&!number("universe",0u))
                    ||(sized&&(!number("size_reference_body",state.world.reference_body)
                        ||!number("reference_radius_meters",state.world.reference_radius_meters)))
                    ||((scoped||solar)&&!number("original_sunlight",solar?1u:0u))
                    ||!number("anchor",state.anchor)||!number("target",state.target)
                    ||!number("seconds",state.seconds)||!number("time_scale",state.time_scale)||!number("speed_factor",state.speed_factor)
                    ||!number("mode",static_cast<unsigned>(state.mode))||!vec("local_position",state.local_position)
                    ||!vec("forward",state.forward)||!vec("up",state.up)||!vec("fall_velocity",state.fall_velocity)
                    ||!number("gravity_enabled",state.gravity_enabled?1u:0u)||!number("level_assist",state.level_assist?1u:0u)
                    ||!number("accumulator",accumulator_)||!number("last_nonzero_scale",last_nonzero_scale_))return {};
                return output;
            }
            [[nodiscard]] bool restore_session(std::string_view text) override
            {
                const bool scoped=text.starts_with("epoch.planet.exploration.v4\nworld=EpochPlanet0.2.3\n");
                if(scoped==binding_.universe)return false;
                const bool solar=text.starts_with("epoch.planet.exploration.v3\nworld=EpochPlanet0.2.3\n");
                const bool sized=scoped||solar||text.starts_with("epoch.planet.exploration.v2\nworld=EpochPlanet0.2.3\n");
                if(text.size()>16384u||(!sized&&!text.starts_with("epoch.planet.exploration.v1\nworld=EpochPlanet0.2.3\n")))return false;
                text.remove_prefix(std::string_view{"epoch.planet.exploration.v1\nworld=EpochPlanet0.2.3\n"}.size());
                const auto field=[&](std::string_view key)->std::optional<std::string_view> {
                    if(!text.starts_with(key)||text.size()<=key.size()||text[key.size()]!='=')return {};
                    text.remove_prefix(key.size()+1u);const auto end=text.find('\n');if(end==std::string_view::npos||end==0u)return {};
                    const auto value=text.substr(0,end);text.remove_prefix(end+1u);return value;
                };
                const auto project=field("project_id"),source=field("source_hash");
                if(!project||*project!=binding_.project_id||!source||*source!=binding_.source_hash)return false;
                const auto number=[&](std::string_view key,auto& output) {
                    const auto value=field(key);if(!value)return false;
                    const auto parsed=std::from_chars(value->data(),value->data()+value->size(),output);
                    return parsed.ec==std::errc{}&&parsed.ptr==value->data()+value->size();
                };
                exploration::SessionState state;unsigned edition{},initial{},ocean{},mode{},gravity{},level{},originalSun{},universe{};
                double accumulator{},lastScale{};
                const auto vec=[&](std::string_view key,core::DVec3& value) {
                    return number(std::string{key}+"_x",value.x)&&number(std::string{key}+"_y",value.y)&&number(std::string{key}+"_z",value.z);
                };
                if(!number("edition",edition)||edition!=binding_.edition||!number("seed",state.seed)||state.seed!=binding_.seed
                    ||!number("initial_body",initial)||initial!=binding_.initial_body||!number("ocean_surface",ocean)||ocean!=(binding_.ocean_surface?1u:0u)
                    ||(scoped&&(!number("universe",universe)||universe!=0u))
                    ||(sized&&(!number("size_reference_body",state.world.reference_body)
                        ||!number("reference_radius_meters",state.world.reference_radius_meters)))
                    ||state.world!=core::WorldConfig{binding_.size_reference_body,binding_.reference_radius_meters}
                    ||((scoped||solar)&&(!number("original_sunlight",originalSun)||originalSun>1u))
                    ||(originalSun!=0u)!=binding_.original_sunlight
                    ||!number("anchor",state.anchor)||!number("target",state.target)||!number("seconds",state.seconds)
                    ||!number("time_scale",state.time_scale)||!number("speed_factor",state.speed_factor)||!number("mode",mode)||mode>2u
                    ||!vec("local_position",state.local_position)||!vec("forward",state.forward)||!vec("up",state.up)||!vec("fall_velocity",state.fall_velocity)
                    ||!number("gravity_enabled",gravity)||gravity>1u||!number("level_assist",level)||level>1u
                    ||!number("accumulator",accumulator)||!std::isfinite(accumulator)||accumulator<0||accumulator>=fixed_step
                    ||!number("last_nonzero_scale",lastScale)||!std::isfinite(lastScale)||lastScale<=0||lastScale>10000||!text.empty())return false;
                state.mode=static_cast<exploration::TravelMode>(mode);state.gravity_enabled=gravity!=0;state.level_assist=level!=0;
                if(!exploration::valid_state(state)||(!binding_.universe
                    &&(state.anchor!=binding_.initial_body||state.target!=binding_.initial_body)))return false;
                auto replacement=simulation_;replacement.restore(state);
                simulation_=std::move(replacement);accumulator_=accumulator;last_nonzero_scale_=lastScale;
                return true;
            }
        };
    }
    std::unique_ptr<host::Controller> make_controller(const host::Binding& binding)
    {
        if(!host::valid_binding(binding))return {};
        auto controller=std::make_unique<OriginalController>(binding);
        return controller->capture().code==host::Code::ready?std::move(controller):nullptr;
    }
}

extern "C" bool epoch_install_planetary_runtime(const char* root,const char* id) noexcept
{
    try
    {
        if(!root||!id||!epoch_bind_admitted_planetary_runtime(root,id))return false;
        const auto binding=epochengine::project_planetary_runtime::admitted_project_binding();
        if(!binding||binding->project_root!=root||binding->project_id!=id)return false;
        return epoch::extensions::planetary_terrain::exploration_runtime::register_controller(*binding);
    }
    catch(...){return false;}
}
