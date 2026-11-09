// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace epoch::extensions::engine_network_runtime
{
    enum class PacketKind : std::uint16_t
    {
        Hello = 1,
        InputCommand = 2,
        Snapshot = 3,
        Delta = 4,
        Goodbye = 5
    };

    struct SessionId
    {
        std::uint64_t value{};

        friend constexpr bool operator==(const SessionId&, const SessionId&) = default;
    };

    struct PeerId
    {
        std::uint32_t value{};

        friend constexpr bool operator==(const PeerId&, const PeerId&) = default;
    };

    struct Endpoint
    {
        std::string host;
        std::uint16_t port{};
    };

    struct PacketHeader
    {
        SessionId session{};
        PeerId peer{};
        PacketKind kind{};
        std::uint32_t sequence{};
        std::uint32_t payloadBytes{};
    };

    struct NetworkRuntimePolicy
    {
        bool humanApprovedNetwork{};
        bool allowServerOrListener{};
        bool autoRunAllowed{};
        std::uint16_t requestedPort{};
    };

    struct PolicyDecision
    {
        bool allowed{};
        const char* reason{};
    };

    [[nodiscard]] PolicyDecision evaluate_runtime_policy(const NetworkRuntimePolicy& policy) noexcept;
    [[nodiscard]] std::vector<std::byte> make_packet_frame(const PacketHeader& header, std::span<const std::byte> payload);
    [[nodiscard]] bool validate_packet_frame(std::span<const std::byte> frame) noexcept;
}
