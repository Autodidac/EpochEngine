/*
 * EPOCH - updater device identity
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace epochengine::updater::device_identity
{
    enum class KeyProtection
    {
        Unavailable,
        WindowsPlatformNonExportable,
        WindowsSoftwareNonExportable,
        LinuxTpmNonExportable,
        LinuxOsProtectedExportable
    };

    struct Identity final
    {
        bool ready{};
        std::string public_jwk{};
        std::string device_id{};
        KeyProtection protection{KeyProtection::Unavailable};
        std::string diagnostic{};
    };

    [[nodiscard]] Identity load_or_create();
    [[nodiscard]] std::optional<std::array<unsigned char, 64>> sign_sha256(
        std::span<const unsigned char, 32> digest);

    [[nodiscard]] std::optional<std::string> load_registered_device_id();
    [[nodiscard]] bool store_registered_device_id(std::string_view device_id);
    void clear_registered_device_id() noexcept;

    [[nodiscard]] std::string_view protection_name(KeyProtection protection) noexcept;
}
