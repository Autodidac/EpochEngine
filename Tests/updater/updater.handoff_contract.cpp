// SPDX-License-Identifier: LicenseRef-MIT-NoSell
#include "../../Engine/src/updater/updater.handoff_script.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace epochengine::updater::system_detail {
inline std::filesystem::path fixture_root;
inline std::optional<std::string> sha256_file_hex(const std::filesystem::path& path)
{ std::ifstream input(path, std::ios::binary); return input ? std::optional{std::string(64, 'a')} : std::nullopt; }
inline void log_error(const std::string&) {}
inline std::filesystem::path make_temp_script_path(const char*) { return fixture_root / "scripts/handoff.bat"; }
inline std::string quote_shell_arg(const std::string& text) { return "\'" + text + "\'"; }
}
namespace epochengine::updater {
PRODUCTION_HELPERS
}

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    const std::filesystem::path root{argv[1]};
    std::ofstream windows(root / "handoff.ps1", std::ios::binary);
    windows << epochengine::updater::handoff_script::windows;
    windows.close();
    std::ofstream posix(root / "handoff.sh", std::ios::binary);
    posix << "#!/bin/sh\nset -eu\n" << epochengine::updater::handoff_script::posix
        << "case \"$EPOCH_UPDATE_MODE\" in\n"
        << "  stage) epoch_stage_payload;;\n  commit) epoch_install_payload;;\n  recover) epoch_restore_payload;;\n  *) exit 1;;\nesac\n";
    posix.close();
    if (!windows || !posix) return 2;
    namespace fs = std::filesystem;
    fs::create_directories(root / "payload/assets");
    fs::create_directories(root / "payload/lib");
    fs::create_directories(root / "overrides");
    fs::create_directories(root / "installed");
    std::ofstream(root / "payload/app.exe") << "executable";
    std::ofstream(root / "payload/a.dll") << "library";
    std::ofstream(root / "payload/assets/a.txt") << "asset";
    std::ofstream(root / "payload/lib/b.so") << "library";
    std::ofstream(root / "payload/user.settings") << "excluded";
    std::ofstream(root / "overrides/a.txt") << "override";
    epochengine::updater::system_detail::fixture_root = root;
    const auto handoff = epochengine::updater::prepare_binary_handoff(
        root / "installed/app.exe", root / "payload/app.exe", true, root / "overrides");
    if (!handoff) return 3;
    std::ifstream plan(handoff->manifest);
    const std::string text{std::istreambuf_iterator<char>{plan}, {}};
    if (text.find("user.settings") != std::string::npos
        || text.find("assets/a.txt") == std::string::npos
        || text.find("lib/b.so") == std::string::npos
        || text.find((root / "overrides" / "a.txt").make_preferred().string()) == std::string::npos) return 4;
    std::ofstream generated(root / "generated.stub");
    epochengine::updater::append_binary_stage(generated, *handoff, root / "installed/app.exe", root / "payload/app.exe");
    epochengine::updater::append_binary_commit(generated, *handoff);
    return generated ? 0 : 5;
}
