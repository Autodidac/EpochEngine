// Standalone C++23 test for Epoch-owned native model/runtime storage.
#include "../../Engine/src/ai/ai.epoch_native_storage.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
int main() {
    namespace fs=std::filesystem;
    namespace ns=epochengine::ai::native_storage;
    const auto base=fs::temp_directory_path()/"epoch-native-storage-regression";
    fs::remove_all(base);
    const auto paths=ns::under_epoch_root(base/"EpochEngine");
    assert(paths.data_root==base/"EpochEngine"/"AI"/"native");
    assert(paths.workspace==paths.data_root/"workspace");
    assert(paths.model_cache==paths.data_root/"model-cache");
    ns::initialize(paths);
    assert(fs::is_directory(paths.workspace));
    assert(fs::is_directory(paths.model_cache));
    assert(fs::is_directory(paths.learning_root));
    assert(fs::is_directory(paths.model_root));
    assert(fs::is_directory(paths.web_root));
    ns::initialize(paths); // idempotent
    std::ofstream(paths.workspace/"existing.txt")<<"retained";
    ns::initialize(paths);
    assert(fs::exists(paths.workspace/"existing.txt"));
    // A file cannot be silently treated as a valid workspace.
    fs::remove_all(paths.workspace);
    std::ofstream(paths.workspace)<<"file blocker";
    bool rejected=false;
    try { ns::initialize(paths); } catch(const std::runtime_error&) { rejected=true; }
    assert(rejected);
    fs::remove_all(base);
}
