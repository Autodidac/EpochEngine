// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include "epoch/extensions/engine_network_runtime/engine_network_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace epoch::extensions::engine_authoritative_dedicated_server
{
    struct DedicatedServerConfig
    {
        epoch::extensions::engine_network_runtime::Endpoint endpoint{"0.0.0.0", 7777U};
        std::uint16_t tickRate{60};
        std::uint16_t maxClients{32};
        bool humanApprovedNetwork{};
        bool headlessOnly{true};
    };

    struct DedicatedServerPlan
    {
        bool readyForManualRun{};
        bool headlessOnly{};
        const char* gate{};
    };

    struct SnapshotEntity
    {
        std::uint64_t id{};
        float x{};
        float y{};
        float z{};
    };

    [[nodiscard]] DedicatedServerPlan plan_dedicated_server(const DedicatedServerConfig& config) noexcept;
    [[nodiscard]] std::vector<std::byte> make_snapshot_payload(std::span<const SnapshotEntity> entities);
}
