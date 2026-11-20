// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
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
import planet.cut_assembly;
import planet.streaming;
import asset.cloud_volume;
import asset.mesh_artifact;
import project.planetary_snapshot;
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
import water.fft;
import water.planetary;
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
    namespace patches = epochengine::project_planetary_snapshot;
    namespace stream = epoch::extensions::planetary_terrain::streaming;
    namespace assembly = epoch::extensions::planetary_terrain::cut_assembly;
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
        class OriginalGroundCover final : public host::GroundCoverProvider
        {
        public:
            explicit OriginalGroundCover(const host::Binding& binding)
                : world_{binding.seed,{binding.size_reference_body,binding.reference_radius_meters}},
                  initial_body_{binding.initial_body}, universe_{binding.universe} {}

            [[nodiscard]] std::optional<host::GroundCoverDrawPatch> build_patch(std::uint32_t body_index,
                std::array<double,3> direction,std::stop_token stop) const override
            {
                if(stop.stop_requested()||!admitted_body(body_index))return {};
                const auto& body=world_.body(body_index);
                const auto key=coverage::grass_patch_at(body,{direction[0],direction[1],direction[2]});
                if(!key)return {};
                return draw_patch(coverage::build_grass_patch(body,*key,stop),stop);
            }

            [[nodiscard]] std::optional<std::vector<host::GroundCoverDrawPatch>> build_neighborhood(
                std::uint32_t body_index,std::array<double,3> direction,std::stop_token stop) const override
            {
                std::vector<host::GroundCoverDrawPatch> patches;
                if(stop.stop_requested()||!admitted_body(body_index))return patches;
                const auto& body=world_.body(body_index);
                const auto keys=coverage::grass_patch_neighborhood(body,{direction[0],direction[1],direction[2]});
                patches.reserve(keys.size());
                for(const auto key:keys)
                {
                    auto patch=draw_patch(coverage::build_grass_patch(body,key,stop),stop);
                    if(!patch||stop.stop_requested())return std::vector<host::GroundCoverDrawPatch>{};
                    if(patch->mesh)patches.push_back(std::move(*patch));
                }
                return patches;
            }
            [[nodiscard]] std::optional<std::vector<host::GroundCoverDrawPatch>> build_visible_neighborhood(
                std::uint32_t body_index,std::array<double,3> direction,double altitude,std::stop_token stop) const override
            {
                static_assert(coverage::maximum_grass_visible_patches==host::maximum_visible_ground_cover_patches);
                std::vector<host::GroundCoverDrawPatch> patches;
                if(stop.stop_requested()||!admitted_body(body_index)||!std::isfinite(altitude)||altitude>=32.0)return patches;
                const auto& body=world_.body(body_index);
                const core::DVec3 radial{direction[0],direction[1],direction[2]};
                const auto keys=coverage::grass_visible_patch_keys(body,radial);
                if(keys.empty())return patches;
                const coverage::GrassDrawView view{core::normalized(radial)*body.radius,body.radius,altitude};
                std::vector<CachedGrassPatch> next;
                patches.reserve(keys.size());
                next.reserve(keys.size());
                std::lock_guard cache_lock{visible_cache_mutex_};
                for(const auto key:keys)
                {
                    if(stop.stop_requested())return std::vector<host::GroundCoverDrawPatch>{};
                    const auto found=std::ranges::find(visible_cache_,key,&CachedGrassPatch::key);
                    auto grass=found==visible_cache_.end()
                        ?std::make_shared<const coverage::GrassPatch>(coverage::build_grass_patch(body,key,stop))
                        :found->grass;
                    auto patch=draw_patch(*grass,stop,&view);
                    if(!patch)return std::vector<host::GroundCoverDrawPatch>{};
                    if(patch->mesh)patches.push_back(std::move(*patch));
                    next.push_back({key,std::move(grass)});
                }
                if(stop.stop_requested())return std::vector<host::GroundCoverDrawPatch>{};
                visible_cache_=std::move(next);
                return patches;
            }
        private:
            struct CachedGrassPatch final
            {
                coverage::GrassPatchKey key{};
                std::shared_ptr<const coverage::GrassPatch> grass{};
            };

            [[nodiscard]] bool admitted_body(std::uint32_t body_index) const noexcept
            {
                return body_index<world_.bodies().size()&&(universe_||body_index==initial_body_);
            }

            [[nodiscard]] static std::optional<host::GroundCoverDrawPatch> draw_patch(const coverage::GrassPatch& patch,
                std::stop_token stop,const coverage::GrassDrawView* view=nullptr)
            {
                const auto geometry=coverage::mesh_grass_patch(patch,stop,view);
                if(!geometry)return {};
                host::GroundCoverDrawPatch output{vector(geometry->anchor)};
                if(geometry->indices.empty())return output;
                std::vector<mesh::Vertex> vertices;
                vertices.reserve(geometry->vertices.size());
                for(const auto& vertex:geometry->vertices)
                {
                    if(stop.stop_requested())return {};
                    vertices.push_back({{vertex.position.x,vertex.position.y,vertex.position.z},
                        {vertex.normal.x,vertex.normal.y,vertex.normal.z},{},
                        {vertex.color[0],vertex.color[1],vertex.color[2]}});
                }
                output.mesh=mesh::build_artifact(vertices,geometry->indices).artifact;
                if(!output.mesh||stop.stop_requested())return {};
                return output;
            }
            const core::Universe world_;
            const std::uint32_t initial_body_;
            const bool universe_;
            mutable std::mutex visible_cache_mutex_;
            mutable std::vector<CachedGrassPatch> visible_cache_;
        };

#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
        class OriginalWaterLattice final : public patches::WaterLatticeProvider
        {
        public:
            explicit OriginalWaterLattice(const host::Binding& binding)
                : world_(binding.seed,{binding.size_reference_body,binding.reference_radius_meters}),
                  body_(binding.initial_body),universe_(binding.universe) {}

            [[nodiscard]] std::optional<std::vector<patches::WaterVertex>> build_basis(patches::TileKey key,
                double radius,std::uint32_t cells,bool skirts,std::stop_token stop) const override
            {
                if(stop.stop_requested()||!patches::valid_key(key)||key.body>=world_.bodies().size()
                    ||(!universe_&&key.body!=body_))return {};
                const auto& body=world_.body(key.body);
                if(radius!=body.radius)return {};
                return water::build_water_lattice_basis(body,{key.x,key.y,key.body,key.face,key.level},cells,skirts,stop);
            }

        private:
            const core::Universe world_;
            const std::uint32_t body_;
            const bool universe_;
        };
#endif

        class LinkedTerrain final : public host::TerrainProvider
        {
            const host::TerrainProfile profile_;
            const std::uint32_t body_;
            const std::string source_hash_, recipe_key_;
            std::mutex mutex_;
            stream::State state_;
            std::uint64_t request_{};
            std::vector<core::TileKey> last_cut_;
            std::shared_ptr<const mesh::Artifact> artifact_;
            std::array<double,3> origin_{};
            patches::SelectedCut wet_cut_;
            std::shared_ptr<const epochengine::water::wave::Tile> presented_sample_;

        public:
            LinkedTerrain(const host::Binding& binding,const host::TerrainProfile& profile,std::string_view recipe_key={})
                :profile_(profile),body_(binding.initial_body),
                 source_hash_(binding.source_hash),recipe_key_(recipe_key),
                 state_(binding.seed,{binding.size_reference_body,binding.reference_radius_meters}) {}

            [[nodiscard]] host::TerrainBatch step(const host::TerrainView& view,std::stop_token stop) override
            {
                using Status=host::TerrainStatus;
                host::TerrainBatch result{.request_id=view.request_id};
                const auto finish=[&](Status status,std::string_view failure={}) {
                    result.status=status;result.failure=failure;return result; };
                if(stop.stop_requested())return finish(Status::cancelled);
                std::unique_lock lock{mutex_,std::try_to_lock};
                if(!lock.owns_lock())return finish(Status::busy);
                const bool wet=!recipe_key_.empty();
                if(wet!=static_cast<bool>(view.water_sample))return finish(Status::invalid,"water sample");
                if(view.request_id<=request_||view.viewport_height<64u||view.viewport_height>16384u
                    ||!std::isfinite(view.vertical_fov)||view.vertical_fov<0.1||view.vertical_fov>3.0
                    ||!std::isfinite(view.aspect_ratio)||view.aspect_ratio<0.1||view.aspect_ratio>10.0
                    ||!std::isfinite(view.seconds)||view.seconds<0.0||view.seconds>=1.0e12
                    ||std::ranges::any_of(view.world_camera_meters,[](double value){return !std::isfinite(value);}))
                    return finish(Status::invalid,"terrain view");

                const auto vector=[](const std::array<double,3>& v){return core::DVec3{v[0],v[1],v[2]};};
                stream::Request request;
                request.body_index=body_;
                request.seconds=view.seconds;
                request.camera=vector(view.world_camera_meters);
                request.viewport_height=static_cast<int>(view.viewport_height);
                request.vertical_fov=view.vertical_fov;
                request.lod={.max_level=static_cast<std::uint8_t>(profile_.maximum_level),
                    .max_leaves=profile_.maximum_leaves,.patch_pixels=profile_.patch_pixels,.hysteresis=profile_.lod_hysteresis};
                request.cells_per_tile=profile_.cells_per_tile;
                request.maximum_resident_patches=4096;
                request.maximum_bytes=std::uint64_t{profile_.resident_megabytes}*1024u*1024u;
                request.view=core::TerrainView{vector(view.forward),view.aspect_ratio,vector(view.up)};
                request.ocean_surface=wet;

                const auto update=stream::update(state_,request,stop);
                if(update==stream::Status::cancelled)return finish(Status::cancelled);
                if(update==stream::Status::capacity_exceeded)return finish(Status::capacity,"stream update");
                if(update!=stream::Status::ready)return finish(Status::invalid,"stream update");
                request_=view.request_id;
                // The existing scheduler owns this bounded step. Avoid a
                // nested pool or waiting on another task in the same pool.
                constexpr std::size_t maximum_per_step=16u;
                for(;result.built<maximum_per_step&&!stop.stop_requested();++result.built)
                {
                    const auto work=stream::next_work(state_);
                    if(!work)break;
                    try
                    {
                        const auto done=stream::complete(state_,*work,stream::build(state_,*work,stop),stop);
                        if(done==stream::Status::cancelled)return finish(Status::cancelled);
                        if(done==stream::Status::capacity_exceeded)return finish(Status::capacity,"patch completion");
                        if(done!=stream::Status::ready)return finish(Status::failed,"patch completion");
                    }
                    catch(...)
                    {
                        (void)stream::abandon(state_,*work);
                        return finish(Status::failed,"patch evaluation");
                    }
                }
                if(stop.stop_requested())return finish(Status::cancelled);
                const auto stats=stream::stats(state_);
                result.generation=stats.generation;
                result.pending=stats.pending_patches;
                result.draw_patches=stats.draw_patches;
                result.maximum_level=stats.maximum_level;
                if(result.pending&&!result.built)return finish(Status::capacity,"patch residency");
                if(!stats.complete_coverage)return finish(result.built?Status::pending:Status::capacity);
                if(wet&&result.pending)return finish(Status::pending);

                const auto frame=assembly::frame_for(state_,true);
                if(!frame)return finish(Status::invalid);
                assembly::Assembly assembly{state_,*frame,profile_.artifact,wet,wet};
                if(const auto failure=assembly.begin(view.water_sample,stop);failure)
                    return finish(stop.stop_requested()?Status::cancelled:Status::failed,failure.stage);
                if(!std::ranges::equal(last_cut_,assembly.draw_cut())||view.water_sample!=presented_sample_)
                {
                    for(const auto tile:assembly.tiles())
                    {
                        if(stop.stop_requested())return finish(Status::cancelled);
                        const auto work=assembly.work_for(tile);
                        if(!work)return finish(Status::failed,"patch missing");
                        if(work->scratch>assembly.scratch_available())return finish(Status::capacity,"patch scratch");
                        const auto geometry=assembly.prepare(*work,stop);
                        if(!geometry)return finish(stop.stop_requested()?Status::cancelled:Status::failed,"patch assembly");
                        if(const auto failure=assembly.append(*work,*geometry);failure)
                            return finish(failure.limit?Status::capacity:Status::failed,failure.stage);
                    }
                    auto built=assembly.build();
                    if(stop.stop_requested())return finish(Status::cancelled);
                    if(!built)return finish(Status::failed,"artifact build");
                    const std::array origin{frame->origin.x,frame->origin.y,frame->origin.z};
                    patches::SelectedCut wet_cut;
                    if(wet)
                    {
                        auto& metadata=assembly.metadata;
                        metadata.identity={built.artifact->key(),source_hash_,recipe_key_,view.request_id};
                        metadata.water_appearance=patches::admit_water_appearance(
                            assembly.water_ranges,*built.artifact,metadata,stop);
                        if(!metadata.water_appearance)
                            return finish(stop.stop_requested()?Status::cancelled:Status::failed,"water appearance");
                        const double phase=view.water_sample->seconds();
                        wet_cut.source=std::make_shared<const patches::Snapshot>(patches::Snapshot{
                            std::move(built.artifact),std::move(metadata),origin,phase});
                        auto limits=profile_.wet_transition_limits;
                        limits.artifact=profile_.artifact;
                        patches::Transition transition{limits};
                        if(!transition.admit(wet_cut.source,stop,patches::InitialCut::selected_patches))
                            return finish(stop.stop_requested()?Status::cancelled:Status::failed,"snapshot admission");
                        patches::WetTransitionFailure shoreline_failure;
                        if(patches::requires_water_edge_resample(*wet_cut.source))
                            wet_cut.artifact=transition.materialize_wet(profile_.cells_per_tile,phase,stop,
                                &wet_cut.water_appearance,0u,&shoreline_failure);
                        else
                            wet_cut.artifact=transition.materialize(stop,&wet_cut.water_appearance);
                        if(!wet_cut.artifact||!wet_cut.water_appearance)
                            return finish(stop.stop_requested()?Status::cancelled:Status::failed,
                                shoreline_failure.reason.empty()?"snapshot materialization":shoreline_failure.reason);
                    }
                    if(stop.stop_requested())return finish(Status::cancelled);
                    last_cut_.assign(assembly.draw_cut().begin(),assembly.draw_cut().end());
                    artifact_=wet?wet_cut.artifact:std::move(built.artifact);
                    origin_=origin;
                    wet_cut_=std::move(wet_cut);
                    presented_sample_=view.water_sample;
                }
                result.artifact=artifact_;
                result.source_origin_meters=origin_;
                result.aquatic_source=wet_cut_.source;
                result.water_appearance=wet_cut_.water_appearance;
                return finish(Status::ready);
            }
        };

        class OriginalController final : public host::Controller
        {
            host::Binding binding_;
            exploration::Simulation simulation_;
            std::array<std::shared_ptr<const mesh::Artifact>,6> meshes_{};
            std::array<std::optional<epochengine::asset::cloud_volume::Recipe>,6> clouds_{};
            bool cloud_layers_ready_{true};
            double accumulator_{},last_nonzero_scale_{1.0};
            std::shared_ptr<epochengine::water::wave::Provider> water_{};
            std::shared_ptr<const patches::WaterLatticeProvider> water_lattice_{};
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
                    water_lattice_=std::make_shared<const OriginalWaterLattice>(binding);
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
            [[nodiscard]] std::shared_ptr<const patches::WaterLatticeProvider> water_lattice_provider() const override
            {return water_lattice_;}
            [[nodiscard]] std::shared_ptr<const host::GroundCoverProvider> ground_cover_provider() const override
            {return std::make_shared<const OriginalGroundCover>(binding_);}
        private:
            [[nodiscard]] bool valid_terrain_profile(const host::TerrainProfile& profile) const
            {
                if(binding_.universe||profile.cells_per_tile<2u||profile.cells_per_tile>64u
                    ||profile.maximum_level>20u||profile.maximum_leaves<6u||profile.maximum_leaves>2048u
                    ||profile.resident_megabytes<4u||profile.resident_megabytes>256u
                    ||!std::isfinite(profile.patch_pixels)||profile.patch_pixels<32.0||profile.patch_pixels>4096.0
                    ||!std::isfinite(profile.lod_hysteresis)||profile.lod_hysteresis<0.0||profile.lod_hysteresis>0.45
                    ||profile.artifact.maximum_bytes<48u||profile.artifact.maximum_bytes>64u*1024u*1024u)return false;
                const std::uint64_t cells=profile.cells_per_tile,side=cells+1u;
                const std::uint64_t layers=binding_.ocean_surface?2u:1u;
                const auto vertices=layers*profile.maximum_leaves*(side*side+4u*cells);
                const auto indices=layers*profile.maximum_leaves*(6u*cells*cells+24u*cells);
                return vertices<=profile.artifact.maximum_vertices&&indices<=profile.artifact.maximum_indices
                    &&48u+vertices*44u+indices*4u<=profile.artifact.maximum_bytes;
            }
        public:
            [[nodiscard]] std::shared_ptr<host::TerrainProvider> create_dry_terrain_provider(
                const host::TerrainProfile& profile) const override
            {
                if(binding_.ocean_surface||!valid_terrain_profile(profile))return {};
                return std::make_shared<LinkedTerrain>(binding_,profile);
            }
            [[nodiscard]] std::shared_ptr<host::TerrainProvider> create_wet_terrain_provider(
                const host::TerrainProfile& profile,std::string_view recipe_key) const override
            {
                if(!water_||!water_lattice_||binding_.initial_body!=0u||!valid_terrain_profile(profile)
                    ||recipe_key.size()!=64u||!std::ranges::all_of(recipe_key,[](char c) {
                        return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))return {};
                return std::make_shared<LinkedTerrain>(binding_,profile,recipe_key);
            }
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
                return capture_frame(simulation_,accumulator_);
            }
        private:
            [[nodiscard]] host::Result capture_frame(const exploration::Simulation& simulation,double accumulator) const
            {
                if(!cloud_layers_ready_)return {host::Code::provider_failure,{}};
                host::Frame frame;
                const auto camera=simulation.interpolated_camera(accumulator/fixed_step);
                frame.seed=binding_.seed;frame.anchor=static_cast<std::uint32_t>(simulation.anchor());
                frame.target=static_cast<std::uint32_t>(simulation.target());
                frame.camera=vector(camera.position);frame.forward=vector(camera.forward);frame.up=vector(camera.up);
                frame.seconds=simulation.seconds();frame.time_scale=simulation.time_scale();
                frame.speed_factor=simulation.speed_factor();
                // Report the pilot clearance above the contact surface, excluding the camera offset.
                frame.altitude=(std::max)(0.0,simulation.altitude()-exploration::surface_clearance_meters);
                frame.speed=simulation.speed();
                frame.gravity_enabled=simulation.gravity_enabled();frame.level_assist=simulation.level_assist();
                frame.mode=static_cast<host::TravelMode>(simulation.mode());
                if(binding_.original_sunlight)
                {
                    // The modeled star is at the world origin. Lighting uses
                    // the active carrier's physical center at the scene time,
                    // so flying around the body cannot move its terminator.
                    const auto direction=-simulation.world().center(frame.anchor,frame.seconds);
                    const auto length=core::length(direction);
                    if(!std::isfinite(length)||length<=0.0)return {host::Code::invalid_frame,{}};
                    frame.sunlight=host::Sunlight{.direction_to_sun=vector(direction/length)};
                }
                for(std::uint32_t index=0;index<simulation.world().bodies().size();++index)
                {
                    if(!binding_.universe&&index!=binding_.initial_body)continue;
                    const auto& body=simulation.world().body(index);
                    frame.bodies[frame.body_count++]={body.id,binding_.universe?body.parent:-1,std::string{body.name},
                        vector(simulation.world().center(index,frame.seconds)),body.radius,
                        body.kind!=core::BodyKind::gas,meshes_[index],{},clouds::wind_degrees(frame.seconds),
                        clouds_[index],body.atmosphere_height};
                }
                frame.valid=true;
                return host::valid_frame(frame)?host::Result{host::Code::ready,std::move(frame)}
                    :host::Result{host::Code::invalid_frame,{}};
            }
        public:
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
                state.local_position = up * (ground + exploration::surface_clearance_meters);
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
            [[nodiscard]] host::Result start_in_flight(const host::FlightStart& start) override
            {
                const auto& world=simulation_.world();
                if(start.body>=world.bodies().size()||(!binding_.universe&&start.body!=binding_.initial_body))
                    return {host::Code::invalid_input,{}};
                const core::DVec3 local{start.body_local_position_meters[0],start.body_local_position_meters[1],
                    start.body_local_position_meters[2]};
                const core::DVec3 forward{start.body_local_forward[0],start.body_local_forward[1],start.body_local_forward[2]};
                const core::DVec3 up{start.body_local_up[0],start.body_local_up[1],start.body_local_up[2]};
                const double radius=core::length(local);
                const double alignment=forward.x*up.x+forward.y*up.y+forward.z*up.z;
                if(!core::finite(local)||!core::finite(forward)||!core::finite(up)||!std::isfinite(radius)||radius<=0.0
                    ||std::abs(core::length(forward)-1.0)>1e-6||std::abs(core::length(up)-1.0)>1e-6
                    ||std::abs(alignment)>1e-6)
                    return {host::Code::invalid_input,{}};
                // Admit the exact requested eye only if the existing flight
                // collision floor will accept it without a surface correction.
                if(radius<world.ground_radius(start.body,local/radius)+exploration::surface_clearance_meters)
                    return {host::Code::invalid_input,{}};
                auto state=simulation_.state();
                state.anchor=state.target=start.body;state.mode=exploration::TravelMode::flight;
                state.local_position=local;state.forward=forward;state.up=up;state.fall_velocity={};
                if(!exploration::valid_state(state))return {host::Code::invalid_input,{}};
                try
                {
                    auto replacement=simulation_;
                    replacement.restore(state);
                    auto result=capture_frame(replacement,0.0);
                    if(result.code==host::Code::ready)
                    {
                        simulation_=std::move(replacement);
                        accumulator_=0.0;
                    }
                    return result;
                }
                catch(...){return {host::Code::provider_failure,{}};}
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
