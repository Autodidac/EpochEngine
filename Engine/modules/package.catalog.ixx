/*
 * This file is part of the Epoch Project.
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

export module package.catalog;

export namespace epochengine::package_catalog
{
    inline constexpr std::string_view schema = "epoch-package-catalog/v1";
    inline constexpr std::string_view endpoint =
        "https://epoch.adamrushford.chatgpt.site/api/epoch/packages/catalog";

    enum class Scope : std::uint8_t { engine, project, user };
    enum class Availability : std::uint8_t { available, descriptor_only };

    struct Entry
    {
        std::string id{};
        std::string display_name{};
        std::string summary{};
        std::string kind{};
        std::string activation{};
        Scope scope{ Scope::engine };
        Availability availability{ Availability::descriptor_only };
        std::string source_url{};
        std::string immutable_revision{};
        std::string sha256{};
        std::uint64_t size_bytes{};
        std::string license_url{};
        std::vector<std::string> platforms{};
        bool identity_required{};
        bool automatic_execution{};
        bool local_admission_required{ true };
        bool signed_payload{};
        bool content_addressed{};
    };

    struct Snapshot
    {
        bool ok{};
        std::string revision{};
        std::vector<Entry> entries{};
        std::string message{};
    };

    [[nodiscard]] Snapshot parse(std::string_view json);
    [[nodiscard]] Snapshot fetch();
    [[nodiscard]] bool contract_self_test();
    [[nodiscard]] std::string_view scope_name(Scope value) noexcept;
    [[nodiscard]] std::string_view availability_name(
        Availability value) noexcept;
}
