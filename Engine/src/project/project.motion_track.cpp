// SPDX-License-Identifier: LicenseRef-MIT-NoSell
module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include "../../include/core.utf8.hpp"
module project.motion_track;
import temporal.request;

namespace epochengine::project_motion_track
{
    struct Storage final
    {
        Binding binding{};
        temporal::HistorySeries<std::array<double,3>> history{maximum_keys,temporal::TruthClass::ErrorBounded};
    };
    BuildResult make_track(Binding binding,std::span<const Keyframe> keys)
    {
        if(!binding.project_generation||!binding.scene_id||!binding.entity_id||binding.project_id.empty()
            ||binding.project_id.size()>4096u||binding.project_id.find('\0')!=std::string::npos
            ||!core::utf8::valid(binding.project_id))return {.code=Code::invalid_binding};
        if(keys.empty()||keys.size()>maximum_keys)return {.code=Code::invalid_keys};
        for(std::size_t i{};i<keys.size();++i)
        {
            const auto& key=keys[i];
            if(!std::isfinite(key.seconds)||key.seconds<0.0||key.seconds>86400.0
                ||!std::all_of(key.offset.begin(),key.offset.end(),[](double v)
                    {return std::isfinite(v)&&std::abs(v)<=static_cast<double>((std::numeric_limits<float>::max)());}))
                return {.code=Code::invalid_keys};
            if(i&&(!(keys[i-1u].seconds<key.seconds)
                ||temporal::same_sample_time({keys[i-1u].seconds},{key.seconds})))
                return {.code=Code::out_of_order};
        }
        auto storage=std::make_shared<Storage>();storage->binding=std::move(binding);
        for(std::size_t i{};i<keys.size();++i)
        {
            if(storage->history.append({.time={keys[i].seconds},.sequence=i+1u,.state=keys[i].offset})
                !=temporal::AppendStatus::Inserted)return {.code=Code::invalid_keys};
        }
        Track track;track.storage_=std::move(storage);
        return {.code=Code::ready,.track=std::move(track)};
    }
    Sample sample_translation(const Track& track,const Binding& current,double seconds) noexcept
    {
        if(!track.storage_)return {.code=Code::empty};
        if(track.storage_->binding!=current)return {.code=Code::stale_binding};
        if(!std::isfinite(seconds))return {.code=Code::invalid_sample};
        const auto observation=track.storage_->history.observe(temporal::make_observation_request(
            {0u,1u},{{{0.0},{0.0}},{1.0}},{seconds},temporal::ObservationMode::Bracket,
            {(std::numeric_limits<double>::infinity)()},true));
        if(!observation.ready()||!observation.lower||!observation.upper)return {.code=Code::invalid_sample};
        Sample result{.code=Code::ready};
        for(std::size_t i{};i<result.offset.size();++i)
        {
            result.offset[i]=std::lerp(observation.lower->state[i],observation.upper->state[i],observation.plan.interpolation_alpha);
            if(!std::isfinite(result.offset[i]))return {.code=Code::invalid_sample};
        }
        return result;
    }
}
