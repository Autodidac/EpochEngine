module;

export module aslidingpuzzlelike;

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
import <cstdint>;
import <span>;
import <stdexcept>;
import <string>;
import <string_view>;
import <tuple>;
import <unordered_map>;
import <utility>;
import <vector>;

export namespace almondnamespace::slidinglike
{
    struct SlidingPuzzleLikeScene : public scene::Scene
    {
        SlidingPuzzleLikeScene(logger::Logger* L = nullptr, timing::Timer* C = nullptr)
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

            for (int row = 0; row < kRows; ++row)
            {
                for (int col = 0; col < kCols; ++col)
                {
                    const int value = grid[static_cast<size_t>(row * kCols + col)];
                    const char* spriteName = (value == 0) ? "empty" : "tile";
                    auto it = sprites.find(spriteName);
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
                .name = "aslidingpuzzlelike_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false });

            auto* registrar = atlasmanager::get_registrar("aslidingpuzzlelike_atlas");
            if (!registrar)
                throw std::runtime_error("[SlidingPuzzleLike] Missing atlas registrar");

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

                auto img = a_loadImage("assets/games/aslidingpuzzlelike/" + name + ".ppm", false);
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
            ensureSprite("tile");
            ensureSprite("empty");

            if (createdAtlas || registered)
            {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        std::unordered_map<std::string, SpriteHandle> sprites{};
        static constexpr int kRows = 4;
        static constexpr int kCols = 4;
        std::vector<int> grid{};

        void initializeGrid()
        {
            grid.resize(static_cast<size_t>(kRows * kCols));
            int value = 1;
            for (int row = 0; row < kRows; ++row)
            {
                for (int col = 0; col < kCols; ++col)
                {
                    const size_t index = static_cast<size_t>(row * kCols + col);
                    if (index == grid.size() - 1)
                        grid[index] = 0;
                    else
                        grid[index] = value++;
                }
            }
        }
    };

    export bool run_slidingpuzzle(std::shared_ptr<core::Context> ctx)
    {
        SlidingPuzzleLikeScene scene;
        scene.load();

        auto* window = ctx ? ctx->windowData : nullptr;

        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace almondnamespace::slidingpuzzlelike
