// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
#pragma once
#include <string>
#include <string_view>

namespace epoch_ai::core {
[[nodiscard]] std::string make_id(std::string_view prefix);
}
