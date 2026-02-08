module;
export module aimage.loader;

import <algorithm>;
import <cstdint>;
import <cstring>;
import <filesystem>;
import <fstream>;
import <iostream>;
import <limits>;
import <stdexcept>;
import <string>;
import <vector>;

export namespace almondnamespace
{
    struct ImageData
    {
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
        int channels = 0;

        ImageData(std::vector<uint8_t> p, int w, int h, int c)
            : pixels(std::move(p)), width(w), height(h), channels(c) {}
    };

    inline void a_listSupportedImageTypes()
    {
        std::cout << "Supported image types: BMP, TGA, PPM\n";
    }

    inline ImageData a_loadBMP(const std::filesystem::path& path, bool flipVertically);
    inline ImageData a_loadTGA(const std::filesystem::path& path, bool flipVertically);
    inline ImageData a_loadPPM(const std::filesystem::path& path, bool flipVertically);

    inline ImageData a_loadImage(const std::filesystem::path& filepath, bool flipVertically = false)
    {
        auto ext = filepath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".bmp") return a_loadBMP(filepath, flipVertically);
        if (ext == ".tga") return a_loadTGA(filepath, flipVertically);
        if (ext == ".ppm") return a_loadPPM(filepath, flipVertically);

        throw std::runtime_error("Unsupported image format: " + filepath.string());
    }

    inline ImageData a_loadBMP(const std::filesystem::path& filepath, bool flipVertically)
    {
        std::ifstream f(filepath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open BMP: " + filepath.string());

        char hdr[54];
        f.read(hdr, 54);
        if (std::memcmp(hdr, "BM", 2) != 0)
            throw std::runtime_error("Invalid BMP header: " + filepath.string());

        int w = *reinterpret_cast<int*>(&hdr[18]);
        int h = *reinterpret_cast<int*>(&hdr[22]);
        int bpp = *reinterpret_cast<short*>(&hdr[28]);
        int ch = bpp / 8;
        int pad = (4 - (w * ch) % 4) % 4;

        std::vector<uint8_t> raw(size_t(w) * size_t(h) * ch);

        f.seekg(*reinterpret_cast<int*>(&hdr[10]));
        for (int y = 0; y < h; ++y) {
            f.read(reinterpret_cast<char*>(raw.data() + y * w * ch), w * ch);
            f.ignore(pad);
        }

        std::vector<uint8_t> out(size_t(w) * size_t(h) * 4);
        for (int y = 0; y < h; ++y) {
            int srcY = flipVertically ? (h - 1 - y) : y;
            for (int x = 0; x < w; ++x) {
                int srcIdx = (srcY * w + x) * ch;
                int dstIdx = (y * w + x) * 4;
                out[dstIdx + 0] = raw[srcIdx + 2];
                out[dstIdx + 1] = raw[srcIdx + 1];
                out[dstIdx + 2] = raw[srcIdx + 0];
                out[dstIdx + 3] = (ch == 4) ? raw[srcIdx + 3] : 255;
            }
        }

        return ImageData(std::move(out), w, h, 4);
    }

    inline ImageData a_loadTGA(const std::filesystem::path& filepath, bool flipVertically)
    {
        std::ifstream f(filepath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open TGA: " + filepath.string());

        uint8_t hdr[18];
        f.read(reinterpret_cast<char*>(hdr), 18);

        uint8_t  idLen = hdr[0];
        uint8_t  imgType = hdr[2];
        uint16_t w = *reinterpret_cast<uint16_t*>(&hdr[12]);
        uint16_t h = *reinterpret_cast<uint16_t*>(&hdr[14]);
        uint8_t  bpp = hdr[16];
        uint8_t  desc = hdr[17];

        if ((imgType != 2 && imgType != 10) || (bpp != 24 && bpp != 32))
            throw std::runtime_error("Unsupported TGA format: " + filepath.string());

        int ch = bpp / 8;
        bool originTopLeft = (desc & 0x20) != 0;

        f.seekg(idLen, std::ios::cur);

        const size_t pixelCount = size_t(w) * size_t(h);
        std::vector<uint8_t> raw(pixelCount * ch);

        if (imgType == 2) {
            f.read(reinterpret_cast<char*>(raw.data()), raw.size());
        }
        else {
            size_t totalRead = 0;
            while (totalRead < pixelCount) {
                uint8_t header = 0;
                f.read(reinterpret_cast<char*>(&header), 1);

                int count = (header & 0x7F) + 1;
                if (header & 0x80) {
                    std::vector<uint8_t> pixel(ch);
                    f.read(reinterpret_cast<char*>(pixel.data()), ch);
                    for (int i = 0; i < count; ++i)
                        std::copy(pixel.begin(), pixel.end(), raw.begin() + (totalRead + i) * ch);
                }
                else {
                    f.read(reinterpret_cast<char*>(raw.data() + totalRead * ch), count * ch);
                }
                totalRead += count;
            }
        }

        std::vector<uint8_t> out(size_t(w) * size_t(h) * 4);
        for (int y = 0; y < h; ++y) {
            int srcY = flipVertically ? (h - 1 - y) : y;
            if (originTopLeft)
                srcY = h - 1 - srcY;

            for (int x = 0; x < w; ++x) {
                int srcIdx = (srcY * w + x) * ch;
                int dstIdx = (y * w + x) * 4;
                out[dstIdx + 0] = raw[srcIdx + 2];
                out[dstIdx + 1] = raw[srcIdx + 1];
                out[dstIdx + 2] = raw[srcIdx + 0];
                out[dstIdx + 3] = (ch == 4) ? raw[srcIdx + 3] : 255;
            }
        }

        return ImageData(std::move(out), static_cast<int>(w), static_cast<int>(h), 4);
    }

    inline ImageData a_loadPPM(const std::filesystem::path& filepath, bool flipVertically)
    {
        std::ifstream f(filepath, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open PPM: " + filepath.string());

        auto skip_ws_and_comments = [&]()
            {
                while (true)
                {
                    // eat whitespace
                    while (f && std::isspace(static_cast<unsigned char>(f.peek())))
                        (void)f.get();

                    // eat comment line
                    if (f && f.peek() == '#')
                    {
                        std::string dummy;
                        std::getline(f, dummy);
                        continue;
                    }
                    break;
                }
            };

        auto read_int = [&](int& v)
            {
                skip_ws_and_comments();
                f >> v;
                if (!f) throw std::runtime_error("Invalid PPM header: " + filepath.string());
            };

        std::string magic;
        f >> magic;
        if (magic != "P6")
            throw std::runtime_error("Unsupported PPM format (expected P6): " + filepath.string());

        int w = 0, h = 0, maxVal = 0;
        read_int(w);
        read_int(h);
        read_int(maxVal);

        if (w <= 0 || h <= 0 || maxVal <= 0 || maxVal > 255)
            throw std::runtime_error("Invalid PPM header: " + filepath.string());

        // After maxVal there is a single whitespace byte before binary data.
        // Consume exactly one char if it's whitespace; if not, still consume one to align.
        int c = f.get();
        if (c == EOF) throw std::runtime_error("Truncated PPM: " + filepath.string());
        if (!std::isspace(static_cast<unsigned char>(c)))
        {
            // some writers don't include whitespace; treat this byte as first pixel channel
            f.unget();
        }

        const std::size_t rgbBytes = std::size_t(w) * std::size_t(h) * 3;
        std::vector<std::uint8_t> raw(rgbBytes);
        f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
        if (f.gcount() != static_cast<std::streamsize>(raw.size()))
            throw std::runtime_error("Truncated PPM pixel data: " + filepath.string());

        std::vector<std::uint8_t> out(std::size_t(w) * std::size_t(h) * 4);
        for (int y = 0; y < h; ++y)
        {
            const int srcY = flipVertically ? (h - 1 - y) : y;
            for (int x = 0; x < w; ++x)
            {
                const int srcIdx = (srcY * w + x) * 3;
                const int dstIdx = (y * w + x) * 4;
                out[dstIdx + 0] = raw[srcIdx + 0];
                out[dstIdx + 1] = raw[srcIdx + 1];
                out[dstIdx + 2] = raw[srcIdx + 2];
                out[dstIdx + 3] = 255;
            }
        }

        return ImageData(std::move(out), w, h, 4);
    }
}
