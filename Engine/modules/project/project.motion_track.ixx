// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
export module project.motion_track;

export namespace epochengine::project_motion_track
{
    inline constexpr std::uint32_t maximum_keys=256u;
    struct Binding final
    {
        std::uint64_t project_generation{},scene_id{},entity_id{};
        std::string project_id{};
        friend bool operator==(const Binding&,const Binding&)=default;
    };
    struct Keyframe final
    {
        double seconds{};
        std::array<double,3> offset{};
    };
    enum class Code : std::uint8_t
    {
        ready,invalid_binding,invalid_keys,out_of_order,empty,stale_binding,invalid_sample
    };
    struct Storage;
    struct BuildResult;
    struct Sample;
    class Track final
    {
    public:
        Track()=default;
        [[nodiscard]] explicit operator bool() const noexcept {return bool(storage_);}
    private:
        std::shared_ptr<const Storage> storage_{};
        friend BuildResult make_track(Binding,std::span<const Keyframe>);
        friend Sample sample_translation(const Track&,const Binding&,double) noexcept;
    };
    struct BuildResult final
    {
        Code code{Code::invalid_keys};
        Track track{};
        [[nodiscard]] explicit operator bool() const noexcept {return code==Code::ready&&bool(track);}
    };
    struct Sample final
    {
        Code code{Code::empty};
        std::array<double,3> offset{};
        [[nodiscard]] explicit operator bool() const noexcept {return code==Code::ready;}
    };
    // Own all submitted data before returning. No script code/function pointer is
    // retained. Times are finite, strictly increasing and bounded to one day.
    [[nodiscard]] BuildResult make_track(Binding,std::span<const Keyframe>);
    // A pure observation of the central sample time. Ends hold, without looping.
    // Add the result to the CURRENT authored position; never write it to authoring.
    [[nodiscard]] Sample sample_translation(const Track&,const Binding& current,double seconds) noexcept;
}
