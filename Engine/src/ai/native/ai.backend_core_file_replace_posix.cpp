// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#include "ai.backend_core_file_replace.hpp"

#ifndef _WIN32
namespace epoch_ai::core {
std::error_code replace_file(const std::filesystem::path& staged,
                             const std::filesystem::path& destination) noexcept {
    std::error_code ec;
    std::filesystem::rename(staged, destination, ec);
    return ec;
}
} // namespace epoch_ai::core
#endif
