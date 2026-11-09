// SPDX-License-Identifier: LicenseRef-MIT-NoSell
// Copyright (c) 2026 Adam Rushford

#include "epoch/extensions/engine_network_runtime/engine_network_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace epoch::extensions::engine_network_runtime
{
    namespace
    {
        constexpr std::array<std::byte, 4> kMagic{
            std::byte{'E'},
            std::byte{'P'},
            std::byte{'N'},
            std::byte{'1'}
        };

        template <typename T>
        void append_pod(std::vector<std::byte>& output, const T& value)
        {
            const auto* bytes = reinterpret_cast<const std::byte*>(&value);
            output.insert(output.end(), bytes, bytes + sizeof(T));
        }
    }

    PolicyDecision evaluate_runtime_policy(const NetworkRuntimePolicy& policy) noexcept
    {
        if (policy.autoRunAllowed)
        {
            return PolicyDecision{false, "network packages may not auto-run"};
        }

        if ((policy.allowServerOrListener || policy.requestedPort != 0U) && !policy.humanApprovedNetwork)
        {
            return PolicyDecision{false, "server/listener capability requires explicit human approval"};
        }

        return PolicyDecision{true, "network runtime contract is inert"};
    }

    std::vector<std::byte> make_packet_frame(const PacketHeader& header, std::span<const std::byte> payload)
    {
        auto frameHeader = header;
        frameHeader.payloadBytes = static_cast<std::uint32_t>(payload.size());

        std::vector<std::byte> frame;
        frame.reserve(kMagic.size() + sizeof(PacketHeader) + payload.size());
        frame.insert(frame.end(), kMagic.begin(), kMagic.end());
        append_pod(frame, frameHeader);
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    bool validate_packet_frame(std::span<const std::byte> frame) noexcept
    {
        if (frame.size() < kMagic.size() + sizeof(PacketHeader))
        {
            return false;
        }

        if (!std::equal(kMagic.begin(), kMagic.end(), frame.begin()))
        {
            return false;
        }

        PacketHeader header{};
        std::memcpy(&header, frame.data() + kMagic.size(), sizeof(PacketHeader));
        return frame.size() == kMagic.size() + sizeof(PacketHeader) + header.payloadBytes;
    }
}
