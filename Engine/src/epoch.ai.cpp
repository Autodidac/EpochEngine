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

#include "../include/_epoch.stl_types.hpp"

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <winhttp.h>
#   pragma comment(lib, "winhttp.lib")
#elif defined(EPOCH_HAS_CURL)
#   include <curl/curl.h>
#endif

#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module epoch.ai;

import ai.runtime;
import ai.dataset;
import ai.train;
import ai.mcp;
import ai.eval;
import core.log;

namespace epoch::ai
{
    namespace
    {
        Bot* g_bot = nullptr;
        ProviderMode g_providerMode = ProviderMode::LmStudioOracle;
        std::string g_selectedModel{};
        std::string g_selectedEndpoint{ "http://localhost:1234" };

        static bool ends_with(std::string_view s, std::string_view suf)
        {
            return s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf;
        }

        static void rstrip_slashes(std::string& s)
        {
            while (!s.empty() && s.back() == '/')
                s.pop_back();
        }

        static std::string normalize_lmstudio_native_chat_endpoint(std::string endpoint)
        {
            // Accept:
            //  - http://host:port
            //  - http://host:port/api/v1/chat
            // Normalize to full /api/v1/chat.
            rstrip_slashes(endpoint);

            if (ends_with(endpoint, "/api/v1/chat"))
                return endpoint;

            return endpoint + "/api/v1/chat";
        }

        static std::string normalize_model_list_endpoint(std::string endpoint)
        {
            rstrip_slashes(endpoint);

            constexpr std::string_view suffixes[] = {
                "/api/v1/chat",
                "/v1/chat/completions",
                "/v1/models"
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
                    break;
                if (avail == 0) break;

                std::string buf(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buf.data(), avail, &read))
                    break;
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
                    break;
                if (avail == 0) break;

                std::string buf(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buf.data(), avail, &read))
                    break;
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
                core::log::warn("ai", epoch::string_view{msg.data(), msg.size()});
                return {};
            }
        }

        static std::string resolve_model_name(const std::string& endpoint, std::string requested)
        {
            auto detected = fetch_detected_models(endpoint);

            if (!detected.empty())
                return detected.front();

            return requested;
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
            std::string lastReasoning;

            auto extract_last_quoted_after = [&](std::string_view text, std::string_view marker) -> std::string
            {
                const std::size_t markerPos = text.rfind(marker);
                if (markerPos == std::string_view::npos)
                    return {};

                std::size_t pos = markerPos + marker.size();
                std::string found{};
                while (true)
                {
                    const std::size_t open = text.find('"', pos);
                    if (open == std::string_view::npos)
                        break;

                    std::string raw;
                    bool closed = false;
                    for (std::size_t i = open + 1; i < text.size(); ++i)
                    {
                        const char c = text[i];
                        if (c == '"' && text[i - 1] != '\\')
                        {
                            const std::string candidate = trim(json_unescape(raw));
                            if (!candidate.empty())
                                found = candidate;
                            pos = i + 1;
                            closed = true;
                            break;
                        }
                        raw.push_back(c);
                    }

                    if (!closed)
                        break;
                }

                return found;
            };

            auto reasoning_fallback = [&](std::string_view reasoning) -> std::string
            {
                static constexpr std::string_view markers[] = {
                    "Final Selection:",
                    "Final selection:",
                    "Final Answer:",
                    "Final answer:",
                    "Answer:"
                };

                for (const auto marker : markers)
                {
                    if (const std::string extracted = extract_last_quoted_after(reasoning, marker); !extracted.empty())
                        return extracted;
                }

                std::string bestLine{};
                std::size_t start = 0;
                while (start < reasoning.size())
                {
                    std::size_t end = reasoning.find('\n', start);
                    if (end == std::string_view::npos)
                        end = reasoning.size();

                    std::string line = trim(reasoning.substr(start, end - start));
                    while (!line.empty() && (line.front() == '*' || line.front() == '-' || line.front() == '"' || line.front() == '>'))
                    {
                        line.erase(line.begin());
                        line = trim(line);
                    }
                    while (!line.empty() && (line.back() == '"' || line.back() == '.' || line.back() == ':'))
                        line.pop_back();
                    line = trim(line);

                    if (!line.empty() && line.find("Analyze the Request") == std::string::npos
                        && line.find("Determine the appropriate response") == std::string::npos
                        && line.find("Drafting the response") == std::string::npos
                        && line.find("Check constraints") == std::string::npos
                        && line.find("Final Selection") == std::string::npos
                        && line.find("Final Answer") == std::string::npos)
                    {
                        bestLine = line;
                    }

                    start = (end < reasoning.size()) ? end + 1 : reasoning.size();
                }

                return bestLine;
            };

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
                else if (itemType == "reasoning" && !text.empty())
                    lastReasoning = std::move(text);
            }

            if (last.empty() && !lastReasoning.empty())
                last = reasoning_fallback(lastReasoning);

            return last;
        }

        static std::string lmstudio_chat_complete(const std::string& endpoint_full,
            std::string_view model,
            std::string_view system_prompt,
            std::string_view input,
            const std::vector<std::pair<std::string, std::string>>& headers)
        {
            std::string body;
            body.reserve(256 + input.size());
            body += "{";
            body += "\"model\":\"" + json_escape(model) + "\",";
            body += "\"system_prompt\":\"" + json_escape(system_prompt) + "\",";
            body += "\"input\":\"" + json_escape(input) + "\",";
            body += "\"reasoning\":\"off\",";
            body += "\"max_output_tokens\":128,";
            body += "\"store\":false";
            body += "}";

            try
            {
#if defined(_WIN32)
                const std::string resp = winhttp_post_json(endpoint_full, body, headers);
#elif defined(EPOCH_HAS_CURL)
                const std::string resp = http_post_json(endpoint_full, body, headers);
#else
                (void)endpoint_full;
                (void)headers;
                core::log::error("ai", "LM Studio request failed: no non-Windows HTTP transport is configured (build with libcurl).");
                return {};
#endif
                return trim(extract_lmstudio_message_content(resp));
            }
            catch (const std::exception& ex)
            {
                std::string msg = "LM Studio request failed: ";
                msg += ex.what();
                core::log::error("ai", epoch::string_view{msg.data(), msg.size()});
                return {};
            }
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

    Bot::Bot(Config cfg)
        : m_cfg(std::move(cfg))
    {
        if (m_cfg.backend.empty())
            m_cfg.backend = "lmstudio_chat";
        if (m_cfg.best_of == 0)
            m_cfg.best_of = 1;

        if (m_cfg.backend != "lmstudio_chat")
            core::log::info("ai", "Bot backend is not lmstudio_chat; only lmstudio_chat is implemented here.");

        m_endpoint_full = normalize_lmstudio_native_chat_endpoint(m_cfg.endpoint);
        m_cfg.model = resolve_model_name(m_cfg.endpoint, m_cfg.model);
        g_providerMode = ProviderMode::LmStudioOracle;
        g_selectedEndpoint = m_cfg.endpoint;
        g_selectedModel = m_cfg.model;
        if (!m_cfg.model.empty())
        {
            std::string msg = "Bot model: ";
            msg += m_cfg.model;
            core::log::info("ai", epoch::string_view{msg.data(), msg.size()});
        }
    }

    BotReply Bot::submit(std::string_view user_input)
    {
        BotReply out{};

        const std::string sys =
            "You are EpochBot.\n"
            "Rules:\n"
            " - Reply with correct English grammar.\n"
            " - Capitalize the first letter of the response.\n"
            " - Do not mimic the user's bad grammar.\n"
            " - Do not include hidden reasoning.\n"
            " - Stay grounded in the current Epoch editor/project context.\n"
            " - Prefer concrete editor, scene, engine, and C++ guidance that teaches the internal Epoch bot what to do next.\n"
            " - The local MCP/control layer can teach and steer EpochBot while it operates; keep responses useful for that training loop instead of acting like a generic assistant.\n"
            " - When suggesting project or file work, keep it relevant to the active engine/runtime context instead of drifting into generic setup advice.\n";

        // Best-of with a fast accept to reduce latency.
        constexpr double kFastAcceptScore = 0.25; // placeholder (no scorer yet; kept for interface parity)

        const std::size_t n = std::max<std::size_t>(1, m_cfg.best_of);

        // For now: no scorer; pick first non-empty.
        for (std::size_t i = 0; i < n; ++i)
        {
            std::string txt = lmstudio_chat_complete(
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
                // fast accept when first is good
                if (i == 0) break;
            }

            if (out.score >= kFastAcceptScore)
                break;
        }

        return out;
    }

    void init_bot()
    {
        if (g_bot) return;

        g_bot = new Bot({
            .backend = "lmstudio_chat",
            .endpoint = "http://localhost:1234",
            .model = {},
            .best_of = 1
        });

        core::log::info("ai", "Bot initialized");
        {
            std::string msg = "AI provider: ";
            msg += active_provider_summary();
            core::log::info("ai", epoch::string_view{msg.data(), msg.size()});
        }
        {
            std::string msg = "AI endpoint: ";
            msg += g_selectedEndpoint;
            core::log::info("ai", epoch::string_view{msg.data(), msg.size()});
        }
        {
            std::string msg = "AI raw capture path: ";
            msg += local_capture_jsonl_path();
            core::log::info("ai", epoch::string_view{msg.data(), msg.size()});
        }
        {
            std::string msg = "AI MCP capture path: ";
            msg += local_mcp_capture_jsonl_path();
            core::log::info("ai", epoch::string_view{msg.data(), msg.size()});
        }
    }

    void shutdown_bot()
    {
        delete g_bot;
        g_bot = nullptr;
        core::log::info("ai", "Bot shutdown");
    }


    std::string default_workspace_root()
    {
#if defined(_WIN32)
        // Default: alongside exe in ./workspace
        return "workspace";
#else
        return "workspace";
#endif
    }

    std::string curated_datasets_root()
    {
        return "Engine/ai/datasets/curated";
    }

    std::string evals_root()
    {
        return "Engine/ai/evals";
    }

    std::string tokenizer_root()
    {
        return "Engine/ai/tokenizer";
    }

    std::string prompts_root()
    {
        return "Engine/ai/prompts";
    }

    std::string manifests_root()
    {
        return "Engine/ai/manifests";
    }

    std::string local_capture_jsonl_path()
    {
        return default_workspace_root() + "/auto_train.jsonl";
    }

    std::string local_mcp_capture_jsonl_path()
    {
        return default_workspace_root() + "/mcp_capture.jsonl";
    }

    std::string local_checkpoint_root()
    {
        return default_workspace_root() + "/ai/checkpoints";
    }

    std::string local_model_root()
    {
        return default_workspace_root() + "/ai/models";
    }

    std::string local_cache_root()
    {
        return default_workspace_root() + "/ai/cache";
    }

    ProviderMode current_provider_mode() noexcept
    {
        return g_providerMode;
    }

    std::string active_model_name()
    {
        return g_selectedModel;
    }

    std::string active_provider_summary()
    {
        std::string summary = std::string(provider_mode_name(g_providerMode));
        if (!g_selectedModel.empty())
        {
            summary += " :: ";
            summary += g_selectedModel;
        }
        return summary;
    }

    ModelManifest active_model_manifest()
    {
        ModelManifest manifest{};
        manifest.id = g_selectedModel;
        manifest.provider = g_providerMode;
        manifest.endpoint = g_selectedEndpoint;
        manifest.tokenizer_path = tokenizer_root() + "/epoch_tokenizer_manifest.json";
        manifest.repo_safe_manifest = true;
        manifest.local_weights_only = true;
        manifest.available = !g_selectedModel.empty();

        switch (g_providerMode)
        {
        case ProviderMode::EmbeddedTiny:
            manifest.display_name = g_selectedModel.empty() ? std::string("Embedded EpochBot") : g_selectedModel;
            manifest.manifest_path = manifests_root() + "/embedded_tiny_epoch.json";
            break;
        case ProviderMode::McpOperations:
            manifest.display_name = g_selectedModel.empty() ? std::string("Local MCP control/training bot") : g_selectedModel;
            manifest.manifest_path = manifests_root() + "/local_mcp_control.json";
            break;
        case ProviderMode::LmStudioOracle:
        default:
            manifest.display_name = g_selectedModel.empty() ? std::string("LM Studio development helper") : g_selectedModel;
            manifest.manifest_path = manifests_root() + "/teacher_oracle_lmstudio.json";
            break;
        }

        return manifest;
    }

    TrainingPaths default_training_paths()
    {
        return TrainingPaths{
            .workspace_root = default_workspace_root(),
            .local_capture_jsonl = local_capture_jsonl_path(),
            .mcp_capture_jsonl = local_mcp_capture_jsonl_path(),
            .checkpoint_root = local_checkpoint_root(),
            .model_root = local_model_root(),
            .cache_root = local_cache_root(),
            .curated_dataset_root = curated_datasets_root(),
            .eval_root = evals_root()
        };
    }

    void append_training_sample(std::string_view prompt, std::string_view answer, std::string_view source)
    {
        const TrainingPaths paths = default_training_paths();
        const std::filesystem::path workspaceDir = paths.workspace_root;
        const std::filesystem::path checkpointDir = paths.checkpoint_root;
        const std::filesystem::path modelDir = paths.model_root;
        const std::filesystem::path cacheDir = paths.cache_root;
        std::error_code ec;
        std::filesystem::create_directories(workspaceDir, ec);
        std::filesystem::create_directories(checkpointDir, ec);
        std::filesystem::create_directories(modelDir, ec);
        std::filesystem::create_directories(cacheDir, ec);

        const std::filesystem::path file = paths.local_capture_jsonl;

        std::ostringstream oss;
        oss << "{";
        oss << "\"prompt\":\"" << json_escape(prompt) << "\",";
        oss << "\"answer\":\"" << json_escape(answer) << "\",";
        oss << "\"source\":\"" << json_escape(source) << "\"";
        oss << "}\n";
        if (append_jsonl_line(file, trim(oss.str())))
        {
            static bool loggedCapturePath = false;
            if (!loggedCapturePath)
            {
                loggedCapturePath = true;
                std::string msg = "AI appended local training capture: ";
                msg += file.string();
                core::log::info("ai", epoch::string_view{msg.data(), msg.size()});
            }
        }
    }

    void append_mcp_capture(const McpCaptureRecord& record)
    {
        const TrainingPaths paths = default_training_paths();
        const std::filesystem::path file = paths.mcp_capture_jsonl;

        std::ostringstream oss;
        oss << "{";
        oss << "\"server\":\"" << json_escape(record.server) << "\",";
        oss << "\"tool\":\"" << json_escape(record.tool) << "\",";
        oss << "\"prompt\":\"" << json_escape(record.prompt) << "\",";
        oss << "\"normalized_output\":\"" << json_escape(record.normalized_output) << "\",";
        oss << "\"source_path\":\"" << json_escape(record.source_path) << "\"";
        oss << "}";

        if (append_jsonl_line(file, oss.str()))
        {
            static bool loggedCapturePath = false;
            if (!loggedCapturePath)
            {
                loggedCapturePath = true;
                std::string msg = "AI appended MCP capture: ";
                msg += file.string();
                core::log::info("ai", epoch::string_view{msg.data(), msg.size()});
            }
        }
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

    std::string send_to_bot(const std::string& user_text)
    {
        if (!g_bot) init_bot();
        if (!g_bot) return {};

        const auto reply = g_bot->submit(user_text);
        if (!reply.text.empty())
        {
            append_mcp_capture(McpCaptureRecord{
                .server = "local-lmstudio",
                .tool = "chat",
                .prompt = user_text,
                .normalized_output = reply.text,
                .source_path = active_model_manifest().manifest_path
            });
        }
        return reply.text;
    }
}
