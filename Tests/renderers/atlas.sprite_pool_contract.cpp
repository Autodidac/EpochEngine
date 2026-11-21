// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

import atlas.manager;
import atlas.texture;
import image.loader;
import sprite.handle;
import sprite.pool;
import sprite.registry;

int main(int argc, char** argv)
{
    using namespace epochengine;
    if (argc != 3) return 12;
    const auto image = a_loadImage(std::filesystem::path(argv[1]));
    if (image.width <= 0 || image.height <= 0 || image.channels != 4 ||
        image.pixels.size() != std::size_t(image.width) * image.height * 4) return 13;
    std::ifstream reference(argv[2], std::ios::binary);
    for (const auto channel : image.pixels)
        if (reference.get() != channel) return 14;
    if (!reference || reference.peek() != std::ifstream::traits_type::eof()) return 14;
    for (const auto& filename : {"lf.ppm", "cr.ppm", "crlf.ppm"}) {
        const auto probe = a_loadImage(filename);
        if (probe.width != 1 || probe.height != 1 || probe.channels != 4 ||
            probe.pixels != std::vector<std::uint8_t>{10, 32, 9, 255}) return 15;
    }
    // Follow the standalone game's first-use path in a fresh process.
    const auto width = static_cast<std::uint32_t>(image.width);
    const auto height = static_cast<std::uint32_t>(image.height);
    if (!atlasmanager::create_atlas({.name = "pacman_atlas", .width = width, .height = height}))
        return 1;
    auto* registrar = atlasmanager::get_registrar("pacman_atlas");
    if (!registrar) return 2;
    auto& atlas = registrar->atlas;
    const auto first = registrar->register_atlas_sprites_by_image("pacman", image.pixels, image.width, image.height, atlas);
    if (!first || !first->is_valid() || !spritepool::is_alive(*first)) return 3;
    const auto registered = atlasmanager::registry.get("pacman");
    if (!registered || std::get<0>(*registered) != *first) return 4;
    AtlasRegion region{};
    std::string name;
    if (first->atlasIndex != static_cast<std::uint32_t>(atlas.get_index()) ||
        !atlas.try_get_entry_info(static_cast<int>(first->localIndex), region, &name) ||
        name != "pacman" || region.width != width || region.height != height) return 5;
    if (registrar->register_atlas_sprites_by_image("pacman", image.pixels, image.width, image.height, atlas) != first ||
        atlas.entry_count() != 1 || atlas.snapshot_pixels().pixels != image.pixels) return 6;
    atlasmanager::registry.remove("pacman");
    spritepool::free(*first);

    std::array<SpriteHandle, 3> slots;
    spritepool::initialize(slots.size());
    for (auto& slot : slots) {
        slot = spritepool::allocate();
        if (!slot.is_valid() || !spritepool::is_alive(slot)) return 7;
    }
    if (slots[0].id == slots[1].id || slots[0].id == slots[2].id || slots[1].id == slots[2].id ||
        spritepool::allocate().is_valid()) return 8;
    spritepool::free(slots[1]);
    const auto replacement = spritepool::allocate();
    if (!spritepool::is_alive(replacement) || replacement.id != slots[1].id ||
        replacement.generation == slots[1].generation || spritepool::is_alive(slots[1])) return 9;
    if (!spritepool::is_alive(slots[0]) || !spritepool::is_alive(slots[2]) ||
        spritepool::allocate().is_valid()) return 10;
    spritepool::clear();
    if (spritepool::allocate().is_valid() || spritepool::is_alive(replacement)) return 11;
}
