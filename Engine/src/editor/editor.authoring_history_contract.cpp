// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Epoch system notes (auto-checked):
// Epoch Editor: keep editor state and source changes within their owning workflows.

import editor.authoring_history;

int main()
{
    return epochengine::editor_authoring_history::run_contract()
            == epochengine::editor_authoring_history::ContractFailure::none
        ? 0
        : 1;
}
