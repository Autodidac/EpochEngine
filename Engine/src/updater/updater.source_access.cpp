/************************************************
 *  EPOCH - authenticated private source access *
 *                                              *
 *  SPDX-License-Identifier:                   *
 *  LicenseRef-MIT-NoSell                      *
 ***********************************************/
module;

#include "updater.distribution.hpp"
#include "updater.device_identity.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module updater.source_access;

import epoch.version;
import updater.config;

#if EPOCH_EDITION_PUBLIC
namespace epochengine::updater
{
    SourceAccessStatus private_source_access_status() { return {}; }
    bool open_private_source_verification_page() { return false; }
    void cancel_private_source_access() noexcept {}
    bool private_source_access_enabled() noexcept { return false; }
    bool private_source_device_registered() noexcept { return false; }
    void reset_private_source_device_registration() noexcept {}
    PreparedSourceArchive acquire_private_source_archive(const std::filesystem::path&)
    {
        return {.message = "Public Epoch source uses its GitHub repository; account authorization is unavailable in this edition."};
    }
    bool private_source_access_contract_self_test()
    {
        return !private_source_access_enabled() && !private_source_device_registered()
            && PROJECT_SOURCE_DEVICE_START_URL().empty() && PROJECT_SOURCE_DEVICE_TOKEN_URL().empty()
            && PROJECT_SOURCE_DEVICE_CHALLENGE_URL().empty();
    }
}
#else
#endif // EPOCH_EDITION_PUBLIC
