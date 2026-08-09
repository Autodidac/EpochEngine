/************************************************
 *  ███████╗██████╗  ██████╗  ██████╗██╗  ██╗   *
 *  ██╔════╝██╔══██╗██╔═══██╗██╔════╝██║  ██║   *
 *  █████╗  ██████╔╝██║   ██║██║     ███████║   *
 *  ██╔══╝  ██╔═══╝ ██║   ██║██║     ██╔══██║   *
 *  ███████╗██║     ╚██████╔╝╚██████╗██║  ██║   *
 *  ╚══════╝╚═╝      ╚═════╝  ╚═════╝╚═╝  ╚═╝   *
 *                                              *
 *   This file is part of the Epoch   Project.  *
 *   epochengine - Modular C++ Framework        *
 *                                              *
 *   SPDX-License-Identifier:                   *
 *   LicenseRef-MIT-NoSell                      *
 *                                              *
 *   Provided "AS IS", without warranty         *
 *   of any kind.                               *
 *                                              *
 *   Use permitted for Non-Commercial           *
 *   Purposes ONLY, without prior               *
 *   commercial licensing agreement.            *
 *                                              *
 *   Redistribution Allowed with This Notice    *
 *   and LICENSE file.                          *
 *                                              *
 *   No obligation to disclose                  *
 *   modifications.                             *
 *                                              *
 *   See LICENSE file for full terms.           *
 *                                              *
 ***********************************************/
module;

#include "../include/core.stl_types.hpp"

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <winhttp.h>
#   pragma comment(lib, "winhttp.lib")
#else
#   include <fcntl.h>
#   include <signal.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   include <unistd.h>
#   if defined(EPOCH_HAS_CURL)
#       include <curl/curl.h>
#   endif
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module ai.engine;

import ai.runtime;
import ai.dataset;
import ai.session;
import ai.mcp;
import ai.eval;
import core.log;
import core.path;

namespace epochengine::ai
{
    namespace
    {
        EngineAiModel* g_engineAi = nullptr;
        ProviderMode g_providerMode = ProviderMode::OpenSourceLocal;
        static std::string read_env_var(const char* name)
        {
#if defined(_WIN32)
            const DWORD needed = GetEnvironmentVariableA(name, nullptr, 0);
            if (needed == 0)
                return {};
            std::string value(needed, '\0');
            const DWORD written = GetEnvironmentVariableA(name, value.data(), needed);
            if (written == 0)
                return {};
            value.resize(static_cast<std::size_t>(written));
            return value;
#else
            const char* value = std::getenv(name);
            return value ? std::string(value) : std::string{};
#endif
        }

        static std::string trim_env_value(std::string value)
        {
            auto isSpace = [](unsigned char c) noexcept
            {
                return std::isspace(c) != 0;
            };
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
                value.erase(value.begin());
            while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
                value.pop_back();
            return value;
        }

        static std::string configured_endpoint()
        {
            constexpr const char* candidates[] = {
                "EPOCH_AI_ENDPOINT",
                "EPOCH_OPENAI_BASE_URL",
                "LM_STUDIO_BASE_URL",
                "OPENAI_BASE_URL"
            };
            for (const char* name : candidates)
            {
                std::string value = trim_env_value(read_env_var(name));
                if (!value.empty())
                    return value;
            }
            return "http://localhost:1234";
        }

        static std::string configured_model()
        {
            constexpr const char* candidates[] = {
                "EPOCH_AI_MODEL",
                "EPOCH_OPENAI_MODEL",
                "LM_STUDIO_MODEL",
                "OPENAI_MODEL"
            };
            for (const char* name : candidates)
            {
                std::string value = trim_env_value(read_env_var(name));
                if (!value.empty())
                    return value;
            }
            return {};
        }

        std::string g_selectedEndpoint{ configured_endpoint() };
        std::string g_selectedModel{ configured_model() };
        bool g_selectedModelPreferenceLoaded = false;
        std::vector<std::string> g_detectedModels{};
        std::string g_modelDetectionStatus = g_selectedModel.empty()
            ? std::string{ "Not scanned." }
            : std::string{ "Configured model: " } + g_selectedModel;
        LocalInferenceTransport g_localTransport =
            trim_env_value(read_env_var("EPOCH_AI_BACKEND")) == "llama_cpp_cli"
                ? LocalInferenceTransport::LlamaCppCli
                : LocalInferenceTransport::OpenAiCompatible;
        std::string g_directExecutable = trim_env_value(read_env_var("EPOCH_LLAMA_CPP_EXECUTABLE"));
        std::string g_directModel = trim_env_value(read_env_var("EPOCH_AI_MODEL_PATH"));
        bool g_runtimePreferenceLoaded = false;

        static std::string executable_cache_bucket(std::string_view bucket)
        {
            const auto runtimeRoot = epochengine::core::path::runtime_root_dir();
            if (!runtimeRoot.empty())
                return (runtimeRoot / "cache" / std::string{ bucket }).generic_string();

            return std::string{ "cache/" } + std::string{ bucket };
        }

        static std::filesystem::path selected_model_preference_file()
        {
            return std::filesystem::path{ executable_cache_bucket("models") } / "selected_os_model.txt";
        }

        static std::filesystem::path local_runtime_preference_file()
        {
            return std::filesystem::path{ executable_cache_bucket("models") } / "local_inference_runtime.conf";
        }

        static bool ends_with(std::string_view s, std::string_view suf)
        {
            return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
        }

        static void rstrip_slashes(std::string& s)
        {
            while (!s.empty() && s.back() == '/')
                s.pop_back();
        }

        static std::string normalize_openai_chat_endpoint(std::string endpoint)
        {
            // Accept:
            //  - http://host:port
            //  - http://host:port/v1
            //  - http://host:port/v1/chat/completions
            //  - http://host:port/api/v1/chat
            // Normalize to OpenAI-compatible /v1/chat/completions.
            rstrip_slashes(endpoint);

            constexpr std::string_view suffixes[] = {
                "/api/v1/chat",
                "/v1/chat/completions",
                "/v1/models",
                "/v1"
            };

            for (const auto suffix : suffixes)
            {
                if (ends_with(endpoint, suffix))
                {
                    endpoint.resize(endpoint.size() - suffix.size());
                    rstrip_slashes(endpoint);
                    break;
                }
            }

            return endpoint + "/v1/chat/completions";
        }

        static std::string normalize_model_list_endpoint(std::string endpoint)
        {
            rstrip_slashes(endpoint);

            constexpr std::string_view suffixes[] = {
                "/api/v1/chat",
                "/v1/chat/completions",
                "/v1/models",
                "/v1"
            };

            for (const auto suffix : suffixes)
            {
                if (ends_with(endpoint, suffix))
                {
                    endpoint.resize(endpoint.size() - suffix.size());
                    rstrip_slashes(endpoint);
                    break;
                }
            }

            return endpoint + "/v1/models";
        }

        static std::string trim(std::string_view s)
        {
            auto is_ws = [](unsigned char c) { return std::isspace(c) != 0; };
            std::size_t b = 0;
            while (b < s.size() && is_ws((unsigned char)s[b])) ++b;
            std::size_t e = s.size();
            while (e > b && is_ws((unsigned char)s[e - 1])) --e;
            return std::string(s.substr(b, e - b));
        }

        static std::string lowercase_ascii(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (const unsigned char c : s)
                out.push_back(static_cast<char>(std::tolower(c)));
            return out;
        }

        [[nodiscard]] static bool contains_text(std::string_view haystack, std::string_view needle) noexcept
        {
            return haystack.find(needle) != std::string_view::npos;
        }

        [[nodiscard]] static bool starts_with_text(std::string_view haystack, std::string_view needle) noexcept
        {
            return haystack.size() >= needle.size() && haystack.substr(0, needle.size()) == needle;
        }

        static std::string json_escape(std::string_view s)
        {
            std::string out;
            out.reserve(s.size() + 16);
            for (unsigned char c : s)
            {
                switch (c)
                {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20)
                    {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04X", (unsigned)c);
                        out += buf;
                    }
                    else
                    {
                        out.push_back((char)c);
                    }
                    break;
                }
            }
            return out;
        }

        static std::string json_unescape(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                char c = s[i];
                if (c != '\\')
                {
                    out.push_back(c);
                    continue;
                }
                if (i + 1 >= s.size())
                    break;
                char n = s[++i];
                switch (n)
                {
                case '\\': out.push_back('\\'); break;
                case '"':  out.push_back('"'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'u':
                    // Minimal: skip \uXXXX (keep as '?')
                    if (i + 4 < s.size()) i += 4;
                    out.push_back('?');
                    break;
                default:
                    out.push_back(n);
                    break;
                }
            }
            return out;
        }

        static std::string slugify(std::string_view input)
        {
            std::string out;
            out.reserve(input.size());

            bool previousWasDash = false;
            for (const unsigned char c : input)
            {
                if (std::isalnum(c) != 0)
                {
                    out.push_back(static_cast<char>(std::tolower(c)));
                    previousWasDash = false;
                    continue;
                }

                if (!previousWasDash)
                {
                    out.push_back('-');
                    previousWasDash = true;
                }
            }

            while (!out.empty() && out.front() == '-')
                out.erase(out.begin());
            while (!out.empty() && out.back() == '-')
                out.pop_back();

            if (out.empty())
                out = "record";

            return out;
        }

        static bool append_jsonl_line(const std::filesystem::path& file, const std::string& line)
        {
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);

            FILE* handle = nullptr;
#if defined(_WIN32)
            if (0 != fopen_s(&handle, file.string().c_str(), "ab"))
                return false;
#else
            handle = std::fopen(file.string().c_str(), "ab");
            if (!handle)
                return false;
#endif

            const std::string payload = line + "\n";
            const bool ok = std::fwrite(payload.data(), 1, payload.size(), handle) == payload.size();
            std::fclose(handle);
            return ok;
        }

        static bool write_text_file(const std::filesystem::path& file, const std::string& contents)
        {
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);

            FILE* handle = nullptr;
#if defined(_WIN32)
            if (0 != fopen_s(&handle, file.string().c_str(), "wb"))
                return false;
#else
            handle = std::fopen(file.string().c_str(), "wb");
            if (!handle)
                return false;
#endif

            const bool ok = std::fwrite(contents.data(), 1, contents.size(), handle) == contents.size();
            std::fclose(handle);
            return ok;
        }

        static std::string read_small_text_file(const std::filesystem::path& file, std::uintmax_t maxBytes = 4096)
        {
            std::error_code ec;
            if (!std::filesystem::exists(file, ec))
                return {};

            const auto size = std::filesystem::file_size(file, ec);
            if (ec || size == 0 || size > maxBytes)
                return {};

            FILE* handle = nullptr;
#if defined(_WIN32)
            if (0 != fopen_s(&handle, file.string().c_str(), "rb"))
                return {};
#else
            handle = std::fopen(file.string().c_str(), "rb");
            if (!handle)
                return {};
#endif

            std::string contents(static_cast<std::size_t>(size), '\0');
            const auto read = std::fread(contents.data(), 1, contents.size(), handle);
            std::fclose(handle);
            contents.resize(read);
            return trim(contents);
        }

        static void restore_selected_model_preference_if_needed()
        {
            if (g_selectedModelPreferenceLoaded)
                return;
            g_selectedModelPreferenceLoaded = true;

            const std::string restored = read_small_text_file(selected_model_preference_file());
            if (restored.empty())
                return;

            if (g_selectedModel == restored)
                return;

            g_selectedModel = restored;
            g_modelDetectionStatus = "Restored selected OS model preference: " + restored;
        }

        static void persist_selected_model_preference(std::string_view model_id)
        {
            const std::string selected = trim(model_id);
            if (selected.empty())
                return;

            if (!write_text_file(selected_model_preference_file(), selected + "\n"))
                core::log::error("ai", "Failed to persist selected OS model preference.");
        }

        static void restore_runtime_preference_if_needed()
        {
            if (g_runtimePreferenceLoaded)
                return;
            g_runtimePreferenceLoaded = true;

            std::istringstream input(read_small_text_file(local_runtime_preference_file(), 16384));
            std::string line;
            while (std::getline(input, line))
            {
                const auto separator = line.find('=');
                if (separator == std::string::npos)
                    continue;
                const std::string key = trim(std::string_view{line}.substr(0u, separator));
                const std::string value = trim(std::string_view{line}.substr(separator + 1u));
                if (key == "transport" && value == "llama_cpp_cli")
                    g_localTransport = LocalInferenceTransport::LlamaCppCli;
                else if (key == "transport" && value == "openai_compatible")
                    g_localTransport = LocalInferenceTransport::OpenAiCompatible;
                else if (key == "executable" && g_directExecutable.empty())
                    g_directExecutable = value;
                else if (key == "model" && g_directModel.empty())
                    g_directModel = value;
            }
        }

        static void persist_runtime_preference()
        {
            std::ostringstream output;
            output << "transport="
                   << (g_localTransport == LocalInferenceTransport::LlamaCppCli
                       ? "llama_cpp_cli" : "openai_compatible") << '\n';
            output << "executable=" << g_directExecutable << '\n';
            output << "model=" << g_directModel << '\n';
            if (!write_text_file(local_runtime_preference_file(), output.str()))
                core::log::error("ai", "Failed to persist local inference runtime preference.");
        }

        [[nodiscard]] static bool regular_file(const std::filesystem::path& path)
        {
            std::error_code ec;
            return !path.empty() && std::filesystem::is_regular_file(path, ec);
        }

        [[nodiscard]] static std::filesystem::path find_path_executable(std::string_view name)
        {
            const std::filesystem::path requested{std::string{name}};
            if (regular_file(requested))
                return std::filesystem::absolute(requested);

            const std::string pathValue = read_env_var("PATH");
#if defined(_WIN32)
            constexpr char separator = ';';
            constexpr std::string_view suffix = ".exe";
#else
            constexpr char separator = ':';
            constexpr std::string_view suffix{};
#endif
            std::size_t first = 0u;
            while (first <= pathValue.size())
            {
                const std::size_t pastLast = pathValue.find(separator, first);
                const std::string_view item = std::string_view{pathValue}.substr(
                    first,
                    pastLast == std::string::npos ? std::string::npos : pastLast - first);
                if (!item.empty())
                {
                    std::filesystem::path candidate = std::filesystem::path{std::string{item}} / requested;
                    if (candidate.extension().empty())
                        candidate += suffix;
                    if (regular_file(candidate))
                        return std::filesystem::absolute(candidate);
                }
                if (pastLast == std::string::npos)
                    break;
                first = pastLast + 1u;
            }
            return {};
        }

        [[nodiscard]] static std::filesystem::path discover_llama_executable()
        {
            if (regular_file(g_directExecutable))
                return std::filesystem::absolute(g_directExecutable);

            const std::filesystem::path cacheRoot{executable_cache_bucket("packages")};
            constexpr std::string_view names[] = {
#if defined(_WIN32)
                "llama-cli.exe", "main.exe"
#else
                "llama-cli", "main"
#endif
            };
            const std::filesystem::path roots[] = {
                cacheRoot / "local_ai_llama_cpp_runtime" / "bin",
                cacheRoot / "llama.cpp" / "bin",
                cacheRoot / "llama.cpp" / "build" / "bin"
            };
            for (const auto& root : roots)
                for (const auto name : names)
                    if (const auto candidate = root / name; regular_file(candidate))
                        return std::filesystem::absolute(candidate);
            for (const auto name : names)
                if (const auto candidate = find_path_executable(name); !candidate.empty())
                    return candidate;
            return {};
        }

        [[nodiscard]] static std::filesystem::path discover_gguf_model()
        {
            if (regular_file(g_directModel))
                return std::filesystem::absolute(g_directModel);

            const std::filesystem::path root{executable_cache_bucket("models")};
            std::error_code ec;
            if (!std::filesystem::exists(root, ec))
                return {};

            std::filesystem::path best;
            std::size_t visited = 0u;
            std::filesystem::recursive_directory_iterator iterator{
                root,
                std::filesystem::directory_options::skip_permission_denied,
                ec};
            const std::filesystem::recursive_directory_iterator end{};
            while (!ec && iterator != end && visited++ < 8192u)
            {
                const auto path = iterator->path();
                if (iterator->is_regular_file(ec) && path.extension() == ".gguf"
                    && (best.empty() || path.generic_string() < best.generic_string()))
                    best = path;
                iterator.increment(ec);
            }
            return best.empty() ? best : std::filesystem::absolute(best);
        }

        static std::string utc_timestamp_slug()
        {
            const std::time_t now = std::time(nullptr);
            std::tm utc{};
#if defined(_WIN32)
            gmtime_s(&utc, &now);
#else
            gmtime_r(&now, &utc);
#endif

            char buffer[32]{};
            if (std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%SZ", &utc) == 0)
                return "timestamp";
            return buffer;
        }

#if defined(_WIN32)
        struct WinHttpUrl
        {
            std::wstring host;
            INTERNET_PORT port = 0;
            std::wstring path;
            bool secure = false;
        };

        static WinHttpUrl crack_url(const std::string& url_utf8)
        {
            // Convert to UTF-16
            int wlen = MultiByteToWideChar(CP_UTF8, 0, url_utf8.c_str(), (int)url_utf8.size(), nullptr, 0);
            if (wlen <= 0) throw std::runtime_error("WinHTTP: url UTF-8->UTF-16 failed");
            std::wstring wurl((std::size_t)wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, url_utf8.c_str(), (int)url_utf8.size(), wurl.data(), wlen);

            URL_COMPONENTS uc{};
            uc.dwStructSize = sizeof(uc);
            uc.dwSchemeLength = (DWORD)-1;
            uc.dwHostNameLength = (DWORD)-1;
            uc.dwUrlPathLength = (DWORD)-1;
            uc.dwExtraInfoLength = (DWORD)-1;

            if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc))
                throw std::runtime_error("WinHTTP: WinHttpCrackUrl failed");

            WinHttpUrl out{};
            out.secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
            out.port = uc.nPort;

            out.host.assign(uc.lpszHostName, uc.dwHostNameLength);

            std::wstring full_path;
            if (uc.dwUrlPathLength && uc.lpszUrlPath)
                full_path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
            if (uc.dwExtraInfoLength && uc.lpszExtraInfo)
                full_path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

            if (full_path.empty()) full_path = L"/";
            out.path = std::move(full_path);
            return out;
        }

        static std::string winhttp_post_json(const std::string& url, const std::string& body_utf8, const std::vector<std::pair<std::string, std::string>>& headers)
        {
            const auto u = crack_url(url);

            HINTERNET hSession = WinHttpOpen(L"EpochAI/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) throw std::runtime_error("WinHTTP: WinHttpOpen failed");
            (void)WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 180000);

            HINTERNET hConnect = WinHttpConnect(hSession, u.host.c_str(), u.port, 0);
            if (!hConnect)
            {
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpConnect failed");
            }

            DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", u.path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!hRequest)
            {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpOpenRequest failed");
            }

            // Default headers
            std::wstring hdr = L"Content-Type: application/json\r\nAccept: application/json\r\n";
            for (const auto& [k, v] : headers)
            {
                int wk = MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), nullptr, 0);
                int wv = MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), nullptr, 0);
                if (wk <= 0 || wv <= 0) continue;
                std::wstring ws_k((std::size_t)wk, L'\0');
                std::wstring ws_v((std::size_t)wv, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), ws_k.data(), wk);
                MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), ws_v.data(), wv);
                hdr += ws_k;
                hdr += L": ";
                hdr += ws_v;
                hdr += L"\r\n";
            }

            if (!WinHttpAddRequestHeaders(hRequest, hdr.c_str(), (DWORD)hdr.size(),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
            {
                // continue; not fatal
            }

            BOOL ok = WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                (LPVOID)body_utf8.data(), (DWORD)body_utf8.size(),
                (DWORD)body_utf8.size(), 0);

            if (!ok)
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpSendRequest failed");
            }

            if (!WinHttpReceiveResponse(hRequest, nullptr))
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpReceiveResponse failed");
            }

            std::string resp;
            for (;;)
            {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &avail))
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpQueryDataAvailable failed");
                }
                if (avail == 0) break;

                std::string buf(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buf.data(), avail, &read))
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpReadData failed");
                }
                buf.resize(read);
                resp += buf;
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return resp;
        }

        static std::string winhttp_get_json(const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers)
        {
            const auto u = crack_url(url);

            HINTERNET hSession = WinHttpOpen(L"EpochAI/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) throw std::runtime_error("WinHTTP: WinHttpOpen failed");
            (void)WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 180000);

            HINTERNET hConnect = WinHttpConnect(hSession, u.host.c_str(), u.port, 0);
            if (!hConnect)
            {
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpConnect failed");
            }

            DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", u.path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!hRequest)
            {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpOpenRequest failed");
            }

            std::wstring hdr = L"Accept: application/json\r\n";
            for (const auto& [k, v] : headers)
            {
                int wk = MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), nullptr, 0);
                int wv = MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), nullptr, 0);
                if (wk <= 0 || wv <= 0) continue;
                std::wstring ws_k((std::size_t)wk, L'\0');
                std::wstring ws_v((std::size_t)wv, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, k.c_str(), (int)k.size(), ws_k.data(), wk);
                MultiByteToWideChar(CP_UTF8, 0, v.c_str(), (int)v.size(), ws_v.data(), wv);
                hdr += ws_k;
                hdr += L": ";
                hdr += ws_v;
                hdr += L"\r\n";
            }

            (void)WinHttpAddRequestHeaders(hRequest, hdr.c_str(), (DWORD)hdr.size(),
                WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

            BOOL ok = WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0,
                0, 0);

            if (!ok)
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpSendRequest failed");
            }

            if (!WinHttpReceiveResponse(hRequest, nullptr))
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpReceiveResponse failed");
            }

            std::string resp;
            for (;;)
            {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &avail))
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpQueryDataAvailable failed");
                }
                if (avail == 0) break;

                std::string buf(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buf.data(), avail, &read))
                {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpReadData failed");
                }
                buf.resize(read);
                resp += buf;
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return resp;
        }
#endif

#if !defined(_WIN32) && defined(EPOCH_HAS_CURL)
        static std::size_t curl_write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
        {
            const std::size_t bytes = size * nmemb;
            if (userdata && ptr && bytes)
            {
                auto* out = static_cast<std::string*>(userdata);
                out->append(ptr, bytes);
            }
            return bytes;
        }

        static std::string http_post_json(const std::string& url,
            const std::string& body_utf8,
            const std::vector<std::pair<std::string, std::string>>& headers)
        {
            CURL* curl = curl_easy_init();
            if (!curl)
                throw std::runtime_error("CURL: curl_easy_init failed");

            std::string resp;
            struct curl_slist* request_headers = nullptr;
            request_headers = curl_slist_append(request_headers, "Content-Type: application/json");
            request_headers = curl_slist_append(request_headers, "Accept: application/json");

            for (const auto& [k, v] : headers)
            {
                std::string hdr = k;
                hdr += ": ";
                hdr += v;
                request_headers = curl_slist_append(request_headers, hdr.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_utf8.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body_utf8.size()));
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            const CURLcode code = curl_easy_perform(curl);
            if (code != CURLE_OK)
            {
                const std::string err = std::string("CURL: curl_easy_perform failed: ") + curl_easy_strerror(code);
                curl_slist_free_all(request_headers);
                curl_easy_cleanup(curl);
                throw std::runtime_error(err);
            }

            long http_status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

            curl_slist_free_all(request_headers);
            curl_easy_cleanup(curl);

            if (http_status < 200 || http_status >= 300)
            {
                std::ostringstream oss;
                oss << "CURL: HTTP status " << http_status;
                throw std::runtime_error(oss.str());
            }

            return resp;
        }

        static std::string http_get_json(const std::string& url,
            const std::vector<std::pair<std::string, std::string>>& headers)
        {
            CURL* curl = curl_easy_init();
            if (!curl)
                throw std::runtime_error("CURL: curl_easy_init failed");

            std::string resp;
            struct curl_slist* request_headers = nullptr;
            request_headers = curl_slist_append(request_headers, "Accept: application/json");

            for (const auto& [k, v] : headers)
            {
                std::string hdr = k;
                hdr += ": ";
                hdr += v;
                request_headers = curl_slist_append(request_headers, hdr.c_str());
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

            const CURLcode code = curl_easy_perform(curl);
            if (code != CURLE_OK)
            {
                const std::string err = std::string("CURL: curl_easy_perform failed: ") + curl_easy_strerror(code);
                curl_slist_free_all(request_headers);
                curl_easy_cleanup(curl);
                throw std::runtime_error(err);
            }

            long http_status = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

            curl_slist_free_all(request_headers);
            curl_easy_cleanup(curl);

            if (http_status < 200 || http_status >= 300)
            {
                std::ostringstream oss;
                oss << "CURL: HTTP status " << http_status;
                throw std::runtime_error(oss.str());
            }

            return resp;
        }
#endif

        static std::vector<std::string> extract_openai_model_ids(const std::string& response)
        {
            std::vector<std::string> ids{};
            std::string_view sv{ response };
            std::size_t pos = 0;

            while (true)
            {
                pos = sv.find("\"id\"", pos);
                if (pos == std::string_view::npos)
                    break;

                std::size_t colon = sv.find(':', pos + 4);
                if (colon == std::string_view::npos)
                    break;

                std::size_t q = colon + 1;
                while (q < sv.size() && std::isspace(static_cast<unsigned char>(sv[q])) != 0)
                    ++q;
                if (q >= sv.size() || sv[q] != '"')
                {
                    pos = colon + 1;
                    continue;
                }

                ++q;
                std::string raw;
                for (std::size_t i = q; i < sv.size(); ++i)
                {
                    const char c = sv[i];
                    if (c == '"' && (i == q || sv[i - 1] != '\\'))
                    {
                        pos = i + 1;
                        break;
                    }
                    raw.push_back(c);
                }

                std::string id = trim(json_unescape(raw));
                if (!id.empty()
                    && std::find(ids.begin(), ids.end(), id) == ids.end())
                {
                    ids.push_back(std::move(id));
                }
            }

            return ids;
        }

        static std::vector<std::string> fetch_detected_models(const std::string& endpoint)
        {
            const std::string modelsEndpoint = normalize_model_list_endpoint(endpoint);
            try
            {
#if defined(_WIN32)
                const std::string response = winhttp_get_json(modelsEndpoint, {});
#elif defined(EPOCH_HAS_CURL)
                const std::string response = http_get_json(modelsEndpoint, {});
#else
                (void)modelsEndpoint;
                core::log::error("ai", "Model detection failed: no HTTP transport is configured.");
                return {};
#endif
                return extract_openai_model_ids(response);
            }
            catch (const std::exception& ex)
            {
                std::string msg = "Model detection failed: ";
                msg += ex.what();
                core::log::warn("ai", epochengine::string_view{msg.data(), msg.size()});
                return {};
            }
        }

        static std::string resolve_model_name(const std::string& endpoint, std::string requested)
        {
            (void)endpoint;
            return requested;
        }

        static std::string extract_json_string_field_after(std::string_view sv, std::string_view field, std::size_t from = 0)
        {
            std::size_t key = sv.find(field, from);
            if (key == std::string_view::npos)
                return {};

            std::size_t colon = sv.find(':', key + field.size());
            if (colon == std::string_view::npos)
                return {};

            std::size_t q = colon + 1;
            while (q < sv.size() && std::isspace(static_cast<unsigned char>(sv[q])) != 0)
                ++q;
            if (q >= sv.size() || sv[q] != '"')
                return {};

            ++q;
            std::string raw;
            for (std::size_t i = q; i < sv.size(); ++i)
            {
                const char c = sv[i];
                if (c == '"' && (i == q || sv[i - 1] != '\\'))
                    return trim(json_unescape(raw));
                raw.push_back(c);
            }

            return {};
        }

        static std::string extract_json_error_message(const std::string& response)
        {
            std::string_view sv{ response };
            const std::size_t errorPos = sv.find("\"error\"");
            if (errorPos == std::string_view::npos)
                return {};

            return extract_json_string_field_after(sv, "\"message\"", errorPos);
        }

        static std::string extract_openai_choice_message_content(const std::string& response)
        {
            std::string_view sv{ response };
            const std::size_t choicesPos = sv.find("\"choices\"");
            if (choicesPos == std::string_view::npos)
                return {};

            if (const std::string direct = extract_json_string_field_after(sv, "\"content\"", choicesPos); !direct.empty())
                return direct;

            if (const std::string outputText = extract_json_string_field_after(sv, "\"text\"", choicesPos); !outputText.empty())
                return outputText;

            return {};
        }

        [[nodiscard]] static bool has_hidden_reasoning_without_visible_content(const std::string& response)
        {
            if (response.find("\"reasoning_content\"") == std::string::npos)
                return false;

            if (!trim(extract_openai_choice_message_content(response)).empty())
                return false;

            std::string_view sv{ response };
            if (!trim(extract_json_string_field_after(sv, "\"content\"")).empty())
                return false;

            if (!trim(extract_json_string_field_after(sv, "\"text\"")).empty())
                return false;

            return true;
        }

        [[nodiscard]] static bool is_promotable_assistant_text(std::string_view text)
        {
            const std::string candidate = trim(text);
            if (candidate.empty() || candidate == "(empty reply)")
                return false;

            const std::string lower = lowercase_ascii(candidate);
            if (starts_with_text(lower, "no ai model selected")
                || starts_with_text(lower, "os ai model could not initialize")
                || starts_with_text(lower, "engine ai model could not initialize")
                || starts_with_text(lower, "local model api error")
                || starts_with_text(lower, "local openai-compatible request failed")
                || starts_with_text(lower, "direct llama.cpp inference")
                || starts_with_text(lower, "local model returned hidden reasoning")
                || starts_with_text(lower, "local model returned no decodable assistant text")
                || starts_with_text(lower, "no decodable reply from selected local model"))
            {
                return false;
            }

            if (contains_text(lower, "\"reasoning_content\"")
                || contains_text(lower, "reasoning_content")
                || contains_text(lower, "we need to produce a response that follows the rules")
                || contains_text(lower, "the user asks:")
                || contains_text(lower, "must reply with correct english grammar")
                || contains_text(lower, "not mimic bad grammar")
                || contains_text(lower, "hidden reasoning"))
            {
                return false;
            }

            return true;
        }

        static std::string extract_lmstudio_message_content(const std::string& response)
        {
            // LM Studio v1: { "output": [ { "type":"message", "content":"..." }, ... ] , ... }
            // We do a lightweight scan to avoid dragging a JSON library in.
            auto find_str = [&](std::string_view hay, std::string_view needle, std::size_t from = 0) -> std::size_t
            {
                return hay.find(needle, from);
            };

            std::string_view sv{ response };

            std::size_t out_pos = find_str(sv, "\"output\"");
            if (out_pos == std::string_view::npos)
                return {};

            std::size_t arr_pos = find_str(sv, "[", out_pos);
            if (arr_pos == std::string_view::npos)
                return {};

            std::string last;

            // scan items by looking for "type":"message"
            std::size_t pos = arr_pos;
            while (true)
            {
                std::size_t type_pos = find_str(sv, "\"type\"", pos);
                if (type_pos == std::string_view::npos) break;

                std::size_t type_colon = find_str(sv, ":", type_pos);
                if (type_colon == std::string_view::npos) break;

                std::size_t type_q = type_colon + 1;
                while (type_q < sv.size() && (sv[type_q] == ' ' || sv[type_q] == '\t' || sv[type_q] == '\r' || sv[type_q] == '\n')) ++type_q;
                if (type_q >= sv.size() || sv[type_q] != '"') { pos = type_colon + 1; continue; }
                ++type_q;

                std::string rawType;
                std::size_t itemEnd = type_q;
                for (std::size_t i = type_q; i < sv.size(); ++i)
                {
                    const char c = sv[i];
                    if (c == '"' && sv[i - 1] != '\\')
                    {
                        itemEnd = i + 1;
                        break;
                    }
                    rawType.push_back(c);
                }

                const std::string itemType = trim(json_unescape(rawType));

                std::size_t content_key = find_str(sv, "\"content\"", itemEnd);
                if (content_key == std::string_view::npos) { pos = itemEnd; continue; }

                std::size_t colon = find_str(sv, ":", content_key);
                if (colon == std::string_view::npos) { pos = content_key + 9; continue; }

                // skip spaces
                std::size_t q = colon + 1;
                while (q < sv.size() && (sv[q] == ' ' || sv[q] == '\t' || sv[q] == '\r' || sv[q] == '\n')) ++q;
                if (q >= sv.size() || sv[q] != '"') { pos = q; continue; }
                ++q;

                // parse JSON string until unescaped quote
                std::string raw;
                for (std::size_t i = q; i < sv.size(); ++i)
                {
                    char c = sv[i];
                    if (c == '"' && (i == q || sv[i - 1] != '\\'))
                    {
                        pos = i + 1;
                        break;
                    }
                    raw.push_back(c);
                }

                std::string text = trim(json_unescape(raw));
                if (itemType == "message" && !text.empty())
                    last = std::move(text);
            }

            if (!last.empty())
                return last;

            if (const std::string openaiChoice = trim(extract_openai_choice_message_content(response)); !openaiChoice.empty())
                return openaiChoice;

            if (const std::string directContent = trim(extract_json_string_field_after(sv, "\"content\"")); !directContent.empty())
                return directContent;

            if (const std::string directText = trim(extract_json_string_field_after(sv, "\"text\"")); !directText.empty())
                return directText;

            return {};
        }

        static std::string openai_chat_complete(const std::string& endpoint_full,
            std::string_view model,
            std::string_view system_prompt,
            std::string_view input,
            const std::vector<std::pair<std::string, std::string>>& headers)
        {
            const auto build_body = [&](bool includeReasoning) -> std::string
            {
                std::string body;
                body.reserve(256 + input.size());
                body += "{";
                (void)includeReasoning;
                body += "\"model\":\"" + json_escape(model) + "\",";
                body += "\"messages\":[";
                body += "{\"role\":\"system\",\"content\":\"" + json_escape(system_prompt) + "\"},";
                body += "{\"role\":\"user\",\"content\":\"" + json_escape(input) + "\"}";
                body += "],";
                body += "\"max_tokens\":768,";
                body += "\"stream\":false";
                body += "}";
                return body;
            };

            const auto request_once = [&](bool includeReasoning, std::string* rawResponse) -> std::string
            {
                const std::string body = build_body(includeReasoning);
#if defined(_WIN32)
                const std::string resp = winhttp_post_json(endpoint_full, body, headers);
#elif defined(EPOCH_HAS_CURL)
                const std::string resp = http_post_json(endpoint_full, body, headers);
#else
                (void)endpoint_full;
                (void)headers;
                core::log::error("ai", "Local OpenAI-compatible request failed: no non-Windows HTTP transport is configured (build with libcurl).");
                return {};
#endif
                if (rawResponse)
                    *rawResponse = resp;
                std::string parsed = trim(extract_lmstudio_message_content(resp));
                if (parsed.empty())
                    parsed = trim(extract_openai_choice_message_content(resp));
                if (parsed.empty())
                {
                    std::string_view sv{ resp };
                    parsed = trim(extract_json_string_field_after(sv, "\"content\""));
                    if (parsed.empty())
                        parsed = trim(extract_json_string_field_after(sv, "\"text\""));
                }
                if (!parsed.empty())
                    return parsed;

                if (!resp.empty())
                {
                    if (has_hidden_reasoning_without_visible_content(resp))
                    {
                        std::string warn = "Local OpenAI-compatible reply contained hidden reasoning without visible assistant content. bytes=";
                        warn += std::to_string(resp.size());
                        core::log::warn("ai", epochengine::string_view{warn.data(), warn.size()});
                        return {};
                    }

                    std::string snippet = trim(resp.substr(0, (std::min)(resp.size(), static_cast<std::size_t>(240))));
                    if (snippet.empty())
                        snippet = "(non-empty body with no decodable content)";
                    std::string warn = "Local OpenAI-compatible reply body could not be decoded. bytes=";
                    warn += std::to_string(resp.size());
                    warn += " snippet=";
                    warn += snippet;
                    core::log::warn("ai", epochengine::string_view{warn.data(), warn.size()});
                }

                return {};
            };

            try
            {
                std::string rawResponse{};
                std::string reply = request_once(false, &rawResponse);
                if (!reply.empty())
                    return reply;

                const std::string error = extract_json_error_message(rawResponse);
                if (!error.empty())
                {
                    core::log::warn("ai", epochengine::string_view{error.data(), error.size()});
                    return std::string("Local model API error: ") + error;
                }
                if (has_hidden_reasoning_without_visible_content(rawResponse))
                    return "Local model returned hidden reasoning without visible assistant content. Select a content-producing model or disable reasoning export before using OS AI chat.";
                return "Local model returned no decodable assistant text. Check the selected model, endpoint, and OpenAI-compatible /v1/chat/completions response.";
            }
            catch (const std::exception& ex)
            {
                std::string msg = "Local OpenAI-compatible request failed: ";
                msg += ex.what();
                core::log::error("ai", epochengine::string_view{msg.data(), msg.size()});
                return {};
            }
        }

        struct ProcessCapture
        {
            std::string output{};
            int exit_code{-1};
            bool launched{};
            bool timed_out{};
        };

        constexpr std::size_t kMaximumInferenceOutputBytes = 16u * 1024u * 1024u;

        static void append_process_output(std::string& output, const char* data, std::size_t size)
        {
            if (output.size() >= kMaximumInferenceOutputBytes)
                return;
            output.append(data, (std::min)(size, kMaximumInferenceOutputBytes - output.size()));
        }

#if defined(_WIN32)
        [[nodiscard]] static std::wstring utf8_to_wide(std::string_view value)
        {
            if (value.empty())
                return {};
            const int required = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (required <= 0)
                return {};
            std::wstring converted(static_cast<std::size_t>(required), L'\0');
            if (MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                    converted.data(), required) != required)
                return {};
            return converted;
        }

        [[nodiscard]] static std::wstring quote_windows_argument(std::wstring_view argument)
        {
            if (argument.find_first_of(L" \t\"") == std::wstring_view::npos)
                return std::wstring{argument};

            std::wstring quoted{L"\""};
            std::size_t slashes = 0u;
            for (const wchar_t character : argument)
            {
                if (character == L'\\')
                {
                    ++slashes;
                    continue;
                }
                if (character == L'\"')
                {
                    quoted.append(slashes * 2u + 1u, L'\\');
                    quoted.push_back(L'\"');
                    slashes = 0u;
                    continue;
                }
                quoted.append(slashes, L'\\');
                slashes = 0u;
                quoted.push_back(character);
            }
            quoted.append(slashes * 2u, L'\\');
            quoted.push_back(L'\"');
            return quoted;
        }

        [[nodiscard]] static ProcessCapture capture_process(
            const std::filesystem::path& executable,
            const std::vector<std::string>& arguments,
            std::chrono::seconds timeout)
        {
            ProcessCapture capture{};
            SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            if (!CreatePipe(&readPipe, &writePipe, &security, 0))
                return capture;
            SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

            std::wstring command = quote_windows_argument(executable.wstring());
            for (const auto& argument : arguments)
            {
                command.push_back(L' ');
                command += quote_windows_argument(utf8_to_wide(argument));
            }
            command.push_back(L'\0');

            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            startup.wShowWindow = SW_HIDE;
            startup.hStdOutput = writePipe;
            startup.hStdError = writePipe;
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION process{};
            capture.launched = CreateProcessW(
                executable.wstring().c_str(), command.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr, executable.parent_path().wstring().c_str(),
                &startup, &process) != FALSE;
            CloseHandle(writePipe);
            if (!capture.launched)
            {
                CloseHandle(readPipe);
                return capture;
            }

            const auto deadline = std::chrono::steady_clock::now() + timeout;
            bool running = true;
            char buffer[8192];
            while (running)
            {
                DWORD available = 0u;
                while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0u)
                {
                    DWORD read = 0u;
                    const DWORD requested = (std::min)(available, static_cast<DWORD>(sizeof(buffer)));
                    if (!ReadFile(readPipe, buffer, requested, &read, nullptr) || read == 0u)
                        break;
                    append_process_output(capture.output, buffer, read);
                }
                running = WaitForSingleObject(process.hProcess, 20u) == WAIT_TIMEOUT;
                if (running && std::chrono::steady_clock::now() >= deadline)
                {
                    capture.timed_out = true;
                    TerminateProcess(process.hProcess, 124u);
                    WaitForSingleObject(process.hProcess, 5000u);
                    running = false;
                }
            }

            DWORD read = 0u;
            while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0u)
                append_process_output(capture.output, buffer, read);
            DWORD exitCode = 1u;
            GetExitCodeProcess(process.hProcess, &exitCode);
            capture.exit_code = static_cast<int>(exitCode);
            CloseHandle(readPipe);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return capture;
        }
#else
        [[nodiscard]] static ProcessCapture capture_process(
            const std::filesystem::path& executable,
            const std::vector<std::string>& arguments,
            std::chrono::seconds timeout)
        {
            ProcessCapture capture{};
            int outputPipe[2]{};
            if (pipe(outputPipe) != 0)
                return capture;

            const pid_t child = fork();
            if (child < 0)
            {
                close(outputPipe[0]);
                close(outputPipe[1]);
                return capture;
            }
            if (child == 0)
            {
                dup2(outputPipe[1], STDOUT_FILENO);
                dup2(outputPipe[1], STDERR_FILENO);
                close(outputPipe[0]);
                close(outputPipe[1]);

                std::vector<std::string> owned;
                owned.reserve(arguments.size() + 1u);
                owned.push_back(executable.string());
                owned.insert(owned.end(), arguments.begin(), arguments.end());
                std::vector<char*> argv;
                argv.reserve(owned.size() + 1u);
                for (auto& argument : owned)
                    argv.push_back(argument.data());
                argv.push_back(nullptr);
                execvp(argv.front(), argv.data());
                _exit(127);
            }

            capture.launched = true;
            close(outputPipe[1]);
            const int flags = fcntl(outputPipe[0], F_GETFL, 0);
            if (flags >= 0)
                fcntl(outputPipe[0], F_SETFL, flags | O_NONBLOCK);

            const auto deadline = std::chrono::steady_clock::now() + timeout;
            int status = 0;
            bool running = true;
            char buffer[8192];
            while (running)
            {
                for (;;)
                {
                    const ssize_t read = ::read(outputPipe[0], buffer, sizeof(buffer));
                    if (read <= 0)
                        break;
                    append_process_output(capture.output, buffer, static_cast<std::size_t>(read));
                }

                const pid_t waited = waitpid(child, &status, WNOHANG);
                running = waited == 0;
                if (running && std::chrono::steady_clock::now() >= deadline)
                {
                    capture.timed_out = true;
                    kill(child, SIGTERM);
                    std::this_thread::sleep_for(std::chrono::milliseconds{250});
                    if (waitpid(child, &status, WNOHANG) == 0)
                        kill(child, SIGKILL);
                    waitpid(child, &status, 0);
                    running = false;
                }
                if (running)
                    std::this_thread::sleep_for(std::chrono::milliseconds{20});
            }

            for (;;)
            {
                const ssize_t read = ::read(outputPipe[0], buffer, sizeof(buffer));
                if (read <= 0)
                    break;
                append_process_output(capture.output, buffer, static_cast<std::size_t>(read));
            }
            close(outputPipe[0]);
            capture.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
            return capture;
        }
#endif

        [[nodiscard]] static std::string llama_cpp_complete(
            const EngineAiModel::Config& config,
            std::string_view systemPrompt,
            std::string_view userText)
        {
            std::vector<std::string> arguments{
                "--model", config.model,
                "--offline",
                "--log-disable",
                "--no-display-prompt",
                "--no-show-timings",
                "--single-turn",
                "--system-prompt", std::string{systemPrompt},
                "--prompt", std::string{userText},
                "--ctx-size", std::to_string(config.context_tokens),
                "--n-predict", std::to_string(config.output_tokens),
                "--gpu-layers", std::to_string(config.gpu_layers)
            };
            if (config.threads > 0u)
            {
                arguments.push_back("--threads");
                arguments.push_back(std::to_string(config.threads));
            }

            ProcessCapture capture = capture_process(
                std::filesystem::path{config.executable}, arguments,
                std::chrono::seconds{config.timeout_seconds});
            if (!capture.launched)
                return "Direct llama.cpp inference could not start. Check the configured llama-cli executable.";
            if (capture.timed_out)
                return "Direct llama.cpp inference exceeded its time budget and was stopped.";
            if (capture.exit_code != 0)
            {
                const std::string detail = trim(capture.output);
                return "Direct llama.cpp inference failed with exit code "
                    + std::to_string(capture.exit_code)
                    + (detail.empty() ? std::string{"."} : std::string{": "} + detail);
            }
            return trim(capture.output);
        }
        static std::string build_transcript(std::string_view system_prompt, std::string_view user_text)
        {
            // Keep it tight: context kills latency on local models.
            std::string t;
            t.reserve(system_prompt.size() + user_text.size() + 64);
            // system_prompt is sent separately; do not duplicate here.
            t.append("user: ");
            t.append(user_text);
            return t;
        }
    } // namespace

    EngineAiModel::EngineAiModel(Config cfg)
        : m_cfg(std::move(cfg))
    {
        if (m_cfg.backend.empty())
            m_cfg.backend = "openai_chat";
        if (m_cfg.best_of == 0)
            m_cfg.best_of = 1;

        if (m_cfg.backend == "llama_cpp_cli")
        {
            g_localTransport = LocalInferenceTransport::LlamaCppCli;
            g_directExecutable = m_cfg.executable;
            g_directModel = m_cfg.model;
        }
        else
        {
            m_cfg.backend = "openai_chat";
            m_endpoint_full = normalize_openai_chat_endpoint(m_cfg.endpoint);
            m_cfg.model = resolve_model_name(m_cfg.endpoint, m_cfg.model);
            g_localTransport = LocalInferenceTransport::OpenAiCompatible;
            g_selectedEndpoint = m_cfg.endpoint;
        }
        g_providerMode = ProviderMode::OpenSourceLocal;
        g_selectedModel = m_cfg.backend == "llama_cpp_cli"
            ? std::filesystem::path{m_cfg.model}.filename().string()
            : m_cfg.model;
        if (!m_cfg.model.empty())
        {
            std::string msg = "OS AI model: ";
            msg += m_cfg.model;
            core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        }
    }

    EngineAiReply EngineAiModel::submit(std::string_view user_input)
    {
        EngineAiReply out{};

        const std::string sys =
            "You are the selected open-source OS AI model.\n"
            "Epoch is a C++23 game engine, editor, renderer, tooling, and AI self-iteration codebase; never interpret engine tasks as vehicle repair.\n"
            "Rules:\n"
            " - Reply with correct English grammar.\n"
            " - Capitalize the first letter of the response.\n"
            " - Do not mimic the user's bad grammar.\n"
            " - Put only the final answer in assistant content; do not include or rely on hidden reasoning.\n"
            " - Stay grounded in the current Epoch editor/project context.\n"
            " - Prefer concrete editor, scene, engine, and C++ guidance that teaches the OS AI harness what to do next.\n"
            " - MCP tool calls are bounded requests; Epoch validates and executes them, then returns structured evidence to the selected model.\n"
            " - If asked whether Epoch, OS AI, or a development pass is working, cite the visible tool, build, scene, packet, log, or capture evidence that proves it.\n"
            " - Treat sandboxed 3D scene work as a harness exercise: name the intended edit, tool call, evidence, and pass/fail condition.\n"
            " - Keep self-iteration separate from normal ProjectLauncher game/software editing unless the operator explicitly asks to change the project/editor scene.\n"
            " - When suggesting project or file work, keep it relevant to the active engine/runtime context instead of drifting into generic setup advice.\n";

        const std::size_t n = std::max<std::size_t>(1, m_cfg.best_of);

        // No scorer is active in this lane; first non-empty assistant content wins.
        for (std::size_t i = 0; i < n; ++i)
        {
            std::string txt = m_cfg.backend == "llama_cpp_cli"
                ? llama_cpp_complete(m_cfg, sys, user_input)
                : openai_chat_complete(
                    m_endpoint_full,
                    m_cfg.model,
                    sys,
                    build_transcript(sys, user_input),
                    {});

            txt = trim(txt);
            if (txt.empty())
                continue;

            Candidate c{ txt, 0.0 };
            out.alternatives.push_back(c);

            if (out.text.empty())
            {
                out.text = txt;
                out.score = 0.0;
                break;
            }
        }

        return out;
    }

    void init_engine_ai()
    {
        if (g_engineAi)
            return;

        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::LlamaCppCli)
        {
            if (const auto executable = discover_llama_executable(); !executable.empty())
                g_directExecutable = executable.generic_string();
            if (const auto model = discover_gguf_model(); !model.empty())
                g_directModel = model.generic_string();
            if (!regular_file(g_directExecutable) || !regular_file(g_directModel))
            {
                g_modelDetectionStatus = "Direct llama.cpp runtime needs both llama-cli and a GGUF model.";
                core::log::info("ai", "OS AI direct runtime not initialized: llama-cli or GGUF model is missing.");
                return;
            }

            g_engineAi = new EngineAiModel({
                .backend = "llama_cpp_cli",
                .model = g_directModel,
                .executable = g_directExecutable,
                .threads = (std::max)(1u, std::thread::hardware_concurrency()),
                .context_tokens = 4096,
                .output_tokens = 512,
                .gpu_layers = -1,
                .timeout_seconds = 120,
                .best_of = 1
            });
            g_modelDetectionStatus = "Direct llama.cpp runtime initialized with "
                + std::filesystem::path{g_directModel}.filename().string() + ".";
        }
        else
        {
            restore_selected_model_preference_if_needed();
            if (g_detectedModels.empty() && g_modelDetectionStatus == "Not scanned.")
                (void)refresh_detected_models();
            if (g_selectedModel.empty())
            {
                core::log::info("ai", "OS AI model not initialized: no local model selected.");
                return;
            }

            g_engineAi = new EngineAiModel({
                .backend = "openai_chat",
                .endpoint = g_selectedEndpoint,
                .model = g_selectedModel,
                .best_of = 1
            });
        }

        core::log::info("ai", "OS AI model initialized");
        {
            std::string msg = "AI provider: ";
            msg += active_provider_summary();
            core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        }
        {
            std::string msg = g_localTransport == LocalInferenceTransport::LlamaCppCli
                ? "AI executable: " + g_directExecutable
                : "AI endpoint: " + g_selectedEndpoint;
            core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        }
        {
            std::string msg = "AI raw capture path: ";
            msg += local_model_exchange_jsonl_path();
            core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        }
        {
            std::string msg = "AI tool evidence capture path: ";
            msg += local_tool_trace_jsonl_path();
            core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        }
    }
    void shutdown_engine_ai()
    {
        delete g_engineAi;
        g_engineAi = nullptr;
        core::log::info("ai", "OS AI model shutdown");
    }


    std::string default_workspace_root()
    {
        const auto workspace = epochengine::core::path::example_console_workspace_dir();
        if (!workspace.empty())
            return workspace.generic_string();

        return {};
    }

    std::string research_staging_root()
    {
        return default_workspace_root() + "/research/staged";
    }

    std::string iteration_packet_root()
    {
        return default_workspace_root() + "/ai/iterations";
    }

    std::string curated_datasets_root()
    {
        return "Engine/ai/datasets/curated";
    }

    std::string evals_root()
    {
        return "Engine/ai/evals";
    }

    std::string prompts_root()
    {
        return "Engine/ai/prompts";
    }

    std::string manifests_root()
    {
        return "Engine/ai/manifests";
    }

    std::string local_model_exchange_jsonl_path()
    {
        return default_workspace_root() + "/model_exchange.jsonl";
    }

    std::string local_tool_trace_jsonl_path()
    {
        return default_workspace_root() + "/tool_trace.jsonl";
    }

    std::string local_session_root()
    {
        return default_workspace_root() + "/ai/sessions";
    }

    std::string local_model_root()
    {
        return executable_cache_bucket("models");
    }

    std::string local_cache_root()
    {
        return executable_cache_bucket("ai");
    }

    ProviderMode current_provider_mode() noexcept
    {
        return g_providerMode;
    }

    LocalInferenceTransport current_local_inference_transport() noexcept
    {
        restore_runtime_preference_if_needed();
        return g_localTransport;
    }

    std::string_view local_inference_transport_name(LocalInferenceTransport transport) noexcept
    {
        switch (transport)
        {
        case LocalInferenceTransport::OpenAiCompatible:
            return "Local OpenAI-compatible API";
        case LocalInferenceTransport::LlamaCppCli:
            return "Direct llama.cpp CLI";
        }
        return "Unknown local runtime";
    }

    DirectRuntimeStatus direct_runtime_status()
    {
        restore_runtime_preference_if_needed();
        DirectRuntimeStatus status{};
        status.executable = g_directExecutable;
        status.model = g_directModel;
        status.executable_ready = regular_file(status.executable);
        status.model_ready = regular_file(status.model)
            && std::filesystem::path{status.model}.extension() == ".gguf";
        if (status.ready())
            status.message = "Direct llama.cpp runtime is ready.";
        else if (!status.executable_ready && !status.model_ready)
            status.message = "llama-cli and a GGUF model are required.";
        else if (!status.executable_ready)
            status.message = "A GGUF model was found; llama-cli is missing.";
        else
            status.message = "llama-cli was found; place a GGUF model in cache/models.";
        return status;
    }

    DirectRuntimeStatus discover_direct_runtime()
    {
        restore_runtime_preference_if_needed();
        if (const auto executable = discover_llama_executable(); !executable.empty())
            g_directExecutable = executable.generic_string();
        if (const auto model = discover_gguf_model(); !model.empty())
            g_directModel = model.generic_string();
        const DirectRuntimeStatus status = direct_runtime_status();
        g_modelDetectionStatus = status.message;
        return status;
    }

    bool select_direct_runtime(std::string_view executable, std::string_view model)
    {
        const std::filesystem::path executablePath{trim(executable)};
        const std::filesystem::path modelPath{trim(model)};
        if (!regular_file(executablePath) || !regular_file(modelPath) || modelPath.extension() != ".gguf")
        {
            g_modelDetectionStatus = "Direct runtime selection rejected: choose an existing llama-cli executable and GGUF model.";
            return false;
        }

        shutdown_engine_ai();
        g_directExecutable = std::filesystem::absolute(executablePath).generic_string();
        g_directModel = std::filesystem::absolute(modelPath).generic_string();
        g_localTransport = LocalInferenceTransport::LlamaCppCli;
        g_selectedModel = modelPath.filename().string();
        persist_runtime_preference();
        init_engine_ai();
        return g_engineAi != nullptr;
    }

    void select_openai_compatible_runtime()
    {
        shutdown_engine_ai();
        g_localTransport = LocalInferenceTransport::OpenAiCompatible;
        persist_runtime_preference();
        init_engine_ai();
    }

    std::string active_model_name()
    {
        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::LlamaCppCli)
            return g_directModel.empty() ? std::string{} : std::filesystem::path{g_directModel}.filename().string();
        restore_selected_model_preference_if_needed();
        return g_selectedModel;
    }

    std::string active_provider_summary()
    {
        restore_runtime_preference_if_needed();
        std::string summary = std::string(local_inference_transport_name(g_localTransport));
        const std::string modelName = active_model_name();
        if (!modelName.empty())
        {
            summary += " :: ";
            summary += modelName;
        }
        return summary;
    }

    std::vector<std::string> detected_model_names()
    {
        return g_detectedModels;
    }

    std::string model_detection_status()
    {
        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::OpenAiCompatible)
            restore_selected_model_preference_if_needed();
        return g_modelDetectionStatus;
    }

    bool is_engine_ai_initialized() noexcept
    {
        return g_engineAi != nullptr;
    }

    std::string model_connection_status()
    {
        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::LlamaCppCli)
        {
            const auto status = direct_runtime_status();
            if (!status.ready())
                return status.message;
            if (!g_engineAi)
                init_engine_ai();
            return g_engineAi
                ? "Direct llama.cpp client is initialized."
                : "Direct llama.cpp files are present, but the client did not initialize.";
        }

        restore_selected_model_preference_if_needed();
        if (g_selectedModel.empty())
            return "No local OS model selected.";
        if (g_engineAi)
            return "Selected model client is initialized.";
        init_engine_ai();
        if (g_engineAi)
            return "Selected model client is initialized.";
        return "Model selected, but client is not initialized; scan models or check the endpoint.";
    }
    std::vector<std::string> refresh_detected_models()
    {
        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::LlamaCppCli)
        {
            const auto status = discover_direct_runtime();
            g_detectedModels.clear();
            if (status.model_ready)
                g_detectedModels.push_back(std::filesystem::path{status.model}.filename().string());
            return g_detectedModels;
        }
        restore_selected_model_preference_if_needed();
        g_detectedModels = fetch_detected_models(g_selectedEndpoint);
        if (g_detectedModels.empty())
        {
            g_modelDetectionStatus = g_selectedModel.empty()
                ? "No local models detected. Start LM Studio, Ollama, or another OpenAI-compatible API and check endpoint."
                : "Configured model '" + g_selectedModel + "' is selected, but no local model inventory was detected at the endpoint.";
        }
        else
        {
            if (!g_selectedModel.empty())
            {
                const bool selectedAvailable =
                    std::find(g_detectedModels.begin(), g_detectedModels.end(), g_selectedModel) != g_detectedModels.end();
                if (selectedAvailable)
                {
                    if (!g_engineAi)
                        init_engine_ai();

                    g_modelDetectionStatus = g_engineAi
                        ? "Selected and initialized model client: " + g_selectedModel
                        : "Selected model: " + g_selectedModel + " (" + std::to_string(g_detectedModels.size()) + " local model(s) detected), but client init failed.";
                    persist_selected_model_preference(g_selectedModel);
                }
                else
                {
                    const std::string staleModel = g_selectedModel;
                    delete g_engineAi;
                    g_engineAi = nullptr;
                    g_modelDetectionStatus =
                        "Selected model '" + staleModel + "' was not reported by the endpoint; keeping the operator preference. Choose another detected model to replace it.";
                }
            }
            else
            {
                g_modelDetectionStatus = "Detected " + std::to_string(g_detectedModels.size()) + " local model(s); choose the exact OS AI model to enable chat/tooling.";
            }
        }

        return g_detectedModels;
    }

    bool select_active_model(std::string_view model_id)
    {
        if (current_local_inference_transport() == LocalInferenceTransport::LlamaCppCli)
            return select_direct_runtime(g_directExecutable, g_directModel);

        const std::string selected = trim(model_id);
        if (selected.empty())
            return false;

        restore_selected_model_preference_if_needed();
        if (g_detectedModels.empty())
            g_detectedModels = fetch_detected_models(g_selectedEndpoint);

        if (std::find(g_detectedModels.begin(), g_detectedModels.end(), selected) == g_detectedModels.end())
        {
            g_modelDetectionStatus = "Could not select OS model '" + selected + "' because the endpoint did not report it.";
            return false;
        }

        delete g_engineAi;
        g_engineAi = nullptr;
        g_selectedModel = selected;
        persist_selected_model_preference(selected);
        init_engine_ai();
        g_modelDetectionStatus = g_engineAi
            ? "Selected and initialized model client: " + selected
            : "Selected model '" + selected + "', but the OS AI client did not initialize.";

        std::string msg = "AI model selected: ";
        msg += selected;
        core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        return true;
    }

    ModelManifest active_model_manifest()
    {
        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::OpenAiCompatible)
            restore_selected_model_preference_if_needed();
        const std::string selectedModel = active_model_name();
        ModelManifest manifest{};
        manifest.id = selectedModel;
        manifest.provider = g_providerMode;
        manifest.endpoint = g_localTransport == LocalInferenceTransport::LlamaCppCli
            ? g_directExecutable : g_selectedEndpoint;
        manifest.repo_safe_manifest = true;
        manifest.local_weights_only = g_localTransport == LocalInferenceTransport::LlamaCppCli;
        manifest.available = g_localTransport == LocalInferenceTransport::LlamaCppCli
            ? direct_runtime_status().ready() : !selectedModel.empty();

        manifest.display_name = selectedModel;
        manifest.manifest_path = manifests_root() + "/open_source_model_provider.json";

        return manifest;
    }

    EvidencePaths default_evidence_paths()
    {
        return EvidencePaths{
            .workspace_root = default_workspace_root(),
            .model_exchange_jsonl = local_model_exchange_jsonl_path(),
            .tool_trace_jsonl = local_tool_trace_jsonl_path(),
            .session_root = local_session_root(),
            .model_root = local_model_root(),
            .cache_root = local_cache_root(),
            .curated_dataset_root = curated_datasets_root(),
            .eval_root = evals_root()
        };
    }

    void append_tool_trace(const McpCaptureRecord& record)
    {
        const EvidencePaths paths = default_evidence_paths();
        const std::filesystem::path file = paths.tool_trace_jsonl;

        std::ostringstream oss;
        oss << "{";
        oss << "\"session_id\":\"" << json_escape(record.session_id) << "\",";
        oss << "\"call_id\":\"" << json_escape(record.call_id) << "\",";
        oss << "\"server\":\"" << json_escape(record.server) << "\",";
        oss << "\"tool\":\"" << json_escape(record.tool) << "\",";
        oss << "\"prompt\":\"" << json_escape(record.prompt) << "\",";
        oss << "\"normalized_output\":\"" << json_escape(record.normalized_output) << "\",";
        oss << "\"source_path\":\"" << json_escape(record.source_path) << "\",";
        oss << "\"state\":" << static_cast<unsigned>(record.state) << ",";
        oss << "\"error\":" << static_cast<unsigned>(record.error);
        oss << "}";

        if (append_jsonl_line(file, oss.str()))
        {
            static bool loggedCapturePath = false;
            if (!loggedCapturePath)
            {
                loggedCapturePath = true;
                std::string msg = "AI appended structured tool trace: ";
                msg += file.string();
                core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
            }
        }
    }

    std::string stage_iteration_packet(const IterationPacket& packet)
    {
        const std::string packetSlug = slugify(
            packet.packet_name.empty()
                ? (packet.project_id.empty() ? std::string("epoch-iteration") : packet.project_id + "-iteration")
                : packet.packet_name);
        const std::string timestamp = utc_timestamp_slug();
        const std::filesystem::path packetDir =
            std::filesystem::path(iteration_packet_root()) / (packetSlug + "-" + timestamp);
        const std::filesystem::path jsonFile = packetDir / "iteration.json";
        const std::filesystem::path taskFile = packetDir / "task.md";

        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"staged\",\n";
        json << "  \"created_utc\": \"" << json_escape(timestamp) << "\",\n";
        json << "  \"packet_name\": \"" << json_escape(packet.packet_name) << "\",\n";
        json << "  \"task_prompt\": \"" << json_escape(packet.task_prompt) << "\",\n";
        json << "  \"assistant_hint\": \"" << json_escape(packet.assistant_hint) << "\",\n";
        json << "  \"operator_notes\": \"" << json_escape(packet.operator_notes) << "\",\n";
        json << "  \"control_loop_stage\": \"" << json_escape(packet.control_loop_stage) << "\",\n";
        json << "  \"review_gate_state\": \"" << json_escape(packet.review_gate_state) << "\",\n";
        json << "  \"review_gate_evidence\": \"" << json_escape(packet.review_gate_evidence) << "\",\n";
        json << "  \"project_id\": \"" << json_escape(packet.project_id) << "\",\n";
        json << "  \"project_name\": \"" << json_escape(packet.project_name) << "\",\n";
        json << "  \"scene_id\": \"" << json_escape(packet.scene_id) << "\",\n";
        json << "  \"project_root\": \"" << json_escape(packet.project_root) << "\",\n";
        json << "  \"scene_path\": \"" << json_escape(packet.scene_path) << "\",\n";
        json << "  \"active_script\": \"" << json_escape(packet.active_script) << "\",\n";
        json << "  \"build_log_path\": \"" << json_escape(packet.build_log_path) << "\",\n";
        json << "  \"output_path\": \"" << json_escape(packet.output_path) << "\",\n";
        json << "  \"provider_summary\": \"" << json_escape(packet.provider_summary) << "\",\n";
        json << "  \"active_model\": \"" << json_escape(packet.active_model) << "\",\n";
        json << "  \"manifest_path\": \"" << json_escape(packet.manifest_path) << "\",\n";
        json << "  \"workspace_root\": \"" << json_escape(packet.workspace_root) << "\",\n";
        json << "  \"model_exchange_path\": \"" << json_escape(packet.model_exchange_path) << "\",\n";
        json << "  \"tool_trace_path\": \"" << json_escape(packet.tool_trace_path) << "\",\n";
        json << "  \"session_root\": \"" << json_escape(packet.session_root) << "\",\n";
        json << "  \"model_root\": \"" << json_escape(packet.model_root) << "\",\n";
        json << "  \"cache_root\": \"" << json_escape(packet.cache_root) << "\",\n";
        json << "  \"curated_dataset_root\": \"" << json_escape(packet.curated_dataset_root) << "\",\n";
        json << "  \"eval_root\": \"" << json_escape(packet.eval_root) << "\",\n";
        json << "  \"evidence_paths\": [";
        for (std::size_t i = 0; i < packet.evidence_paths.size(); ++i)
        {
            if (i != 0)
                json << ", ";
            json << "\"" << json_escape(packet.evidence_paths[i]) << "\"";
        }
        json << "]\n";
        json << "}\n";

        std::ostringstream task;
        task << "# " << (packet.packet_name.empty() ? "Epoch Iteration Packet" : packet.packet_name) << "\n\n";
        task << "## Task Prompt\n" << (packet.task_prompt.empty() ? "(empty)" : packet.task_prompt) << "\n\n";
        task << "## Assistant Hint\n" << (packet.assistant_hint.empty() ? "(none)" : packet.assistant_hint) << "\n\n";
        task << "## Operator Notes\n" << (packet.operator_notes.empty() ? "(none)" : packet.operator_notes) << "\n\n";
        task << "## Self-Iteration Loop\n";
        task << "- Stage: " << (packet.control_loop_stage.empty() ? "(unknown)" : packet.control_loop_stage) << "\n";
        task << "- Review gate: " << (packet.review_gate_state.empty() ? "(unknown)" : packet.review_gate_state) << "\n";
        task << "- Evidence: " << (packet.review_gate_evidence.empty() ? "(none)" : packet.review_gate_evidence) << "\n";
        task << "- Contract: planner -> executor -> builder -> verifier -> gate; staged packets only, no blind write-through.\n\n";
        task << "## Runtime Snapshot\n";
        task << "- Provider: " << packet.provider_summary << "\n";
        task << "- Active model: " << (packet.active_model.empty() ? "(none selected)" : packet.active_model) << "\n";
        task << "- Manifest: " << packet.manifest_path << "\n";
        task << "- Workspace root: " << packet.workspace_root << "\n";
        task << "- Raw capture: " << packet.model_exchange_path << "\n";
        task << "- Tool evidence capture: " << packet.tool_trace_path << "\n";
        task << "- Curated datasets: " << packet.curated_dataset_root << "\n";
        task << "- Eval root: " << packet.eval_root << "\n";
        task << "- Checkpoints: " << packet.session_root << "\n";
        task << "- Local models: " << packet.model_root << "\n";
        task << "- Cache: " << packet.cache_root << "\n\n";
        task << "## Project Snapshot\n";
        task << "- Project id: " << packet.project_id << "\n";
        task << "- Project name: " << packet.project_name << "\n";
        task << "- Scene id: " << packet.scene_id << "\n";
        task << "- Project root: " << packet.project_root << "\n";
        task << "- Scene path: " << packet.scene_path << "\n";
        task << "- Active script: " << packet.active_script << "\n";
        task << "- Build log: " << packet.build_log_path << "\n";
        task << "- Output path: " << packet.output_path << "\n\n";
        task << "## Evidence Paths\n";
        if (packet.evidence_paths.empty())
        {
            task << "- (none)\n";
        }
        else
        {
            for (const auto& path : packet.evidence_paths)
                task << "- " << path << "\n";
        }

        if (!write_text_file(jsonFile, json.str()) || !write_text_file(taskFile, task.str()))
            return {};

        std::string msg = "AI staged iteration packet: ";
        msg += packetDir.string();
        core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        return packetDir.string();
    }

    bool promote_dataset_record(const DatasetRecord& record, std::string_view dataset_name)
    {
        const std::filesystem::path datasetFile =
            std::filesystem::path(curated_datasets_root()) / (slugify(dataset_name) + ".jsonl");

        std::ostringstream oss;
        oss << "{";
        oss << "\"prompt\":\"" << json_escape(record.prompt) << "\",";
        oss << "\"answer\":\"" << json_escape(record.answer) << "\",";
        oss << "\"source\":\"" << json_escape(record.source) << "\",";
        oss << "\"role\":\"" << json_escape(record.role) << "\",";
        oss << "\"tags\":[";
        for (std::size_t i = 0; i < record.tags.size(); ++i)
        {
            if (i != 0)
                oss << ",";
            oss << "\"" << json_escape(record.tags[i]) << "\"";
        }
        oss << "]";
        oss << "}";

        return append_jsonl_line(datasetFile, oss.str());
    }

    bool promote_tool_trace_record(const McpCaptureRecord& record, std::string_view dataset_name)
    {
        std::string source = record.server;
        if (!record.tool.empty())
        {
            if (!source.empty())
                source += ":";
            source += record.tool;
        }
        if (!record.source_path.empty())
        {
            if (!source.empty())
                source += ":";
            source += record.source_path;
        }

        std::vector<std::string> tags{
            "mcp-capture",
            slugify(record.server),
            slugify(record.tool)
        };
        if (!record.source_path.empty())
            tags.push_back("scene-guidance");

        return promote_dataset_record(DatasetRecord{
            .prompt = record.prompt,
            .answer = record.normalized_output,
            .source = std::move(source),
            .role = "assistant",
            .tags = std::move(tags)
        }, dataset_name);
    }

    bool promote_eval_case(const EvalCase& record, std::string_view suite_name)
    {
        const std::string fileName = slugify(suite_name) + "-" + slugify(record.name) + ".json";
        const std::filesystem::path evalFile = std::filesystem::path(evals_root()) / fileName;

        std::ostringstream oss;
        oss << "{\n";
        oss << "  \"suite\": \"" << json_escape(suite_name) << "\",\n";
        oss << "  \"name\": \"" << json_escape(record.name) << "\",\n";
        oss << "  \"prompt\": \"" << json_escape(record.prompt) << "\",\n";
        oss << "  \"expected_contains\": \"" << json_escape(record.expected_contains) << "\",\n";
        oss << "  \"project_id\": \"" << json_escape(record.project_id) << "\",\n";
        oss << "  \"scene_id\": \"" << json_escape(record.scene_id) << "\"\n";
        oss << "}\n";

        return write_text_file(evalFile, oss.str());
    }

    bool is_promotable_assistant_reply(std::string_view reply)
    {
        return is_promotable_assistant_text(reply);
    }

    HelperReviewGateResult classify_helper_review_reply(std::string_view reply)
    {
        const std::string lower = lowercase_ascii(reply);

        HelperReviewGateResult result{};
        result.state = "rejected_missing_evidence";
        result.reason = "helper reply did not cite enough staged packet/build/output/verifier evidence";

        const bool bypassIntent =
            contains_text(lower, "auto promote")
            || contains_text(lower, "autopromote")
            || contains_text(lower, "commit it")
            || contains_text(lower, "push it")
            || contains_text(lower, "no review")
            || contains_text(lower, "skip review")
            || contains_text(lower, "run server")
            || contains_text(lower, "start server")
            || contains_text(lower, "open port")
            || contains_text(lower, "bind port")
            || contains_text(lower, "listener")
            || contains_text(lower, "self accessible")
            || contains_text(lower, "model-accessible");
        if (bypassIntent)
        {
            result.state = "rejected_bypass_request";
            result.reason = "helper reply requested automatic promotion or a bypass-capable runtime action";
            return result;
        }

        const bool saysFineWithoutEvidence =
            (contains_text(lower, "working fine")
                || contains_text(lower, "looks good")
                || contains_text(lower, "all good")
                || contains_text(lower, "ship it"))
            && !contains_text(lower, "packet")
            && !contains_text(lower, "build")
            && !contains_text(lower, "verifier")
            && !contains_text(lower, "child_self_test");
        if (saysFineWithoutEvidence)
        {
            result.state = "rejected_status_only";
            result.reason = "helper reply asserted success without visible tool/build evidence";
            return result;
        }

        int evidenceScore = 0;
        if (contains_text(lower, "packet")) ++evidenceScore;
        if (contains_text(lower, "build log") || contains_text(lower, "build=pass") || contains_text(lower, "build pass")) ++evidenceScore;
        if (contains_text(lower, "output") || contains_text(lower, ".exe") || contains_text(lower, "artifact")) ++evidenceScore;
        if (contains_text(lower, "child_self_test") || contains_text(lower, "self-test") || contains_text(lower, "verifier")) ++evidenceScore;
        if (contains_text(lower, "mcp") || contains_text(lower, "capture") || contains_text(lower, "evidence path")) ++evidenceScore;
        result.evidence_score = evidenceScore;

        const bool humanGated =
            contains_text(lower, "human")
            || contains_text(lower, "manual")
            || contains_text(lower, "operator")
            || contains_text(lower, "review")
            || contains_text(lower, "approval");
        if (!humanGated)
        {
            result.state = "rejected_missing_human_gate";
            result.reason = "helper reply cited evidence but did not keep promotion behind a human review gate";
            return result;
        }

        if (evidenceScore < 4)
            return result;

        result.accepted = true;
        result.state = "ready_for_human_review";
        result.reason = "helper reply cites staged packet/build/output/verifier evidence and keeps promotion human-gated";
        return result;
    }

    std::string send_to_engine_ai(const std::string& user_text)
    {
        if (g_selectedModel.empty())
            return "No AI model selected. Open Workspace > AI, scan local models, and choose a model before running chat/tooling.";

        if (!g_engineAi) init_engine_ai();
        if (!g_engineAi)
            return "OS AI model could not initialize. Confirm a local model is selected and the endpoint is reachable.";

        const auto reply = g_engineAi->submit(user_text);
        if (!reply.text.empty() && !is_promotable_assistant_text(reply.text))
        {
            core::log::warn("ai", "Local model returned non-promotable assistant content.");
            return "Local model returned reasoning/debug text instead of final assistant content. Adjust the local model chat template or choose a content-producing OS model before using Engine AI chat.";
        }
        if (reply.text.empty())
            return std::string("No decodable reply from selected local model '") + g_selectedModel
                + "' at " + g_selectedEndpoint
                + ". Check the endpoint/model selection and retry.";
        return reply.text;
    }
}
