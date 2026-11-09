// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
import project.planetary;

int main()
{
    namespace planet = epochengine::project_planetary;
    for (const auto mode : planet::modes)
    {
        planet::Settings source;
        source.mode = mode;
        source.seed = std::numeric_limits<std::uint32_t>::max();
        source.tile_span_meters = 1.234567890123456;
        source.stream_lod = false;
        source.maximum_level = 7;
        source.maximum_leaves = 60;
        source.resident_megabytes = 12;
        const auto text = planet::encode_settings(source);
        const auto loaded = planet::decode_settings(text);
        if (!loaded || *loaded != source) return 1;
        if (planet::decode_settings(text + "seed=2\n")) return 2;
        if (planet::decode_settings(text.substr(0, text.size() - 1u))) return 3;
        auto bad = text;
        const auto offset = bad.find("tile_span_meters=") + 17u;
        bad.replace(offset, bad.size() - offset, "nan\n");
        if (planet::decode_settings(bad)) return 4;
    }
    planet::Settings oversized;
    oversized.mode = "universe";
    oversized.cells_per_tile = 64u;
    if (planet::valid_settings(oversized) || !planet::encode_settings(oversized).empty()) return 5;
    oversized.mode = "tiled";
    oversized.cells_per_tile = 16u;
    oversized.tiles_per_axis = 16u;
    if (planet::valid_settings(oversized)) return 6;
    if (planet::decode_settings("epoch.planet.recipe.v2\nmode=planet\n")) return 7;
    const auto legacy = planet::decode_settings("epoch.planet.recipe.v1\nmode=planet\nseed=1337\ncells_per_tile=16\ntiles_per_axis=4\nbody_index=0\ntile_span_meters=128\n");
    if (!legacy || legacy->stream_lod || legacy->ocean_surface || legacy->maximum_leaves != 48u) return 8;
    auto invalid = *legacy;
    invalid.resident_megabytes = 0;
    if (!planet::encode_settings(invalid).empty()) return 9;
    const std::string dryGolden="epoch.planet.recipe.v2\nmode=planet\nseed=1337\ncells_per_tile=16\ntiles_per_axis=4\nbody_index=0\ntile_span_meters=128\nstream_lod=1\nmaximum_level=10\nmaximum_leaves=48\nresident_megabytes=16\n";
    if(planet::encode_settings(planet::Settings{})!=dryGolden) return 10;
    const auto dry=planet::decode_settings(dryGolden);
    if(!dry||dry->ocean_surface) return 11;
    for(const auto mode: {"planet","hemisphere","universe"})
    {
        planet::Settings wet;wet.mode=mode;wet.ocean_surface=true;
        const auto text=planet::encode_settings(wet);
        const auto decoded=planet::decode_settings(text);
        if(!text.starts_with("epoch.planet.recipe.v3\n")||!text.ends_with("ocean_surface=1\n")
            ||!decoded||*decoded!=wet) return 12;
        if(planet::decode_settings(text+"ocean_surface=0\n")) return 13;
        auto malformed=text;
        malformed.replace(malformed.find("ocean_surface=1"),15u,"ocean_surface=2");
        if(planet::decode_settings(malformed)) return 14;
        malformed=text;malformed.erase(malformed.find("ocean_surface="));
        if(planet::decode_settings(malformed)) return 15;
        malformed=text;malformed.replace(0u,21u,"epoch.planet.recipe.v2");
        if(planet::decode_settings(malformed)) return 16;
    }
    for(const auto mode:{"classic","tiled"})
    {
        auto wet=planet::Settings{};wet.mode=mode;wet.ocean_surface=true;
        if(planet::valid_settings(wet)||!planet::encode_settings(wet).empty()) return 17;
    }
    auto budget=planet::Settings{};budget.cells_per_tile=32;
    if(!planet::valid_settings(budget)) return 18;
    budget.ocean_surface=true;
    if(planet::valid_settings(budget)) return 19;
    budget.cells_per_tile=64;budget.stream_lod=false;budget.ocean_surface=false;
    if(!planet::valid_settings(budget)) return 20;
    budget.ocean_surface=true;
    if(!planet::valid_settings(budget)) return 21; // Real static wet geometry fits its explicit4MiB byte envelope.
    for(std::uint32_t body=1;body<=5;++body)
    {
        auto wet=planet::Settings{};wet.ocean_surface=true;wet.body_index=body;
        if(planet::valid_settings(wet)||!planet::encode_settings(wet).empty()) return 22;
        wet.mode="hemisphere";
        if(planet::valid_settings(wet)) return 23;
        wet.mode="universe";
        if(!planet::valid_settings(wet)) return 24;
    }
    for(const auto mode:planet::modes)
        for(std::uint32_t body=0;body<=5;++body)
        {
            auto settings=planet::Settings{};settings.mode=mode;settings.body_index=body;
            const bool expected=mode=="universe"||(body==0u&&(mode=="planet"||mode=="hemisphere"));
            if(planet::supports_ocean_surface(settings)!=expected) return 25;
            settings.ocean_surface=true;
            if(planet::valid_settings(settings)!=expected) return 26;
        }
    auto unknown=planet::Settings{};unknown.mode="unknown";
    if(planet::supports_ocean_surface(unknown)) return 27;
    unknown.mode="universe";unknown.body_index=99u;
    if(planet::supports_ocean_surface(unknown)) return 28;
    auto oldWet=planet::Settings{};oldWet.ocean_surface=true;oldWet.maximum_leaves=64;
    if(!planet::valid_settings(oldWet))return 29;
    const auto oldWetBytes=planet::encode_settings(oldWet);
    if(!oldWetBytes.starts_with("epoch.planet.recipe.v3\n")||oldWetBytes.find("snapshot_megabytes=")!=std::string::npos)return 30;
    if(planet::decode_settings(oldWetBytes)!=oldWet)return 31;
    auto detailed=planet::Settings{};detailed.ocean_surface=true;detailed.maximum_level=18;
    detailed.maximum_leaves=384;detailed.resident_megabytes=128;detailed.snapshot_megabytes=32;
    if(!planet::valid_settings(detailed))return 32;
    const auto v4=planet::encode_settings(detailed);
    if(!v4.starts_with("epoch.planet.recipe.v4\n")||!v4.ends_with("snapshot_megabytes=32\nocean_surface=1\n")
        ||planet::decode_settings(v4)!=detailed)return 33;
    if(planet::decode_settings(v4+"snapshot_megabytes=32\n"))return 34;
    auto malformed=v4;malformed.erase(malformed.find("snapshot_megabytes="),22u);
    if(planet::decode_settings(malformed))return 35;
    malformed=v4;malformed.replace(malformed.find("snapshot_megabytes=32"),21u,"snapshot_megabytes=0");
    if(planet::decode_settings(malformed))return 36;
    auto oldHeader=v4;oldHeader.replace(0u,21u,"epoch.planet.recipe.v3");
    if(planet::decode_settings(oldHeader))return 37;
    const auto limits=planet::artifact_limits(detailed);
    const auto footprint=planet::preview_memory_footprint(detailed);
    if(limits.maximum_bytes!=32ull*1024*1024||limits.maximum_vertices<=65536u||limits.maximum_indices<=196608u
        ||footprint.encoded_upper_bytes>limits.maximum_bytes||footprint.encoded_upper_bytes<=4ull*1024*1024
        ||footprint.decode_transient_upper_bytes!=4u*limits.maximum_bytes
        ||footprint.replacement_upper_bytes<footprint.transition_retained_upper_bytes+footprint.transition_transient_upper_bytes
        ||footprint.cpu_upper_bytes!=128ull*1024*1024+footprint.replacement_upper_bytes)return 38;
    const auto transition=planet::snapshot_transition_limits(detailed);
    if(transition.maximum_retained_bytes!=footprint.transition_retained_upper_bytes
        ||transition.maximum_transient_bytes!=footprint.transition_transient_upper_bytes
        ||transition.artifact.maximum_bytes!=limits.maximum_bytes)return 99;
    detailed.snapshot_megabytes=4;if(planet::valid_settings(detailed))return 39;
    detailed.snapshot_megabytes=65;if(planet::valid_settings(detailed)||planet::artifact_limits(detailed).maximum_bytes!=0u)return 40;
    detailed.snapshot_megabytes=32;detailed.maximum_leaves=std::numeric_limits<std::uint32_t>::max();
    if(planet::valid_settings(detailed)||planet::preview_memory_footprint(detailed).cpu_upper_bytes!=0u)return 41;
    detailed.maximum_leaves=512;detailed.cells_per_tile=64;detailed.snapshot_megabytes=64;
    if(planet::valid_settings(detailed))return 42;
    auto mobile=planet::Settings{};mobile.cells_per_tile=4;mobile.maximum_leaves=6;mobile.resident_megabytes=4;mobile.snapshot_megabytes=1;
    if(!planet::valid_settings(mobile)||!planet::encode_settings(mobile).starts_with("epoch.planet.recipe.v4\n"))return 43;
    const auto oldDry32=planet::Settings{.cells_per_tile=32};
    if(!planet::valid_settings(oldDry32)||!planet::encode_settings(oldDry32).starts_with("epoch.planet.recipe.v2\n"))return 44;
    const auto scene=planet::scene_mesh_budget(planet::Settings{});
    if(scene.artifact.maximum_bytes!=4ull*1024*1024||scene.cache_bytes!=20ull*1024*1024
        ||scene.frame_bytes!=scene.cache_bytes||scene.frame_indices<=196608u)return 45;
    auto maximum=planet::Settings{};maximum.snapshot_megabytes=64;
    const auto largest=planet::scene_mesh_budget(maximum);
    if(largest.frame_bytes!=64ull*1024*1024||largest.cache_bytes!=64ull*1024*1024
        ||largest.frame_indices!=6291456u)return 46;
    maximum.snapshot_megabytes=std::numeric_limits<std::uint32_t>::max();
    const auto refused=planet::scene_mesh_budget(maximum);
    if(refused.artifact.maximum_bytes!=0u||refused.frame_bytes!=16ull*1024*1024)return 47;
    const auto downgraded=planet::preview_memory_footprint(planet::Settings{},32ull*1024*1024);
    if(downgraded.decode_transient_upper_bytes!=16ull*1024*1024
        ||downgraded.replacement_upper_bytes<downgraded.transition_retained_upper_bytes+downgraded.transition_transient_upper_bytes+28ull*1024*1024
        ||downgraded.cpu_upper_bytes!=16ull*1024*1024+downgraded.replacement_upper_bytes)return 48;
    if(planet::preview_memory_footprint(planet::Settings{},65ull*1024*1024).cpu_upper_bytes!=0u
        ||planet::preview_memory_footprint(planet::Settings{},std::numeric_limits<std::uint64_t>::max()).cpu_upper_bytes!=0u)return 49;
    const auto smaller=planet::preview_memory_footprint(planet::Settings{},1024u);
    if(smaller.replacement_upper_bytes<smaller.transition_retained_upper_bytes+smaller.transition_transient_upper_bytes)return 50;
    for(bool ocean:{false,true})
    {
        planet::Settings authored;authored.seed=987u;authored.ocean_surface=ocean;
        authored.tile_span_meters=256.0;authored.tiles_per_axis=3u;
        const auto before=authored;
        const auto fine=planet::with_detail_preset(authored,planet::DetailPreset::detailed);
        if(!fine||authored!=before||fine->seed!=987u||fine->ocean_surface!=ocean
            ||fine->tile_span_meters!=256.0||fine->tiles_per_axis!=3u
            ||fine->cells_per_tile!=16u||fine->maximum_level!=20u||fine->maximum_leaves!=384u
            ||fine->resident_megabytes!=128u||fine->snapshot_megabytes!=32u||!fine->stream_lod)return 51;
        const auto fineText=planet::encode_settings(*fine);
        if(!fineText.starts_with("epoch.planet.recipe.v5\n")||planet::decode_settings(fineText)!=fine)return 52;
        const auto planned=planet::preview_memory_footprint(*fine,64ull*1024u*1024u);
        if(planned.cpu_upper_bytes<320ull*1024u*1024u||planned.cpu_upper_bytes>=1024ull*1024u*1024u)return 53;
        const auto compact=planet::with_detail_preset(*fine,planet::DetailPreset::compact);
        if(!compact||compact->maximum_level!=10u||compact->maximum_leaves!=48u
            ||compact->resident_megabytes!=16u||compact->snapshot_megabytes!=4u
            ||compact->seed!=987u||compact->ocean_surface!=ocean)return 54;
        auto invalidManual=*fine;invalidManual.snapshot_megabytes=4u;
        if(planet::valid_settings(invalidManual)||!planet::encode_settings(invalidManual).empty())return 55;
        if(planet::with_detail_preset(authored,static_cast<planet::DetailPreset>(99)))return 56;
    }
    auto flatPreset=planet::Settings{};flatPreset.mode="classic";
    if(planet::with_detail_preset(flatPreset,planet::DetailPreset::detailed))return 57;
    for(const auto coverage:{"planet","universe"})
    {
        auto authored=planet::Settings{};authored.mode=coverage;authored.ocean_surface=true;
        const auto original=authored;
        if(!planet::supports_exploration(authored))return 58;
        const auto profile=planet::with_detail_preset(authored,planet::DetailPreset::detailed);
        if(!profile||profile->mode!=coverage||profile->maximum_leaves!=384u
            ||profile->resident_megabytes!=128u||profile->snapshot_megabytes!=32u)return 59;
        for(std::uint32_t body=0;body<6;++body)
        {
            const auto active=planet::active_body_stream_settings(*profile,body);
            if(!active||active->mode!="planet"||active->body_index!=body
                ||active->ocean_surface!=(body==0u)||active->maximum_leaves!=384u
                ||active->snapshot_megabytes!=32u)return 60;
        }
        if(authored!=original||planet::active_body_stream_settings(authored,6u))return 61;
        const auto encoded=planet::encode_settings(*profile);
        if(planet::decode_settings(encoded)!=profile)return 62;
    }
    auto universe=planet::Settings{};universe.mode="universe";universe.cells_per_tile=32u;universe.maximum_leaves=64u;
    if(!planet::valid_settings(universe)||planet::active_body_stream_settings(universe,0u))return 63;
    // Valid coarse coverage does not grant an oversized active-body LOD mesh.
    if(planet::supports_exploration(flatPreset)||planet::active_body_stream_settings(flatPreset,0u))return 64;
    for(bool ocean:{false,true})
    {
        auto base=planet::Settings{};base.ocean_surface=ocean;
        const auto fine=planet::with_detail_preset(base,planet::DetailPreset::fine);
        if(!fine||fine->maximum_level!=20u||fine->maximum_leaves!=576u
            ||fine->patch_pixels!=170.0||fine->lod_hysteresis!=0.15
            ||fine->resident_megabytes!=192u||fine->snapshot_megabytes!=32u)return 65;
        const auto bytes=planet::encode_settings(*fine);
        if(!bytes.starts_with("epoch.planet.recipe.v5\n")||planet::decode_settings(bytes)!=fine)return 66;
        if(!planet::requires_lod_controls(*fine)||planet::requires_lod_controls(base))return 67;
        auto forged=bytes;forged.replace(0u,22u,"epoch.planet.recipe.v4\n");
        forged.erase(forged.find("patch_pixels="));
        if(planet::decode_settings(forged))return 68;
        if(planet::decode_settings(bytes+"patch_pixels=210\n"))return 69;
        auto missing=bytes;missing.erase(missing.find("lod_hysteresis="));
        if(planet::decode_settings(missing))return 70;
        const auto fast=planet::with_detail_preset(base,planet::DetailPreset::fast);
        if(!fast||fast->maximum_level!=17u||fast->maximum_leaves!=288u
            ||fast->patch_pixels!=260.0||fast->lod_hysteresis!=0.18)return 71;
        if(planet::decode_settings(planet::encode_settings(*fast))!=fast)return 72;
        const auto compact=planet::with_detail_preset(*fine,planet::DetailPreset::compact);
        if(!compact||compact->patch_pixels!=210.0||compact->lod_hysteresis!=0.15)return 73;
        const auto footprint=planet::preview_memory_footprint(*fine,64ull*1024u*1024u);
        if(footprint.encoded_upper_bytes>planet::artifact_limits(*fine).maximum_bytes
            ||footprint.cpu_upper_bytes<384ull*1024u*1024u||footprint.cpu_upper_bytes>=1024ull*1024u*1024u)return 74;
        auto tooSmall=*fine;tooSmall.snapshot_megabytes=4u;
        if(planet::valid_settings(tooSmall))return 75;
    }
    auto boundary=planet::Settings{};boundary.cells_per_tile=2;boundary.maximum_level=20;boundary.maximum_leaves=2048;
    boundary.patch_pixels=32.0;boundary.lod_hysteresis=0.0;
    if(!planet::valid_settings(boundary)||planet::decode_settings(planet::encode_settings(boundary))!=boundary)return 76;
    for(double pixels:{31.0,4097.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {auto bad=boundary;bad.patch_pixels=pixels;if(planet::valid_settings(bad)||!planet::encode_settings(bad).empty())return 77;}
    for(double stability:{-0.01,0.46,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    {auto bad=boundary;bad.lod_hysteresis=stability;if(planet::valid_settings(bad))return 78;}
    boundary.maximum_level=21;if(planet::valid_settings(boundary))return 79;
    boundary.maximum_level=20;boundary.maximum_leaves=2049;if(planet::valid_settings(boundary))return 80;
    // Physical size is authoring identity, independent of selected body and
    // near-camera detail. Legacy/default recipes retain their exact bytes.
    for(const auto radius:{250.0,25000.0,1737000.0,6371000.0,100000000.0})
    for(std::uint32_t reference=0u;reference<6u;++reference)
    {
        planet::Settings sized;sized.size_reference_body=reference;sized.reference_radius_meters=radius;
        const auto text=planet::encode_settings(sized);
        if(!planet::valid_settings(sized)||planet::decode_settings(text)!=sized)return 81;
        const bool custom=reference!=0u||radius!=6371000.0;
        if(planet::requires_size_controls(sized)!=custom
            ||text.starts_with("epoch.planet.recipe.v6\n")!=custom)return 82;
        for(std::uint32_t body=0u;body<6u;++body)
        {
            const auto active=planet::active_body_stream_settings(sized,body);
            if(!active||active->size_reference_body!=reference||active->reference_radius_meters!=radius)return 83;
        }
        for(const auto preset:{planet::DetailPreset::compact,planet::DetailPreset::fine})
        {
            const auto detailed=planet::with_detail_preset(sized,preset);
            if(!detailed||detailed->size_reference_body!=reference||detailed->reference_radius_meters!=radius)return 84;
        }
        if(custom)
        {
            auto missing=text;missing.erase(missing.find("reference_radius_meters="));
            if(planet::decode_settings(missing)||planet::decode_settings(text+"size_reference_body=1\n"))return 85;
            auto old=text;old.replace(0u,22u,"epoch.planet.recipe.v5\n");
            if(planet::decode_settings(old))return 86;
        }
    }
    for(const auto radius:{249.999,100000000.001,-1.0,std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()})
    {planet::Settings bad;bad.reference_radius_meters=radius;if(planet::valid_settings(bad)||!planet::encode_settings(bad).empty())return 87;}
    planet::Settings badReference;badReference.size_reference_body=6u;
    if(planet::valid_settings(badReference)||!planet::encode_settings(badReference).empty())return 88;
    const std::string sizeGolden="epoch.planet.recipe.v6\nmode=planet\nseed=1337\ncells_per_tile=16\ntiles_per_axis=4\nbody_index=0\ntile_span_meters=128\nstream_lod=1\nmaximum_level=10\nmaximum_leaves=48\nresident_megabytes=16\nsnapshot_megabytes=4\nocean_surface=0\npatch_pixels=210\nlod_hysteresis=0.14999999999999999\nsize_reference_body=0\nreference_radius_meters=25000\n";
    planet::Settings legacySize;legacySize.reference_radius_meters=25000;
    if(planet::encode_settings(legacySize)!=sizeGolden||planet::decode_settings(sizeGolden)!=legacySize)return 89;
    auto solar=legacySize;solar.original_sunlight=true;
    auto solarGolden=sizeGolden;solarGolden.replace(0,solarGolden.find('\n')+1u,"epoch.planet.recipe.v7\n");solarGolden+="original_sunlight=1\n";
    if(planet::encode_settings(solar)!=solarGolden||planet::decode_settings(solarGolden)!=solar)return 90;
    if(planet::decode_settings(solarGolden+"original_sunlight=1\n"))return 91;
    auto noFlag=solarGolden;noFlag.erase(noFlag.find("original_sunlight="));if(planet::decode_settings(noFlag))return 92;
    auto invalidFlag=solarGolden;invalidFlag.replace(invalidFlag.find("original_sunlight=1"),19,"original_sunlight=0");
    if(planet::decode_settings(invalidFlag))return 93;
    solar.mode="classic";if(planet::valid_settings(solar))return 94;
    return 0;
}
