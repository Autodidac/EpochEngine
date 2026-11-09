// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include "epoch/extensions/engine_network_runtime/engine_network_runtime.hpp"

#include <cstdint>
#include <string>

namespace epoch::extensions::engine_client_listen_server
{
    struct ListenServerConfig
    {
        epoch::extensions::engine_network_runtime::Endpoint endpoint{"127.0.0.1", 7777U};
        std::uint16_t maxClients{8};
        bool humanApprovedNetwork{};
        bool allowExternalClients{};
    };

    struct ListenServerPlan
    {
        bool readyForManualRun{};
        bool bindsPortWhenRun{};
        const char* gate{};
    };

    [[nodiscard]] ListenServerPlan plan_listen_server(const ListenServerConfig& config) noexcept;
}
