/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

export module game.frogger_like;

// Epoch engine / project modules
import core.context;      // core::Context
import context.window;    // core::WindowData
import input.engine;             // input::Key
import atlas.manager;            // atlasmanager
import atlas.texture;            // TextureAtlas
import image.loader;             // a_loadImage
import sprite.handle;             // SpriteHandle
import sprite.pool;              // spritepool
import scene.core;                    // scene::Scene
import core.logger;       // logger::Logger
import core.timer;         // timing::Timer

// C++ std

namespace epochengine::froggerlike
{
    export struct FroggerLikeScene : public scene::Scene
    {
        FroggerLikeScene(logger::Logger* L = nullptr, timing::Timer* C = nullptr)
            : Scene(L, C) {}

        void load() override
        {
            Scene::load();
            setupSprites();
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override
        {
            if (!ctx) return false;

            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            ctx->clear_safe();

            auto atlasVec = atlasmanager::get_atlas_vector_snapshot(); // by value
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            if (auto it = sprites.find("bg"); it != sprites.end() && spritepool::is_alive(it->second))
            {
                ctx->draw_sprite_safe(it->second, atlasSpan, 0.0f, 0.0f,
                    float(ctx->get_width_safe()), float(ctx->get_height_safe()));
            }

            const float width = float((std::max)(1, ctx->get_width_safe()));
            const float height = float((std::max)(1, ctx->get_height_safe()));
            const float laneHeight = height / 5.0f;
            const float spriteSize = (std::min)(width / 6.0f, laneHeight);

            auto drawRow = [&](std::string_view name, int lane, int count, float offset)
                {
                    auto it = sprites.find(std::string(name));
                    if (it == sprites.end() || !spritepool::is_alive(it->second))
                        return;

                    const float y = laneHeight * lane + (laneHeight - spriteSize) * 0.5f;
                    for (int i = 0; i < count; ++i)
                    {
                        const float x = std::fmod(offset + i * (spriteSize * 1.5f), width);
                        ctx->draw_sprite_safe(it->second, atlasSpan, x, y, spriteSize, spriteSize);
                    }
                };

            drawRow("water", 0, 3, 0.0f);
            drawRow("log", 1, 3, spriteSize * 0.5f);
            drawRow("car", 3, 3, spriteSize);

            if (auto it = sprites.find("frog"); it != sprites.end() && spritepool::is_alive(it->second))
            {
                const float frogY = laneHeight * 4 + (laneHeight - spriteSize) * 0.5f;
                ctx->draw_sprite_safe(it->second, atlasSpan, (width - spriteSize) * 0.5f, frogY, spriteSize, spriteSize);
            }

            ctx->present_safe();
            return true;
        }

        void unload() override
        {
            Scene::unload();
            sprites.clear();
        }

    private:
        void setupSprites()
        {
            sprites.clear();

            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "afroggerlike_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("afroggerlike_atlas");
            if (!registrar)
                throw std::runtime_error("[FroggerLike] Missing atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registered = false;

            auto ensureSprite = [&](std::string_view id)
            {
                std::string name(id);

                if (auto existing = atlasmanager::registry.get(name))
                {
                    auto handle = std::get<0>(*existing);
                    if (spritepool::is_alive(handle))
                    {
                        sprites[name] = handle;
                        return;
                    }
                }

                auto img = a_loadImage("assets/games/afroggerlike/" + name + ".ppm", false);
                if (img.pixels.empty())
                    return; // optional assets

                auto handleOpt = registrar->register_atlas_sprites_by_image(
                    name, img.pixels, img.width, img.height, atlas);

                if (handleOpt && spritepool::is_alive(*handleOpt))
                {
                    sprites[name] = *handleOpt;
                    registered = true;
                }
            };

            // Always try to load a background sprite if available
            ensureSprite("bg");
            ensureSprite("frog");
            ensureSprite("car");
            ensureSprite("log");
            ensureSprite("water");

            if (createdAtlas || registered)
            {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        std::unordered_map<std::string, SpriteHandle> sprites{};
    };

    export bool run_froggerlike(std::shared_ptr<core::Context> ctx)
    {
        FroggerLikeScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;

        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace epochengine::froggerlike
