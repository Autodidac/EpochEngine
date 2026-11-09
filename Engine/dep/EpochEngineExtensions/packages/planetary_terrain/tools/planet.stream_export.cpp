// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// One supervised offline streaming step. No permanent worker, listener or viewer.
#include "planet.stream_cache.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <future>
#include <thread>
#include <iomanip>
#include <optional>
#include <span>
import editor.task_scheduler;
import planet.terrain;
import planet.streaming;
import planet.coverage;
import planet.clouds;
import asset.cloud_volume;
import asset.mesh_artifact;
import asset.texture_artifact;
import project.planetary_snapshot;
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
import water.planetary;
#endif

namespace core=epoch::extensions::planetary_terrain::core;
namespace stream=epoch::extensions::planetary_terrain::streaming;
namespace cache=planet_stream_cache;
namespace coverage=epoch::extensions::planetary_terrain::coverage;
namespace artifact=epochengine::asset::mesh;
namespace patches=epochengine::project_planetary_snapshot;
namespace tasks=epochengine::editor_tasks;
namespace fs=std::filesystem;
artifact::Limits planet_export_limits(std::uint64_t bytes);
bool planet_export_options(int& argc,char** argv,std::uint64_t& meshBytes,std::optional<double>& seconds,
    double* patch_pixels,double* lod_hysteresis,core::WorldConfig* world,std::string* patch_identity);
bool planet_write_mesh_frame(const fs::path& output,const coverage::Request& request,double scale,
    core::DVec3 origin,std::string_view key,std::size_t vertices,std::size_t indices);

namespace
{
    struct WorkerActivity
    {
        std::atomic<std::size_t> running{},peak{},started{};
        std::atomic<std::uint64_t> nanoseconds{};
        struct Scope
        {
            WorkerActivity& activity;
            std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
            explicit Scope(WorkerActivity& value):activity(value)
            {
                activity.started.fetch_add(1u,std::memory_order_relaxed);
                const auto count=activity.running.fetch_add(1u,std::memory_order_relaxed)+1u;
                auto previous=activity.peak.load(std::memory_order_relaxed);
                while(previous<count&&!activity.peak.compare_exchange_weak(previous,count,std::memory_order_relaxed)){}
            }
            ~Scope()
            {
                activity.nanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now()-start).count()),std::memory_order_relaxed);
                activity.running.fetch_sub(1u,std::memory_order_relaxed);
            }
        };
    };
    template<class T> bool parse(const char* text,T& value)
    {
        const std::string_view input{text};
        const auto result=std::from_chars(input.data(),input.data()+input.size(),value);
        return result.ec==std::errc{}&&result.ptr==input.data()+input.size();
    }
    bool prepare_cache(const fs::path& directory,stream::State& state,std::uint32_t seed,tasks::Scheduler& scheduler,WorkerActivity& activity,std::size_t& reused)
    {
        if(directory.empty()||directory.filename().empty()||!cache::plain_path(fs::absolute(directory).parent_path(),true))return false;
        if(!fs::exists(directory)&&!fs::create_directory(directory))return false;
        if(!cache::plain_path(directory,true))return false;
        const auto expected=cache::encode_options({seed,state.request.cells_per_tile,state.request.body_index,
            state.request.ocean_surface,state.world.config().reference_body,state.world.config().reference_radius_meters});
        const auto identity=directory/"cache.epoch_planet_stream";
        if(!fs::exists(identity))
        {
            if(!fs::is_empty(directory))return false;
            std::ofstream output{identity,std::ios::binary|std::ios::noreplace};output<<expected;output.close();if(!output)return false;
        }
        if(!cache::plain_path(identity,false)||fs::file_size(identity)!=expected.size())return false;
        std::ifstream input{identity,std::ios::binary};const std::string actual{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
        if(actual!=expected)return false;
        struct Entry {fs::path path;core::TileKey key;bool incomplete;};std::vector<Entry> entries;
        // Validate the whole small owner directory before deleting obsolete cache
        // patches. Unknown files and redirects are retained and cause refusal.
        for(const auto& item:fs::directory_iterator(directory))
        {
            if(item.path().filename()==identity.filename())continue;
            const auto file=item.path().filename().string();
            if(file=="plan.epoch_planet_stream"||file=="plan.epoch_planet_stream.pending")
            {
                if(!cache::plain_path(item.path(),false)||fs::file_size(item.path())>256u*1024u)return false;
                continue;
            }
            if(file=="cloud.epoch_mesh")
            {
                if(!cache::plain_path(item.path(),false)||fs::file_size(item.path())>2ull*1024u*1024u)return false;
                continue;
            }
            core::TileKey key;
            auto name=item.path().filename().string();
            const bool incomplete=name.ends_with(".pending");
            if(incomplete)name.resize(name.size()-8u);
            if(entries.size()>=4096u||!cache::parse_filename(name,key))return false;
            entries.push_back({item.path(),key,incomplete});
        }
        // Retain planner hysteresis as well as meshes across supervised steps.
        // The plan is bounded metadata, never an executable or an external path.
        const auto previous_path=directory/"plan.epoch_planet_stream";
        if(fs::exists(previous_path))
        {
            std::ifstream previous{previous_path,std::ios::binary};
            std::string line;std::size_t needed{},leaves{};
            if(!std::getline(previous,line)||line!="epoch.planet.stream-plan.v1"
                ||!(previous>>needed>>leaves)||needed>4096u||leaves>2048u||needed<6u||leaves<6u)return false;
            core::TerrainPlan history;
            const auto keys=[&](std::size_t count,std::vector<core::TileKey>& result){
                result.reserve(count);
                for(std::size_t i=0;i<count;++i)
                {
                    if(!(previous>>line))return false;
                    core::TileKey key;if(!cache::parse_filename(line,key)||key.body!=state.world.body(state.request.body_index).id
                        ||std::ranges::find(result,key)!=result.end())return false;
                    result.push_back(key);
                }
                return true;
            };
            if(!keys(needed,history.needed)||!keys(leaves,history.leaves)||(previous>>line))return false;
            for(const auto key:history.leaves)if(std::ranges::find(history.needed,key)==history.needed.end())return false;
            state.plan=std::move(history);
            if(stream::update(state,state.request)!=stream::Status::ready)return false;
        }
        // Decode wanted patches in the same bounded jobs that validate the
        // owner directory. read_patch still checks every ancestor immediately
        // before its read. No obsolete file is removed until every entry has
        // passed and all jobs have retired. Partial restored State is private
        // to this provider request and is discarded on any later failure.
        {
            struct CachedEntry {bool valid{};std::optional<core::Mesh> mesh;};
            struct CacheJob {core::TileKey key;tasks::Submission<CachedEntry> task;};
            std::vector<CacheJob> jobs;jobs.reserve(state.request.maximum_in_flight);
            std::vector<stream::Resident> decoded;decoded.reserve(state.plan.needed.size());
            const auto retire=[&] {
                for(auto& job:jobs)(void)scheduler.cancel(job.task.cancellation);
                for(auto& job:jobs)if(job.task.completion.valid())try{(void)job.task.completion.get();}catch(...){}
                jobs.clear();
            };
            try
            {
                std::size_t next{};
                while(next<entries.size()||!jobs.empty())
                {
                    while(next<entries.size()&&jobs.size()<state.request.maximum_in_flight)
                    {
                        const auto entry=entries[next++];
                        const bool wanted=!entry.incomplete&&stream::detail::wanted(state,entry.key);
                        const auto cells=state.request.cells_per_tile;const auto ocean=state.request.ocean_surface;
                        auto task=scheduler.submit_cancellable("Validate/read cached planet patch",
                            [entry,wanted,cells,ocean,&activity](std::stop_token stop)->CachedEntry {
                                WorkerActivity::Scope measured{activity};
                                if(stop.stop_requested())return {};
                                if(!wanted)return {!stop.stop_requested()&&cache::plain_path(entry.path,false),{}};
                                core::Mesh mesh;
                                if(!cache::read_patch(entry.path,entry.key,cells,mesh,ocean)||stop.stop_requested())return {};
                                return {true,std::optional<core::Mesh>{std::move(mesh)}};
                            });
                        jobs.push_back({entry.key,std::move(task)});
                    }
                    // Refill a completed read's slot even when an earlier
                    // read is slow. Restoration below still follows the plan,
                    // and every read retires before obsolete files are removed.
                    const auto ready=std::ranges::find_if(jobs,[](const CacheJob& job) {
                        return job.task.completion.wait_for(std::chrono::milliseconds{0})==std::future_status::ready;
                    });
                    if(ready==jobs.end())
                    {
                        (void)jobs.front().task.completion.wait_for(std::chrono::milliseconds{1});
                        continue;
                    }
                    auto result=ready->task.completion.get();
                    if(!result.valid){retire();return false;}
                    if(result.mesh)
                    {
                        decoded.push_back({ready->key,std::move(*result.mesh)});
                    }
                    jobs.erase(ready);(void)scheduler.maintain();
                }
            }
            catch(...){retire();throw;}
            // Preserve the exact admitted plan order, independently of directory
            // enumeration or worker completion order. Reserved retained bounds
            // cover decoded meshes; at most four encoded patch reads coexist.
            for(const auto key:state.plan.needed)
            {
                const auto at=std::ranges::find(decoded,key,&stream::Resident::key);
                if(at==decoded.end())continue;
                if(stream::restore(state,key,std::move(at->mesh))!=stream::Status::ready)return false;
                ++reused;
            }
        }
        const auto interrupted=directory/"plan.epoch_planet_stream.pending";
        if(fs::exists(interrupted)&&!fs::remove(interrupted))return false;
        for(const auto& entry:entries)
            if((entry.incomplete||!stream::detail::wanted(state,entry.key))&&!fs::remove(entry.path))return false;
        return true;
    }
    bool save_plan(const fs::path& directory,const stream::State& state)
    {
        const auto path=directory/"plan.epoch_planet_stream",pending=directory/"plan.epoch_planet_stream.pending";
        std::ofstream output{pending,std::ios::binary|std::ios::noreplace};
        output<<"epoch.planet.stream-plan.v1\n"<<state.plan.needed.size()<<' '<<state.plan.leaves.size()<<'\n';
        for(const auto key:state.plan.needed)output<<cache::filename(key)<<'\n';
        for(const auto key:state.plan.leaves)output<<cache::filename(key)<<'\n';
        output.close();if(!output)return false;
        if(fs::exists(path)&&!fs::remove(path))return false;
        fs::rename(pending,path);return true;
    }
    // Transport includes ancestors as well as the selected cut. If its actual
    // clipped geometry exceeds the artifact budget, merge complete deepest
    // sibling quartets; never discard individual faces or raise the budget.
    // Choosing only the deepest level preserves the balanced cut's 2:1 rule.
    bool coarsen_snapshot(stream::State& state)
    {
        auto cut=stream::draw_cut(state);
        const auto initial=cut.size();
        if(initial<=6u||!state.in_flight.empty())return false;
        const auto target=(std::max)(std::size_t{6u},initial-initial/4u);
        while(cut.size()>target)
        {
            std::optional<core::TileKey> selected;
            std::uint8_t deepest{};double farthest{-1.0};
            const auto camera=state.request.camera-state.world.center(state.request.body_index,state.request.seconds);
            for(const auto key:cut)
            {
                if(!key.level||key.level<deepest)continue;
                const auto parent=core::parent(key);
                const auto siblings=core::children(parent);
                if(!stream::detail::find(state,parent)||!std::ranges::all_of(siblings,
                    [&](auto sibling){return std::ranges::find(cut,sibling)!=cut.end();}))continue;
                const auto* resident=stream::detail::find(state,parent);
                const auto offset=resident->mesh.anchor-camera;
                const double distance=offset.x*offset.x+offset.y*offset.y+offset.z*offset.z;
                if(key.level>deepest||distance>farthest)
                {selected=parent;deepest=key.level;farthest=distance;}
            }
            if(!selected)break;
            const auto siblings=core::children(*selected);
            std::erase_if(cut,[&](auto key){return std::ranges::find(siblings,key)!=siblings.end();});
            cut.push_back(*selected);
        }
        if(cut.size()==initial)return false;
        state.plan.leaves=std::move(cut);
        std::erase_if(state.plan.needed,[&](auto key){return !std::ranges::any_of(state.plan.leaves,
            [&](auto leaf){return core::contains(key,leaf);});});
        std::erase_if(state.resident,[&](const auto& resident){return !stream::detail::wanted(state,resident.key);});
        return true;
    }
    int write_snapshot(const fs::path& output,const stream::State& state,std::uint64_t request_id,std::size_t built,std::size_t reused,artifact::Limits limits,std::optional<double> simulation_seconds,std::string_view patch_identity,tasks::Scheduler& scheduler,WorkerActivity& activity,bool metric_frame=false, const std::shared_ptr<const artifact::Artifact>& cloud_mesh={},const std::optional<epochengine::asset::cloud_volume::Recipe>& cloud_density={})
    {
        const auto snapshot_started=std::chrono::steady_clock::now();
        auto assembly_done=snapshot_started,validation_done=snapshot_started,encoding_done=snapshot_started,verification_done=snapshot_started;
        const auto stats=stream::stats(state);
        const auto cut=stream::draw_cut(state);
        const auto origin=state.world.center(state.request.body_index,state.request.seconds);
        // epoch.planet.stream-frame.v1: retain metre-scale fine geometry near
        // the admitted camera; planner/cache coordinates remain body-local.
        // epoch.planet.stream-origin.v2: shared, bounded metric origin. Near
        // the surface it remains the exact camera; a far-orbit camera cannot
        // push otherwise valid terrain outside the mesh codec's position range.
        const auto camera_origin=state.request.camera-origin;
        const auto metric_origin=patches::metric_source_origin(
            {camera_origin.x,camera_origin.y,camera_origin.z},true);
        if(metric_frame&&!metric_origin)return 5;
        const auto artifact_origin=metric_frame?core::DVec3{
            (*metric_origin)[0],(*metric_origin)[1],(*metric_origin)[2]}:core::DVec3{};
        const double scale=metric_frame?1.0:10.0/state.world.body(state.request.body_index).radius;
        const auto& world=state.world.config();const bool sized=world!=core::WorldConfig{};
        std::string key;
        std::size_t nv{},ni{};
        const auto fail=[&](std::string_view stage,std::size_t vertices=0u,std::size_t indices=0u,int detail=0) {
            std::ofstream evidence{output/"terrain.failure",std::ios::binary|std::ios::trunc};
            if(evidence)evidence<<"stage="<<stage<<"\nrequest_id="<<request_id<<"\ndetail="<<detail
                <<"\nvertices="<<vertices<<"\nindices="<<indices<<"\nmaximum_bytes="<<limits.maximum_bytes<<'\n';
            return 5;
        };
        if(stats.complete_coverage)
        {
            std::vector<artifact::Vertex> vertices;
            std::vector<std::uint32_t> indices;
            patches::Metadata metadata;
            if(cloud_mesh||cloud_density)metadata.cloud=patches::CloudLayer{static_cast<std::uint32_t>(state.request.body_index),
                {origin.x,origin.y,origin.z},state.world.body(state.request.body_index).radius,
                cloud_density?cloud_density->wind_period_seconds:604800.0,cloud_mesh,cloud_density};
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
            epoch::extensions::planetary_terrain::water::Surface ocean;
            if(state.request.ocean_surface&&!ocean.evaluate(state.world.seed(),state.request.seconds,limits.maximum_bytes))return fail("ocean evaluation");
#else
            if(state.request.ocean_surface)return fail("ocean provider unavailable");
#endif
            std::vector<core::TileKey> exported=cut;
            if(!patch_identity.empty())
            {
                // Explicit new transport only. The ordinary legacy artifact is
                // still the draw cut and can never expose hidden ancestors.
                exported.clear();
                for(const auto& resident:state.resident)
                    if(std::ranges::any_of(cut,[&](const auto leaf){return core::contains(resident.key,leaf);}))exported.push_back(resident.key);
                metadata.identity.source_hash=patch_identity.substr(0,71u);
                metadata.identity.recipe_key=patch_identity.substr(72u);
                metadata.identity.request_id=request_id;
                if(state.request.ocean_surface)
                {metadata.water_radius_meters=state.world.body(state.request.body_index).radius;metadata.water_meters_to_artifact=scale;
                    metadata.water_origin_meters={artifact_origin.x,artifact_origin.y,artifact_origin.z};}
            }
            struct PatchGeometry
            {
                std::vector<artifact::Vertex> vertices;
                std::vector<std::uint32_t> indices;
                std::vector<std::array<float,3>> deltas;
            };
            // Each patch has independent conversion and FFT clipping. Keep the
            // existing scheduler's four-worker bound and merge in source order.
            // Reserve the worst clipping expansion before starting each job;
            // worker scratch is charged alongside retained terrain residency.
            const std::uint64_t scratch_available=state.request.maximum_bytes
                -stream::detail::overhead-stream::resident_bytes(state);
            {
                struct AssemblyJob {core::TileKey key;std::uint64_t scratch;tasks::Submission<std::optional<PatchGeometry>> task;};
                std::vector<AssemblyJob> jobs;
                jobs.reserve(state.request.maximum_in_flight);
                std::uint64_t scratch_reserved{};
                const auto retire=[&] {
                    for(auto& job:jobs)(void)scheduler.cancel(job.task.cancellation);
                    for(auto& job:jobs)if(job.task.completion.valid())
                        try{(void)job.task.completion.get();}catch(...){}
                    jobs.clear();
                };
                try
                {
                    std::size_t next{};
                    while(next<exported.size()||!jobs.empty())
                    {
                      while(next<exported.size()&&jobs.size()<state.request.maximum_in_flight)
                      {
                        const auto tile=exported[next];
                        const auto* resident=stream::detail::find(state,tile);
                        if(!resident){retire();return fail("selected patch unavailable",vertices.size(),indices.size());}
                        const auto* mesh=&resident->mesh;
                        const bool with_deltas=!patch_identity.empty();
                        const std::size_t maximum_vertices=mesh->vertices.size()+2u*mesh->indices.size()/3u;
                        const std::size_t maximum_indices=2u*mesh->indices.size();
                        const std::uint64_t side=state.request.cells_per_tile+1u;
                        const std::uint64_t water_scratch=state.request.ocean_surface
                            ?mesh->indices.size()*sizeof(std::uint32_t)+mesh->vertices.size()*sizeof(std::uint32_t)
                                +side*side*(sizeof(core::DVec3)+sizeof(std::uint8_t)):0u;
                        const std::uint64_t scratch=maximum_vertices*(sizeof(artifact::Vertex)+(with_deltas?sizeof(std::array<float,3>):0u))
                            +maximum_indices*sizeof(std::uint32_t)+water_scratch+sizeof(artifact::Vertex);
                        if(scratch>scratch_available-scratch_reserved)
                        {if(jobs.empty())return fail("snapshot working budget");break;}
                        scratch_reserved+=scratch;
                        const auto body=state.world.body(state.request.body_index);
                        const auto cells=state.request.cells_per_tile;
                        auto task=scheduler.submit_cancellable("Assemble planet patch",
                            [mesh,tile,body,cells,scale,artifact_origin,with_deltas,maximum_vertices,maximum_indices,scratch,&activity
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
                            ,&ocean,wet=state.request.ocean_surface
#endif
                            ](std::stop_token stop)->std::optional<PatchGeometry> {
                                WorkerActivity::Scope measured{activity};
                                if(stop.stop_requested())return {};
                                PatchGeometry result;
                                result.vertices.reserve(maximum_vertices);result.indices.reserve(maximum_indices);
                                if(with_deltas)result.deltas.reserve(maximum_vertices);
                                for(const auto& vertex:mesh->vertices)
                                {
                                    const auto relative=(mesh->anchor+core::DVec3{vertex.position}-artifact_origin)*scale;
                                    result.vertices.push_back({{static_cast<float>(relative.x),static_cast<float>(relative.y),static_cast<float>(relative.z)},
                                        {vertex.normal.x,vertex.normal.y,vertex.normal.z},{},{vertex.color[0],vertex.color[1],vertex.color[2]}});
                                    if(with_deltas)result.deltas.push_back({static_cast<float>(vertex.parent_delta.x*scale),
                                        static_cast<float>(vertex.parent_delta.y*scale),static_cast<float>(vertex.parent_delta.z*scale)});
                                }
                                result.indices.insert(result.indices.end(),mesh->indices.begin(),mesh->indices.end());
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
                                if(wet&&!ocean.apply_patch(result.vertices,result.indices,0u,0u,*mesh,body,tile,cells,scale,core::DVec3{}-artifact_origin,scratch,
                                    with_deltas?&result.deltas:nullptr))return {};
#endif
                                if(stop.stop_requested()||result.vertices.size()>maximum_vertices||result.indices.size()>maximum_indices
                                    ||result.vertices.capacity()*sizeof(artifact::Vertex)+result.indices.capacity()*sizeof(std::uint32_t)
                                        +result.deltas.capacity()*sizeof(std::array<float,3>)>scratch)return {};
                                return result;
                            });
                        jobs.push_back({tile,scratch,std::move(task)});++next;
                      }
                        auto& job=jobs.front();
                        auto patch_geometry=job.task.completion.get();
                        if(!patch_geometry){retire();return fail("patch assembly",vertices.size(),indices.size());}
                        const auto& chunk=*patch_geometry;
                        if(vertices.size()+chunk.vertices.size()>limits.maximum_vertices
                            ||indices.size()+chunk.indices.size()>limits.maximum_indices
                            ||48u+(vertices.size()+chunk.vertices.size())*44u+(indices.size()+chunk.indices.size())*4u>limits.maximum_bytes)
                        {retire();(void)fail("terrain assembly limit",vertices.size()+chunk.vertices.size(),indices.size()+chunk.indices.size());return 10;}
                        const auto base=static_cast<std::uint32_t>(vertices.size());
                        if(!patch_identity.empty())
                        {
                            patches::Patch patch;
                            const auto convert=[](core::TileKey key){return patches::TileKey{key.x,key.y,key.body,key.face,key.level};};
                            patch.key=convert(job.key);const auto neighbors=core::neighbors(job.key);
                            for(unsigned i=0;i<4u;++i)patch.neighbors[i]=convert(neighbors[i]);
                            patch.first_vertex=base;patch.vertex_count=static_cast<std::uint32_t>(chunk.vertices.size());
                            patch.first_index=static_cast<std::uint32_t>(indices.size());patch.index_count=static_cast<std::uint32_t>(chunk.indices.size());
                            patch.target=std::ranges::find(cut,job.key)!=cut.end();
#if defined(EPOCH_PLANET_HAS_WATER_EXTENSION)
                            // epoch.planet.water-recipe.v2: selected fine ocean
                            // patches retain their typed undeformed basis.
                            if(state.request.ocean_surface&&!epoch::extensions::planetary_terrain::water::append_recipe(
                                metadata,patch,stream::detail::find(state,job.key)->mesh,
                                state.world.body(state.request.body_index),job.key,state.request.cells_per_tile,scale))
                            {retire();return fail("ocean recipe",vertices.size(),indices.size());}
#endif
                            metadata.patches.push_back(patch);
                            metadata.parent_delta.insert(metadata.parent_delta.end(),chunk.deltas.begin(),chunk.deltas.end());
                        }
                        vertices.insert(vertices.end(),chunk.vertices.begin(),chunk.vertices.end());
                        for(const auto index:chunk.indices)indices.push_back(base+index);
                        scratch_reserved-=job.scratch;
                        jobs.erase(jobs.begin());(void)scheduler.maintain();
                    }
                }
                catch(...){retire();throw;}
            }
            assembly_done=std::chrono::steady_clock::now();
            const auto built_artifact=artifact::build_artifact(vertices,indices,limits);
            if(!built_artifact)return fail("mesh validation",vertices.size(),indices.size(),static_cast<int>(built_artifact.code));
            validation_done=std::chrono::steady_clock::now();
            const auto bytes=artifact::encode_artifact(*built_artifact.artifact);if(bytes.empty())return fail("mesh encoding",vertices.size(),indices.size());
            encoding_done=std::chrono::steady_clock::now();
            std::ofstream mesh_file{output/"terrain.epoch_mesh",std::ios::binary|std::ios::noreplace};
            mesh_file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));mesh_file.close();if(!mesh_file)return 7;
            // Verify exactly the bytes written before publishing the receipt.
            std::ifstream check{output/"terrain.epoch_mesh",std::ios::binary|std::ios::ate};
            if(!check||check.tellg()!=static_cast<std::streamoff>(bytes.size()))return 7;
            check.seekg(0);
            if(!cache::matches_bytes(check,bytes))return 7;
            verification_done=std::chrono::steady_clock::now();
            key=epochengine::asset::texture::content_hash_hex(built_artifact.artifact->key());nv=vertices.size();ni=indices.size();
            if(!patch_identity.empty())
            {
                metadata.identity.artifact_key=built_artifact.artifact->key();
                const auto sidecar=patches::encode(metadata,*built_artifact.artifact,{limits});if(sidecar.empty())return fail("patch metadata",vertices.size(),indices.size());
                std::ofstream patch_file{output/"terrain.epoch_planet_patch",std::ios::binary|std::ios::noreplace};
                patch_file.write(reinterpret_cast<const char*>(sidecar.data()),static_cast<std::streamsize>(sidecar.size()));
                patch_file.close();if(!patch_file)return 7;
            }
            std::ofstream recipe{output/"terrain.preview.json",std::ios::noreplace};
            recipe<<std::setprecision(17)<<"{\"schema\":\""<<(sized?"epoch.planet.mesh-preview.v2":"epoch.planet.mesh-preview.v1")<<"\",\"mode\":\"planet\",\"seed\":"<<state.world.seed()
                <<",\"body_index\":"<<state.request.body_index<<",\"cells_per_tile\":"<<state.request.cells_per_tile
                <<",\"tiles_per_axis\":1,\"tile_span_meters\":128,\"seconds\":"<<state.request.seconds
                <<",\"preview_only\":true,\"meters_to_preview_units\":"<<scale<<",\"origin_meters\":["<<origin.x+artifact_origin.x<<','<<origin.y+artifact_origin.y<<','<<origin.z+artifact_origin.z
                <<"],\"mesh_file\":\"terrain.epoch_mesh\",\"content_key\":\""<<key<<"\",\"vertices\":"<<nv<<",\"indices\":"<<ni<<",\"patches\":[";
            bool first=true;
            for(const auto tile:cut)
            {
                if(!first)recipe<<',';first=false;
                const auto& mesh=stream::detail::find(state,tile)->mesh;
                recipe<<"{\"body\":"<<unsigned(tile.body)<<",\"body_center\":["<<origin.x<<','<<origin.y<<','<<origin.z
                    <<"],\"anchor\":["<<mesh.anchor.x<<','<<mesh.anchor.y<<','<<mesh.anchor.z
                    <<"],\"planar\":false,\"grid\":[0,0],\"tile\":["<<tile.x<<','<<tile.y<<','<<unsigned(tile.face)<<','<<unsigned(tile.level)<<"]}";
            }
            recipe<<']';
            if(state.request.ocean_surface)recipe<<",\"ocean_surface\":true";
            if(sized)recipe<<",\"size_reference_body\":"<<world.reference_body<<",\"reference_radius_meters\":"<<world.reference_radius_meters;
            recipe<<"}\n";recipe.close();if(!recipe)return 7;
            coverage::Request frame;frame.mode=coverage::Mode::planet;frame.seed=state.world.seed();
            frame.body_index=state.request.body_index;frame.cells_per_tile=state.request.cells_per_tile;
            frame.tiles_per_axis=1;frame.seconds=state.request.seconds;frame.world=world;
            if(!planet_write_mesh_frame(output,frame,scale,origin+artifact_origin,key,nv,ni))return 7;
        }
        {
            const auto milliseconds=[](auto from,auto to){return std::chrono::duration<double,std::milli>(to-from).count();};
            std::ofstream timing{output/"terrain.snapshot-performance",std::ios::binary|std::ios::noreplace};
            if(timing)timing<<std::setprecision(17)<<"epoch.planet.snapshot-performance.v1\nrequest_id="<<request_id
                <<"\nassembly_ms="<<milliseconds(snapshot_started,assembly_done)
                <<"\nartifact_validation_ms="<<milliseconds(assembly_done,validation_done)
                <<"\nencoding_ms="<<milliseconds(validation_done,encoding_done)
                <<"\nwrite_verify_ms="<<milliseconds(encoding_done,verification_done)
                <<"\nmetadata_ms="<<milliseconds(verification_done,std::chrono::steady_clock::now())
                <<"\nvertices="<<nv<<"\nindices="<<ni<<'\n';
        }
        // Receipt is last: its existence means this entire step has finished.
        std::ofstream receipt{output/"terrain.stream",std::ios::binary|std::ios::noreplace};
        const bool lod_controls=state.request.lod.patch_pixels!=210.0||state.request.lod.hysteresis!=0.15;
        receipt<<std::setprecision(17)<<(state.request.view&&state.request.view->up?"epoch.planet.stream.v9\n":state.request.view?"epoch.planet.stream.v8\n":metric_frame?"epoch.planet.stream.v7\n":!patch_identity.empty()?"epoch.planet.stream.v6\n":sized?"epoch.planet.stream.v5\n":lod_controls?"epoch.planet.stream.v4\n":simulation_seconds?"epoch.planet.stream.v3\n"
            :state.request.ocean_surface?"epoch.planet.stream.v2\n":"epoch.planet.stream.v1\n")
            <<"request_id="<<request_id
            <<"\nsnapshot_ready="<<(stats.complete_coverage?1:0)<<"\nbuilt="<<built<<"\nreused="<<reused
            <<"\nresident="<<stats.resident_patches<<"\npending="<<stats.pending_patches<<"\ndraw="<<stats.draw_patches
            <<"\nmin_level="<<unsigned(stats.minimum_level)<<"\nmax_level="<<unsigned(stats.maximum_level)
            <<"\nresident_bytes="<<stats.resident_bytes<<"\nmaximum_bytes="<<stats.maximum_bytes
            <<"\nmeters_to_preview_units="<<scale;
        if(metric_frame)receipt<<"\nbody_radius_meters="<<state.world.body(state.request.body_index).radius
            <<"\nsource_origin_x_meters="<<artifact_origin.x<<"\nsource_origin_y_meters="<<artifact_origin.y<<"\nsource_origin_z_meters="<<artifact_origin.z;
        receipt<<"\nmesh_file="<<(stats.complete_coverage?"terrain.epoch_mesh":"")
            <<"\ncontent_key="<<key<<"\nvertices="<<nv<<"\nindices="<<ni<<'\n';
        if(sized||lod_controls||simulation_seconds)receipt<<"ocean_surface="<<(state.request.ocean_surface?1u:0u)
            <<"\nsimulation_seconds="<<simulation_seconds.value_or(0.0)<<'\n';
        else if(state.request.ocean_surface)receipt<<"ocean_surface=1\n";
        if(sized||lod_controls)receipt<<"patch_pixels="<<state.request.lod.patch_pixels
            <<"\nlod_hysteresis="<<state.request.lod.hysteresis<<'\n';
        if(sized)receipt<<"size_reference_body="<<world.reference_body<<"\nreference_radius_meters="<<world.reference_radius_meters<<'\n';
        if(!patch_identity.empty())receipt<<"patch_file="<<(stats.complete_coverage?"terrain.epoch_planet_patch":"")<<'\n';
        if(state.request.view)receipt<<"view_forward_x="<<state.request.view->forward.x
            <<"\nview_forward_y="<<state.request.view->forward.y<<"\nview_forward_z="<<state.request.view->forward.z
            <<"\nview_aspect="<<state.request.view->aspect_ratio<<'\n';
        if(state.request.view&&state.request.view->up)receipt<<"view_up_x="<<state.request.view->up->x
            <<"\nview_up_y="<<state.request.view->up->y<<"\nview_up_z="<<state.request.view->up->z<<'\n';
        receipt.close();return receipt?0:7;
    }
}
int planet_stream_main(int argc,char** argv)
{
    // --stream-step new-output cache seed cells body cx cy cz height fov
    //               max-level max-leaves resident-bytes max-new request-id
    std::uint64_t meshBytes=4ull*1024u*1024u;
    std::optional<double> seconds;
    stream::Request request;
    core::WorldConfig world;
    std::string patch_identity;
    // epoch.planet.cloud-layer.v1: explicit opt-in preserves older host bytes.
    // epoch.planet.cloud-density.v1: v5 metric density; no lobe mesh is emitted.
    const bool cloud_density_transport=argc>1&&std::string_view{argv[argc-1]}=="--cloud-density-v1";
    const bool cloud_transport=cloud_density_transport||(argc>1&&std::string_view{argv[argc-1]}=="--cloud-layer-v1");
    if(cloud_transport)--argc;
    // Additive capability: epoch.planet.view-footprint.v1 carries the camera
    // basis for altitude-blended forward/45-degree-down/radial-nadir refinement.
    // The original
    // epoch.planet.view-cone.v1 packet retains its exact single-ray behavior.
    const bool footprint=argc>=3&&std::string_view{argv[argc-2]}=="--view-footprint-v1";
    if(argc>=3&&(footprint||std::string_view{argv[argc-2]}=="--view-cone-v1"))
    {
        std::string_view packet{argv[argc-1]};
        std::array<double,7> values{};
        const std::size_t count=footprint?7u:4u;
        for(std::size_t i=0;i<count;++i)
        {
            const auto end=packet.find(',');
            if((i+1u<count)!=(end!=std::string_view::npos))return 2;
            const auto component=packet.substr(0,end);
            const auto decoded=std::from_chars(component.data(),component.data()+component.size(),values[i]);
            if(decoded.ec!=std::errc{}||decoded.ptr!=component.data()+component.size()||!std::isfinite(values[i]))return 2;
            if(end!=std::string_view::npos)packet.remove_prefix(end+1u);
        }
        request.view=core::TerrainView{{values[0],values[1],values[2]},values[3]};
        if(footprint)request.view->up=core::DVec3{values[4],values[5],values[6]};
        argc-=2;
    }
    const bool metric_frame=argc>1&&std::string_view{argv[argc-1]}=="--metric-frame-v1";
    if(metric_frame)--argc;
    if(!planet_export_options(argc,argv,meshBytes,seconds,
        &request.lod.patch_pixels,&request.lod.hysteresis,&world,&patch_identity))return 2;
    if((request.view||cloud_transport)&&(!metric_frame||patch_identity.empty()))return 2;
    const auto limits=planet_export_limits(meshBytes);
    if(limits.maximum_bytes==0u|| (argc!=17&&argc!=18))return 2;
    std::uint32_t seed{},level{},maximum_new{};
    std::uint64_t request_id{};
    request.seconds=seconds.value_or(0.0);
    if(argc==18)
    {
        std::uint32_t ocean{};
        if(!parse(argv[17],ocean)||ocean>1u)return 2;
        request.ocean_surface=ocean!=0u;
    }
    if(!parse(argv[4],seed)||!parse(argv[5],request.cells_per_tile)||!parse(argv[6],request.body_index)
        ||!parse(argv[7],request.camera.x)||!parse(argv[8],request.camera.y)||!parse(argv[9],request.camera.z)
        ||!parse(argv[10],request.viewport_height)||!parse(argv[11],request.vertical_fov)||!parse(argv[12],level)
        ||!parse(argv[13],request.lod.max_leaves)||!parse(argv[14],request.maximum_bytes)
        ||!parse(argv[15],maximum_new)||!parse(argv[16],request_id)
        // epoch.planet.stream-batch.v2: bounded supervised batches reuse the
        // admitted cache/snapshot setup; concurrent patch jobs stay at four.
        ||level>20u||maximum_new<1u||maximum_new>128u||request_id==0u)return 2;
    request.lod.max_level=static_cast<std::uint8_t>(level);
    request.maximum_resident_patches=4096;
    request.maximum_in_flight=(std::min)(static_cast<std::size_t>(maximum_new),
        std::clamp<std::size_t>(std::thread::hardware_concurrency()/3u,1u,4u));
    // The existing artifact codec's real limits also bound this transport. This
    // conservative bound includes skirts and all possible selected-body leaves.
    if(request.cells_per_tile<2u||request.cells_per_tile>64u
        ||request.lod.max_leaves<6u||request.lod.max_leaves>2048u)return 2;
    const std::uint64_t cells=request.cells_per_tile,side=cells+1u;
    const std::uint64_t leaves=request.lod.max_leaves;
    const std::uint64_t layers=request.ocean_surface?2u:1u;
    if(layers*leaves*(side*side+4u*cells)>limits.maximum_vertices
        ||layers*leaves*(6u*cells*cells+24u*cells)>limits.maximum_indices
        ||48u+layers*leaves*((side*side+4u*cells)*44u+(6u*cells*cells+24u*cells)*4u)>limits.maximum_bytes)return 2;
    try
    {
        const auto step_started=std::chrono::steady_clock::now();
        stream::State state{seed,world};
        if(std::string_view{argv[1]}=="--stream-step-preview")
        {
            if(request.body_index>=state.world.bodies().size())return 3;
            request.camera=state.world.center(request.body_index,request.seconds)
                +request.camera*(state.world.body(request.body_index).radius/10.0);
        }
        const auto admitted=stream::update(state,request);if(admitted!=stream::Status::ready)return 3;
        const auto planned=std::chrono::steady_clock::now();
        const fs::path output{argv[2]},directory{argv[3]};
        // Refuse a repeated output before touching the reusable cache.
        if(!cache::new_output(output))return 6;
        WorkerActivity activity;
        tasks::Scheduler scheduler{{.worker_count=request.maximum_in_flight,.retained_finished_nodes=32u}};
        std::size_t reused{},built{};
        if(!prepare_cache(directory,state,seed,scheduler,activity,reused))return 9;
        std::shared_ptr<const artifact::Artifact> cloud_mesh;
        std::optional<epochengine::asset::cloud_volume::Recipe> cloud_density;
        if(cloud_density_transport)
        {
            const auto& body=state.world.body(state.request.body_index);
            cloud_density=epoch::extensions::planetary_terrain::clouds::density_recipe(body);
            if(epoch::extensions::planetary_terrain::clouds::supports(body)&&!cloud_density)return 9;
        }
        if(cloud_transport&&!cloud_density_transport)
        {
            const auto& body=state.world.body(request.body_index);
            if(epoch::extensions::planetary_terrain::clouds::supports(body))
            {
                // The existing recipe/source-isolated cache owns this immutable
                // layer. Time changes its transform, never its mesh.
                const auto cloud_file=directory/"cloud.epoch_mesh";
                const auto cached=artifact::read_artifact_file(cloud_file,{24'000u,140'000u,2ull*1024u*1024u});
                if(cached)cloud_mesh=cached.artifact;
                else
                {
                    cloud_mesh=epoch::extensions::planetary_terrain::clouds::build(body);
                    if(!cloud_mesh)return 9;
                    const auto encoded=artifact::encode_artifact(*cloud_mesh);
                    std::ofstream file{cloud_file,std::ios::binary|std::ios::trunc};
                    file.write(reinterpret_cast<const char*>(encoded.data()),static_cast<std::streamsize>(encoded.size()));file.flush();
                    if(!file)return 9;
                }
            }
        }
        const auto prepared=std::chrono::steady_clock::now();
        // Cache admission and decoding are fused above, rather than traversing
        // every path a second time in a separate restore wave.
        const auto restored=std::chrono::steady_clock::now();
        if(stream::stats(state).pending_patches!=0u)
        {
            struct Job {stream::Work work; tasks::Submission<core::Mesh> task;};
            std::vector<Job> jobs; jobs.reserve(request.maximum_in_flight);
            // An error cannot retire the provider while its jobs still execute.
            // Discard results before releasing their individual reservations.
            const auto retire=[&] {
                for(auto& job:jobs) (void)scheduler.cancel(job.task.cancellation);
                for(auto& job:jobs)
                {
                    if(job.task.completion.valid())
                        try {(void)job.task.completion.get();} catch(...) {}
                    (void)stream::abandon(state,job.work);
                }
                jobs.clear();
            };
            try
            {
                while(built<maximum_new)
                {
                    // Refill the bounded FIFO after each ordered completion;
                    // a slow sibling cannot hold every other worker idle.
                    // Workers never access State/world/cache. Completed meshes
                    // remain reservation-owned until the owner commits them.
                    while(jobs.size()<request.maximum_in_flight&&built+jobs.size()<maximum_new)
                    {
                        const auto work=stream::next_work(state); if(!work)break;
                        const auto body=state.world.body(work->key.body);
                        try
                        {
                            auto task=scheduler.submit_cancellable("Planet patch",
                                [body,work=*work,&activity](std::stop_token stop) {WorkerActivity::Scope measured{activity};return stream::build(body,work,stop);});
                            jobs.push_back({*work,std::move(task)});
                        }
                        catch(...) {(void)stream::abandon(state,*work);throw;}
                    }
                    if(jobs.empty())break;
                    if(!jobs.empty())
                    {
                        // Execution stays parallel, while cache/snapshot order
                        // follows the admitted claims rather than timing races.
                        const auto ready=jobs.begin();
                        if(ready->task.completion.wait_for(std::chrono::milliseconds{0})!=std::future_status::ready)
                        {
                            (void)jobs.front().task.completion.wait_for(std::chrono::milliseconds{1});
                            continue;
                        }
                        auto mesh=ready->task.completion.get();
                        if(stream::complete(state,ready->work,std::move(mesh))!=stream::Status::ready)
                        {retire();return 5;}
                        const auto key=ready->work.key;
                        jobs.erase(ready);
                        const auto path=directory/cache::filename(key);
                        const auto pending=fs::path{path.string()+".pending"};
                        const auto* resident=stream::detail::find(state,key);
                        if(!resident||!cache::write_patch(pending,key,resident->mesh)) {retire();return 9;}
                        // Cache publication remains on the State owner, after
                        // structural validation and current-generation admission.
                        fs::rename(pending,path);
                        ++built;
                    }
                    (void)scheduler.maintain();
                }
            }
            catch(...) {retire();throw;}
        }
        const auto meshed=std::chrono::steady_clock::now();
        if(!save_plan(directory,state))return 9;
        const auto saved=std::chrono::steady_clock::now();
        auto result=write_snapshot(output,state,request_id,built,reused,limits,seconds,patch_identity,scheduler,activity,metric_frame,cloud_mesh,cloud_density);
        const auto requested_draw=stream::stats(state).draw_patches;
        std::size_t budget_reductions{};
        while(result==10&&budget_reductions<8u&&coarsen_snapshot(state))
        {
            ++budget_reductions;
            result=write_snapshot(output,state,request_id,built,reused,limits,seconds,patch_identity,scheduler,activity,metric_frame,cloud_mesh,cloud_density);
        }
        if(result==10)return 5; // Refuse if the bounded retries cannot admit a complete cut.
        if(result==0&&budget_reductions)
        {
            if(!save_plan(directory,state))return 9;
            fs::remove(output/"terrain.failure");
            std::ofstream budget{output/"terrain.snapshot-budget",std::ios::binary|std::ios::noreplace};
            budget<<"epoch.planet.snapshot-budget.v1\nrequested_draw="<<requested_draw
                <<"\nadmitted_draw="<<stream::stats(state).draw_patches<<"\nreductions="<<budget_reductions
                <<"\nmaximum_bytes="<<limits.maximum_bytes<<'\n';
            budget.close();if(!budget)return 7;
        }
        if(result==0)
        {
            const auto milliseconds=[](auto from,auto to){return std::chrono::duration<double,std::milli>(to-from).count();};
            const auto stats=stream::stats(state);
            std::ofstream timing{output/"terrain.performance",std::ios::binary|std::ios::noreplace};
            if(timing)timing<<std::setprecision(17)<<"epoch.planet.stream-performance.v1\nrequest_id="<<request_id
                <<"\nbody="<<request.body_index<<"\ncamera_x="<<request.camera.x<<"\ncamera_y="<<request.camera.y<<"\ncamera_z="<<request.camera.z
                <<"\nsimulation_seconds="<<request.seconds<<"\nplanning_ms="<<milliseconds(step_started,planned)
                <<"\ncache_prepare_ms="<<milliseconds(planned,prepared)<<"\ncache_restore_ms="<<milliseconds(prepared,restored)
                <<"\ncache_read_fused=1"
                <<"\npatch_jobs_ms="<<milliseconds(restored,meshed)<<"\nplan_save_ms="<<milliseconds(meshed,saved)
                <<"\nsnapshot_ms="<<milliseconds(saved,std::chrono::steady_clock::now())
                <<"\nworker_limit="<<request.maximum_in_flight
                <<"\npeak_active_workers="<<activity.peak.load(std::memory_order_relaxed)
                <<"\nworker_jobs_started="<<activity.started.load(std::memory_order_relaxed)
                <<"\nworker_run_ms="<<double(activity.nanoseconds.load(std::memory_order_relaxed))/1.0e6
                <<"\nbuilt="<<built<<"\nreused="<<reused<<"\nresident="<<stats.resident_patches
                <<"\npending="<<stats.pending_patches<<"\ndraw="<<stats.draw_patches<<'\n';
        }
        return result;
    }
    catch(...){return 8;}
}
