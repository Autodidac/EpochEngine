module;

export module apacmanlike;

// ------------------------------------------------------------
// Almond engine / project modules
// ------------------------------------------------------------
import aengine.core.context;      // core::Context
import aengine.context.window;    // core::WindowData
import aengine.input;             // input::Key
import agamecore;                 // grid helpers
import aatlas.manager;            // atlas manager + registry
import aimage.loader;             // a_loadImage / ImageData
import aspritehandle;             // SpriteHandle
import asprite.pool;              // spritepool
import ascene;                    // scene::Scene

// ------------------------------------------------------------
// Engine feature modules
// ------------------------------------------------------------
import aengine.core.time;         // timing::Timer
import aengine.core.logger;       // logger::Logger

// ------------------------------------------------------------
// C++ standard library modules
// ------------------------------------------------------------
import <algorithm>;
import <cstddef>;
import <deque>;
import <memory>;
import <span>;
import <stdexcept>;
import <string>;
import <string_view>;
import <tuple>;
import <utility>;

export namespace almondnamespace::pacmanlike
{
    inline constexpr int GRID_W = 28;
    inline constexpr int GRID_H = 31;

    enum Tile : int { EMPTY, WALL, PELLET };

    struct GameState
    {
        gamecore::grid_t<int>  map;
        gamecore::grid_t<bool> pellet;

        int px = 14, py = 23;
        std::deque<std::pair<int, int>> ghosts;

        GameState()
            : map(gamecore::make_grid<int>(GRID_W, GRID_H, EMPTY))
            , pellet(gamecore::make_grid<bool>(GRID_W, GRID_H, true))
        {
            for (int y = 0; y < GRID_H; ++y)
            {
                for (int x = 0; x < GRID_W; ++x)
                {
                    if (x == 0 || x == GRID_W - 1 || y == 0 || y == GRID_H - 1)
                    {
                        map[gamecore::idx(GRID_W, x, y)] = WALL;
                        pellet[gamecore::idx(GRID_W, x, y)] = false;
                    }
                }
            }

            ghosts = { {13, 14}, {14, 14}, {13, 15}, {14, 15} };
            for (auto [gx, gy] : ghosts)
                pellet[gamecore::idx(GRID_W, gx, gy)] = false;
        }

        bool pellets_remaining() const
        {
            for (std::size_t i = 0; i < pellet.size(); ++i)
                if (pellet[i]) return true;
            return false;
        }
    };

    export struct PacmanLikeScene final : public scene::Scene
    {
        PacmanLikeScene(logger::Logger* L = nullptr, timing::Timer* C = nullptr)
            : Scene(L, C)
        {
        }

        void load() override
        {
            Scene::load();
            setupSprites();
            state = {};
            won = false;
        }

        bool frame(std::shared_ptr<core::Context> ctx, core::WindowData*) override
        {
            if (!ctx) return false;

            if (ctx->is_key_down_safe(input::Key::Escape))
                return false;

            int nx = state.px;
            int ny = state.py;

            if (ctx->is_key_down_safe(input::Key::Left))  --nx;
            if (ctx->is_key_down_safe(input::Key::Right)) ++nx;
            if (ctx->is_key_down_safe(input::Key::Up))    --ny;
            if (ctx->is_key_down_safe(input::Key::Down))  ++ny;

            if (gamecore::in_bounds(GRID_W, GRID_H, nx, ny) &&
                state.map[gamecore::idx(GRID_W, nx, ny)] != WALL)
            {
                state.px = nx;
                state.py = ny;

                const auto i = gamecore::idx(GRID_W, nx, ny);
                if (state.pellet[i])
                {
                    state.pellet[i] = false;
                    if (!state.pellets_remaining())
                        won = true;
                }
            }

            // Your Context::clear_safe() takes no args (per the error log).
            ctx->clear_safe();

            auto atlasVec = atlasmanager::get_atlas_vector_snapshot(); // by value
            std::span<const TextureAtlas* const> atlasSpan(atlasVec.data(), atlasVec.size());

            const float cw = float((std::max)(1, ctx->get_width_safe())) / float(GRID_W);
            const float ch = float((std::max)(1, ctx->get_height_safe())) / float(GRID_H);

            auto draw_grid = [&](SpriteHandle handle, auto&& pred)
                {
                    if (!spritepool::is_alive(handle)) return;

                    for (int y = 0; y < GRID_H; ++y)
                        for (int x = 0; x < GRID_W; ++x)
                            if (pred(x, y))
                                ctx->draw_sprite_safe(handle, atlasSpan, x * cw, y * ch, cw, ch);
                };

            draw_grid(pelletHandle, [&](int x, int y)
                {
                    return state.pellet[gamecore::idx(GRID_W, x, y)];
                });

            draw_grid(wallHandle, [&](int x, int y)
                {
                    return state.map[gamecore::idx(GRID_W, x, y)] == WALL;
                });

            if (spritepool::is_alive(pacmanHandle))
                ctx->draw_sprite_safe(pacmanHandle, atlasSpan, state.px * cw, state.py * ch, cw, ch);

            if (spritepool::is_alive(ghostHandle))
                for (auto [gx, gy] : state.ghosts)
                    ctx->draw_sprite_safe(ghostHandle, atlasSpan, gx * cw, gy * ch, cw, ch);

            ctx->present_safe();
            return !won;
        }

        void unload() override
        {
            Scene::unload();
            state = {};
            won = false;
        }

    private:
        void setupSprites()
        {
            const bool createdAtlas = atlasmanager::create_atlas({
                .name = "pacman_atlas",
                .width = 512,
                .height = 512,
                .generate_mipmaps = false
                });

            auto* registrar = atlasmanager::get_registrar("pacman_atlas");
            if (!registrar)
                throw std::runtime_error("[Pacman] Failed to get atlas registrar");

            TextureAtlas& atlas = registrar->atlas;
            bool registeredAny = false;

            auto ensureSprite = [&](std::string_view name, SpriteHandle& outHandle)
                {
                    // Reuse cached sprite if it already exists and is alive.
                    if (auto existing = atlasmanager::registry.get(std::string(name)))
                    {
                        auto handle = std::get<0>(*existing);
                        if (spritepool::is_alive(handle))
                        {
                            outHandle = handle;
                            return;
                        }
                    }

                    auto img = a_loadImage("assets/games/apacmanlike/" + std::string(name) + ".ppm", false);
                    if (img.pixels.empty())
                        throw std::runtime_error("[Pacman] Failed to load image '" + std::string(name) + "'");

                    auto handleOpt = registrar->register_atlas_sprites_by_image(
                        std::string(name), img.pixels, img.width, img.height, atlas);

                    if (!handleOpt || !spritepool::is_alive(*handleOpt))
                        throw std::runtime_error("[Pacman] Failed to register sprite '" + std::string(name) + "'");

                    outHandle = *handleOpt;
                    registeredAny = true;
                };

            ensureSprite("pacman", pacmanHandle);
            ensureSprite("ghost", ghostHandle);
            ensureSprite("pellet", pelletHandle);
            ensureSprite("wall", wallHandle);

            if (createdAtlas || registeredAny)
            {
                atlas.rebuild_pixels();
                atlasmanager::ensure_uploaded(atlas);
            }
        }

        GameState state{};
        SpriteHandle pacmanHandle{};
        SpriteHandle ghostHandle{};
        SpriteHandle pelletHandle{};
        SpriteHandle wallHandle{};
        bool won = false;
    };

    export bool run_pacman(std::shared_ptr<core::Context> ctx)
    {
        PacmanLikeScene scene;
        scene.load();

        core::WindowData* window = nullptr;
        bool running = true;
        while (running && ctx)
            running = scene.frame(ctx, window);

        scene.unload();
        return running;
    }
} // namespace almondnamespace::pacman
