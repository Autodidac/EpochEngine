// Epoch Engine AI runtime - SPDX-License-Identifier: LicenseRef-MIT-NoSell
#pragma once
#include <string>
#include <string_view>

namespace epoch_ai::core {
[[nodiscard]] std::string make_id(std::string_view prefix);
}
