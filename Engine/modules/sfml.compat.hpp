#pragma once

#include <cstdint>
#include <memory>
#include <string>

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

namespace epoch::sfml_compat
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
