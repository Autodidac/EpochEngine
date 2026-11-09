// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/fft_ocean/fft_ocean.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace ocean = epoch::extensions::fft_ocean;
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

namespace
{
    std::uint64_t hash(std::span<const ocean::OceanSample> samples) noexcept
    {
        std::uint64_t result = 1469598103934665603ull;
        for (const auto& s : samples)
            for (const float value : std::array{s.xMeters,s.zMeters,s.heightMeters,
                s.displacementXMeters,s.displacementZMeters,s.normalX,s.normalY,s.normalZ,s.foam})
            {
                const auto bits = std::bit_cast<std::uint32_t>(value);
                for (unsigned i=0;i<4;++i) { result ^= (bits >> (i*8)) & 255u; result *= 1099511628211ull; }
            }
        return result;
    }
    bool finite(std::span<const ocean::OceanSample> samples) noexcept
    {
        for (const auto& s : samples)
        {
            for (const float value : std::array{s.xMeters,s.zMeters,s.heightMeters,
                s.displacementXMeters,s.displacementZMeters,s.normalX,s.normalY,s.normalZ,s.foam})
                if (!std::isfinite(value)) return false;
            const float length = std::sqrt(s.normalX*s.normalX+s.normalY*s.normalY+s.normalZ*s.normalZ);
            if (std::abs(length-1.0f)>0.0001f || s.foam<0.0f || s.foam>1.0f) return false;
        }
        return true;
    }
}

int main()
{
    try
    {
        ocean::OceanSettings settings;
        settings.resolution=32; settings.tileLengthMeters=96; settings.seed=42;
        const auto admission=ocean::admit(settings);
        CHECK(admission);
        CHECK(admission.memory.residentBytes==53248u);
        CHECK(admission.memory.evaluationPeakBytes==98560u);
        CHECK(admission.memory.replacementPeakBytes==151808u);
        ocean::OceanTile tile{settings};
        CHECK(tile.samples().size()==1024u);
        CHECK(tile.evaluate(3.25f));
        const auto first=hash(tile.samples());
        CHECK(first==17368026875614010600ull); // Physical spectral-density conversion, D/R bytes.
        CHECK(tile.evaluate(19.5f)); CHECK(hash(tile.samples())!=first);
        CHECK(tile.evaluate(3.25f)); CHECK(hash(tile.samples())==first);
        CHECK(tile.evaluate(-3.25f)); CHECK(hash(tile.samples())==688061740171333509ull);
        CHECK(tile.evaluate(3.25f)); CHECK(hash(tile.samples())==first);
        CHECK(finite(tile.samples()));
        ocean::OceanTile restarted{settings};
        CHECK(restarted.evaluate(3.25f)); CHECK(hash(restarted.samples())==first);

        const float nan=std::numeric_limits<float>::quiet_NaN();
        const float infinity=std::numeric_limits<float>::infinity();
        for(const auto time:{nan,infinity,-infinity,(std::numeric_limits<float>::max)()})
        {
            CHECK(tile.evaluate(time).code==ocean::OceanCode::invalid_time);
            CHECK(hash(tile.samples())==first);
        }
        const std::array members{&ocean::OceanSettings::tileLengthMeters,
            &ocean::OceanSettings::windSpeedMetersPerSecond,&ocean::OceanSettings::windDirectionRadians,
            &ocean::OceanSettings::amplitude,&ocean::OceanSettings::choppiness,
            &ocean::OceanSettings::normalStrength,&ocean::OceanSettings::foamSlopeThreshold,
            &ocean::OceanSettings::gravity};
        for(const auto member:members) for(const auto value:{nan,infinity,-infinity})
        {
            auto invalid=settings; invalid.*member=value;
            CHECK(!ocean::admit(invalid)); CHECK(!tile.reset(invalid));
            CHECK(hash(tile.samples())==first);
            CHECK(tile.settings().seed==42u && tile.resolution()==32u && tile.tileLengthMeters()==96.0f);
        }
        for(const auto n:{0u,4u,7u,9u,513u,1024u,0x80000000u,0xffffffffu})
        {
            auto invalid=settings; invalid.resolution=n;
            CHECK(!ocean::admit(invalid)); CHECK(!tile.reset(invalid)); CHECK(hash(tile.samples())==first);
        }
        for(const auto member:members)
        {
            if(member==&ocean::OceanSettings::windDirectionRadians) continue;
            auto invalid=settings; invalid.*member=-1.0f;
            CHECK(!ocean::admit(invalid)); CHECK(!tile.reset(invalid));
        }
        for(const auto member:{&ocean::OceanSettings::tileLengthMeters,
            &ocean::OceanSettings::gravity,&ocean::OceanSettings::normalStrength})
        {
            auto invalid=settings; invalid.*member=0.0f;
            CHECK(!ocean::admit(invalid)); CHECK(!tile.reset(invalid));
        }
        auto invalid=settings; invalid.windSpeedMetersPerSecond=(std::numeric_limits<float>::max)();
        CHECK(!ocean::admit(invalid)); CHECK(!tile.reset(invalid));
        invalid=settings; invalid.tileLengthMeters=(std::numeric_limits<float>::denorm_min)();
        CHECK(!ocean::admit(invalid)); CHECK(!tile.reset(invalid));

        for(const ocean::OceanLimits limits:std::array<ocean::OceanLimits,5>{{{1024,67108864},{0,67108864},
            {512,0},{512,(std::numeric_limits<std::uint64_t>::max)()},{512,98559}}})
        {
            CHECK(!ocean::admit(settings,limits)); CHECK(!tile.reset(settings,limits)); CHECK(hash(tile.samples())==first);
        }
        CHECK(tile.reset(settings,{512,admission.memory.replacementPeakBytes-1u}).code==ocean::OceanCode::limit_exceeded);
        CHECK(hash(tile.samples())==first);
        CHECK(tile.reset(settings,{512,admission.memory.replacementPeakBytes}));
        CHECK(tile.evaluate(3.25f)); CHECK(hash(tile.samples())==first);
        ocean::OceanTile exact{settings,{512,admission.memory.evaluationPeakBytes}};
        CHECK(exact.evaluate(3.25f)); CHECK(hash(exact.samples())==first);

        for(const auto value:{nan,infinity,-infinity,(std::numeric_limits<float>::max)()})
        {
            CHECK(tile.sample(value,0).heightMeters==0.0f); CHECK(tile.sample(0,value).normalY==1.0f);
        }
        const auto center=tile.sample(0,0),periodic=tile.sample(96,-96);
        CHECK(center.heightMeters==periodic.heightMeters);
        CHECK(center.displacementXMeters==periodic.displacementXMeters);
        auto flat=settings; flat.amplitude=0.0f;flat.windSpeedMetersPerSecond=0.0f;
        CHECK(tile.reset(flat)); CHECK(tile.evaluate(-12.0f)); CHECK(finite(tile.samples()));
        for(const auto& sample:tile.samples()) CHECK(sample.heightMeters==0.0f && sample.normalY==1.0f);

        // Resolution refines a fixed physical sea; it must not divide wave
        // height by N squared. Average seeds because the retained seeded random
        // sequence assigns different modes when the lattice size changes.
        double referenceRms{};
        for(const auto resolution:{32u,64u,128u})
        {
            double meanSquare{};
            for(std::uint32_t seed=0u;seed<32u;++seed)
            {
                ocean::OceanSettings sea;sea.resolution=resolution;sea.seed=seed;
                ocean::OceanTile measured{sea};CHECK(measured.evaluateAbsolute(3.25));
                for(const auto& sample:measured.samples())
                    meanSquare+=double(sample.heightMeters)*sample.heightMeters
                        /double(measured.samples().size())/32.0;
            }
            const double rms=std::sqrt(meanSquare);
            CHECK(rms>0.1 && rms<3.0);
            if(referenceRms==0.0)referenceRms=rms;
            CHECK(rms/referenceRms>0.8 && rms/referenceRms<1.2);
        }

        auto large=settings; large.resolution=512;
        const auto memory=ocean::admit(large);
        CHECK(memory && memory.memory.residentBytes==13631488u);
        CHECK(memory.memory.evaluationPeakBytes==25169920u && memory.memory.replacementPeakBytes==38801408u);
        CHECK(tile.reset(large)); CHECK(tile.evaluate(3.25f)); CHECK(finite(tile.samples()));
        const auto largeHash=hash(tile.samples());
        CHECK(tile.evaluate(1.0f)); CHECK(tile.evaluate(3.25f)); CHECK(hash(tile.samples())==largeHash);
        CHECK(tile.reset(large,{512,memory.memory.replacementPeakBytes-1u}).code==ocean::OceanCode::limit_exceeded);
        CHECK(hash(tile.samples())==largeHash);
        CHECK(tile.reset(large,{512,memory.memory.replacementPeakBytes}));
        CHECK(tile.evaluate(3.25f)); CHECK(hash(tile.samples())==largeHash);
        ocean::OceanTile moved{std::move(tile)};
        CHECK(tile.evaluate(0).code==ocean::OceanCode::invalid_settings);
        CHECK(tile.sample(0,0).normalY==1.0f);
        CHECK(tile.reset(settings)); CHECK(moved.evaluate(0));
        return 0;
    }
    catch (...) { return 250; }
}
