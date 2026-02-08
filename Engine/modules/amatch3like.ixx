module;

export module amatch3like;

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
import <algorithm>;
import <array>;
import <cstdint>;
import <span>;
import <stdexcept>;
import <string>;
import <string_view>;
import <tuple>;
import <unordered_map>;
import <utility>;
import <vector>;

export namespace almondnamespace::match3like
{
    struct Match3LikeScene : public scene::Scene
    {
        Match3LikeScene(logger::Logger* L = nullptr, timing::Timer* C = nullptr)
            : Scene(L, C) {}

        void load() override
        {
            Scene::load();
            setupSprites();
            initializeGrid();
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

            const float width = float(ctx->get_width_safe());
            const float height = float(ctx->get_height_safe());
            const float cellSize = std::min(width / float(kCols), height / float(kRows));
            const float offsetX = (width - cellSize * float(kCols)) * 0.5f;
            const float offsetY = (height - cellSize * float(kRows)) * 0.5f;
            constexpr std::array<std::string_view, 6> gemNames = {
                "gem0", "gem1", "gem2", "gem3", "gem4", "gem5"
            };

            for (int row = 0; row < kRows; ++row)
            {
                for (int col = 0; col < kCols; ++col)
                {
                    const int gemIndex = grid[static_cast<size_t>(row * kCols + col)];
                    if (gemIndex < 0 || gemIndex >= static_cast<int>(gemNames.size()))
                        continue;

                    auto it = sprites.find(std::string(gemNames[static_cast<size_t>(gemIndex)]));
                    if (it == sprites.end() || !spritepool::is_alive(it->second))
                        continue;

                    ctx->draw_sprite_safe(it->second, atlasSpan,
                        offsetX + cellSize * float(col),
                        offsetY + cellSize * float(row),
                        cellSize, cellSize);
                }
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
                .name = "amatch3like_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("amatch3like_atlas");
            if (!registrar)
                throw std::runtime_error("[Match3Like] Missing atlas registrar");

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

                auto img = a_loadImage("assets/games/amatch3like/" + name + ".ppm", false);
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
            ensureSprite("gem0");
            ensureSprite("gem1");
            ensureSprite("gem2");
            ensureSprite("gem3");
            ensureSprite("gem4");
            ensureSprite("gem5");

            if (createdAtlas || registered)
            {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        std::unordered_map<std::string, SpriteHandle> sprites{};
        static constexpr int kRows = 8;
        static constexpr int kCols = 8;
        std::vector<int> grid{};

        void initializeGrid()
        {
            grid.resize(static_cast<size_t>(kRows * kCols));
            for (int row = 0; row < kRows; ++row)
            {
                for (int col = 0; col < kCols; ++col)
                {
                    grid[static_cast<size_t>(row * kCols + col)] = (row + col) % 6;
                }
            }
        }
    };

    export bool run_match3like(std::shared_ptr<core::Context> ctx)
    {
        Match3LikeScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;

        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace almondnamespace::match3like
