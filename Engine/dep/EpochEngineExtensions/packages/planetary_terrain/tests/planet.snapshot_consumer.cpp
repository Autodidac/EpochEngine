// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <vector>
import planet.terrain;
import project.planetary_snapshot;
import asset.mesh_artifact;
import asset.cloud_volume;
namespace p=epochengine::project_planetary_snapshot;
namespace mesh=epochengine::asset::mesh;
namespace core=epoch::extensions::planetary_terrain::core;

namespace
{
    constexpr p::Limits limits{{65536u,196608u,4u*1024u*1024u}};
    std::shared_ptr<p::Snapshot> fixture(unsigned depth,std::uint64_t request,std::uint8_t branch=0u)
    {
        auto result=std::make_shared<p::Snapshot>();
        result->metadata.identity.source_hash="sha256:"+std::string(64u,'a');
        result->metadata.identity.recipe_key=std::string(64u,'b');result->metadata.identity.request_id=request;
        std::vector<mesh::Vertex> vertices;std::vector<std::uint32_t> indices;
        const auto add=[&](core::TileKey key,bool target){
            p::Patch patch;patch.key={key.x,key.y,key.body,key.face,key.level};patch.target=target;
            unsigned n{};for(const auto neighbor:core::neighbors(key))patch.neighbors[n++]={neighbor.x,neighbor.y,neighbor.body,neighbor.face,neighbor.level};
            patch.first_vertex=static_cast<std::uint32_t>(vertices.size());patch.vertex_count=4u;
            patch.first_index=static_cast<std::uint32_t>(indices.size());patch.index_count=6u;
            const auto side=static_cast<float>(1u<<key.level);const float z=key.face*2.0f+key.level*.1f;
            for(unsigned i=0;i<4u;++i)
            {
                mesh::Vertex vertex;vertex.position={(key.x+(i%2u))/side,(key.y+(i/2u))/side,z};vertices.push_back(vertex);
                result->metadata.parent_delta.push_back({0.f,0.f,key.level?-.1f:0.f});
            }
            for(const auto index:std::array<std::uint32_t,6>{0,1,2,1,3,2})indices.push_back(patch.first_vertex+index);
            result->metadata.patches.push_back(patch);
        };
        for(std::uint8_t face=0;face<6u;++face)add({0,0,0,face,0},!depth||(depth<3u&&face!=branch));
        if(depth)for(std::uint8_t face=0;face<6u;++face)
            if(depth>=3u||face==branch)
                for(const auto child:core::children({0,0,0,face,0}))add(child,depth<2u||face!=branch||child.x||child.y);
        if(depth>1u)for(const auto child:core::children({0,0,0,branch,1}))add(child,true);
        result->artifact=mesh::build_artifact(vertices,indices,limits.artifact).artifact;
        if(result->artifact)result->metadata.identity.artifact_key=result->artifact->key();
        return result;
    }
    bool coverage(const p::Transition& transition)
    {
        std::array<std::uint64_t,6> area{};
        for(const auto& d:transition.draw())
        {
            const auto key=d.snapshot->metadata.patches[d.patch].key;
            area[key.face]+=std::uint64_t{1}<<(2u*(20u-key.level));
            if(d.morph<0.f||d.morph>1.f)return false;
            for(const auto& e:transition.draw())
            {
                const auto other=e.snapshot->metadata.patches[e.patch].key;
                if(key!=other&&(p::contains(key,other)||p::contains(other,key)))return false;
            }
        }
        for(const auto value:area)if(value!=(std::uint64_t{1}<<40u))return false;
        return true;
    }
}
int main(int argc,char** argv)
{
    if(argc==4||argc==5)
    {
        const bool large=argc==5&&std::string{argv[4]}=="--large-selected";
        if(argc==5&&!large)return 48;
        auto limits=::limits;
        if(large)
        {
            limits.artifact={762599u,6291456u,32ull*1024u*1024u};
            limits.maximum_retained_bytes=128ull*1024u*1024u;
            limits.maximum_transient_bytes=128ull*1024u*1024u;
        }
        const auto artifact=mesh::read_artifact_file(argv[1],limits.artifact);if(!artifact)return 40;
        std::uint64_t request{};const std::string id{argv[3]};
        const auto parsed=std::from_chars(id.data(),id.data()+id.size(),request);
        if(parsed.ec!=std::errc{}||parsed.ptr!=id.data()+id.size())return 41;
        std::ifstream input{argv[2],std::ios::binary|std::ios::ate};if(!input||input.tellg()>16u*1024u*1024u||input.tellg()<0)return 42;
        std::vector<std::byte> encoded(static_cast<std::size_t>(input.tellg()));input.seekg(0);
        if(!input.read(reinterpret_cast<char*>(encoded.data()),static_cast<std::streamsize>(encoded.size())))return 43;
        p::Identity expected{artifact.artifact->key(),"sha256:"+std::string(64u,'a'),std::string(64u,'b'),request};
        auto metadata=p::decode(encoded,*artifact.artifact,expected,limits);if(!metadata)return 44;
        auto snapshot=std::make_shared<const p::Snapshot>(p::Snapshot{artifact.artifact,std::move(*metadata)});
        p::Transition animation{limits};
        if(!animation.admit(snapshot,{},large?p::InitialCut::selected_patches:p::InitialCut::coarse_roots))return 45;
        if(large)return animation.settled()&&coverage(animation)&&animation.materialize()?0:46;
        for(unsigned i=0;i<128u&&!animation.settled();++i)
            if(!animation.advance(.1)||!coverage(animation)||!animation.materialize())return 46;
        return animation.settled()?0:47;
    }
    if(argc!=1)return 48;
    auto fine=fixture(1,1),coarse=fixture(0,2);
    if(!fine->artifact||!coarse->artifact||!p::valid_metadata(fine->metadata,*fine->artifact,limits))return 1;
    // Production v4 codec keeps old envelopes unchanged when clouds are absent.
    auto cloudy=fine->metadata;
    cloudy.cloud=p::CloudLayer{0u,{100.0,200.0,300.0},6371000.0,604800.0,coarse->artifact};
    const auto cloudBytes=p::encode(cloudy,*fine->artifact,limits);
    if(cloudBytes.empty())return 60;
    const auto roundtrip=p::decode(cloudBytes,*fine->artifact,cloudy.identity,limits);
    if(!roundtrip||!roundtrip->cloud||roundtrip->cloud->artifact->key()!=coarse->artifact->key()
        ||roundtrip->cloud->center_meters!=cloudy.cloud->center_meters)return 61;
    auto corruptedCloud=cloudBytes;corruptedCloud[corruptedCloud.size()-34u]^=std::byte{1};
    if(p::decode(corruptedCloud,*fine->artifact,cloudy.identity,limits))return 62;
    if(p::decode(std::span{cloudBytes}.first(cloudBytes.size()-1u),*fine->artifact,cloudy.identity,limits))return 63;
    auto wrongBody=cloudy;wrongBody.cloud->body=1u;if(p::valid_metadata(wrongBody,*fine->artifact,limits))return 64;
    auto invalidCloud=cloudy;invalidCloud.cloud->radius_meters=NAN;if(p::valid_metadata(invalidCloud,*fine->artifact,limits))return 65;
    auto cloudLimit=limits;cloudLimit.maximum_metadata_bytes=cloudBytes.size()-1u;
    if(p::decode(cloudBytes,*fine->artifact,cloudy.identity,cloudLimit))return 66;
    std::stop_source cloudStop;cloudStop.request_stop();
    if(p::decode(cloudBytes,*fine->artifact,cloudy.identity,limits,cloudStop.get_token()))return 67;
    auto cloudless=*roundtrip;cloudless.cloud.reset();
    if(p::encode(cloudless,*fine->artifact,limits)!=p::encode(fine->metadata,*fine->artifact,limits))return 68;
    auto volume=fine->metadata;
    epochengine::asset::cloud_volume::Recipe recipe{77u,2700.0,2160.0,12742.0,0.60f,0.0012f};
    volume.cloud=p::CloudLayer{0u,{1e9,2e9,3e9},6371000.0,604800.0,{},recipe};
    const auto volumeBytes=p::encode(volume,*fine->artifact,limits);
    if(volumeBytes.empty()||std::to_integer<unsigned>(volumeBytes[4])!=5u)return 80;
    const auto volumeRead=p::decode(volumeBytes,*fine->artifact,volume.identity,limits);
    if(!volumeRead||!volumeRead->cloud||volumeRead->cloud->artifact
        ||volumeRead->cloud->density!=volume.cloud->density||volumeRead->cloud->center_meters!=volume.cloud->center_meters)return 81;
    auto damagedVolume=volumeBytes;damagedVolume[damagedVolume.size()-36u]^=std::byte{1};
    if(p::decode(damagedVolume,*fine->artifact,volume.identity,limits))return 82;
    if(p::decode(std::span{volumeBytes}.first(volumeBytes.size()-1u),*fine->artifact,volume.identity,limits))return 83;
    if(p::decode(volumeBytes,*fine->artifact,volume.identity,limits,cloudStop.get_token()))return 84;
    auto ambiguous=volume;ambiguous.cloud->artifact=coarse->artifact;
    if(p::valid_metadata(ambiguous,*fine->artifact,limits)||!p::encode(ambiguous,*fine->artifact,limits).empty())return 85;
    auto invalidDensity=volume;invalidDensity.cloud->density->thickness_meters=NAN;
    if(p::valid_metadata(invalidDensity,*fine->artifact,limits))return 86;
    invalidDensity=volume;invalidDensity.cloud->density->wind_period_seconds=60.0;
    if(p::valid_metadata(invalidDensity,*fine->artifact,limits))return 87;
    auto densityLimit=limits;densityLimit.maximum_metadata_bytes=volumeBytes.size()-1u;
    if(p::decode(volumeBytes,*fine->artifact,volume.identity,densityLimit))return 88;
    auto densityless=*volumeRead;densityless.cloud.reset();
    if(p::encode(densityless,*fine->artifact,limits)!=p::encode(fine->metadata,*fine->artifact,limits))return 89;
    const auto encoded=p::encode(fine->metadata,*fine->artifact,limits);
    if(encoded.empty()||!p::decode(encoded,*fine->artifact,fine->metadata.identity,limits))return 2;
    auto bad=encoded;bad.back()^=std::byte{1};if(p::decode(bad,*fine->artifact,fine->metadata.identity,limits))return 3;
    auto stale=fine->metadata.identity;++stale.request_id;if(p::decode(encoded,*fine->artifact,stale,limits))return 4;
    stale=fine->metadata.identity;stale.source_hash[7]='c';if(p::decode(encoded,*fine->artifact,stale,limits))return 5;
    stale=fine->metadata.identity;stale.recipe_key[0]='c';if(p::decode(encoded,*fine->artifact,stale,limits))return 6;
    auto hole=fine->metadata;hole.patches.back().target=false;if(p::valid_metadata(hole,*fine->artifact,limits))return 7;
    auto overlap=fine->metadata;overlap.patches.front().target=true;if(p::valid_metadata(overlap,*fine->artifact,limits))return 8;
    auto range=fine->metadata;++range.patches.back().first_vertex;if(p::valid_metadata(range,*fine->artifact,limits))return 9;
    std::stop_source cancelled;cancelled.request_stop();
    if(p::decode(encoded,*fine->artifact,fine->metadata.identity,limits,cancelled.get_token()))return 10;
    p::Transition animation{limits};if(!animation.admit(fine)||animation.draw().size()!=6u)return 11;
    if(animation.advance(-1)||animation.advance(NAN)||!coverage(animation)||animation.draw().size()!=6u)return 12;
    if(!animation.advance(.05)||animation.draw().size()!=9u)return 13;
    for(const auto& d:animation.draw())if(d.snapshot->metadata.patches[d.patch].key.level&&d.morph!=1.f)return 14;
    const auto parent_endpoint=animation.materialize();if(!parent_endpoint)return 15;
    if(!animation.advance(.05))return 16;
    float phase{};for(const auto& d:animation.draw())if(d.snapshot->metadata.patches[d.patch].key.level)phase=d.morph;
    if(std::abs(phase-.75f)>1e-6f)return 17;
    if(!animation.advance(10,true)||animation.advance(.05,false,cancelled.get_token()))return 18;
    for(const auto& d:animation.draw())if(d.snapshot->metadata.patches[d.patch].key.level&&d.morph!=phase)return 19;
    // Mid-transition coarsen reverses from the current phase, then a fresh
    // refinement reverses again without resetting the vertex position.
    if(!animation.admit(coarse)||!animation.advance(.025))return 20;
    for(const auto& d:animation.draw())if(d.snapshot->metadata.patches[d.patch].key.level&&std::abs(d.morph-.875f)>1e-6f)return 21;
    auto retarget=fixture(1,3);if(!animation.admit(retarget)||!animation.advance(.025))return 22;
    for(const auto& d:animation.draw())if(d.snapshot->metadata.patches[d.patch].key.level&&std::abs(d.morph-.75f)>1e-6f)return 23;
    for(unsigned i=0;i<8u;++i)if(!animation.advance(.1)||!coverage(animation))return 24;
    if(!animation.settled()||!animation.materialize())return 25;
    coarse=fixture(0,4);if(!animation.admit(coarse))return 26;
    for(unsigned i=0;i<8u;++i)if(!animation.advance(.1)||!coverage(animation))return 27;
    if(!animation.settled()||animation.draw().size()!=6u)return 28;
    // Two adjacent cube-face roots must refine to level one before one child
    // may refine to level two. This isolated deep target is unbalanced.
    auto deep=fixture(2,5);if(p::valid_metadata(deep->metadata,*deep->artifact,limits))return 29;
    auto wrong=fixture(1,5);wrong->metadata.identity.source_hash[7]='c';if(animation.admit(wrong))return 30;
    if(animation.admit(coarse)||animation.admit(retarget))return 31;
    p::Transition tiny{{{65536u,196608u,1024u}}};if(tiny.admit(fine))return 32;
    auto retained_limit=limits;retained_limit.maximum_retained_bytes=1024u;
    p::Transition retained{retained_limit};if(retained.admit(fine))return 55;
    auto transient_limit=limits;transient_limit.maximum_transient_bytes=1024u;
    p::Transition transient{transient_limit};if(transient.admit(fine))return 56;
    if(!animation.materialize()||animation.materialize(cancelled.get_token()))return 33;
    // Corrupt/truncated metadata cannot reserve from an untrusted count.
    if(p::decode(std::span{encoded}.first(10u),*fine->artifact,fine->metadata.identity,limits))return 34;
    // Balanced multi-level refinement then complete coarsening retain all six
    // faces, including cube edges and the four polar neighbor orientations.
    auto balanced=fixture(3,5);if(!p::valid_metadata(balanced->metadata,*balanced->artifact,limits)||!animation.admit(balanced))return 35;
    for(unsigned i=0;i<20u;++i)if(!animation.advance(.1)||!coverage(animation)||!animation.materialize())return 36;
    if(!animation.settled()||animation.draw().size()!=27u)return 37;
    if(!animation.admit(fixture(0,6)))return 38;
    for(unsigned i=0;i<20u;++i)if(!animation.advance(.1)||!coverage(animation))return 39;
    if(!animation.settled()||animation.draw().size()!=6u)
    {
        std::ofstream diagnostic{"planet.snapshot_failure.txt"};
        for(const auto& d:animation.draw())
        {const auto k=d.snapshot->metadata.patches[d.patch].key;diagnostic<<unsigned(k.face)<<' '<<unsigned(k.level)<<' '<<k.x<<' '<<k.y<<' '<<d.morph<<'\n';}
        return 50;
    }
    // Move the refinement request to a disjoint face during transition.
    if(!animation.admit(fixture(1,7,4))||!animation.advance(.1)||!animation.advance(.05)
        ||!animation.admit(fixture(1,8,1)))return 51;
    for(unsigned i=0;i<20u;++i)if(!animation.advance(.025)||!coverage(animation)||!animation.materialize())return 52;
    if(!animation.settled()||animation.draw().size()!=9u)return 53;
    auto wrong_body=fixture(1,9,1);
    for(auto& patch:wrong_body->metadata.patches){patch.key.body=1u;for(auto& neighbor:patch.neighbors)neighbor.body=1u;}
    if(animation.admit(wrong_body))return 54;
    return 0;
}
