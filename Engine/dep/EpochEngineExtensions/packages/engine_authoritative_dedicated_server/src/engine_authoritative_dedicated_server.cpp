// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/engine_authoritative_dedicated_server/engine_authoritative_dedicated_server.hpp"

namespace epoch::extensions::engine_authoritative_dedicated_server
{
    namespace
    {
        template <typename T>
        void append_pod(std::vector<std::byte>& output, const T& value)
        {
            const auto* bytes = reinterpret_cast<const std::byte*>(&value);
            output.insert(output.end(), bytes, bytes + sizeof(T));
        }
    }

    DedicatedServerPlan plan_dedicated_server(const DedicatedServerConfig& config) noexcept
    {
        if (!config.humanApprovedNetwork)
        {
            return DedicatedServerPlan{false, config.headlessOnly, "server/listener capability requires explicit human approval"};
        }

        if (!config.headlessOnly)
        {
            return DedicatedServerPlan{false, false, "dedicated server package must remain headless"};
        }

        if (config.tickRate == 0U || config.maxClients == 0U || config.endpoint.port == 0U)
        {
            return DedicatedServerPlan{false, true, "dedicated server tick rate, client count, and manual-run port must be non-zero"};
        }

        return DedicatedServerPlan{true, true, "manual headless server run is approved"};
    }

    std::vector<std::byte> make_snapshot_payload(std::span<const SnapshotEntity> entities)
    {
        std::vector<std::byte> payload;
        const auto count = static_cast<std::uint32_t>(entities.size());
        payload.reserve(sizeof(count) + entities.size() * sizeof(SnapshotEntity));
        append_pod(payload, count);
        for (const auto& entity : entities)
        {
            append_pod(payload, entity);
        }
        return payload;
    }
}
