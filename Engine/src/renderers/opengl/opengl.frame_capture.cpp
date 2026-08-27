#include "opengl.frame_capture.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <include/engine.config.hpp>

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
#include <glad/glad.h>
#endif

extern "C" const char* epoch_reserve_capture_path_utf8(
    const char* backend,
    std::uintptr_t windowId);

extern "C" void epoch_release_capture_path_utf8(
    const char* backend,
    std::uintptr_t windowId);

extern "C" void core_log_write(
    std::uint32_t level,
    const char* tag,
    const char* message);

#if defined(EPOCH_USING_OPENGL) && (EPOCH_USING_OPENGL == 1)
namespace
{
    struct ScopedReadbackState final
    {
        GLint readFramebuffer{};
        GLint readBuffer{GL_BACK};
        GLint pixelPackBuffer{};
        GLint packAlignment{4};
        GLint packRowLength{};
        GLint packSkipRows{};
        GLint packSkipPixels{};

        ScopedReadbackState() noexcept
        {
            glGetIntegerv(
                GL_READ_FRAMEBUFFER_BINDING,
                &readFramebuffer);
            glGetIntegerv(GL_READ_BUFFER, &readBuffer);
            glGetIntegerv(
                GL_PIXEL_PACK_BUFFER_BINDING,
                &pixelPackBuffer);
            glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
            glGetIntegerv(GL_PACK_ROW_LENGTH, &packRowLength);
            glGetIntegerv(GL_PACK_SKIP_ROWS, &packSkipRows);
            glGetIntegerv(GL_PACK_SKIP_PIXELS, &packSkipPixels);
        }

        ScopedReadbackState(const ScopedReadbackState&) = delete;
        ScopedReadbackState& operator=(const ScopedReadbackState&) = delete;

        ~ScopedReadbackState() noexcept
        {
            glBindFramebuffer(
                GL_READ_FRAMEBUFFER,
                static_cast<GLuint>(readFramebuffer));
            glReadBuffer(static_cast<GLenum>(readBuffer));
            glBindBuffer(
                GL_PIXEL_PACK_BUFFER,
                static_cast<GLuint>(pixelPackBuffer));
            glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
            glPixelStorei(GL_PACK_ROW_LENGTH, packRowLength);
            glPixelStorei(GL_PACK_SKIP_ROWS, packSkipRows);
            glPixelStorei(GL_PACK_SKIP_PIXELS, packSkipPixels);
        }
    };

    [[nodiscard]] bool write_rgba8_bmp(
        const std::filesystem::path& path,
        const std::vector<std::uint8_t>& rgba,
        const int width,
        const int height)
    {
        if (width <= 0 || height <= 0)
            return false;
        const std::size_t pixelCount =
            static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height);
        if (rgba.size() < pixelCount * 4u)
            return false;

        const int rowBytes = width * 3;
        const int padding = (4 - (rowBytes % 4)) % 4;
        const int stride = rowBytes + padding;
        std::vector<std::uint8_t> bmp(
            static_cast<std::size_t>(stride)
                * static_cast<std::size_t>(height),
            0u);
        for (int y = 0; y < height; ++y)
        {
            const auto* source =
                rgba.data()
                + static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(width)
                    * 4u;
            auto* destination =
                bmp.data()
                + static_cast<std::size_t>(y)
                    * static_cast<std::size_t>(stride);
            for (int x = 0; x < width; ++x)
            {
                const std::size_t sourceOffset =
                    static_cast<std::size_t>(x) * 4u;
                const std::size_t destinationOffset =
                    static_cast<std::size_t>(x) * 3u;
                destination[destinationOffset + 0u] =
                    source[sourceOffset + 2u];
                destination[destinationOffset + 1u] =
                    source[sourceOffset + 1u];
                destination[destinationOffset + 2u] =
                    source[sourceOffset + 0u];
            }
        }

        std::uint8_t fileHeader[14]{
            'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
        std::uint8_t infoHeader[40]{
            40, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            1, 0,
            24, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0};
        const std::uint32_t fileSize =
            54u + static_cast<std::uint32_t>(bmp.size());
        std::memcpy(&fileHeader[2], &fileSize, sizeof(fileSize));
        std::memcpy(&infoHeader[4], &width, sizeof(width));
        std::memcpy(&infoHeader[8], &height, sizeof(height));

        std::ofstream output(path, std::ios::binary);
        if (!output)
            return false;
        output.write(
            reinterpret_cast<const char*>(fileHeader),
            sizeof(fileHeader));
        output.write(
            reinterpret_cast<const char*>(infoHeader),
            sizeof(infoHeader));
        output.write(
            reinterpret_cast<const char*>(bmp.data()),
            static_cast<std::streamsize>(bmp.size()));
        return output.good();
    }
}

namespace epochengine::openglcapture
{
    [[nodiscard]] bool capture_frame_to_bmp(
        const int width,
        const int height,
        const char* outputPathText,
        bool& hasPixels)
    {
        hasPixels = false;
        const std::filesystem::path outputPath{
            outputPathText != nullptr ? outputPathText : ""};
        if (outputPath.empty() || width <= 0 || height <= 0)
            return false;

        std::vector<std::uint8_t> pixels(
            static_cast<std::size_t>(width)
                * static_cast<std::size_t>(height)
                * 4u,
            0u);
        const ScopedReadbackState preservedState{};
        GLboolean doubleBuffered = GL_TRUE;
        glGetBooleanv(GL_DOUBLEBUFFER, &doubleBuffered);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glFinish();
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glReadBuffer(
            doubleBuffered == GL_TRUE ? GL_BACK : GL_FRONT);
        glReadPixels(
            0,
            0,
            width,
            height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels.data());

        for (std::size_t offset = 0u;
            offset + 2u < pixels.size();
            offset += 4u)
        {
            if (pixels[offset] != 0u
                || pixels[offset + 1u] != 0u
                || pixels[offset + 2u] != 0u)
            {
                hasPixels = true;
                break;
            }
        }
        if (!hasPixels)
            return false;

        return write_rgba8_bmp(
            outputPath,
            pixels,
            width,
            height);
    }

    void capture_frame_if_requested(
        const int width,
        const int height,
        const std::uintptr_t windowId)
    {
        const char* outputPath =
            epoch_reserve_capture_path_utf8("opengl", windowId);
        if (outputPath == nullptr || outputPath[0] == '\0')
            return;

        bool hasPixels = false;
        const bool captured =
            capture_frame_to_bmp(
                width,
                height,
                outputPath,
                hasPixels);
        if (!captured)
            epoch_release_capture_path_utf8("opengl", windowId);
        if (!captured && !hasPixels)
            return;

        const std::string message = captured
            ? std::string{"Captured frame to "} + outputPath
            : std::string{"Failed to write capture to "} + outputPath;
        core_log_write(
            captured ? 1u : 2u,
            "OpenGL",
            message.c_str());
    }
}
#else
namespace epochengine::openglcapture
{
    void capture_frame_if_requested(
        int,
        int,
        std::uintptr_t)
    {
    }
}
#endif
