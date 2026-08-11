/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <string_view>

export module project.contracts;

export namespace epochengine::project_contracts
{
    struct ContractResult final
    {
        bool passed{};
        std::string_view stage{"not_run"};
    };

    [[nodiscard]] ContractResult run_asset_spine_contract() noexcept;
}
