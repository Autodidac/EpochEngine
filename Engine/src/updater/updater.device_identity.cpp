/*
 * EPOCH - updater device identity
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */

#include "updater.device_identity.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#pragma comment(lib, "ncrypt.lib")
#else
#include <sys/stat.h>
#endif

namespace epochengine::updater::device_identity
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr std::wstring_view key_name =
            L"EpochEngine.SourceDeviceAuth.v1";

        [[nodiscard]] bool safe_device_id(const std::string_view text)
        {
            return !text.empty() && text.size() <= 160u
                && std::all_of(text.begin(), text.end(), [](const char ch)
                {
                    return (ch >= 'a' && ch <= 'z')
                        || (ch >= 'A' && ch <= 'Z')
                        || (ch >= '0' && ch <= '9')
                        || ch == '-' || ch == '_' || ch == '.' || ch == ':';
                });
        }

        [[nodiscard]] fs::path registration_path()
        {
#if defined(_WIN32)
            const DWORD required = GetEnvironmentVariableW(
                L"LOCALAPPDATA",
                nullptr,
                0u);
            if (required > 1u)
            {
                std::wstring value(required, L'\0');
                const DWORD written = GetEnvironmentVariableW(
                    L"LOCALAPPDATA",
                    value.data(),
                    required);
                if (written > 0u && written < required)
                {
                    value.resize(written);
                    return fs::path{value}
                        / "EpochEngine"
                        / "config"
                        / "source_device_registration.v1";
                }
            }
#else
            if (const char* xdg = std::getenv("XDG_CONFIG_HOME");
                xdg != nullptr && *xdg != '\0')
            {
                return fs::path{xdg}
                    / "epochengine"
                    / "source_device_registration.v1";
            }
            if (const char* user_home = std::getenv("HOME");
                user_home != nullptr && *user_home != '\0')
            {
                return fs::path{user_home}
                    / ".config"
                    / "epochengine"
                    / "source_device_registration.v1";
            }
#endif
            return fs::current_path()
                / "config"
                / "source_device_registration.v1";
        }
        [[nodiscard]] std::optional<fs::path> safe_registration_path() noexcept
        {
            try
            {
                return registration_path();
            }
            catch (...)
            {
                return std::nullopt;
            }
        }


        [[nodiscard]] std::string encode_base64url(
            const unsigned char* bytes,
            const std::size_t size)
        {
            static constexpr char alphabet[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz"
                "0123456789-_";
            std::string output;
            output.reserve((size * 4u + 2u) / 3u);
            std::size_t index = 0u;
            while (index + 3u <= size)
            {
                const std::uint32_t value =
                    (static_cast<std::uint32_t>(bytes[index]) << 16u)
                    | (static_cast<std::uint32_t>(bytes[index + 1u]) << 8u)
                    | static_cast<std::uint32_t>(bytes[index + 2u]);
                output.push_back(alphabet[(value >> 18u) & 0x3fu]);
                output.push_back(alphabet[(value >> 12u) & 0x3fu]);
                output.push_back(alphabet[(value >> 6u) & 0x3fu]);
                output.push_back(alphabet[value & 0x3fu]);
                index += 3u;
            }
            const std::size_t remaining = size - index;
            if (remaining == 1u)
            {
                const std::uint32_t value =
                    static_cast<std::uint32_t>(bytes[index]) << 16u;
                output.push_back(alphabet[(value >> 18u) & 0x3fu]);
                output.push_back(alphabet[(value >> 12u) & 0x3fu]);
            }
            else if (remaining == 2u)
            {
                const std::uint32_t value =
                    (static_cast<std::uint32_t>(bytes[index]) << 16u)
                    | (static_cast<std::uint32_t>(bytes[index + 1u]) << 8u);
                output.push_back(alphabet[(value >> 18u) & 0x3fu]);
                output.push_back(alphabet[(value >> 12u) & 0x3fu]);
                output.push_back(alphabet[(value >> 6u) & 0x3fu]);
            }
            return output;
        }

#if defined(_WIN32)
        struct NativeKey final
        {
            NCRYPT_PROV_HANDLE provider{};
            NCRYPT_KEY_HANDLE key{};
            KeyProtection protection{KeyProtection::Unavailable};

            NativeKey() = default;
            NativeKey(
                const NCRYPT_PROV_HANDLE provider_value,
                const NCRYPT_KEY_HANDLE key_value,
                const KeyProtection protection_value) noexcept
                : provider{provider_value},
                  key{key_value},
                  protection{protection_value}
            {
            }
            NativeKey(const NativeKey&) = delete;
            NativeKey& operator=(const NativeKey&) = delete;
            NativeKey(NativeKey&& other) noexcept
                : provider{std::exchange(other.provider, 0u)},
                  key{std::exchange(other.key, 0u)},
                  protection{other.protection}
            {
            }
            NativeKey& operator=(NativeKey&& other) noexcept
            {
                if (this != &other)
                {
                    if (key != 0u)
                        NCryptFreeObject(key);
                    if (provider != 0u)
                        NCryptFreeObject(provider);
                    provider = std::exchange(other.provider, 0u);
                    key = std::exchange(other.key, 0u);
                    protection = other.protection;
                }
                return *this;
            }
            ~NativeKey()
            {
                if (key != 0u)
                    NCryptFreeObject(key);
                if (provider != 0u)
                    NCryptFreeObject(provider);
            }
        };

        struct ProviderChoice final
        {
            const wchar_t* name{};
            KeyProtection protection{KeyProtection::Unavailable};
        };

        constexpr std::array providers{
            ProviderChoice{
                MS_PLATFORM_CRYPTO_PROVIDER,
                KeyProtection::WindowsPlatformNonExportable
            },
            ProviderChoice{
                MS_KEY_STORAGE_PROVIDER,
                KeyProtection::WindowsSoftwareNonExportable
            }
        };

        [[nodiscard]] std::optional<NativeKey> open_existing(
            const ProviderChoice& choice)
        {
            NCRYPT_PROV_HANDLE provider = 0u;
            if (NCryptOpenStorageProvider(&provider, choice.name, 0u) != ERROR_SUCCESS)
                return std::nullopt;

            NCRYPT_KEY_HANDLE key = 0u;
            if (NCryptOpenKey(
                provider,
                &key,
                key_name.data(),
                0u,
                NCRYPT_SILENT_FLAG) != ERROR_SUCCESS)
            {
                NCryptFreeObject(provider);
                return std::nullopt;
            }

            DWORD export_policy = ~0u;
            DWORD export_size = 0u;
            DWORD usage = 0u;
            DWORD usage_size = 0u;
            const bool constrained =
                NCryptGetProperty(
                    key,
                    NCRYPT_EXPORT_POLICY_PROPERTY,
                    reinterpret_cast<PBYTE>(&export_policy),
                    sizeof(export_policy),
                    &export_size,
                    0u) == ERROR_SUCCESS
                && export_size == sizeof(export_policy)
                && export_policy == 0u
                && NCryptGetProperty(
                    key,
                    NCRYPT_KEY_USAGE_PROPERTY,
                    reinterpret_cast<PBYTE>(&usage),
                    sizeof(usage),
                    &usage_size,
                    0u) == ERROR_SUCCESS
                && usage_size == sizeof(usage)
                && (usage & NCRYPT_ALLOW_SIGNING_FLAG) != 0u;
            if (!constrained)
            {
                NCryptFreeObject(key);
                NCryptFreeObject(provider);
                return std::nullopt;
            }
            return NativeKey{provider, key, choice.protection};
        }

        [[nodiscard]] std::optional<NativeKey> create_key(
            const ProviderChoice& choice)
        {
            NCRYPT_PROV_HANDLE provider = 0u;
            if (NCryptOpenStorageProvider(&provider, choice.name, 0u) != ERROR_SUCCESS)
                return std::nullopt;

            NCRYPT_KEY_HANDLE key = 0u;
            const SECURITY_STATUS created = NCryptCreatePersistedKey(
                provider,
                &key,
                NCRYPT_ECDSA_P256_ALGORITHM,
                key_name.data(),
                0u,
                0u);
            if (created == NTE_EXISTS)
            {
                NCryptFreeObject(provider);
                return open_existing(choice);
            }
            if (created != ERROR_SUCCESS)
            {
                NCryptFreeObject(provider);
                return std::nullopt;
            }

            DWORD export_policy = 0u;
            DWORD usage = NCRYPT_ALLOW_SIGNING_FLAG;
            const bool finalized =
                NCryptSetProperty(
                    key,
                    NCRYPT_EXPORT_POLICY_PROPERTY,
                    reinterpret_cast<PBYTE>(&export_policy),
                    sizeof(export_policy),
                    0u) == ERROR_SUCCESS
                && NCryptSetProperty(
                    key,
                    NCRYPT_KEY_USAGE_PROPERTY,
                    reinterpret_cast<PBYTE>(&usage),
                    sizeof(usage),
                    0u) == ERROR_SUCCESS
                && NCryptFinalizeKey(key, NCRYPT_SILENT_FLAG) == ERROR_SUCCESS;
            if (!finalized)
            {
                NCryptDeleteKey(key, NCRYPT_SILENT_FLAG);
                NCryptFreeObject(provider);
                return std::nullopt;
            }
            return NativeKey{provider, key, choice.protection};
        }

        [[nodiscard]] std::optional<NativeKey> load_or_create_native_key()
        {
            for (const ProviderChoice& provider : providers)
            {
                if (auto key = open_existing(provider))
                    return key;
            }
            for (const ProviderChoice& provider : providers)
            {
                if (auto key = create_key(provider))
                    return key;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<NativeKey> load_native_key()
        {
            for (const ProviderChoice& provider : providers)
            {
                if (auto key = open_existing(provider))
                    return key;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<std::string> public_jwk(NCRYPT_KEY_HANDLE key)
        {
            DWORD size = 0u;
            if (NCryptExportKey(
                key,
                0u,
                BCRYPT_ECCPUBLIC_BLOB,
                nullptr,
                nullptr,
                0u,
                &size,
                0u) != ERROR_SUCCESS
                || size < sizeof(BCRYPT_ECCKEY_BLOB) + 64u)
            {
                return std::nullopt;
            }

            std::vector<unsigned char> blob(size);
            if (NCryptExportKey(
                key,
                0u,
                BCRYPT_ECCPUBLIC_BLOB,
                nullptr,
                blob.data(),
                size,
                &size,
                0u) != ERROR_SUCCESS)
            {
                return std::nullopt;
            }
            const auto* header =
                reinterpret_cast<const BCRYPT_ECCKEY_BLOB*>(blob.data());
            if (header->dwMagic != BCRYPT_ECDSA_PUBLIC_P256_MAGIC
                || header->cbKey != 32u
                || size != sizeof(BCRYPT_ECCKEY_BLOB) + 64u)
            {
                return std::nullopt;
            }
            const unsigned char* x = blob.data() + sizeof(BCRYPT_ECCKEY_BLOB);
            const unsigned char* y = x + 32u;
            return "{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\""
                + encode_base64url(x, 32u)
                + "\",\"y\":\"" + encode_base64url(y, 32u) + "\"}";
        }
#endif
    }

    std::string_view protection_name(const KeyProtection protection) noexcept
    {
        switch (protection)
        {
        case KeyProtection::WindowsPlatformNonExportable:
            return "windows_platform_non_exportable";
        case KeyProtection::WindowsSoftwareNonExportable:
            return "windows_software_non_exportable";
        case KeyProtection::LinuxTpmNonExportable:
            return "linux_tpm_non_exportable";
        case KeyProtection::LinuxOsProtectedExportable:
            return "linux_os_protected_exportable";
        case KeyProtection::Unavailable:
        default:
            return "unavailable";
        }
    }

    std::optional<std::string> load_registered_device_id()
    {
        const auto target = safe_registration_path();
        if (!target)
            return std::nullopt;
        std::ifstream input(*target, std::ios::binary);
        std::string device_id;
        if (!input || !std::getline(input, device_id) || !safe_device_id(device_id))
            return std::nullopt;
        return device_id;
    }

    bool store_registered_device_id(const std::string_view device_id)
    {
        if (!safe_device_id(device_id))
            return false;
        const auto target = safe_registration_path();
        if (!target)
            return false;
        const fs::path temporary = target->string() + ".tmp";
        std::error_code ec;
        fs::create_directories(target->parent_path(), ec);
        if (ec)
            return false;
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output.write(device_id.data(), static_cast<std::streamsize>(device_id.size()));
            output.put('\n');
            output.flush();
            if (!output)
            {
                output.close();
                fs::remove(temporary, ec);
                return false;
            }
        }
#if !defined(_WIN32)
        (void)::chmod(temporary.c_str(), S_IRUSR | S_IWUSR);
#endif
        fs::remove(*target, ec);
        ec.clear();
        fs::rename(temporary, *target, ec);
        if (ec)
        {
            fs::remove(temporary, ec);
            return false;
        }
#if defined(_WIN32)
        (void)SetFileAttributesW(target->c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
        return true;
    }

    void clear_registered_device_id() noexcept
    {
        const auto target = safe_registration_path();
        if (!target)
            return;
        std::error_code ec;
        fs::remove(*target, ec);
    }

    Identity load_or_create()
    {
#if defined(_WIN32)
        auto key = load_or_create_native_key();
        const auto jwk = key ? public_jwk(key->key) : std::nullopt;
        if (!key || !jwk)
        {
            return {
                .ready = false,
                .diagnostic =
                    "Windows could not create or reopen the non-exportable source-device signing key."
            };
        }
        return {
            .ready = true,
            .public_jwk = *jwk,
            .device_id = load_registered_device_id().value_or(std::string{}),
            .protection = key->protection,
            .diagnostic = "The source-device signing key is protected by Windows CNG."
        };
#else
        return {
            .ready = false,
            .device_id = load_registered_device_id().value_or(std::string{}),
            .protection = KeyProtection::Unavailable,
            .diagnostic =
                "This Linux build has no admitted persistent TPM or Secret Service signing provider."
        };
#endif
    }

    std::optional<std::array<unsigned char, 64>> sign_sha256(
        const std::span<const unsigned char, 32> digest)
    {
#if defined(_WIN32)
        auto key = load_native_key();
        if (!key)
            return std::nullopt;
        DWORD required = 0u;
        if (NCryptSignHash(
            key->key,
            nullptr,
            const_cast<PBYTE>(digest.data()),
            static_cast<DWORD>(digest.size()),
            nullptr,
            0u,
            &required,
            NCRYPT_SILENT_FLAG) != ERROR_SUCCESS
            || required != 64u)
        {
            return std::nullopt;
        }
        std::array<unsigned char, 64> signature{};
        if (NCryptSignHash(
            key->key,
            nullptr,
            const_cast<PBYTE>(digest.data()),
            static_cast<DWORD>(digest.size()),
            signature.data(),
            static_cast<DWORD>(signature.size()),
            &required,
            NCRYPT_SILENT_FLAG) != ERROR_SUCCESS
            || required != static_cast<DWORD>(signature.size()))
        {
            return std::nullopt;
        }
        return signature;
#else
        (void)digest;
        return std::nullopt;
#endif
    }
}
