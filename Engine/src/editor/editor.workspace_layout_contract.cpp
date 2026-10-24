/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.
#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import editor.workspace_layout;

int main()
{
    return static_cast<int>(epochengine::editor_workspace::run_contract());
}
