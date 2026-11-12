"""Exercise the production PNG cutout codec with no renderer."""
import os
from pathlib import Path
import tempfile
import unittest

from Tests.support.cpp import compile_and_run


ROOT = Path(__file__).resolve().parents[2]


class FoliageCutout(unittest.TestCase):
    def test_cutout_round_trip(self):
        reports = ROOT / 'Artifacts/Reports/first-landing-tree-sequence-nov11'
        reports.mkdir(parents=True, exist_ok=True)
        report = Path(tempfile.mkdtemp(prefix='cutout-', dir=reports))
        source = report / 'consumer.cpp'
        source.write_text(r'''
#include "image.stb.cpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
using namespace epochengine::image_codec;

int main(int argc, char** argv)
{
    std::vector<std::byte> image(16u * 16u * 4u, std::byte{255});
    const auto original = image;
    assert(!make_magenta_cutout(image, 16, 16) && image == original);
    for (std::size_t pixel{}; pixel < image.size() / 4u; ++pixel)
        image[pixel * 4u + 1u] = std::byte{0};
    for (std::size_t y = 4; y < 12; ++y)
        for (std::size_t x = 5; x < 11; ++x)
        {
            const auto offset = (y * 16 + x) * 4;
            image[offset] = std::byte{220};
            image[offset + 1] = std::byte{137};
            image[offset + 2] = std::byte{22};
        }
    const auto edge = (4 * 16 + 5) * 4;
    image[edge + 2] = std::byte{150};
    assert(make_magenta_cutout(image, 16, 16));
    assert(image[edge + 2] == std::byte{137});
    const auto amber = (8 * 16 + 8) * 4;
    assert(image[3] == std::byte{0} && image[amber + 1] == std::byte{137}
        && image[amber + 3] == std::byte{255});
    const auto encoded = encode_rgba_png(image, 16, 16);
    assert(decode_png(encoded, 16, image.size()).rgba == image);
    assert(!make_magenta_cutout(image, 17, 16));
    if (argc == 3)
    {
        const auto path = [](const char* argument) {
            const std::string_view value{argument};
            return std::filesystem::path{std::u8string{value.begin(), value.end()}};
        };
        std::ifstream in(path(argv[1]), std::ios::binary);
        const std::vector<char> bytes{std::istreambuf_iterator<char>{in}, {}};
        auto native = decode_png(std::as_bytes(std::span{bytes}), 2048, 16u * 1024u * 1024u);
        assert(native.status == PngStatus::ready);
        assert(make_magenta_cutout(native.rgba, native.width, native.height));
        const auto png = encode_rgba_png(native.rgba, native.width, native.height);
        assert(!png.empty() && decode_png(png, 2048, native.rgba.size()).rgba == native.rgba);
        std::ofstream out(path(argv[2]), std::ios::binary);
        assert(out.write(reinterpret_cast<const char*>(png.data()), png.size()));
    }
}
''', encoding='utf-8')
        native = os.environ.get('EPOCH_TEST_FOLIAGE_PNG')
        compile_and_run(self, source, report, includes=[ROOT / 'Engine/src/assets'],
                        args=lambda config: (native, report / f'{config}-foliage-cutout.png') if native else ())
