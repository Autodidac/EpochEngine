// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/engine_authoritative_dedicated_server/engine_authoritative_dedicated_server.hpp"

#include <array>
#include <stdexcept>

using namespace epoch::extensions::engine_authoritative_dedicated_server;

int main()
{
    const auto denied = plan_dedicated_server(DedicatedServerConfig{});
    if (denied.readyForManualRun)
    {
        throw std::runtime_error("dedicated server package bypassed human approval");
    }

    auto config = DedicatedServerConfig{};
    config.humanApprovedNetwork = true;
    const auto approved = plan_dedicated_server(config);
    if (!approved.readyForManualRun || !approved.headlessOnly)
    {
        throw std::runtime_error("dedicated server package failed approved headless planning");
    }

    const std::array entities{
        SnapshotEntity{1U, 1.0F, 2.0F, 3.0F},
        SnapshotEntity{2U, 4.0F, 5.0F, 6.0F}
    };

    const auto payload = make_snapshot_payload(entities);
    if (payload.size() <= sizeof(std::uint32_t))
    {
        throw std::runtime_error("dedicated server snapshot payload was empty");
    }

    return 0;
}
