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
#   include <sys/resource.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   include <unistd.h>
#   if defined(EPOCH_HAS_CURL)
#       include <curl/curl.h>
#   endif
#endif

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module ai.engine;

import ai.model_install;
import ai.runtime;

import ai.session;
import ai.mcp;
import ai.eval;
import ai.project_profile;
import core.log;
import core.path;

namespace epochengine::ai
{
    namespace
    {
        std::recursive_mutex g_aiStateMutex{};
        std::mutex g_aiRequestMutex{};
        std::atomic_uint64_t g_aiRequestCancellationGeneration{1u};
        thread_local std::uint64_t g_aiRequestStartCancellationGeneration{1u};
        std::atomic_uint64_t g_directInferenceOutputSequence{1u};
#if defined(_WIN32)
        std::mutex g_activeAiWinHttpMutex{};
        HINTERNET g_activeAiWinHttpRequest{};
#endif
        std::shared_ptr<EngineAiModel> g_engineAi{};
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
        std::string g_loadedProjectAiProfile{};
        bool g_modelUseConfirmedForSession = false;

        static std::string executable_cache_bucket(std::string_view bucket)
        {
            const auto runtimeRoot = epochengine::core::path::runtime_root_dir();
            const auto executableRoot = epochengine::core::path::executable_dir();
            if (!executableRoot.empty())
                return (executableRoot / "cache" / std::string{ bucket }).generic_string();

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


        [[nodiscard]] static int json_hex_digit(char value) noexcept
        {
            if (value >= '0' && value <= '9')
                return value - '0';
            if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;
            if (value >= 'A' && value <= 'F')
                return value - 'A' + 10;
            return -1;
        }

        [[nodiscard]] static bool append_utf8_code_point(
            std::string& output,
            std::uint32_t codePoint)
        {
            if (codePoint > 0x10ffffu
                || (codePoint >= 0xd800u && codePoint <= 0xdfffu))
            {
                return false;
            }
            if (codePoint <= 0x7fu)
            {
                output.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7ffu)
            {
                output.push_back(static_cast<char>(0xc0u | (codePoint >> 6u)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            else if (codePoint <= 0xffffu)
            {
                output.push_back(static_cast<char>(0xe0u | (codePoint >> 12u)));
                output.push_back(static_cast<char>(
                    0x80u | ((codePoint >> 6u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            else
            {
                output.push_back(static_cast<char>(0xf0u | (codePoint >> 18u)));
                output.push_back(static_cast<char>(
                    0x80u | ((codePoint >> 12u) & 0x3fu)));
                output.push_back(static_cast<char>(
                    0x80u | ((codePoint >> 6u) & 0x3fu)));
                output.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
            }
            return true;
        }

        [[nodiscard]] static bool json_character_is_escaped(
            std::string_view source,
            std::size_t index,
            std::size_t stringBegin) noexcept
        {
            std::size_t slashCount{};
            while (index > stringBegin && source[index - 1u] == '\\')
            {
                --index;
                ++slashCount;
            }
            return (slashCount & 1u) != 0u;
        }

        static std::string json_unescape(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                const unsigned char byte = static_cast<unsigned char>(s[i]);
                if (byte < 0x20u)
                    return {};
                if (s[i] != '\\')
                {
                    out.push_back(s[i]);
                    continue;
                }
                if (++i >= s.size())
                    return {};

                const char escaped = s[i];
                switch (escaped)
                {
                case '\\': out.push_back('\\'); break;
                case '"':  out.push_back('"'); break;
                case '/':  out.push_back('/'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'u':
                {
                    if (i + 4u >= s.size())
                        return {};
                    std::uint32_t codePoint{};
                    for (std::size_t digit = 0u; digit < 4u; ++digit)
                    {
                        const int value = json_hex_digit(s[i + 1u + digit]);
                        if (value < 0)
                            return {};
                        codePoint = (codePoint << 4u)
                            | static_cast<std::uint32_t>(value);
                    }
                    i += 4u;
                    if (codePoint >= 0xd800u && codePoint <= 0xdbffu)
                    {
                        if (i + 6u >= s.size() || s[i + 1u] != '\\'
                            || s[i + 2u] != 'u')
                        {
                            return {};
                        }
                        std::uint32_t low{};
                        for (std::size_t digit = 0u; digit < 4u; ++digit)
                        {
                            const int value = json_hex_digit(s[i + 3u + digit]);
                            if (value < 0)
                                return {};
                            low = (low << 4u)
                                | static_cast<std::uint32_t>(value);
                        }
                        if (low < 0xdc00u || low > 0xdfffu)
                            return {};
                        codePoint = 0x10000u
                            + ((codePoint - 0xd800u) << 10u)
                            + (low - 0xdc00u);
                        i += 6u;
                    }
                    else if (codePoint >= 0xdc00u && codePoint <= 0xdfffu)
                    {
                        return {};
                    }
                    if (!append_utf8_code_point(out, codePoint))
                        return {};
                    break;
                }
                default:
                    return {};
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

        static std::string read_exact_text_file(
            const std::filesystem::path& file,
            const std::uintmax_t maxBytes)
        {
            std::error_code error{};
            if (!std::filesystem::is_regular_file(file, error) || error
                || std::filesystem::is_symlink(file, error) || error)
                return {};
            const auto size = std::filesystem::file_size(file, error);
            if (error || size == 0u || size > maxBytes)
                return {};
            std::ifstream input(file, std::ios::binary);
            if (!input)
                return {};
            std::string contents(static_cast<std::size_t>(size), '\0');
            input.read(
                contents.data(),
                static_cast<std::streamsize>(contents.size()));
            return input && input.peek() == std::char_traits<char>::eof()
                ? contents : std::string{};
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
        [[nodiscard]] static bool regular_file_with_size(
            const std::filesystem::path& path,
            std::uint64_t expectedBytes)
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec) || ec)
                return false;
            const auto bytes = std::filesystem::file_size(path, ec);
            return !ec && bytes == expectedBytes;
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_llama_cpp_root_path()
        {
            return std::filesystem::path{executable_cache_bucket("packages")}
                / std::string{kEpochLocalLlamaCppRuntimePackageId};
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_llama_cpp_executable_path()
        {
            std::filesystem::path path = epoch_local_llama_cpp_root_path()
                / "versions" / std::string{kEpochLocalLlamaCppRelease} / "bin";
#if defined(_WIN32)
            path /= "llama-cli.exe";
#else
            path /= "llama-cli";
#endif
            return path;
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_qwen38_legacy_root_path()
        {
            return std::filesystem::path{executable_cache_bucket("models")}
                / std::string{kEpochLocalQwen38ModelPackageId};
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_qwen38_root_path()
        {
            const auto plan = model_install::plan_for(
                kEpochLocalQwen38ModelPackageId);
            if (!plan || !model_install::valid_plan(*plan))
                return {};
            const std::filesystem::path modelsRoot{
                executable_cache_bucket("models")};
            const std::filesystem::path versioned =
                model_install::version_root(modelsRoot, *plan);
            return versioned.empty()
                ? modelsRoot / plan->package_id / "versions" / plan->revision
                : versioned;
        }

        [[nodiscard]] static std::filesystem::path
            epoch_local_qwen38_model_path()
        {
            return epoch_local_qwen38_root_path()
                / std::string{kEpochLocalQwen38ModelFile};
        }

        [[nodiscard]] static bool receipt_contains(
            const std::filesystem::path& path,
            std::initializer_list<std::string_view> fields)
        {
            const std::string receipt = read_small_text_file(path, 64u * 1024u);
            return !receipt.empty()
                && std::all_of(fields.begin(), fields.end(),
                    [&](std::string_view field)
                    {
                        return receipt.find(field) != std::string::npos;
                    });
        }

        static void apply_project_ai_profile_if_present()
        {
            const std::string configured =
                trim_env_value(read_env_var("EPOCH_PROJECT_AI_PROFILE"));
            if (configured.empty()
                || configured == g_loadedProjectAiProfile)
                return;

            const std::filesystem::path profilePath{configured};
            const std::string profile =
                read_small_text_file(profilePath, 64u * 1024u);
            if (profile.empty())
            {
                g_engineAi.reset();
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project AI profile rejected: the configured profile could not be read within its bounded size.";
                return;
            }

            g_loadedProjectAiProfile =
                std::filesystem::absolute(profilePath).generic_string();
            const project_profile::CodecResult decoded =
                project_profile::parse_profile(profile);
            if (!decoded)
            {
                g_engineAi.reset();
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project AI profile rejected: " + decoded.status;
                return;
            }

            switch (decoded.profile.provider)
            {
            case project_profile::Provider::disabled:
                g_engineAi.reset();
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project AI is disabled by its explicit project profile.";
                return;
            case project_profile::Provider::epoch_local_qwen38:
            {
                const EpochLocalAiInstallStatus installed =
                    epoch_local_ai_install_status();
                if (!installed.ready())
                {
                    g_engineAi.reset();
                    g_modelUseConfirmedForSession = false;
                    g_modelDetectionStatus = installed.message;
                    return;
                }
                g_localTransport =
                    LocalInferenceTransport::LlamaCppCli;
                g_directExecutable = installed.runtime_executable;
                g_directModel = installed.model_file;
                g_selectedModel =
                    std::string{kEpochLocalQwen38ModelFile};
                g_modelUseConfirmedForSession = true;
                g_modelDetectionStatus =
                    "Project selected the installed Epoch-local Qwen3.8 provider.";
                return;
            }
            case project_profile::Provider::external_mcp:
                g_localTransport =
                    LocalInferenceTransport::OpenAiCompatible;
                g_modelUseConfirmedForSession =
                    !g_selectedModel.empty();
                g_modelDetectionStatus = g_selectedModel.empty()
                    ? "Project selected external MCP/OpenAI-compatible inference; choose the operator-managed model."
                    : "Project selected the existing external MCP/OpenAI-compatible provider.";
                return;
            case project_profile::Provider::engine_selected:
                g_modelUseConfirmedForSession = false;
                g_modelDetectionStatus =
                    "Project delegates AI to the engine-selected provider; confirm the current local or external model for this session.";
                return;
            }
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
            if (const auto installed = epoch_local_llama_cpp_executable_path();
                regular_file(installed))
                return std::filesystem::absolute(installed);

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
            if (const auto installed = epoch_local_qwen38_model_path();
                regular_file_with_size(installed, kEpochLocalQwen38ModelBytes))
                return std::filesystem::absolute(installed);

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

        [[nodiscard]] static int model_http_timeout_milliseconds(
            std::uint32_t timeoutSeconds) noexcept
        {
            const std::uint64_t milliseconds =
                (std::max)(std::uint64_t{1u},
                    static_cast<std::uint64_t>(timeoutSeconds)) * 1'000u;
            return static_cast<int>((std::min)(
                milliseconds,
                static_cast<std::uint64_t>(
                    (std::numeric_limits<int>::max)())));
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

        [[nodiscard]] static bool register_active_ai_winhttp_request(HINTERNET request)
        {
            const std::lock_guard lock{g_activeAiWinHttpMutex};
            if (g_aiRequestCancellationGeneration.load(std::memory_order_acquire)
                != g_aiRequestStartCancellationGeneration)
            {
                return false;
            }
            g_activeAiWinHttpRequest = request;
            return true;
        }

        [[nodiscard]] static bool release_active_ai_winhttp_request(HINTERNET request)
        {
            const std::lock_guard lock{g_activeAiWinHttpMutex};
            if (g_activeAiWinHttpRequest != request)
                return false;
            g_activeAiWinHttpRequest = nullptr;
            return true;
        }

        static void close_active_ai_winhttp_request(HINTERNET request)
        {
            if (release_active_ai_winhttp_request(request))
                WinHttpCloseHandle(request);
        }

        static std::string winhttp_post_json(const std::string& url,
            const std::string& body_utf8,
            const std::vector<std::pair<std::string, std::string>>& headers,
            std::uint32_t timeoutSeconds)
        {
            const auto u = crack_url(url);

            HINTERNET hSession = WinHttpOpen(L"EpochAI/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) throw std::runtime_error("WinHTTP: WinHttpOpen failed");
            // Source iterations can legitimately take several minutes on a
            // local model. Honor the workload's bounded timeout while keeping
            // connect and send failures responsive; the editor can still
            // cancel the active WinHTTP request immediately.
            const int receiveTimeout =
                model_http_timeout_milliseconds(timeoutSeconds);
            (void)WinHttpSetTimeouts(
                hSession, 10000, 10000, 15000, receiveTimeout);

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

            if (!register_active_ai_winhttp_request(hRequest))
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: local-model request cancelled before dispatch");
            }
            const auto closeRequest = [&]() noexcept
            {
                close_active_ai_winhttp_request(hRequest);
            };

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
                closeRequest();
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("WinHTTP: WinHttpSendRequest failed");
            }

            if (!WinHttpReceiveResponse(hRequest, nullptr))
            {
                closeRequest();
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
                    closeRequest();
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpQueryDataAvailable failed");
                }
                if (avail == 0) break;

                std::string buf(avail, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buf.data(), avail, &read))
                {
                    closeRequest();
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    throw std::runtime_error("WinHTTP: WinHttpReadData failed");
                }
                buf.resize(read);
                resp += buf;
            }

            closeRequest();
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
            (void)WinHttpSetTimeouts(hSession, 2000, 2000, 3000, 5000);

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

        static int curl_ai_request_progress(
            void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
        {
            return g_aiRequestCancellationGeneration.load(std::memory_order_acquire)
                    == g_aiRequestStartCancellationGeneration
                ? 0
                : 1;
        }

        static std::string http_post_json(const std::string& url,
            const std::string& body_utf8,
            const std::vector<std::pair<std::string, std::string>>& headers,
            std::uint32_t timeoutSeconds)
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
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
            curl_easy_setopt(
                curl,
                CURLOPT_TIMEOUT_MS,
                static_cast<long>(
                    model_http_timeout_milliseconds(timeoutSeconds)));
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_ai_request_progress);

            const CURLcode code = curl_easy_perform(curl);
            if (code != CURLE_OK)
            {
                const std::string err = code == CURLE_ABORTED_BY_CALLBACK
                    ? "CURL: local-model request cancelled"
                    : std::string("CURL: curl_easy_perform failed: ") + curl_easy_strerror(code);
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
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000L);

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
                    if (c == '"' && !json_character_is_escaped(sv, i, q))
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
                if (c == '"' && !json_character_is_escaped(sv, i, q))
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

        [[nodiscard]] static std::string normalize_assistant_text(std::string_view text)
        {
            struct ReasoningWrapper final
            {
                std::string_view opening{};
                std::string_view closing{};
            };
            constexpr std::array wrappers{
                ReasoningWrapper{ "<think>", "</think>" },
                ReasoningWrapper{ "<analysis>", "</analysis>" },
                ReasoningWrapper{ "<reasoning>", "</reasoning>" }
            };

            std::string candidate = trim(text);
            for (;;)
            {
                const std::string lower = lowercase_ascii(candidate);
                bool removed = false;
                for (const ReasoningWrapper& wrapper : wrappers)
                {
                    if (!starts_with_text(lower, wrapper.opening))
                        continue;

                    const std::size_t closing = lower.find(
                        wrapper.closing,
                        wrapper.opening.size());
                    if (closing == std::string::npos)
                        return {};

                    candidate = trim(std::string_view{candidate}.substr(
                        closing + wrapper.closing.size()));
                    removed = true;
                    break;
                }
                if (!removed)
                    break;
            }

            const std::string lower = lowercase_ascii(candidate);
            if (starts_with_text(lower, "<final>"))
            {
                constexpr std::size_t openingSize = 7u;
                constexpr std::size_t closingSize = 8u;
                const std::size_t closing = lower.rfind("</final>");
                if (closing == std::string::npos
                    || !trim(std::string_view{candidate}.substr(closing + closingSize)).empty())
                {
                    return {};
                }
                candidate = trim(std::string_view{candidate}.substr(
                    openingSize,
                    closing - openingSize));
            }
            return candidate;
        }
        [[nodiscard]] static std::string normalize_direct_llama_cpp_text(
            std::string_view text)
        {
            std::string candidate = trim(text);
            for (;;)
            {
                const std::size_t lastLineBegin = candidate.rfind('\n');
                const std::string lastLine = trim(
                    lastLineBegin == std::string::npos
                        ? std::string_view{candidate}
                        : std::string_view{candidate}.substr(lastLineBegin + 1u));
                if (lowercase_ascii(lastLine) != "exiting...")
                    break;

                candidate = trim(
                    lastLineBegin == std::string::npos
                        ? std::string_view{}
                        : std::string_view{candidate}.substr(0u, lastLineBegin));
            }

            const std::string lower = lowercase_ascii(candidate);
            constexpr std::string_view endThinking{"[end thinking]"};
            const std::size_t thinkingEnd = lower.rfind(endThinking);
            if (thinkingEnd != std::string::npos)
            {
                candidate = trim(std::string_view{candidate}.substr(
                    thinkingEnd + endThinking.size()));
            }
            else if (lower.find("[start thinking]") != std::string::npos)
            {
                return {};
            }
            return normalize_assistant_text(candidate);
        }

        [[nodiscard]] static std::string normalize_lf(std::string_view text)
        {
            std::string normalized;
            normalized.reserve(text.size());
            for (const char character : text)
            {
                if (character != '\r')
                    normalized.push_back(character);
            }
            return normalized;
        }

        [[nodiscard]] static std::string
            normalize_direct_llama_cpp_transcript(
                std::string_view transcript,
                std::string_view userText)
        {
            const std::string normalizedTranscript = normalize_lf(transcript);
            const std::string normalizedUserText = normalize_lf(userText);
            std::string expectedPrefix{"User:\n"};
            expectedPrefix += normalizedUserText;
            expectedPrefix += "\n\nAssistant:\n";
            if (!starts_with_text(normalizedTranscript, expectedPrefix))
                return {};

            return normalize_direct_llama_cpp_text(
                std::string_view{normalizedTranscript}.substr(
                    expectedPrefix.size()));
        }

        [[nodiscard]] static std::size_t strict_source_reply_offset(
            std::string_view reply) noexcept
        {
            const auto lineFramedOffset =
                [&](const std::string_view header) noexcept
                {
                    std::size_t offset = reply.find(header);
                    while (offset != std::string_view::npos)
                    {
                        const std::size_t end = offset + header.size();
                        if ((offset == 0u || reply[offset - 1u] == '\n')
                            && (end == reply.size() || reply[end] == '\n'))
                        {
                            return offset;
                        }
                        offset = reply.find(header, offset + 1u);
                    }
                    return std::string_view::npos;
                };

            static constexpr std::array<std::string_view, 3>
                structuredHeaders{
                "EPOCH_SOURCE_PATCH_PROPOSAL_V1",
                "EPOCH_SOURCE_PROPOSAL_V1",
                "EPOCH_SOURCE_CONTEXT_REQUEST_V1"};
            for (const std::string_view header : structuredHeaders)
            {
                const std::size_t offset = lineFramedOffset(header);
                if (offset != std::string_view::npos)
                    return offset;
            }
            return lineFramedOffset(
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1");
        }

        [[nodiscard]] static std::size_t strict_source_reply_end(
            std::string_view reply,
            const std::size_t packetOffset) noexcept
        {
            const std::string_view packet = reply.substr(packetOffset);
            std::string_view terminator{};
            if (packet.starts_with("EPOCH_SOURCE_PATCH_PROPOSAL_V1")
                || packet.starts_with("EPOCH_SOURCE_PROPOSAL_V1"))
            {
                terminator = "end_proposal";
            }
            else if (packet.starts_with("EPOCH_SOURCE_CONTEXT_REQUEST_V1"))
            {
                terminator = "end_request";
            }
            else if (packet.starts_with(
                "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1"))
            {
                terminator = "EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1";
            }
            else
            {
                return std::string_view::npos;
            }

            std::size_t offset = reply.find(terminator, packetOffset);
            while (offset != std::string_view::npos)
            {
                const std::size_t end = offset + terminator.size();
                if ((offset == packetOffset || reply[offset - 1u] == '\n')
                    && (end == reply.size() || reply[end] == '\n'))
                    return end;
                offset = reply.find(terminator, offset + 1u);
            }
            return std::string_view::npos;
        }

        [[nodiscard]] static std::string
            normalize_direct_llama_cpp_source_transcript(
                std::string_view transcript,
                std::string_view userText)
        {
            std::string direct = normalize_direct_llama_cpp_transcript(
                transcript, userText);
            if (direct.empty())
            {
                direct = normalize_direct_llama_cpp_text(
                    normalize_lf(transcript));
                constexpr std::string_view assistantPrefix{"Assistant:\n"};
                if (starts_with_text(direct, assistantPrefix))
                {
                    direct = normalize_direct_llama_cpp_text(
                        std::string_view{direct}.substr(
                            assistantPrefix.size()));
                }
            }
            const std::size_t packetOffset =
                strict_source_reply_offset(direct);
            if (packetOffset == std::string::npos)
                return {};

            const std::size_t packetEnd = strict_source_reply_end(
                direct, packetOffset);
            const std::size_t packetSize = packetEnd == std::string_view::npos
                ? std::string_view::npos
                : packetEnd - packetOffset;
            std::string packet = trim(
                std::string_view{direct}.substr(packetOffset, packetSize));
            if (packet.ends_with("\n```"))
            {
                packet = trim(std::string_view{packet}.substr(
                    0u, packet.size() - 4u));
            }
            return packet;
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
                || contains_text(lower, "<think>")
                || contains_text(lower, "</think>")
                || contains_text(lower, "<analysis>")
                || contains_text(lower, "</analysis>")
                || contains_text(lower, "<reasoning>")
                || contains_text(lower, "</reasoning>")
                || contains_text(lower, "[start thinking]")
                || contains_text(lower, "[end thinking]")
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
                    if (c == '"' && !json_character_is_escaped(sv, i, type_q))
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
                    if (c == '"' && !json_character_is_escaped(sv, i, q))
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

        enum class StructuredSourceReply : std::uint8_t
        {
            none,
            context,
            patch
        };

        [[nodiscard]] static StructuredSourceReply structured_source_reply_for(
            std::string_view input,
            bool suppressReasoning) noexcept
        {
            if (!suppressReasoning)
                return StructuredSourceReply::none;
            if (input.find("EPOCH_SOURCE_PATCH_PROPOSAL_V1")
                != std::string_view::npos)
            {
                return StructuredSourceReply::patch;
            }
            if (input.find("EPOCH_SOURCE_CONTEXT_REQUEST_V1")
                != std::string_view::npos)
            {
                return StructuredSourceReply::context;
            }
            return StructuredSourceReply::none;
        }

        [[nodiscard]] static std::string_view structured_source_schema(
            StructuredSourceReply shape) noexcept
        {
            if (shape == StructuredSourceReply::context)
            {
                return R"json({"type":"json_schema","json_schema":{"name":"epoch_source_context","strict":true,"schema":{"type":"object","additionalProperties":false,"required":["reason","paths"],"properties":{"reason":{"type":"string","minLength":1,"maxLength":512},"paths":{"type":"array","minItems":1,"maxItems":12,"uniqueItems":true,"items":{"type":"string","minLength":1,"maxLength":1024}}}}}})json";
            }
            if (shape == StructuredSourceReply::patch)
            {
                return R"json({"type":"json_schema","json_schema":{"name":"epoch_source_patch","strict":true,"schema":{"type":"object","additionalProperties":false,"required":["title","rationale","operations"],"properties":{"title":{"type":"string","minLength":1,"maxLength":160},"rationale":{"type":"string","minLength":1,"maxLength":1024},"operations":{"type":"array","minItems":1,"maxItems":4,"items":{"type":"object","additionalProperties":false,"required":["path","summary","search","replacement"],"properties":{"path":{"type":"string","minLength":1,"maxLength":1024},"summary":{"type":"string","minLength":1,"maxLength":512},"search":{"type":"string","minLength":1,"maxLength":32768},"replacement":{"type":"string","maxLength":32768}}}}}}}})json";
            }
            return {};
        }

        class StructuredJsonCursor final
        {
        public:
            explicit StructuredJsonCursor(std::string_view source) noexcept
                : source_(source)
            {
            }

            [[nodiscard]] bool consume(char expected) noexcept
            {
                skip_space();
                if (position_ >= source_.size()
                    || source_[position_] != expected)
                {
                    return false;
                }
                ++position_;
                return true;
            }

            [[nodiscard]] std::optional<std::string> string()
            {
                skip_space();
                if (position_ >= source_.size()
                    || source_[position_] != '"')
                {
                    return std::nullopt;
                }
                const std::size_t begin = ++position_;
                for (std::size_t index = begin; index < source_.size(); ++index)
                {
                    if (source_[index] != '"'
                        || json_character_is_escaped(source_, index, begin))
                    {
                        continue;
                    }
                    const std::string_view raw =
                        source_.substr(begin, index - begin);
                    std::string decoded = json_unescape(raw);
                    if (!raw.empty() && decoded.empty())
                        return std::nullopt;
                    position_ = index + 1u;
                    return decoded;
                }
                return std::nullopt;
            }

            [[nodiscard]] bool complete() noexcept
            {
                skip_space();
                return position_ == source_.size();
            }

        private:
            void skip_space() noexcept
            {
                while (position_ < source_.size()
                    && std::isspace(static_cast<unsigned char>(
                        source_[position_])) != 0)
                {
                    ++position_;
                }
            }

            std::string_view source_{};
            std::size_t position_{};
        };

        struct StructuredPatchOperation final
        {
            std::string path{};
            std::string summary{};
            std::string search{};
            std::string replacement{};
        };

        [[nodiscard]] static bool parse_string_array(
            StructuredJsonCursor& cursor,
            std::vector<std::string>& values,
            std::size_t maximumValues)
        {
            if (!cursor.consume('['))
                return false;
            if (cursor.consume(']'))
                return true;
            for (;;)
            {
                auto value = cursor.string();
                if (!value || values.size() >= maximumValues)
                    return false;
                values.push_back(std::move(*value));
                if (cursor.consume(']'))
                    return true;
                if (!cursor.consume(','))
                    return false;
            }
        }

        [[nodiscard]] static bool parse_patch_operation(
            StructuredJsonCursor& cursor,
            StructuredPatchOperation& operation)
        {
            if (!cursor.consume('{'))
                return false;
            bool pathSeen{};
            bool summarySeen{};
            bool searchSeen{};
            bool replacementSeen{};
            if (cursor.consume('}'))
                return false;
            for (;;)
            {
                const auto key = cursor.string();
                if (!key || !cursor.consume(':'))
                    return false;
                auto value = cursor.string();
                if (!value)
                    return false;
                if (*key == "path" && !pathSeen)
                {
                    operation.path = std::move(*value);
                    pathSeen = true;
                }
                else if (*key == "summary" && !summarySeen)
                {
                    operation.summary = std::move(*value);
                    summarySeen = true;
                }
                else if (*key == "search" && !searchSeen)
                {
                    operation.search = std::move(*value);
                    searchSeen = true;
                }
                else if (*key == "replacement" && !replacementSeen)
                {
                    operation.replacement = std::move(*value);
                    replacementSeen = true;
                }
                else
                {
                    return false;
                }
                if (cursor.consume('}'))
                    break;
                if (!cursor.consume(','))
                    return false;
            }
            return pathSeen && summarySeen && searchSeen && replacementSeen;
        }

        [[nodiscard]] static bool parse_patch_operations(
            StructuredJsonCursor& cursor,
            std::vector<StructuredPatchOperation>& operations)
        {
            if (!cursor.consume('[') || cursor.consume(']'))
                return false;
            for (;;)
            {
                if (operations.size() >= 4u)
                    return false;
                StructuredPatchOperation operation{};
                if (!parse_patch_operation(cursor, operation))
                    return false;
                operations.push_back(std::move(operation));
                if (cursor.consume(']'))
                    return true;
                if (!cursor.consume(','))
                    return false;
            }
        }

        [[nodiscard]] static std::string one_line_metadata(
            std::string value,
            std::size_t maximumBytes)
        {
            for (char& byte : value)
            {
                if (byte == '\r' || byte == '\n' || byte == '\t')
                    byte = ' ';
            }
            value = trim(value);
            if (value.size() > maximumBytes)
                value.resize(maximumBytes);
            return value;
        }

        [[nodiscard]] static bool append_protocol_block(
            std::string& packet,
            std::string value,
            std::string_view finalNewlineField,
            std::string_view beginMarker,
            std::string_view endMarker,
            bool requireContent)
        {
            if (value.find('\0') != std::string::npos)
                return false;
            for (std::size_t position = 0u;
                 (position = value.find("\r\n", position))
                    != std::string::npos;)
            {
                value.replace(position, 2u, "\n");
            }
            std::ranges::replace(value, '\r', '\n');
            const bool finalNewline = !value.empty() && value.back() == '\n';
            if (finalNewline)
                value.pop_back();
            if (requireContent && value.empty())
                return false;

            packet.append(finalNewlineField);
            packet.append(finalNewline ? "true\n" : "false\n");
            packet.append(beginMarker);
            packet.push_back('\n');
            std::size_t begin{};
            for (;;)
            {
                const std::size_t newline = value.find('\n', begin);
                packet.push_back('|');
                packet.append(value.substr(
                    begin,
                    newline == std::string::npos
                        ? std::string::npos
                        : newline - begin));
                packet.push_back('\n');
                if (newline == std::string::npos)
                    break;
                begin = newline + 1u;
            }
            packet.append(endMarker);
            packet.push_back('\n');
            return true;
        }

        [[nodiscard]] static std::string normalize_structured_context_reply(
            std::string_view reply)
        {
            StructuredJsonCursor cursor{reply};
            if (!cursor.consume('{') || cursor.consume('}'))
                return {};
            std::string reason{};
            std::vector<std::string> paths{};
            bool reasonSeen{};
            bool pathsSeen{};
            for (;;)
            {
                const auto key = cursor.string();
                if (!key || !cursor.consume(':'))
                    return {};
                if (*key == "reason" && !reasonSeen)
                {
                    auto value = cursor.string();
                    if (!value)
                        return {};
                    reason = one_line_metadata(std::move(*value), 512u);
                    reasonSeen = true;
                }
                else if (*key == "paths" && !pathsSeen)
                {
                    if (!parse_string_array(cursor, paths, 12u))
                        return {};
                    pathsSeen = true;
                }
                else
                {
                    return {};
                }
                if (cursor.consume('}'))
                    break;
                if (!cursor.consume(','))
                    return {};
            }
            if (!cursor.complete() || !reasonSeen || reason.empty()
                || !pathsSeen || paths.empty())
            {
                return {};
            }

            std::string packet = "EPOCH_SOURCE_CONTEXT_REQUEST_V1\nreason: ";
            packet += reason;
            packet += "\npath_count: " + std::to_string(paths.size()) + "\n";
            for (const auto& path : paths)
                packet += "path: " + path + "\n";
            packet += "end_request\n";
            return packet;
        }

        [[nodiscard]] static std::string normalize_structured_patch_reply(
            std::string_view reply)
        {
            StructuredJsonCursor cursor{reply};
            if (!cursor.consume('{') || cursor.consume('}'))
                return {};
            std::string title{};
            std::string rationale{};
            std::vector<StructuredPatchOperation> operations{};
            bool titleSeen{};
            bool rationaleSeen{};
            bool operationsSeen{};
            for (;;)
            {
                const auto key = cursor.string();
                if (!key || !cursor.consume(':'))
                    return {};
                if (*key == "title" && !titleSeen)
                {
                    auto value = cursor.string();
                    if (!value)
                        return {};
                    title = one_line_metadata(std::move(*value), 160u);
                    titleSeen = true;
                }
                else if (*key == "rationale" && !rationaleSeen)
                {
                    auto value = cursor.string();
                    if (!value)
                        return {};
                    rationale = one_line_metadata(std::move(*value), 1'024u);
                    rationaleSeen = true;
                }
                else if (*key == "operations" && !operationsSeen)
                {
                    if (!parse_patch_operations(cursor, operations))
                        return {};
                    operationsSeen = true;
                }
                else
                {
                    return {};
                }
                if (cursor.consume('}'))
                    break;
                if (!cursor.consume(','))
                    return {};
            }
            if (!cursor.complete() || !titleSeen || title.empty()
                || !rationaleSeen || rationale.empty() || !operationsSeen
                || operations.empty())
            {
                return {};
            }

            std::string packet = "EPOCH_SOURCE_PATCH_PROPOSAL_V1\ntitle: ";
            packet += title;
            packet += "\nrationale: " + rationale;
            packet += "\nlifetime_seconds: 900\noperation_count: ";
            packet += std::to_string(operations.size());
            packet.push_back('\n');
            for (auto& operation : operations)
            {
                const std::string summary = one_line_metadata(
                    std::move(operation.summary), 512u);
                if (operation.path.empty() || summary.empty())
                    return {};
                packet += "begin_operation\narea: ";
                packet += operation.path.starts_with("Engine/")
                    ? "engine\n" : "project\n";
                packet += "path: " + operation.path + "\nsummary: "
                    + summary + "\n";
                if (!append_protocol_block(
                        packet,
                        std::move(operation.search),
                        "search_final_newline: ",
                        "begin_search",
                        "end_search",
                        true)
                    || !append_protocol_block(
                        packet,
                        std::move(operation.replacement),
                        "replacement_final_newline: ",
                        "begin_replacement",
                        "end_replacement",
                        false))
                {
                    return {};
                }
                packet += "end_operation\n";
            }
            packet += "end_proposal\n";
            return packet;
        }

        [[nodiscard]] static std::string normalize_structured_source_reply(
            std::string_view reply,
            StructuredSourceReply shape)
        {
            if (shape == StructuredSourceReply::context)
                return normalize_structured_context_reply(reply);
            if (shape == StructuredSourceReply::patch)
                return normalize_structured_patch_reply(reply);
            return std::string{reply};
        }

        [[nodiscard]] static std::string openai_chat_request_body(
            std::string_view model,
            std::string_view systemPrompt,
            std::string_view input,
            std::size_t maximumTokens,
            bool recoveryRequest,
            bool suppressReasoning)
        {
            std::string requestInput{input};
            if (recoveryRequest)
            {
                requestInput =
                    "Return only the final assistant answer. Do not expose analysis, reasoning, debug text, or drafting notes.\n"
                    "Original request:\n" + requestInput;
            }
            if (contains_text(lowercase_ascii(model), "qwen"))
                requestInput += "\n/no_think";

            const StructuredSourceReply sourceReply =
                structured_source_reply_for(input, suppressReasoning);

            std::string body;
            body.reserve(288u + systemPrompt.size() + requestInput.size());
            body += "{";
            body += "\"model\":\"" + json_escape(model) + "\",";
            body += "\"messages\":[";
            body += "{\"role\":\"system\",\"content\":\""
                + json_escape(systemPrompt) + "\"},";
            body += "{\"role\":\"user\",\"content\":\""
                + json_escape(requestInput) + "\"}";
            body += "],";
            if (suppressReasoning)
                body += "\"reasoning_effort\":\"none\",";
            body += "\"max_tokens\":" + std::to_string(maximumTokens) + ",";
            if (sourceReply != StructuredSourceReply::none)
            {
                body += "\"response_format\":";
                body += structured_source_schema(sourceReply);
                body.push_back(',');
            }
            body += "\"stream\":false";
            body += "}";
            return body;
        }

        static std::string openai_chat_complete(const std::string& endpoint_full,
            std::string_view model,
            std::string_view system_prompt,
            std::string_view input,
            const std::vector<std::pair<std::string, std::string>>& headers,
            std::size_t maximumTokens,
            std::uint32_t timeoutSeconds,
            bool suppressReasoning)
        {
            const StructuredSourceReply sourceReply =
                structured_source_reply_for(input, suppressReasoning);
            const auto request_once = [&](bool recoveryRequest, std::string* rawResponse) -> std::string
            {
                const std::string body = openai_chat_request_body(
                    model, system_prompt, input, maximumTokens, recoveryRequest,
                    suppressReasoning);
#if defined(_WIN32)
                const std::string resp = winhttp_post_json(
                    endpoint_full, body, headers, timeoutSeconds);
#elif defined(EPOCH_HAS_CURL)
                const std::string resp = http_post_json(
                    endpoint_full, body, headers, timeoutSeconds);
#else
                (void)endpoint_full;
                (void)headers;
                core::log::error("ai", "Local OpenAI-compatible request failed: no non-Windows HTTP transport is configured (build with libcurl).");
                return {};
#endif
                if (rawResponse)
                    *rawResponse = resp;
                std::string parsed = normalize_assistant_text(extract_lmstudio_message_content(resp));
                if (parsed.empty())
                    parsed = normalize_assistant_text(extract_openai_choice_message_content(resp));
                if (parsed.empty())
                {
                    std::string_view sv{ resp };
                    parsed = normalize_assistant_text(extract_json_string_field_after(sv, "\"content\""));
                    if (parsed.empty())
                        parsed = normalize_assistant_text(extract_json_string_field_after(sv, "\"text\""));
                }
                if (!parsed.empty())
                {
                    if (sourceReply == StructuredSourceReply::none)
                        return parsed;
                    std::string normalized =
                        normalize_structured_source_reply(parsed, sourceReply);
                    if (!normalized.empty())
                        return normalized;
                    core::log::warn(
                        "ai",
                        "Schema-constrained source reply could not be normalized; retrying without staging bytes.");
                    return {};
                }

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

            std::string lastFailure{};
            for (std::size_t attempt = 0u; attempt < 2u; ++attempt)
            {
                try
                {
                    std::string rawResponse{};
                    std::string reply = request_once(
                        attempt > 0u, &rawResponse);
                    if (is_promotable_assistant_text(reply))
                        return reply;

                    const std::string error =
                        extract_json_error_message(rawResponse);
                    if (!error.empty())
                    {
                        core::log::warn(
                            "ai",
                            epochengine::string_view{
                                error.data(), error.size()});
                        lastFailure =
                            std::string{"Local model API error: "} + error;
                        if (contains_text(
                                lowercase_ascii(error), "cancelled"))
                        {
                            return lastFailure;
                        }
                    }
                    else
                    {
                        if (!reply.empty())
                            return reply;
                        lastFailure =
                            has_hidden_reasoning_without_visible_content(rawResponse)
                            ? std::string{
                                "Local model returned hidden reasoning without visible assistant content. Epoch retried with reasoning disabled but received no final answer."}
                            : std::string{
                                "Local model returned no decodable assistant text. Check the selected model, endpoint, and OpenAI-compatible /v1/chat/completions response."};
                    }
                }
                catch (const std::exception& ex)
                {
                    lastFailure =
                        "Local OpenAI-compatible request failed: ";
                    lastFailure += ex.what();
                    if (contains_text(
                            lowercase_ascii(lastFailure), "cancelled"))
                    {
                        core::log::warn(
                            "ai",
                            epochengine::string_view{
                                lastFailure.data(), lastFailure.size()});
                        return lastFailure;
                    }
                }

                if (attempt == 0u)
                {
                    core::log::warn(
                        "ai",
                        "Local source-model request produced no usable response; retrying once with an explicit final-answer request.");
                }
            }
            core::log::error(
                "ai",
                epochengine::string_view{
                    lastFailure.data(), lastFailure.size()});
            return lastFailure;
        }

        struct ProcessCapture
        {
            std::string output{};
            int exit_code{-1};
            bool launched{};
            bool timed_out{};
            bool cancelled{};
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
                CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr,
                executable.parent_path().wstring().c_str(),
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
                if (running && g_aiRequestCancellationGeneration.load(std::memory_order_acquire)
                    != g_aiRequestStartCancellationGeneration)
                {
                    capture.cancelled = true;
                    TerminateProcess(process.hProcess, 125u);
                    WaitForSingleObject(process.hProcess, 5000u);
                    running = false;
                }
                else if (running && std::chrono::steady_clock::now() >= deadline)
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
                (void)setpriority(PRIO_PROCESS, 0, 5);

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
                if (running && g_aiRequestCancellationGeneration.load(std::memory_order_acquire)
                    != g_aiRequestStartCancellationGeneration)
                {
                    capture.cancelled = true;
                    kill(child, SIGTERM);
                    std::this_thread::sleep_for(std::chrono::milliseconds{250});
                    if (waitpid(child, &status, WNOHANG) == 0)
                        kill(child, SIGKILL);
                    waitpid(child, &status, 0);
                    running = false;
                }
                else if (running && std::chrono::steady_clock::now() >= deadline)
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

        [[nodiscard]] static unsigned int
            responsive_direct_thread_count(unsigned int logicalThreads) noexcept
        {
            if (logicalThreads == 0u)
                logicalThreads = 1u;
            return (std::max)(
                1u, (logicalThreads + 1u) / 2u);
        }

        [[nodiscard]] static std::vector<std::string>
            direct_llama_cpp_file_arguments(
                const EngineAiModel::Config& config,
                const std::filesystem::path& outputPath,
                const std::filesystem::path& systemPromptPath,
                const std::filesystem::path& userPromptPath)
        {
            std::vector<std::string> arguments{
                "--model", config.model,
                "--offline",
                "--log-disable",
                "--no-display-prompt",
                "--no-show-timings",
                "--single-turn",
                "--simple-io",
                "--color", "off",
                "--reasoning", "off",
                "--reasoning-budget", "0",
                "--output-file", outputPath.generic_string(),
                "--system-prompt-file", systemPromptPath.generic_string(),
                "--file", userPromptPath.generic_string(),
                "--ctx-size", std::to_string(config.context_tokens),
                "--n-predict", std::to_string(config.output_tokens),
                "--gpu-layers", std::to_string(config.gpu_layers)
            };
            if (config.threads > 0u)
            {
                arguments.push_back("--threads");
                arguments.push_back(std::to_string(config.threads));
                arguments.push_back("--threads-batch");
                arguments.push_back(std::to_string(config.threads));
            }
            return arguments;
        }

        [[nodiscard]] static bool
            direct_llama_cpp_prompt_file_arguments_contract()
        {
            EngineAiModel::Config config{};
            config.model = "model.gguf";
            config.context_tokens = 65'536u;
            config.output_tokens = 32'768u;
            config.threads = 12u;
            const std::filesystem::path outputPath{"response.txt"};
            const std::filesystem::path systemPath{"system.txt"};
            const std::filesystem::path userPath{"user.txt"};
            const auto arguments = direct_llama_cpp_file_arguments(
                config, outputPath, systemPath, userPath);
            const auto hasPair = [&](std::string_view flag,
                                     std::string_view value)
            {
                for (std::size_t index = 0u; index + 1u < arguments.size(); ++index)
                {
                    if (arguments[index] == flag && arguments[index + 1u] == value)
                        return true;
                }
                return false;
            };
            return hasPair("--output-file", "response.txt")
                && hasPair("--system-prompt-file", "system.txt")
                && hasPair("--file", "user.txt")
                && hasPair("--threads", "12")
                && hasPair("--threads-batch", "12")
                && responsive_direct_thread_count(24u) == 12u
                && responsive_direct_thread_count(0u) == 1u
                && std::ranges::find(arguments, "--prompt") == arguments.end()
                && std::ranges::find(arguments, "--system-prompt")
                    == arguments.end();
        }

        [[nodiscard]] static std::string llama_cpp_complete(
            const EngineAiModel::Config& config,
            std::string_view systemPrompt,
            std::string_view userText,
            bool allowStrictSourcePacket)
        {
            const std::filesystem::path outputRoot =
                std::filesystem::path{executable_cache_bucket("ai")}
                    / "transient";
            std::error_code outputError;
            std::filesystem::create_directories(outputRoot, outputError);
            if (outputError)
                return "Direct llama.cpp inference could not prepare its transient response path.";

            const std::uint64_t sequence =
                g_directInferenceOutputSequence.fetch_add(
                    1u, std::memory_order_relaxed);
            const std::uint64_t clock =
                static_cast<std::uint64_t>(
                    std::chrono::steady_clock::now()
                        .time_since_epoch().count());
            const std::string transientStem = "llama-cli-"
                + std::to_string(clock) + "-" + std::to_string(sequence);
            const std::filesystem::path outputPath =
                outputRoot / (transientStem + "-response.txt");
            const std::filesystem::path systemPromptPath =
                outputRoot / (transientStem + "-system.txt");
            const std::filesystem::path userPromptPath =
                outputRoot / (transientStem + "-user.txt");
            for (const auto& path :
                std::array{outputPath, systemPromptPath, userPromptPath})
            {
                std::filesystem::remove(path, outputError);
                outputError.clear();
            }

            struct ScopedTransientFiles final
            {
                ~ScopedTransientFiles()
                {
                    std::error_code ignored;
                    for (const auto& path : paths)
                    {
                        std::filesystem::remove(path, ignored);
                        ignored.clear();
                    }
                }
                std::array<std::filesystem::path, 3u> paths{};
            };
            const ScopedTransientFiles transientCleanup{{
                outputPath, systemPromptPath, userPromptPath}};

            const auto writePrompt = [](const std::filesystem::path& path,
                                        std::string_view text)
            {
                std::ofstream stream{
                    path, std::ios::binary | std::ios::trunc};
                if (!stream)
                    return false;
                stream.write(
                    text.data(), static_cast<std::streamsize>(text.size()));
                return stream.good();
            };
            if (!writePrompt(systemPromptPath, systemPrompt)
                || !writePrompt(userPromptPath, userText))
            {
                return "Direct llama.cpp inference could not prepare its transient prompt files.";
            }
            const std::vector<std::string> arguments =
                direct_llama_cpp_file_arguments(
                    config, outputPath, systemPromptPath, userPromptPath);

            ProcessCapture capture = capture_process(
                std::filesystem::path{config.executable}, arguments,
                std::chrono::seconds{config.timeout_seconds});
            if (!capture.launched)
                return "Direct llama.cpp inference could not start. Check the configured llama-cli executable.";
            if (capture.cancelled)
                return "Direct llama.cpp inference was cancelled.";
            if (capture.timed_out)
                return "Direct llama.cpp inference exceeded its time budget and was stopped.";
            if (capture.exit_code != 0)
            {
                const std::string captured = trim(capture.output);
                const std::string detail = captured.substr(
                    0u, (std::min)(captured.size(), std::size_t{2048u}));
                return "Direct llama.cpp inference failed with exit code "
                    + std::to_string(capture.exit_code)
                    + (detail.empty() ? std::string{"."} : std::string{": "} + detail);
            }
            const std::string transcript = read_small_text_file(
                outputPath, kMaximumInferenceOutputBytes);
            const std::string reply = allowStrictSourcePacket
                ? normalize_direct_llama_cpp_source_transcript(
                    transcript, userText)
                : normalize_direct_llama_cpp_transcript(
                    transcript, userText);
            if (reply.empty())
            {
                return transcript.empty()
                    ? "Direct llama.cpp inference returned no parseable assistant content: the response file was empty."
                    : "Direct llama.cpp inference returned no parseable assistant content: no framed assistant turn or strict source packet was found.";
            }
            return reply;
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

        if (m_cfg.backend != "llama_cpp_cli")
        {
            m_cfg.backend = "openai_chat";
            m_endpoint_full = normalize_openai_chat_endpoint(m_cfg.endpoint);
            m_cfg.model = resolve_model_name(m_cfg.endpoint, m_cfg.model);
        }
        if (!m_cfg.model.empty())
        {
            std::string msg = "OS AI model: ";
            msg += m_cfg.model;
            core::log::info("ai", epochengine::string_view{msg.data(), msg.size()});
        }
    }

    EngineAiReply EngineAiModel::submit(
        std::string_view user_input,
        InferenceWorkload workload)
    {
        EngineAiReply out{};
        const InferenceBudget budget = inference_budget(workload);
        if (!budget.valid() || user_input.empty()
            || user_input.size() > budget.maximum_prompt_bytes)
        {
            core::log::warn(
                "ai",
                "Local-model request rejected because its workload budget or prompt size is invalid.");
            return out;
        }

        Config effective = m_cfg;
        effective.context_tokens = budget.context_tokens;
        effective.output_tokens = budget.output_tokens;
        effective.timeout_seconds = budget.timeout_seconds;
        if (workload == InferenceWorkload::source_iteration
            && effective.backend == "llama_cpp_cli")
        {
            effective.gpu_layers = 0;
        }

        std::string sys = effective.backend == "llama_cpp_cli"
            ? "You are an operator-selected Epoch-local OS AI model invoked directly by Epoch through llama.cpp.\n"
            : "You are an operator-selected external OS AI model connected to Epoch through an operator-managed endpoint.\n";
        sys +=
            "Epoch is a C++23 game engine, editor, renderer, and tooling host. Model compute may be Epoch-local or offloaded to an external machine; both use the same host-owned MCP authority, approval, and evidence boundaries. Epoch never trains or self-trains the selected model.\n"
            "Rules:\n"
            " - Reply with correct English grammar and put only the final answer in assistant content.\n"
            " - Stay grounded in the current visible Epoch editor/project context.\n"
            " - Project authoring may change only the active scene or GUI through validated semantic calls and explicit operator approval.\n"
            " - Guarded engine development is a separate disposable sandbox lane with source proposals, builds, tests, evidence, and review gates.\n"
            " - Evidence review diagnoses existing output; it is not a source proposal and must not claim files were changed.\n"
            " - The operator's personal AI development is external to Epoch. Never access, modify, train from, or collect data from it.\n"
            " - Runtime exchanges, chat, scene edits, traces, and captures are operational evidence, never automatic training data.\n"
            " - MCP tool calls are bounded requests independent of model location; Epoch validates and executes them, then returns structured evidence.\n"
            " - Cite visible tool, build, scene, packet, log, or capture evidence before claiming a pass works.\n"
            " - Never create a server, listener, port bind, hidden control surface, or model bypass without explicit operator action.\n";
        if (workload == InferenceWorkload::source_iteration)
        {
            sys +=
                " - Guarded source iteration is a machine protocol, not a prose answer. The first response line must be an EPOCH_SOURCE_ protocol header from the request. "
                "If evidence is insufficient, return exactly EPOCH_SOURCE_EVIDENCE_INSUFFICIENT_V1 and nothing else. "
                "Otherwise return one bounded atomic proposal and never claim that Epoch staged, built, tested, or promoted it.\n";
        }

        const std::size_t n = std::max<std::size_t>(1, effective.best_of);

        // No scorer is active in this lane; first non-empty assistant content wins.
        for (std::size_t i = 0; i < n; ++i)
        {
            std::string txt = effective.backend == "llama_cpp_cli"
                ? llama_cpp_complete(
                    effective, sys, user_input,
                    workload == InferenceWorkload::source_iteration)
                : openai_chat_complete(
                    m_endpoint_full,
                    effective.model,
                    sys,
                    user_input,
                    {},
                    effective.output_tokens,
                    effective.timeout_seconds,
                    workload == InferenceWorkload::source_iteration);

            if (workload == InferenceWorkload::source_iteration
                && effective.backend != "llama_cpp_cli")
            {
                // OpenAI-compatible servers may wrap one otherwise valid
                // machine packet in a Markdown fence or short prose. Reuse
                // the bounded line-framed extractor from the direct runtime;
                // proposal decoding, grounding, review, and approval remain
                // mandatory after extraction.
                if (std::string packet =
                        normalize_direct_llama_cpp_source_transcript(
                            txt, user_input);
                    !packet.empty())
                    txt = std::move(packet);
            }

            txt = trim(txt);
            if (txt.empty())
                continue;
            if (txt.size() > budget.maximum_reply_bytes)
            {
                core::log::warn(
                    "ai",
                    "Local-model reply exceeded the bounded workload response size and was rejected.");
                continue;
            }

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
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        if (g_engineAi)
            return;

        restore_runtime_preference_if_needed();
        apply_project_ai_profile_if_present();
        if (!g_modelUseConfirmedForSession)
        {
            core::log::info("ai", "OS AI model not initialized: model use awaits session confirmation.");
            return;
        }
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

            g_engineAi = std::make_shared<EngineAiModel>(EngineAiModel::Config{
                .backend = "llama_cpp_cli",
                .model = g_directModel,
                .executable = g_directExecutable,
                .threads = responsive_direct_thread_count(
                    std::thread::hardware_concurrency()),
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
            if (g_selectedModel.empty())
            {
                core::log::info("ai", "OS AI model not initialized: no local model selected.");
                return;
            }

            g_engineAi = std::make_shared<EngineAiModel>(EngineAiModel::Config{
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
        cancel_engine_ai_request();
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        g_engineAi.reset();
        core::log::info("ai", "OS AI model shutdown");
    }

    void cancel_engine_ai_request() noexcept
    {
        g_aiRequestCancellationGeneration.fetch_add(1u, std::memory_order_acq_rel);
#if defined(_WIN32)
        HINTERNET activeRequest{};
        {
            const std::lock_guard lock{g_activeAiWinHttpMutex};
            activeRequest = g_activeAiWinHttpRequest;
            g_activeAiWinHttpRequest = nullptr;
        }
        if (activeRequest)
            WinHttpCloseHandle(activeRequest);
#endif
    }


    std::string default_workspace_root()
    {
        const auto workspace = epochengine::core::path::example_console_workspace_dir();
        if (!workspace.empty())
            return workspace.generic_string();

        return {};
    }

    std::string review_fixtures_root()
    {
        return "Engine/ai/evals/fixtures";
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

    std::string epoch_local_llama_cpp_root()
    {
        return epoch_local_llama_cpp_root_path().generic_string();
    }

    std::string epoch_local_llama_cpp_executable()
    {
        return epoch_local_llama_cpp_executable_path().generic_string();
    }

    std::string epoch_local_qwen38_root()
    {
        return epoch_local_qwen38_root_path().generic_string();
    }

    std::string epoch_local_qwen38_model()
    {
        return epoch_local_qwen38_model_path().generic_string();
    }

    EpochLocalAiInstallStatus epoch_local_ai_install_status()
    {
        EpochLocalAiInstallStatus status{};
        const auto runtimeRoot = epoch_local_llama_cpp_root_path();
        const auto runtimeExecutable = epoch_local_llama_cpp_executable_path();
        const auto canonicalModelRoot = epoch_local_qwen38_root_path();
        const auto canonicalModelFile = epoch_local_qwen38_model_path();
        const auto legacyModelRoot = epoch_local_qwen38_legacy_root_path();
        const auto legacyModelFile = legacyModelRoot
            / std::string{kEpochLocalQwen38ModelFile};
        const auto modelPlan = model_install::plan_for(
            kEpochLocalQwen38ModelPackageId);
        status.runtime_root = runtimeRoot.generic_string();
        status.runtime_executable = runtimeExecutable.generic_string();
        status.model_root = canonicalModelRoot.generic_string();
        status.model_file = canonicalModelFile.generic_string();
        status.runtime_executable_ready = regular_file(runtimeExecutable);
        status.runtime_receipt_ready = receipt_contains(
            runtimeRoot / "installed.runtime.json",
            {kEpochLocalLlamaCppRelease,
             kEpochLocalLlamaCppRevision,
             kEpochLocalLlamaCppArtifact,
             kEpochLocalLlamaCppArtifactSha256});

        if (modelPlan && model_install::valid_plan(*modelPlan)
            && modelPlan->artifacts.size() == 1u)
        {
            const auto& artifact = modelPlan->artifacts.front();
            status.model_file_ready = regular_file_with_size(
                canonicalModelFile, artifact.bytes);
            status.model_receipt_ready = model_install::receipt_matches(
                *modelPlan,
                read_exact_text_file(
                    canonicalModelRoot
                        / std::string{model_install::receipt_filename},
                    128u * 1024u));

            if (!status.model_file_ready || !status.model_receipt_ready)
            {
                const bool legacyFileReady = regular_file_with_size(
                    legacyModelFile, artifact.bytes);
                const bool legacyReceiptReady = receipt_contains(
                    legacyModelRoot / "installed.model.json",
                    {"epoch.local_ai.model.install.v1",
                     kEpochLocalQwen38ModelPackageId,
                     kEpochLocalQwen38ModelRevision,
                     kEpochLocalQwen38ModelFile,
                     kEpochLocalQwen38ModelSha256});
                if (legacyFileReady && legacyReceiptReady)
                {
                    status.model_root = legacyModelRoot.generic_string();
                    status.model_file = legacyModelFile.generic_string();
                    status.model_file_ready = true;
                    status.model_receipt_ready = true;
                }
            }
        }

        if (status.ready())
        {
            status.message =
                "Epoch-local Qwen3.8 and llama.cpp are installed and integrity-receipted.";
        }
        else if (!status.runtime_executable_ready
            || !status.runtime_receipt_ready)
        {
            status.message =
                "Epoch-local llama.cpp is not installed from the pinned artifact.";
        }
        else
        {
            status.message =
                "Epoch-local Qwen3.8 is not installed from the pinned GGUF artifact.";
        }
        return status;
    }

    bool epoch_local_ai_install_contract() noexcept
    {
        try
        {
            const auto plan = model_install::plan_for(
                kEpochLocalQwen38ModelPackageId);
            if (!plan || !model_install::valid_plan(*plan)
                || plan->artifacts.size() != 1u)
                return false;

            const auto& artifact = plan->artifacts.front();
            const std::filesystem::path executableRoot =
                epochengine::core::path::executable_dir();
            const std::filesystem::path modelsRoot{
                executable_cache_bucket("models")};
            const std::filesystem::path expectedLegacy = modelsRoot
                / std::string{kEpochLocalQwen38ModelPackageId};
            const std::filesystem::path expectedVersion = expectedLegacy
                / "versions" / std::string{kEpochLocalQwen38ModelRevision};
            const bool executableLocalCache = executableRoot.empty()
                || (epoch_local_llama_cpp_root_path()
                        == executableRoot / "cache" / "packages"
                            / std::string{kEpochLocalLlamaCppRuntimePackageId}
                    && modelsRoot == executableRoot / "cache" / "models");
            const std::string receipt =
                model_install::deterministic_receipt(*plan);
            std::string mutatedReceipt = receipt;
            mutatedReceipt.push_back(' ');

            return executableLocalCache
                && epoch_local_qwen38_legacy_root_path() == expectedLegacy
                && epoch_local_qwen38_root_path() == expectedVersion
                && epoch_local_qwen38_model_path()
                    == expectedVersion
                        / std::string{kEpochLocalQwen38ModelFile}
                && plan->package_id == kEpochLocalQwen38ModelPackageId
                && plan->revision == kEpochLocalQwen38ModelRevision
                && plan->official_source
                    == "https://huggingface.co/Qwen/Qwen3.8-27B"
                && plan->artifact_source
                    == "https://huggingface.co/unsloth/Qwen3.8-27B-GGUF"
                && artifact.file == kEpochLocalQwen38ModelFile
                && artifact.bytes == kEpochLocalQwen38ModelBytes
                && artifact.sha256 == kEpochLocalQwen38ModelSha256
                && artifact.source_url == kEpochLocalQwen38ModelUrl
                && !receipt.empty()
                && model_install::receipt_matches(*plan, receipt)
                && !model_install::receipt_matches(*plan, mutatedReceipt)
                && kEpochLocalLlamaCppRuntimePackageId
                    == std::string_view{"local_ai_llama_cpp_runtime"}
                && kEpochLocalLlamaCppRevision.size() == 40u
                && kEpochLocalLlamaCppArtifactSha256.size() == 64u
                && kEpochLocalLlamaCppArtifact.find(
                    kEpochLocalLlamaCppRelease) != std::string_view::npos;
        }
        catch (...)
        {
            return false;
        }
    }
    ProviderMode current_provider_mode() noexcept
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        return g_providerMode;
    }

    LocalInferenceTransport current_local_inference_transport() noexcept
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        restore_runtime_preference_if_needed();
        return g_localTransport;
    }

    std::string_view local_inference_transport_name(LocalInferenceTransport transport) noexcept
    {
        switch (transport)
        {
        case LocalInferenceTransport::OpenAiCompatible:
            return "OpenAI-compatible HTTP API";
        case LocalInferenceTransport::LlamaCppCli:
            return "Direct llama.cpp CLI";
        }
        return "Unknown local runtime";
    }

    DirectRuntimeStatus direct_runtime_status()
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
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
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        restore_runtime_preference_if_needed();
        if (const auto executable = discover_llama_executable(); !executable.empty())
            g_directExecutable = executable.generic_string();
        if (const auto model = discover_gguf_model(); !model.empty())
            g_directModel = model.generic_string();
        const DirectRuntimeStatus status = direct_runtime_status();
        if (g_localTransport == LocalInferenceTransport::LlamaCppCli)
            g_modelDetectionStatus = status.message;
        return status;
    }

    bool select_direct_runtime(std::string_view executable, std::string_view model)
    {
        const std::filesystem::path executablePath{trim(executable)};
        const std::filesystem::path modelPath{trim(model)};
        if (!regular_file(executablePath) || !regular_file(modelPath) || modelPath.extension() != ".gguf")
        {
            const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
            g_modelDetectionStatus = "Direct runtime selection rejected: choose an existing llama-cli executable and GGUF model.";
            return false;
        }

        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        g_engineAi.reset();
        g_directExecutable = std::filesystem::absolute(executablePath).generic_string();
        g_directModel = std::filesystem::absolute(modelPath).generic_string();
        g_localTransport = LocalInferenceTransport::LlamaCppCli;
        g_modelUseConfirmedForSession = true;
        g_selectedModel = modelPath.filename().string();
        persist_runtime_preference();
        init_engine_ai();
        return g_engineAi != nullptr;
    }

    void select_openai_compatible_runtime()
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        g_engineAi.reset();
        g_localTransport = LocalInferenceTransport::OpenAiCompatible;
        g_modelUseConfirmedForSession = false;
        if (!g_detectedModels.empty())
        {
            g_modelDetectionStatus =
                "Detected " + std::to_string(g_detectedModels.size())
                + " local API model(s); model use requires operator confirmation.";
        }
        else if (!g_selectedModel.empty())
        {
            g_modelDetectionStatus =
                "Configured local API model: " + g_selectedModel;
        }
        else
        {
            g_modelDetectionStatus =
                "Local API selected; model inventory has not been scanned.";
        }
        persist_runtime_preference();
        init_engine_ai();
    }

    std::string active_model_name()
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::LlamaCppCli)
            return g_directModel.empty() ? std::string{} : std::filesystem::path{g_directModel}.filename().string();
        restore_selected_model_preference_if_needed();
        return g_selectedModel;
    }

    std::string active_provider_summary()
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
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
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        return g_detectedModels;
    }

    std::string model_detection_status()
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        restore_runtime_preference_if_needed();
        if (g_localTransport == LocalInferenceTransport::OpenAiCompatible)
            restore_selected_model_preference_if_needed();
        return g_modelDetectionStatus;
    }

    bool is_engine_ai_initialized() noexcept
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        return g_engineAi != nullptr;
    }

    bool is_model_use_confirmed() noexcept
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        return g_modelUseConfirmedForSession;
    }

    std::string model_connection_status()
    {
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        restore_runtime_preference_if_needed();
        if (!g_modelUseConfirmedForSession)
        {
            return active_model_name().empty()
                ? "No local model is configured."
                : "Configured model awaits confirmation for this session.";
        }
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
        std::string endpoint;
        {
            const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
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
            endpoint = g_selectedEndpoint;
        }

        std::vector<std::string> detected = fetch_detected_models(endpoint);

        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        if (g_localTransport != LocalInferenceTransport::OpenAiCompatible
            || g_selectedEndpoint != endpoint)
        {
            return g_detectedModels;
        }

        g_detectedModels = std::move(detected);
        if (g_detectedModels.empty())
        {
            g_modelDetectionStatus = g_selectedModel.empty()
                ? "No local models detected. Start LM Studio, Ollama, or another OpenAI-compatible API and check endpoint."
                : "Configured model '" + g_selectedModel + "' is selected, but no local model inventory was detected at the endpoint.";
        }
        else if (!g_selectedModel.empty())
        {
            const bool selectedAvailable =
                std::find(g_detectedModels.begin(), g_detectedModels.end(), g_selectedModel) != g_detectedModels.end();
            if (selectedAvailable)
            {
                if (g_modelUseConfirmedForSession && !g_engineAi)
                    init_engine_ai();

                g_modelDetectionStatus = !g_modelUseConfirmedForSession
                    ? "Configured model is available and awaits session confirmation: " + g_selectedModel
                    : g_engineAi
                    ? "Selected and initialized model client: " + g_selectedModel
                    : "Selected model: " + g_selectedModel + " (" + std::to_string(g_detectedModels.size()) + " local model(s) detected), but client init failed.";
                persist_selected_model_preference(g_selectedModel);
            }
            else
            {
                const std::string staleModel = g_selectedModel;
                g_engineAi.reset();
                g_modelDetectionStatus =
                    "Selected model '" + staleModel + "' was not reported by the endpoint; keeping the operator preference. Choose another detected model to replace it.";
            }
        }
        else
        {
            g_modelDetectionStatus = "Detected " + std::to_string(g_detectedModels.size()) + " local model(s); choose the exact OS AI model to enable chat/tooling.";
        }

        return g_detectedModels;
    }

    bool select_active_model(std::string_view model_id)
    {
        if (current_local_inference_transport() == LocalInferenceTransport::LlamaCppCli)
        {
            std::string executable;
            std::string model;
            {
                const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
                executable = g_directExecutable;
                model = g_directModel;
            }
            return select_direct_runtime(executable, model);
        }

        const std::string selected = trim(model_id);
        if (selected.empty())
            return false;

        bool inventoryMissing = false;
        {
            const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
            restore_selected_model_preference_if_needed();
            inventoryMissing = g_detectedModels.empty();
        }
        if (inventoryMissing)
            (void)refresh_detected_models();

        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
        if (std::find(g_detectedModels.begin(), g_detectedModels.end(), selected) == g_detectedModels.end())
        {
            g_modelDetectionStatus = "Could not select OS model '" + selected + "' because the endpoint did not report it.";
            return false;
        }

        g_engineAi.reset();
        g_selectedModel = selected;
        g_modelUseConfirmedForSession = true;
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
        const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
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
        const InferenceBudget sourceBudget =
            inference_budget(InferenceWorkload::source_iteration);
        manifest.host_context_budget_tokens = sourceBudget.context_tokens;
        manifest.host_output_budget_tokens = sourceBudget.output_tokens;
        manifest.source_iteration_budget_available =
            manifest.available && sourceBudget.valid();

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
            .review_fixture_root = review_fixtures_root(),
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

    std::string normalize_assistant_reply(std::string_view reply)
    {
        return normalize_assistant_text(reply);
    }
    std::string normalize_direct_llama_cpp_reply(
        std::string_view transcript,
        std::string_view userText)
    {
        return normalize_direct_llama_cpp_transcript(transcript, userText);
    }

    std::string normalize_direct_llama_cpp_source_reply(
        std::string_view transcript,
        std::string_view userText)
    {
        return normalize_direct_llama_cpp_source_transcript(
            transcript, userText);
    }

    bool direct_llama_cpp_prompt_transport_contract()
    {
        return direct_llama_cpp_prompt_file_arguments_contract();
    }

    bool openai_source_iteration_request_contract()
    {
        const std::string sourceBody = openai_chat_request_body(
            "qwen/test", "system",
            "EPOCH_SOURCE_CONTEXT_REQUEST_V1", 512u, false, true);
        const std::string patchBody = openai_chat_request_body(
            "qwen/test", "system",
            "EPOCH_SOURCE_PATCH_PROPOSAL_V1", 512u, false, true);
        const std::string ordinaryBody = openai_chat_request_body(
            "qwen/test", "system", "request", 512u, false, false);
        const std::string contextPacket = normalize_structured_context_reply(
            R"json({"paths":["Engine/src/ai/ai.engine.cpp"],"reason":"Inspect the selected transport"})json");
        const std::string patchPacket = normalize_structured_patch_reply(
            R"json({"operations":[{"replacement":"int value = 2;","search":"int value = 1;","summary":"Change the reviewed value","path":"Engine/src/ai/example.cpp"}],"rationale":"Repair the reviewed value","title":"Repair value"})json");
        return sourceBody.find("\"reasoning_effort\":\"none\"")
                != std::string::npos
            && sourceBody.find("/no_think") != std::string::npos
            && sourceBody.find("\"response_format\"") != std::string::npos
            && sourceBody.find("epoch_source_context") != std::string::npos
            && patchBody.find("epoch_source_patch") != std::string::npos
            && sourceBody.find("\"stream\":false") != std::string::npos
            && ordinaryBody.find("\"reasoning_effort\"")
                == std::string::npos
            && ordinaryBody.find("\"response_format\"")
                == std::string::npos
            && contextPacket.find(
                "EPOCH_SOURCE_CONTEXT_REQUEST_V1\nreason: Inspect the selected transport\npath_count: 1\npath: Engine/src/ai/ai.engine.cpp\nend_request\n")
                == 0u
            && patchPacket.find(
                "EPOCH_SOURCE_PATCH_PROPOSAL_V1\ntitle: Repair value\n")
                == 0u
            && patchPacket.find(
                "begin_search\n|int value = 1;\nend_search\n")
                != std::string::npos
            && patchPacket.find(
                "begin_replacement\n|int value = 2;\nend_replacement\n")
                != std::string::npos
            && model_http_timeout_milliseconds(600u) == 600'000;
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

    std::string send_to_engine_ai(
        const std::string& user_text,
        InferenceWorkload workload)
    {
        const InferenceBudget budget = inference_budget(workload);
        if (!budget.valid())
            return "The selected local-model workload has an invalid host budget.";
        if (user_text.empty() || user_text.size() > budget.maximum_prompt_bytes)
        {
            return "The local-model request is empty or exceeds its bounded host prompt budget.";
        }

        const std::uint64_t requestCancellationGeneration =
            g_aiRequestCancellationGeneration.load(std::memory_order_acquire);

        std::shared_ptr<EngineAiModel> client;
        std::string selectedModel;
        std::string selectedEndpoint;
        {
            const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
            if (!g_modelUseConfirmedForSession || g_selectedModel.empty())
                return "No local AI model has been confirmed for this session. Confirm a discovered model before sending a request.";

            if (!g_engineAi)
                init_engine_ai();
            client = g_engineAi;
            selectedModel = g_selectedModel;
            selectedEndpoint = g_selectedEndpoint;
        }
        if (!client)
            return "OS AI model could not initialize. Confirm a local model is selected and the endpoint is reachable.";

        EngineAiReply reply{};
        {
            const std::lock_guard<std::mutex> requestLock{g_aiRequestMutex};
            if (g_aiRequestCancellationGeneration.load(std::memory_order_acquire)
                != requestCancellationGeneration)
            {
                return "Local-model request cancelled before execution.";
            }
            g_aiRequestStartCancellationGeneration = requestCancellationGeneration;
            reply = client->submit(user_text, workload);
        }
        const std::string loweredReply = lowercase_ascii(reply.text);
        if (starts_with_text(loweredReply, "local model api error")
            || starts_with_text(loweredReply,
                "local openai-compatible request failed")
            || starts_with_text(loweredReply,
                "local model returned hidden reasoning")
            || starts_with_text(loweredReply,
                "local model returned no decodable assistant text")
            || starts_with_text(loweredReply,
                "direct llama.cpp inference could not")
            || starts_with_text(loweredReply,
                "direct llama.cpp inference exceeded")
            || starts_with_text(loweredReply,
                "direct llama.cpp inference was cancelled")
            || starts_with_text(loweredReply,
                "direct llama.cpp inference returned no parseable"))
        {
            return reply.text;
        }
        if (!reply.text.empty() && !is_promotable_assistant_text(reply.text))
        {
            core::log::warn("ai", "Local model returned non-promotable assistant content.");
            return "Local model returned reasoning/debug text instead of final assistant content. Adjust the local model chat template or choose a content-producing OS model before using Engine AI chat.";
        }
        if (reply.text.empty())
            return std::string("No decodable reply from selected local model '") + selectedModel
                + "' at " + selectedEndpoint
                + ". Check the endpoint/model selection and retry.";
        return reply.text;
    }
}
