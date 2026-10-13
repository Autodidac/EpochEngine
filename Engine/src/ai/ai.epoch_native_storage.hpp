#pragma once
// Epoch-owned locations for the embedded C++ agent runtime. No EngCoder
// application-data directory is created or required.
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace epochengine::ai::native_storage {
struct Paths final {
    std::filesystem::path data_root;
    std::filesystem::path workspace;
    std::filesystem::path model_cache;
    std::filesystem::path model_root;
    std::filesystem::path learning_root;
    std::filesystem::path web_root;
};

[[nodiscard]] inline Paths under_epoch_root(const std::filesystem::path& epoch_root) {
    if (epoch_root.empty()) throw std::runtime_error("Epoch application-data root is empty");
    const auto data = epoch_root / "AI" / "native";
    return {data, data / "workspace", data / "model-cache", data / "models",
            data / "learning", data / "web"};
}

[[nodiscard]] inline std::string environment(const char* key) {
#ifdef _WIN32
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, key) != 0 || buffer == nullptr) return {};
    std::string value{buffer};
    std::free(buffer);
    return value;
#else
    if (const auto* value = std::getenv(key)) return value;
    return {};
#endif
}

[[nodiscard]] inline Paths defaults() {
#ifdef _WIN32
    if (const auto local = environment("LOCALAPPDATA"); !local.empty())
        return under_epoch_root(std::filesystem::path{local} / "EpochEngine");
    if (const auto home = environment("USERPROFILE"); !home.empty())
        return under_epoch_root(std::filesystem::path{home} / "AppData" / "Local" / "EpochEngine");
#else
    if (const auto xdg = environment("XDG_DATA_HOME"); !xdg.empty())
        return under_epoch_root(std::filesystem::path{xdg} / "EpochEngine");
    if (const auto home = environment("HOME"); !home.empty())
        return under_epoch_root(std::filesystem::path{home} / ".local" / "share" / "EpochEngine");
#endif
    // Explicitly keep the fallback Epoch-owned, never under EngCoder.
    return under_epoch_root(std::filesystem::current_path() / ".epoch");
}

inline void initialize(const Paths& paths) {
    for (const auto& path : {paths.data_root, paths.workspace, paths.model_cache,
                             paths.model_root, paths.learning_root, paths.web_root}) {
        std::error_code ec;
        if (!std::filesystem::create_directories(path, ec) && ec)
            throw std::runtime_error("Cannot initialize Epoch AI directory " + path.string() + ": " + ec.message());
        ec.clear();
        if (!std::filesystem::is_directory(path, ec) || ec)
            throw std::runtime_error("Epoch AI path is not a directory: " + path.string());
    }
}
} // namespace epochengine::ai::native_storage
