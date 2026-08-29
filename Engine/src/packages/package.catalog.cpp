/*
 * This file is part of the Epoch Project.
 * SPDX-License-Identifier: LicenseRef-MIT-NoSell
 */
module;

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#elif defined(EPOCH_HAS_CURL)
#include <curl/curl.h>
#endif

module package.catalog;

namespace epochengine::package_catalog
{
    namespace
    {
        constexpr std::size_t kMaximumBodyBytes = 256u * 1024u;
        constexpr std::size_t kMaximumEntries = 64u;
        constexpr std::size_t kMaximumDepth = 16u;
        constexpr std::uint64_t kMaximumArtifactBytes = 8ull * 1024ull * 1024ull * 1024ull;
        constexpr std::string_view kSiteOrigin =
            "https://epoch.adamrushford.chatgpt.site";

        struct Number { std::string text{}; };
        struct Value;
        using Object = std::map<std::string, Value, std::less<>>;
        using Array = std::vector<Value>;
        struct Value
        {
            using Storage = std::variant<
                std::nullptr_t, bool, std::string, Number, Array, Object>;
            Storage storage{};
        };

        class Reader final
        {
        public:
            explicit Reader(std::string_view source) noexcept : source_(source) {}
            [[nodiscard]] std::optional<Value> document()
            {
                auto value = parse_value(0u);
                space();
                return value && cursor_ == source_.size()
                    ? std::move(value) : std::nullopt;
            }

        private:
            void space() noexcept
            {
                while (cursor_ < source_.size()
                    && (source_[cursor_] == ' ' || source_[cursor_] == '\t'
                        || source_[cursor_] == '\r' || source_[cursor_] == '\n'))
                    ++cursor_;
            }
            [[nodiscard]] bool take(char expected) noexcept
            {
                space();
                if (cursor_ >= source_.size() || source_[cursor_] != expected)
                    return false;
                ++cursor_;
                return true;
            }
            [[nodiscard]] bool word(std::string_view expected) noexcept
            {
                space();
                if (source_.substr(cursor_, expected.size()) != expected)
                    return false;
                cursor_ += expected.size();
                return true;
            }
            static void utf8(std::string& out, std::uint32_t value)
            {
                if (value <= 0x7fu) out.push_back(static_cast<char>(value));
                else if (value <= 0x7ffu)
                {
                    out.push_back(static_cast<char>(0xc0u | (value >> 6u)));
                    out.push_back(static_cast<char>(0x80u | (value & 0x3fu)));
                }
                else
                {
                    out.push_back(static_cast<char>(0xe0u | (value >> 12u)));
                    out.push_back(static_cast<char>(0x80u | ((value >> 6u) & 0x3fu)));
                    out.push_back(static_cast<char>(0x80u | (value & 0x3fu)));
                }
            }
            [[nodiscard]] std::optional<std::string> text()
            {
                space();
                if (cursor_ >= source_.size() || source_[cursor_++] != '"')
                    return std::nullopt;
                std::string out{};
                while (cursor_ < source_.size())
                {
                    const unsigned char character =
                        static_cast<unsigned char>(source_[cursor_++]);
                    if (character == '"') return out;
                    if (character < 0x20u) return std::nullopt;
                    if (character != '\\')
                    {
                        out.push_back(static_cast<char>(character));
                        continue;
                    }
                    if (cursor_ >= source_.size()) return std::nullopt;
                    switch (source_[cursor_++])
                    {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u':
                    {
                        if (cursor_ + 4u > source_.size()) return std::nullopt;
                        std::uint32_t codepoint{};
                        for (std::size_t index = 0u; index < 4u; ++index)
                        {
                            const char digit = source_[cursor_++];
                            codepoint <<= 4u;
                            if (digit >= '0' && digit <= '9') codepoint |= digit - '0';
                            else if (digit >= 'a' && digit <= 'f') codepoint |= digit - 'a' + 10;
                            else if (digit >= 'A' && digit <= 'F') codepoint |= digit - 'A' + 10;
                            else return std::nullopt;
                        }
                        if (codepoint >= 0xd800u && codepoint <= 0xdfffu)
                            return std::nullopt;
                        utf8(out, codepoint);
                        break;
                    }
                    default: return std::nullopt;
                    }
                }
                return std::nullopt;
            }
            [[nodiscard]] std::optional<Value> number()
            {
                space();
                const std::size_t begin = cursor_;
                if (cursor_ < source_.size() && source_[cursor_] == '-') ++cursor_;
                if (cursor_ >= source_.size()
                    || !std::isdigit(static_cast<unsigned char>(source_[cursor_])))
                    return std::nullopt;
                while (cursor_ < source_.size()
                    && std::isdigit(static_cast<unsigned char>(source_[cursor_])))
                    ++cursor_;
                if (cursor_ < source_.size()
                    && (source_[cursor_] == '.' || source_[cursor_] == 'e'
                        || source_[cursor_] == 'E'))
                    return std::nullopt;
                return Value{ Number{ std::string(source_.substr(begin, cursor_ - begin)) } };
            }
            [[nodiscard]] std::optional<Value> array(std::size_t depth)
            {
                if (!take('[')) return std::nullopt;
                Array values{};
                if (take(']')) return Value{ std::move(values) };
                for (;;)
                {
                    auto value = parse_value(depth + 1u);
                    if (!value || values.size() >= kMaximumEntries * 4u)
                        return std::nullopt;
                    values.push_back(std::move(*value));
                    if (take(']')) return Value{ std::move(values) };
                    if (!take(',')) return std::nullopt;
                }
            }
            [[nodiscard]] std::optional<Value> object(std::size_t depth)
            {
                if (!take('{')) return std::nullopt;
                Object values{};
                if (take('}')) return Value{ std::move(values) };
                for (;;)
                {
                    auto key = text();
                    if (!key || key->size() > 96u || !take(':')) return std::nullopt;
                    auto value = parse_value(depth + 1u);
                    if (!value || values.size() >= 64u
                        || !values.emplace(std::move(*key), std::move(*value)).second)
                        return std::nullopt;
                    if (take('}')) return Value{ std::move(values) };
                    if (!take(',')) return std::nullopt;
                }
            }
            [[nodiscard]] std::optional<Value> parse_value(std::size_t depth)
            {
                if (depth > kMaximumDepth) return std::nullopt;
                space();
                if (cursor_ >= source_.size()) return std::nullopt;
                switch (source_[cursor_])
                {
                case '{': return object(depth);
                case '[': return array(depth);
                case '"':
                {
                    auto value = text();
                    return value ? std::optional<Value>{ Value{ std::move(*value) } }
                                 : std::nullopt;
                }
                case 't': return word("true") ? std::optional<Value>{ Value{ true } } : std::nullopt;
                case 'f': return word("false") ? std::optional<Value>{ Value{ false } } : std::nullopt;
                case 'n': return word("null") ? std::optional<Value>{ Value{ nullptr } } : std::nullopt;
                default: return number();
                }
            }
            std::string_view source_{};
            std::size_t cursor_{};
        };

        [[nodiscard]] const Value* member(
            const Object& object, std::string_view key) noexcept
        {
            const auto found = object.find(key);
            return found == object.end() ? nullptr : &found->second;
        }
        [[nodiscard]] const std::string* text_member(
            const Object& object, std::string_view key) noexcept
        {
            const Value* value = member(object, key);
            return value ? std::get_if<std::string>(&value->storage) : nullptr;
        }
        [[nodiscard]] std::optional<bool> bool_member(
            const Object& object, std::string_view key) noexcept
        {
            const Value* value = member(object, key);
            const bool* parsed = value ? std::get_if<bool>(&value->storage) : nullptr;
            return parsed ? std::optional<bool>{ *parsed } : std::nullopt;
        }
        [[nodiscard]] std::optional<std::uint64_t> unsigned_member(
            const Object& object, std::string_view key) noexcept
        {
            const Value* value = member(object, key);
            const Number* number = value ? std::get_if<Number>(&value->storage) : nullptr;
            if (!number || number->text.empty() || number->text.front() == '-')
                return std::nullopt;
            std::uint64_t output{};
            const auto parsed = std::from_chars(
                number->text.data(), number->text.data() + number->text.size(), output);
            return parsed.ec == std::errc{}
                && parsed.ptr == number->text.data() + number->text.size()
                ? std::optional<std::uint64_t>{ output } : std::nullopt;
        }
        [[nodiscard]] bool catalog_revision(std::string_view value) noexcept
        {
            if (value.empty() || value.size() > 96u)
                return false;
            const bool validFirst = (value.front() >= 'a' && value.front() <= 'z')
                || (value.front() >= '0' && value.front() <= '9');
            return validFirst && std::all_of(value.begin(), value.end(), [](char character)
            {
                return (character >= 'a' && character <= 'z')
                    || (character >= '0' && character <= '9')
                    || character == '.' || character == '_'
                    || character == '+' || character == '-';
            });
        }
        [[nodiscard]] bool identifier(std::string_view value) noexcept
        {
            return !value.empty() && value.size() <= 64u
                && std::all_of(value.begin(), value.end(), [](char character)
                {
                    return (character >= 'a' && character <= 'z')
                        || (character >= '0' && character <= '9')
                        || character == '.' || character == '_'
                        || character == '-';
                });
        }
        [[nodiscard]] bool lower_hex(
            std::string_view value, std::size_t size) noexcept
        {
            return value.size() == size
                && std::all_of(value.begin(), value.end(), [](char character)
                {
                    return (character >= '0' && character <= '9')
                        || (character >= 'a' && character <= 'f');
                });
        }
        [[nodiscard]] bool semantic_revision(std::string_view value) noexcept
        {
            if (value.size() < 6u || value.front() != 'v') return false;
            std::size_t cursor = 1u;
            for (int component = 0; component < 3; ++component)
            {
                const std::size_t begin = cursor;
                while (cursor < value.size()
                    && value[cursor] >= '0' && value[cursor] <= '9') ++cursor;
                if (cursor == begin) return false;
                if (component < 2
                    && (cursor >= value.size() || value[cursor++] != '.')) return false;
            }
            if (cursor == value.size()) return true;
            if (value[cursor++] != '-' || cursor == value.size()) return false;
            for (; cursor < value.size(); ++cursor)
            {
                const char character = value[cursor];
                if (!(character >= 'a' && character <= 'z')
                    && !(character >= '0' && character <= '9')
                    && character != '.' && character != '-') return false;
            }
            return true;
        }
        [[nodiscard]] bool immutable_revision(std::string_view value) noexcept
        {
            return lower_hex(value, 40u) || semantic_revision(value);
        }
        [[nodiscard]] bool canonical_download_url(
            std::string_view value, std::string_view prefix) noexcept
        {
            const std::string expected = std::string{kSiteOrigin} + std::string{prefix};
            if (!value.starts_with(expected) || value.size() <= expected.size())
                return false;
            const std::string_view suffix = value.substr(expected.size());
            return suffix.find('?') == std::string_view::npos
                && suffix.find('#') == std::string_view::npos
                && suffix.find('\\') == std::string_view::npos
                && suffix.find('%') == std::string_view::npos
                && suffix.find("..") == std::string_view::npos
                && suffix.find("//") == std::string_view::npos
                && suffix.find(' ') == std::string_view::npos;
        }
        [[nodiscard]] bool one_of(
            std::string_view value,
            std::initializer_list<std::string_view> choices) noexcept
        {
            return std::find(choices.begin(), choices.end(), value) != choices.end();
        }
        [[nodiscard]] std::optional<Scope> parse_scope(std::string_view value) noexcept
        {
            if (value == "engine") return Scope::engine;
            if (value == "project") return Scope::project;
            if (value == "user") return Scope::user;
            return std::nullopt;
        }
        [[nodiscard]] std::optional<Availability> parse_availability(
            std::string_view value) noexcept
        {
            if (value == "available") return Availability::available;
            if (value == "descriptor_only") return Availability::descriptor_only;
            return std::nullopt;
        }

        struct HttpResult { long status{}; std::string body{}; std::string error{}; };
#if defined(_WIN32)
        struct WinHttpHandle final
        {
            HINTERNET value{};
            explicit WinHttpHandle(HINTERNET handle = nullptr) noexcept : value(handle) {}
            ~WinHttpHandle() { if (value) WinHttpCloseHandle(value); }
            WinHttpHandle(const WinHttpHandle&) = delete;
            WinHttpHandle& operator=(const WinHttpHandle&) = delete;
        };
#elif defined(EPOCH_HAS_CURL)
        std::size_t curl_write(
            char* data, std::size_t size, std::size_t count, void* user)
        {
            auto& body = *static_cast<std::string*>(user);
            const std::size_t bytes = size * count;
            if (bytes == 0u || bytes > kMaximumBodyBytes - body.size()) return 0u;
            body.append(data, bytes);
            return bytes;
        }
#endif

        [[nodiscard]] HttpResult fetch_body()
        {
            HttpResult result{};
#if defined(_WIN32)
            WinHttpHandle session{ WinHttpOpen(
                L"EpochPackageCatalog/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0u) };
            if (!session.value)
            {
                result.error = "The HTTPS session could not start.";
                return result;
            }
            WinHttpSetTimeouts(session.value, 2000, 2000, 5000, 5000);
            WinHttpHandle connection{ WinHttpConnect(
                session.value, L"epoch.adamrushford.chatgpt.site",
                INTERNET_DEFAULT_HTTPS_PORT, 0u) };
            WinHttpHandle request{ connection.value ? WinHttpOpenRequest(
                connection.value, L"GET", L"/api/epoch/packages/catalog",
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE) : nullptr };
            if (!connection.value || !request.value)
            {
                result.error = "The Epoch Site catalog request could not be created.";
                return result;
            }
            DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            (void)WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY,
                &redirect, sizeof(redirect));
            constexpr wchar_t headers[] =
                L"Accept: application/json\r\nCache-Control: no-cache\r\n";
            if (!WinHttpSendRequest(request.value, headers,
                    static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA,
                    0u, 0u, 0u)
                || !WinHttpReceiveResponse(request.value, nullptr))
            {
                result.error = "The catalog HTTPS request failed.";
                return result;
            }
            DWORD status{};
            DWORD statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(request.value,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                    WINHTTP_NO_HEADER_INDEX))
            {
                result.error = "The catalog response had no status.";
                return result;
            }
            result.status = static_cast<long>(status);
            while (result.body.size() < kMaximumBodyBytes)
            {
                DWORD available{};
                if (!WinHttpQueryDataAvailable(request.value, &available))
                {
                    result.error = "The catalog response could not be read.";
                    return result;
                }
                if (available == 0u) break;
                if (available > kMaximumBodyBytes - result.body.size())
                {
                    result.error = "The catalog response exceeded its size limit.";
                    return result;
                }
                const std::size_t offset = result.body.size();
                result.body.resize(offset + available);
                DWORD read{};
                if (!WinHttpReadData(request.value,
                        result.body.data() + offset, available, &read))
                {
                    result.error = "The catalog response could not be read.";
                    return result;
                }
                result.body.resize(offset + read);
            }
#elif defined(EPOCH_HAS_CURL)
            static std::once_flag once{};
            std::call_once(once, [] { (void)curl_global_init(CURL_GLOBAL_DEFAULT); });
            CURL* curl = curl_easy_init();
            if (!curl)
            {
                result.error = "The HTTPS client could not start.";
                return result;
            }
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Cache-Control: no-cache");
            const std::string url{ endpoint };
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
            const CURLcode code = curl_easy_perform(curl);
            (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
            if (code != CURLE_OK) result.error = "The catalog HTTPS request failed.";
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
#else
            result.error = "This build has no HTTPS catalog transport.";
#endif
            return result;
        }

    }

    Snapshot parse(std::string_view json)
    {
        Snapshot result{};
        if (json.empty() || json.size() > kMaximumBodyBytes)
        {
            result.message = "The Site catalog was empty or exceeded 256 KiB.";
            return result;
        }
        Reader reader{ json };
        const auto document = reader.document();
        const Object* root = document ? std::get_if<Object>(&document->storage) : nullptr;
        if (!root || root->size() != 3u)
        {
            result.message = "The Site catalog is invalid JSON or has unexpected fields.";
            return result;
        }
        const std::string* schemaValue = text_member(*root, "schema");
        const std::string* revision = text_member(*root, "catalog_revision");
        const Value* packagesValue = member(*root, "packages");
        const Array* packages = packagesValue
            ? std::get_if<Array>(&packagesValue->storage) : nullptr;
        if (!schemaValue || *schemaValue != schema || !revision
            || !catalog_revision(*revision) || !packages || packages->empty()
            || packages->size() > kMaximumEntries)
        {
            result.message = "The Site catalog schema, revision, or package array is invalid.";
            return result;
        }

        std::set<std::string, std::less<>> ids{};
        result.revision = *revision;
        result.entries.reserve(packages->size());
        for (const Value& packageValue : *packages)
        {
            const Object* object = std::get_if<Object>(&packageValue.storage);
            if (!object || object->size() != 18u)
            {
                result.message = "A Site package has missing or unexpected fields.";
                result.entries.clear();
                return result;
            }
            const std::string* id = text_member(*object, "id");
            const std::string* displayName = text_member(*object, "display_name");
            const std::string* summary = text_member(*object, "summary");
            const std::string* kind = text_member(*object, "kind");
            const std::string* activation = text_member(*object, "activation");
            const std::string* scopeValue = text_member(*object, "scope");
            const std::string* availabilityValue = text_member(*object, "availability");
            const std::string* sourceUrl = text_member(*object, "source_url");
            const std::string* immutableRevision = text_member(*object, "immutable_revision");
            const std::string* sha256 = text_member(*object, "sha256");
            const std::string* licenseUrl = text_member(*object, "license_url");
            const auto sizeBytes = unsigned_member(*object, "size_bytes");
            const auto identityRequired = bool_member(*object, "identity_required");
            const auto automaticExecution = bool_member(*object, "automatic_execution");
            const auto localAdmission = bool_member(*object, "local_admission_required");
            const auto signedPayload = bool_member(*object, "signed_payload");
            const auto contentAddressed = bool_member(*object, "content_addressed");
            const Value* platformsValue = member(*object, "platforms");
            const Array* platforms = platformsValue
                ? std::get_if<Array>(&platformsValue->storage) : nullptr;
            const auto parsedScope = scopeValue ? parse_scope(*scopeValue) : std::nullopt;
            const auto parsedAvailability = availabilityValue
                ? parse_availability(*availabilityValue) : std::nullopt;
            if (!id || !identifier(*id) || !ids.insert(*id).second
                || !displayName || displayName->empty() || displayName->size() > 96u
                || !summary || summary->empty() || summary->size() > 1024u
                || !kind || !one_of(*kind,
                    {"descriptor", "engine_extension", "library", "project_template", "tool"})
                || !activation || !one_of(*activation,
                    {"build_time", "engine_extension", "manual", "project_dependency"})
                || !parsedScope || !parsedAvailability
                || !sourceUrl || !canonical_download_url(*sourceUrl, "/downloads/packages/")
                || !immutableRevision || !immutable_revision(*immutableRevision)
                || !sha256 || !lower_hex(*sha256, 64u)
                || !sizeBytes || *sizeBytes == 0u || *sizeBytes > kMaximumArtifactBytes
                || !licenseUrl || !canonical_download_url(*licenseUrl, "/downloads/licenses/")
                || !platforms || platforms->empty() || platforms->size() > 8u
                || !identityRequired || !automaticExecution || !localAdmission
                || !signedPayload || !contentAddressed
                || *identityRequired || *automaticExecution || !*localAdmission)
            {
                result.message = "A Site package violates the bounded catalog contract.";
                result.entries.clear();
                return result;
            }

            Entry entry{
                .id = *id,
                .display_name = *displayName,
                .summary = *summary,
                .kind = *kind,
                .activation = *activation,
                .scope = *parsedScope,
                .availability = *parsedAvailability,
                .source_url = *sourceUrl,
                .immutable_revision = *immutableRevision,
                .sha256 = *sha256,
                .size_bytes = *sizeBytes,
                .license_url = *licenseUrl,
                .identity_required = *identityRequired,
                .automatic_execution = *automaticExecution,
                .local_admission_required = *localAdmission,
                .signed_payload = *signedPayload,
                .content_addressed = *contentAddressed
            };
            std::set<std::string, std::less<>> uniquePlatforms{};
            for (const Value& platformValue : *platforms)
            {
                const std::string* platform =
                    std::get_if<std::string>(&platformValue.storage);
                if (!platform || !one_of(*platform, {"any", "linux-x64", "windows-x64"})
                    || !uniquePlatforms.insert(*platform).second)
                {
                    result.message = "A Site package has an invalid platform list.";
                    result.entries.clear();
                    return result;
                }
                entry.platforms.push_back(*platform);
            }
            result.entries.push_back(std::move(entry));
        }
        result.ok = true;
        result.message = "Site catalog loaded.";
        return result;
    }

    Snapshot fetch()
    {
        const HttpResult response = fetch_body();
        if (!response.error.empty()) return Snapshot{ .message = response.error };
        if (response.status != 200)
            return Snapshot{ .message = "The Epoch Site package catalog is unavailable." };
        return parse(response.body);
    }

    bool contract_self_test()
    {
        constexpr std::string_view valid = R"JSON({"schema":"epoch-package-catalog/v1","catalog_revision":"test-1","packages":[{"id":"epoch-gui","display_name":"EpochGui","summary":"Backend-neutral native interface library.","kind":"library","activation":"project_dependency","scope":"project","availability":"available","source_url":"https://epoch.adamrushford.chatgpt.site/downloads/packages/epoch-gui/v0.89.29/source.tar.gz","immutable_revision":"8882503ac579add67456459986983ad7fd7c96db","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","size_bytes":111063,"license_url":"https://epoch.adamrushford.chatgpt.site/downloads/licenses/epoch-gui-v0.89.29.txt","platforms":["any"],"identity_required":false,"automatic_execution":false,"local_admission_required":true,"signed_payload":false,"content_addressed":false}]})JSON";
        const Snapshot parsed = parse(valid);
        if (!parsed.ok || parsed.entries.size() != 1u
            || parsed.entries.front().id != "epoch-gui") return false;
        std::string extra{ valid };
        const std::size_t close = extra.rfind('}');
        extra.insert(close, ",\"unexpected\":true");
        std::string unsafe{ valid };
        const std::string safePrefix = "/downloads/packages/";
        unsafe.replace(unsafe.find(safePrefix), safePrefix.size(), "/redirect/");
        return !parse(extra).ok && !parse(unsafe).ok
            && !parse(R"JSON({"schema":"epoch-package-catalog/v1","catalog_revision":"x","packages":[]})JSON").ok;
    }

    std::string_view scope_name(Scope value) noexcept
    {
        switch (value)
        {
        case Scope::engine: return "Engine";
        case Scope::project: return "Project";
        case Scope::user: return "User";
        }
        return "Unknown";
    }

    std::string_view availability_name(Availability value) noexcept
    {
        return value == Availability::available ? "Available" : "Descriptor only";
    }
}
