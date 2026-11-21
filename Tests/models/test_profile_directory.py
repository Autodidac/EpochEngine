"""Check catalog path ownership and ordinary registration removal with the native methods."""
from pathlib import Path
import tempfile
import unittest

from Tests.support.cpp import compile_and_run, dependency_include
from Tools.source.owned_source_text import definition

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "Engine/src/ai/native"


class ProfileDirectory(unittest.TestCase):
    def test_manifest_removal_preserves_unrelated_files(self):
        source = (NATIVE / "ai.backend_model_native_model_manager.cpp").read_text(encoding="utf-8")
        methods = "\n".join(definition(source, name) for name in (
            "[[nodiscard]] std::string strip_scheme(",
            "std::string NativeModelManager::normalize_id(",
            "std::filesystem::path NativeModelManager::manifest_path(",
            "std::expected<nlohmann::json, core::Error> NativeModelManager::remove(",
        ))
        code = r'''
#include <expected>
#include "ai.backend_model_runtime_model.cpp"
#include "ai.backend_core_error.cpp"
namespace epoch_ai::model {
struct NativeModelManager {
    std::filesystem::path catalog_root_;
    RuntimeModelProfile selected;
    std::mutex mutex_;
    nlohmann::json install_state_ = nlohmann::json::object();
    std::string normalize_id(std::string_view id) const;
    std::filesystem::path manifest_path(std::string_view id) const;
    std::optional<RuntimeModelProfile> find(std::string_view id) const {
        return id == selected.id ? std::optional{selected} : std::nullopt;
    }
    bool model_loaded(std::string_view) const { return false; }
    std::expected<nlohmann::json,core::Error> delete_downloaded_weights(std::string_view) {
        throw std::logic_error("registration removal requested cache deletion");
    }
    void load_profiles_locked() {}
    std::expected<nlohmann::json,core::Error> remove(std::string_view,bool);
};
METHODS
}
int main(int argc,char** argv) {
    if (argc != 2) return 1;
    namespace fs = std::filesystem;
    epoch_ai::model::NativeModelManager manager;
    manager.catalog_root_ = fs::path(argv[1]) / "catalog";
    manager.selected.id = "user.model-2";
    const auto manifest = manager.manifest_path(manager.selected.id);
    fs::create_directories(manifest.parent_path());
    const auto notes = manifest.parent_path() / "my-notes.txt";
    std::ofstream(notes) << "keep";
    std::ofstream(manifest) << "{}";
    if (!manager.remove(manager.selected.id,false) || fs::exists(manifest) || !fs::is_regular_file(notes)) return 2;
    fs::remove(notes);
    std::ofstream(manifest) << "{}";
    if (!manager.remove(manager.selected.id,false) || fs::exists(manifest.parent_path())) return 3;
    for (const auto* id : {"", ".", "..", "name.", "nul", "COM1.gguf"}) {
        try { (void)manager.manifest_path(id); return 4; }
        catch (const std::invalid_argument&) {}
    }
}
'''.replace("METHODS", methods)
        parent = ROOT / "build/contract-tests"
        parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=parent, prefix="catalog-owner-") as directory:
            root = Path(directory)
            unit = root / "contract.cpp"
            unit.write_text(code, encoding="utf-8")
            compile_and_run(self, unit, root, args=lambda mode: [root / mode],
                            includes=(NATIVE, dependency_include(ROOT)))
