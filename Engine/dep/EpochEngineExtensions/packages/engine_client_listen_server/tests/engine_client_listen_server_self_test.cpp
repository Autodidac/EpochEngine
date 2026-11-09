// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/engine_client_listen_server/engine_client_listen_server.hpp"

#include <stdexcept>

using namespace epoch::extensions::engine_client_listen_server;

int main()
{
    const auto denied = plan_listen_server(ListenServerConfig{});
    if (denied.readyForManualRun || denied.bindsPortWhenRun)
    {
        throw std::runtime_error("listen-server package bypassed human approval");
    }

    auto approvedConfig = ListenServerConfig{};
    approvedConfig.humanApprovedNetwork = true;
    approvedConfig.maxClients = 4;

    const auto approved = plan_listen_server(approvedConfig);
    if (!approved.readyForManualRun || !approved.bindsPortWhenRun)
    {
        throw std::runtime_error("listen-server package failed approved manual-run planning");
    }

    return 0;
}
