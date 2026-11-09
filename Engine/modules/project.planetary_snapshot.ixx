// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

export module project.planetary_snapshot;
import asset.mesh_artifact;
import asset.cloud_volume;
import water.wave;

export namespace epochengine::project_planetary_snapshot
{
    struct TileKey final
    {
        std::uint32_t x{}, y{};
        std::uint8_t body{}, face{}, level{};
        friend bool operator==(const TileKey&, const TileKey&) = default;
    };
    struct Identity final
    {
        asset::mesh::ContentHash artifact_key{};
        std::string source_hash{}, recipe_key{};
        std::uint64_t request_id{};
        friend bool operator==(const Identity&, const Identity&) = default;
    };
    struct Patch final
    {
        TileKey key{};
        // Provider-owned cube topology; no renderer/streamer implementation is
        // copied into the host. Neighbors are at this patch's level.
        std::array<TileKey,4> neighbors{};
        std::uint32_t first_vertex{}, vertex_count{}, first_index{}, index_count{};
        bool target{};
        // Optional v2 ocean recipe. Ground stays in the artifact; displaced
        // water and clipped shoreline vertices are disposable time samples.
        std::uint32_t terrain_vertices{}, terrain_indices{};
        std::uint32_t first_water_vertex{}, first_water_triangle{}, water_triangles{};
    };
    struct WaterVertex final
    {
        std::array<double,3> direction{};
        double skirt_depth_meters{};
        std::uint32_t surface_vertex{}; // Top-edge wetness for a lowered skirt.
    };
    struct WaterTriangle final
    {
        std::array<std::uint32_t,3> water{}, ground{};
    };
    // Optional package-produced normalized-radius-ten cloud layer. The host only
    // admits/transforms this geometry; generation remains in the extension.
    struct CloudLayer final
    {
        std::uint32_t body{};
        std::array<double,3> center_meters{};
        double radius_meters{}, wind_period_seconds{604800.0};
        std::shared_ptr<const asset::mesh::Artifact> artifact{};
        // v5 replaces opaque lobes with bounded body-local metric density.
        // Legacy v4 artifacts remain readable, but cannot accompany density.
        std::optional<asset::cloud_volume::Recipe> density{};
    };
    struct Metadata final
    {
        Identity identity{};
        std::optional<CloudLayer> cloud{};
        std::vector<Patch> patches{};
        // Artifact coordinate units; exactly one delta per artifact vertex.
        std::vector<std::array<float,3>> parent_delta{};
        double water_radius_meters{}, water_meters_to_artifact{};
        // Body-local metres subtracted before converting disposable water positions.
        std::array<double,3> water_origin_meters{};
        std::vector<WaterVertex> water_vertices{};
        std::vector<WaterTriangle> water_triangles{};
    };
    struct Snapshot final
    {
        std::shared_ptr<const asset::mesh::Artifact> artifact{};
        Metadata metadata{};
    };
    struct Limits final
    {
        asset::mesh::Limits artifact{};
        std::uint32_t maximum_patches{4096u};
        std::uint64_t maximum_metadata_bytes{16ull*1024u*1024u};
        std::uint64_t maximum_retained_bytes{20ull*1024u*1024u};
        std::uint64_t maximum_transient_bytes{16ull*1024u*1024u};
    };
    // Body-local metres subtracted before narrowing mesh vertices to floats.
    // v1 providers use the exact camera. Opt-in v2 bounds a distant camera's
    // origin while retaining metric units and the mesh codec's geometry limits.
    [[nodiscard]] std::optional<std::array<double,3>> metric_source_origin(
        std::array<double,3> camera_meters, bool bounded_origin = false) noexcept;
    [[nodiscard]] bool valid_key(TileKey) noexcept;
    [[nodiscard]] TileKey parent(TileKey) noexcept;
    [[nodiscard]] bool contains(TileKey,TileKey) noexcept;
    [[nodiscard]] bool valid_metadata(const Metadata&, const asset::mesh::Artifact&, Limits = {}) noexcept;
    [[nodiscard]] std::vector<std::byte> encode(const Metadata&, const asset::mesh::Artifact&, Limits = {}) noexcept;
    [[nodiscard]] std::optional<Metadata> decode(std::span<const std::byte>,
        const asset::mesh::Artifact&, const Identity& expected, Limits = {}, std::stop_token = {}) noexcept;
    // A caller-owned worker updates only the explicit ocean recipe at the
    // immutable FFT tile's actual time. It never remeshes terrain, starts a
    // process or changes the source snapshot. Old v1/dry snapshots return null.
    [[nodiscard]] std::shared_ptr<const asset::mesh::Artifact> resample_water(
        const Snapshot&, const water::wave::Tile&, Limits = {}, std::stop_token = {}) noexcept;

    struct DrawPatch final
    {
        std::shared_ptr<const Snapshot> snapshot{};
        std::uint32_t patch{};
        float morph{}; // zero=fine; one=exact immediate parent triangle.
    };
    enum class InitialCut : std::uint8_t { coarse_roots, selected_patches };
    // Value state owned by the existing scene packet lifecycle. It owns no
    // thread, clock, worker, renderer, or publication mechanism.
    class Transition final
    {
        std::shared_ptr<const Snapshot> target_{};
        std::vector<DrawPatch> active_{};
        Limits limits_{};
    public:
        explicit Transition(Limits limits = {}) : limits_(limits) {}
        // Animated scene owners start at coarse roots. A nonanimated consumer
        // can request the validated selected cut without displaying ancestors.
        [[nodiscard]] bool admit(std::shared_ptr<const Snapshot>, std::stop_token = {},
            InitialCut initial_cut = InitialCut::coarse_roots) noexcept;
        // Invalid/negative time and cancellation retain the previous complete
        // cut. Pause freezes morph phase; simulation time is not this clock.
        [[nodiscard]] bool advance(double elapsed_seconds, bool paused = false, std::stop_token = {}) noexcept;
        [[nodiscard]] std::span<const DrawPatch> draw() const noexcept { return active_; }
        [[nodiscard]] bool settled() const noexcept;
        [[nodiscard]] std::shared_ptr<const asset::mesh::Artifact> materialize(std::stop_token = {}) const noexcept;
        void clear() noexcept { active_.clear(); target_.reset(); }
    };
}
