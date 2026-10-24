/*
 * EPOCH - updater device identity
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */

#include "updater.device_identity.hpp"
#include "updater.distribution.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#if EPOCH_EDITION_PUBLIC
namespace epochengine::updater::device_identity
{
    Identity load_or_create() { return {.diagnostic = "Public edition has no source-account identity."}; }
    std::optional<std::array<unsigned char, 64>> sign_sha256(std::span<const unsigned char, 32>) { return {}; }
    std::optional<std::string> load_registered_device_id() { return {}; }
    bool store_registered_device_id(std::string_view) { return false; }
    void clear_registered_device_id() noexcept {}
    std::string_view protection_name(KeyProtection) noexcept { return "Unavailable"; }
}
#else
#endif // EPOCH_EDITION_PUBLIC
