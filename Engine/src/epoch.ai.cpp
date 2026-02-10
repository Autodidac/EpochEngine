module;

#include "../include/_epoch.stl_types.hpp"

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <winhttp.h>
#   pragma comment(lib, "winhttp.lib")
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

import core.log;

namespace epoch::ai
{
    namespace
    {
        Bot* g_bot = nullptr;

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
#endif

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

                std::size_t msg_pos = find_str(sv, "\"message\"", type_pos);
                if (msg_pos == std::string_view::npos) { pos = type_pos + 6; continue; }

                std::size_t content_key = find_str(sv, "\"content\"", msg_pos);
                if (content_key == std::string_view::npos) { pos = msg_pos + 9; continue; }

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
                if (!text.empty())
                    last = std::move(text);
            }

            return last;
        }

        static std::string lmstudio_chat_complete(const std::string& endpoint_full,
            std::string_view model,
            std::string_view system_prompt,
            std::string_view input,
            const std::vector<std::pair<std::string, std::string>>& headers)
        {
#if !defined(_WIN32)
            (void)endpoint_full; (void)model; (void)system_prompt; (void)input; (void)headers;
            return {};
#else
            std::string body;
            body.reserve(256 + input.size());
            body += "{";
            body += "\"model\":\"" + json_escape(model) + "\",";
            body += "\"system_prompt\":\"" + json_escape(system_prompt) + "\",";
            body += "\"input\":\"" + json_escape(input) + "\"";
            body += "}";

            const std::string resp = winhttp_post_json(endpoint_full, body, headers);
            return extract_lmstudio_message_content(resp);
#endif
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
    }

    BotReply Bot::submit(std::string_view user_input)
    {
        BotReply out{};

        const std::string sys =
            "You are AlmondBot.\n"
            "Rules:\n"
            " - Reply with correct English grammar.\n"
            " - Capitalize the first letter of the response.\n"
            " - Do not mimic the user's bad grammar.\n"
            " - Do not include hidden reasoning.\n";

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
            .model = "arliai_glm-4.5-air-derestricted",
            .best_of = 1
        });

        core::log::info("ai", "Bot initialized");
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

    void append_training_sample(std::string_view prompt, std::string_view answer, std::string_view source)
    {
        const std::string ws = default_workspace_root();
        const std::filesystem::path dir = std::filesystem::path(ws) / "datasets";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        const std::filesystem::path file = dir / "auto_train.jsonl";

        std::ostringstream oss;
        oss << "{";
        oss << "\"prompt\":\"" << json_escape(prompt) << "\",";
        oss << "\"answer\":\"" << json_escape(answer) << "\",";
        oss << "\"source\":\"" << json_escape(source) << "\"";
        oss << "}\n";

        // Append (best-effort)
        FILE* f = nullptr;
#if defined(_WIN32)
        if (0 == fopen_s(&f, file.string().c_str(), "ab"))
#else
        f = std::fopen(file.string().c_str(), "ab");
        if (f)
#endif
        {
            std::fwrite(oss.str().data(), 1, oss.str().size(), f);
            std::fclose(f);
        }
    }
std::string send_to_bot(const std::string& user_text)
    {
        if (!g_bot) init_bot();
        if (!g_bot) return {};

        const auto reply = g_bot->submit(user_text);
        return reply.text;
    }
}
