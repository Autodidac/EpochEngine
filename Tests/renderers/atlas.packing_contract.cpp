// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <utility>
#include <random>
namespace epochengine {
struct Texture { std::uint32_t width{}, height{}; std::vector<std::uint8_t> pixels; };
namespace logger { template<class... T> void warnf_loc(T&&...) {} }
}
ATLAS_PRODUCTION
#define VERIFY(x) do { if (!(x)) return __LINE__; } while(false)
using namespace epochengine;
int main() {
    std::mt19937 random(71923);
    for (const u32 width : {1u, 2u, 63u, 64u, 65u, 127u, 128u, 129u, 257u}) {
        const u32 height = 71;
        auto atlas = TextureAtlas::create({"test", width, height});
        std::vector<std::vector<bool>> old(height, std::vector<bool>(width));
        std::vector<u8> expected(static_cast<size_t>(width)*height*4u);
        auto original = [&](u32 w, u32 h) -> std::optional<std::pair<u32,u32>> {
            if (!w || !h || w > width || h > height) return std::nullopt;
            for (u32 y=0; y+h<=height; ++y)
                for (u32 x=0; x+w<=width; ++x) {
                    bool fits=true;
                    for (u32 dy=0; dy<h && fits; ++dy)
                        for (u32 dx=0; dx<w; ++dx)
                            if (old[y+dy][x+dx]) { fits=false; break; }
                    if (fits) return std::pair{x,y};
                }
            return std::nullopt;
        };
        u64 accepted=0;
        for (u32 attempt=0; attempt<300; ++attempt) {
            const u32 w = attempt < 4 ? std::min(width, 63u + attempt) : 1u + random()%(width+2u);
            const u32 h = 1u + random()%19u;
            const auto reference=original(w,h);
            VERIFY(atlas->can_fit(w,h)==reference.has_value());
            Texture texture{w,h,std::vector<u8>(static_cast<size_t>(w)*h*4u, static_cast<u8>(attempt))};
            const auto entry=atlas->add_entry(std::to_string(attempt),texture);
            VERIFY(entry.has_value()==reference.has_value());
            if (!entry) continue;
            VERIFY(entry->region.x==reference->first && entry->region.y==reference->second);
            VERIFY(entry->index==static_cast<int>(accepted));
            VERIFY(entry->pixels==texture.pixels);
            ++accepted;
            for(u32 dy=0;dy<h;++dy) for(u32 dx=0;dx<w;++dx) {
                old[reference->second+dy][reference->first+dx]=true;
                for(u32 c=0;c<4;++c) expected[((reference->second+dy)*width+reference->first+dx)*4+c]=static_cast<u8>(attempt);
            }
            VERIFY(atlas->pixel_data==expected);
            VERIFY(atlas->current_version()==accepted);
            VERIFY(!atlas->add_entry(std::to_string(attempt),texture));
        }
        const auto before=atlas->snapshot_pixels();
        atlas->rebuild_pixels();
        VERIFY(atlas->snapshot_pixels().pixels==before.pixels);
        VERIFY(!atlas->can_fit(0,1) && !atlas->can_fit(width+1,1));
        atlas->init({"reset",width,height});
        VERIFY(atlas->can_fit(width,height));
        VERIFY(atlas->entries.empty() && atlas->current_version()==0);
    }
    // Wide GUI-like controls exercise whole words and partial trailing words.
    auto large=TextureAtlas::create({"gui",2048,2048});
    Texture control{257,33,std::vector<u8>(257*33*4,255)};
    for(u32 i=0;i<350;++i) {
        const auto entry=large->add_entry(std::to_string(i),control);
        VERIFY(entry && entry->region.x==(i%7)*257 && entry->region.y==(i/7)*33);
    }
    return 0;
}
