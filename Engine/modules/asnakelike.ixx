module;

export module asnakelike;

// Almond engine / project modules
import aengine.core.context;      // core::Context
import aengine.context.window;    // core::WindowData
import aengine.input;             // input::Key
import aatlas.manager;            // atlasmanager
import aimage.loader;             // a_loadImage
import aspritehandle;             // SpriteHandle
import asprite.pool;              // spritepool
import ascene;                    // scene::Scene
import aengine.core.logger;       // logger::Logger
import aengine.core.time;         // timing::Timer

// C++ std
import <cstdint>;
import <span>;
import <stdexcept>;
import <string>;
import <string_view>;
import <tuple>;
import <unordered_map>;
import <utility>;
import <vector>;

export namespace almondnamespace::snakelike
{
    struct SnakeLikeScene : public scene::Scene
    {
        SnakeLikeScene(logger::Logger* L = nullptr, timing::Timer* C = nullptr)
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

            // Example: draw head as a sanity check (or bg if you have it)
            if (auto it = sprites.find("head"); it != sprites.end() && spritepool::is_alive(it->second))
            {
                auto atlasVec = atlasmanager::get_atlas_vector_snapshot(); // by value
                std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());


                ctx->draw_sprite_safe(
                    it->second,
                    atlasSpan,
                    0.0f, 0.0f,
                    float(ctx->get_width_safe()),
                    float(ctx->get_height_safe()));
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
                .name = "asnakelike_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("asnakelike_atlas");
            if (!registrar)
                throw std::runtime_error("[SnakeLike] Missing atlas registrar");

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

                auto img = a_loadImage("assets/games/asnakelike/" + name + ".ppm", false);
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
            //ensureSprite("bg");
            ensureSprite("head");
            ensureSprite("body");
            ensureSprite("food");

            if (createdAtlas || registered)
            {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        std::unordered_map<std::string, SpriteHandle> sprites{};
    };

    export bool run_snakelike(std::shared_ptr<almondnamespace::core::Context> ctx)
    {
        SnakeLikeScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;

        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace almondnamespace::snakelike
