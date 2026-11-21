/************************************************
 *  EPOCH - authenticated private source access *
 *                                              *
 *  SPDX-License-Identifier:                   *
 *  LicenseRef-MIT-NoSell                      *
 ***********************************************/
module;

#include "updater.distribution.hpp"
#include <filesystem>
#include <string>

module updater.source_access;

import updater.config;

#if EPOCH_EDITION_PUBLIC
namespace epochengine::updater
{
    bool SourceAccessStatus::active() const noexcept { return false; }
    bool SourceAccessStatus::awaiting_device_approval() const noexcept { return false; }
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
#endif
