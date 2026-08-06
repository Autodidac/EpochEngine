#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#if defined(EPOCH_USING_SFML) && (EPOCH_USING_SFML == 1)
#include <SFML/Config.hpp>
#include <SFML/Graphics.hpp>

#if defined(SFML_VERSION_MAJOR) && (SFML_VERSION_MAJOR >= 3)
#define EPOCH_SFML_HAS_V3_API 1
#include <SFML/Window/WindowEnums.hpp>
#else
#define EPOCH_SFML_HAS_V3_API 0
#include <SFML/Window/WindowStyle.hpp>
#endif

namespace epochengine::sfml_compat
{
    [[nodiscard]] inline sf::FloatRect float_rect(
        float left,
        float top,
        float width,
        float height) noexcept
    {
#if EPOCH_SFML_HAS_V3_API
        return sf::FloatRect(
            sf::Vector2f{ left, top },
            sf::Vector2f{ width, height });
#else
        return sf::FloatRect(left, top, width, height);
#endif
    }

    [[nodiscard]] inline sf::IntRect int_rect(
        int left,
        int top,
        int width,
        int height) noexcept
    {
#if EPOCH_SFML_HAS_V3_API
        return sf::IntRect(
            sf::Vector2i{ left, top },
            sf::Vector2i{ width, height });
#else
        return sf::IntRect(left, top, width, height);
#endif
    }

    [[nodiscard]] inline sf::VideoMode video_mode(
        unsigned int width,
        unsigned int height,
        unsigned int bits_per_pixel = 32u)
    {
#if EPOCH_SFML_HAS_V3_API
        return sf::VideoMode(sf::Vector2u{ width, height }, bits_per_pixel);
#else
        return sf::VideoMode(width, height, bits_per_pixel);
#endif
    }

    [[nodiscard]] inline bool resize_texture(
        sf::Texture& texture,
        unsigned int width,
        unsigned int height)
    {
#if EPOCH_SFML_HAS_V3_API
        return texture.resize(sf::Vector2u{ width, height });
#else
        return texture.create(width, height);
#endif
    }

    [[nodiscard]] inline bool resize_render_texture(
        sf::RenderTexture& target,
        unsigned int width,
        unsigned int height)
    {
#if EPOCH_SFML_HAS_V3_API
        return target.resize(sf::Vector2u{ width, height });
#else
        return target.create(width, height);
#endif
    }

    class ArcadePreviewSurface final
    {
    public:
        [[nodiscard]] bool ensure(
            sf::RenderWindow& owner,
            unsigned int width,
            unsigned int height)
        {
            if (m_target && m_width == width && m_height == height)
                return true;
            if (width == 0u || height == 0u)
                return false;

            reset(&owner);
            (void)owner.setActive(false);

            auto target = std::make_unique<sf::RenderTexture>();
            const bool ready = resize_render_texture(*target, width, height);
            (void)target->setActive(false);
            (void)owner.setActive(true);
            if (!ready)
                return false;

            m_target = std::move(target);
            m_width = width;
            m_height = height;
            m_frame = 0u;
            return true;
        }

        [[nodiscard]] bool begin_update(sf::RenderWindow& owner)
        {
            if (!m_target || m_updating)
                return false;

            (void)owner.setActive(false);
            if (!m_target->setActive(true))
            {
                (void)owner.setActive(true);
                return false;
            }

            m_target->clear(sf::Color(4u, 6u, 11u, 255u));
            m_updating = true;
            return true;
        }

        void fill(
            int x,
            int y,
            int width,
            int height,
            const std::array<float, 4>& color)
        {
            if (!m_target || !m_updating || width <= 0 || height <= 0)
                return;

            sf::RectangleShape rectangle{};
            rectangle.setPosition(sf::Vector2f(static_cast<float>(x), static_cast<float>(y)));
            rectangle.setSize(sf::Vector2f(static_cast<float>(width), static_cast<float>(height)));
            rectangle.setFillColor(sf::Color(
                to_channel(color[0]),
                to_channel(color[1]),
                to_channel(color[2]),
                to_channel(color[3])));
            m_target->draw(rectangle);
        }

        [[nodiscard]] bool end_update(sf::RenderWindow& owner)
        {
            if (!m_target || !m_updating)
                return false;

            m_target->display();
            (void)m_target->setActive(false);
            m_updating = false;
            ++m_frame;

            const bool owner_restored = owner.setActive(true);
            if (owner_restored)
                owner.resetGLStates();
            return owner_restored;
        }

        void reset(sf::RenderWindow* owner = nullptr) noexcept
        {
            if (owner)
                (void)owner->setActive(false);
            if (m_target)
                (void)m_target->setActive(false);

            m_target.reset();
            m_width = 0u;
            m_height = 0u;
            m_frame = 0u;
            m_updating = false;

            if (owner)
                (void)owner->setActive(true);
        }

        [[nodiscard]] const sf::Texture* texture() const noexcept
        {
            return m_target ? &m_target->getTexture() : nullptr;
        }

        [[nodiscard]] unsigned int width() const noexcept { return m_width; }
        [[nodiscard]] unsigned int height() const noexcept { return m_height; }
        [[nodiscard]] std::uint64_t frame_number() const noexcept { return m_frame; }

    private:
        [[nodiscard]] static std::uint8_t to_channel(float value) noexcept
        {
            if (value <= 0.0f)
                return 0u;
            if (value >= 1.0f)
                return 255u;
            return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
        }

        std::unique_ptr<sf::RenderTexture> m_target{};
        unsigned int m_width = 0u;
        unsigned int m_height = 0u;
        std::uint64_t m_frame = 0u;
        bool m_updating = false;
    };

    inline void resize_image(
        sf::Image& image,
        unsigned int width,
        unsigned int height,
        const std::uint8_t* pixels)
    {
#if EPOCH_SFML_HAS_V3_API
        image.resize(sf::Vector2u{ width, height }, pixels);
#else
        image.create(width, height, reinterpret_cast<const sf::Uint8*>(pixels));
#endif
    }

    [[nodiscard]] inline std::unique_ptr<sf::RenderWindow> make_render_window(
        const sf::VideoMode& mode,
        const std::string& title,
        const sf::ContextSettings& settings)
    {
#if EPOCH_SFML_HAS_V3_API
        return std::make_unique<sf::RenderWindow>(
            mode,
            title,
            sf::Style::Default,
            sf::State::Windowed,
            settings);
#else
        return std::make_unique<sf::RenderWindow>(
            mode,
            title,
            sf::Style::Default,
            settings);
#endif
    }

    [[nodiscard]] inline auto native_handle(sf::Window& window)
    {
#if EPOCH_SFML_HAS_V3_API
        return window.getNativeHandle();
#else
        return window.getSystemHandle();
#endif
    }
}

#endif
