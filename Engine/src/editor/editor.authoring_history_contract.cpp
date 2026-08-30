// SPDX-License-Identifier: LicenseRef-MIT-NoSell

import editor.authoring_history;

int main()
{
    return epochengine::editor_authoring_history::run_contract()
            == epochengine::editor_authoring_history::ContractFailure::none
        ? 0
        : 1;
}
