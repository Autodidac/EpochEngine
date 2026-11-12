// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

export module project.planetary;
import asset.mesh_artifact;
export import project.planetary_snapshot;

export namespace epochengine::project_planetary
{
    // Project-side authoring data only. Terrain algorithms remain in the
    // optional Extensions provider; ordinary Engine startup does not link it.
    struct Settings final
    {
        std::string mode{"planet"};
        std::uint32_t seed{1337}, cells_per_tile{16}, tiles_per_axis{4}, body_index{};
        double tile_span_meters{128.0};
        bool stream_lod{true};
        bool ocean_surface{};
        std::uint32_t maximum_level{10}, maximum_leaves{48}, resident_megabytes{16};
        std::uint32_t snapshot_megabytes{4};
        double patch_pixels{210.0}, lod_hysteresis{0.15};
        std::uint32_t size_reference_body{};
        double reference_radius_meters{6371000.0};
        bool original_sunlight{};
        [[nodiscard]] friend bool operator==(const Settings&, const Settings&) = default;
    };
    struct GeneratedMeshFrame final
    {
        double meters_per_unit{1.0};
        std::array<double,3> source_origin_meters{};
        [[nodiscard]] friend bool operator==(const GeneratedMeshFrame&, const GeneratedMeshFrame&) = default;
    };
    struct MeshGenerationIdentity final
    {
        std::array<std::uint64_t,4> artifact_key{};
        std::uint32_t vertex_count{}, index_count{};
        GeneratedMeshFrame coordinates{};
        [[nodiscard]] friend bool operator==(const MeshGenerationIdentity&, const MeshGenerationIdentity&) = default;
    };
    struct GeneratedBody final
    {
        std::uint32_t id{};
        std::int32_t parent{-1};
        std::string name{};
        std::array<double,3> center_meters{};
        double radius_meters{};
        std::uint32_t first_vertex{}, vertex_count{}, first_index{}, index_count{};
        std::array<float,3> minimum{}, maximum{};
        [[nodiscard]] friend bool operator==(const GeneratedBody&, const GeneratedBody&) = default;
    };
    struct Result final
    {
        bool ok{}, cancelled{}, worker_busy{};
        std::string message{};
        std::filesystem::path artifact_path{}, recipe_path{}, build_log{};
        double build_seconds{}, generation_seconds{};
        std::optional<GeneratedMeshFrame> mesh_frame{};
        std::vector<GeneratedBody> bodies{};
    };
    struct StreamView final
    {
        std::array<double, 3> camera{}; // Body-local preview coordinates; radius ten.
        std::uint32_t viewport_height{720};
        double vertical_fov{1.0471975511965976};
        std::uint64_t request_id{};
        std::optional<double> simulation_seconds{}; // Unset retains the original zero-time transport.
        // Optional body-local viewing direction. Legacy consumers retain their
        // omnidirectional planner; supported providers prioritize this cone.
        std::optional<std::array<double,3>> forward{};
        double aspect_ratio{16.0/9.0};
        std::optional<std::array<double,3>> up{}; // Camera roll, for a lower viewport focus.
    };
    [[nodiscard]] inline bool valid_stream_up(const StreamView& view) noexcept
    {
        if(!view.up)return true;
        if(!view.forward)return false;
        const auto& u=*view.up;const auto& f=*view.forward;
        const double ul=std::hypot(u[0],u[1],u[2]),fl=std::hypot(f[0],f[1],f[2]);
        if(!std::isfinite(ul)||!std::isfinite(fl)||ul<1e-12||fl<1e-12)return false;
        const double dot=(u[0]/ul)*(f[0]/fl)+(u[1]/ul)*(f[1]/fl)+(u[2]/ul)*(f[2]/fl);
        return std::isfinite(dot)&&std::abs(dot)<1.0-1e-10;
    }
    // A completed provider cut covers all six faces, including fallback parents.
    // Camera movement changes desired detail, not that coverage. Accepting
    // such a cut avoids starving publication during flight; the current view is
    // still queued next. This policy is not valid for a frustum-culled snapshot.
    [[nodiscard]] inline bool stream_view_reusable(const StreamView& current,
        const StreamView& requested) noexcept
    {
        const auto valid=[](const StreamView& view) {
            if(!valid_stream_up(view))return false;
            if(view.viewport_height<64u||view.viewport_height>16384u
                ||!std::isfinite(view.vertical_fov)||view.vertical_fov<0.1||view.vertical_fov>3.0
                ||(view.simulation_seconds&&(!std::isfinite(*view.simulation_seconds)
                    ||*view.simulation_seconds<0.0||*view.simulation_seconds>=1.0e12)))return false;
            if(view.forward)
            {
                const auto& f=*view.forward;
                const auto magnitude=std::hypot(f[0],f[1],f[2]);
                if(!std::isfinite(magnitude)||magnitude<1e-12||!std::isfinite(view.aspect_ratio)
                    ||view.aspect_ratio<0.1||view.aspect_ratio>10.0)return false;
            }
            for(const auto value:view.camera)
                if(!std::isfinite(value)||std::abs(value)>1.0e10)return false;
            return true;
        };
        if(!valid(current)||!valid(requested)||current.viewport_height!=requested.viewport_height
            ||std::abs(current.vertical_fov-requested.vertical_fov)>0.001)return false;
        return true;
    }
    // Immutable request evidence, not current selection state. Two projects can
    // legitimately attach identical provider bytes; that does not transfer a
    // running operation or its completion into the other project.
    struct SourceGeneration final
    {
        std::filesystem::path project_root{};
        std::string project_manifest{}, content_hash{};
        [[nodiscard]] friend bool operator==(const SourceGeneration&, const SourceGeneration&) = default;
    };
    struct StreamMeshBinding final
    {
        SourceGeneration source{};
        MeshGenerationIdentity mesh{};
    };
    struct StreamResult final
    {
        Result operation{};
        bool waiting_for_worker{};
        bool snapshot_ready{};
        std::uint64_t request_id{}, resident_bytes{}, maximum_bytes{};
        std::uint32_t built{}, reused{}, resident{}, pending{}, draw{}, minimum_level{}, maximum_level{};
        std::shared_ptr<const asset::mesh::Artifact> snapshot{};
        // Present only after versioned artifact/source/recipe/request admission.
        // snapshot remains a safe cut-only mesh for existing consumers.
        std::shared_ptr<const project_planetary_snapshot::Snapshot> patch_snapshot{};
        // Set only by the worker after its final full source/recipe check.
        // Consumers bind this receipt to their admitted project generation;
        // reading it does not rewalk or rehash package files on the frame owner.
        std::optional<SourceGeneration> verified_source{};
    };
    // Immutable admitted mesh evidence plus a body-local procedural direction.
    // This query does not establish contact with a displayed coarse triangle.
    struct SurfaceQuery final
    {
        SourceGeneration source{};
        Settings settings{};
        MeshGenerationIdentity mesh{};
        GeneratedBody body{};
        std::shared_ptr<const asset::mesh::Artifact> artifact{};
        std::array<double,3> direction{};
        std::uint64_t request_id{};
    };
    enum class SurfaceCode : std::uint8_t
    { ready, unsupported, stale, cancelled, busy, invalid, provider_failure };
    struct SurfaceResult final
    {
        SurfaceCode code{SurfaceCode::invalid};
        Result operation{};
        std::uint64_t request_id{};
        std::array<double,3> point_meters{}, radial_up{}, normal{};
        double height_meters{};
    };
    inline constexpr std::size_t kMaximumSurfaceBatchSamples = 64u;
    struct SurfaceBatchQuery final
    {
        SurfaceQuery query{};
        std::vector<std::array<double,3>> directions{};
    };
    // Ordered, all-or-none procedural evidence. One provider admission/process
    // serves the entire bounded batch; no scene or mesh is changed here.
    struct SurfaceBatchResult final
    {
        SurfaceCode code{SurfaceCode::invalid};
        Result operation{};
        std::uint64_t request_id{};
        std::vector<SurfaceResult> samples{};
    };
    struct ShorelineResult final
    {
        SurfaceCode code{SurfaceCode::invalid};
        Result operation{};
        SurfaceResult land{}, water{};
    };
    struct LandingSiteResult final
    {
        SurfaceCode code{SurfaceCode::invalid};
        Result operation{};
        SurfaceResult beacon{}, player_start{};
    };
    inline constexpr std::string_view recipe_relative_path = "Assets/Worlds/Planet.epoch_planet";
    inline constexpr std::array<std::string_view, 5> modes{
        "classic", "tiled", "hemisphere", "planet", "universe"};

    [[nodiscard]] inline bool supports_exploration(const Settings& value) noexcept
    { return value.mode=="planet"||value.mode=="universe"; }

    [[nodiscard]] inline bool supports_ocean_surface(const Settings& value) noexcept
    {
        // The reviewed six-body provider defines a sea-level ocean only on
        // body zero. Universe coverage includes that body explicitly.
        return value.body_index <= 5u && (value.mode == "universe"
            || (value.body_index == 0u && (value.mode == "planet" || value.mode == "hemisphere")));
    }

    [[nodiscard]] inline asset::mesh::Limits artifact_limits(const Settings& value) noexcept
    {
        if(value.snapshot_megabytes<1u||value.snapshot_megabytes>64u)return {0u,0u,0u};
        const std::uint64_t bytes=std::uint64_t{value.snapshot_megabytes}*1024u*1024u;
        return {static_cast<std::uint32_t>((std::min)(std::uint64_t{1'048'576u},(bytes-48u)/44u)),
            static_cast<std::uint32_t>((std::min)(std::uint64_t{6'291'456u},(bytes-48u)/4u)),bytes};
    }

    [[nodiscard]] inline bool valid_settings(const Settings& value) noexcept
    {
        bool known{};
        for(const auto mode:modes)known=known||mode==value.mode;
        if(!known||value.cells_per_tile<2u||value.cells_per_tile>64u
            ||value.tiles_per_axis<1u||value.tiles_per_axis>16u
            ||value.body_index>5u||!std::isfinite(value.tile_span_meters)
            ||value.tile_span_meters<1.0||value.tile_span_meters>16384.0
            ||value.maximum_level>20u||value.maximum_leaves<6u||value.maximum_leaves>2048u
            ||!std::isfinite(value.patch_pixels)||value.patch_pixels<32.0||value.patch_pixels>4096.0
            ||!std::isfinite(value.lod_hysteresis)||value.lod_hysteresis<0.0||value.lod_hysteresis>0.45
            ||value.resident_megabytes<4u||value.resident_megabytes>256u
            ||value.snapshot_megabytes<1u||value.snapshot_megabytes>64u)return false;
        if(value.size_reference_body>5u||!std::isfinite(value.reference_radius_meters)
            ||value.reference_radius_meters<250.0||value.reference_radius_meters>100000000.0)return false;
        if(value.ocean_surface&&!supports_ocean_surface(value))return false;
        if(value.original_sunlight&&!supports_exploration(value))return false;
        const auto limits=artifact_limits(value);
        const std::uint64_t cells=value.cells_per_tile,side=cells+1u;
        const std::uint64_t layers=value.ocean_surface?2u:1u;
        const auto admitted=[&](std::uint64_t patches,bool skirts){
            const auto vertices=layers*patches*(side*side+(skirts?4u*cells:0u));
            const auto indices=layers*patches*(6u*cells*cells+(skirts?24u*cells:0u));
            return vertices<=limits.maximum_vertices&&indices<=limits.maximum_indices
                &&48u+vertices*44u+indices*4u<=limits.maximum_bytes;
        };
        if(value.stream_lod&&value.mode=="planet"&&!admitted(value.maximum_leaves,true))return false;
        const std::uint64_t patches=value.mode=="tiled"?std::uint64_t{value.tiles_per_axis}*value.tiles_per_axis
            :value.mode=="universe"?36u:value.mode=="hemisphere"?12u:value.mode=="planet"?6u:1u;
        return patches<=128u&&admitted(patches,false);
    }

    [[nodiscard]] inline std::optional<Settings> active_body_stream_settings(Settings value,std::uint32_t body)
    {
        if(!supports_exploration(value)||!valid_settings(value)||body>5u)return {};
        value.mode="planet";value.body_index=body;
        value.ocean_surface=value.ocean_surface&&body==0u;
        return valid_settings(value)?std::optional<Settings>{value}:std::nullopt;
    }

    [[nodiscard]] inline bool requires_lod_controls(const Settings& value) noexcept
    { return value.patch_pixels != 210.0 || value.lod_hysteresis != 0.15; }

    [[nodiscard]] inline bool requires_size_controls(const Settings& value) noexcept
    { return value.size_reference_body != 0u || value.reference_radius_meters != 6371000.0; }

    enum class DetailPreset : std::uint8_t { compact, detailed, fast, fine };
    [[nodiscard]] inline std::optional<Settings> with_detail_preset(Settings value,DetailPreset preset)
    {
        if(!supports_exploration(value))return {};
        value.cells_per_tile=16u;value.stream_lod=true;
        if(preset==DetailPreset::compact)
        {value.maximum_level=10u;value.maximum_leaves=48u;value.resident_megabytes=16u;value.snapshot_megabytes=4u;
            value.patch_pixels=210.0;value.lod_hysteresis=0.15;}
        else if(preset==DetailPreset::detailed)
        {value.maximum_level=20u;value.maximum_leaves=384u;value.resident_megabytes=128u;value.snapshot_megabytes=32u;
            value.patch_pixels=210.0;value.lod_hysteresis=0.15;}
        else if(preset==DetailPreset::fast)
        {value.maximum_level=17u;value.maximum_leaves=288u;value.resident_megabytes=128u;value.snapshot_megabytes=32u;
            value.patch_pixels=260.0;value.lod_hysteresis=0.18;}
        else if(preset==DetailPreset::fine)
        {value.maximum_level=20u;value.maximum_leaves=576u;value.resident_megabytes=192u;value.snapshot_megabytes=32u;
            value.patch_pixels=170.0;value.lod_hysteresis=0.15;}
        else return {};
        return valid_settings(value)?std::optional<Settings>{value}:std::nullopt;
    }

    struct PreviewMemoryFootprint final
    {
        std::uint64_t cache_bytes{},snapshot_budget_bytes{},encoded_upper_bytes{};
        std::uint64_t decode_transient_upper_bytes{},replacement_upper_bytes{},cpu_upper_bytes{};
        std::uint64_t transition_retained_upper_bytes{},transition_transient_upper_bytes{};
    };
    // Conservative CPU policy estimates, not reservations or measured usage.
    // Replacement retains the previous aggregate while the file/decoded/copy/
    // content-key buffers can coexist. Pass retained prior bytes when shrinking
    // the budget; omission conservatively assumes a previous snapshot of the
    // current budget. GPU copies and renderer costs are separate.
    [[nodiscard]] inline PreviewMemoryFootprint preview_memory_footprint(
        const Settings& value,std::uint64_t prior_snapshot_bytes=0u) noexcept
    {
        if(!valid_settings(value)||prior_snapshot_bytes>64ull*1024u*1024u)return {};
        const std::uint64_t cells=value.cells_per_tile,side=cells+1u,layers=value.ocean_surface?2u:1u;
        const bool streamed=value.stream_lod&&value.mode=="planet";
        const std::uint64_t patches=streamed?value.maximum_leaves
            :value.mode=="tiled"?std::uint64_t{value.tiles_per_axis}*value.tiles_per_axis
            :value.mode=="universe"?36u:value.mode=="hemisphere"?12u:value.mode=="planet"?6u:1u;
        const auto vertices=layers*patches*(side*side+(streamed?4u*cells:0u));
        const auto indices=layers*patches*(6u*cells*cells+(streamed?24u*cells:0u));
        PreviewMemoryFootprint result;
        result.cache_bytes=std::uint64_t{value.resident_megabytes}*1024u*1024u;
        result.snapshot_budget_bytes=artifact_limits(value).maximum_bytes;
        result.encoded_upper_bytes=48u+vertices*44u+indices*4u;
        result.decode_transient_upper_bytes=4u*result.snapshot_budget_bytes;
        result.replacement_upper_bytes=result.decode_transient_upper_bytes
            +(std::max)(result.snapshot_budget_bytes,prior_snapshot_bytes);
        if(streamed)
        {
            // A transition retains up to two immutable hierarchies (including
            // parent deltas), plus the old/replacement cuts. These are CPU
            // snapshots, not the renderer's independently bounded scene cache.
            const project_planetary_snapshot::Limits transition;
            const auto metadata=transition.maximum_metadata_bytes
                +std::uint64_t{transition.maximum_patches}*sizeof(project_planetary_snapshot::Patch);
            result.transition_retained_upper_bytes=4u*result.snapshot_budget_bytes+2u*metadata+1536u;
            result.transition_transient_upper_bytes=2u*result.snapshot_budget_bytes
                +5u*transition.maximum_patches*sizeof(project_planetary_snapshot::DrawPatch)+512u;
            result.replacement_upper_bytes=(std::max)(result.replacement_upper_bytes,
                result.transition_retained_upper_bytes+result.transition_transient_upper_bytes
                +(std::max)(result.snapshot_budget_bytes,prior_snapshot_bytes)-result.snapshot_budget_bytes);
        }
        result.cpu_upper_bytes=result.cache_bytes+result.replacement_upper_bytes;
        return result;
    }

    struct SceneMeshBudget final
    {
        asset::mesh::Limits artifact{0u,0u,0u};
        std::uint64_t cache_bytes{16ull*1024u*1024u};
        std::uint64_t frame_indices{196'608u},frame_bytes{16ull*1024u*1024u};
    };
    // This is an additional admitted package envelope, not a new default for
    // ordinary imported meshes. Keep the ordinary subbudget independently.
    [[nodiscard]] inline SceneMeshBudget scene_mesh_budget(const Settings& value) noexcept
    {
        if(!valid_settings(value))return {};
        SceneMeshBudget result;
        result.artifact=artifact_limits(value);
        constexpr std::uint64_t mib=1024u*1024u;
        result.cache_bytes=(std::min)(std::uint64_t{64u}*mib,
            std::uint64_t{16u}*mib+result.artifact.maximum_bytes);
        result.frame_bytes=result.cache_bytes;
        result.frame_indices=(std::min)(std::uint64_t{6'291'456u},
            std::uint64_t{196'608u}+result.artifact.maximum_indices);
        return result;
    }

    [[nodiscard]] inline project_planetary_snapshot::Limits snapshot_transition_limits(const Settings& value) noexcept
    {
        const auto scene=scene_mesh_budget(value);
        const auto memory=preview_memory_footprint(value);
        return {scene.artifact,4096u,16ull*1024u*1024u,
            memory.transition_retained_upper_bytes,memory.transition_transient_upper_bytes};
    }

    [[nodiscard]] inline std::string encode_settings(const Settings& value)
    {
        if (!valid_settings(value)) return {};
        std::array<char, 64> number{};
        const auto real = std::to_chars(number.data(), number.data() + number.size(),
            value.tile_span_meters, std::chars_format::general, std::numeric_limits<double>::max_digits10);
        if (real.ec != std::errc{}) return {};
        const bool version7=value.original_sunlight;
        const bool version6=version7||requires_size_controls(value);
        const bool version5=version6||requires_lod_controls(value)||value.maximum_level>18u||value.maximum_leaves>512u;
        const bool version4=version5||value.snapshot_megabytes!=4u||value.maximum_leaves<36u
            ||value.maximum_leaves>64u||value.resident_megabytes>64u;
        auto result = std::string{version7 ? "epoch.planet.recipe.v7\nmode=" : version6 ? "epoch.planet.recipe.v6\nmode=" : version5 ? "epoch.planet.recipe.v5\nmode=" : version4 ? "epoch.planet.recipe.v4\nmode="
            :value.ocean_surface ? "epoch.planet.recipe.v3\nmode=" : "epoch.planet.recipe.v2\nmode="}
            + value.mode + "\nseed=" + std::to_string(value.seed)
            + "\ncells_per_tile=" + std::to_string(value.cells_per_tile)
            + "\ntiles_per_axis=" + std::to_string(value.tiles_per_axis)
            + "\nbody_index=" + std::to_string(value.body_index)
            + "\ntile_span_meters=" + std::string(number.data(), real.ptr)
            + "\nstream_lod=" + std::to_string(value.stream_lod ? 1u : 0u)
            + "\nmaximum_level=" + std::to_string(value.maximum_level)
            + "\nmaximum_leaves=" + std::to_string(value.maximum_leaves)
            + "\nresident_megabytes=" + std::to_string(value.resident_megabytes) + "\n";
        if(version4)result+="snapshot_megabytes="+std::to_string(value.snapshot_megabytes)+"\n";
        if(version4||value.ocean_surface)result+="ocean_surface="+std::to_string(value.ocean_surface?1u:0u)+"\n";
        if(version5)
        {
            const auto append_real=[&](std::string_view name,double value) {
                const auto written=std::to_chars(number.data(),number.data()+number.size(),value,
                    std::chars_format::general,std::numeric_limits<double>::max_digits10);
                if(written.ec!=std::errc{})return false;
                result+=name;result.append(number.data(),written.ptr);result+='\n';return true;
            };
            if(!append_real("patch_pixels=",value.patch_pixels)
                ||!append_real("lod_hysteresis=",value.lod_hysteresis))return {};
            if(version6)
            {
                result+="size_reference_body="+std::to_string(value.size_reference_body)+"\n";
                if(!append_real("reference_radius_meters=",value.reference_radius_meters))return {};
            }
        }
        if(version7)result+="original_sunlight=1\n";
        return result;
    }

    [[nodiscard]] inline std::optional<Settings> decode_settings(std::string_view text)
    {
        const bool version7 = text.starts_with("epoch.planet.recipe.v7\n");
        const bool version6 = version7 || text.starts_with("epoch.planet.recipe.v6\n");
        const bool version5 = version6 || text.starts_with("epoch.planet.recipe.v5\n");
        const bool version4 = version5 || text.starts_with("epoch.planet.recipe.v4\n");
        const bool version3 = version4 || text.starts_with("epoch.planet.recipe.v3\n");
        const bool version2 = version3 || text.starts_with("epoch.planet.recipe.v2\n");
        if (text.size() > 1024u || (!version2 && !text.starts_with("epoch.planet.recipe.v1\n"))) return {};
        text.remove_prefix(std::string_view{"epoch.planet.recipe.v1\n"}.size());
        const auto field = [&text](std::string_view name) -> std::optional<std::string_view> {
            if (!text.starts_with(name)) return {};
            text.remove_prefix(name.size());
            const auto end = text.find('\n');
            if (end == std::string_view::npos || end == 0u) return {};
            const auto result = text.substr(0, end);
            text.remove_prefix(end + 1u);
            return result;
        };
        Settings result;
        if (!version2) result.stream_lod = false; // Existing reviewed providers predate streaming.
        const auto mode = field("mode=");
        if (!mode) return {};
        result.mode = *mode;
        const auto integer = [&field](std::string_view name, std::uint32_t& output) {
            const auto value = field(name);
            if (!value) return false;
            const auto parsed = std::from_chars(value->data(), value->data() + value->size(), output);
            return parsed.ec == std::errc{} && parsed.ptr == value->data() + value->size();
        };
        if (!integer("seed=", result.seed) || !integer("cells_per_tile=", result.cells_per_tile)
            || !integer("tiles_per_axis=", result.tiles_per_axis) || !integer("body_index=", result.body_index)) return {};
        const auto span = field("tile_span_meters=");
        if (!span) return {};
        const auto parsed = std::from_chars(span->data(), span->data() + span->size(), result.tile_span_meters);
        if (parsed.ec != std::errc{} || parsed.ptr != span->data() + span->size()) return {};
        if (version2)
        {
            std::uint32_t enabled{};
            if (!integer("stream_lod=", enabled) || enabled > 1u
                || !integer("maximum_level=", result.maximum_level)
                || !integer("maximum_leaves=", result.maximum_leaves)
                || !integer("resident_megabytes=", result.resident_megabytes)) return {};
            result.stream_lod = enabled != 0u;
        }
        if(version4&&!integer("snapshot_megabytes=",result.snapshot_megabytes))return {};
        if (version3)
        {
            std::uint32_t ocean{};
            if (!integer("ocean_surface=", ocean) || ocean > 1u) return {};
            result.ocean_surface = ocean != 0u;
        }
        if(!version4&&(result.maximum_leaves<36u||result.maximum_leaves>64u||result.resident_megabytes>64u))return {};
        if(version5)
        {
            const auto real=[&](std::string_view name,double& output) {
                const auto value=field(name);if(!value)return false;
                const auto parsed=std::from_chars(value->data(),value->data()+value->size(),output);
                return parsed.ec==std::errc{}&&parsed.ptr==value->data()+value->size();
            };
            if(!real("patch_pixels=",result.patch_pixels)||!real("lod_hysteresis=",result.lod_hysteresis))return {};
            if(version6&&(!integer("size_reference_body=",result.size_reference_body)
                ||!real("reference_radius_meters=",result.reference_radius_meters)))return {};
        }
        else if(result.maximum_level>18u||result.maximum_leaves>512u)return {};
        if(version7){std::uint32_t sun{};if(!integer("original_sunlight=",sun)||sun!=1u)return {};result.original_sunlight=true;}
        if (!text.empty() || !valid_settings(result)) return {};
        return result;
    }

    [[nodiscard]] std::optional<Settings> load_settings(const std::filesystem::path& project_root);
    [[nodiscard]] bool save_settings(const std::filesystem::path& project_root, const Settings& settings);
    struct RuntimeRequirement final
    {
        bool ok{}, requires_project_runtime{};
        std::string message{};
    };
    // Admission only: this query never builds, activates or launches a provider.
    [[nodiscard]] RuntimeRequirement runtime_requirement(const std::filesystem::path& project_root);
    // Refresh only managed link/build fragments for explicit Run/Build.
    // No source generation, compiler, provider activation or recipe mutation.
    [[nodiscard]] Result prepare_runtime_dependency(const std::filesystem::path& project_root);
    struct SessionFile final
    {
        bool ok{}, present{};
        std::string bytes{}, message{};
    };
    // Opaque provider-validated state. Reading does not activate a provider;
    // saving requires the same admitted source and the last observed bytes.
    [[nodiscard]] SessionFile load_exploration_session(const std::filesystem::path& project_root);
    [[nodiscard]] SessionFile save_exploration_session(const std::filesystem::path& project_root,
        const SourceGeneration& admitted, std::string_view bytes, const SessionFile& previous);
    [[nodiscard]] std::optional<SourceGeneration> capture_source_generation(
        const std::filesystem::path& project_root, std::string_view admitted_content_hash);
    [[nodiscard]] bool source_generation_is_current(const std::filesystem::path& project_root,
        const SourceGeneration& admitted, std::stop_token cancellation = {});
    // Only successful host generation creates this immutable association. An
    // unrecorded legacy mesh remains viewable, but cannot borrow another recipe
    // for live LOD. Reading never generates, migrates or repairs project files.
    [[nodiscard]] bool mesh_generation_is_current(const std::filesystem::path& project_root,
        const SourceGeneration& source, const Settings& settings, const MeshGenerationIdentity& mesh);
    [[nodiscard]] std::optional<Settings> mesh_generation_settings(const std::filesystem::path& project_root,
        const SourceGeneration& source, const MeshGenerationIdentity& mesh);
    [[nodiscard]] std::vector<GeneratedBody> mesh_generation_bodies(const std::filesystem::path& project_root,
        const SourceGeneration& source, const MeshGenerationIdentity& mesh, const asset::mesh::Artifact& artifact);
    // Called by the existing cancellable Editor task after explicit Build & Generate.
    // Uses the admitted project dependency and existing child-process supervisor.
    [[nodiscard]] Result generate(const std::filesystem::path& project_root,
        const std::filesystem::path& provider_source_root,
        const std::filesystem::path& engine_source_root,
        Settings settings, std::stop_token cancellation = {});
    // Uses an already built, admitted provider. Runs on the existing caller's
    // task queue; no compilation or scene/authoring writes occur here.
    [[nodiscard]] SurfaceResult query_surface(const std::filesystem::path& project_root,
        const SurfaceQuery& query, std::stop_token cancellation = {});
    [[nodiscard]] SurfaceBatchResult query_surfaces(const std::filesystem::path& project_root,
        const SurfaceBatchQuery& query, std::stop_token cancellation = {});
    // Finds a dry point within the requested surface distance of verified
    // ocean water. Both samples come from the same admitted terrain recipe.
    [[nodiscard]] ShorelineResult query_ocean_shoreline(const std::filesystem::path& project_root,
        const SurfaceQuery& query, double maximum_distance_meters, std::stop_token cancellation = {});
    [[nodiscard]] LandingSiteResult query_forest_landing_site(const std::filesystem::path& project_root,
        const SurfaceQuery& query, double forest_radius_meters, double beacon_radius_meters,
        std::stop_token cancellation = {});
    [[nodiscard]] StreamResult stream_step(const std::filesystem::path& project_root,
        Settings settings, StreamView view, std::stop_token cancellation = {},
        const StreamMeshBinding* binding = nullptr);
}
