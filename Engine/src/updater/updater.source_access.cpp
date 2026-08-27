/************************************************
 *  EPOCH - authenticated private source access *
 *                                              *
 *  SPDX-License-Identifier:                   *
 *  LicenseRef-MIT-NoSell                      *
 ***********************************************/
module;

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

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#elif defined(EPOCH_HAS_CURL)
#include <curl/curl.h>
#include <sys/stat.h>
#endif

module updater.source_access;

import epoch.version;
import updater.config;

namespace epochengine::updater
{

    namespace source_access_detail
    {
        struct SharedStatus
        {
            std::mutex mutex{};
            SourceAccessStatus status{};
            std::atomic<bool> cancel_requested{ false };
        };

        [[nodiscard]] inline SharedStatus& shared_status()
        {
            static SharedStatus state{};
            return state;
        }

        inline void publish_status(
            const SourceAccessPhase phase,
            std::string message,
            std::string user_code = {},
            std::string verification_uri = {},
            const float progress = 0.0f)
        {
            auto& shared = shared_status();
            std::scoped_lock lock{ shared.mutex };
            shared.status = SourceAccessStatus{
                .phase = phase,
                .message = std::move(message),
                .user_code = std::move(user_code),
                .verification_uri = std::move(verification_uri),
                .progress = std::clamp(progress, 0.0f, 1.0f)
            };
        }

        [[nodiscard]] inline std::string json_escape(const std::string_view text)
        {
            std::string result;
            result.reserve(text.size() + 16u);
            for (const unsigned char ch : text)
            {
                switch (ch)
                {
                case '\\': result += "\\\\"; break;
                case '"': result += "\\\""; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (ch < 0x20u)
                    {
                        constexpr char digits[] = "0123456789abcdef";
                        result += "\\u00";
                        result.push_back(digits[(ch >> 4u) & 0x0fu]);
                        result.push_back(digits[ch & 0x0fu]);
                    }
                    else
                    {
                        result.push_back(static_cast<char>(ch));
                    }
                    break;
                }
            }
            return result;
        }

        [[nodiscard]] inline std::optional<std::string> unescape_json_ascii(
            const std::string_view escaped)
        {
            std::string result;
            result.reserve(escaped.size());
            for (std::size_t index = 0; index < escaped.size(); ++index)
            {
                const char ch = escaped[index];
                if (ch != '\\')
                {
                    if (static_cast<unsigned char>(ch) < 0x20u)
                        return std::nullopt;
                    result.push_back(ch);
                    continue;
                }

                if (++index >= escaped.size())
                    return std::nullopt;
                switch (escaped[index])
                {
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case '"': result.push_back('"'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                default: return std::nullopt;
                }
            }
            return result;
        }

        [[nodiscard]] inline bool json_space(const char ch) noexcept
        {
            return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
        }

        [[nodiscard]] inline std::optional<std::size_t>
        json_member_value_start(const std::string_view document, const std::string_view field,
                                std::size_t& search) noexcept
        {
            while (search < document.size())
            {
                const std::size_t key_open = document.find('"', search);
                if (key_open == std::string_view::npos)
                    return std::nullopt;

                bool escaping = false;
                std::size_t key_close = key_open + 1u;
                for (; key_close < document.size(); ++key_close)
                {
                    const char ch = document[key_close];
                    if (escaping)
                        escaping = false;
                    else if (ch == '\\')
                        escaping = true;
                    else if (ch == '"')
                        break;
                }
                if (key_close >= document.size())
                    return std::nullopt;

                search = key_close + 1u;
                if (document.substr(key_open + 1u, key_close - key_open - 1u) != field)
                    continue;

                std::size_t value = key_close + 1u;
                while (value < document.size() && json_space(document[value]))
                    ++value;
                if (value >= document.size() || document[value] != ':')
                    continue;
                do
                    ++value;
                while (value < document.size() && json_space(document[value]));
                if (value >= document.size())
                    return std::nullopt;
                return value;
            }
            return std::nullopt;
        }

        [[nodiscard]] inline std::optional<std::string>
        json_string_once(const std::string& document, const std::string_view field)
        {
            std::optional<std::string> found;
            std::size_t search = 0u;
            while (const auto value_start = json_member_value_start(document, field, search))
            {
                if (document[*value_start] != '"')
                    return std::nullopt;

                bool escaping = false;
                std::size_t value_end = *value_start + 1u;
                for (; value_end < document.size(); ++value_end)
                {
                    const char ch = document[value_end];
                    if (escaping)
                        escaping = false;
                    else if (ch == '\\')
                        escaping = true;
                    else if (ch == '"')
                        break;
                }
                if (value_end >= document.size())
                    return std::nullopt;

                const auto value = unescape_json_ascii(std::string_view{document}.substr(
                    *value_start + 1u, value_end - *value_start - 1u));
                if (!value || found)
                    return std::nullopt;
                found = *value;
                search = value_end + 1u;
            }
            return found;
        }

        [[nodiscard]] inline std::optional<std::uint64_t>
        json_u64_once(const std::string& document, const std::string_view field)
        {
            std::optional<std::uint64_t> found;
            std::size_t search = 0u;
            while (const auto value_start = json_member_value_start(document, field, search))
            {
                std::size_t value_end = *value_start;
                while (value_end < document.size() && document[value_end] >= '0' &&
                       document[value_end] <= '9')
                {
                    ++value_end;
                }
                if (value_end == *value_start)
                    return std::nullopt;

                std::uint64_t value = 0u;
                const char* first = document.data() + *value_start;
                const char* last = document.data() + value_end;
                const auto [parsed, ec] = std::from_chars(first, last, value);
                if (ec != std::errc{} || parsed != last || found)
                    return std::nullopt;
                found = value;
                search = value_end;
            }
            return found;
        }

        [[nodiscard]] inline std::optional<std::string>
        json_object_once(const std::string& document, const std::string_view field)
        {
            const std::string key = "\"" + std::string{ field } + "\"";
            std::size_t search = 0u;
            std::optional<std::string> found;
            while ((search = document.find(key, search)) != std::string::npos)
            {
                const auto colon = document.find(':', search + key.size());
                const auto open = colon == std::string::npos
                    ? std::string::npos
                    : document.find_first_not_of(" \t\r\n", colon + 1u);
                if (open == std::string::npos || document[open] != '{')
                {
                    search += key.size();
                    continue;
                }

                bool in_string = false;
                bool escaping = false;
                int depth = 0;
                for (std::size_t index = open; index < document.size(); ++index)
                {
                    const char ch = document[index];
                    if (in_string)
                    {
                        if (escaping)
                            escaping = false;
                        else if (ch == '\\')
                            escaping = true;
                        else if (ch == '"')
                            in_string = false;
                        continue;
                    }
                    if (ch == '"')
                    {
                        in_string = true;
                        continue;
                    }
                    if (ch == '{')
                        ++depth;
                    else if (ch == '}' && --depth == 0)
                    {
                        if (found)
                            return std::nullopt;
                        found = document.substr(open, index - open + 1u);
                        search = index + 1u;
                        break;
                    }
                }
                if (search <= open)
                    return std::nullopt;
            }
            return found;
        }

        [[nodiscard]] inline std::optional<std::vector<unsigned char>> decode_base64url(
            const std::string_view encoded)
        {
            if (encoded.find('=') != std::string_view::npos)
                return std::nullopt;
            std::vector<unsigned char> decoded;
            decoded.reserve((encoded.size() * 3u) / 4u + 2u);
            std::uint32_t accumulator = 0u;
            int bits = 0;
            for (const unsigned char ch : encoded)
            {
                int value = -1;
                if (ch >= 'A' && ch <= 'Z') value = static_cast<int>(ch - 'A');
                else if (ch >= 'a' && ch <= 'z') value = 26 + static_cast<int>(ch - 'a');
                else if (ch >= '0' && ch <= '9') value = 52 + static_cast<int>(ch - '0');
                else if (ch == '-') value = 62;
                else if (ch == '_') value = 63;
                else return std::nullopt;
                accumulator = (accumulator << 6u) | static_cast<std::uint32_t>(value);
                bits += 6;
                if (bits >= 8)
                {
                    bits -= 8;
                    decoded.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xffu));
                    accumulator &= bits == 0 ? 0u : ((1u << bits) - 1u);
                }
            }
            if (bits != 0 && accumulator != 0u)
                return std::nullopt;
            return decoded;
        }

        [[nodiscard]] inline std::string encode_base64url(
            const unsigned char* data,
            const std::size_t size)
        {
            constexpr char alphabet[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            std::string encoded;
            encoded.reserve((size * 4u + 2u) / 3u);
            for (std::size_t index = 0; index < size; index += 3u)
            {
                const std::uint32_t a = data[index];
                const std::uint32_t b = index + 1u < size ? data[index + 1u] : 0u;
                const std::uint32_t c = index + 2u < size ? data[index + 2u] : 0u;
                const std::uint32_t value = (a << 16u) | (b << 8u) | c;
                encoded.push_back(alphabet[(value >> 18u) & 0x3fu]);
                encoded.push_back(alphabet[(value >> 12u) & 0x3fu]);
                if (index + 1u < size)
                    encoded.push_back(alphabet[(value >> 6u) & 0x3fu]);
                if (index + 2u < size)
                    encoded.push_back(alphabet[value & 0x3fu]);
            }
            return encoded;
        }

        [[nodiscard]] inline std::optional<std::string> random_base64url(
            const std::size_t size)
        {
            std::vector<unsigned char> bytes(size);
            if (size == 0u
                || RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
                return std::nullopt;
            return encode_base64url(bytes.data(), bytes.size());
        }

        [[nodiscard]] inline std::string hex_lower(
            const unsigned char* data,
            const std::size_t size)
        {
            constexpr char digits[] = "0123456789abcdef";
            std::string result(size * 2u, '0');
            for (std::size_t index = 0; index < size; ++index)
            {
                result[index * 2u] = digits[(data[index] >> 4u) & 0x0fu];
                result[index * 2u + 1u] = digits[data[index] & 0x0fu];
            }
            return result;
        }

        [[nodiscard]] inline std::optional<std::array<unsigned char, 32>> sha256(
            const std::string_view text)
        {
            std::array<unsigned char, 32> digest{};
            unsigned int digest_size = 0u;
            EVP_MD_CTX* context = EVP_MD_CTX_new();
            if (context == nullptr)
                return std::nullopt;
            const bool ok =
                EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1
                && EVP_DigestUpdate(context, text.data(), text.size()) == 1
                && EVP_DigestFinal_ex(context, digest.data(), &digest_size) == 1
                && digest_size == digest.size();
            EVP_MD_CTX_free(context);
            if (!ok)
                return std::nullopt;
            return digest;
        }

        [[nodiscard]] inline bool verify_ed25519(
            const std::string_view message,
            const std::string_view signature_text)
        {
            const auto public_key = decode_base64url(
                PROJECT_RELEASE_SIGNING_PUBLIC_KEY_BASE64URL);
            const auto signature = decode_base64url(signature_text);
            if (!public_key || public_key->size() != 32u
                || !signature || signature->size() != 64u)
            {
                return false;
            }
            EVP_PKEY* key = EVP_PKEY_new_raw_public_key(
                EVP_PKEY_ED25519,
                nullptr,
                public_key->data(),
                public_key->size());
            EVP_MD_CTX* context = key == nullptr ? nullptr : EVP_MD_CTX_new();
            const bool verified =
                context != nullptr
                && EVP_DigestVerifyInit(context, nullptr, nullptr, nullptr, key) == 1
                && EVP_DigestVerify(
                    context,
                    signature->data(),
                    signature->size(),
                    reinterpret_cast<const unsigned char*>(message.data()),
                    message.size()) == 1;
            EVP_MD_CTX_free(context);
            EVP_PKEY_free(key);
            return verified;
        }

        struct EphemeralP256Key
        {
            EVP_PKEY* value{};

            EphemeralP256Key() = default;
            explicit EphemeralP256Key(EVP_PKEY* key) : value{ key } {}
            EphemeralP256Key(const EphemeralP256Key&) = delete;
            EphemeralP256Key& operator=(const EphemeralP256Key&) = delete;
            EphemeralP256Key(EphemeralP256Key&& other) noexcept
                : value{ std::exchange(other.value, nullptr) }
            {
            }
            EphemeralP256Key& operator=(EphemeralP256Key&& other) noexcept
            {
                if (this != &other)
                {
                    EVP_PKEY_free(value);
                    value = std::exchange(other.value, nullptr);
                }
                return *this;
            }
            ~EphemeralP256Key()
            {
                EVP_PKEY_free(value);
            }
        };

        struct PublicP256Jwk
        {
            std::string x{};
            std::string y{};

            [[nodiscard]] std::string canonical() const
            {
                return "{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\""
                    + x + "\",\"y\":\"" + y + "\"}";
            }

            [[nodiscard]] std::string request_json() const
            {
                return "{\"kty\":\"EC\",\"crv\":\"P-256\",\"x\":\""
                    + x + "\",\"y\":\"" + y + "\"}";
            }
        };

        [[nodiscard]] inline std::optional<EphemeralP256Key> generate_p256_key()
        {
            EVP_PKEY* key = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
            if (key == nullptr)
                return std::nullopt;
            return EphemeralP256Key{ key };
        }

        [[nodiscard]] inline std::optional<PublicP256Jwk> public_jwk(
            EVP_PKEY* key)
        {
            BIGNUM* x = nullptr;
            BIGNUM* y = nullptr;
            const bool queried =
                key != nullptr
                && EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_EC_PUB_X, &x) == 1
                && EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_EC_PUB_Y, &y) == 1;
            std::array<unsigned char, 32> x_bytes{};
            std::array<unsigned char, 32> y_bytes{};
            const bool encoded =
                queried
                && BN_bn2binpad(x, x_bytes.data(), static_cast<int>(x_bytes.size()))
                    == static_cast<int>(x_bytes.size())
                && BN_bn2binpad(y, y_bytes.data(), static_cast<int>(y_bytes.size()))
                    == static_cast<int>(y_bytes.size());
            BN_free(x);
            BN_free(y);
            if (!encoded)
                return std::nullopt;
            return PublicP256Jwk{
                .x = encode_base64url(x_bytes.data(), x_bytes.size()),
                .y = encode_base64url(y_bytes.data(), y_bytes.size())
            };
        }

        [[nodiscard]] inline std::optional<EphemeralP256Key> import_public_p256(
            const PublicP256Jwk& jwk)
        {
            const auto x = decode_base64url(jwk.x);
            const auto y = decode_base64url(jwk.y);
            if (!x || !y || x->size() != 32u || y->size() != 32u)
                return std::nullopt;

            std::array<unsigned char, 65> encoded{};
            encoded[0] = 0x04u;
            std::copy(x->begin(), x->end(), encoded.begin() + 1);
            std::copy(y->begin(), y->end(), encoded.begin() + 33);

            EVP_PKEY_CTX* context = EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr);
            if (context == nullptr || EVP_PKEY_fromdata_init(context) != 1)
            {
                EVP_PKEY_CTX_free(context);
                return std::nullopt;
            }
            char group[] = "P-256";
            OSSL_PARAM params[] = {
                OSSL_PARAM_construct_utf8_string(
                    OSSL_PKEY_PARAM_GROUP_NAME,
                    group,
                    0u),
                OSSL_PARAM_construct_octet_string(
                    OSSL_PKEY_PARAM_PUB_KEY,
                    encoded.data(),
                    encoded.size()),
                OSSL_PARAM_construct_end()
            };
            EVP_PKEY* key = nullptr;
            const bool ok =
                EVP_PKEY_fromdata(context, &key, EVP_PKEY_PUBLIC_KEY, params) == 1;
            EVP_PKEY_CTX_free(context);
            if (!ok)
            {
                EVP_PKEY_free(key);
                return std::nullopt;
            }
            return EphemeralP256Key{ key };
        }

        [[nodiscard]] inline std::optional<std::vector<unsigned char>> derive_ecdh_secret(
            EVP_PKEY* private_key,
            EVP_PKEY* peer_key)
        {
            EVP_PKEY_CTX* context = EVP_PKEY_CTX_new(private_key, nullptr);
            if (context == nullptr
                || EVP_PKEY_derive_init(context) != 1
                || EVP_PKEY_derive_set_peer(context, peer_key) != 1)
            {
                EVP_PKEY_CTX_free(context);
                return std::nullopt;
            }
            std::size_t size = 0u;
            if (EVP_PKEY_derive(context, nullptr, &size) != 1 || size == 0u)
            {
                EVP_PKEY_CTX_free(context);
                return std::nullopt;
            }
            std::vector<unsigned char> secret(size);
            const bool ok = EVP_PKEY_derive(context, secret.data(), &size) == 1;
            EVP_PKEY_CTX_free(context);
            if (!ok)
                return std::nullopt;
            secret.resize(size);
            return secret;
        }

        [[nodiscard]] inline std::optional<std::array<unsigned char, 32>> hkdf_sha256(
            const std::vector<unsigned char>& secret,
            const std::vector<unsigned char>& salt,
            const std::string_view info)
        {
            EVP_KDF* algorithm = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
            EVP_KDF_CTX* context = algorithm == nullptr
                ? nullptr
                : EVP_KDF_CTX_new(algorithm);
            EVP_KDF_free(algorithm);
            if (context == nullptr)
                return std::nullopt;

            char digest_name[] = "SHA256";
            OSSL_PARAM params[] = {
                OSSL_PARAM_construct_utf8_string(
                    OSSL_KDF_PARAM_DIGEST,
                    digest_name,
                    0u),
                OSSL_PARAM_construct_octet_string(
                    OSSL_KDF_PARAM_KEY,
                    const_cast<unsigned char*>(secret.data()),
                    secret.size()),
                OSSL_PARAM_construct_octet_string(
                    OSSL_KDF_PARAM_SALT,
                    const_cast<unsigned char*>(salt.data()),
                    salt.size()),
                OSSL_PARAM_construct_octet_string(
                    OSSL_KDF_PARAM_INFO,
                    const_cast<char*>(info.data()),
                    info.size()),
                OSSL_PARAM_construct_end()
            };
            std::array<unsigned char, 32> output{};
            const bool ok =
                EVP_KDF_derive(context, output.data(), output.size(), params) == 1;
            EVP_KDF_CTX_free(context);
            if (!ok)
                return std::nullopt;
            return output;
        }

        [[nodiscard]] inline std::optional<std::vector<unsigned char>> decrypt_aes256_gcm(
            const std::vector<unsigned char>& sealed,
            const std::array<unsigned char, 32>& key,
            const std::vector<unsigned char>& nonce,
            const std::string_view aad,
            const std::size_t tag_bytes)
        {
            if (nonce.size() != 12u || tag_bytes != 16u || sealed.size() < tag_bytes)
                return std::nullopt;
            const std::size_t ciphertext_size = sealed.size() - tag_bytes;
            std::vector<unsigned char> plaintext(ciphertext_size + 16u);
            EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
            int written = 0;
            int total = 0;
            bool ok =
                context != nullptr
                && EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
                && EVP_CIPHER_CTX_ctrl(
                    context,
                    EVP_CTRL_GCM_SET_IVLEN,
                    static_cast<int>(nonce.size()),
                    nullptr) == 1
                && EVP_DecryptInit_ex(
                    context,
                    nullptr,
                    nullptr,
                    key.data(),
                    nonce.data()) == 1;
            if (ok && !aad.empty())
            {
                ok = EVP_DecryptUpdate(
                    context,
                    nullptr,
                    &written,
                    reinterpret_cast<const unsigned char*>(aad.data()),
                    static_cast<int>(aad.size())) == 1;
            }
            if (ok)
            {
                ok = EVP_DecryptUpdate(
                    context,
                    plaintext.data(),
                    &written,
                    sealed.data(),
                    static_cast<int>(ciphertext_size)) == 1;
                total = written;
            }
            if (ok)
            {
                ok = EVP_CIPHER_CTX_ctrl(
                    context,
                    EVP_CTRL_GCM_SET_TAG,
                    static_cast<int>(tag_bytes),
                    const_cast<unsigned char*>(sealed.data() + ciphertext_size)) == 1
                    && EVP_DecryptFinal_ex(context, plaintext.data() + total, &written) == 1;
                total += written;
            }
            EVP_CIPHER_CTX_free(context);
            if (!ok)
                return std::nullopt;
            plaintext.resize(static_cast<std::size_t>(total));
            return plaintext;
        }

        inline void restrict_cache_path(const std::filesystem::path& path)
        {
#if defined(_WIN32)
            const std::wstring native = path.wstring();
            const DWORD attributes = GetFileAttributesW(native.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES)
                (void)SetFileAttributesW(native.c_str(), attributes | FILE_ATTRIBUTE_HIDDEN);
#else
            std::error_code ec;
            const bool directory = std::filesystem::is_directory(path, ec);
            if (!ec)
                (void)::chmod(path.c_str(), directory ? 0700 : 0600);
#endif
        }

        [[nodiscard]] inline std::optional<std::string> random_token()
        {
            std::array<unsigned char, 16> bytes{};
            if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
                return std::nullopt;
            return encode_base64url(bytes.data(), bytes.size());
        }

        [[nodiscard]] inline bool decrypt_archive_file(
            const std::filesystem::path& encrypted_path,
            const std::filesystem::path& plaintext_path,
            const std::array<unsigned char, 32>& key,
            const std::vector<unsigned char>& nonce,
            const std::uint64_t expected_ciphertext_size,
            const std::string& expected_ciphertext_sha256,
            const std::uint64_t expected_plaintext_size,
            const std::string& expected_plaintext_sha256)
        {
            namespace fs = std::filesystem;
            std::error_code ec;
            const auto actual_size = fs::file_size(encrypted_path, ec);
            if (ec || actual_size != expected_ciphertext_size
                || actual_size < 16u || nonce.size() != 12u)
            {
                return false;
            }

            std::ifstream input(encrypted_path, std::ios::binary);
            const fs::path temporary = plaintext_path.string() + ".partial";
            fs::remove(temporary, ec);
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!input || !output)
                return false;
            restrict_cache_path(temporary);

            const std::uint64_t ciphertext_bytes = actual_size - 16u;
            input.seekg(static_cast<std::streamoff>(ciphertext_bytes), std::ios::beg);
            std::array<unsigned char, 16> tag{};
            input.read(reinterpret_cast<char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
            if (input.gcount() != static_cast<std::streamsize>(tag.size()))
            {
                output.close();
                fs::remove(temporary, ec);
                return false;
            }
            input.clear();
            input.seekg(0, std::ios::beg);

            EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
            EVP_MD_CTX* cipher_hash = EVP_MD_CTX_new();
            EVP_MD_CTX* plain_hash = EVP_MD_CTX_new();
            bool ok =
                cipher != nullptr && cipher_hash != nullptr && plain_hash != nullptr
                && EVP_DecryptInit_ex(cipher, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
                && EVP_CIPHER_CTX_ctrl(
                    cipher,
                    EVP_CTRL_GCM_SET_IVLEN,
                    static_cast<int>(nonce.size()),
                    nullptr) == 1
                && EVP_DecryptInit_ex(cipher, nullptr, nullptr, key.data(), nonce.data()) == 1
                && EVP_DigestInit_ex(cipher_hash, EVP_sha256(), nullptr) == 1
                && EVP_DigestInit_ex(plain_hash, EVP_sha256(), nullptr) == 1;

            std::array<unsigned char, 64u * 1024u> input_buffer{};
            std::array<unsigned char, 64u * 1024u + 16u> output_buffer{};
            std::uint64_t remaining = ciphertext_bytes;
            std::uint64_t plain_size = 0u;
            while (ok && remaining > 0u)
            {
                if (shared_status().cancel_requested.load(std::memory_order_acquire))
                {
                    ok = false;
                    break;
                }
                const std::size_t requested = static_cast<std::size_t>(
                    (std::min)(remaining, static_cast<std::uint64_t>(input_buffer.size())));
                input.read(
                    reinterpret_cast<char*>(input_buffer.data()),
                    static_cast<std::streamsize>(requested));
                if (input.gcount() != static_cast<std::streamsize>(requested))
                {
                    ok = false;
                    break;
                }
                int written = 0;
                ok =
                    EVP_DigestUpdate(cipher_hash, input_buffer.data(), requested) == 1
                    && EVP_DecryptUpdate(
                        cipher,
                        output_buffer.data(),
                        &written,
                        input_buffer.data(),
                        static_cast<int>(requested)) == 1;
                if (ok && written > 0)
                {
                    output.write(
                        reinterpret_cast<const char*>(output_buffer.data()),
                        written);
                    ok = static_cast<bool>(output)
                        && EVP_DigestUpdate(
                            plain_hash,
                            output_buffer.data(),
                            static_cast<std::size_t>(written)) == 1;
                    plain_size += static_cast<std::uint64_t>(written);
                }
                remaining -= requested;
                publish_status(
                    SourceAccessPhase::Decrypting,
                    "Authenticating and decrypting the authorized source archive.",
                    {},
                    {},
                    0.70f + 0.25f * static_cast<float>(
                        static_cast<double>(ciphertext_bytes - remaining)
                        / static_cast<double>((std::max<std::uint64_t>)(1u, ciphertext_bytes))));
            }

            if (ok)
                ok = EVP_DigestUpdate(cipher_hash, tag.data(), tag.size()) == 1;
            if (ok)
            {
                int final_written = 0;
                ok =
                    EVP_CIPHER_CTX_ctrl(
                        cipher,
                        EVP_CTRL_GCM_SET_TAG,
                        static_cast<int>(tag.size()),
                        tag.data()) == 1
                    && EVP_DecryptFinal_ex(
                        cipher,
                        output_buffer.data(),
                        &final_written) == 1;
                if (ok && final_written > 0)
                {
                    output.write(
                        reinterpret_cast<const char*>(output_buffer.data()),
                        final_written);
                    ok = static_cast<bool>(output)
                        && EVP_DigestUpdate(
                            plain_hash,
                            output_buffer.data(),
                            static_cast<std::size_t>(final_written)) == 1;
                    plain_size += static_cast<std::uint64_t>(final_written);
                }
            }

            std::array<unsigned char, 32> cipher_digest{};
            std::array<unsigned char, 32> plain_digest{};
            unsigned int cipher_digest_size = 0u;
            unsigned int plain_digest_size = 0u;
            if (ok)
            {
                ok =
                    EVP_DigestFinal_ex(
                        cipher_hash,
                        cipher_digest.data(),
                        &cipher_digest_size) == 1
                    && EVP_DigestFinal_ex(
                        plain_hash,
                        plain_digest.data(),
                        &plain_digest_size) == 1
                    && cipher_digest_size == cipher_digest.size()
                    && plain_digest_size == plain_digest.size();
            }
            EVP_CIPHER_CTX_free(cipher);
            EVP_MD_CTX_free(cipher_hash);
            EVP_MD_CTX_free(plain_hash);
            output.close();

            ok =
                ok
                && plain_size == expected_plaintext_size
                && hex_lower(cipher_digest.data(), cipher_digest.size())
                    == expected_ciphertext_sha256
                && hex_lower(plain_digest.data(), plain_digest.size())
                    == expected_plaintext_sha256;
            if (!ok)
            {
                fs::remove(temporary, ec);
                return false;
            }

            fs::remove(plaintext_path, ec);
            ec.clear();
            fs::rename(temporary, plaintext_path, ec);
            if (ec)
            {
                fs::remove(temporary, ec);
                return false;
            }
            restrict_cache_path(plaintext_path);
            return true;
        }

        struct HttpResponse
        {
            long status{};
            std::string body{};
            std::string error{};

            [[nodiscard]] bool ok() const noexcept
            {
                return status >= 200 && status < 300 && error.empty();
            }
        };

        [[nodiscard]] inline bool trusted_site_url(
            const std::string_view url,
            const std::string_view path_prefix)
        {
            const std::string expected =
                std::string{ EPOCH_SITE_BASE } + std::string{ path_prefix };
            return url.starts_with(expected)
                && url.find('#') == std::string_view::npos;
        }

        [[nodiscard]] inline bool trusted_site_exact_url(
            const std::string_view url,
            const std::string_view exact_path)
        {
            return url == std::string{ EPOCH_SITE_BASE } + std::string{ exact_path };
        }

#if defined(_WIN32)
        [[nodiscard]] inline std::wstring widen_utf8(const std::string_view text)
        {
            if (text.empty())
                return {};
            const int count = MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0);
            if (count <= 0)
                return {};
            std::wstring result(static_cast<std::size_t>(count), L'\0');
            if (MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                text.data(),
                static_cast<int>(text.size()),
                result.data(),
                count) != count)
            {
                return {};
            }
            return result;
        }

        struct WinHttpUrl
        {
            std::wstring host{};
            std::wstring path{};
            INTERNET_PORT port{};
        };

        [[nodiscard]] inline std::optional<WinHttpUrl> parse_https_url(
            const std::string& url)
        {
            const std::wstring wide = widen_utf8(url);
            if (wide.empty())
                return std::nullopt;
            URL_COMPONENTS components{};
            components.dwStructSize = sizeof(components);
            components.dwSchemeLength = static_cast<DWORD>(-1);
            components.dwHostNameLength = static_cast<DWORD>(-1);
            components.dwUrlPathLength = static_cast<DWORD>(-1);
            components.dwExtraInfoLength = static_cast<DWORD>(-1);
            if (!WinHttpCrackUrl(
                wide.c_str(),
                static_cast<DWORD>(wide.size()),
                0,
                &components)
                || components.nScheme != INTERNET_SCHEME_HTTPS
                || components.dwHostNameLength == 0u)
            {
                return std::nullopt;
            }
            WinHttpUrl parsed{};
            parsed.host.assign(
                components.lpszHostName,
                components.dwHostNameLength);
            parsed.path.assign(
                components.lpszUrlPath,
                components.dwUrlPathLength);
            if (components.dwExtraInfoLength > 0u)
            {
                parsed.path.append(
                    components.lpszExtraInfo,
                    components.dwExtraInfoLength);
            }
            if (parsed.path.empty())
                parsed.path = L"/";
            parsed.port = components.nPort;
            return parsed;
        }

        struct WinHttpHandles
        {
            HINTERNET session{};
            HINTERNET connection{};
            HINTERNET request{};

            WinHttpHandles() = default;
            WinHttpHandles(const WinHttpHandles&) = delete;
            WinHttpHandles& operator=(const WinHttpHandles&) = delete;
            WinHttpHandles(WinHttpHandles&& other) noexcept
                : session{ std::exchange(other.session, nullptr) },
                  connection{ std::exchange(other.connection, nullptr) },
                  request{ std::exchange(other.request, nullptr) }
            {
            }
            WinHttpHandles& operator=(WinHttpHandles&& other) noexcept
            {
                if (this != &other)
                {
                    if (request != nullptr) WinHttpCloseHandle(request);
                    if (connection != nullptr) WinHttpCloseHandle(connection);
                    if (session != nullptr) WinHttpCloseHandle(session);
                    session = std::exchange(other.session, nullptr);
                    connection = std::exchange(other.connection, nullptr);
                    request = std::exchange(other.request, nullptr);
                }
                return *this;
            }

            ~WinHttpHandles()
            {
                if (request != nullptr) WinHttpCloseHandle(request);
                if (connection != nullptr) WinHttpCloseHandle(connection);
                if (session != nullptr) WinHttpCloseHandle(session);
            }
        };

        [[nodiscard]] inline std::optional<WinHttpHandles> begin_winhttp_request(
            const std::wstring& method,
            const std::string& url)
        {
            const auto parsed = parse_https_url(url);
            if (!parsed)
                return std::nullopt;
            WinHttpHandles handles{};
            handles.session = WinHttpOpen(
                L"EpochSourceAccess/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (handles.session == nullptr)
                return std::nullopt;
            (void)WinHttpSetTimeouts(
                handles.session,
                10000,
                10000,
                30000,
                120000);
            DWORD secure_protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
            secure_protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
            (void)WinHttpSetOption(
                handles.session,
                WINHTTP_OPTION_SECURE_PROTOCOLS,
                &secure_protocols,
                sizeof(secure_protocols));
            handles.connection = WinHttpConnect(
                handles.session,
                parsed->host.c_str(),
                parsed->port,
                0);
            if (handles.connection == nullptr)
                return std::nullopt;
            handles.request = WinHttpOpenRequest(
                handles.connection,
                method.c_str(),
                parsed->path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);
            if (handles.request == nullptr)
                return std::nullopt;
            DWORD disabled_features = WINHTTP_DISABLE_REDIRECTS;
            (void)WinHttpSetOption(
                handles.request,
                WINHTTP_OPTION_DISABLE_FEATURE,
                &disabled_features,
                sizeof(disabled_features));
            return handles;
        }

        [[nodiscard]] inline bool winhttp_send(
            WinHttpHandles& handles,
            const std::string& body,
            const std::string& bearer)
        {
            std::wstring headers =
                L"Accept: application/json, application/octet-stream\r\n"
                L"Cache-Control: no-store\r\n";
            if (!body.empty())
                headers += L"Content-Type: application/json; charset=utf-8\r\n";
            if (!bearer.empty())
            {
                const std::wstring token = widen_utf8(bearer);
                if (token.empty())
                    return false;
                headers += L"Authorization: Bearer ";
                headers += token;
                headers += L"\r\n";
            }
            return WinHttpSendRequest(
                handles.request,
                headers.c_str(),
                static_cast<DWORD>(headers.size()),
                body.empty()
                    ? WINHTTP_NO_REQUEST_DATA
                    : const_cast<char*>(body.data()),
                static_cast<DWORD>(body.size()),
                static_cast<DWORD>(body.size()),
                0) != FALSE
                && WinHttpReceiveResponse(handles.request, nullptr) != FALSE;
        }

        [[nodiscard]] inline long winhttp_status(HINTERNET request)
        {
            DWORD status = 0u;
            DWORD size = sizeof(status);
            if (!WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &size,
                WINHTTP_NO_HEADER_INDEX))
            {
                return 0;
            }
            return static_cast<long>(status);
        }
#endif

#if !defined(_WIN32) && defined(EPOCH_HAS_CURL)
        struct CurlBodySink
        {
            std::string* body{};
            std::ofstream* file{};
            std::uint64_t bytes{};
            std::uint64_t limit{};
        };

        inline std::size_t curl_write(
            char* data,
            const std::size_t size,
            const std::size_t count,
            void* user)
        {
            auto& sink = *static_cast<CurlBodySink*>(user);
            const std::size_t bytes = size * count;
            if (bytes == 0u
                || sink.bytes > (std::numeric_limits<std::uint64_t>::max)() - bytes
                || (sink.limit != 0u && sink.bytes + bytes > sink.limit))
            {
                return 0u;
            }
            if (sink.body != nullptr)
                sink.body->append(data, bytes);
            else if (sink.file != nullptr)
            {
                sink.file->write(data, static_cast<std::streamsize>(bytes));
                if (!*sink.file)
                    return 0u;
            }
            sink.bytes += bytes;
            return bytes;
        }

        inline void initialize_curl()
        {
            static std::once_flag once;
            std::call_once(once, [] { (void)curl_global_init(CURL_GLOBAL_DEFAULT); });
        }
#endif

        [[nodiscard]] inline HttpResponse http_json(
            const std::string_view method,
            const std::string& url,
            const std::string& body = {},
            const std::string& bearer = {})
        {
            HttpResponse response{};
            if (!url.starts_with("https://"))
            {
                response.error = "HTTPS is required.";
                return response;
            }
#if defined(_WIN32)
            auto handles = begin_winhttp_request(widen_utf8(method), url);
            if (!handles || !winhttp_send(*handles, body, bearer))
            {
                response.error = "The HTTPS request failed.";
                return response;
            }
            response.status = winhttp_status(handles->request);
            constexpr std::size_t maximum_body = 1024u * 1024u;
            while (response.body.size() < maximum_body)
            {
                DWORD available = 0u;
                if (!WinHttpQueryDataAvailable(handles->request, &available))
                {
                    response.error = "The HTTPS response could not be read.";
                    return response;
                }
                if (available == 0u)
                    break;
                if (available > maximum_body - response.body.size())
                {
                    response.error = "The HTTPS response exceeded the metadata limit.";
                    return response;
                }
                const std::size_t offset = response.body.size();
                response.body.resize(offset + available);
                DWORD read = 0u;
                if (!WinHttpReadData(
                    handles->request,
                    response.body.data() + offset,
                    available,
                    &read))
                {
                    response.error = "The HTTPS response could not be read.";
                    return response;
                }
                response.body.resize(offset + read);
            }
#elif defined(EPOCH_HAS_CURL)
            initialize_curl();
            CURL* curl = curl_easy_init();
            if (curl == nullptr)
            {
                response.error = "The HTTPS client could not initialize.";
                return response;
            }
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Cache-Control: no-store");
            if (!body.empty())
                headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
            std::string authorization;
            if (!bearer.empty())
            {
                authorization = "Authorization: Bearer " + bearer;
                headers = curl_slist_append(headers, authorization.c_str());
            }
            CurlBodySink sink{
                .body = &response.body,
                .limit = 1024u * 1024u
            };
            const std::string method_text{ method };
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_text.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
            if (!body.empty())
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
            }
            const CURLcode code = curl_easy_perform(curl);
            (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
            if (code != CURLE_OK)
                response.error = "The HTTPS request failed.";
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
#else
            (void)method;
            (void)body;
            (void)bearer;
            response.error = "This build has no private-source HTTPS transport.";
#endif
            return response;
        }

        [[nodiscard]] inline HttpResponse http_download(
            const std::string& url,
            const std::string& bearer,
            const std::filesystem::path& output_path,
            const std::uint64_t maximum_size)
        {
            HttpResponse response{};
            if (!url.starts_with("https://") || bearer.empty())
            {
                response.error = "Authorized HTTPS download parameters are incomplete.";
                return response;
            }
            std::error_code ec;
            std::filesystem::create_directories(output_path.parent_path(), ec);
            if (ec)
            {
                response.error = "The protected source cache could not be prepared.";
                return response;
            }
            restrict_cache_path(output_path.parent_path());
            std::filesystem::remove(output_path, ec);
#if defined(_WIN32)
            auto handles = begin_winhttp_request(L"GET", url);
            if (!handles || !winhttp_send(*handles, {}, bearer))
            {
                response.error = "The authorized source download failed.";
                return response;
            }
            response.status = winhttp_status(handles->request);
            if (response.status < 200 || response.status >= 300)
                return response;
            std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                response.error = "The encrypted source cache file could not be created.";
                return response;
            }
            restrict_cache_path(output_path);
            std::uint64_t total = 0u;
            while (true)
            {
                DWORD available = 0u;
                if (!WinHttpQueryDataAvailable(handles->request, &available))
                {
                    response.error = "The encrypted source response could not be read.";
                    break;
                }
                if (available == 0u)
                    break;
                if (total > maximum_size || available > maximum_size - total)
                {
                    response.error = "The encrypted source response exceeded its signed size bound.";
                    break;
                }
                const DWORD requested = (std::min<DWORD>)(available, 64u * 1024u);
                std::vector<unsigned char> buffer(requested);
                DWORD read = 0u;
                if (!WinHttpReadData(
                    handles->request,
                    buffer.data(),
                    requested,
                    &read))
                {
                    response.error = "The encrypted source response could not be read.";
                    break;
                }
                output.write(
                    reinterpret_cast<const char*>(buffer.data()),
                    read);
                if (!output)
                {
                    response.error = "The encrypted source cache write failed.";
                    break;
                }
                total += read;
                publish_status(
                    SourceAccessPhase::Downloading,
                    "Downloading the encrypted authorized source archive.",
                    {},
                    {},
                    0.40f + 0.25f * static_cast<float>(
                        static_cast<double>(total)
                        / static_cast<double>((std::max<std::uint64_t>)(1u, maximum_size))));
                if (shared_status().cancel_requested.load(std::memory_order_acquire))
                {
                    response.error = "Source access was cancelled.";
                    break;
                }
            }
            output.close();
#elif defined(EPOCH_HAS_CURL)
            initialize_curl();
            std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
            CURL* curl = output ? curl_easy_init() : nullptr;
            if (curl == nullptr)
            {
                response.error = "The encrypted source cache or HTTPS client could not initialize.";
                return response;
            }
            restrict_cache_path(output_path);
            curl_slist* headers = nullptr;
            const std::string authorization = "Authorization: Bearer " + bearer;
            headers = curl_slist_append(headers, "Accept: application/octet-stream");
            headers = curl_slist_append(headers, "Cache-Control: no-store");
            headers = curl_slist_append(headers, authorization.c_str());
            CurlBodySink sink{
                .file = &output,
                .limit = maximum_size
            };
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
            const CURLcode code = curl_easy_perform(curl);
            (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
            if (code != CURLE_OK)
                response.error = "The authorized source download failed.";
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            output.close();
#else
            (void)maximum_size;
            response.error = "This build has no private-source HTTPS transport.";
#endif
            if (!response.ok())
                std::filesystem::remove(output_path, ec);
            return response;
        }

        inline bool open_verification_uri(const std::string& url)
        {
#if defined(_WIN32)
            const std::wstring wide = widen_utf8(url);
            if (wide.empty())
                return false;
            const auto result = reinterpret_cast<std::intptr_t>(
                ShellExecuteW(
                    nullptr,
                    L"open",
                    wide.c_str(),
                    nullptr,
                    nullptr,
                    SW_SHOWNORMAL));
            return result > 32;
#else
            (void)url;
            return false;
#endif
        }

        struct PrivateSourceManifest
        {
            std::string artifact_id{};
            std::string source_version{};
            std::string commit{};
            std::string archive_format{};
            std::string download_url{};
            std::string ciphertext_sha256{};
            std::uint64_t ciphertext_size{};
            std::string plaintext_sha256{};
            std::uint64_t plaintext_size{};
            std::vector<unsigned char> archive_nonce{};
            PublicP256Jwk server_jwk{};
            std::vector<unsigned char> wrap_salt{};
            std::vector<unsigned char> wrap_nonce{};
            std::vector<unsigned char> wrapped_dek{};
        };

        [[nodiscard]] inline bool lowercase_hex(
            const std::string_view text,
            const std::size_t size)
        {
            return text.size() == size
                && std::all_of(text.begin(), text.end(), [](const char ch)
                {
                    return (ch >= '0' && ch <= '9')
                        || (ch >= 'a' && ch <= 'f');
                });
        }

        [[nodiscard]] inline bool safe_identifier(const std::string_view text)
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

        [[nodiscard]] inline std::optional<PrivateSourceManifest> parse_manifest(
            const std::string& document,
            const std::string& expected_client_jwk_hash,
            const std::string& expected_download_url)
        {
            const auto canonical_payload = json_string_once(document, "canonical_payload");
            const auto signature = json_object_once(document, "signature");
            if (!canonical_payload || !signature)
                return std::nullopt;
            const auto algorithm = json_string_once(*signature, "algorithm");
            const auto key_id = json_string_once(*signature, "key_id");
            const auto signature_value = json_string_once(*signature, "value");
            if (!algorithm || !key_id || !signature_value
                || *algorithm != PROJECT_RELEASE_SIGNING_ALGORITHM
                || *key_id != PROJECT_RELEASE_SIGNING_KEY_ID
                || !verify_ed25519(*canonical_payload, *signature_value))
                return std::nullopt;

            const auto schema = json_string_once(*canonical_payload, "schema");
            const auto artifact_id = json_string_once(*canonical_payload, "artifact_id");
            const auto source_version = json_string_once(*canonical_payload, "source_version");
            const auto commit = json_string_once(*canonical_payload, "commit");
            const auto archive_format = json_string_once(*canonical_payload, "archive_format");
            const auto download_url = json_string_once(*canonical_payload, "download_url");
            const auto ciphertext_sha256 = json_string_once(*canonical_payload, "ciphertext_sha256");
            const auto ciphertext_size = json_u64_once(*canonical_payload, "ciphertext_size");
            const auto plaintext_sha256 = json_string_once(*canonical_payload, "plaintext_sha256");
            const auto plaintext_size = json_u64_once(*canonical_payload, "plaintext_size");
            const auto archive_aead = json_object_once(*canonical_payload, "archive_aead");
            const auto key_wrap = json_object_once(*canonical_payload, "key_wrap");
            if (!schema || !artifact_id || !source_version || !commit
                || !archive_format || !download_url || !ciphertext_sha256
                || !ciphertext_size || !plaintext_sha256 || !plaintext_size
                || !archive_aead || !key_wrap)
                return std::nullopt;

            const auto archive_algorithm = json_string_once(*archive_aead, "algorithm");
            const auto archive_nonce_text = json_string_once(*archive_aead, "nonce");
            const auto archive_tag_bytes = json_u64_once(*archive_aead, "tag_bytes");
            const auto archive_layout = json_string_once(*archive_aead, "layout");
            const auto archive_nonce = archive_nonce_text
                ? decode_base64url(*archive_nonce_text) : std::nullopt;

            const auto curve = json_string_once(*key_wrap, "curve");
            const auto kdf = json_string_once(*key_wrap, "kdf");
            const auto info = json_string_once(*key_wrap, "info");
            const auto wrap_salt_text = json_string_once(*key_wrap, "salt");
            const auto server_jwk_object = json_object_once(*key_wrap, "server_ephemeral_p256_jwk");
            const auto client_jwk_hash = json_string_once(*key_wrap, "client_ephemeral_p256_jwk_sha256");
            const auto wrap_algorithm = json_string_once(*key_wrap, "algorithm");
            const auto wrap_nonce_text = json_string_once(*key_wrap, "nonce");
            const auto wrap_tag_bytes = json_u64_once(*key_wrap, "tag_bytes");
            const auto wrap_layout = json_string_once(*key_wrap, "layout");
            const auto aad_format = json_string_once(*key_wrap, "aad_format");
            const auto wrapped_dek_text = json_string_once(*key_wrap, "wrapped_dek");
            if (!archive_algorithm || !archive_nonce || !archive_tag_bytes
                || !archive_layout || !curve || !kdf || !info || !wrap_salt_text
                || !server_jwk_object || !client_jwk_hash || !wrap_algorithm
                || !wrap_nonce_text || !wrap_tag_bytes || !wrap_layout
                || !aad_format || !wrapped_dek_text)
                return std::nullopt;

            const auto server_kty = json_string_once(*server_jwk_object, "kty");
            const auto server_curve = json_string_once(*server_jwk_object, "crv");
            const auto server_x = json_string_once(*server_jwk_object, "x");
            const auto server_y = json_string_once(*server_jwk_object, "y");
            const auto wrap_salt = decode_base64url(*wrap_salt_text);
            const auto wrap_nonce = decode_base64url(*wrap_nonce_text);
            const auto wrapped_dek = decode_base64url(*wrapped_dek_text);

            constexpr std::uint64_t maximum_archive_size = 2ull * 1024ull * 1024ull * 1024ull;
            const bool archive_format_ok =
#if defined(_WIN32)
                *archive_format == "zip";
#else
                *archive_format == "tar.gz";
#endif
            const std::string expected_aad_format =
                std::string{ PROJECT_PRIVATE_SOURCE_WRAP_AAD_PREFIX }
                + "\\n<artifact_id>\\n<commit>";
            if (*schema != PROJECT_PRIVATE_SOURCE_MANIFEST_SCHEMA
                || !safe_identifier(*artifact_id)
                || !safe_identifier(*source_version)
                || !lowercase_hex(*commit, 40u)
                || !archive_format_ok
                || *download_url != expected_download_url
                || !trusted_site_url(*download_url, "/api/private/epoch-engine/device/download/")
                || !lowercase_hex(*ciphertext_sha256, 64u)
                || !lowercase_hex(*plaintext_sha256, 64u)
                || *ciphertext_size < 17u || *ciphertext_size > maximum_archive_size
                || *plaintext_size == 0u || *plaintext_size > maximum_archive_size
                || *archive_algorithm != "AES-256-GCM"
                || archive_nonce->size() != 12u || *archive_tag_bytes != 16u
                || *archive_layout != "ciphertext-tag"
                || *curve != "P-256" || *kdf != "HKDF-SHA256"
                || *info != PROJECT_PRIVATE_SOURCE_WRAP_INFO
                || !wrap_salt || wrap_salt->size() != 32u
                || !server_kty || *server_kty != "EC"
                || !server_curve || *server_curve != "P-256"
                || !server_x || !server_y
                || *client_jwk_hash != expected_client_jwk_hash
                || *wrap_algorithm != "AES-256-GCM"
                || !wrap_nonce || wrap_nonce->size() != 12u
                || *wrap_tag_bytes != 16u || *wrap_layout != "ciphertext-tag"
                || *aad_format != expected_aad_format
                || !wrapped_dek || wrapped_dek->size() != 48u)
                return std::nullopt;

            return PrivateSourceManifest{
                .artifact_id = *artifact_id,
                .source_version = *source_version,
                .commit = *commit,
                .archive_format = *archive_format,
                .download_url = *download_url,
                .ciphertext_sha256 = *ciphertext_sha256,
                .ciphertext_size = *ciphertext_size,
                .plaintext_sha256 = *plaintext_sha256,
                .plaintext_size = *plaintext_size,
                .archive_nonce = *archive_nonce,
                .server_jwk = PublicP256Jwk{ .x = *server_x, .y = *server_y },
                .wrap_salt = *wrap_salt,
                .wrap_nonce = *wrap_nonce,
                .wrapped_dek = *wrapped_dek
            };
        }

        [[nodiscard]] inline std::optional<std::array<unsigned char, 32>> unwrap_dek(
            EVP_PKEY* client_private_key,
            const PrivateSourceManifest& manifest)
        {
            const auto server_key = import_public_p256(manifest.server_jwk);
            if (!server_key)
                return std::nullopt;
            auto shared_secret = derive_ecdh_secret(client_private_key, server_key->value);
            if (!shared_secret)
                return std::nullopt;
            auto wrap_key = hkdf_sha256(
                *shared_secret,
                manifest.wrap_salt,
                PROJECT_PRIVATE_SOURCE_WRAP_INFO);
            OPENSSL_cleanse(shared_secret->data(), shared_secret->size());
            if (!wrap_key)
                return std::nullopt;
            const std::string aad =
                std::string{ PROJECT_PRIVATE_SOURCE_WRAP_AAD_PREFIX }
                + "\n" + manifest.artifact_id
                + "\n" + manifest.commit;
            auto dek = decrypt_aes256_gcm(
                manifest.wrapped_dek,
                *wrap_key,
                manifest.wrap_nonce,
                aad,
                16u);
            OPENSSL_cleanse(wrap_key->data(), wrap_key->size());
            if (!dek || dek->size() != 32u)
                return std::nullopt;
            std::array<unsigned char, 32> result{};
            std::copy(dek->begin(), dek->end(), result.begin());
            OPENSSL_cleanse(dek->data(), dek->size());
            return result;
        }

        [[nodiscard]] inline bool wait_with_cancel(
            const std::chrono::seconds duration)
        {
            const auto deadline = std::chrono::steady_clock::now() + duration;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (shared_status().cancel_requested.load(std::memory_order_acquire))
                    return false;
                std::this_thread::sleep_for(std::chrono::milliseconds{ 200 });
            }
            return true;
        }

        [[nodiscard]] inline PreparedSourceArchive source_failure(
            const std::filesystem::path& cleanup_root,
            std::string message,
            const bool cancelled = false)
        {
            std::error_code ec;
            if (!cleanup_root.empty())
                std::filesystem::remove_all(cleanup_root, ec);
            publish_status(
                cancelled ? SourceAccessPhase::Cancelled : SourceAccessPhase::Failed,
                message);
            return PreparedSourceArchive{
                .ok = false,
                .message = std::move(message)
            };
        }

        struct AuthorizedSession
        {
            std::string access_token{};
            std::string manifest_url{};
            std::string download_url{};
            std::string message{};
            bool cancelled{};
        };

        [[nodiscard]] constexpr std::string_view source_platform() noexcept
        {
#if defined(_WIN32)
            return "windows-x64";
#else
            return "linux-x64";
#endif
        }

        [[nodiscard]] inline std::string registered_challenge_body(
            const std::string_view device_id,
            const std::string_view client_nonce,
            const PublicP256Jwk& client_jwk)
        {
            return
                "{\"schema\":\"epoch-source-device-auth/v1\""
                ",\"device_id\":\"" + json_escape(device_id)
                + "\",\"client_version\":\"" + json_escape(GetEngineVersionString())
                + "\",\"platform\":\"" + std::string{ source_platform() }
                + "\",\"artifact_intent\":\"latest-source\""
                + ",\"client_nonce\":\"" + std::string{ client_nonce }
                + "\",\"client_ephemeral_p256_jwk\":"
                + client_jwk.request_json() + "}";
        }

        [[nodiscard]] inline std::string device_auth_signing_payload(
            const std::string_view device_id,
            const std::string_view challenge_id,
            const std::string_view challenge_nonce,
            const std::string_view request_sha256)
        {
            return
                "EPOCH_SOURCE_DEVICE_AUTH_V1\n"
                + std::string{ device_id } + "\n"
                + std::string{ challenge_id } + "\n"
                + std::string{ challenge_nonce } + "\n"
                + std::string{ request_sha256 } + "\n";
        }

        [[nodiscard]] inline std::optional<std::string> device_enroll_signing_payload(
            const std::string_view device_code,
            const std::string_view client_nonce,
            const std::string_view signing_jwk,
            const std::string_view ephemeral_jwk)
        {
            const auto device_code_digest = sha256(device_code);
            const auto signing_jwk_digest = sha256(signing_jwk);
            const auto ephemeral_jwk_digest = sha256(ephemeral_jwk);
            if (!device_code_digest || !signing_jwk_digest || !ephemeral_jwk_digest)
                return std::nullopt;
            return
                "EPOCH_SOURCE_DEVICE_ENROLL_V1\n"
                + encode_base64url(
                    device_code_digest->data(),
                    device_code_digest->size()) + "\n"
                + std::string{ client_nonce } + "\n"
                + encode_base64url(
                    signing_jwk_digest->data(),
                    signing_jwk_digest->size()) + "\n"
                + encode_base64url(
                    ephemeral_jwk_digest->data(),
                    ephemeral_jwk_digest->size()) + "\n";
        }

        [[nodiscard]] inline AuthorizedSession parse_authorized_session(
            const HttpResponse& token,
            const bool require_registered_device = false)
        {
            const auto token_type = json_string_once(token.body, "token_type");
            const auto token_value = json_string_once(token.body, "access_token");
            const auto token_manifest = json_string_once(token.body, "manifest_url");
            const auto token_download = json_string_once(token.body, "download_url");
            const auto token_expires = json_u64_once(token.body, "expires_in");
            const auto registered_device_id = json_string_once(token.body, "device_id");
            if (!token_type || *token_type != "Bearer"
                || !token_value || token_value->size() < 40u || token_value->size() > 256u
                || !token_manifest || !token_download || !token_expires
                || *token_expires == 0u || *token_expires > 600u
                || (require_registered_device && !registered_device_id)
                || !trusted_site_url(
                    *token_manifest,
                    "/api/private/epoch-engine/device/manifest")
                || !trusted_site_url(
                    *token_download,
                    "/api/private/epoch-engine/device/download/")
                || (registered_device_id && !safe_identifier(*registered_device_id)))
            {
                return {
                    .message =
                        "The Epoch Site returned an invalid private source token response."
                };
            }
            if (require_registered_device && registered_device_id
                && !device_identity::store_registered_device_id(*registered_device_id))
            {
                return {
                    .message =
                        "Epoch authorized this device but could not save its "
                        "nonsecret registration ID."
                };
            }
            return {
                .access_token = *token_value,
                .manifest_url = *token_manifest,
                .download_url = *token_download
            };
        }

        [[nodiscard]] inline bool registration_rejected(
            const std::optional<std::string>& error)
        {
            return error
                && (*error == "invalid_device"
                    || *error == "unknown_device"
                    || *error == "device_not_found"
                    || *error == "device_revoked"
                    || *error == "device_expired");
        }

        [[nodiscard]] inline AuthorizedSession authorize_registered_device(
            const device_identity::Identity& identity,
            const PublicP256Jwk& client_jwk)
        {
            constexpr std::string_view schema = "epoch-source-device-auth/v1";
            const auto client_nonce = random_base64url(32u);
            if (!identity.ready || identity.device_id.empty() || !client_nonce)
            {
                return {
                    .message =
                        "The protected source-device registration is unavailable. "
                        "Use Pair This Device before requesting source again."
                };
            }
            const std::string challenge_body = registered_challenge_body(
                identity.device_id,
                *client_nonce,
                client_jwk);
            const auto request_digest = sha256(challenge_body);
            if (!request_digest)
            {
                return {
                    .message =
                        "Epoch could not hash the protected device challenge request."
                };
            }
            const std::string request_sha256 = encode_base64url(
                request_digest->data(),
                request_digest->size());
            const HttpResponse challenge = http_json(
                "POST",
                PROJECT_SOURCE_DEVICE_CHALLENGE_URL(),
                challenge_body);
            if (!challenge.ok())
            {
                const auto error = json_string_once(challenge.body, "error");
                if (registration_rejected(error))
                {
                    return {
                        .message =
                            "This source-device registration is no longer active. "
                            "Use Pair This Device to authorize it again."
                    };
                }
                return {
                    .message =
                        "The Epoch Site could not issue a protected device challenge."
                };
            }
            const auto response_schema = json_string_once(challenge.body, "schema");
            const auto challenge_id = json_string_once(challenge.body, "challenge_id");
            const auto challenge_nonce = json_string_once(
                challenge.body,
                "challenge_nonce");
            const auto echoed_request_hash = json_string_once(
                challenge.body,
                "request_sha256");
            const auto expires_in = json_u64_once(challenge.body, "expires_in");
            const auto challenge_nonce_bytes = challenge_nonce
                ? decode_base64url(*challenge_nonce)
                : std::nullopt;
            if (!response_schema || *response_schema != schema
                || !challenge_id || !safe_identifier(*challenge_id)
                || !challenge_nonce || !challenge_nonce_bytes
                || challenge_nonce_bytes->size() < 16u
                || challenge_nonce_bytes->size() > 64u
                || !echoed_request_hash || *echoed_request_hash != request_sha256
                || !expires_in || *expires_in == 0u || *expires_in > 60u)
            {
                return {
                    .message =
                        "The Epoch Site returned invalid protected "
                        "device-challenge metadata."
                };
            }
            const std::string signing_payload = device_auth_signing_payload(
                identity.device_id,
                *challenge_id,
                *challenge_nonce,
                request_sha256);
            const auto signing_digest = sha256(signing_payload);
            const auto signature = signing_digest
                ? device_identity::sign_sha256(*signing_digest)
                : std::nullopt;
            if (!signature)
            {
                return {
                    .message =
                        "The protected source-device key could not sign the server "
                        "challenge. Use Pair This Device if the OS key was lost."
                };
            }
            publish_status(
                SourceAccessPhase::ExchangingToken,
                "Authorizing private source with this device's protected signing key.",
                {},
                {},
                0.18f);
            const std::string token_body =
                "{\"schema\":\"" + std::string{ schema }
                + "\",\"device_id\":\"" + json_escape(identity.device_id)
                + "\",\"challenge_id\":\"" + json_escape(*challenge_id)
                + "\",\"signature_algorithm\":\"ECDSA-P256-SHA256\""
                + ",\"signature\":\""
                + encode_base64url(signature->data(), signature->size()) + "\"}";
            const HttpResponse token = http_json(
                "POST",
                PROJECT_SOURCE_DEVICE_TOKEN_URL(),
                token_body);
            if (token.ok())
                return parse_authorized_session(token);
            const auto error = json_string_once(token.body, "error");
            if (registration_rejected(error))
            {
                return {
                    .message =
                        "This source-device registration is no longer active. "
                        "Use Pair This Device to authorize it again."
                };
            }
            if (error && *error == "expired_challenge")
            {
                return {
                    .message =
                        "The protected device challenge expired. "
                        "Request source again to retry."
                };
            }
            if (error && *error == "invalid_signature")
            {
                return {
                    .message =
                        "The Epoch Site rejected this device's signature. "
                        "Use Pair This Device if its protected key changed."
                };
            }
            return {
                .message =
                    "The Epoch Site rejected the protected device authorization exchange."
            };
        }

        [[nodiscard]] inline AuthorizedSession authorize_device(
            const PublicP256Jwk& client_jwk,
            const device_identity::Identity& identity)
        {
            const auto client_nonce = identity.ready
                ? random_base64url(32u)
                : std::optional<std::string>{};
            if (identity.ready && !client_nonce)
            {
                return {
                    .message =
                        "Private source enrollment could not create a client nonce."
                };
            }
            const std::string legacy_start_body =
                "{\"client_version\":\"" + json_escape(GetEngineVersionString())
                + "\",\"platform\":\"" + std::string{ source_platform() }
                + "\",\"client_ephemeral_p256_jwk\":"
                + client_jwk.request_json()
                + "}";
            const std::string start_body = identity.ready
                ? legacy_start_body.substr(0u, legacy_start_body.size() - 1u)
                    + ",\"device_signing_p256_jwk\":" + identity.public_jwk
                    + ",\"device_label\":\"Epoch source device\""
                    + ",\"key_protection\":\""
                    + std::string{
                        device_identity::protection_name(identity.protection)
                    }
                    + "\",\"client_nonce\":\"" + *client_nonce + "\"}"
                : legacy_start_body;
            HttpResponse start = http_json(
                "POST",
                PROJECT_SOURCE_DEVICE_START_URL(),
                start_body);
            if (!start.ok() && identity.ready && start.status == 400)
                start = http_json(
                    "POST",
                    PROJECT_SOURCE_DEVICE_START_URL(),
                    legacy_start_body);
            if (!start.ok())
                return { .message = "Private source authorization could not start on the Epoch Site." };

            const auto device_code = json_string_once(start.body, "device_code");
            const auto user_code = json_string_once(start.body, "user_code");
            const auto verification_uri = json_string_once(start.body, "verification_uri");
            const auto verification_uri_complete = json_string_once(start.body, "verification_uri_complete");
            const auto expires_in = json_u64_once(start.body, "expires_in");
            const auto interval = json_u64_once(start.body, "interval");
            const auto enrollment_schema = json_string_once(
                start.body, "device_enrollment_schema");
            if (!device_code || !user_code || !verification_uri
                || !verification_uri_complete || !expires_in || !interval
                || device_code->size() < 40u || device_code->size() > 128u
                || user_code->size() < 4u || user_code->size() > 24u
                || *expires_in < 60u || *expires_in > 900u
                || *interval < 1u || *interval > 30u
                || !trusted_site_exact_url(*verification_uri, "/private/epoch-engine/device")
                || *verification_uri_complete != *verification_uri
                || (enrollment_schema
                    && *enrollment_schema != "epoch-source-device-enroll/v1"))
            {
                return { .message = "The Epoch Site returned invalid device-authorization metadata." };
            }

            const bool enrollment_supported =
                identity.ready && enrollment_schema
                && *enrollment_schema == "epoch-source-device-enroll/v1";
            std::string enrollment_token_extension;
            if (enrollment_supported)
            {
                const auto enrollment_payload = device_enroll_signing_payload(
                    *device_code,
                    *client_nonce,
                    identity.public_jwk,
                    client_jwk.canonical());
                if (!enrollment_payload)
                {
                    return {
                        .message =
                            "Private source enrollment evidence could not be hashed."
                    };
                }
                const auto enrollment_digest = sha256(*enrollment_payload);
                const auto enrollment_signature = enrollment_digest
                    ? device_identity::sign_sha256(*enrollment_digest)
                    : std::nullopt;
                if (!enrollment_signature)
                {
                    return {
                        .message =
                            "The protected device key could not sign its enrollment proof."
                    };
                }
                enrollment_token_extension =
                    ",\"enrollment_schema\":\"epoch-source-device-enroll/v1\""
                    ",\"enrollment_signature_algorithm\":\"ECDSA-P256-SHA256\""
                    ",\"enrollment_signature\":\""
                    + encode_base64url(
                        enrollment_signature->data(),
                        enrollment_signature->size())
                    + "\"";
            }

            publish_status(
                SourceAccessPhase::AwaitingApproval,
                "Approve private source access in the browser. Code: " + *user_code,
                *user_code,
                *verification_uri,
                0.10f);
            (void)open_verification_uri(*verification_uri);

            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds{ *expires_in };
            std::uint64_t poll_interval = *interval;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (!wait_with_cancel(std::chrono::seconds{ poll_interval }))
                {
                    return {
                        .message = "Private source authorization was cancelled.",
                        .cancelled = true
                    };
                }
                publish_status(
                    SourceAccessPhase::ExchangingToken,
                    "Waiting for approved private source authorization. Code: " + *user_code,
                    *user_code,
                    *verification_uri,
                    0.18f);
                const std::string token_body =
                    "{\"device_code\":\"" + json_escape(*device_code)
                    + "\"" + enrollment_token_extension + "}";
                const HttpResponse token = http_json(
                    "POST",
                    PROJECT_SOURCE_DEVICE_TOKEN_URL(),
                    token_body);
                if (token.ok())
                    return parse_authorized_session(token, enrollment_supported);

                const auto error = json_string_once(token.body, "error");
                if (error && *error == "authorization_pending")
                    continue;
                if (error && *error == "slow_down")
                {
                    poll_interval = (std::min<std::uint64_t>)(30u, poll_interval + 5u);
                    continue;
                }
                if (error && *error == "expired_token")
                    return { .message = "Private source authorization expired before approval." };
                if (error && *error == "access_denied")
                    return { .message = "Private source authorization was denied." };
                return { .message = "The Epoch Site rejected the private source authorization exchange." };
            }
            return { .message = "Private source authorization expired before approval." };
        }
    }

    SourceAccessStatus private_source_access_status()
    {
        auto& shared = source_access_detail::shared_status();
        std::scoped_lock lock{ shared.mutex };
        return shared.status;
    }

    void cancel_private_source_access() noexcept
    {
        source_access_detail::shared_status().cancel_requested.store(
            true,
            std::memory_order_release);
    }

    bool private_source_access_enabled() noexcept
    {
        return AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED;
    }

    bool private_source_device_registered() noexcept
    {
        return device_identity::load_registered_device_id().has_value();
    }

    void reset_private_source_device_registration() noexcept
    {
        device_identity::clear_registered_device_id();
    }

    PreparedSourceArchive acquire_private_source_archive(
        const std::filesystem::path& protected_cache_root)
    {
        using namespace source_access_detail;
        if (!AUTHORIZED_SOURCE_DISTRIBUTION_ENABLED)
        {
            return source_failure(
                {},
                "This Epoch build was compiled with binary-only distribution policy.");
        }

        auto& shared = shared_status();
        shared.cancel_requested.store(false, std::memory_order_release);
        publish_status(
            SourceAccessPhase::Starting,
            "Preparing an ephemeral source authorization key.");
        const device_identity::Identity identity = device_identity::load_or_create();
        if (!identity.ready && !identity.device_id.empty())
        {
            return source_failure(
                {},
                "The registered source device's protected key is unavailable. "
                "Use Pair This Device before requesting source again.");
        }
        auto client_key = generate_p256_key();
        const auto client_jwk = client_key ? public_jwk(client_key->value) : std::nullopt;
        const auto client_jwk_digest = client_jwk
            ? sha256(client_jwk->canonical()) : std::nullopt;
        if (!client_key || !client_jwk || !client_jwk_digest)
        {
            return source_failure(
                {},
                "Private source authorization could not create an ephemeral P-256 key.");
        }
        const std::string client_jwk_hash = hex_lower(
            client_jwk_digest->data(),
            client_jwk_digest->size());

        AuthorizedSession session;
        if (identity.ready && !identity.device_id.empty())
        {
            publish_status(
                SourceAccessPhase::Starting,
                "Requesting a short-lived challenge for this registered device.",
                {},
                {},
                0.08f);
            session = authorize_registered_device(identity, *client_jwk);
        }
        else
            session = authorize_device(*client_jwk, identity);
        if (session.access_token.empty())
            return source_failure({}, std::move(session.message), session.cancelled);

        publish_status(
            SourceAccessPhase::FetchingManifest,
            "Verifying the signed private source manifest.",
            {},
            {},
            0.28f);
        const HttpResponse manifest_response =
            http_json("GET", session.manifest_url, {}, session.access_token);
        const auto manifest = manifest_response.ok()
            ? parse_manifest(
                manifest_response.body,
                client_jwk_hash,
                session.download_url)
            : std::nullopt;
        if (!manifest)
        {
            OPENSSL_cleanse(session.access_token.data(), session.access_token.size());
            return source_failure(
                {},
                "The private source manifest or its Ed25519 signature was invalid.");
        }
        auto dek = unwrap_dek(client_key->value, *manifest);
        if (!dek)
        {
            OPENSSL_cleanse(session.access_token.data(), session.access_token.size());
            return source_failure(
                {},
                "The private source archive key could not be unwrapped for this process.");
        }

        const auto token = random_token();
        if (!token)
        {
            OPENSSL_cleanse(session.access_token.data(), session.access_token.size());
            OPENSSL_cleanse(dek->data(), dek->size());
            return source_failure(
                {},
                "The protected source cache could not create a random run identity.");
        }
        const std::filesystem::path cleanup_root =
            protected_cache_root / ".source_access" / *token;
        std::error_code ec;
        std::filesystem::create_directories(cleanup_root, ec);
        if (ec)
        {
            OPENSSL_cleanse(session.access_token.data(), session.access_token.size());
            OPENSSL_cleanse(dek->data(), dek->size());
            return source_failure(
                cleanup_root,
                "The protected source cache directory could not be created.");
        }
        restrict_cache_path(cleanup_root.parent_path());
        restrict_cache_path(cleanup_root);
        const std::string extension =
            manifest->archive_format == "zip" ? ".zip" : ".tar.gz";
        const std::filesystem::path encrypted_path =
            cleanup_root / ("source_snapshot" + extension + ".aes256gcm");
        const std::filesystem::path plaintext_path =
            cleanup_root / ("source_snapshot" + extension);

        publish_status(
            SourceAccessPhase::Downloading,
            "Downloading the encrypted authorized source archive.",
            {},
            {},
            0.40f);
        const HttpResponse download = http_download(
            manifest->download_url,
            session.access_token,
            encrypted_path,
            manifest->ciphertext_size);
        OPENSSL_cleanse(session.access_token.data(), session.access_token.size());
        if (!download.ok())
        {
            OPENSSL_cleanse(dek->data(), dek->size());
            return source_failure(
                cleanup_root,
                "The encrypted authorized source archive download failed.",
                shared.cancel_requested.load(std::memory_order_acquire));
        }

        publish_status(
            SourceAccessPhase::Decrypting,
            "Authenticating and decrypting the authorized source archive.",
            {},
            {},
            0.70f);
        const bool decrypted = decrypt_archive_file(
            encrypted_path,
            plaintext_path,
            *dek,
            manifest->archive_nonce,
            manifest->ciphertext_size,
            manifest->ciphertext_sha256,
            manifest->plaintext_size,
            manifest->plaintext_sha256);
        OPENSSL_cleanse(dek->data(), dek->size());
        std::filesystem::remove(encrypted_path, ec);
        if (!decrypted)
        {
            return source_failure(
                cleanup_root,
                "The encrypted source archive failed AES-GCM authentication or signed hash verification.",
                shared.cancel_requested.load(std::memory_order_acquire));
        }

        publish_status(
            SourceAccessPhase::Ready,
            "Authorized source is verified and ready for the local operation.",
            {},
            {},
            1.0f);
        return PreparedSourceArchive{
            .ok = true,
            .archive_path = plaintext_path,
            .cleanup_root = cleanup_root,
            .archive_format = manifest->archive_format,
            .source_version = manifest->source_version,
            .commit = manifest->commit,
            .message = "Authorized source archive verified for local use."
        };
    }

    bool private_source_access_contract_self_test()
    {
        using namespace source_access_detail;
        auto client = generate_p256_key();
        auto server = generate_p256_key();
        const auto client_jwk = client ? public_jwk(client->value) : std::nullopt;
        const auto server_jwk = server ? public_jwk(server->value) : std::nullopt;
        auto imported_client = client_jwk ? import_public_p256(*client_jwk) : std::nullopt;
        auto imported_server = server_jwk ? import_public_p256(*server_jwk) : std::nullopt;
        auto client_secret = client && imported_server
            ? derive_ecdh_secret(client->value, imported_server->value)
            : std::nullopt;
        auto server_secret = server && imported_client
            ? derive_ecdh_secret(server->value, imported_client->value)
            : std::nullopt;
        if (!client_jwk || !server_jwk || !client_secret || !server_secret
            || *client_secret != *server_secret)
        {
            return false;
        }

        const std::vector<unsigned char> salt{
            0x00u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u,
            0x88u, 0x99u, 0xaau, 0xbbu, 0xccu, 0xddu, 0xeeu, 0xffu
        };
        auto client_wrap_key = hkdf_sha256(
            *client_secret,
            salt,
            PROJECT_PRIVATE_SOURCE_WRAP_INFO);
        auto server_wrap_key = hkdf_sha256(
            *server_secret,
            salt,
            PROJECT_PRIVATE_SOURCE_WRAP_INFO);
        OPENSSL_cleanse(client_secret->data(), client_secret->size());
        OPENSSL_cleanse(server_secret->data(), server_secret->size());
        if (!client_wrap_key || !server_wrap_key
            || *client_wrap_key != *server_wrap_key)
        {
            return false;
        }

        const std::vector<unsigned char> nonce{
            0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u,
            0x70u, 0x80u, 0x90u, 0xa0u, 0xb0u, 0xc0u
        };
        const std::string aad =
            std::string{ PROJECT_PRIVATE_SOURCE_WRAP_AAD_PREFIX }
            + "\nepoch-contract-artifact\n0123456789abcdef0123456789abcdef01234567";
        const std::vector<unsigned char> plaintext{
            'e', 'p', 'o', 'c', 'h', '-', 's', 'o', 'u', 'r', 'c', 'e'
        };
        std::vector<unsigned char> sealed(plaintext.size() + 16u);
        EVP_CIPHER_CTX* cipher = EVP_CIPHER_CTX_new();
        int written = 0;
        int total = 0;
        bool encrypted =
            cipher != nullptr
            && EVP_EncryptInit_ex(cipher, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
            && EVP_CIPHER_CTX_ctrl(
                cipher,
                EVP_CTRL_GCM_SET_IVLEN,
                static_cast<int>(nonce.size()),
                nullptr) == 1
            && EVP_EncryptInit_ex(
                cipher,
                nullptr,
                nullptr,
                server_wrap_key->data(),
                nonce.data()) == 1
            && EVP_EncryptUpdate(
                cipher,
                nullptr,
                &written,
                reinterpret_cast<const unsigned char*>(aad.data()),
                static_cast<int>(aad.size())) == 1
            && EVP_EncryptUpdate(
                cipher,
                sealed.data(),
                &written,
                plaintext.data(),
                static_cast<int>(plaintext.size())) == 1;
        total = written;
        if (encrypted)
        {
            encrypted = EVP_EncryptFinal_ex(cipher, sealed.data() + total, &written) == 1;
            total += written;
        }
        if (encrypted)
        {
            encrypted = EVP_CIPHER_CTX_ctrl(
                cipher,
                EVP_CTRL_GCM_GET_TAG,
                16,
                sealed.data() + total) == 1;
        }
        EVP_CIPHER_CTX_free(cipher);
        sealed.resize(encrypted ? static_cast<std::size_t>(total) + 16u : 0u);

        const auto opened = encrypted
            ? decrypt_aes256_gcm(sealed, *client_wrap_key, nonce, aad, 16u)
            : std::nullopt;
        std::vector<unsigned char> tampered = sealed;
        if (!tampered.empty())
            tampered.back() ^= 0x01u;
        const auto rejected = tampered.empty()
            ? std::optional<std::vector<unsigned char>>{}
            : decrypt_aes256_gcm(tampered, *client_wrap_key, nonce, aad, 16u);
        OPENSSL_cleanse(client_wrap_key->data(), client_wrap_key->size());
        OPENSSL_cleanse(server_wrap_key->data(), server_wrap_key->size());

        const PublicP256Jwk contract_jwk{
            .x = "ephemeral-x",
            .y = "ephemeral-y"
        };
        const std::string contract_challenge = registered_challenge_body(
            "contract-device",
            "contract-client-nonce",
            contract_jwk);
        const std::string expected_challenge =
            "{\"schema\":\"epoch-source-device-auth/v1\""
            ",\"device_id\":\"contract-device\""
            ",\"client_version\":\"" + json_escape(GetEngineVersionString())
            + "\",\"platform\":\"" + std::string{ source_platform() }
            + "\",\"artifact_intent\":\"latest-source\""
            ",\"client_nonce\":\"contract-client-nonce\""
            ",\"client_ephemeral_p256_jwk\":"
            "{\"kty\":\"EC\",\"crv\":\"P-256\","
            "\"x\":\"ephemeral-x\",\"y\":\"ephemeral-y\"}}";
        const auto contract_challenge_digest = sha256(contract_challenge);
        const std::string contract_auth_payload = device_auth_signing_payload(
            "contract-device",
            "contract-challenge",
            "contract-challenge-nonce",
            "contract-request-hash");
        const auto contract_enroll_payload = device_enroll_signing_payload(
            "device-code-contract",
            "client-nonce-contract",
            "{\"crv\":\"P-256\",\"kty\":\"EC\","
            "\"x\":\"signing-x\",\"y\":\"signing-y\"}",
            "{\"crv\":\"P-256\",\"kty\":\"EC\","
            "\"x\":\"ephemeral-x\",\"y\":\"ephemeral-y\"}");
        const std::string parser_padding(128u * 1024u, 'x');
        const std::string parser_document =
            "{\"status\":\"" + parser_padding + "\",\"expires_in\":300}";
        const auto parser_status = json_string_once(parser_document, "status");
        const auto parser_expiry = json_u64_once(parser_document, "expires_in");

        const bool policy_matches = private_source_access_enabled()
            ? !PROJECT_SOURCE_DEVICE_START_URL().empty()
                && !PROJECT_SOURCE_DEVICE_TOKEN_URL().empty()
                && !PROJECT_SOURCE_DEVICE_CHALLENGE_URL().empty()
            : PROJECT_SOURCE_DEVICE_START_URL().empty()
                && PROJECT_SOURCE_DEVICE_TOKEN_URL().empty()
                && PROJECT_SOURCE_DEVICE_CHALLENGE_URL().empty();
        return opened && *opened == plaintext && !rejected
            && parser_status && *parser_status == parser_padding
            && parser_expiry && *parser_expiry == 300u
            && !json_string_once("{\"status\":\"one\",\"status\":\"two\"}", "status")
            && !json_u64_once("{\"expires_in\":1,\"expires_in\":2}", "expires_in")
            && contract_challenge == expected_challenge
            && contract_challenge_digest
            && encode_base64url(
                contract_challenge_digest->data(),
                contract_challenge_digest->size()).size() == 43u
            && contract_auth_payload
                == "EPOCH_SOURCE_DEVICE_AUTH_V1\ncontract-device\n"
                   "contract-challenge\ncontract-challenge-nonce\n"
                   "contract-request-hash\n"
            && contract_enroll_payload
            && *contract_enroll_payload
                == "EPOCH_SOURCE_DEVICE_ENROLL_V1\n"
                   "2HvqaFuXi6NVVJnBRGjyBjTYRRCrXNWLXLzJeYq1Uic\n"
                   "client-nonce-contract\n"
                   "WHsME9jB_u6QYmc7WxhGush-nbfYlXr2HE6fn2s0pRg\n"
                   "w1d7Lo0zdHWLpjyCg18DAZJsgAghRsWPwhDbQhTcm_Q\n"
            && client_jwk->canonical().starts_with(
                "{\"crv\":\"P-256\",\"kty\":\"EC\",\"x\":\"")
            && safe_identifier("epoch-engine-v0.89.27-5f3613941a45")
            && !safe_identifier("../source")
            && trusted_site_url(
                "https://epoch.adamrushford.chatgpt.site/api/private/epoch-engine/device/manifest",
                "/api/private/epoch-engine/device/manifest")
            && !trusted_site_url(
                "https://example.invalid/api/private/epoch-engine/device/manifest",
                "/api/private/epoch-engine/device/manifest")
            && trusted_site_exact_url(
                "https://epoch.adamrushford.chatgpt.site/private/epoch-engine/device",
                "/private/epoch-engine/device")
            && !trusted_site_exact_url(
                "https://epoch.adamrushford.chatgpt.site/private/epoch-engine/device?user_code=ABCD-EFGH",
                "/private/epoch-engine/device")
            && policy_matches;
    }
}
