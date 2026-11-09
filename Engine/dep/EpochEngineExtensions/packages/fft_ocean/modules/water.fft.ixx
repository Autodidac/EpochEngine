// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford
module;
#include "epoch/extensions/fft_ocean/fft_ocean.hpp"
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

export module water.fft;
export import water.wave;

export namespace epochengine::water::fft
{
    using Settings=::epoch::extensions::fft_ocean::OceanSettings;
    using Code=wave::Code;
    using Result=wave::Evaluation;

    // A worker-owned implementation evaluator, not a scheduler or Water body
    // manager. The existing host Scheduler supplies cancellation and lifetime.
    // Published tiles are immutable, including their actual evaluation time.
    class Evaluator final
    {
    public:
        [[nodiscard]] Result evaluate(const Settings& settings,double seconds,
            std::uint64_t maximum_bytes=64u*1024u*1024u,std::stop_token stop={}) noexcept
        {
            namespace provider=::epoch::extensions::fft_ocean;
            if(stop.stop_requested())return {Code::cancelled,tile_};
            if(!std::isfinite(seconds)||std::abs(seconds)>=1e12)return {Code::invalid,tile_};
            if(settings_&&same(*settings_,settings)&&tile_&&seconds==tile_->seconds())
            {
                if(tile_->payload_bytes()>maximum_bytes)return {Code::limit_exceeded,tile_};
                return {Code::ready,tile_,tile_->payload_bytes(),true};
            }
            const auto admitted=provider::admit(settings);
            if(!admitted)return {admitted.code==provider::OceanCode::limit_exceeded
                ?Code::limit_exceeded:Code::invalid,tile_};
            const auto count=std::uint64_t(settings.resolution)*settings.resolution;
            const auto previous=tile_?tile_->payload_bytes():0u;
            const auto converted=count*sizeof(wave::Sample);
            const auto peak=previous+admitted.memory.evaluationPeakBytes+converted;
            if(maximum_bytes>64u*1024u*1024u||peak>maximum_bytes)
                return {Code::limit_exceeded,tile_,peak};
            try
            {
                provider::OceanTile implementation{settings};
                if(stop.stop_requested())return {Code::cancelled,tile_,peak};
                if(!implementation.evaluateAbsolute(seconds))return {Code::evaluation_failed,tile_,peak};
                if(stop.stop_requested())return {Code::cancelled,tile_,peak};
                std::vector<wave::Sample> samples;
                samples.reserve(static_cast<std::size_t>(count));
                for(const auto& value:implementation.samples())
                {
                    if(stop.stop_requested())return {Code::cancelled,tile_,peak};
                    samples.push_back({value.heightMeters,value.displacementXMeters,value.displacementZMeters,
                        {value.normalX,value.normalY,value.normalZ},value.foam});
                }
                auto next=wave::Tile::create(settings.resolution,settings.tileLengthMeters,seconds,
                    std::move(samples),maximum_bytes);
                if(!next)return {Code::evaluation_failed,tile_,peak};
                if(stop.stop_requested())return {Code::cancelled,tile_,peak};
                tile_=std::move(next);settings_=settings;
                return {Code::ready,tile_,peak};
            }
            catch(...) {return {Code::evaluation_failed,tile_,peak};}
        }
        [[nodiscard]] std::shared_ptr<const wave::Tile> current() const noexcept {return tile_;}
    private:
        [[nodiscard]] static bool same(const Settings& a,const Settings& b) noexcept
        {
            return a.resolution==b.resolution&&a.tileLengthMeters==b.tileLengthMeters
                &&a.windSpeedMetersPerSecond==b.windSpeedMetersPerSecond
                &&a.windDirectionRadians==b.windDirectionRadians&&a.amplitude==b.amplitude
                &&a.choppiness==b.choppiness&&a.normalStrength==b.normalStrength
                &&a.foamSlopeThreshold==b.foamSlopeThreshold&&a.gravity==b.gravity&&a.seed==b.seed;
        }
        std::optional<Settings> settings_{};
        std::shared_ptr<const wave::Tile> tile_{};
    };
    class Implementation final : public wave::Provider
    {
    public:
        explicit Implementation(Settings settings):settings_(settings){}
        [[nodiscard]] Result evaluate(double seconds,std::uint64_t maximum_bytes,std::stop_token stop={}) noexcept override
        {return evaluator_.evaluate(settings_,seconds,maximum_bytes,stop);}
    private:
        Settings settings_{};
        Evaluator evaluator_{};
    };
}
