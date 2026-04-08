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
 *   Epoch Engine - AI Integration (LM Studio)
 *   License: MIT (adapt as needed)
 **************************************************************/
module;

#include "../include/_epoch.stl_types.hpp"

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

export module epoch.ai;

export import ai.runtime;
export import ai.dataset;
export import ai.train;
export import ai.mcp;
export import ai.eval;

import core.log;

export namespace epoch::ai
{
    struct Candidate
    {
        std::string text;
        double score = 0.0;
    };

    struct BotReply
    {
        std::string text;
        double score = 0.0;
        std::vector<Candidate> alternatives;
    };

    class Bot
    {
    public:
        struct Config
        {
            std::string backend = "lmstudio_chat";     // currently only lmstudio_chat
            std::string endpoint = "http://localhost:1234"; // base or full
            std::string model{};
            std::size_t best_of = 1;
        };

        explicit Bot(Config cfg);
        [[nodiscard]] BotReply submit(std::string_view user_input);

    private:
        Config m_cfg{};
        std::string m_endpoint_full{}; // normalized to /api/v1/chat
    };

    // Engine-global service wrapper (simple singleton)
    void init_bot();
    void shutdown_bot();
    void append_training_sample(std::string_view prompt, std::string_view answer, std::string_view source = "win32_chat_panel");
    [[nodiscard]] std::string send_to_bot(const std::string& user_text);
    [[nodiscard]] std::string default_workspace_root();
    [[nodiscard]] std::string curated_datasets_root();
    [[nodiscard]] std::string evals_root();
    [[nodiscard]] std::string tokenizer_root();
    [[nodiscard]] std::string prompts_root();
    [[nodiscard]] std::string manifests_root();
    [[nodiscard]] std::string local_capture_jsonl_path();
    [[nodiscard]] std::string local_mcp_capture_jsonl_path();
    [[nodiscard]] std::string local_checkpoint_root();
    [[nodiscard]] std::string local_model_root();
    [[nodiscard]] std::string local_cache_root();
    [[nodiscard]] ProviderMode current_provider_mode() noexcept;
    [[nodiscard]] std::string active_model_name();
    [[nodiscard]] std::string active_provider_summary();
    [[nodiscard]] ModelManifest active_model_manifest();
    [[nodiscard]] TrainingPaths default_training_paths();
    void append_mcp_capture(const McpCaptureRecord& record);
    [[nodiscard]] bool promote_mcp_capture_record(const McpCaptureRecord& record, std::string_view dataset_name = "epoch_mcp_curated");
    [[nodiscard]] bool promote_dataset_record(const DatasetRecord& record, std::string_view dataset_name = "epoch_editor_curated");
    [[nodiscard]] bool promote_eval_case(const EvalCase& record, std::string_view suite_name = "editor_ai_smoke");
}
