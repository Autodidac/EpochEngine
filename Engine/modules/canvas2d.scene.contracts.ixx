/************************************************
 * Epoch Engine
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 ************************************************/
module;

#include <string_view>

export module canvas2d.scene.contracts;

export namespace epochengine::canvas2d_scene_contracts
{
    struct ContractResult final
    {
        bool passed{};
        std::string_view stage{"not_run"};
    };

    [[nodiscard]] ContractResult run() noexcept;
}
