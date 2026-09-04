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
 /**************************************************************
 *   Epoch Engine - AI Integration (Local OpenAI-Compatible)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/core.stl_types.hpp"

#if defined(_WIN32)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   include <winhttp.h>
#   pragma comment(lib, "winhttp.lib")
#endif

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <utility>

export module ai.engine;

export import ai.runtime;
export import ai.session;
export import ai.voice_session;
export import ai.mcp;
export import ai.eval;

import core.log;

export namespace epochengine::ai
{
    enum class LocalInferenceTransport : std::uint8_t
    {
        OpenAiCompatible,
        LlamaCppCli
    };

    enum class InferenceWorkload : std::uint8_t
    {
        chat,
        authoring,
        source_iteration,
        source_self_review
    };

    struct InferenceBudget final
    {
        std::size_t context_tokens{};
        std::size_t output_tokens{};
        std::size_t maximum_prompt_bytes{};
        std::size_t maximum_reply_bytes{};
        std::uint32_t timeout_seconds{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return context_tokens >= 4'096u
                && output_tokens > 0u
                && output_tokens < context_tokens
                && maximum_prompt_bytes > 0u
                && maximum_reply_bytes > 0u
                && timeout_seconds > 0u;
        }
    };

    [[nodiscard]] constexpr InferenceBudget inference_budget(
        InferenceWorkload workload) noexcept
    {
        switch (workload)
        {
        case InferenceWorkload::chat:
            return {16'384u, 2'048u, 64u * 1024u, 64u * 1024u, 120u};
        case InferenceWorkload::authoring:
            return {32'768u, 4'096u, 128u * 1024u, 128u * 1024u, 180u};
        case InferenceWorkload::source_iteration:
            return {65'536u, 32'768u, 256u * 1024u, 1024u * 1024u, 600u};
        case InferenceWorkload::source_self_review:
            return {65'536u, 8'192u, 256u * 1024u, 256u * 1024u, 300u};
        }
        return {};
    }

    struct DirectRuntimeStatus
    {
        std::string executable{};
        std::string model{};
        std::string message{};
        bool executable_ready{};
        bool model_ready{};

        [[nodiscard]] bool ready() const noexcept
        {
            return executable_ready && model_ready;
        }
    };

    inline constexpr std::string_view kEpochLocalQwen38ModelPackageId =
        "os_model_qwen_3_8_27b";
    inline constexpr std::string_view kEpochLocalLlamaCppRuntimePackageId =
        "local_ai_llama_cpp_runtime";
    inline constexpr std::string_view kEpochLocalLlamaCppRelease = "b10516";
    inline constexpr std::string_view kEpochLocalLlamaCppRevision =
        "b95502ba9aa0eb73a2f4fc8878d7fbe6a847a0b9";
    inline constexpr std::string_view kEpochLocalQwen38ModelRevision =
        "4ca720788d1e01f1bff70c033e0d0028fd02e502";
    inline constexpr std::string_view kEpochLocalQwen38ModelFile =
        "Qwen3.8-27B-UD-Q4_K_M.gguf";
    inline constexpr std::uint64_t kEpochLocalQwen38ModelBytes =
        16'464'440'224ull;
    inline constexpr std::string_view kEpochLocalQwen38ModelSha256 =
        "322e194ff79741c7baa497c240f677f54b201b0efab44ca8e50f122b39123482";
    inline constexpr std::string_view kEpochLocalQwen38ModelUrl =
        "https://huggingface.co/unsloth/Qwen3.8-27B-GGUF/resolve/"
        "4ca720788d1e01f1bff70c033e0d0028fd02e502/"
        "Qwen3.8-27B-UD-Q4_K_M.gguf?download=true";
#if defined(_WIN32)
    inline constexpr std::string_view kEpochLocalLlamaCppArtifact =
        "llama-b10516-bin-win-vulkan-x64.zip";
    inline constexpr std::string_view kEpochLocalLlamaCppArtifactSha256 =
        "530f57d2a874ce017827c1e5a926812b9d5de4667248575d1372b1c0acf94d83";
#else
    inline constexpr std::string_view kEpochLocalLlamaCppArtifact =
        "llama-b10516-bin-ubuntu-vulkan-x64.tar.gz";
    inline constexpr std::string_view kEpochLocalLlamaCppArtifactSha256 =
        "5ce186720f43c415465869b0cd93973b828b219cbf6fbcc22aa899531973c505";
#endif

    struct EpochLocalAiInstallStatus final
    {
        std::string runtime_root{};
        std::string runtime_executable{};
        std::string model_root{};
        std::string model_file{};
        std::string message{};
        bool runtime_receipt_ready{};
        bool runtime_executable_ready{};
        bool model_receipt_ready{};
        bool model_file_ready{};

        [[nodiscard]] bool ready() const noexcept
        {
            return runtime_receipt_ready && runtime_executable_ready
                && model_receipt_ready && model_file_ready;
        }
    };
    struct Candidate
    {
        std::string text;
        double score = 0.0;
    };

    struct EngineAiReply
    {
        std::string text;
        double score = 0.0;
        std::vector<Candidate> alternatives;
    };

    class EngineAiModel
    {
    public:
        struct Config
        {
            std::string backend = "openai_chat";
            std::string endpoint = "http://localhost:1234"; // base or full
            std::string model{};
            std::string executable{};
            std::size_t threads{};
            std::size_t context_tokens = 4096;
            std::size_t output_tokens = 512;
            int gpu_layers = -1;
            std::uint32_t timeout_seconds = 120;
            std::size_t best_of = 1;
        };

        explicit EngineAiModel(Config cfg);
        [[nodiscard]] EngineAiReply submit(
            std::string_view user_input,
            InferenceWorkload workload = InferenceWorkload::chat);

    private:
        Config m_cfg{};
        std::string m_endpoint_full{}; // normalized to /v1/chat/completions
    };

    // Engine-global service wrapper (simple singleton)
    void init_engine_ai();
    void shutdown_engine_ai();
    void cancel_engine_ai_request() noexcept;

    [[nodiscard]] std::string send_to_engine_ai(
        const std::string& user_text,
        InferenceWorkload workload = InferenceWorkload::chat);
    [[nodiscard]] std::string default_workspace_root();
    [[nodiscard]] std::string review_fixtures_root();
    [[nodiscard]] std::string evals_root();
    [[nodiscard]] std::string prompts_root();
    [[nodiscard]] std::string manifests_root();
    [[nodiscard]] std::string local_model_exchange_jsonl_path();
    [[nodiscard]] std::string local_tool_trace_jsonl_path();
    [[nodiscard]] std::string local_session_root();
    [[nodiscard]] std::string local_model_root();
    [[nodiscard]] std::string local_cache_root();
    [[nodiscard]] std::string epoch_local_llama_cpp_root();
    [[nodiscard]] std::string epoch_local_llama_cpp_executable();
    [[nodiscard]] std::string epoch_local_qwen38_root();
    [[nodiscard]] std::string epoch_local_qwen38_model();
    [[nodiscard]] EpochLocalAiInstallStatus epoch_local_ai_install_status();
    [[nodiscard]] bool epoch_local_ai_install_contract() noexcept;
    [[nodiscard]] ProviderMode current_provider_mode() noexcept;
    [[nodiscard]] LocalInferenceTransport current_local_inference_transport() noexcept;
    [[nodiscard]] std::string_view local_inference_transport_name(LocalInferenceTransport transport) noexcept;
    [[nodiscard]] DirectRuntimeStatus direct_runtime_status();
    [[nodiscard]] DirectRuntimeStatus discover_direct_runtime();
    [[nodiscard]] bool select_direct_runtime(std::string_view executable, std::string_view model);
    void select_openai_compatible_runtime();
    [[nodiscard]] std::string active_model_name();
    [[nodiscard]] std::string active_provider_summary();
    [[nodiscard]] ModelManifest active_model_manifest();
    [[nodiscard]] std::vector<std::string> detected_model_names();
    [[nodiscard]] std::string model_detection_status();
    [[nodiscard]] std::string model_connection_status();
    [[nodiscard]] bool is_engine_ai_initialized() noexcept;
    [[nodiscard]] bool is_model_use_confirmed() noexcept;
    [[nodiscard]] std::vector<std::string> refresh_detected_models();
    [[nodiscard]] bool select_active_model(std::string_view model_id);
    [[nodiscard]] EvidencePaths default_evidence_paths();
    void append_tool_trace(const McpCaptureRecord& record);

    [[nodiscard]] bool promote_eval_case(const EvalCase& record, std::string_view suite_name = "editor_ai_smoke");
    [[nodiscard]] std::string normalize_assistant_reply(std::string_view reply);
    [[nodiscard]] std::string normalize_direct_llama_cpp_reply(
        std::string_view transcript,
        std::string_view user_text);
    [[nodiscard]] std::string normalize_direct_llama_cpp_source_reply(
        std::string_view transcript,
        std::string_view user_text);
    [[nodiscard]] bool direct_llama_cpp_prompt_transport_contract();
    [[nodiscard]] bool openai_source_iteration_request_contract();
    [[nodiscard]] bool is_promotable_assistant_reply(std::string_view reply);
    [[nodiscard]] HelperReviewGateResult classify_helper_review_reply(std::string_view reply);
}
