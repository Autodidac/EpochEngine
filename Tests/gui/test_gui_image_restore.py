"""Cold GUI image cache recovery using the authored texture store."""

import os
from pathlib import Path
import unittest

from Tests.build.test_opengl_sprite_stream import definition
from Tests.support.gui_contract import ROOT, run_gui_contract


ADAPTER = ROOT / "Engine/src/project/project.gui_epochgui.cpp"


@unittest.skipUnless(os.name == "nt", "Windows long-path manifest and MSVC")
class GuiImageRestore(unittest.TestCase):
    def test_image_and_image_button_restore_authored_texture(self):
        adapter = ADAPTER.read_text(encoding="utf-8")
        consumer = r"""
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#define EPOCH_ENABLE_AUTHORING_PLATFORM 1
#define EPOCH_ENABLE_TEXTURE_EDITOR 1
import asset.gui_artifact;
import asset.texture_artifact;
import authoring.texture;
import project.texture_library;
import project.texture_source;
namespace fs = std::filesystem;
namespace epochengine::project_gui {
class EpochGuiAdapter {
public:
    std::string project_id_, project_root_;
    void prepare_images(std::span<const asset::gui::CompiledWidget>);
};
LOAD_IMAGE
PREPARE_IMAGES
}

int main() {
    using namespace epochengine;
    using namespace authoring::texture;
    constexpr const char *image = "Assets/Textures/gui_image.epoch_texture";
    constexpr const char *button = "Assets/Textures/gui_button.epoch_texture";
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::current_path() / ("fixture-" + std::to_string(tick));
    fs::create_directories(root);
    project_gui::EpochGuiAdapter adapter{"gui-image-restore", root.string()};
    project_texture_sources::ProjectTextureSourceStore sources{adapter.project_id_, adapter.project_root_};
    project_textures::TextureArtifactLibrary library{adapter.project_id_, adapter.project_root_};
    TextureDocument document{DocumentHandle{0u, 1u}, BranchIdentity{3u, 7u}, CanvasDescriptor{
        .width=8u, .height=8u, .tile_extent=4u, .mip_count=1u,
        .format=PixelFormat::rgba8_unorm, .color_space=ColorSpace::linear}};
    const auto layer = document.create_layer(LayerDescriptor{.name="Paint"}, 0u,
        TemporalPoint{BranchIdentity{3u, 7u}, 1});
    StrokeDescriptor stroke{};
    stroke.target = layer.layer;
    stroke.color = {255u, 32u, 16u, 255u};
    stroke.radius_subpixels = 256u;
    stroke.samples.push_back({.x_subpixels=640, .y_subpixels=896});
    if (!layer || !document.apply_stroke(stroke, TemporalPoint{BranchIdentity{3u, 7u}, 2})) return 1;
    if (!sources.save(image, document) || !sources.save(button, document)) return 2;
    if (fs::exists(root / "Library/Textures")) return 3;

    std::array<asset::gui::CompiledWidget, 2> widgets{};
    widgets[0].kind = asset::gui::WidgetKind::image;
    widgets[0].asset_path = image;
    widgets[1].kind = asset::gui::WidgetKind::image_button;
    widgets[1].asset_path = button;
    adapter.prepare_images(widgets);
    const auto restored = project_gui::load_gui_image(library, image);
    if (!restored || !project_gui::load_gui_image(library, button)) return 4;

    const fs::path artifact{restored.locator.storage_path};
    std::ofstream(artifact, std::ios::binary | std::ios::trunc) << "corrupt";
    adapter.prepare_images(widgets);
    std::ifstream bytes(artifact, std::ios::binary);
    std::string content;
    bytes >> content;
    if (content != "corrupt" || project_gui::load_gui_image(library, image)) return 5;
    return project_gui::load_gui_image(library, button) ? 0 : 6;
}
"""
        consumer = consumer.replace("LOAD_IMAGE", definition(
            adapter, "        [[nodiscard]] project_textures::LoadedTextureArtifact load_gui_image("))
        consumer = consumer.replace("PREPARE_IMAGES", definition(
            adapter, "    void EpochGuiAdapter::prepare_images("))
        run_gui_contract(
            self, "gui-image-cold-cache", consumer, [Path(__file__), ADAPTER],
            "Restore two GUI image types from authored source and preserve an existing corrupt artifact.",
            extra_sources=(
                "Engine/src/project/project.texture_source.cpp",
                "Engine/src/project/project.texture_library.cpp",
                "Engine/src/project/project.asset_registry.cpp",
                "Engine/src/assets/texture/asset.texture_artifact.cpp",
                "Engine/src/authoring/texture/authoring.texture_document.cpp",
                "Engine/src/authoring/texture/authoring.texture_residency.cpp",
            ),
            extra_modules=(
                "authoring.texture.ixx", "authoring.texture_detail.ixx",
                "asset.texture_artifact.ixx", "project.texture_source.ixx",
                "project.texture_library.ixx", "project.asset_registry.ixx",
                "platform.filesystem.ixx",
            ),
            libraries=("shell32", "ole32"), long_paths=True,
        )
