// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stop_token>
#include <vector>
#include "epoch/extensions/fft_ocean/fft_ocean.hpp"
import water.fft;
import water.system;
import water.surface;
import asset.mesh_artifact;

#define CHECK(...) do {if(!(__VA_ARGS__))return __LINE__;} while(false)
namespace w=epochengine::water;
namespace f=epochengine::water::fft;
namespace p=epoch::extensions::fft_ocean;

std::uint64_t hash(const w::wave::Tile& tile)
{
    std::uint64_t result=1469598103934665603ull;
    for(const auto& s:tile.samples())
        for(const auto value:{s.height,s.displacement_x,s.displacement_z,s.normal[0],s.normal[1],s.normal[2],s.foam})
        {result^=std::bit_cast<std::uint32_t>(value);result*=1099511628211ull;}
    return result;
}

int main(int argc,char** argv)
{
    CHECK(argc==2);
    f::Settings settings;settings.resolution=32;settings.seed=42;
    f::Evaluator evaluator;
    auto first=evaluator.evaluate(settings,3.25);CHECK(first&&!first.reused);
    const auto firstHash=hash(*first.tile);
    CHECK(evaluator.evaluate(settings,3.25).reused);
    p::OceanTile actual{settings};CHECK(actual.evaluateAbsolute(3.25));
    CHECK(first.tile->samples().size()==actual.samples().size());
    for(std::size_t i=0;i<actual.samples().size();++i)
    {
        CHECK(first.tile->samples()[i].height==actual.samples()[i].heightMeters);
        CHECK(first.tile->samples()[i].normal[0]==actual.samples()[i].normalX);
        CHECK(first.tile->samples()[i].foam==actual.samples()[i].foam);
    }
    CHECK(evaluator.evaluate(settings,9.0));CHECK(hash(*evaluator.current())!=firstHash);
    CHECK(evaluator.evaluate(settings,-3.25));CHECK(hash(*evaluator.current())!=firstHash);
    CHECK(evaluator.evaluate(settings,3.25));CHECK(hash(*evaluator.current())==firstHash);
    // Beyond float-time precision, consecutive 60Hz samples must still move.
    CHECK(evaluator.evaluate(settings,100000000.0));const auto late=hash(*evaluator.current());
    CHECK(evaluator.evaluate(settings,100000000.0+1.0/60.0));CHECK(hash(*evaluator.current())!=late);
    CHECK(evaluator.evaluate(settings,100000000.0));CHECK(hash(*evaluator.current())==late);
    CHECK(evaluator.evaluate(settings,3.25));
    const auto retained=evaluator.current();
    CHECK(evaluator.evaluate(settings,4.0,1u).code==f::Code::limit_exceeded);
    CHECK(evaluator.current()==retained);
    std::stop_source stop;stop.request_stop();
    CHECK(evaluator.evaluate(settings,4.0,67108864u,stop.get_token()).code==f::Code::cancelled);
    CHECK(evaluator.current()==retained);
    CHECK(!evaluator.evaluate(settings,std::numeric_limits<double>::infinity()));
    CHECK(evaluator.current()==retained);
    CHECK(!retained->sample(0.0,0.0,4.0));
    const auto a=retained->sample(-64.0,64.0,3.25),b=retained->sample(64.0,-64.0,3.25);
    CHECK(a&&b&&a->height==b->height&&a->normal==b->normal);
    CHECK(retained->sample(1e100,-1e100,3.25));
    CHECK(!retained->sample(std::numeric_limits<double>::quiet_NaN(),0.0,3.25));
    for(const auto point:{std::array{-64.0,64.0,0.0},std::array{1.25,-9.5,129.75},
        std::array{1e100,-1e100,3e99}})
    {
        const auto fields=retained->displacement_projections(point,3.25);CHECK(fields);
        const std::array planes{retained->sample(point[1],point[2],3.25),
            retained->sample(point[0],point[2],3.25),retained->sample(point[0],point[1],3.25)};
        for(unsigned i=0;i<3u;++i)
            CHECK(planes[i]&&(*fields)[i].height==planes[i]->height
                &&(*fields)[i].displacement_x==planes[i]->displacement_x
                &&(*fields)[i].displacement_z==planes[i]->displacement_z
                &&(*fields)[i].foam==planes[i]->foam);
    }
    CHECK(!retained->displacement_projections({0,0,0},4.0));
    CHECK(!retained->displacement_projections({0,std::numeric_limits<double>::quiet_NaN(),0},3.25));

    w::WaterBodyDesc body;body.waveTile=retained;body.halfExtents={8,4,8};
    w::WaterManager manager{1};const auto handle=manager.create(body);CHECK(handle.valid());
    const auto sampled=manager.sample(handle,{{0,0,0},3.25});
    const auto expected=retained->sample(0,0,3.25);CHECK(expected&&sampled.found);
    CHECK(sampled.surfaceHeight==expected->height&&sampled.foam==expected->foam);
    CHECK(!manager.sample(handle,{{0,0,0},4.0}).found);
    const auto surface=w::surface::build(body,3.25,8);CHECK(surface);
    CHECK(!w::surface::build(body,4.0,8));
    const auto bytes=epochengine::asset::mesh::encode_artifact(*surface.artifact);
    CHECK(w::surface::build(body,3.25,8).artifact->key()==surface.artifact->key());

    constexpr double r=0.7071067811865475244;
    for(const auto direction:std::array<std::array<double,3>,8>{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},
        {0,0,1},{0,0,-1},{r,r,0},{r,0,r}}})
    {
        const auto sphere=w::wave::spherical_sample(*retained,direction,6371000.0,3.25);CHECK(sphere);
        const auto repeated=w::wave::spherical_sample(*retained,direction,6371000.0,3.25);CHECK(repeated);
        CHECK(sphere->position_meters==repeated->position_meters&&sphere->normal==repeated->normal);
        const auto length=std::hypot(sphere->normal[0],sphere->normal[1],sphere->normal[2]);
        CHECK(std::abs(length-1.0)<1e-5);
        double outward{};for(std::size_t i=0;i<3;++i)outward+=sphere->normal[i]*direction[i];
        CHECK(outward>0.95);
        const auto radius=std::hypot(sphere->position_meters[0],sphere->position_meters[1],sphere->position_meters[2]);
        CHECK(std::abs(radius-6371000.0)<10.0);
    }
    CHECK(!w::wave::spherical_sample(*retained,{0,0,0},6371000.0,3.25));
    CHECK(!w::wave::spherical_sample(*retained,{1,0,0},6371000.0,4.0));
    std::ofstream output{argv[1]};output<<firstHash<<'\n'<<late<<'\n'<<bytes.size()<<'\n';
    output<<"PASS: real FFT, exact-time rewind, bounded retention, cancellation, shared Water samples, curved seam coordinates\n";
    output.close();CHECK(output.good());
    return 0;
}
