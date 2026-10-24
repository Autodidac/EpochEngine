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
// Epoch system notes (auto-checked):
// Epoch STL: honor core.stl_types.hpp types/operators; fix callers, not core overloads.
// Epoch AI: preserve model selection, candidate isolation and host verification gates.
module;

#include "../include/core.stl_types.hpp"
#include "../../include/epoch.edition.hpp"
#include "../../include/core.utf8.hpp"
#include "../include/ai.token_metrics.hpp"
#include "ai.native_runtime.hpp"
#include "ai.thinking_policy.hpp"

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
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
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stop_token>
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
import core.logger;
import core.path;

namespace epochengine::ai
{
    namespace
    {
        std::recursive_mutex g_aiStateMutex{};
        std::optional<std::string> g_localModelApiToken{};
        std::string g_localModelApiTokenEndpoint{};
        std::atomic_uint64_t g_directInferenceOutputSequence{1u};
        std::atomic_bool g_modelHttpRetirementUncertain{false};
        std::atomic_uint64_t g_modelHttpNullContextCallbacks{0u};
        constexpr std::size_t kMaximumModelHttpEnvelopeBytes = 8u * 1024u * 1024u;
        constexpr std::string_view kModelRequestCancelled =
            "Local-model request cancelled before completion.";
        using ModelReplyChunkConsumer = std::function<void(std::string_view)>;

        void notify_model_stage(const ModelRequestObserver& observer,
            ModelRequestStage stage) noexcept
        {
            try { if (observer) observer(stage); }
            catch (...) { /* Observers cannot disrupt request ownership. */ }
        }

        void notify_model_progress(const ModelProgressObserver& observer,
            const ModelRequestProgress& progress) noexcept
        {
            try { if (observer) observer(progress); }
            catch (...) { /* Telemetry cannot change request ownership. */ }
        }

        struct ScopedModelObservation final
        {
            const ModelRequestObserver& observer;
            std::stop_token cancellation;
            bool succeeded{};
            bool retirement_failed{};
            ~ScopedModelObservation()
            {
                notify_model_stage(observer, retirement_failed
                    ? ModelRequestStage::retirement_failed : cancellation.stop_requested()
                    ? ModelRequestStage::cancelled
                    : succeeded ? ModelRequestStage::completed : ModelRequestStage::failed);
            }
        };

        class ModelTransportRetirementFailure final : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        enum class ModelAttemptFailure : unsigned char
        {
            recoverable, operation_timeout, total_timeout, cancelled, retirement_failed
        };

        class ModelTransportTimeout final : public std::runtime_error
        {
        public:
            ModelTransportTimeout(bool totalBudget, const char* detail)
                : std::runtime_error(detail), total_budget(totalBudget) {}
            bool total_budget{};
        };

        [[nodiscard]] constexpr bool retry_model_attempt(std::size_t attempt,
            ModelAttemptFailure failure, bool cancelled, bool generationStarted = false) noexcept
        {
            return !cancelled && !generationStarted && attempt == 0u
                && (failure == ModelAttemptFailure::recoverable
                    || failure == ModelAttemptFailure::operation_timeout);
        }

        [[nodiscard]] constexpr std::size_t model_attempt_limit(std::string_view phase) noexcept
        {
            // All read-only model requests receive the same one-attempt recovery
            // policy; tool actions and source writes are never replayed here.
            (void)phase;
            return 2u;
        }

        [[nodiscard]] constexpr bool model_total_budget_elapsed(
            std::uint64_t elapsedMilliseconds, std::uint32_t budgetSeconds) noexcept
        {
            const auto limit = static_cast<std::uint64_t>(budgetSeconds) * 1'000u;
            // Native timeout timers may round slightly before our steady-clock
            // observation. A sub-second boundary must not restart a long call.
            return elapsedMilliseconds >= limit
                || limit - elapsedMilliseconds <= 999u;
        }

        [[nodiscard]] std::string model_timeout_message(std::size_t attempt,
            std::uint64_t elapsedMilliseconds, std::uint32_t budgetSeconds,
            bool totalBudget, std::string_view transportDetail, std::size_t maximumAttempts = 2u)
        {
            return "Local OpenAI-compatible request failed: attempt "
                + std::to_string(attempt + 1u) + "/" + std::to_string(maximumAttempts) + ", elapsed "
                + std::to_string(elapsedMilliseconds / 1'000u) + "s, per-attempt limit "
                + std::to_string(budgetSeconds) + "s. "
                + (totalBudget
                    ? "The whole request time budget expired; the same expensive generation was not automatically restarted. "
                    : "A transport operation timed out before the whole request budget expired. ")
                + std::string{transportDetail};
        }

        class GlobalRequestCancellation final
        {
        public:
            [[nodiscard]] std::stop_token capture()
            {
                std::scoped_lock lock{mutex_};
                if (source_.stop_requested())
                    source_ = std::stop_source{};
                return source_.get_token();
            }

            void cancel() noexcept
            {
                std::stop_source captured{std::nostopstate};
                {
                    std::scoped_lock lock{mutex_};
                    captured = source_;
                }
                // Callbacks may acquire their request's wait mutex; no global
                // mutex is held while they execute synchronously here.
                (void)captured.request_stop();
            }

        private:
            std::mutex mutex_{};
            std::stop_source source_{};
        };

        struct ForwardRequestStop final
        {
            std::stop_source destination;
            void operator()() noexcept { (void)destination.request_stop(); }
        };

        struct RequestCancellation final
        {
            explicit RequestCancellation(std::stop_token local, std::stop_token global)
                : local_(local, ForwardRequestStop{combined_}),
                  global_(global, ForwardRequestStop{combined_}) {}

            [[nodiscard]] std::stop_token token() const noexcept
            { return combined_.get_token(); }

        private:
            std::stop_source combined_{};
            std::stop_callback<ForwardRequestStop> local_;
            std::stop_callback<ForwardRequestStop> global_;
        };

        class ModelRequestGate final
        {
        public:
            [[nodiscard]] bool acquire(std::stop_token cancellation)
            {
                std::unique_lock lock{mutex_};
                if (!ready_.wait(lock, cancellation, [this] { return !occupied_; })
                    || cancellation.stop_requested())
                    return false;
                occupied_ = true;
                return true;
            }

            void release() noexcept
            {
                {
                    std::scoped_lock lock{mutex_};
                    occupied_ = false;
                }
                ready_.notify_all();
            }

        private:
            std::mutex mutex_{};
            std::condition_variable_any ready_{};
            bool occupied_{};
        };

        class ScopedModelRequest final
        {
        public:
            ScopedModelRequest(ModelRequestGate& gate, std::stop_token cancellation)
                : gate_(gate), acquired_(gate.acquire(cancellation)) {}
            ~ScopedModelRequest() { if (acquired_) gate_.release(); }
            ScopedModelRequest(const ScopedModelRequest&) = delete;
            ScopedModelRequest& operator=(const ScopedModelRequest&) = delete;
            [[nodiscard]] bool acquired() const noexcept { return acquired_; }
        private:
            ModelRequestGate& gate_;
            bool acquired_{};
        };

        GlobalRequestCancellation g_aiCancellation{};
        ModelRequestGate g_aiRequestGate{};

        template<class Operation>
        [[nodiscard]] std::string run_model_transport_attempt(
            std::stop_token cancellation, Operation&& operation)
        {
            if (cancellation.stop_requested())
                return std::string{kModelRequestCancelled};
            std::string reply = std::forward<Operation>(operation)();
            return cancellation.stop_requested()
                ? std::string{kModelRequestCancelled} : std::move(reply);
        }
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

        static std::string configured_endpoint_override()
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
            return {};
        }

        static std::string configured_endpoint()
        {
            const auto override = configured_endpoint_override();
            return override.empty() ? "http://localhost:1234" : override;
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
        std::vector<std::string> g_savedLocalEndpoints{
            "http://localhost:1234/v1", "http://127.0.0.1:14321/v1"};
        bool g_endpointPreferencesLoaded{};
        // Streaming is the default for compatible APIs, including current
        // Epoch AI. Preserve an explicit saved opt-out for older providers.
        std::vector<std::string> g_nonStreamingLocalEndpoints{};
        struct LocalContextOverride final
        {
            std::string endpoint{};
            std::string model{};
            std::size_t tokens{};
            friend bool operator==(const LocalContextOverride&, const LocalContextOverride&) = default;
        };
        std::vector<LocalContextOverride> g_localContextOverrides{};
        std::string g_selectedModel{ configured_model() };
        // Model-specific, user-visible thinking selection. Keep it independent
        // from endpoint/model enumeration and model load state.
        std::unordered_map<std::string, thinking::Mode> g_thinkingOverrides{};
        bool g_thinkingPreferencesLoaded{};

        LocalModelSelectionOrigin g_selectedModelOrigin = g_selectedModel.empty()
            ? LocalModelSelectionOrigin::none : LocalModelSelectionOrigin::configured;
        struct LocalModelPreference
        {
            std::string model_id{};
            std::string endpoint{};
            bool legacy{};
        };
        LocalModelPreference g_rememberedModel{};
        bool g_selectedModelPreferenceLoaded = false;
        std::vector<std::string> g_detectedModels{};
        struct DetectedModelCapacity final
        {
            std::string model_id{};
            std::size_t loaded_context_tokens{};
            std::size_t maximum_context_tokens{};
        };
        std::vector<DetectedModelCapacity> g_detectedModelCapacities{};
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
        bool g_projectAiAllowsLocalFallback = true;

        [[nodiscard]] static std::filesystem::path cache_bucket_path(
            const std::filesystem::path& candidateData,
            const std::filesystem::path& executableRoot,
            const std::filesystem::path& runtimeRoot,
            std::string_view bucket)
        {
            if (!candidateData.empty())
                return candidateData / "cache" / std::string{bucket};
            if (!executableRoot.empty())
                return executableRoot / "cache" / std::string{bucket};
            if (!runtimeRoot.empty())
                return runtimeRoot / "cache" / std::string{bucket};
            return std::filesystem::path{"cache"} / std::string{bucket};
        }

        static std::string executable_cache_bucket(std::string_view bucket)
        {
            const auto candidate = epochengine::core::path::candidate_data_root();
            if (!candidate.empty())
                return cache_bucket_path(candidate, {}, {}, bucket).generic_string();
            return cache_bucket_path({},
                epochengine::core::path::executable_dir(),
                epochengine::core::path::runtime_root_dir(), bucket).generic_string();
        }

        static std::filesystem::path thinking_preference_file()
        {
            return std::filesystem::path{executable_cache_bucket("models")}
                / "thinking_preferences.conf";
        }

        static std::filesystem::path selected_model_preference_file()
        {
            return std::filesystem::path{ executable_cache_bucket("models") } / "selected_os_model.txt";
        }

        static std::filesystem::path local_runtime_preference_file()
        {
            return std::filesystem::path{ executable_cache_bucket("models") } / "local_inference_runtime.conf";
        }

        static std::filesystem::path local_endpoint_preference_file()
        {
            return std::filesystem::path{executable_cache_bucket("models")}
                / "local_api_endpoints.conf";
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
                "/api/v1/models",
                "/api/v1",
                "/api/v1/chat",
                "/v1/chat/completions",
                "/v1/responses",
                "/api/tasks",
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
                "/api/v1/models",
                "/api/v1",
                "/api/v1/chat",
                "/v1/chat/completions",
                "/v1/responses",
                "/api/tasks",
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

        constexpr std::string_view kLocalModelPreferenceHeader =
            "EPOCH_LOCAL_MODEL_PREFERENCE_V1\n";
        constexpr std::string_view kDefaultLocalModel = "qwen/qwen3.8-27b";
        constexpr std::string_view kOriginalLocalEndpoint =
            "http://localhost:1234/v1/chat/completions";

#include "implementation/ai.connection_preferences.inl"
#include "implementation/ai.model_http_transport.inl"
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

        static std::vector<std::string> extract_native_model_keys(std::string_view response);
        [[nodiscard]] static std::vector<DetectedModelCapacity>
            extract_native_model_capacities(std::string_view response);

        static std::vector<std::string> fetch_detected_models(const std::string& endpoint,
            std::string& failure, bool passive = false,
            const std::vector<std::pair<std::string, std::string>>* suppliedHeaders = nullptr)
        {
            const std::string modelsEndpoint = normalize_model_list_endpoint(endpoint);
            try
            {
                // Passive scans receive an immutable, UI-thread-captured header
                // snapshot. Never read mutable provider settings from that worker.
                const auto headers = suppliedHeaders ? *suppliedHeaders
                    : model_request_headers(endpoint);
                const auto fetch = [&](const std::string& url) -> std::string
                {
#if defined(_WIN32)
                    return winhttp_get_json(url, headers, passive ? 2500 : 180000);
#elif defined(EPOCH_HAS_CURL)
                    return http_get_json(url, headers, passive ? 2500 : 180000);
#else
                    throw std::runtime_error("No HTTP transport is configured.");
#endif
                };
                const bool nativeRequested = endpoint.find("/api/v1") != std::string::npos;
                const auto identity = local_endpoint_identity(endpoint);
                if (nativeRequested || identity == kOriginalLocalEndpoint
                    || identity == "http://127.0.0.1:1234/v1/chat/completions"
                    || identity == "http://[::1]:1234/v1/chat/completions")
                {
                    const auto nativeEndpoint = modelsEndpoint.substr(0u,
                        modelsEndpoint.size() - std::string_view{"/v1/models"}.size())
                        + "/api/v1/models";
                    try
                    {
                        const std::string nativeResponse = fetch(nativeEndpoint);
                        auto keys = extract_native_model_keys(nativeResponse);
                        const auto capacities = extract_native_model_capacities(nativeResponse);
                        if (!passive)
                        {
                            const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
                            g_detectedModelCapacities = capacities;
                        }
                        return keys;
                    }
                    catch (const std::exception& error)
                    {
                        const std::string_view message{error.what()};
                        if (nativeRequested || (message.compare("Model endpoint returned HTTP status 404") != 0
                            && message.compare("Model endpoint returned HTTP status 405") != 0)) throw;
                    }
                }
                if (!passive)
                {
                    const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
                    g_detectedModelCapacities.clear();
                }
                return extract_openai_model_ids(fetch(modelsEndpoint));
            }
            catch (const std::exception& ex)
            {
                std::string msg = "Model detection failed: ";
                msg += ex.what();
                failure = msg;
                if (!passive)
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

        [[nodiscard]] static std::size_t extract_json_size_field_after(
            std::string_view sv,
            std::string_view field,
            std::size_t from,
            std::size_t before = std::string_view::npos) noexcept
        {
            const std::size_t key = sv.find(field, from);
            if (key == std::string_view::npos || (before != std::string_view::npos && key >= before))
                return 0u;
            const std::size_t colon = sv.find(':', key + field.size());
            if (colon == std::string_view::npos || (before != std::string_view::npos && colon >= before))
                return 0u;
            std::size_t pos = colon + 1u;
            while (pos < sv.size() && std::isspace(static_cast<unsigned char>(sv[pos])) != 0) ++pos;
            std::size_t value{};
            bool sawDigit{};
            constexpr std::size_t maximum = 16u * 1024u * 1024u;
            while (pos < sv.size() && sv[pos] >= '0' && sv[pos] <= '9')
            {
                sawDigit = true;
                const std::size_t digit = static_cast<std::size_t>(sv[pos] - '0');
                if (value > (maximum - digit) / 10u) return 0u;
                value = value * 10u + digit;
                ++pos;
            }
            return sawDigit ? value : 0u;
        }

        [[nodiscard]] static std::vector<DetectedModelCapacity>
            extract_native_model_capacities(std::string_view response)
        {
            std::vector<DetectedModelCapacity> capacities{};
            std::size_t pos{};
            while ((pos = response.find("\"key\"", pos)) != std::string_view::npos)
            {
                const std::string key = extract_json_string_field_after(response, "\"key\"", pos);
                if (key.empty()) { pos += 5u; continue; }
                const std::size_t next = response.find("\"key\"", pos + 5u);
                const std::size_t loaded = extract_json_size_field_after(
                    response, "\"context_length\"", pos, next);
                const std::size_t maximum = extract_json_size_field_after(
                    response, "\"max_context_length\"", pos, next);
                capacities.push_back(DetectedModelCapacity{
                    .model_id = key,
                    .loaded_context_tokens = loaded,
                    .maximum_context_tokens = maximum});
                if (next == std::string_view::npos) break;
                pos = next;
            }
            return capacities;
        }

        [[nodiscard]] static DetectedModelCapacity selected_model_capacity() noexcept
        {
            const std::lock_guard<std::recursive_mutex> stateLock{g_aiStateMutex};
            const auto found = std::ranges::find(
                g_detectedModelCapacities, g_selectedModel,
                &DetectedModelCapacity::model_id);
            return found == g_detectedModelCapacities.end()
                ? DetectedModelCapacity{} : *found;
        }

        [[nodiscard]] static std::size_t selected_model_context_capacity() noexcept
        {
            // Native Epoch AI is independent of the OpenAI-compatible route.
            // Read the *resident* server context, not the manifest default or
            // the requested tuning: the runtime can fall back during loading.
            if (native_runtime::selected())
            {
                const auto resident = native_runtime::snapshot();
                if (!resident.loaded) return 0u;
                for (const auto& entry : resident.models)
                    if (entry.active && entry.id.compare(resident.model) == 0)
                        return entry.loaded_context_tokens;
                return 0u; // Unknown, never fabricated from profile tuning.
            }
            if (g_localTransport != LocalInferenceTransport::OpenAiCompatible) return 0u;
            const auto capacity = selected_model_capacity();
            const auto endpoint = local_api_routes(g_selectedEndpoint).base_url;
            const auto declared = std::ranges::find_if(g_localContextOverrides, [&](const auto& value)
                { return value.endpoint == endpoint && value.model == g_selectedModel; });
            const auto detected = capacity.loaded_context_tokens != 0u
                ? capacity.loaded_context_tokens : capacity.maximum_context_tokens;
            if (declared == g_localContextOverrides.end()) return detected != 0u
                ? detected : inference_budget(InferenceWorkload::authoring).context_tokens;
            return detected != 0u ? (std::min)(declared->tokens, detected) : declared->tokens;
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
                || starts_with_text(lower, "local model returned assistant text, but")
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

#include "implementation/ai.model_protocol.inl"
        [[nodiscard]] static std::string model_system_prompt(
            bool directRuntime, InferenceWorkload workload,
            std::string_view input)
        {
            const bool projectScript = workload == InferenceWorkload::authoring
                && input.starts_with("Return one complete C++23 project script for review, using this exact envelope:\nEPOCH_PROJECT_SCRIPT_V1\n");
            std::string prompt = directRuntime
                ? "You are an operator-selected Epoch-local OS AI model invoked directly by Epoch through llama.cpp.\n"
                : "You are an operator-selected external OS AI model connected to Epoch through an operator-managed endpoint.\n";
            prompt +=
                "Epoch is a C++23 game engine, editor, renderer, and tooling host. Model compute may be Epoch-local or offloaded to an external machine; both use the same host-owned MCP authority, approval, and evidence boundaries. Epoch never trains or self-trains the selected model.\n"
                "Rules:\n"
                " - Stay grounded in the current visible Epoch editor/project context.\n";
            prompt += projectScript
                ? " - Current task: propose one complete active-project C++23 script in EPOCH_PROJECT_SCRIPT_V1. The host owns its destination and stages the source for review. Saving, compilation and native execution require the displayed operator action; never claim any of them happened from model output alone. Use only the documented EpochScriptHost callbacks for scene work.\n"
                : " - Project authoring may change only the active scene or GUI through validated semantic calls and explicit operator approval.\n";
            prompt +=
                " - Evidence review diagnoses existing output; it is not a source proposal and must not claim files were changed.\n"
                " - The operator's personal AI development is external to Epoch. Never access, modify, train from, or collect data from it.\n"
                " - Runtime exchanges, chat, scene edits, traces, and captures are operational evidence, never automatic training data.\n"
                " - MCP tool calls are bounded requests independent of model location; Epoch validates and executes them, then returns structured evidence.\n"
                " - Cite visible tool, build, scene, packet, log, or capture evidence before claiming a pass works.\n"
                " - Never create a server, listener, port bind, hidden control surface, or model bypass without explicit operator action.\n";
            bool conversational = true;
            if (conversational)
            {
                prompt += projectScript
                    ? " - Return only the complete script review envelope requested by the host, without Markdown fences or drafting notes.\n"
                    : " - Follow the requested answer format. Return only the final result in assistant content, without drafting notes. Project authoring returns the requested validated semantic-call packet, not a prose substitute.\n";
                return prompt;
            }
            return prompt;
        }

        [[nodiscard]] std::string candidate_image_base64(std::string_view bytes);

        [[nodiscard]] static std::string openai_chat_request_body(
            std::string_view model,
            std::string_view systemPrompt,
            std::string_view input,
            std::size_t maximumTokens,
            bool recoveryRequest,
            bool structuredSource,
            bool streamReplies = true,
            std::string_view epochHostStage = {},
            std::span<const ReferenceImage> images = {})
        {
            std::string requestInput{input};
            std::string body;
            body.reserve(1'024u + systemPrompt.size() + requestInput.size());
            body += "{";
            body += "\"model\":\"" + json_escape(model) + "\",";
            body += "\"messages\":[";
            const auto requestedThinking = resolved_thinking_mode(model);
            const std::string thinkingDirective = requestedThinking == thinking::Mode::low
                ? "\nReasoning budget preference: low. Produce the required result directly; do not repeatedly recite the task or reason beyond the action needed. Preserve correctness, safety, tool and build gates."
                : requestedThinking == thinking::Mode::off
                ? "\nThinking preference: off. Return completed final content or the exact required tool call directly; do not expose or prolong private analysis. Preserve all verification gates."
                : std::string{};
            body += "{\"role\":\"system\",\"content\":\""
                + json_escape(std::string{systemPrompt} + thinkingDirective) + "\"},";
            if (images.empty())
            {
            body += "{\"role\":\"user\",\"content\":\""
                + json_escape(requestInput) + "\"}";
            }
            else
            {
                if (images.size() > 4u) throw std::invalid_argument("Attach up to four images per request.");
                std::size_t imageBytes{};
                body += "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"" + json_escape(requestInput) + "\"}";
                for (const auto& image : images)
                {
                    imageBytes += image.bytes.size();
                    if (image.bytes.empty() || imageBytes > 4u * 1024u * 1024u
                        || (image.mime_type != "image/png" && image.mime_type != "image/jpeg" && image.mime_type != "image/webp"))
                        throw std::invalid_argument("Image attachments must be PNG, JPEG or WebP and total at most 4 MiB.");
                    body += ",{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:"
                        + image.mime_type + ";base64," + candidate_image_base64(image.bytes) + "\"}}";
                }
                body += "]}";
            }
            body += "],";
            // Response shape and model reasoning are separate capabilities.
            // Source actions use function calling because the model is selecting
            // a host action, not formatting a user-facing answer. This removes
            // impossible mixed states such as action=context plus edit operations.
            // Qwen's template only supports on/off. Low must not silently run
            // unbounded native reasoning, especially in source-edit requests.
            // For non-Qwen providers this extension is intentionally absent.
            const auto thinkingMode = resolved_thinking_mode(model);
            if (thinking::uses_qwen_template_flag(model))
            {
                body += "\"chat_template_kwargs\":{\"enable_thinking\":";
                body += thinking::template_thinking_enabled(thinkingMode)
                    ? "true}," : "false},";
            }
            body += "\"max_tokens\":" + std::to_string(maximumTokens) + ",";
            // Request the optional terminal usage frame from compatible
            // OpenAI-style runtimes. Never estimate tokens from SSE bytes.
            body += streamReplies
                ? "\"stream\":true,\"stream_options\":{\"include_usage\":true}"
                : "\"stream\":false";
            // This routing hint is authored by the Epoch host, never by the
            // model or by untrusted source-text markers in the message body.
            if (!epochHostStage.empty())
                body += ",\"epoch_host_stage\":\"" + json_escape(epochHostStage) + "\"";
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
            bool structuredSource,
            std::stop_token cancellation,
            ModelTerminalFailure& terminalFailure,
            const ModelRequestObserver& observer = {},
            bool streamReplies = true,
            const ModelProgressObserver& progressObserver = {},
            std::string_view workloadPhase = {},
            std::span<const ReferenceImage> images = {})
        {
            terminalFailure = ModelTerminalFailure::none;
            const auto cancelled = [&]() -> std::string
            {
                terminalFailure = ModelTerminalFailure::cancelled;
                return std::string{kModelRequestCancelled};
            };
            std::string phase = workloadPhase.empty() ? "conversation" : std::string{workloadPhase};
            std::string replyDiagnostic;
            ModelRequestProgress completedProgress{};
            // The deadline belongs to the logical request, not each attempt.
            // A retry must never silently restart the whole stage allowance.
            const auto requestDeadline = std::chrono::steady_clock::now()
                + std::chrono::seconds{timeoutSeconds};
            std::uint32_t attemptTimeoutSeconds = timeoutSeconds;
            const auto request_once = [&](bool recoveryRequest, std::string* rawResponse) -> std::string
            {
                replyDiagnostic.clear();
                if (cancellation.stop_requested())
                    return cancelled();
                const std::string body = openai_chat_request_body(
                    model, system_prompt, input, maximumTokens, recoveryRequest,
                    structuredSource, streamReplies, {}, images);
                bool preserveSourcePacket = false;
                ModelReplyStream stream{preserveSourcePacket};
                ModelRequestProgress progress{
                    .phase = phase,
                    .attempt = recoveryRequest ? 2u : 1u,
                    .streaming_requested = streamReplies,
                    .prompt_bytes = input.size(),
                    .timeout_seconds = attemptTimeoutSeconds};
                completedProgress = progress;
                notify_model_progress(progressObserver, progress);
                auto lastProgressLog = std::chrono::steady_clock::time_point{};
                std::optional<std::chrono::steady_clock::time_point> firstPayloadAt;
                const ModelReplyChunkConsumer consumeChunk = [&](std::string_view chunk)
                {
                    const auto priorPayload = progress.content_bytes
                        + progress.tool_argument_bytes + progress.reasoning_bytes;
                    stream.feed(chunk);
                    stream.update_progress(progress);
                    if (!firstPayloadAt && progress.content_bytes
                        + progress.tool_argument_bytes + progress.reasoning_bytes > priorPayload)
                        firstPayloadAt = std::chrono::steady_clock::now();
                    completedProgress = progress;
                    notify_model_progress(progressObserver, progress);
                    const auto observedAt = std::chrono::steady_clock::now();
                    if (lastProgressLog.time_since_epoch().count() == 0
                        || observedAt - lastProgressLog >= std::chrono::seconds{30})
                    {
                        lastProgressLog = observedAt;
                        logger::get("Engine.AI.Transport").log(logger::LogLevel::INFO,
                            "Local model activity; stage=" + phase
                            + "; attempt=" + std::to_string(progress.attempt)
                            + "; response_bytes=" + std::to_string(progress.response_bytes)
                            + "; stream_events=" + std::to_string(progress.response_events)
                            + "; content_bytes=" + std::to_string(progress.content_bytes)
                            + "; tool_argument_bytes=" + std::to_string(progress.tool_argument_bytes)
                            + "; reasoning_bytes=" + std::to_string(progress.reasoning_bytes));
                    }
                };
                const std::string wireResponse = run_model_transport_attempt(cancellation, [&]() -> std::string
                {
#if defined(_WIN32)
                    return winhttp_post_json(
                        endpoint_full, body, headers, attemptTimeoutSeconds, cancellation, observer, consumeChunk);
#elif defined(EPOCH_HAS_CURL)
                    return http_post_json(
                        endpoint_full, body, headers, attemptTimeoutSeconds, cancellation, observer, consumeChunk);
#else
                    (void)endpoint_full;
                    (void)headers;
                    core::log::error("ai", "Local OpenAI-compatible request failed: no non-Windows HTTP transport is configured (build with libcurl).");
                    return {};
#endif
                });
                if (cancellation.stop_requested())
                    return cancelled();
                const std::string resp = stream.finish(wireResponse);
                stream.update_progress(progress);
                if (!progress.provider_generation_timing && firstPayloadAt)
                    progress.generation_ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - *firstPayloadAt).count();
                completedProgress = progress;
                notify_model_progress(progressObserver, progress);
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
                    return parsed;
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

                    std::string warn = "Local OpenAI-compatible reply body could not be decoded. bytes=";
                    warn += std::to_string(resp.size());
                    warn += "; response contents were not copied into the transport log.";
                    core::log::warn("ai", epochengine::string_view{warn.data(), warn.size()});
                }

                return {};
            };

            std::string lastFailure{};
            // Interactive project work is one operator-owned generation. An
            // empty/reasoning-only completion must not silently start another
            // expensive inference. Source recovery retains its separate policy.
            const std::size_t maximumAttempts = model_attempt_limit(phase);
            for (std::size_t attempt = 0u; attempt < maximumAttempts; ++attempt)
            {
                if (cancellation.stop_requested())
                    return cancelled();
                const auto now = std::chrono::steady_clock::now();
                const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    requestDeadline - now).count();
                const auto remaining = remainingMs <= 0 ? 0
                    : (remainingMs + 999) / 1'000;
                // If the original request consumed its time budget, there is
                // no safe or useful second generation to start. A transient
                // early failure may be replayed once within the SAME deadline.
                if (remaining < (attempt == 0u ? 1 : 30))
                {
                    terminalFailure = ModelTerminalFailure::total_timeout;
                    lastFailure += " Shared request deadline exhausted; no further model attempt was dispatched.";
                    break;
                }
                attemptTimeoutSeconds = static_cast<std::uint32_t>(remaining);
                const auto attemptStarted = std::chrono::steady_clock::now();
                ModelAttemptFailure failure{ModelAttemptFailure::recoverable};
                const auto startedMessage = "Local model transport attempt "
                    + std::to_string(attempt + 1u) + "/" + std::to_string(maximumAttempts) + " started; per-attempt wall budget "
                    + std::to_string(attemptTimeoutSeconds)
                    + "s (shared request deadline); stage=" + phase
                    + "; prompt_bytes=" + std::to_string(input.size())
                    + "; output_token_limit=" + std::to_string(maximumTokens)
                    + ". Window focus does not control this worker; Stop remains available.";
                core::log::info("ai", epochengine::string_view{startedMessage.data(), startedMessage.size()});
                logger::get("Engine.AI.Transport").log(logger::LogLevel::INFO, startedMessage);
                try
                {
                    std::string rawResponse{};
                    std::string reply = request_once(
                        attempt > 0u, &rawResponse);
                    const auto receivedMessage = "Local model transport returned; stage="
                        + phase
                        + "; attempt=" + std::to_string(attempt + 1u)
                        + "; elapsed_ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - attemptStarted).count())
                        + "; response_bytes=" + std::to_string(rawResponse.size())
                        + "; visible_reply_bytes=" + std::to_string(reply.size())
                        + "; stream_events=" + std::to_string(completedProgress.response_events)
                        + "; content_bytes=" + std::to_string(completedProgress.content_bytes)
                        + "; tool_argument_bytes=" + std::to_string(completedProgress.tool_argument_bytes)
                        + "; reasoning_bytes=" + std::to_string(completedProgress.reasoning_bytes)
                        + "; reply_kind=" + std::string{model_reply_kind(reply)};
                    core::log::info("ai", epochengine::string_view{receivedMessage.data(), receivedMessage.size()});
                    logger::get("Engine.AI.Transport").log(logger::LogLevel::INFO, receivedMessage);
                    if (cancellation.stop_requested())
                        return cancelled();
                    // Schema/content failures belong to the host's single
                    // correction budget, not an additional transport retry.
                    bool visibleReply = model_reply_has_visible_content(reply);
                    if (visibleReply)
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
                            terminalFailure = ModelTerminalFailure::cancelled;
                            return lastFailure;
                        }
                    }
                    else
                    {
                        if (!reply.empty())
                            return reply;
                        lastFailure =
                            !replyDiagnostic.empty() ? replyDiagnostic
                            : (completedProgress.reasoning_bytes > 0u || has_hidden_reasoning_without_visible_content(rawResponse))
                            ? std::string{
                                "Local model finished without a usable final answer (reasoning is not an action). No project change was applied. Use Retry on your message to request another generation."}
                            : std::string{
                                "Local model completed the request without decodable assistant text. No project change was applied. Use Retry on your message; check the provider's final-output and token-budget settings."};
                    }
                }
                catch (const ModelOutputLimit& limit)
                {
                    if (cancellation.stop_requested()) return cancelled();
                    terminalFailure = ModelTerminalFailure::request_rejected;
                    lastFailure = "Local OpenAI-compatible request failed: " + std::string{limit.what()}
                        + " Epoch requested " + std::to_string(maximumTokens) + " output tokens.";
                    logger::get("Engine.AI.Transport").log(logger::LogLevel::Error, lastFailure);
                    return lastFailure;
                }
                catch (const ModelTransportRetirementFailure&)
                {
                    // A failed close is not an ordinary HTTP failure. Starting
                    // another request would overlap an unretired operation.
                    // Preserve the typed failure through cancellation checks.
                    throw;
                }
                catch (const ModelHttpFailure& http)
                {
                    if (cancellation.stop_requested())
                        return cancelled();
                    lastFailure = "Local OpenAI-compatible request failed: " + http.diagnostic();
                    logger::get("Engine.AI.Transport").log(logger::LogLevel::Error, lastFailure);
                    if (http.provider_timeout || http.rejected)
                    {
                        terminalFailure = http.provider_timeout
                            ? ModelTerminalFailure::provider_timeout
                            : ModelTerminalFailure::request_rejected;
                        core::log::error("ai", epochengine::string_view{lastFailure.data(), lastFailure.size()});
                        return lastFailure;
                    }
                }
                catch (const ModelTransportTimeout& timeout)
                {
                    if (cancellation.stop_requested())
                        return cancelled();
                    failure = timeout.total_budget ? ModelAttemptFailure::total_timeout
                        : ModelAttemptFailure::operation_timeout;
                    if (timeout.total_budget)
                        terminalFailure = ModelTerminalFailure::total_timeout;
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - attemptStarted).count();
                    lastFailure = model_timeout_message(attempt,
                        static_cast<std::uint64_t>((std::max)(elapsed, decltype(elapsed){0})),
                        attemptTimeoutSeconds, timeout.total_budget, timeout.what(), maximumAttempts);
                    core::log::warn("ai", epochengine::string_view{lastFailure.data(), lastFailure.size()});
                    logger::get("Engine.AI.Transport").log(logger::LogLevel::Error, lastFailure);
                }
                catch (const std::exception& ex)
                {
                    if (cancellation.stop_requested())
                        return cancelled();
                    lastFailure =
                        "Local OpenAI-compatible request failed: ";
                    lastFailure += ex.what();
                    logger::get("Engine.AI.Transport").log(logger::LogLevel::Error, lastFailure);
                    if (contains_text(
                            lowercase_ascii(lastFailure), "cancelled"))
                    {
                        terminalFailure = ModelTerminalFailure::cancelled;
                        core::log::warn(
                            "ai",
                            epochengine::string_view{
                                lastFailure.data(), lastFailure.size()});
                        return lastFailure;
                    }
                }

                if (cancellation.stop_requested())
                    return cancelled();
                const bool generationStarted = completedProgress.reasoning_bytes != 0u
                    || completedProgress.content_bytes != 0u || completedProgress.tool_argument_bytes != 0u;
                if (generationStarted)
                {
                    lastFailure += " Generation had already started; Epoch did not restart the expensive request automatically. No partial action was applied.";
                    logger::get("Engine.AI.Transport").log(logger::LogLevel::Error, lastFailure);
                }
                if (attempt + 1u >= maximumAttempts || !retry_model_attempt(attempt, failure, false, generationStarted))
                    break;
                if (attempt == 0u)
                {
                    core::log::warn(
                        "ai",
                        "Local source-model request produced no usable response; retrying once with an explicit stage contract.");
                }
            }
            core::log::error(
                "ai",
                epochengine::string_view{
                    lastFailure.data(), lastFailure.size()});
            // Exhausted connection/decoding failures are host outcomes, not
            // assistant prose to feed into authoring or source parsers.
            if (terminalFailure == ModelTerminalFailure::none)
                terminalFailure = ModelTerminalFailure::request_failed;
            return lastFailure;
        }

#include "implementation/ai.runtime_process.inl"
        [[nodiscard]] bool model_reply_is_failure(std::string_view text)
        {
            const std::string lower = lowercase_ascii(std::string{text});
            for (const std::string_view prefix : {
                "local model api error", "local openai-compatible request failed",
                "local-model request cancelled", "local model returned hidden reasoning",
                "local model returned reasoning", "local model returned no decodable assistant text",
                "local model returned assistant text, but",
                "direct llama.cpp inference could not", "direct llama.cpp inference exceeded",
                "direct llama.cpp inference was cancelled", "direct llama.cpp inference failed",
                "direct llama.cpp inference returned no parseable"})
            {
                if (starts_with_text(lower, prefix))
                    return true;
            }
            return false;
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
        InferenceWorkload workload,
        std::stop_token cancellation,
        ModelRequestObserver observer,
        ModelProgressObserver progress_observer,
        std::span<const ReferenceImage> images)
    {
        ScopedModelObservation observation{observer, cancellation};
        EngineAiReply out{};
        if (cancellation.stop_requested())
        {
            out.text = kModelRequestCancelled;
            out.terminal_failure = ModelTerminalFailure::cancelled;
            return out;
        }
        const InferenceBudget budget = request_inference_budget(workload, user_input);
        if (!budget.valid() || user_input.empty()
            || user_input.size() > budget.maximum_prompt_bytes)
        {
            core::log::warn(
                "ai",
                "Local-model request rejected because its workload budget or prompt size is invalid.");
            return out;
        }

        if (!images.empty() && m_cfg.backend == "llama_cpp_cli")
        {
            out.text = "Image input requires an image-capable Native or Local API model. Direct CLI image transport is not configured.";
            out.terminal_failure = ModelTerminalFailure::request_rejected;
            return out;
        }
        Config effective = m_cfg;
        effective.context_tokens = budget.context_tokens;
        effective.output_tokens = budget.output_tokens;
        effective.timeout_seconds = budget.timeout_seconds;
        const std::string sys = model_system_prompt(
            effective.backend == "llama_cpp_cli", workload, user_input);

        bool sourcePacket = false;
        bool structuredSource = false;
        const std::size_t n = std::max<std::size_t>(1, effective.best_of);

        // No scorer is active in this lane; first non-empty assistant content wins.
        try
        {
        for (std::size_t i = 0; i < n; ++i)
        {
            if (cancellation.stop_requested())
            {
                out.text = kModelRequestCancelled;
                out.alternatives.clear();
                out.terminal_failure = ModelTerminalFailure::cancelled;
                return out;
            }
            std::string txt = effective.backend == "llama_cpp_cli"
                ? llama_cpp_complete(
                    effective, sys, user_input,
                    sourcePacket, cancellation,
                    out.terminal_failure, observer)
                : openai_chat_complete(
                    m_endpoint_full,
                    effective.model,
                    sys,
                    user_input,
                    model_request_headers(m_endpoint_full),
                    effective.output_tokens,
                    effective.timeout_seconds,
                    structuredSource, cancellation,
                    out.terminal_failure, observer, effective.stream_replies, progress_observer,
                    workload == InferenceWorkload::authoring ? "project authoring/tool plan" : "", images);

            if (cancellation.stop_requested())
            {
                out.text = kModelRequestCancelled;
                out.alternatives.clear();
                out.terminal_failure = ModelTerminalFailure::cancelled;
                return out;
            }
            if (out.terminal_failure != ModelTerminalFailure::none)
            {
                // Host transport outcomes are not assistant alternatives or
                // source packets. Preserve their type through every UI layer.
                out.text = std::move(txt);
                out.alternatives.clear();
                return out;
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

        if (cancellation.stop_requested())
        {
            out.text = kModelRequestCancelled;
            out.alternatives.clear();
            out.terminal_failure = ModelTerminalFailure::cancelled;
        }
        }
        catch (const ModelTransportRetirementFailure& exception)
        {
            out.text = std::string{"Local model transport retirement failed: "} + exception.what();
            out.alternatives.clear();
            out.transport_retirement_failed = true;
            out.terminal_failure = ModelTerminalFailure::retirement_failed;
            observation.retirement_failed = true;
            core::log::error("ai", epochengine::string_view{out.text.data(), out.text.size()});
            return out;
        }
        bool visibleReply = model_reply_has_visible_content(out.text);
        observation.succeeded = !out.text.empty() && !model_reply_is_failure(out.text)
            && visibleReply;
        return out;
    }

#include "implementation/ai.model_management.inl"
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

    bool valid_project_context_text(std::string_view text) noexcept
    {
        return text.find('\0') == std::string_view::npos
            && epochengine::core::utf8::valid(text);
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

#include "implementation/ai.transport_contracts.inl"
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
        InferenceWorkload workload,
        std::stop_token cancellation,
        ModelRequestObserver observer,
        ModelTerminalFailure* terminal_failure,
        ModelProgressObserver progress_observer,
        std::span<const ReferenceImage> images)
    {
        if (terminal_failure)
            *terminal_failure = ModelTerminalFailure::none;
        const auto cancelled = [&]() -> std::string
        {
            if (terminal_failure)
                *terminal_failure = ModelTerminalFailure::cancelled;
            return std::string{kModelRequestCancelled};
        };
        if (cancellation.stop_requested())
        {
            notify_model_stage(observer, ModelRequestStage::cancelled);
            return cancelled();
        }
        // Capture the global epoch before either service lock. Every queued
        // request retains that epoch through all attempts; only a later public
        // request may acquire the next global epoch after shutdown/cancel.
        RequestCancellation requestStop{cancellation, g_aiCancellation.capture()};
        cancellation = requestStop.token();
        ScopedModelObservation observation{observer, cancellation};
        const InferenceBudget budget = request_inference_budget(workload, user_text);
        if (!budget.valid())
            return "The selected local-model workload has an invalid host budget.";
        if (user_text.empty() || user_text.size() > budget.maximum_prompt_bytes)
        {
            return "The local-model request is empty or exceeds its bounded host prompt budget.";
        }
        notify_model_stage(observer, ModelRequestStage::queued);
        // Controlled Epoch workloads use one native completion. The existing
        // host owns source tools, scene/script approval, builds and repair.
        if (native_runtime::selected())
        {
            notify_model_stage(observer, ModelRequestStage::sending);
            bool hostControlled = true;
            bool structuredSource = false;
            ModelRequestProgress progress{.phase="Local model request",.attempt=1u,
                .streaming_requested=true,.prompt_bytes=user_text.size(),
                .timeout_seconds=budget.timeout_seconds};
            const ScopedModelRequest requestPermit{g_aiRequestGate,cancellation};
            if(!requestPermit.acquired()||cancellation.stop_requested())return cancelled();
            notify_model_progress(progress_observer,progress);
            std::chrono::steady_clock::time_point lastProgressUpdate{};
            std::optional<std::chrono::steady_clock::time_point> firstNativePayloadAt;
            bool receivingNotified = false;
            std::string_view nativeHostStage{};
            const auto result =
                native_runtime::complete_host_request(openai_chat_request_body(
                    active_model_name(),model_system_prompt(false,workload,user_text),
                    user_text,budget.output_tokens,false,structuredSource,true,
                    nativeHostStage, images),
                    budget.timeout_seconds,cancellation,
                    [&](native_runtime::OutputKind kind,std::string_view delta) {
                        ++progress.response_events;
                        progress.response_bytes+=delta.size();
                        if (!delta.empty() && !firstNativePayloadAt)
                            firstNativePayloadAt = std::chrono::steady_clock::now();
                        if(kind==native_runtime::OutputKind::reasoning) {
                            progress.reasoning_bytes+=delta.size();
                        } else if(kind==native_runtime::OutputKind::text) {
                            progress.content_bytes+=delta.size();
                        } else if(kind==native_runtime::OutputKind::tool_arguments) {
                            progress.tool_argument_bytes+=delta.size();
                        }
                        if (!receivingNotified)
                        {
                            notify_model_stage(observer, ModelRequestStage::receiving);
                            receivingNotified = true;
                        }
                        const auto now = std::chrono::steady_clock::now();
                        if (lastProgressUpdate == std::chrono::steady_clock::time_point{}
                            || now - lastProgressUpdate >= std::chrono::milliseconds(100))
                        {
                            lastProgressUpdate = now;
                            notify_model_progress(progress_observer, progress);
                        }
                    })
                ;
            if (hostControlled && result.ok)
            {
                // The host completion can include actual provider usage.
                // If not, report the rate as unavailable rather than borrowing
                // a historical model-wide mean or counting UTF-8 bytes.
                // This module already has a bounded JSON reader. Avoid importing
                // a runtime-internal JSON alias or relying on implicit string
                // conversions, both of which fail under the MSVC shared-items build.
                // Usage is supplemental: malformed metadata must never invalidate
                // an otherwise successful coding response.
                const auto reported_unsigned = [](std::string_view encoded)
                    -> std::optional<std::uint64_t>
                {
                    std::uint64_t value{};
                    const auto [end, error] = std::from_chars(
                        encoded.data(), encoded.data() + encoded.size(), value);
                    if (error == std::errc{} && end == encoded.data() + encoded.size())
                        return value;
                    return std::nullopt;
                };
                try
                {
                    const auto fields = model_json_fields(result.message);
                    const auto usageObject = model_json_field(fields, "usage");
                    progress.input_image_tokens = reported_input_image_tokens(usageObject);
                    if (!usageObject.empty() && usageObject.compare("null") != 0)
                    {
                        const auto usage = model_json_fields(usageObject);
                        for (const auto* key : {"completion_tokens", "output_tokens"})
                        {
                            const auto count = reported_unsigned(model_json_field(usage, key));
                            if (!count) continue;
                            progress.completion_tokens = *count;
                            progress.token_usage_reported = true;
                            break;
                        }
                    }
                    const auto timingsObject = model_json_field(fields, "timings");
                    if (!timingsObject.empty() && timingsObject.compare("null") != 0)
                    {
                        const auto timings = model_json_fields(timingsObject);
                        const auto count = reported_unsigned(model_json_field(timings, "predicted_n"));
                        if (!progress.token_usage_reported && count)
                        {
                            progress.completion_tokens = *count;
                            progress.token_usage_reported = true;
                        }
                        const auto duration = model_json_field(timings, "predicted_ms");
                        if (!duration.empty())
                        {
                            double parsed{};
                            const auto [end, error] = std::from_chars(
                                duration.data(), duration.data() + duration.size(), parsed);
                            if (error == std::errc{} && end == duration.data() + duration.size()
                                && parsed > 0.0 && parsed < std::numeric_limits<double>::infinity())
                                {
                                    progress.generation_ms = parsed;
                                    progress.provider_generation_timing = true;
                                }
                        }
                    }
                }
                catch (const std::exception&)
                {
                    // A model provider may omit or return malformed optional
                    // statistics. The response remains subject to existing
                    // source/permission/validation gates, independently of TPS.
                }
                if (progress.generation_ms <= 0.0 && firstNativePayloadAt)
                    progress.generation_ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - *firstNativePayloadAt).count();
            }
            if (result.generation_tps_known)
            {
                progress.completion_tokens = result.completion_tokens;
                progress.measured_tps = result.generation_tps;
                progress.tps_known = true;
                if (!progress.token_usage_reported && result.generation_tps > 0.0)
                {
                    progress.token_usage_reported = true;
                    progress.generation_ms = 1000.0 * result.completion_tokens / result.generation_tps;
                }
            }
            notify_model_progress(progress_observer, progress);
            if (cancellation.stop_requested()) return cancelled();
            if (!result.ok)
            {
                const bool deadlineExpired = result.message.find("deadline") != std::string::npos
                    || result.message.find("timed out") != std::string::npos
                    || result.message.find("Timeout") != std::string::npos;
                if (terminal_failure) *terminal_failure = deadlineExpired
                    ? ModelTerminalFailure::total_timeout
                    : ModelTerminalFailure::request_failed;
                return "Local Models: " + result.message;
            }
            if(hostControlled) {
                observation.succeeded=true;
                return normalize_assistant_text(extract_openai_choice_message_content(result.message));
            }
            observation.succeeded = true;
            return result.message;
        }

        std::shared_ptr<EngineAiModel> client;
        std::string selectedModel;
        std::string selectedEndpoint;
        {
            std::unique_lock<std::recursive_mutex> stateLock{g_aiStateMutex, std::defer_lock};
            while (!stateLock.try_lock())
            {
                if (cancellation.stop_requested())
                    return cancelled();
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
            }
            if (cancellation.stop_requested())
                return cancelled();
            if (!prepare_local_model_for_request()
                || !g_modelUseConfirmedForSession || g_selectedModel.empty())
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
            const ScopedModelRequest requestPermit{g_aiRequestGate, cancellation};
            if (!requestPermit.acquired() || cancellation.stop_requested())
                return cancelled();
            reply = client->submit(user_text, workload, cancellation,
                [&](ModelRequestStage stage)
                {
                    // The service owns its terminal notification, after the
                    // serial permit is released and reply checks have finished.
                    if (stage != ModelRequestStage::completed
                        && stage != ModelRequestStage::cancelled
                        && stage != ModelRequestStage::retirement_failed
                        && stage != ModelRequestStage::failed)
                        notify_model_stage(observer, stage);
                }, progress_observer, images);
        }
        if (terminal_failure)
            *terminal_failure = reply.terminal_failure;
        if (reply.transport_retirement_failed)
        {
            observation.retirement_failed = true;
            return reply.text;
        }
        if (cancellation.stop_requested())
            return cancelled();
        if (reply.terminal_failure != ModelTerminalFailure::none)
            return reply.text;
        if (model_reply_is_failure(reply.text))
        {
            return reply.text;
        }
        bool visibleReply = model_reply_has_visible_content(reply.text);
        if (!reply.text.empty() && !visibleReply)
        {
            core::log::warn("ai", "Local model returned non-promotable assistant content.");
            return "Local model returned reasoning/debug text instead of final assistant content. Adjust the local model chat template or choose a content-producing OS model before using Engine AI chat.";
        }
        if (reply.text.empty())
            return std::string("No decodable reply from selected local model '") + selectedModel
                + "' at " + selectedEndpoint
                + ". Check the endpoint/model selection and retry.";
        observation.succeeded = true;
        return reply.text;
    }
}
