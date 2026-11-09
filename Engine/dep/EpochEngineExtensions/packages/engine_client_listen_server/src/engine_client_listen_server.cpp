// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/engine_client_listen_server/engine_client_listen_server.hpp"

namespace epoch::extensions::engine_client_listen_server
{
    ListenServerPlan plan_listen_server(const ListenServerConfig& config) noexcept
    {
        if (!config.humanApprovedNetwork)
        {
            return ListenServerPlan{false, false, "server/listener capability requires explicit human approval"};
        }

        if (config.maxClients == 0U)
        {
            return ListenServerPlan{false, false, "listen-server package requires at least one client slot"};
        }

        if (config.endpoint.port == 0U)
        {
            return ListenServerPlan{false, false, "listen-server package requires a non-zero port for manual run"};
        }

        return ListenServerPlan{true, true, config.allowExternalClients ? "manual run may allow external clients" : "manual run is local-host scoped"};
    }
}
