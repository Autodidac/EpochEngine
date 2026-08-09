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
export import ai.dataset;
export import ai.session;
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
        [[nodiscard]] EngineAiReply submit(std::string_view user_input);

    private:
        Config m_cfg{};
        std::string m_endpoint_full{}; // normalized to /v1/chat/completions
    };

    // Engine-global service wrapper (simple singleton)
    void init_engine_ai();
    void shutdown_engine_ai();

    [[nodiscard]] std::string send_to_engine_ai(const std::string& user_text);
    [[nodiscard]] std::string default_workspace_root();
    [[nodiscard]] std::string research_staging_root();
    [[nodiscard]] std::string iteration_packet_root();
    [[nodiscard]] std::string curated_datasets_root();
    [[nodiscard]] std::string evals_root();
    [[nodiscard]] std::string prompts_root();
    [[nodiscard]] std::string manifests_root();
    [[nodiscard]] std::string local_model_exchange_jsonl_path();
    [[nodiscard]] std::string local_tool_trace_jsonl_path();
    [[nodiscard]] std::string local_session_root();
    [[nodiscard]] std::string local_model_root();
    [[nodiscard]] std::string local_cache_root();
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
    [[nodiscard]] std::vector<std::string> refresh_detected_models();
    [[nodiscard]] bool select_active_model(std::string_view model_id);
    [[nodiscard]] EvidencePaths default_evidence_paths();
    void append_tool_trace(const McpCaptureRecord& record);
    [[nodiscard]] std::string stage_iteration_packet(const IterationPacket& packet);
    [[nodiscard]] bool promote_tool_trace_record(const McpCaptureRecord& record, std::string_view dataset_name = "epoch_mcp_curated");
    [[nodiscard]] bool promote_dataset_record(const DatasetRecord& record, std::string_view dataset_name = "epoch_editor_curated");
    [[nodiscard]] bool promote_eval_case(const EvalCase& record, std::string_view suite_name = "editor_ai_smoke");
    [[nodiscard]] bool is_promotable_assistant_reply(std::string_view reply);
    [[nodiscard]] HelperReviewGateResult classify_helper_review_reply(std::string_view reply);
}
