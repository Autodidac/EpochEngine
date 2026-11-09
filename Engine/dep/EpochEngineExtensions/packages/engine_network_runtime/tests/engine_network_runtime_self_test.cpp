// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/engine_network_runtime/engine_network_runtime.hpp"

#include <array>
#include <stdexcept>

using namespace epoch::extensions::engine_network_runtime;

int main()
{
    const auto denied = evaluate_runtime_policy(NetworkRuntimePolicy{false, true, false, 7777U});
    if (denied.allowed)
    {
        throw std::runtime_error("network runtime allowed a listener without human approval");
    }

    const auto inert = evaluate_runtime_policy(NetworkRuntimePolicy{false, false, false, 0U});
    if (!inert.allowed)
    {
        throw std::runtime_error("network runtime rejected inert contract usage");
    }

    const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}};
    const auto frame = make_packet_frame(PacketHeader{SessionId{9U}, PeerId{2U}, PacketKind::Snapshot, 11U, 0U}, payload);
    if (!validate_packet_frame(frame))
    {
        throw std::runtime_error("network runtime frame validation failed");
    }

    return 0;
}
