// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once

#include <filesystem>
#include <system_error>

namespace epoch_ai::core {
// Replace a sibling temporary file with a completed manifest without first
// deleting/truncating the destination. Caller is responsible for staging and
// closing/flushing the temporary file. Both paths must be on one filesystem.
[[nodiscard]] std::error_code replace_file(
    const std::filesystem::path& staged,
    const std::filesystem::path& destination) noexcept;
} // namespace epoch_ai::core
