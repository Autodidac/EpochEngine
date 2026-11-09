// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <epoch.edition.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

export module project.planetary_runtime;
import asset.mesh_artifact;
import asset.cloud_volume;
export import water.wave;

export namespace epochengine::project_planetary_runtime
{
    inline constexpr std::uint32_t physical_edition = EPOCH_SOURCE_EDITION_PUBLIC;
    struct Binding final
    {
        std::string project_root{}, project_id{}, source_hash{};
        std::uint32_t edition{physical_edition}, seed{1337};
        std::uint32_t initial_body{};
        bool ocean_surface{};
        std::uint32_t size_reference_body{};
        double reference_radius_meters{6371000.0};
        bool original_sunlight{};
        std::string water_source_hash{};
        bool universe{};
        friend bool operator==(const Binding&, const Binding&) = default;
    };
    enum class Command : std::uint8_t
    {
        none, orbit, land, toggle_cruise, toggle_walk, toggle_time_pause,
        cycle_time_rate, toggle_gravity, toggle_level_assist
    };
    struct Input final
    {
        double elapsed_seconds{}, forward{}, right{}, rise{}, yaw{}, pitch{};
        bool boost{};
        std::optional<std::uint32_t> selected_body{};
        std::optional<double> time_scale{}, speed_multiplier{};
        Command command{Command::none};
        double roll{};
    };
    enum class TravelMode : std::uint8_t { flight, walk, cruise };
    struct BodyFrame final
    {
        std::uint32_t id{};
        std::int32_t parent{-1};
        std::string name{};
        std::array<double, 3> center{};
        double radius{};
        bool landable{};
        // A radius-ten body-local artifact, built once by the optional provider.
        std::shared_ptr<const asset::mesh::Artifact> coarse_mesh{};
        // Optional provider-owned, radius-ten atmospheric geometry. It is
        // independent of the terrain cut and grants no occlusion authority.
        std::shared_ptr<const asset::mesh::Artifact> cloud_mesh{};
        double cloud_wind_degrees{};
        std::optional<asset::cloud_volume::Recipe> cloud_density{};
    };
    struct Sunlight final
    {
        // Provider world direction toward its sun, before authoring rotation.
        std::array<double,3> direction_to_sun{};
        std::array<float,3> color{1.0f,1.0f,1.0f};
        float intensity{1.12f};
    };
    [[nodiscard]] inline bool valid_sunlight(const Sunlight& value) noexcept
    {
        double squared{};
        for(const auto channel:value.direction_to_sun){if(!std::isfinite(channel))return false;squared+=channel*channel;}
        for(const auto channel:value.color)if(!std::isfinite(channel)||channel<0.0f||channel>64.0f)return false;
        return std::abs(squared-1.0)<=1e-6&&std::isfinite(value.intensity)&&value.intensity>=0.0f&&value.intensity<=64.0f;
    }
    struct Frame final
    {
        bool valid{};
        std::uint32_t seed{}, anchor{}, target{}, body_count{};
        std::array<double, 3> camera{}, forward{}, up{};
        std::array<BodyFrame, 16> bodies{};
        double seconds{}, time_scale{}, speed_factor{}, altitude{}, speed{};
        bool gravity_enabled{}, level_assist{};
        TravelMode mode{TravelMode::flight};
        std::optional<Sunlight> sunlight{};
    };
    enum class Code : std::uint8_t { ready, cancelled, invalid_input, invalid_frame, unavailable, provider_failure };
    struct Result final { Code code{Code::unavailable}; Frame frame{}; };

    class Controller
    {
    public:
        virtual ~Controller() = default;
        [[nodiscard]] virtual Result advance(const Input&, std::stop_token = {}) = 0;
        [[nodiscard]] virtual Result capture() const = 0;
        [[nodiscard]] virtual std::string encode_session() const = 0;
        [[nodiscard]] virtual bool restore_session(std::string_view) = 0;
        [[nodiscard]] virtual std::shared_ptr<water::wave::Provider> water_provider() const {return {};}
    };
    using Factory = std::unique_ptr<Controller> (*)(const Binding&);

    [[nodiscard]] inline bool valid_binding(const Binding& value) noexcept
    {
        const auto safe = [](std::string_view text) {
            if (text.empty() || text.size() > 4096u) return false;
            for (const auto c : text) if (static_cast<unsigned char>(c) < 32u) return false;
            return true;
        };
        if (!safe(value.project_root) || !safe(value.project_id) || value.project_id.size() > 128u
            || value.edition != physical_edition || value.initial_body > 5u || !value.source_hash.starts_with("sha256:")
            || value.source_hash.size() != 71u || value.size_reference_body > 5u
            || !std::isfinite(value.reference_radius_meters) || value.reference_radius_meters < 250.0
            || value.reference_radius_meters > 100000000.0) return false;
        for (const auto c : std::string_view{value.source_hash}.substr(7))
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        if(value.ocean_surface)
        {
            if(value.water_source_hash.size()!=71u||!value.water_source_hash.starts_with("sha256:"))return false;
            for(const auto c:std::string_view{value.water_source_hash}.substr(7))
                if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
        }
        else if(!value.water_source_hash.empty())return false;
        return true;
    }

    [[nodiscard]] inline bool valid_frame(const Frame& value) noexcept
    {
        const auto finite = [](const auto& vector) {
            for (const auto component : vector) if (!std::isfinite(component)) return false;
            return true;
        };
        const auto dot = [](const auto& a, const auto& b) {
            return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
        };
        if (!value.valid || !value.body_count || value.body_count > value.bodies.size()
            ||(value.sunlight&&!valid_sunlight(*value.sunlight))
            || !finite(value.camera) || !finite(value.forward) || !finite(value.up)
            || std::abs(dot(value.forward,value.forward)-1.0) > 1e-6
            || std::abs(dot(value.up,value.up)-1.0) > 1e-6
            || std::abs(dot(value.forward,value.up)) > 1e-6
            || !std::isfinite(value.seconds) || value.seconds < 0.0 || value.seconds >= 1.0e12
            || !std::isfinite(value.time_scale) || value.time_scale < 0.0 || value.time_scale > 10000.0
            || !std::isfinite(value.speed_factor) || value.speed_factor < 0.01 || value.speed_factor > 100.0
            || !std::isfinite(value.altitude) || !std::isfinite(value.speed) || value.speed < 0.0
            || static_cast<unsigned>(value.mode) > static_cast<unsigned>(TravelMode::cruise)) return false;
        bool anchor{}, target{};
        for (std::uint32_t i=0; i<value.body_count; ++i)
        {
            const auto& body=value.bodies[i];
            if (!finite(body.center) || !std::isfinite(body.radius) || body.radius <= 0.0
                || !body.coarse_mesh || body.coarse_mesh->vertices().empty() || body.coarse_mesh->indices().empty()
                || !std::isfinite(body.cloud_wind_degrees)||body.cloud_wind_degrees<0.0||body.cloud_wind_degrees>=360.0
                || (body.cloud_density&&(!asset::cloud_volume::valid(*body.cloud_density,body.radius)||body.cloud_mesh))
                || (body.cloud_mesh&&(body.cloud_mesh->vertices().empty()||body.cloud_mesh->indices().empty()
                    ||body.cloud_mesh->vertices().size()>24'000u||body.cloud_mesh->indices().size()>140'000u
                    ||48u+body.cloud_mesh->vertices().size()*44u+body.cloud_mesh->indices().size()*4u>2ull*1024u*1024u))
                || body.parent < -1 || body.parent >= static_cast<std::int32_t>(value.body_count)
                || body.parent == static_cast<std::int32_t>(i)) return false;
            for (std::uint32_t j=0;j<i;++j) if(value.bodies[j].id==body.id)return false;
            anchor=anchor||body.id==value.anchor; target=target||body.id==value.target;
        }
        return anchor && target;
    }

    struct PreviewBody final
    {
        std::uint32_t id{};
        std::array<double,3> center{};
        double radius{};
        std::shared_ptr<const asset::mesh::Artifact> mesh{};
        std::shared_ptr<const asset::mesh::Artifact> cloud_mesh{};
        double cloud_wind_degrees{};
        std::optional<asset::cloud_volume::Recipe> cloud_density{};
    };
    struct PreviewFrame final
    {
        std::array<double,3> camera{}, forward{}, up{};
        std::array<PreviewBody,16> bodies{};
        std::uint32_t body_count{}, anchor{};
        double meters_to_preview{}, far_distance{}, eye_clearance{};
    };
    // Rebase in double before the render host applies its authored transform
    // and converts to floats. A normalized coarse mesh already has radius ten.
    [[nodiscard]] inline std::optional<PreviewFrame> preview_frame(const Frame& frame) noexcept
    {
        if(!valid_frame(frame))return {};
        const BodyFrame* anchor{};
        for(std::uint32_t i=0;i<frame.body_count;++i)
            if(frame.bodies[i].id==frame.anchor)anchor=&frame.bodies[i];
        if(!anchor)return {};
        PreviewFrame result;
        result.body_count=frame.body_count;result.anchor=frame.anchor;
        result.meters_to_preview=10.0/anchor->radius;
        result.eye_clearance=frame.altitude*result.meters_to_preview;
        result.forward=frame.forward;result.up=frame.up;
        for(std::size_t axis=0;axis<3;++axis)
        {
            result.camera[axis]=(frame.camera[axis]-anchor->center[axis])*result.meters_to_preview;
            if(!std::isfinite(result.camera[axis]))return {};
        }
        result.far_distance=64.0;
        for(std::uint32_t i=0;i<frame.body_count;++i)
        {
            auto& body=result.bodies[i];const auto& original=frame.bodies[i];
            body.id=original.id;body.mesh=original.coarse_mesh;
            body.cloud_mesh=original.cloud_mesh;body.cloud_wind_degrees=original.cloud_wind_degrees;
            body.cloud_density=original.cloud_density;
            body.radius=original.radius*result.meters_to_preview;
            for(std::size_t axis=0;axis<3;++axis)
                body.center[axis]=(original.center[axis]-anchor->center[axis])*result.meters_to_preview;
            const auto distance=std::hypot(body.center[0]-result.camera[0],body.center[1]-result.camera[1],body.center[2]-result.camera[2])+body.radius;
            if(!std::isfinite(distance)||!std::isfinite(body.radius))return {};
            if(distance>result.far_distance)result.far_distance=distance;
        }
        return result;
    }

    namespace detail
    {
        inline std::mutex factory_mutex{};
        inline std::optional<Binding> admitted_binding{};
        inline Factory factory{};
    }
    // Trusted, linked-extension routing only, not an OS sandbox or source
    // admission policy. The existing host parser/package gate supplies this
    // exact canonical binding after normal generated-project startup.
    [[nodiscard]] inline bool bind_admitted_project(const Binding& binding)
    {
        if (!valid_binding(binding)) return false;
        std::lock_guard lock(detail::factory_mutex);
        if (detail::admitted_binding) return *detail::admitted_binding == binding;
        detail::admitted_binding=binding;
        return true;
    }
    [[nodiscard]] inline bool register_factory(const Binding& binding, Factory factory)
    {
        if (!factory || !valid_binding(binding)) return false;
        std::lock_guard lock(detail::factory_mutex);
        if (!detail::admitted_binding || *detail::admitted_binding != binding) return false;
        if (detail::factory) return detail::factory == factory;
        detail::factory=factory;
        return true;
    }
    [[nodiscard]] inline std::optional<Binding> admitted_project_binding()
    {
        std::lock_guard lock(detail::factory_mutex);
        return detail::admitted_binding;
    }
    [[nodiscard]] inline std::unique_ptr<Controller> create_controller(const Binding& binding)
    {
        Factory factory{};
        {
            std::lock_guard lock(detail::factory_mutex);
            if (!detail::admitted_binding || *detail::admitted_binding != binding) return {};
            factory=detail::factory;
        }
        if (!factory) return {};
        try { return factory(binding); } catch (...) { return {}; }
    }
}
